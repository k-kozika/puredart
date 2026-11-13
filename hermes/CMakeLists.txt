cmake_minimum_required(VERSION 3.22)
if(TARGET purehermes::core)
    return()
endif()
project(purehermes VERSION 0.1.0 LANGUAGES CXX)
option(PUREHERMES_BUILD_TESTS "Build purehermes host tests" ${PROJECT_IS_TOP_LEVEL})
option(PUREHERMES_ENABLE_RUNTIME_HOOKS "Build Android ARM64 VM hooks" ON)
option(PUREHERMES_BUILD_TOOLS "Build the metadata inspection tool" ${PROJECT_IS_TOP_LEVEL})
add_library(purehermes_core STATIC src/bytecode.cpp src/runtime.cpp)
add_library(purehermes::core ALIAS purehermes_core)
target_compile_features(purehermes_core PUBLIC cxx_std_17)
target_include_directories(purehermes_core PUBLIC include PRIVATE src)
set_target_properties(purehermes_core PROPERTIES POSITION_INDEPENDENT_CODE ON CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN ON)
if(ANDROID AND ANDROID_ABI STREQUAL "arm64-v8a" AND PUREHERMES_ENABLE_RUNTIME_HOOKS)
    enable_language(ASM)
    if(NOT TARGET purenative::core)
        add_subdirectory(../native "${CMAKE_CURRENT_BINARY_DIR}/native")
    endif()
    target_sources(purehermes_core PRIVATE src/dispatch_arm64.S)
    target_compile_definitions(purehermes_core PRIVATE PUREHERMES_ANDROID_RUNTIME)
    target_link_libraries(purehermes_core PRIVATE purenative::core log dl)
    target_compile_options(purehermes_core PRIVATE -mbranch-protection=standard)
endif()
if(PUREHERMES_BUILD_TOOLS)
    add_executable(purehermes-meta tools/meta.cpp)
    target_link_libraries(purehermes-meta PRIVATE purehermes::core)
endif()
if(PUREHERMES_BUILD_TESTS AND NOT ANDROID)
    enable_testing()
    add_executable(purehermes_tests tests/tests.cpp)
    target_link_libraries(purehermes_tests PRIVATE purehermes::core)
    target_compile_definitions(purehermes_tests PRIVATE FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
    add_test(NAME purehermes_tests COMMAND purehermes_tests)
    if(PUREHERMES_BUILD_TOOLS)
        add_test(NAME purehermes_metadata_cli COMMAND ${CMAKE_COMMAND}
            -DTOOL=$<TARGET_FILE:purehermes-meta>
            -DFIXTURE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/v99.hbc
            -DINCLUDE=${CMAKE_CURRENT_SOURCE_DIR}/include
            -DCXX=${CMAKE_CXX_COMPILER}
            -DWORK=${CMAKE_CURRENT_BINARY_DIR}/metadata-cli-tests
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_meta_cli.cmake)
    endif()
    find_program(PUREHERMES_CLANG clang)
    find_program(PUREHERMES_DWARFDUMP llvm-dwarfdump)
    find_program(PUREHERMES_OBJDUMP llvm-objdump)
    if(PUREHERMES_CLANG AND PUREHERMES_DWARFDUMP AND PUREHERMES_OBJDUMP)
        add_test(NAME purehermes_cfi COMMAND ${CMAKE_COMMAND}
            -DCLANG=${PUREHERMES_CLANG} -DREADELF=${PUREHERMES_DWARFDUMP}
            -DOBJDUMP=${PUREHERMES_OBJDUMP}
            -DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/src/dispatch_arm64.S
            -DOBJECT=${CMAKE_CURRENT_BINARY_DIR}/dispatch-test.o
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_cfi.cmake)
    endif()
    add_test(NAME purehermes_consumers COMMAND ${CMAKE_COMMAND}
        -DREPOSITORY=${CMAKE_CURRENT_SOURCE_DIR}/..
        -DWORK=${CMAKE_CURRENT_BINARY_DIR}/consumer-tests
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_consumers.cmake)
endif()
