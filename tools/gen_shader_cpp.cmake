if(NOT DEFINED INPUT_SHADER_FILES OR NOT DEFINED OUTPUT_DIR)
    message(FATAL_ERROR "Usage: gen_shader_cpp.cmake must define INPUT_SHADER_FILES OUTPUT_DIR")
endif()

get_filename_component(TOOLS_SCRIPT_DIR ${CMAKE_SCRIPT_MODE_FILE} DIRECTORY)
separate_arguments(SHADER_FILES NATIVE_COMMAND ${INPUT_SHADER_FILES})
execute_process(
    COMMAND ${CMAKE_COMMAND} -E make_directory ${OUTPUT_DIR}/bin
    COMMAND ${CMAKE_COMMAND} -E make_directory ${OUTPUT_DIR}/gen
)
foreach(SHADER_FILE IN LISTS SHADER_FILES)
    get_filename_component(FILE_NAME ${SHADER_FILE} NAME)
    if(EXISTS ${OUTPUT_DIR}/bin/${FILE_NAME}.spv)
        file(TIMESTAMP ${SHADER_FILE} SOURCE_TIME "%Y%m%d%H%M%S")
        file(TIMESTAMP ${OUTPUT_DIR}/bin/${FILE_NAME}.spv TARGET_TIME "%Y%m%d%H%M%S")
    endif()
    if((NOT EXISTS ${OUTPUT_DIR}/bin/${FILE_NAME}.spv) OR ${SOURCE_TIME} GREATER ${TARGET_TIME})
        # compile shader
        execute_process(
            COMMAND glslc ${SHADER_FILE} -o ${OUTPUT_DIR}/bin/${FILE_NAME}.spv
            COMMAND ${CMAKE_COMMAND} -E echo "-- Compiling ${FILE_NAME}"
        )
    endif()

    string(REPLACE "." "_" VARIABLE ${FILE_NAME})
    if(EXISTS ${OUTPUT_DIR}/gen/${FILE_NAME}.cpp)
        file(TIMESTAMP ${OUTPUT_DIR}/bin/${FILE_NAME}.spv SOURCE_TIME "%Y%m%d%H%M%S")
        file(TIMESTAMP ${OUTPUT_DIR}/gen/${FILE_NAME}.cpp TARGET_TIME "%Y%m%d%H%M%S")
    endif()
    if((NOT EXISTS ${OUTPUT_DIR}/gen/${FILE_NAME}.cpp) OR ${SOURCE_TIME} GREATER ${TARGET_TIME})
        # generate shader variable cpp
        execute_process(
            COMMAND ${CMAKE_COMMAND} 
                -DINPUT_FILE=${OUTPUT_DIR}/bin/${FILE_NAME}.spv
                -DOUTPUT_FILE=${OUTPUT_DIR}/gen/${FILE_NAME}.cpp
                -DVARIABLE_NAME_SPACE=shader
                -DVARIABLE_NAME=${VARIABLE}
                -P ${TOOLS_SCRIPT_DIR}/gen_bin_cpp.cmake
            COMMAND ${CMAKE_COMMAND} -E echo "-- Generate ${FILE_NAME}.cpp"
        )
    endif()

    if(NOT EXISTS ${OUTPUT_DIR}/gen/${FILE_NAME}.hpp)
        # generate shader variable header
        set(CPP_CONTENT "// Auto Generate By gen_shader_cpp.cmake\n\n#pragma once\n\n")
        string(APPEND CPP_CONTENT "namespace shader\n{\n\n")
        string(APPEND CPP_CONTENT "extern unsigned char ${VARIABLE}[]\;\n")
        string(APPEND CPP_CONTENT "extern unsigned int ${VARIABLE}_len\;\n")
        string(APPEND CPP_CONTENT "\n}\n")
        file(WRITE ${OUTPUT_DIR}/gen/${FILE_NAME}.hpp ${CPP_CONTENT})
        execute_process(
            COMMAND ${CMAKE_COMMAND} -E echo "-- Generate ${FILE_NAME}.hpp"
        )
        set(GENERATE_SHADER_LIST_CPP ON)
    endif()

    list(APPEND SHADER_VARIABLES ${VARIABLE})
    list(APPEND SHADER_VARIABLES_HEADERS ${FILE_NAME}.hpp)
endforeach(SHADER_FILE IN LISTS SHADERS)

if(GENERATE_SHADER_LIST_CPP)
    set(CPP_CONTENT "// Auto Generate By gen_shader_cpp.cmake\n\n#pragma once\n\n")
    string(APPEND CPP_CONTENT "#include <initializer_list>\n")
    string(APPEND CPP_CONTENT "#include <utility>\n")

    foreach(SHADER_VARIABLES_HEADER IN LISTS SHADER_VARIABLES_HEADERS)
        string(APPEND CPP_CONTENT "#include \"${SHADER_VARIABLES_HEADER}\"\n")
    endforeach(SHADER_VARIABLES_HEADER IN LISTS SHADER_VARIABLES_HEADERS)
    string(APPEND CPP_CONTENT "\n")

    string(APPEND CPP_CONTENT "namespace shader\n{\n\n")
    string(APPEND CPP_CONTENT "const std::initializer_list<std::pair<unsigned char*, unsigned int>> shaders {\n")
    foreach(SHADER_VARIABLE IN LISTS SHADER_VARIABLES)
        string(APPEND CPP_CONTENT "  { ${SHADER_VARIABLE}, ${SHADER_VARIABLE}_len },\n")
    endforeach(SHADER_VARIABLE IN LISTS SHADER_VARIABLES)
    string(APPEND CPP_CONTENT "}\;\n")
    string(APPEND CPP_CONTENT "\n}\n")
    file(WRITE ${OUTPUT_DIR}/gen/shaders.hpp ${CPP_CONTENT})

    execute_process(
        COMMAND ${CMAKE_COMMAND} -E echo "-- Generate shaders.hpp"
    )
endif()