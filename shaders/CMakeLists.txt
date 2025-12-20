# Shader compilation

set(SHADER_FILES
    GlyphVertex.hlsl
    GlyphPixel.hlsl
)

# Find DXC compiler
find_program(DXC_EXECUTABLE dxc
    PATHS
        "$ENV{WindowsSdkDir}bin/$ENV{WindowsSDKVersion}x64"
        "C:/Program Files (x86)/Windows Kits/10/bin/*/x64"
)

if(NOT DXC_EXECUTABLE)
    message(WARNING "DXC shader compiler not found. Shaders will not be compiled automatically.")
    return()
endif()

# Shader compilation commands
set(SHADER_OUTPUT_DIR ${CMAKE_BINARY_DIR}/shaders)
file(MAKE_DIRECTORY ${SHADER_OUTPUT_DIR})

foreach(SHADER_FILE ${SHADER_FILES})
    get_filename_component(SHADER_NAME ${SHADER_FILE} NAME_WE)
    get_filename_component(SHADER_EXT ${SHADER_FILE} EXT)

    # Determine shader profile
    if(${SHADER_FILE} MATCHES "Vertex")
        set(SHADER_PROFILE vs_6_0)
    elseif(${SHADER_FILE} MATCHES "Pixel")
        set(SHADER_PROFILE ps_6_0)
    elseif(${SHADER_FILE} MATCHES "Compute")
        set(SHADER_PROFILE cs_6_0)
    else()
        message(WARNING "Unknown shader type: ${SHADER_FILE}")
        continue()
    endif()

    set(SHADER_INPUT ${CMAKE_CURRENT_SOURCE_DIR}/${SHADER_FILE})
    set(SHADER_OUTPUT ${SHADER_OUTPUT_DIR}/${SHADER_NAME}.cso)

    add_custom_command(
        OUTPUT ${SHADER_OUTPUT}
        COMMAND ${DXC_EXECUTABLE}
            /T ${SHADER_PROFILE}
            /E main
            /Fo ${SHADER_OUTPUT}
            ${SHADER_INPUT}
        DEPENDS ${SHADER_INPUT}
        COMMENT "Compiling shader: ${SHADER_FILE}"
        VERBATIM
    )

    list(APPEND COMPILED_SHADERS ${SHADER_OUTPUT})
endforeach()

# Custom target for all shaders
add_custom_target(Shaders ALL
    DEPENDS ${COMPILED_SHADERS}
)
