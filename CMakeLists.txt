cmake_minimum_required(VERSION 3.20)
project(Photon VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Options
option(PHOTON_BUILD_EXAMPLES  "Build example applications" ON)
option(PHOTON_BUILD_TESTS     "Build tests"                ON)
option(PHOTON_ENABLE_VALIDATION_DEFAULT "Enable Vulkan validation layers in debug builds" ON)

# Dependencies
if(APPLE AND NOT DEFINED ENV{VULKAN_SDK})
    execute_process(
        COMMAND brew --prefix vulkan-loader
        OUTPUT_VARIABLE VULKAN_LOADER_PREFIX
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    execute_process(
        COMMAND brew --prefix molten-vk
        OUTPUT_VARIABLE MOLTENVK_PREFIX
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    execute_process(
        COMMAND brew --prefix vulkan-headers
        OUTPUT_VARIABLE VULKAN_HEADERS_PREFIX
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(VULKAN_LOADER_PREFIX)
        set(Vulkan_LIBRARY     "${VULKAN_LOADER_PREFIX}/lib/libvulkan.dylib"  CACHE PATH "Vulkan loader library" FORCE)
        set(Vulkan_INCLUDE_DIR "${VULKAN_HEADERS_PREFIX}/include"             CACHE PATH "Vulkan include dir"    FORCE)
        list(APPEND CMAKE_BUILD_RPATH "${VULKAN_LOADER_PREFIX}/lib")
    elseif(MOLTENVK_PREFIX)
        set(Vulkan_LIBRARY     "${MOLTENVK_PREFIX}/lib/libMoltenVK.dylib" CACHE PATH "MoltenVK library"   FORCE)
        set(Vulkan_INCLUDE_DIR "${MOLTENVK_PREFIX}/libexec/include"       CACHE PATH "MoltenVK include dir" FORCE)
        list(APPEND CMAKE_BUILD_RPATH "${MOLTENVK_PREFIX}/lib")
    endif()
endif()
find_package(Vulkan REQUIRED)

include(FetchContent)
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.4
    GIT_SHALLOW    TRUE
)
set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ALL_EXPORTERS_BY_DEFAULT OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    assimp
    GIT_REPOSITORY https://github.com/assimp/assimp.git
    GIT_TAG        v5.4.3
    GIT_SHALLOW    TRUE
)

FetchContent_Declare(
    spirv_reflect
    GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Reflect.git
    GIT_TAG        main
    GIT_SHALLOW    TRUE
)
set(SPIRV_REFLECT_EXECUTABLE     OFF CACHE BOOL "" FORCE)
set(SPIRV_REFLECT_EXAMPLES       OFF CACHE BOOL "" FORCE)
set(SPIRV_REFLECT_STATIC_LIB     ON  CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(spirv_reflect assimp)

# foundation (standalone container/utility library, consumed as a git submodule)
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/extern/foundation/CMakeLists.txt")
    message(FATAL_ERROR
        "foundation submodule not found.\n"
        "Run: git submodule update --init --recursive")
endif()
add_subdirectory(extern/foundation)

# Core photon library
add_library(photon
    src/photon_log.c
    src/photon_status.c
    src/photon_core.c
    src/photon_instance.c
    src/photon_window.c
    src/photon_device.c
    src/photon_shader.c
    src/photon_pipeline.c
)

target_include_directories(photon
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(photon
    PUBLIC
        Vulkan::Vulkan
        foundation
        glfw
        assimp::assimp
        spirv-reflect-static
)

if(PHOTON_ENABLE_VALIDATION_DEFAULT)
    target_compile_definitions(photon PUBLIC
        $<$<CONFIG:Debug>:PHOTON_ENABLE_VALIDATION>
    )
endif()

target_compile_options(photon PRIVATE
    $<$<C_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
    $<$<C_COMPILER_ID:MSVC>:/W4>
)

if(PHOTON_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(PHOTON_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
