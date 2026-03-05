cmake_minimum_required(VERSION 3.16)
project(Wild)

# C++ standard version
set(CMAKE_CXX_STANDARD 17 CACHE STRING "" FORCE)

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

# Shader directories
set(SHADER_SRC_DIR "${CMAKE_SOURCE_DIR}/Shaders")
#set(SHADER_OUT_DIR "${CMAKE_SOURCE_DIR}/Shaders/spirv")

target_compile_definitions(Wild PRIVATE
    SHADER_SOURCE_DIR="${CMAKE_SOURCE_DIR}/Shaders"
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

#${CMAKE_BINARY_DIR}