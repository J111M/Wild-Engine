add_subdirectory("glfw")
target_link_libraries(Wild PRIVATE glfw)
target_include_directories(Wild PRIVATE "./glfw/")

# FastGLTF cmake copied from https://github.com/Gikster007/niji/blob/main/lib/CMakeLists.txt
# https://github.com/spnda/fastgltf
include(FetchContent)
FetchContent_Declare(
    fastgltf
    GIT_REPOSITORY https://github.com/spnda/fastgltf.git
    GIT_TAG v0.8.0
)
FetchContent_MakeAvailable(fastgltf)
target_link_libraries(Wild PRIVATE fastgltf)

# Pix debugging
FetchContent_Declare(
    WinPixEventRuntime
    URL "https://www.nuget.org/api/v2/package/WinPixEventRuntime/1.0.240308001"
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(WinPixEventRuntime)
if(CMAKE_GENERATOR_PLATFORM STREQUAL "ARM64")
    set(PIX_ARCH "ARM64")
else()
    set(PIX_ARCH "x64")
endif()
target_include_directories(Wild PRIVATE
    "${winpixeventruntime_SOURCE_DIR}/Include/WinPixEventRuntime"
)
target_link_libraries(Wild PRIVATE
    "${winpixeventruntime_SOURCE_DIR}/bin/${PIX_ARCH}/WinPixEventRuntime.lib"
)
set(PIX_DLL "${winpixeventruntime_SOURCE_DIR}/bin/${PIX_ARCH}/WinPixEventRuntime.dll" PARENT_SCOPE)

# GLM
FetchContent_Declare(
  glm
  GIT_REPOSITORY https://github.com/g-truc/glm.git
  GIT_TAG        1.0.1
)

FetchContent_MakeAvailable(glm)

target_link_libraries(Wild PRIVATE glm::glm)

# EnTT ECS
add_subdirectory("entt")
target_link_libraries(Wild PUBLIC EnTT)

# stb
target_include_directories(Wild PRIVATE "./stb/")

# spdlog
FetchContent_Declare(spdlog GIT_REPOSITORY https://github.com/gabime/spdlog.git GIT_TAG v1.11.0)

FetchContent_MakeAvailable(spdlog)

target_link_libraries(Wild PRIVATE spdlog::spdlog)

# Slang
FetchContent_Declare(
    slang
    URL https://github.com/shader-slang/slang/releases/download/v2025.24.2/slang-2025.24.2-windows-x86_64.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_MakeAvailable(slang)

set(SLANG_ROOT "${slang_SOURCE_DIR}")
set(SLANG_INCLUDE_DIR "${SLANG_ROOT}/include")
set(SLANG_LIB_DIR     "${SLANG_ROOT}/lib")
set(SLANG_BIN_DIR     "${SLANG_ROOT}/bin")

add_library(slang SHARED IMPORTED)

set_target_properties(slang PROPERTIES
    IMPORTED_IMPLIB "${SLANG_LIB_DIR}/slang.lib"
    IMPORTED_LOCATION "${SLANG_BIN_DIR}/slang.dll"
    INTERFACE_INCLUDE_DIRECTORIES "${SLANG_INCLUDE_DIR}"
)

file(GLOB SLANG_RUNTIME_DLLS
    "${SLANG_BIN_DIR}/*.dll"
)

foreach(cfg Debug Release RelWithDebInfo MinSizeRel)
    file(COPY ${SLANG_RUNTIME_DLLS}
         DESTINATION "${CMAKE_BINARY_DIR}/${cfg}")
endforeach()

# Add ImGui
set(IMGUI_SRC
    imgui/imgui.cpp
    imgui/imgui_demo.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp
    imgui/implot/implot.cpp
    imgui/implot/implot_demo.cpp
    imgui/implot/implot_items.cpp
    imgui/backends/imgui_impl_glfw.cpp
    imgui/backends/imgui_impl_dx12.cpp
)

add_library(imgui_engine STATIC ${IMGUI_SRC})
target_include_directories(imgui_engine PUBLIC 
    "./imgui" 
    "./imgui/backends"
    "./imgui/implot"
    "./glfw/include"
)

# Link ImGui to Wild
#target_link_libraries(Wild PRIVATE imgui)

# Set path to ImGuizmo
set(IMGUIZMO_DIR ./imgizmo/)

# Add ImGuizmo as a library
add_library(ImGuizmo STATIC
    ${IMGUIZMO_DIR}/ImGuizmo.cpp
    ${IMGUIZMO_DIR}/ImGuizmo.h
)

# Global definition for imgui math operators
add_compile_definitions(IMGUI_DEFINE_MATH_OPERATORS)

# Include directories for ImGuizmo
target_include_directories(ImGuizmo PUBLIC ${IMGUIZMO_DIR})

target_link_libraries(ImGuizmo PRIVATE imgui_engine)

# Link ImGuizmo with your executable
target_link_libraries(Wild PRIVATE ImGuizmo imgui_engine)

# DirectX12 shader compiler
find_library(DXC_LIBRARY dxcompiler 
    PATHS 
    "${CMAKE_SOURCE_DIR}/vendor/dxc/lib"
    NO_DEFAULT_PATH
)

find_library(DXIL_LIBRARY dxil
    PATHS 
    "${CMAKE_SOURCE_DIR}/vendor/dxc/lib"
    NO_DEFAULT_PATH
)

target_link_libraries(Wild PRIVATE ${DXC_LIBRARY} ${DXIL_LIBRARY})

target_link_libraries(Wild PRIVATE slang ImGuizmo imgui_engine d3d12 dxgi dxguid)

set(CMAKE_SYSTEM_VERSION 10.0.26100.0 CACHE STRING "" FORCE)

target_include_directories(Wild PRIVATE
    "${CMAKE_SOURCE_DIR}/vendor/d3dx12"
)