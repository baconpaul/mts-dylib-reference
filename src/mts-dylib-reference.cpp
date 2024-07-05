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
#include <thread>

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
int thisProcessAttachments{0};
#endif

static void setDefaultTuning(double *t)
{
    for (int i = 0; i < 128; i++)
        t[i] = 440. * pow(2., (i - 69.) / 12.);
}

static constexpr size_t memSize{sizeof(bool) + sizeof(bool) + sizeof(int32_t) +
                                128 * 16 * sizeof(double) + 128 * sizeof(uint16_t)};
bool *hasMaster{nullptr};
bool *tuningInitialized{nullptr};
int32_t *numClients{nullptr};
double *tuning[16]{};
uint16_t *noteFilter{nullptr}; // channel bitset per key

uint8_t memory[memSize];

bool skipIPC() { return getenv("MTS_REFERENCE_DEACTIVATE_IPC"); }

std::mutex s_connectMutex{};

bool connectToMemory()
{
    std::lock_guard<std::mutex> cl(s_connectMutex);

    bool initValues{false};

    uint8_t *memSeg{nullptr};
#if IPC_SUPPORT

    if (skipIPC())
    {
        LOGDAT << "IPC-enabled platform chooses to skip IPC support" << std::endl;
        memSeg = (uint8_t *)(&(memory[0]));
    }
    else
    {
        if (hasMaster && tuningInitialized && numClients &&
            tuning[0]) // already connected in this process!
        {
            thisProcessAttachments++;
            LOGDAT << "Already set up - no need to reconnect. Process attachment number "
                   << thisProcessAttachments << std::endl;
            return true;
        }

        // We need a shared existing path so
        Dl_info dl_info;
        auto res = dladdr((void *)connectToMemory, &dl_info);
        LOGDAT << "DLL Path is " << dl_info.dli_fname << std::endl;

        key_t key = ftok(dl_info.dli_fname, 63);
        if (key < 0)
        {
            LOGERR;
            LOGDAT << "Unable to create mtsesp / 65 key" << std::endl;
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

        memSeg = (uint8_t *)shmat(shmid, (void *)0, 0);
        thisProcessAttachments = 1;
    }
#else
    memSeg = (uint8_t *)(&(memory[0]));
#endif

    hasMaster = (bool*)memSeg;
    memSeg += sizeof(bool);

    tuningInitialized = (bool *)memSeg;
    memSeg += sizeof(bool);

    numClients = (int32_t *)memSeg;
    memSeg += sizeof(int32_t);

    for (int i=0; i<16; ++i)
    {
        tuning[i] = (double *)memSeg;
        memSeg += 128 * sizeof(double);
    }

    noteFilter = (uint16_t *)memSeg;

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
        for (int i=0; i<16; ++i)
            setDefaultTuning(tuning[i]);
        for (int i=0; i<128; ++i)
            noteFilter[i] = 0;
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

    bool freeSegment{false};
    if (hasMaster && !*hasMaster && numClients && !*numClients)
    {
        freeSegment = true;
    }

    thisProcessAttachments--;
    if (thisProcessAttachments == 0)
    {
        LOGDAT << "Detatching hasMaster from shmem" << std::endl;
        shmdt(hasMaster);
        hasMaster = nullptr;
    }
    else
    {
        LOGDAT << "Remaining attached since there are still " << thisProcessAttachments
               << " attachments in this process" << std::endl;
    }

    if (freeSegment || thisProcessAttachments == 0)
    {
        if (hasMaster)
        {
            shmdt(hasMaster);
            hasMaster = nullptr;
        }
        LOGDAT << "Releasing unused memory segment at " << shmid << std::endl;
        shmctl(shmid, IPC_RMID, nullptr);
    }
#endif
}

extern "C"
{

    // Master-side API
    MTSREF_EXPORT void MTS_RegisterMaster(void *)
    {
        LOGFN;
        connectToMemory();
        *hasMaster = true;
        *numClients = 0;
    }
    MTSREF_EXPORT void MTS_DeregisterMaster()
    {
        LOGFN;
        *hasMaster = false;
        *numClients = 0;

        checkForMemoryRelease();
    }
    MTSREF_EXPORT bool MTS_HasMaster()
    {
        if (hasMaster)
            return *hasMaster;
        else
            return false;
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
        *hasMaster = false;
        *numClients = 0;
        for (int i=0; i<16; ++i)
            setDefaultTuning(tuning[0]);
        for (int i=0; i<128; ++i)
            noteFilter[i] = 0;

        *tuningInitialized = true;
    }

    MTSREF_EXPORT int MTS_GetNumClients() { return *numClients; }

    MTSREF_EXPORT void MTS_SetNoteTunings(const double *d)
    {
        LOGFN;
        for (int ch=0; ch<16; ++ch)
            for (int i = 0; i < 128; ++i)
                tuning[ch][i] = d[i];
    }

    MTSREF_EXPORT void MTS_SetNoteTuning(double f, char idx)
    {
        for (int ch=0; ch<16; ++ch)
            tuning[ch][idx] = f;
    }

    char scaleName[256]{0};
    MTSREF_EXPORT void MTS_SetScaleName(const char *s)
    {
        LOGDAT << s << std::endl;
        strncpy(scaleName, s, 255);
    }

    // Don't implement note filtering or channel specific tuning yet
    MTSREF_EXPORT void MTS_FilterNote(bool doF, char note, char chan) {
        uint16_t mask = 0xFFFF;

        if (chan >= 0 && chan <= 15)
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
    MTSREF_EXPORT void MTS_ClearNoteFilter() {
        for (int i=0; i<128; ++i)
        {
            noteFilter[i] = 0;
        }
    }
    MTSREF_EXPORT void MTS_FilterNoteMultiChannel(bool doF, char note, char chan) {
        if (chan >= 0 && chan < 15)
        {
            MTS_FilterNote(doF, note, chan);
        }
    }
    MTSREF_EXPORT void MTS_ClearNoteFilterMultiChannel(char chan) {
        uint16_t off = 1<<chan;
        for (int i=0; i<128; ++i)
        {
            noteFilter[i] = noteFilter[i] & ~off;
        }
    }

    MTSREF_EXPORT void MTS_SetMultiChannel(bool, char) {}
    MTSREF_EXPORT void MTS_SetMultiChannelNoteTunings(const double *d, char ch) {
        for (int i = 0; i < 128; ++i)
            tuning[ch][i] = d[i];
    }
    MTSREF_EXPORT void MTS_SetMultiChannelNoteTuning(double freq, char note, char ch) {
        LOGDAT << "f=" << freq << " at " << (int) note << " " << (int) ch << std::endl;
        tuning[ch][note] = freq;
    }

    // Client implementation
    MTSREF_EXPORT void MTS_RegisterClient()
    {
        LOGFN;
        connectToMemory();
        (*numClients)++;
    }
    MTSREF_EXPORT void MTS_DeregisterClient()
    {
        LOGFN;
        (*numClients)--;

        checkForMemoryRelease();
    }

    MTSREF_EXPORT bool MTS_ShouldFilterNote(char note, char chan) {
        uint16_t mask = 0xFFFF;
        if (chan >= 0 && chan <= 15)
            mask = 1 << chan;

        return noteFilter[note] & mask;
    }

    MTSREF_EXPORT bool MTS_ShouldFilterNoteMultiChannel(char note, char chan) {
        uint16_t mask = 0xFFFF;
        if (chan >= 0 && chan <= 15)
            mask = 1 << chan;

        return noteFilter[note] & mask;
    }

    MTSREF_EXPORT const double *MTS_GetTuningTable()
    {
        LOGFN;

        /* So why do we have to do this? Well the GetTuningTable happens before any client
         * is registered at all (!!) in the libMTSClient mtsclientglobal constructor. Ugh.
         * But that also means we need to release properly. In the event that a master or
         * client ever attached and detached this won't do anything.
         */
        struct ReleaseThingy
        {
            ~ReleaseThingy() { checkForMemoryRelease(); }
        };
        static ReleaseThingy rt;

        connectToMemory();

        return &tuning[0][0];
    }
    MTSREF_EXPORT const double *MTS_GetMultiChannelTuningTable(char ch) {
        LOGFN;

        struct ReleaseThingy
        {
            ~ReleaseThingy() { checkForMemoryRelease(); }
        };
        static ReleaseThingy rt;

        connectToMemory();

        return &tuning[ch][0];
    }
    MTSREF_EXPORT bool MTS_UseMultiChannelTuning(char) { return true; }
    MTSREF_EXPORT const char *MTSGetScaleName() { return scaleName; }
}
