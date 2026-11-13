foreach(case IN ITEMS hermes native-first hermes-first all reader)
    set(work "${WORK}/fixtures/${case}")
    execute_process(COMMAND "${CMAKE_COMMAND}"
        -S "${CMAKE_CURRENT_LIST_DIR}/consumer" -B "${work}/build"
        "-DREPOSITORY=${REPOSITORY}" "-DCONSUMER_CASE=${case}"
        RESULT_VARIABLE status OUTPUT_VARIABLE out ERROR_VARIABLE error)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "${case}: ${out}\n${error}")
    endif()
    execute_process(COMMAND "${CMAKE_COMMAND}" --build "${work}/build" --parallel 2 RESULT_VARIABLE status OUTPUT_VARIABLE out ERROR_VARIABLE error)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "${case}: ${out}\n${error}")
    endif()
    execute_process(COMMAND "${work}/build/consumer" RESULT_VARIABLE status)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "${case}: consumer failed")
    endif()
endforeach()
