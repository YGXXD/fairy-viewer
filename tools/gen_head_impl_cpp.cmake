if((NOT DEFINED IMPL_MACRO) OR (NOT DEFINE HEAD_FILE) OR (NOT DEFINED OUTPUT_DIR))
    message(FATAL_ERROR "Usage: gen_head_impl_cpp.cmake must define IMPL_MACRO HEAD_FILE OUTPUT_DIR")
endif()

execute_process(
    COMMAND ${CMAKE_COMMAND} -E make_directory ${OUTPUT_DIR}
)
get_filename_component(FILE_NAME ${HEAD_FILE} NAME)
if(EXISTS ${OUTPUT_DIR}/${FILE_NAME}.cpp)
    file(TIMESTAMP ${HEAD_FILE} SOURCE_TIME "%Y%m%d%H%M%S")
    file(TIMESTAMP ${OUTPUT_DIR}/${FILE_NAME}.cpp TARGET_TIME "%Y%m%d%H%M%S")
endif()
if(NOT EXISTS ${OUTPUT_DIR}/${FILE_NAME}.cpp OR ${SOURCE_TIME} GREATER ${TARGET_TIME})
    set(CPP_CONTENT "// Auto Generate By gen_head_impl_cpp.cmake\n\n#define ${IMPL_MACRO}\n#include \"${HEAD_FILE}\";\n\n")
    file(WRITE ${OUTPUT_DIR}/${FILE_NAME}.cpp ${CPP_CONTENT})
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E echo "-- Generate ${FILE_NAME}.cpp"
    )
endif()
