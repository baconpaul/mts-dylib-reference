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
                                128 * sizeof(double)};
bool *hasMaster{nullptr};
bool *tuningInitialized{nullptr};
int32_t *numClients{nullptr};
double *tuning{nullptr};

#if !IPC_SUPPORT
std::array<uint8_t, memSize> memory{};
#endif

bool connectToMemory()
{
    bool initValues{false};
#if IPC_SUPPORT
    if (hasMaster && tuningInitialized && numClients &&
        tuning) // already connected in this process!
    {
        thisProcessAttachments++;
        LOGDAT << "Already set up - no need to reconnect. Process attachment number " << thisProcessAttachments << std::endl;
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

    hasMaster = (bool *)shmat(shmid, (void *)0, 0);
    thisProcessAttachments = 1;
#else
    hasMaster = (bool *)(memory.data());
#endif

    tuningInitialized = (bool *)hasMaster + sizeof(bool);
    numClients = (int32_t *)tuningInitialized + sizeof(bool);
    tuning = (double *)numClients + sizeof(int32_t);

    if (initValues)
    {
        LOGDAT << "Initializing values post creation" << std::endl;
        *hasMaster = false;
        *tuningInitialized = false;
        *numClients = 0;
    }

    if (!*tuningInitialized)
    {
        LOGDAT << "Initializing tuning table to 12-tet" << std::endl;
        setDefaultTuning(tuning);
        *tuningInitialized = true;
    }

    return true;
}

void checkForMemoryRelease()
{
#if IPC_SUPPORT
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
    }
    else
    {
        LOGDAT << "Remaining attached since there are still " << thisProcessAttachments
               << " attachments in this process" << std::endl;
    }

    if (freeSegment)
    {
        LOGDAT << "Releasing unused memory segment at " << shmid << std::endl;
        shmctl(shmid, IPC_RMID, nullptr);
    }
#endif
}

extern "C"
{

    // Master-side API
    void MTS_RegisterMaster(void *)
    {
        LOGFN;
        connectToMemory();
        *hasMaster = true;
        *numClients = 0;
    }
    void MTS_DeregisterMaster()
    {
        LOGFN;
        *hasMaster = false;
        *numClients = 0;

        checkForMemoryRelease();
    }
    bool MTS_HasMaster()
    {
        LOGDAT << *hasMaster << std::endl;
        return *hasMaster;
    }
    bool MTS_HasIPC()
    {
        LOGFN;
#if IPC_SUPPORT
        return true;
#else
        return false;
#endif
    }

    void MTS_Reinitialize()
    {
        LOGFN;
        *hasMaster = false;
        *numClients = 0;
        setDefaultTuning(tuning);
        *tuningInitialized = true;
    }

    int MTS_GetNumClients() { return *numClients; }

    void MTS_SetNoteTunings(const double *d)
    {
        LOGFN;
        for (int i = 0; i < 128; ++i)
            tuning[i] = d[i];
    }

    void MTS_SetNoteTuning(double f, char idx)
    {
        LOGDAT << f << " " << (int)idx << std::endl;
        tuning[idx] = f;
    }

    char scaleName[256]{0};
    void MTS_SetScaleName(const char *s)
    {
        LOGDAT << s << std::endl;
        strncpy(scaleName, s, 255);
    }

    // Don't implement note filtering or channel specific tuning yet
    void MTS_FilterNote(bool, char, char) {}
    void MTS_ClearNoteFilter() {}
    void MTS_SetMultiChannel(bool, char) {}
    void MTS_SetMultiChannelNoteTunings(const double *, char) {}
    void MTS_SetMultiChannelNoteTuning(double, char, char) {}
    void MTS_FilterNoteMultiChannel(bool, char, char) {}
    void MTS_ClearNoteFilterMultiChannel(char) {}

    // Client implementation
    void MTS_RegisterClient()
    {
        LOGFN;
        connectToMemory();
        (*numClients)++;
    }
    void MTS_DeregisterClient()
    {
        LOGFN;
        (*numClients)--;

        checkForMemoryRelease();
    }
    // Don't implement note filtering
    bool MTS_ShouldFilterNote(char, char) { return false; }
    bool MTS_ShouldFilterNoteMultiChannel(char, char) { return false; }

    const double *MTS_GetTuningTable() { return &tuning[0]; }
    const double *MTS_GetMultiChannelTuningTable(char) { return &tuning[0]; }
    bool MTS_UseMultiChannelTuning(char) { return false; }
    const char *MTS_GetScaleName() { return scaleName; }
}
