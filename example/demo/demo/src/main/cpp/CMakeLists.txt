cmake_minimum_required(VERSION 3.22)
project(puredart_demo LANGUAGES C CXX)

if(NOT DEFINED PUREDART_ROOT OR NOT IS_DIRECTORY "${PUREDART_ROOT}")
    message(FATAL_ERROR "PUREDART_ROOT must point to the puredart checkout")
endif()
if(NOT DEFINED PUREDART_META_FILE OR NOT EXISTS "${PUREDART_META_FILE}")
    message(FATAL_ERROR "PUREDART_META_FILE must point to generated metadata")
endif()

set(PUREDART_ENABLE_RUNTIME_HOOKS ON CACHE BOOL "" FORCE)
add_subdirectory("${PUREDART_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}/puredart")

add_library(puredart_demo SHARED demo.cpp)
target_compile_features(puredart_demo PRIVATE cxx_std_17)
target_link_libraries(puredart_demo PRIVATE puredart::core log)
puredart_add_meta(puredart_demo "${PUREDART_META_FILE}")
