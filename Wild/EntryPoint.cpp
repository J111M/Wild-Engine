#include "Tools/Log.hpp"

#include <filesystem>

// Run the agility sdk
extern "C" __declspec(dllexport) extern const unsigned int D3D12SDKVersion = D3D12_AGILITY_SDK_VERSION;
extern "C" __declspec(dllexport) extern const char* D3D12SDKPath = ".\\D3D12\\";

int main(int, char* argv[])
{
    Wild::Log::Initialize();

    auto exePath = std::filesystem::path(argv[0]).parent_path();
    Wild::engine.SetSystemPath(exePath);
    Wild::engine.Initialize();
    Wild::engine.Run();
    Wild::engine.Shutdown();

    return EXIT_SUCCESS;
}
