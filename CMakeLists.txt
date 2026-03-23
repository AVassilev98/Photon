cmake_minimum_required(VERSION 3.20)
project(Photon VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Options
option(PHOTON_BUILD_EXAMPLES  "Build example applications" ON)
option(PHOTON_BUILD_TESTS     "Build tests"                ON)
option(PHOTON_ENABLE_VALIDATION_DEFAULT "Enable Vulkan validation layers in debug builds" ON)

# Dependencies
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
