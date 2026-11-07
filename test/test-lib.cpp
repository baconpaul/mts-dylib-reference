/*
 * A basic set of smoke tests so I can make sure it at least starts and stuff
 */

#include <iostream>
#include <cmath>
#include <string.h>
#include "libMTSMaster.h"
#include "libMTSClient.h"

#if TEST_IPC_SUPPORT
#include <stdlib.h>
#include <sys/shm.h>
#endif

#define LOGDAT                                                                                     \
    std::cout << "test/test-lib.cpp"                                                               \
              << ":" << __LINE__ << " [" << __func__ << "] "

#if TEST_IPC_SUPPORT
// must match shmProjectId in src/mts-dylib-reference.cpp
static constexpr int shmProjectId{64};

// id of the library's live segment, or -1 if there is none
static int liveSegment()
{
    auto loc = getenv("MTS_LIB_LOCATION");
    if (!loc)
        return -1;
    return shmget(ftok(loc, shmProjectId), 0, 0);
}
#endif

int clientTest()
{
    MTSClient *client = MTS_RegisterClient();
    MTS_DeregisterClient(client);
    return 0;
}

int clientMasterTest()
{
    MTS_RegisterMaster();

    auto cl = MTS_RegisterClient();
    double freq = MTS_NoteToFrequency(cl, 69, 0);
    LOGDAT << "Client 1: Note 69 has frequency " << freq << std::endl;
    if (freq != 440)
    {
        return 1;
    }

    for (int i = 0; i < 128; ++i)
    {
        for (auto ch = -1; ch < 16; ++ch)
        {
            if (MTS_ShouldFilterNote(cl, i, ch))
            {
                LOGDAT << "Unexpected filter at " << i << " / " << ch << std::endl;
                return 8;
            }
        }
    }
    MTS_DeregisterClient(cl);

    MTS_SetNoteTuning(432.0, 69);
    MTS_FilterNote(true, 60, -1);

    // Non-channel aware APIs
    auto cl2 = MTS_RegisterClient();
    freq = MTS_NoteToFrequency(cl2, 69, 0);
    LOGDAT << "Client 2: Note 69 has frequency " << freq << std::endl;
    if (freq != 432)
    {
        return 2;
    }

    LOGDAT << "Checking filter" << std::endl;
    for (int i = 0; i < 128; ++i)
    {
        for (auto ch = -1; ch < 16; ++ch)
        {
            auto expected = (i == 60);
            if (MTS_ShouldFilterNote(cl2, i, ch) != expected)
            {
                LOGDAT << "Unexpected filter at " << i << " / " << ch << std::endl;
                MTS_ShouldFilterNote(cl2, i, ch);
                return 3;
            }
        }
    }
    MTS_DeregisterClient(cl2);

    // Channel aware APIs
    MTS_SetMultiChannelNoteTuning(880.0, 69, 4);
    MTS_FilterNote(true, 60, -1);

    auto cl3 = MTS_RegisterClient();
    for (int ch = 0; ch < 16; ++ch)
    {
        freq = MTS_NoteToFrequency(cl3, 69, ch);
        LOGDAT << "Client 3: Note 69 Ch " << ch << " has frequency " << freq << std::endl;
        if (freq != (ch == 4 ? 880 : 432))
        {
            return 2;
        }
    }
    MTS_DeregisterClient(cl3);

    static constexpr int nClients{4};
    MTSClient *clients[nClients];
    for (int i = 0; i < nClients; ++i)
    {
        clients[i] = MTS_RegisterClient();
    }
    for (int i = 0; i < nClients; ++i)
    {
        MTS_DeregisterClient(clients[i]);
    }

    MTS_DeregisterMaster();

    return 0;
}

int apiV103Test()
{
    MTS_RegisterMaster();
    auto cl = MTS_RegisterClient();

    if (MTS_Master_ShouldUpdateLibrary() || MTS_Client_ShouldUpdateLibrary(cl))
    {
        LOGDAT << "Library reports an out of date version" << std::endl;
        return 1;
    }

    if (MTS_GetPeriodRatio(cl) != 2.0 || MTS_GetMapSize(cl) != -1 || MTS_GetMapStartKey(cl) != -1 ||
        MTS_GetRefKey(cl) != -1)
    {
        LOGDAT << "Unexpected defaults" << std::endl;
        return 2;
    }

    MTS_SetPeriodRatio(3.0);
    MTS_SetMapSize(13);
    MTS_SetMapStartKey(60);
    MTS_SetRefKey(69);
    LOGDAT << "Period " << MTS_GetPeriodRatio(cl) << " size " << (int)MTS_GetMapSize(cl)
           << " start " << (int)MTS_GetMapStartKey(cl) << " ref " << (int)MTS_GetRefKey(cl)
           << std::endl;
    if (MTS_GetPeriodRatio(cl) != 3.0 || MTS_GetMapSize(cl) != 13 || MTS_GetMapStartKey(cl) != 60 ||
        MTS_GetRefKey(cl) != 69)
    {
        return 3;
    }
    if (std::fabs(MTS_GetPeriodSemitones(cl) - 12.0 * std::log2(3.0)) > 1e-9)
    {
        LOGDAT << "Bad period semitones " << MTS_GetPeriodSemitones(cl) << std::endl;
        return 4;
    }

    MTS_SetPeriodRatio(-1.0);
    MTS_SetMapSize(-7);
    if (MTS_GetPeriodRatio(cl) != 2.0 || MTS_GetMapSize(cl) != -1)
    {
        LOGDAT << "Invalid values not normalized" << std::endl;
        return 5;
    }

    MTS_SetNoteTuning(432.0, 69);
    MTS_SetScaleName("stale scale");
    MTS_SetMapSize(12);
    if (MTS_NoteToFrequency(cl, 69, -1) != 432.0)
    {
        LOGDAT << "Tuning not applied before deregister" << std::endl;
        return 6;
    }

    // client stays registered so the segment survives the master leaving
    MTS_DeregisterMaster();
    MTS_RegisterMaster();

    // a fresh segment would also read as defaults, so prove this is the same one
    if (MTS_GetNumClients() != 1)
    {
        LOGDAT << "Segment did not survive master deregister" << std::endl;
        return 8;
    }

    auto freq = MTS_NoteToFrequency(cl, 69, -1);
    LOGDAT << "After re-register: freq " << freq << " size " << (int)MTS_GetMapSize(cl) << " name '"
           << MTS_GetScaleName(cl) << "'" << std::endl;
    if (freq != 440.0 || MTS_GetMapSize(cl) != -1 || strcmp(MTS_GetScaleName(cl), "12-TET") != 0)
    {
        return 7;
    }
    MTS_DeregisterClient(cl);
    MTS_DeregisterMaster();

    return 0;
}

// the client wrapper caches table pointers at load, before any client registers
int masterRestartTest()
{
    MTS_RegisterMaster();
    MTS_SetNoteTuning(432.0, 69);
#if TEST_IPC_SUPPORT
    bool checkSegment = MTS_HasIPC() && getenv("MTS_LIB_LOCATION");
    int before = liveSegment();
#endif

    MTS_DeregisterMaster();

#if TEST_IPC_SUPPORT
    // a recreated segment can land at the old address and hide stale pointers
    if (checkSegment && liveSegment() != before)
    {
        LOGDAT << "Segment " << before << " replaced by " << liveSegment() << std::endl;
        return 1;
    }
#endif

    MTS_RegisterMaster();
    MTS_SetNoteTuning(450.0, 69);

    auto cl = MTS_RegisterClient();
    auto freq = MTS_NoteToFrequency(cl, 69, -1);
    LOGDAT << "Client after master restart sees " << freq << std::endl;
    if (freq != 450.0)
        return 2;

    MTS_DeregisterClient(cl);
    MTS_DeregisterMaster();
    return 0;
}

int clientCountTest()
{
    auto cl = MTS_RegisterClient();
    MTS_RegisterMaster();
    LOGDAT << "Clients after master registers: " << MTS_GetNumClients() << std::endl;
    if (MTS_GetNumClients() != 1)
        return 1;

    MTS_DeregisterMaster();
    MTS_RegisterMaster();
    LOGDAT << "Clients after master re-registers: " << MTS_GetNumClients() << std::endl;
    if (MTS_GetNumClients() != 1)
        return 2;

    auto cl2 = MTS_RegisterClient();
    if (MTS_GetNumClients() != 2)
        return 3;

    MTS_DeregisterClient(cl2);
    MTS_DeregisterClient(cl);
    LOGDAT << "Clients after both deregister: " << MTS_GetNumClients() << std::endl;
    if (MTS_GetNumClients() != 0)
        return 4;

    MTS_DeregisterMaster();
    return 0;
}

int invalidCallSequence()
{
    MTS_GetNumClients();
    return 0;
}

int main(int argc, char **argv)
{
    if (argc == 1)
    {
        std::cout << "===== RUNNING TEST: clientMasterTest" << std::endl;
        return clientMasterTest();
    }

    if (strcmp(argv[1], "--null") == 0)
    {
        std::cout << "===== RUNNING TEST: null" << std::endl;
        return 0;
    }

#define RUN(x)                                                                                     \
    if (strcmp(argv[1], "--" #x) == 0)                                                             \
    {                                                                                              \
        std::cout << "===== RUNNING TEST: " << #x << std::endl;                                    \
        return x();                                                                                \
    }

    RUN(clientTest);
    RUN(apiV103Test);
    RUN(clientCountTest);
    RUN(masterRestartTest);
    RUN(invalidCallSequence);

    std::cout << "********* UNABLE to LOCATE TEST " << argv[1] << std::endl;

    exit(2);
}