// Term-Winter 2021

#include "puppet.hpp"

#include <iostream>
using namespace std;

int main( int argc, char **argv ) 
{
	if (argc > 1) {
		std::string luaSceneFile(argv[1]);
		std::string title("3D Puppet - [");
		title += luaSceneFile;
		title += "]";

		CS488Window::launch(argc, argv, new Puppet(luaSceneFile), 1024, 768, title);

	} else {
		cout << "Must supply Lua file as First argument to program.\n";
        cout << "For example:\n";
        cout << "./A3 Assets/simpleScene.lua\n";
	}

	return 0;
}
