# mts-dylib-reference

mts-dylib-reference is an open source implementation of the middleware API used to communicate
between oddsound MTS clients and servers. [The complete documentation for the MTS-ESP api is
available at ODDSounds github](https://github.com/ODDSound/MTS-ESP)

This implementation is not as complete as the closed source oddsound intermediate dynamic 
library. The primary features lacking are

1. IPC is not available on Windows, so sandboxed architectures wont work
2. IPC activation is a compile time choice (on by default) on Linux and Windows

Nonetheless, the project exists to show the inner workings of the intermediate API.
We recommend all production users of the MTS-ESP system use the official intermediate
library builds from Oddsound.

