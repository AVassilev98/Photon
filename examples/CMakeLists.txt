add_executable(ExampleApp example.c)

target_link_libraries(ExampleApp PRIVATE photon glfw)

target_compile_options(ExampleApp PRIVATE
    $<$<C_COMPILER_ID:GNU,Clang>:-Wall -Wextra>
)
