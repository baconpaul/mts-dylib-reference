//
// Created by Paul Walker on 7/5/24.
//

#include <iostream>
#include "libMTSMaster.h"
#include "libMTSClient.h"

#define LOGDAT                                                                                     \
    std::cout << "test/test-lib.cpp"                                                     \
              << ":" << __LINE__ << " [" << __func__ << "] "

void clientTest()
{
    MTSClient *client = MTS_RegisterClient();
    MTS_DeregisterClient(client);
}

void masterTest()
{
    MTS_RegisterMaster();
    MTS_DeregisterMaster();
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
    MTS_DeregisterClient(cl);

    MTS_SetNoteTuning(432.0, 69);

    auto cl2 = MTS_RegisterClient();
    freq = MTS_NoteToFrequency(cl2, 69, 0);
    LOGDAT << "Client 2: Note 69 has frequency " << freq << std::endl;
    if (freq != 432)
    {
        return 2;
    }
    MTS_DeregisterClient(cl2);

    static constexpr int nClients{4};
    MTSClient *clients[nClients];
    for (int i=0; i<nClients; ++i)
    {
        clients[i] = MTS_RegisterClient();
    }
    for (int i=0; i<nClients; ++i)
    {
        MTS_DeregisterClient(clients[i]);
    }

    MTS_DeregisterMaster();

    return 0;
}

int main(int argc, char **argv) {
    if (argc == 1)
    {
        return clientMasterTest();
    }
    // clientTest();
    // masterTest();
    // clientMasterTest();
}