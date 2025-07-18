cmake_minimum_required (VERSION 3.8)

project (VkEngine LANGUAGES CXX)

set (CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin")
set (CMAKE_LIBRARY_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)


find_package(Vulkan REQUIRED)

include(FetchContent)

FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        1.0.1
)
FetchContent_MakeAvailable(glm)


FetchContent_Declare(
    fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt.git
    GIT_TAG        11.2.0
)
FetchContent_MakeAvailable(fmt)


FetchContent_Declare(
    SDL2
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG        "SDL2"
)
FetchContent_MakeAvailable(SDL2)


FetchContent_Declare(
    fastgltf
    GIT_REPOSITORY https://github.com/spnda/fastgltf.git
    GIT_TAG        v0.8.0
)
FetchContent_MakeAvailable(fastgltf)


FetchContent_Declare(
    volk
    GIT_REPOSITORY https://github.com/zeux/volk.git
    GIT_TAG        1.4.304
)
FetchContent_MakeAvailable(volk)


FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.91.9b
)
FetchContent_MakeAvailable(imgui)

add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.h
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
)

target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR})
target_link_libraries(imgui PUBLIC Vulkan::Vulkan SDL2::SDL2)


FetchContent_Declare(
    stb_image
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG        "HEAD"
)
FetchContent_MakeAvailable(stb_image)

add_library(stb_image INTERFACE)
target_include_directories(stb_image INTERFACE ${stb_image_SOURCE_DIR})


FetchContent_Declare(
    vkbootstrap
    GIT_REPOSITORY https://github.com/charles-lunarg/vk-bootstrap.git
    GIT_TAG        v1.4.319
)
FetchContent_MakeAvailable(vkbootstrap)


FetchContent_Declare(
    vma
    GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
    GIT_TAG        v3.3.0
)
FetchContent_MakeAvailable(vma)

add_library(vma INTERFACE)
target_include_directories(vma INTERFACE ${vma_SOURCE_DIR}/include)


file(GLOB_RECURSE CPP_SOURCE_FILES
    ${PROJECT_SOURCE_DIR}/src/*.h
    ${PROJECT_SOURCE_DIR}/src/*.hpp
    ${PROJECT_SOURCE_DIR}/src/*.cpp
)

add_executable(engine ${CPP_SOURCE_FILES})

target_compile_definitions(engine PUBLIC GLM_FORCE_DEPTH_ZERO_TO_ONE)

target_include_directories(engine 
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# target_compile_options(engine PRIVATE
#     $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>: -Wall -Wextra -Wpedantic>
#     $<$<CXX_COMPILER_ID:MSVC>: /W4 /WX>)

target_link_libraries(engine PUBLIC vma glm imgui stb_image Vulkan::Vulkan fmt::fmt SDL2::SDL2 vk-bootstrap::vk-bootstrap fastgltf::fastgltf)

target_precompile_headers(engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/pch.h)

add_custom_command(TARGET engine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:engine> $<TARGET_FILE_DIR:engine>
    COMMAND_EXPAND_LISTS
)

if (${CMAKE_HOST_SYSTEM_PROCESSOR} STREQUAL "AMD64")
    set(ENG_GLSL_VALIDATOR $ENV{VULKAN_SDK}/Bin/glslc.exe)
else()
    set(ENG_GLSL_VALIDATOR $ENV{VULKAN_SDK}/Bin32/glslc.exe)
endif()

set(ENG_SHADERS_DIR ${PROJECT_SOURCE_DIR}/shaders)
set(ENG_SHADERS_SRC_DIR ${ENG_SHADERS_DIR}/src)
set(ENG_SHADERS_BIN_DIR ${ENG_SHADERS_DIR}/bin)

file(GLOB_RECURSE SHADER_SRC_FILES
    ${ENG_SHADERS_SRC_DIR}/*.frag  
    ${ENG_SHADERS_SRC_DIR}/*.vert
    ${ENG_SHADERS_SRC_DIR}/*.comp
)

foreach(SHADER_SRC_FILE ${SHADER_SRC_FILES})
    get_filename_component(FILE_NAME ${SHADER_SRC_FILE} NAME)
    
    set(SHADER_SPIRV ${ENG_SHADERS_BIN_DIR}/${FILE_NAME}.spv)
    
    message(STATUS "${SHADER_SRC_FILE} will be compiled to ${SHADER_SPIRV}")
    
    add_custom_command(
        OUTPUT ${SHADER_SPIRV}
        COMMAND ${ENG_GLSL_VALIDATOR} ${SHADER_SRC_FILE} -o ${SHADER_SPIRV}
        DEPENDS ${SHADER_SRC_FILE})
    list(APPEND SPIRV_BINARY_FILES ${SHADER_SPIRV})
endforeach(SHADER_SRC_FILE)

add_custom_target(shaders DEPENDS ${SPIRV_BINARY_FILES})
add_dependencies(engine shaders)
