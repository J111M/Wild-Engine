#include "Tools/Log.hpp"

int main(int argc, char** argv)
{
	Wild::Log::Initialize();

	Wild::engine.Initialize();
	Wild::engine.Run();
	Wild::engine.Shutdown();
	return 0;
}
