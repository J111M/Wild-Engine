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

# EnTT ECS
add_subdirectory("entt")
target_link_libraries(Wild PUBLIC EnTT)

# stb
target_include_directories(Wild PRIVATE "./stb/")

FetchContent_Declare(spdlog GIT_REPOSITORY https://github.com/gabime/spdlog.git GIT_TAG v1.11.0)

FetchContent_MakeAvailable(spdlog)

target_link_libraries(Wild PRIVATE spdlog::spdlog)

target_link_libraries(Wild PRIVATE d3d12 dxgi dxguid)