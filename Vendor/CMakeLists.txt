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

add_library(imgui STATIC ${IMGUI_SRC})
target_include_directories(imgui PUBLIC 
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

target_link_libraries(ImGuizmo PRIVATE imgui)

# Link ImGuizmo with your executable
target_link_libraries(Wild PRIVATE ImGuizmo imgui)

target_link_libraries(Wild PRIVATE ImGuizmo imgui d3d12 dxgi dxguid d3dcompiler)

set(CMAKE_SYSTEM_VERSION 10.0.26100.0 CACHE STRING "" FORCE)

target_include_directories(Wild PRIVATE
    "${CMAKE_SOURCE_DIR}/vendor/d3dx12"
)