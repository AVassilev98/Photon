find_package(glfw3 QUIET)

if(NOT glfw3_FOUND)
    message(WARNING "GLFW3 not found — tests/ExampleApp will not be built.")
    return()
endif()

add_executable(ExampleApp example.c)

target_link_libraries(ExampleApp PRIVATE photon glfw)

target_compile_options(ExampleApp PRIVATE
    $<$<C_COMPILER_ID:GNU,Clang>:-Wall -Wextra>
)
