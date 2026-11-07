/*
 * An implementation of the MTS-ESP middleware dylib.
 *
 * For more information, see oddsound.com
 *
 * Released under the MIT license
 */

#include <iostream>
#include <cstring>
#include <cstdint>
#include <cmath>
#include <array>
#include <string>
#include <cassert>
#include <mutex>
#include <atomic>
#include <type_traits>

#if !defined(MTSREF_EXPORT)
#if defined _WIN32 || defined __CYGWIN__
#ifdef __GNUC__
#define MTSREF_EXPORT __attribute__((dllexport))
#else
#define MTSREF_EXPORT __declspec(dllexport)
#endif
#else
#if __GNUC__ >= 4 || defined(__clang__)
#define MTSREF_EXPORT __attribute__((visibility("default")))
#else
#define MTSREF_EXPORT
#endif
#endif
#endif

#define LOGDAT                                                                                     \
    std::cout << "src/mts-dylib-reference.cpp"                                                     \
              << ":" << __LINE__ << " [" << __func__ << "] "
#define LOGFN LOGDAT << std::endl;

#if IPC_SUPPORT
#include <sys/shm.h>
#include <sys/errno.h>
#include <stdio.h> // for strerror
#include <dlfcn.h>

#define LOGERR LOGDAT << " ERROR: " << strerror(errno) << std::endl;
int shmid{0};
#endif

static void setDefaultTuning(double *t)
{
    for (int i = 0; i < 128; i++)
        t[i] = 440. * pow(2., (i - 69.) / 12.);
}

// matches the MTS-ESP API version these exports implement
static constexpr int libMTSVersion{0x00010003};

static constexpr size_t maxScaleNameSize{512};

// a struct so the compiler pads each member to its alignof boundary
struct SharedMemory
{
    bool hasMaster;
    bool tuningInitialized;
    int32_t numClients;
    double tuning[16][128];
    uint16_t noteFilter[128]; // channel bitset per key
    char scaleName[maxScaleNameSize];
    double periodRatio;
    int8_t mapSize;
    int8_t mapStartKey;
    int8_t refKey;
};
static_assert(std::is_standard_layout<SharedMemory>::value, "shared across processes");
static constexpr size_t memSize{sizeof(SharedMemory)};

bool *hasMaster{nullptr};
bool *tuningInitialized{nullptr};
int32_t *numClients{nullptr};
double *tuning[16]{};
uint16_t *noteFilter{nullptr};
char *scaleName{nullptr};
double *periodRatio{nullptr};
int8_t *mapSize{nullptr};
int8_t *mapStartKey{nullptr};
int8_t *refKey{nullptr};

static void resetTuningData()
{
    for (int i = 0; i < 16; ++i)
        setDefaultTuning(tuning[i]);
    for (int i = 0; i < 128; ++i)
        noteFilter[i] = 0;
    strncpy(scaleName, "12-TET", maxScaleNameSize - 1);
    *periodRatio = 2.0;
    // -1 tells clients the master has not supplied a mapping
    *mapSize = -1;
    *mapStartKey = -1;
    *refKey = -1;
}

// int cast since char signedness varies by platform
static bool validChannel(char c) { return (int)c >= 0 && (int)c < 16; }
static bool validNote(char n) { return (int)n >= 0 && (int)n < 128; }

SharedMemory localMemory{};

// bump when SharedMemory changes so old builds at the same path use a different segment
static constexpr int shmProjectId{64};

// set once callers hold pointers into the segment, so it must stay attached until exit
std::atomic<bool> segmentPointersShared{false};

bool skipIPC() { return getenv("MTS_REFERENCE_DEACTIVATE_IPC"); }

#if IPC_SUPPORT
// deleting a segment another process has attached would split master and clients
static bool lastAttachment()
{
    struct shmid_ds ds;
    if (shmctl(shmid, IPC_STAT, &ds) < 0)
    {
        LOGERR;
        return false;
    }
    LOGDAT << "Segment has " << ds.shm_nattch << " attachments" << std::endl;
    return ds.shm_nattch <= 1;
}
#endif

std::mutex s_connectMutex{};

bool connectToMemory()
{
    std::lock_guard<std::mutex> cl(s_connectMutex);

    bool initValues{false};

    SharedMemory *shm{nullptr};
#if IPC_SUPPORT

    if (skipIPC())
    {
        LOGDAT << "IPC-enabled platform chooses to skip IPC support" << std::endl;
        shm = &localMemory;
    }
    else
    {
        if (hasMaster && tuningInitialized && numClients &&
            tuning[0]) // already connected in this process!
        {
            return true;
        }

        // We need a shared existing path so
        Dl_info dl_info;
        auto res = dladdr((void *)connectToMemory, &dl_info);
        LOGDAT << "DLL Path is " << dl_info.dli_fname << std::endl;

        key_t key = ftok(dl_info.dli_fname, shmProjectId);
        if (key < 0)
        {
            LOGERR;
            LOGDAT << "Unable to create shmem key" << std::endl;
        }

        // Step one: See if they memory exists without creating it
        LOGDAT << "shmem Key is " << key << std::endl;
        shmid = shmget(key, memSize, 0666);
        if (shmid < 0)
        {
            LOGDAT << "Creating and initializing shared memory segment" << std::endl;
            shmid = shmget(key, memSize, 0666 | IPC_CREAT);
            initValues = true;
            if (shmid < 0)
            {
                LOGERR;
                LOGDAT << "Unable to create shared memory segment" << std::endl;
                return false;
            }
        }

        void *seg = shmat(shmid, (void *)0, 0);
        if (seg == (void *)-1)
        {
            LOGERR;
            LOGDAT << "Unable to attach shared memory segment" << std::endl;
            return false;
        }
        // shmat returns page aligned memory so the struct alignment holds
        shm = (SharedMemory *)seg;
    }
#else
    shm = &localMemory;
#endif

    hasMaster = &shm->hasMaster;
    tuningInitialized = &shm->tuningInitialized;
    numClients = &shm->numClients;
    for (int i = 0; i < 16; ++i)
        tuning[i] = shm->tuning[i];
    noteFilter = shm->noteFilter;
    scaleName = shm->scaleName;
    periodRatio = &shm->periodRatio;
    mapSize = &shm->mapSize;
    mapStartKey = &shm->mapStartKey;
    refKey = &shm->refKey;

    if (initValues)
    {
        LOGDAT << "Initializing values post creation" << std::endl;
        *hasMaster = false;
        *tuningInitialized = false;
        *numClients = 0;
    }

    if (!*tuningInitialized)
    {
        LOGDAT << "Initializing tuning table to 12-tet unfiltered" << std::endl;
        resetTuningData();
        *tuningInitialized = true;
    }

    return true;
}

void checkForMemoryRelease()
{
    std::lock_guard<std::mutex> cl(s_connectMutex);

#if IPC_SUPPORT
    if (skipIPC())
        return;

    if (!hasMaster)
    {
        LOGDAT << "No need to release when hasMaster not bound to shmem" << std::endl;
        return;
    }

    if (*hasMaster || !numClients || *numClients)
        return;

    if (segmentPointersShared)
    {
        LOGDAT << "No clients and no master, but tables are in use so staying attached"
               << std::endl;
        return;
    }

    bool removeSegment = lastAttachment();

    LOGDAT << "Detaching because no clients and no master" << std::endl;
    shmdt(hasMaster);
    hasMaster = nullptr;

    if (removeSegment)
    {
        LOGDAT << "Releasing unused memory segment at " << shmid << std::endl;
        shmctl(shmid, IPC_RMID, nullptr);
    }
#endif
}

struct DisconnectOnExitGuard
{
    ~DisconnectOnExitGuard()
    {
        std::lock_guard<std::mutex> cl(s_connectMutex);

#if IPC_SUPPORT
        if (skipIPC())
            return;

        if (!hasMaster)
            return;

        /* the difference between this and the check above is the check
         * will *also* not detach if it needs to free and this always
         * detaches since it is an exit guard.
         */

        bool freeSegment{false};
        if (hasMaster && !*hasMaster && numClients && !*numClients && lastAttachment())
        {
            freeSegment = true;
        }
        LOGDAT << "Detatching shmem on exit" << std::endl;

        shmdt(hasMaster);
        hasMaster = nullptr;

        if (freeSegment)
        {
            LOGDAT << "Deleting shared memory segment with no clients and master" << std::endl;
            shmctl(shmid, IPC_RMID, nullptr);
        }
#endif
    }
};

extern "C"
{

#define MASTER_SIDE_VALID(x)                                                                       \
    if (!hasMaster)                                                                                \
    {                                                                                              \
        LOGDAT << "Warning: Invalid call sequence" << std::endl;                                   \
        return x;                                                                                  \
    }

    // Master-side API
    MTSREF_EXPORT void MTS_RegisterMaster(void *)
    {
        LOGFN;
        connectToMemory();
        MASTER_SIDE_VALID();
        *hasMaster = true;
    }
    MTSREF_EXPORT void MTS_DeregisterMaster()
    {
        LOGFN;

        // special case - don't use the valid maco
        if (hasMaster)
        {
            *hasMaster = false;
            // so the next master doesn't inherit stale tuning
            resetTuningData();
        }
        checkForMemoryRelease();
    }
    MTSREF_EXPORT bool MTS_HasMaster()
    {
        MASTER_SIDE_VALID(false);

        return *hasMaster;
    }
    MTSREF_EXPORT bool MTS_HasIPC()
    {
        LOGFN;
#if IPC_SUPPORT
        return !skipIPC();
#else
        return false;
#endif
    }

    MTSREF_EXPORT void MTS_Reinitialize()
    {
        LOGFN;
        static DisconnectOnExitGuard dg;

        connectToMemory();

        MASTER_SIDE_VALID();

        *hasMaster = false;
        *numClients = 0;
        resetTuningData();

        *tuningInitialized = true;
    }

    MTSREF_EXPORT int MTS_GetVersionNumber() { return libMTSVersion; }

    MTSREF_EXPORT int MTS_GetNumClients()
    {
        MASTER_SIDE_VALID(-1);
        return *numClients;
    }

    MTSREF_EXPORT void MTS_SetNoteTunings(const double *d)
    {
        LOGFN;
        MASTER_SIDE_VALID();
        for (int ch = 0; ch < 16; ++ch)
            for (int i = 0; i < 128; ++i)
                tuning[ch][i] = d[i];
    }

    MTSREF_EXPORT void MTS_SetNoteTuning(double f, char idx)
    {
        MASTER_SIDE_VALID();
        if (!validNote(idx))
            return;
        for (int ch = 0; ch < 16; ++ch)
            tuning[ch][idx] = f;
    }

    MTSREF_EXPORT void MTS_SetScaleName(const char *s)
    {
        MASTER_SIDE_VALID();
        LOGDAT << s << std::endl;
        strncpy(scaleName, s, maxScaleNameSize - 1);
    }

    MTSREF_EXPORT void MTS_SetPeriodRatio(double r)
    {
        MASTER_SIDE_VALID();
        // nonsense ratios fall back to the octave clients assume when unset
        *periodRatio = (std::isfinite(r) && r > 0) ? r : 2.0;
    }

    // any negative value means unsupplied, and clients expect exactly -1
    MTSREF_EXPORT void MTS_SetMapSize(signed char v)
    {
        MASTER_SIDE_VALID();
        *mapSize = v < 0 ? -1 : v;
    }
    MTSREF_EXPORT void MTS_SetMapStartKey(signed char v)
    {
        MASTER_SIDE_VALID();
        *mapStartKey = v < 0 ? -1 : v;
    }
    MTSREF_EXPORT void MTS_SetRefKey(signed char v)
    {
        MASTER_SIDE_VALID();
        *refKey = v < 0 ? -1 : v;
    }

    // Don't implement note filtering or channel specific tuning yet
    MTSREF_EXPORT void MTS_FilterNote(bool doF, char note, char chan)
    {
        MASTER_SIDE_VALID();
        if (!validNote(note))
            return;

        uint16_t mask = 0xFFFF;

        if (validChannel(chan))
        {
            mask = 1 << chan;
        }

        if (doF)
        {
            noteFilter[note] = noteFilter[note] | mask;
        }
        else
        {
            noteFilter[note] = noteFilter[note] & ~mask;
        }
    }
    MTSREF_EXPORT void MTS_ClearNoteFilter()
    {
        MASTER_SIDE_VALID();
        for (int i = 0; i < 128; ++i)
        {
            noteFilter[i] = 0;
        }
    }
    MTSREF_EXPORT void MTS_FilterNoteMultiChannel(bool doF, char note, char chan)
    {
        MASTER_SIDE_VALID();
        if (validChannel(chan))
        {
            MTS_FilterNote(doF, note, chan);
        }
    }
    MTSREF_EXPORT void MTS_ClearNoteFilterMultiChannel(char chan)
    {
        MASTER_SIDE_VALID();
        if (!validChannel(chan))
            return;
        uint16_t off = 1 << chan;
        for (int i = 0; i < 128; ++i)
        {
            noteFilter[i] = noteFilter[i] & ~off;
        }
    }

    MTSREF_EXPORT void MTS_SetMultiChannel(bool, char) {}
    MTSREF_EXPORT void MTS_SetMultiChannelNoteTunings(const double *d, char ch)
    {
        MASTER_SIDE_VALID();
        if (!validChannel(ch))
            return;
        for (int i = 0; i < 128; ++i)
            tuning[ch][i] = d[i];
    }
    MTSREF_EXPORT void MTS_SetMultiChannelNoteTuning(double freq, char note, char ch)
    {
        MASTER_SIDE_VALID();
        LOGDAT << "f=" << freq << " at " << (int)note << " " << (int)ch << std::endl;
        if (!validChannel(ch) || !validNote(note))
            return;
        tuning[ch][note] = freq;
    }

    // Client implementation
    MTSREF_EXPORT void MTS_RegisterClient()
    {
        if (!connectToMemory())
            return;
        (*numClients)++;
        LOGDAT << "Client count is " << (*numClients) << std::endl;
    }
    MTSREF_EXPORT void MTS_DeregisterClient()
    {
        if (!numClients)
            return;
        // reinitialize zeroes the count under clients that are still registered
        if (*numClients > 0)
            (*numClients)--;
        LOGDAT << "Client count is " << (*numClients) << std::endl;
        checkForMemoryRelease();
    }

    MTSREF_EXPORT bool MTS_ShouldFilterNote(char note, char chan)
    {
        if (!validNote(note))
            return false;

        uint16_t mask = 0xFFFF;
        if (validChannel(chan))
            mask = 1 << chan;

        return noteFilter[note] & mask;
    }

    MTSREF_EXPORT bool MTS_ShouldFilterNoteMultiChannel(char note, char chan)
    {
        if (!validNote(note))
            return false;

        uint16_t mask = 0xFFFF;
        if (validChannel(chan))
            mask = 1 << chan;

        return noteFilter[note] & mask;
    }

    MTSREF_EXPORT const double *MTS_GetTuningTable()
    {
        static DisconnectOnExitGuard rt;
        connectToMemory();
        segmentPointersShared = true;

        return &tuning[0][0];
    }
    MTSREF_EXPORT const double *MTS_GetMultiChannelTuningTable(char ch)
    {
        static DisconnectOnExitGuard rt;
        connectToMemory();
        segmentPointersShared = true;

        if (!validChannel(ch))
            return &tuning[0][0];
        return &tuning[ch][0];
    }
    MTSREF_EXPORT bool MTS_UseMultiChannelTuning(char) { return true; }
    MTSREF_EXPORT const char *MTS_GetScaleName()
    {
        LOGFN;
        if (scaleName)
            segmentPointersShared = true;
        return scaleName;
    }

    // defaults match what the client wrapper assumes when unconnected
    MTSREF_EXPORT double MTS_GetPeriodRatio() { return periodRatio ? *periodRatio : 2.0; }
    MTSREF_EXPORT signed char MTS_GetMapSize() { return mapSize ? *mapSize : -1; }
    MTSREF_EXPORT signed char MTS_GetMapStartKey() { return mapStartKey ? *mapStartKey : -1; }
    MTSREF_EXPORT signed char MTS_GetRefKey() { return refKey ? *refKey : -1; }
}
