project(test-dylib)

add_executable(${PROJECT_NAME} test-lib.cpp
        modified-oddsound/Client/libMTSClient.cpp
        modified-oddsound/Master/libMTSMaster.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE modified-oddsound/Client modified-oddsound/Master)
add_dependencies(${PROJECT_NAME} MTS)