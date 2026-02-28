cmake_minimum_required(VERSION 3.20)

project(fairy-viewer)

set(CMAKE_BUILD_TYPE Release)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(TOOLS_SCRIPT_DIR ${CMAKE_SOURCE_DIR}/tools)

# executable
set(PROJECT_SOURCE_FILES
	${CMAKE_SOURCE_DIR}/src/fairy_viewer.cpp
	${CMAKE_SOURCE_DIR}/src/render_core/gpu_context.cpp
	${CMAKE_SOURCE_DIR}/src/render_core/gpu_buffer.cpp
	${CMAKE_SOURCE_DIR}/src/render_core/gpu_texture.cpp
	${CMAKE_SOURCE_DIR}/src/render_fairy/fairy_pipeline.cpp
	${CMAKE_SOURCE_DIR}/src/render_fairy/fairy_surface.cpp
)
add_executable(${PROJECT_NAME} ${PROJECT_SOURCE_FILES})

# ktm
add_library(ktm INTERFACE)
target_include_directories(ktm INTERFACE ${CMAKE_SOURCE_DIR}/third_party/ktm)
target_link_libraries(${PROJECT_NAME} PUBLIC ktm)

# sdl3
set(SDL_SHARED ON)
set(SDL_STATIC OFF)
set(SDL_TESTS OFF)
set(SDL_TEST_LIBRARY OFF)
add_subdirectory(${CMAKE_SOURCE_DIR}/third_party/sdl3)
target_link_libraries(${PROJECT_NAME} PUBLIC SDL3-shared)

# sdl3_image
set(SDLIMAGE_BUILD_SHARED_LIBS ON)
set(SDLIMAGE_BUILD_STATIC_LIBS OFF)
set(SDLIMAGE_TESTS OFF)
set(SDLIMAGE_SAMPLES OFF)
add_subdirectory(${CMAKE_SOURCE_DIR}/third_party/sdl3_image)
target_link_libraries(${PROJECT_NAME} PUBLIC SDL3_image-shared)

# imgui
set(IMGUI_SOURCE_DIR ${CMAKE_SOURCE_DIR}/third_party/imgui)
add_library(ImGui STATIC
	${IMGUI_SOURCE_DIR}/imgui.cpp
	${IMGUI_SOURCE_DIR}/imgui_draw.cpp
	${IMGUI_SOURCE_DIR}/imgui_widgets.cpp
	${IMGUI_SOURCE_DIR}/imgui_tables.cpp
	${IMGUI_SOURCE_DIR}/imgui_demo.cpp
	${IMGUI_SOURCE_DIR}/backends/imgui_impl_sdlrenderer3.cpp
	${IMGUI_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
)
target_link_libraries(ImGui PUBLIC SDL3-shared)
target_include_directories(ImGui PUBLIC ${IMGUI_SOURCE_DIR})
target_link_libraries(${PROJECT_NAME} PUBLIC ImGui)

# imgui_colortextedit
set(IMGUI_COLORTEXTEDIT_SOURCE_DIR ${CMAKE_SOURCE_DIR}/third_party/imgui_colortextedit)
add_library(ImGuiColorTextEdit STATIC
	${IMGUI_COLORTEXTEDIT_SOURCE_DIR}/TextEditor.cpp
)
target_include_directories(ImGuiColorTextEdit PRIVATE ${IMGUI_SOURCE_DIR})
target_include_directories(ImGuiColorTextEdit PUBLIC ${IMGUI_COLORTEXTEDIT_SOURCE_DIR})
target_link_libraries(${PROJECT_NAME} PUBLIC ImGuiColorTextEdit)

# vulkan
find_package(Vulkan REQUIRED)
target_link_libraries(${PROJECT_NAME} PUBLIC Vulkan::Vulkan)

# shaderc
find_package(Vulkan REQUIRED COMPONENTS shaderc_combined)
target_link_libraries(${PROJECT_NAME} PUBLIC Vulkan::shaderc_combined)

# vmallocator
set(VMALLOCATOR_SOURCE_DIR ${CMAKE_SOURCE_DIR}/third_party/vmallocator)
set(VMALLOCATOR_BINARY_DIR ${CMAKE_BINARY_DIR}/third_party/vmallocator)
execute_process(
    COMMAND ${CMAKE_COMMAND} 
        -DIMPL_MACRO=VMA_IMPLEMENTATION
        -DHEAD_FILE=${VMALLOCATOR_SOURCE_DIR}/include/vk_mem_alloc.h
        -DOUTPUT_DIR=${VMALLOCATOR_BINARY_DIR}
        -P ${TOOLS_SCRIPT_DIR}/gen_head_impl_cpp.cmake
)
set(VMALLOCATOR_SOURCE_FILES ${VMALLOCATOR_BINARY_DIR}/vk_mem_alloc.h.cpp)
add_library(VmAllocator STATIC ${VMALLOCATOR_SOURCE_FILES})
target_link_libraries(VmAllocator PUBLIC Vulkan::Vulkan)
target_include_directories(VmAllocator PUBLIC ${VMALLOCATOR_SOURCE_DIR}/include)
target_link_libraries(${PROJECT_NAME} PUBLIC VmAllocator)

# shaders
set(SHADER_SOURCE_DIR ${CMAKE_SOURCE_DIR}/shaders)
set(SHADER_BINARY_DIR ${CMAKE_BINARY_DIR}/shaders)
file(GLOB SHADER_FILES_LIST
	${SHADER_SOURCE_DIR}/fairy.vert
	${SHADER_SOURCE_DIR}/fairy.frag
)
string(REPLACE ";" " " SHADER_FILES "${SHADER_FILES_LIST}")
execute_process(
    COMMAND ${CMAKE_COMMAND}
        -DINPUT_SHADER_FILES=${SHADER_FILES}
        -DOUTPUT_DIR=${SHADER_BINARY_DIR}
        -P ${TOOLS_SCRIPT_DIR}/gen_shader_cpp.cmake
)
set(SHADER_CPP_FILES
	${SHADER_BINARY_DIR}/gen/fairy.vert.cpp
	${SHADER_BINARY_DIR}/gen/fairy.frag.cpp
)
add_library(Shader STATIC ${SHADER_CPP_FILES})
target_include_directories(Shader INTERFACE ${SHADER_BINARY_DIR}/gen)
target_link_libraries(${PROJECT_NAME} PUBLIC Shader)

# copy assets
set(ASSETS_SOURCE_DIR ${CMAKE_SOURCE_DIR}/assets)
set(ASSETS_BINARY_DIR ${CMAKE_BINARY_DIR}/assets)
execute_process(
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${ASSETS_SOURCE_DIR} ${ASSETS_BINARY_DIR}
    COMMAND ${CMAKE_COMMAND} -E echo "-- Copying assets directory"
)
target_compile_definitions(${PROJECT_NAME} PRIVATE ASSETS_PATH="${ASSETS_BINARY_DIR}/")