if(NOT DEFINED CLANG OR NOT DEFINED INCLUDE)
    message(FATAL_ERROR "ARM64 ShadowCallStack guard test is missing a required path")
endif()

execute_process(
    COMMAND "${CLANG}" --target=aarch64-linux-android24
            -fsanitize=shadow-call-stack -fsyntax-only -x c++
            "-I${INCLUDE}" -include puredart/hook.h /dev/null
    RESULT_VARIABLE compile_result
    OUTPUT_VARIABLE compile_output
    ERROR_VARIABLE compile_error
)

if(compile_result EQUAL 0)
    message(FATAL_ERROR "puredart/hook.h accepted ShadowCallStack-enabled callback code")
endif()

set(expected "puredart callbacks cannot use ShadowCallStack")
string(FIND "${compile_output}${compile_error}" "${expected}" guard_error)
if(guard_error EQUAL -1)
    message(FATAL_ERROR
        "compilation failed without the expected ShadowCallStack diagnostic:\n"
        "${compile_output}${compile_error}")
endif()
