if(NOT DEFINED FBJNI_ROOT)
    message(FATAL_ERROR "FBJNI_ROOT must point to the extracted fbjni AAR")
endif()
add_library(fbjni::fbjni SHARED IMPORTED)
set_target_properties(fbjni::fbjni PROPERTIES
    IMPORTED_LOCATION "${FBJNI_ROOT}/prefab/modules/fbjni/libs/android.arm64-v8a/libfbjni.so"
    INTERFACE_INCLUDE_DIRECTORIES "${FBJNI_ROOT}/prefab/modules/fbjni/include")
