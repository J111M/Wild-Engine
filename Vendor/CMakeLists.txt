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

target_link_libraries(Wild PRIVATE d3d12 dxgi dxguid d3dcompiler)

set(CMAKE_SYSTEM_VERSION 10.0.26100.0 CACHE STRING "" FORCE)

target_include_directories(Wild PRIVATE
    "${CMAKE_SOURCE_DIR}/vendor/d3dx12"
)