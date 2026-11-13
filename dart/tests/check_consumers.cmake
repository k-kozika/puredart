cmake_minimum_required(VERSION 3.22)

foreach(case IN ITEMS native dart native-first dart-first metadata)
    set(build "${WORK}/${case}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}"
            -S "${REPOSITORY}/dart/tests/consumer" -B "${build}"
            -G "${GENERATOR}" "-DCMAKE_CXX_COMPILER=${CXX}"
            "-DREPOSITORY=${REPOSITORY}" "-DCONSUMER_CASE=${case}"
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "${case} configure failed:\n${output}\n${error}")
    endif()
    execute_process(
        COMMAND "${CMAKE_COMMAND}" --build "${build}" --config Debug --parallel 2
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "${case} build failed:\n${output}\n${error}")
    endif()
    execute_process(
        COMMAND "${CMAKE_CTEST_COMMAND}" --test-dir "${build}"
            -C Debug --output-on-failure
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "${case} consumer failed:\n${output}\n${error}")
    endif()
endforeach()
