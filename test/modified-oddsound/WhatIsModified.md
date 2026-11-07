This is ODDSound MTS-ESP with one change. The client and master
loaders read the `MTS_LIB_LOCATION` environment variable to get the
library location, so I can run tests without installing the library
in the default location.

Pulled from https://github.com/ODDSound/MTS-ESP at
`f214739b8832e7f297cb9970d0c0efbf783f1462` (2026-02-04, API v1.03).

Only `Client/`, `Master/`, `README.md` and `LICENSE` are copied; the
prebuilt `libMTS` binaries and installers are not.
