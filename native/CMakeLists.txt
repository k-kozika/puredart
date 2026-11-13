cmake_minimum_required(VERSION 3.22)

# dart/ can already have added this library for its runtime hook adapter.
if(TARGET purenative::core)
    return()
endif()

project(purenative LANGUAGES CXX)

add_library(purenative_core STATIC src/hook.cpp)
add_library(purenative::core ALIAS purenative_core)
target_compile_features(purenative_core PUBLIC cxx_std_17)
target_include_directories(purenative_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
set_target_properties(purenative_core PROPERTIES POSITION_INDEPENDENT_CODE ON
    CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN ON)
if(ANDROID)
    target_link_libraries(purenative_core PRIVATE log)
    if(CMAKE_ANDROID_ARCH_ABI STREQUAL "arm64-v8a")
        target_compile_options(purenative_core PRIVATE
            -mbranch-protection=standard -fno-sanitize=shadow-call-stack)
    endif()
endif()
option(PURENATIVE_BUILD_TESTS "Build ARM64 patch preparation tests" ${PROJECT_IS_TOP_LEVEL})
if(PURENATIVE_BUILD_TESTS AND NOT ANDROID)
    enable_testing()
    # Test the actual ARM64 patch machinery using mapped instruction buffers;
    # host tests never execute those instructions.
    target_compile_definitions(purenative_core PRIVATE PURENATIVE_TESTING)
    add_executable(purenative_tests tests/hook_tests.cpp)
    target_compile_definitions(purenative_tests PRIVATE PURENATIVE_TESTING)
    target_link_libraries(purenative_tests PRIVATE purenative::core)
    add_test(NAME purenative_tests COMMAND purenative_tests)
endif()
