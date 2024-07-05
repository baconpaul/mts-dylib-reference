/*
 * Become an MTS client, and scan the frequency table vs 24 EDO
 */

#include "libMTSClient.h"
#include <cmath>
#include <iostream>

int main(int argc, char **argv)
{
    auto cl = MTS_RegisterClient();

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

    MTS_DeregisterClient(cl);
    return 0;
}