cmake_minimum_required(VERSION 3.22)
project(hermes_harness LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(PUREHERMES_BUILD_TESTS OFF CACHE BOOL "")
set(PUREHERMES_BUILD_TOOLS OFF CACHE BOOL "")
add_subdirectory("${REPOSITORY}/hermes" purehermes)
include("${REPOSITORY}/hermes/tests/device.cmake")
