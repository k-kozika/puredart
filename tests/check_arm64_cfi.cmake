if(NOT DEFINED CLANG OR NOT DEFINED LLVM_READELF OR NOT DEFINED LLVM_OBJDUMP OR
   NOT DEFINED SOURCE OR NOT DEFINED OBJECT)
    message(FATAL_ERROR "ARM64 CFI test is missing a required path")
endif()

execute_process(
    COMMAND "${CLANG}" --target=aarch64-linux-android24 -mbranch-protection=standard
            -c "${SOURCE}" -o "${OBJECT}"
    RESULT_VARIABLE assemble_result
    OUTPUT_VARIABLE assemble_output
    ERROR_VARIABLE assemble_error
)
if(NOT assemble_result EQUAL 0)
    message(FATAL_ERROR "ARM64 assembly failed:\n${assemble_output}${assemble_error}")
endif()

execute_process(
    COMMAND "${LLVM_OBJDUMP}" -d "${OBJECT}"
    RESULT_VARIABLE objdump_result
    OUTPUT_VARIABLE disassembly
    ERROR_VARIABLE objdump_error
)
if(NOT objdump_result EQUAL 0)
    message(FATAL_ERROR "disassembling ARM64 shim failed:\n${objdump_error}")
endif()

string(REGEX MATCHALL "bti[ \t]+c" bti_landings "${disassembly}")
list(LENGTH bti_landings bti_count)
if(NOT bti_count EQUAL 16)
    message(FATAL_ERROR "expected 16 BTI call landing pads, found ${bti_count}:\n${disassembly}")
endif()

execute_process(
    COMMAND "${LLVM_READELF}" --unwind "${OBJECT}"
    RESULT_VARIABLE readelf_result
    OUTPUT_VARIABLE unwind
    ERROR_VARIABLE readelf_error
)
if(NOT readelf_result EQUAL 0)
    message(FATAL_ERROR "reading unwind data failed:\n${readelf_error}")
endif()

execute_process(
    COMMAND "${LLVM_READELF}" --notes "${OBJECT}"
    RESULT_VARIABLE notes_result
    OUTPUT_VARIABLE notes
    ERROR_VARIABLE notes_error
)
if(NOT notes_result EQUAL 0)
    message(FATAL_ERROR "reading ARM64 ELF notes failed:\n${notes_error}")
endif()

string(TOLOWER "${notes}" notes_lower)
string(FIND "${notes_lower}" "aarch64 feature: bti" bti_property)
if(bti_property EQUAL -1)
    message(FATAL_ERROR "ARM64 object does not advertise the BTI GNU property:\n${notes}")
endif()

foreach(required
        "DW_CFA_def_cfa_offset: +544"
        "DW_CFA_offset: reg19 -112"
        "DW_CFA_offset: reg29 -32"
        "DW_CFA_offset: reg30 -24"
        "DW_CFA_restore_state:")
    string(FIND "${unwind}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "missing '${required}' in ARM64 unwind data:\n${unwind}")
    endif()
endforeach()

string(REGEX MATCHALL "FDE length=" fdes "${unwind}")
list(LENGTH fdes fde_count)
if(NOT fde_count EQUAL 17)
    message(FATAL_ERROR "expected CFI for 16 proxies and common shim, found ${fde_count}")
endif()
