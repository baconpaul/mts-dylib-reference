//
// Created by Paul Walker on 7/5/24.
//

#include <iostream>
#include "libMTSClient.h"
#include "libMTSMaster.h"

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

void clientMasterTest()
{
    MTS_RegisterMaster();
    auto cl = MTS_RegisterClient();
    MTS_DeregisterClient(cl);

    auto cl2 = MTS_RegisterClient();
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
}

int main(int argc, char **argv) {
    // clientTest();
    // masterTest();
    clientMasterTest();
}