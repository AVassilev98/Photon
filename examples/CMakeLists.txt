add_executable(ExampleApp example.c)

target_link_libraries(ExampleApp PRIVATE photon glfw)

target_compile_options(ExampleApp PRIVATE
    $<$<C_COMPILER_ID:GNU,Clang>:-Wall -Wextra>
)

find_program(SLANGC slangc REQUIRED)

set(SHADER_SOURCES
    shaders/triangle.slang
)

set(SHADER_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/shaders)
file(MAKE_DIRECTORY ${SHADER_OUTPUT_DIR})

foreach(SHADER ${SHADER_SOURCES})
    get_filename_component(SHADER_NAME ${SHADER} NAME)
    string(REPLACE ".slang" ".spv" OUTPUT_NAME ${SHADER_NAME})

    set(OUTPUT_FILE ${SHADER_OUTPUT_DIR}/${OUTPUT_NAME})

    add_custom_command(
        OUTPUT  ${OUTPUT_FILE}
        COMMAND ${SLANGC} ${CMAKE_CURRENT_SOURCE_DIR}/${SHADER} -o ${OUTPUT_FILE} -target spirv
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${SHADER}
        COMMENT "Compiling ${SHADER}"
    )

    list(APPEND SHADER_OUTPUTS ${OUTPUT_FILE})
endforeach()

add_custom_target(ExampleShaders ALL DEPENDS ${SHADER_OUTPUTS})
add_dependencies(ExampleApp ExampleShaders)

target_compile_definitions(ExampleApp PRIVATE
    SHADER_DIR="${SHADER_OUTPUT_DIR}"
)
