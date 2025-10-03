#include "Tools/Log.hpp"

int main(int argc, char** argv)
{
	Wild::Log::initialize();

	Wild::engine.initialize();
	Wild::engine.run();
	Wild::engine.shutdown();
	return 0;
}
