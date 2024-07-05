project(test-dylib)

add_executable(${PROJECT_NAME} test-lib.cpp
        modified-oddsound/Client/libMTSClient.cpp
        modified-oddsound/Master/libMTSMaster.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE modified-oddsound/Client modified-oddsound/Master)
add_dependencies(${PROJECT_NAME} MTS)

add_executable(mst24EDO mst24EDO.cpp modified-oddsound/Master/libMTSMaster.cpp)
target_include_directories(mst24EDO PRIVATE modified-oddsound/Master)
add_dependencies(mst24EDO MTS)

add_executable(clnt24EDO clnt24EDO.cpp modified-oddsound/Client/libMTSClient.cpp)
target_include_directories(clnt24EDO PRIVATE modified-oddsound/Client)
add_dependencies(clnt24EDO MTS)

add_custom_target(all-tests)
add_dependencies(all-tests ${PROJECT_NAME} clnt24EDO mst24EDO)

if (UNIX OR APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE dl)
    target_link_libraries(mst24EDO PRIVATE dl)
    target_link_libraries(clnt24EDO PRIVATE dl)
endif()

if (${MTS_REFERENCE_INCLUDE_IPC_SUPPORT})
    if (UNIX OR APPLE)
        message(STATUS "Testing Configured for IPC Support")
        target_compile_definitions(${PROJECT_NAME} PRIVATE TEST_IPC_SUPPORT=1)
        target_compile_definitions(${PROJECT_NAME} PRIVATE UNIX_LIKE=1)
    endif()
endif()
