/*
 * Become an MTS master, set up 24EDO, then sleep for 5 seconds,
 * then deregister
 */

#include <cmath>
#include <chrono>
#include <thread>

#include "libMTSMaster.h"

int main(int argc, char **argv)
{
    MTS_RegisterMaster();

    for (int i = 0; i < 128; ++i)
    {
        MTS_SetNoteTuning(440.0 * std::pow(2.0, (i - 69) / 24), i);
    }

    using namespace std::chrono_literals;
    std::this_thread::sleep_for(5000ms);

    MTS_DeregisterMaster();
}