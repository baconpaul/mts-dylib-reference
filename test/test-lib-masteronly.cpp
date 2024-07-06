/*
 * Master0only test to avoid the client init
 */

#include "libMTSMaster.h"
#include <iostream>
#include <string.h>



int masterTwice()
{
    MTS_RegisterMaster();
    MTS_DeregisterMaster();
    MTS_Reinitialize();

    MTS_RegisterMaster();
    MTS_DeregisterMaster();

    return 0;
}

int masterTest()
{
    MTS_RegisterMaster();
    MTS_DeregisterMaster();

    return 0;
}


int masterTestCan()
{
    if (!MTS_CanRegisterMaster())
    {
        std::cout << "ERROR Cant can one" << std::endl;
        return 2;
    }
    MTS_RegisterMaster();
    if (MTS_CanRegisterMaster())
    {
        std::cout << "ERROR Cant can two" << std::endl;
        return 3;
    }
    MTS_DeregisterMaster();
    if (!MTS_CanRegisterMaster())
    {
        std::cout << "ERROR Cant can three" << std::endl;
        return 4;
    }

    return 0;
}

int invalidCallSequence()
{
    MTS_GetNumClients();
    return 0;
}

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        std::cout << "Please pick a test" << std::endl;
        return 2;
    }

#define RUN(x)                                                                                     \
    if (strcmp(argv[1], "--" #x) == 0)                                                             \
    {                                                                                              \
        std::cout << "===== RUNNING TEST: " << #x << std::endl;                                    \
        return x();                                                                                \
    }

    RUN(masterTest);
    RUN(masterTestCan);
    RUN(masterTwice);
    RUN(invalidCallSequence);

    exit(2);
}
