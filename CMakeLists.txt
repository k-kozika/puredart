cmake_minimum_required(VERSION 3.22)
project(puredart VERSION 0.1.0 LANGUAGES C CXX)

option(PURENATIVE_BUILD_TESTS "Build purenative host unit tests" ${PROJECT_IS_TOP_LEVEL})
option(PUREDART_BUILD_TESTS "Build puredart host unit tests" ${PROJECT_IS_TOP_LEVEL})

if(NOT ANDROID AND (PURENATIVE_BUILD_TESTS OR PUREDART_BUILD_TESTS))
    enable_testing()
endif()

add_subdirectory(native)
add_subdirectory(dart)
