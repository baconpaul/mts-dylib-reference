project(test-dylib)

add_executable(${PROJECT_NAME} test-lib.cpp
        modified-oddsound/Client/libMTSClient.cpp
        modified-oddsound/Master/libMTSMaster.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE modified-oddsound/Client modified-oddsound/Master)
add_dependencies(${PROJECT_NAME} MTS)



if (${MTS_REFERENCE_INCLUDE_IPC_SUPPORT})
    if (UNIX OR APPLE)
        message(STATUS "Testing Configured for IPC Support")
        target_compile_definitions(${PROJECT_NAME} PRIVATE TEST_IPC_SUPPORT=1)
        target_compile_definitions(${PROJECT_NAME} PRIVATE UNIX_LIKE=1)
        target_link_libraries(${PROJECT_NAME} PRIVATE dl)
    endif()
endif()
