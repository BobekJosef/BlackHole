cmake_minimum_required(VERSION 3.25)

project(BlackHole
    VERSION 0.1.0
    DESCRIPTION "Interactive GPU ray tracer for rotating (Kerr) black holes"
    LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(BLACKHOLE_WERROR "Treat compiler warnings as errors" OFF)
option(BLACKHOLE_SANITIZE "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(BLACKHOLE_BUILD_TESTS "Build the unit tests" ON)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
include(ProjectOptions)
include(Dependencies)

add_subdirectory(src)

if(BLACKHOLE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
