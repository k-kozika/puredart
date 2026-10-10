cmake_minimum_required(VERSION 3.22)
project(nativehook LANGUAGES CXX)

add_library(nativehook_core STATIC src/hook.cpp)
add_library(nativehook::core ALIAS nativehook_core)
target_compile_features(nativehook_core PUBLIC cxx_std_17)
target_include_directories(nativehook_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
set_target_properties(nativehook_core PROPERTIES POSITION_INDEPENDENT_CODE ON
    CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN ON)
if(ANDROID)
    target_link_libraries(nativehook_core PRIVATE log)
    if(CMAKE_ANDROID_ARCH_ABI STREQUAL "arm64-v8a")
        target_compile_options(nativehook_core PRIVATE
            -mbranch-protection=standard -fno-sanitize=shadow-call-stack)
    endif()
endif()
option(NATIVEHOOK_BUILD_TESTS "Build ARM64 patch preparation tests" ${PROJECT_IS_TOP_LEVEL})
if(NATIVEHOOK_BUILD_TESTS AND NOT ANDROID)
    enable_testing()
    # Test the actual ARM64 patch machinery using mapped instruction buffers;
    # host tests never execute those instructions.
    target_compile_definitions(nativehook_core PRIVATE NATIVEHOOK_TESTING)
    add_executable(nativehook_tests tests/hook_tests.cpp)
    target_compile_definitions(nativehook_tests PRIVATE NATIVEHOOK_TESTING)
    target_link_libraries(nativehook_tests PRIVATE nativehook::core)
    add_test(NAME nativehook_tests COMMAND nativehook_tests)
endif()
