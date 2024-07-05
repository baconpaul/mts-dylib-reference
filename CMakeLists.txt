cmake_minimum_required(VERSION 3.15)
cmake_policy(SET CMP0091 NEW)

option(MTS_REFERENCE_INCLUDE_IPC_SUPPORT "Include IPC support if available on the OS" TRUE)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(CMAKE_OSX_DEPLOYMENT_TARGET 10.15 CACHE STRING "Minimum macOS version")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

project(mts-dylib-reference VERSION 0.9.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

add_library(MTS SHARED src/mts-dylib-reference.cpp)

if (${MTS_REFERENCE_INCLUDE_IPC_SUPPORT})
    if (UNIX OR APPLE)
        message(STATUS "Including IPC Support")
        target_compile_definitions(MTS PRIVATE IPC_SUPPORT=1)
        target_link_libraries(MTS PRIVATE dl)
    endif()
endif()

add_subdirectory(test)
