get_filename_component(_PUREDART_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(PUREDART_META_DIR "${_PUREDART_ROOT}/meta"
    CACHE PATH "Root directory for generated puredart metadata sources")

function(puredart_add_meta target meta)
    if(NOT TARGET "${target}")
        message(FATAL_ERROR "puredart: target does not exist: ${target}")
    endif()
    if(IS_ABSOLUTE "${meta}")
        set(meta_file "${meta}")
    else()
        set(meta_file "${PUREDART_META_DIR}/${meta}")
    endif()
    get_filename_component(meta_file "${meta_file}" ABSOLUTE)
    if(NOT EXISTS "${meta_file}")
        message(FATAL_ERROR
            "puredart: metadata not found: ${meta_file}\n"
            "Generate puredart_meta.cpp with blutter or set -DPUREDART_META_DIR.")
    endif()
    target_sources("${target}" PRIVATE "${meta_file}")
endfunction()
