#include "Tools/Log.hpp"

int main(int argc, char *argv[])
{
    Wild::Log::Initialize();

    auto exePath = std::filesystem::path(argv[0]).parent_path();
    Wild::engine.SetSystemPath(exePath);
    Wild::engine.Initialize();
    Wild::engine.Run();
    Wild::engine.Shutdown();

    return EXIT_SUCCESS;
}
