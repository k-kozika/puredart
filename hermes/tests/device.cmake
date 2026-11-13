# Included by the checked-in Android harness project.
enable_language(ASM)
add_library(test SHARED "${REPOSITORY}/hermes/tests/device.cpp" "${REPOSITORY}/hermes/tests/native_targets.S")
target_include_directories(test PRIVATE "${TOOLCHAIN}" "${TOOLCHAIN}/source/API" "${TOOLCHAIN}/source/public" "${TOOLCHAIN}/source/include" "${TOOLCHAIN}/source/API/jsi" "${TOOLCHAIN}/android/include" "${TOOLCHAIN}/android/lib/config")
add_library(vm SHARED IMPORTED)
set_target_properties(vm PROPERTIES IMPORTED_LOCATION "${VM_LIBRARY}")
target_link_libraries(test PRIVATE purehermes::core purenative::core vm log dl)
