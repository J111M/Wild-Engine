cmake_minimum_required(VERSION 3.16)
project(Wild)

# C++ standard version
set(CMAKE_CXX_STANDARD 17 CACHE STRING "" FORCE)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

#set(CMAKE_CXX_CLANG_TIDY "clang-tidy;-checks=clang-diagnostic-*,clang-analyzer-*")


# Program executable target
add_executable(Wild)
set_target_properties(Wild PROPERTIES OUTPUT_NAME ${CMAKE_PROJECT_NAME})

# Add compiler (debug & release) definitions
target_compile_definitions(Wild PRIVATE $<$<CONFIG:Debug>:DEBUG=1>)
target_compile_definitions(Wild PRIVATE $<$<CONFIG:Release>:NDEBUG=1>)

# Add WINDOWS compiler macro
target_compile_definitions(Wild PRIVATE "WINDOWS=$<STREQUAL:${CMAKE_SYSTEM_NAME},Windows>")

# Includes
target_include_directories(Wild PRIVATE "./Wild")

# Pre-compiled header
target_precompile_headers(Wild PRIVATE "./Wild/precomp.hpp")

# Exclude mikktspcae from my precomp header
set_source_files_properties(
    Wild/Tools/TangentGeneration/mikktspace.c
    PROPERTIES SKIP_PRECOMPILE_HEADERS ON
)

# Glob all source files
file(GLOB_RECURSE SOURCES
	"Wild/*.cpp"
	"Wild/*.c"
)

# Add source files to the program
target_sources(Wild PRIVATE
	${SOURCES}
)

# Sub-directories
add_subdirectory("Vendor")

add_custom_command(TARGET Wild POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_SOURCE_DIR}/ImGuiSettings.ini
            $<TARGET_FILE_DIR:Wild>/ImGuiSettings.ini
    COMMENT "Copying ImGui layout to output directory"
)

# Clang-Tidy flag
#set_target_properties(Wild PROPERTIES CXX_CLANG_TIDY "clang-tidy;-checks=clang-diagnostic-*,clang-analyzer-*" )

# Shader directories
set(SHADER_SRC_DIR "${CMAKE_SOURCE_DIR}/Shaders")
#set(SHADER_OUT_DIR "${CMAKE_SOURCE_DIR}/Shaders/spirv")

target_compile_definitions(Wild PRIVATE
    SHADER_SOURCE_DIR="${CMAKE_SOURCE_DIR}/Shaders"
    ASSETS_SOURCE_DIR="${CMAKE_SOURCE_DIR}/Assets"
)

set_target_properties(Wild PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/Release
)

# Always copy the assets folder after building
add_custom_command(TARGET Wild POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/Assets" "$<TARGET_FILE_DIR:Wild>/Assets"
    COMMENT "Copying assets folders to output directory"
)

add_custom_command(TARGET Wild POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/Shaders $<TARGET_FILE_DIR:Wild>/Shaders
    COMMENT "Copying shader folder to output directory"
)

# DirectX 12 shader compiler
add_custom_command(TARGET Wild POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_SOURCE_DIR}/vendor/dxc/bin/"
    "$<TARGET_FILE_DIR:Wild>/"
)

add_custom_command(TARGET Wild POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${PIX_DLL}"
        $<TARGET_FILE_DIR:Wild>
)

# The D3D12 loader expects the Agility SDK runtime in the folder named by the
# exported D3D12SDKPath symbol
add_custom_command(TARGET Wild POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Wild>/D3D12"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${AGILITY_SDK_DLL_DIR}/D3D12Core.dll"
        "${AGILITY_SDK_DLL_DIR}/d3d12SDKLayers.dll"
        "$<TARGET_FILE_DIR:Wild>/D3D12"
    COMMENT "Copying DirectX 12 Agility SDK runtime to output directory"
)

#${CMAKE_BINARY_DIR}