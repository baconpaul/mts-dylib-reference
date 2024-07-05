/*
 * Become an MTS client, and scan the frequency table vs 24 EDO
 */

#include "libMTSClient.h"
#include <cmath>
#include <iostream>

int main(int argc, char **argv)
{
    auto cl = MTS_RegisterClient();

    const char *sn = MTS_GetScaleName(cl);

    std::cout << "Scale name is " << sn << std::endl;
    if (std::string(sn) != "24EDO")
    {
        std::cout << "Scale name is incorrect" << std::endl;
        return 4;
    }
    for (int i=0; i<128; ++i)
    {
        auto f = MTS_NoteToFrequency(cl, i, -1);
        auto diff = f - 440 * std::pow(2.f, (i-69)/24);

        if (std::fabs(diff) > 0.001)
        {
            std::cout << "ERROR: Mismatch at midi note i " << f << " " << diff << std::endl;
            return 3;
        }
    }

    std::cout << "======================================\n"
              << "======================================\n"
              << "  Loaded 24EDO from Master Correctly\n"
              << "======================================\n"
              << "========================================" << std::endl;


        MTS_DeregisterClient(cl);
    return 0;
}