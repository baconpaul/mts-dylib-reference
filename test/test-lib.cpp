/*
 * A basic set of smoke tests so I can make sure it at least starts and stuff
 */

#include <iostream>
#include <cmath>
#include <string.h>
#include "libMTSMaster.h"
#include "libMTSClient.h"

#define LOGDAT                                                                                     \
    std::cout << "test/test-lib.cpp"                                                               \
              << ":" << __LINE__ << " [" << __func__ << "] "

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

    MTS_DeregisterClient(cl);

    // with ipc the segment is released on deregister, invalidating the client's cached tables
    if (MTS_HasIPC())
    {
        LOGDAT << "Skipping deregister reset check under IPC" << std::endl;
        MTS_DeregisterMaster();
        return 0;
    }

    MTS_SetNoteTuning(432.0, 69);
    MTS_SetScaleName("stale scale");
    MTS_SetMapSize(12);
    MTS_DeregisterMaster();

    MTS_RegisterMaster();
    auto cl2 = MTS_RegisterClient();
    auto freq = MTS_NoteToFrequency(cl2, 69, -1);
    LOGDAT << "After re-register: freq " << freq << " size " << (int)MTS_GetMapSize(cl2)
           << " name '" << MTS_GetScaleName(cl2) << "'" << std::endl;
    if (freq != 440.0 || MTS_GetMapSize(cl2) != -1 || strcmp(MTS_GetScaleName(cl2), "12-TET") != 0)
    {
        return 6;
    }
    MTS_DeregisterClient(cl2);
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
    RUN(invalidCallSequence);

    std::cout << "********* UNABLE to LOCATE TEST " << argv[1] << std::endl;

    exit(2);
}