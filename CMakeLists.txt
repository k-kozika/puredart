cmake_minimum_required(VERSION 3.22)
project(puredart VERSION 0.1.0 LANGUAGES C CXX)

option(PUREDART_BUILD_TESTS "Build puredart host unit tests" ${PROJECT_IS_TOP_LEVEL})
option(PUREDART_ENABLE_RUNTIME_HOOKS "Build the Dart AOT hook adapter" ON)

set(PUREDART_SOURCES
    src/meta.cpp
    src/vm.cpp
)

if(PUREDART_ENABLE_RUNTIME_HOOKS)
    list(APPEND PUREDART_SOURCES
        src/bootstrap.cpp
        src/hook.cpp
        src/puredart.cpp
    )
endif()

if(ANDROID AND PUREDART_ENABLE_RUNTIME_HOOKS AND
   CMAKE_ANDROID_ARCH_ABI STREQUAL "arm64-v8a")
    enable_language(ASM)
    list(APPEND PUREDART_SOURCES src/hook_arm64.S)
endif()

set(NATIVEHOOK_BUILD_TESTS ${PUREDART_BUILD_TESTS} CACHE BOOL "Build native hook tests")
add_subdirectory(native)

add_library(puredart_core STATIC ${PUREDART_SOURCES})
add_library(puredart::core ALIAS puredart_core)

target_compile_features(puredart_core PUBLIC cxx_std_17)
target_include_directories(puredart_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
set_target_properties(puredart_core PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

if(PUREDART_ENABLE_RUNTIME_HOOKS)
    target_link_libraries(puredart_core PRIVATE nativehook::core)
endif()

if(ANDROID)
    if(NOT CMAKE_ANDROID_ARCH_ABI STREQUAL "arm64-v8a")
        message(STATUS "puredart: runtime hooks are disabled for ${CMAKE_ANDROID_ARCH_ABI}")
    endif()
    target_link_libraries(puredart_core PRIVATE android dl log)
    if(CMAKE_ANDROID_ARCH_ABI STREQUAL "arm64-v8a")
        # Dart AOT does not maintain Android's x18 shadow-call-stack register.
        # Propagate the opt-out because std::function's callback invoker is
        # instantiated in the consuming target, not in puredart_core.
        target_compile_options(puredart_core
            PUBLIC -fno-sanitize=shadow-call-stack -mbranch-protection=standard)
    endif()
endif()

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/puredart.cmake)

if(PUREDART_BUILD_TESTS AND NOT ANDROID)
    enable_testing()
    find_package(Threads REQUIRED)
    if(PUREDART_ENABLE_RUNTIME_HOOKS)
        target_compile_definitions(puredart_core PRIVATE PUREDART_TESTING)
    endif()
    add_executable(puredart_tests tests/meta_fixture.cpp tests/puredart_tests.cpp)
    if(PUREDART_ENABLE_RUNTIME_HOOKS)
        target_compile_definitions(puredart_tests PRIVATE PUREDART_TESTING)
    endif()
    target_include_directories(puredart_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tools/blutter)
    target_link_libraries(puredart_tests PRIVATE puredart::core Threads::Threads)
    add_test(NAME puredart_tests COMMAND puredart_tests)

    find_program(PUREDART_CLANG NAMES clang)
    find_program(PUREDART_LLVM_READELF NAMES llvm-readelf)
    find_program(PUREDART_LLVM_OBJDUMP NAMES llvm-objdump)
    if(PUREDART_CLANG AND PUREDART_LLVM_READELF AND PUREDART_LLVM_OBJDUMP)
        add_test(NAME puredart_arm64_cfi
            COMMAND ${CMAKE_COMMAND}
                -DCLANG=${PUREDART_CLANG}
                -DLLVM_READELF=${PUREDART_LLVM_READELF}
                -DLLVM_OBJDUMP=${PUREDART_LLVM_OBJDUMP}
                -DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/src/hook_arm64.S
                -DOBJECT=${CMAKE_CURRENT_BINARY_DIR}/hook_arm64_cfi_test.o
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_arm64_cfi.cmake)
        add_test(NAME puredart_arm64_scs_guard
            COMMAND ${CMAKE_COMMAND}
                -DCLANG=${PUREDART_CLANG}
                -DINCLUDE=${CMAKE_CURRENT_SOURCE_DIR}/include
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_arm64_scs_guard.cmake)
    endif()
endif()

option(PUREDART_BUILD_DEVICE_TESTS "Build the Android ARM64 runtime test executable" OFF)
if(PUREDART_BUILD_DEVICE_TESTS)
    if(NOT ANDROID OR NOT CMAKE_ANDROID_ARCH_ABI STREQUAL "arm64-v8a" OR
       NOT PUREDART_ENABLE_RUNTIME_HOOKS)
        message(FATAL_ERROR "Device tests require Android ARM64 runtime hooks")
    endif()
    add_executable(puredart_arm64_runtime_tests tests/arm64_runtime_tests.cpp
        tests/arm64_runtime_targets.S tests/meta_fixture.cpp)
    target_link_libraries(puredart_arm64_runtime_tests PRIVATE puredart::core nativehook::core)
    target_compile_options(puredart_arm64_runtime_tests PRIVATE -mbranch-protection=standard)
endif()
