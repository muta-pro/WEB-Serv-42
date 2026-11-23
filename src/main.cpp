#include <exception>
#include <iostream>
#include <string>
#include "network/ServerManager.hpp"
#include "config/ConfigParser.hpp"

int main(int argc, char **argv)
{
	if (argc > 2)
	{
		std::cerr << "Usage: " << argv[0] << " [config file]\n";
		return 1;
	}
	const std::string path = argc = 2 ? argv[1] : "conf/default.conf";
	try {
		const auto servers = ConfigParser::parseFile(path);
		ServerManager manager;
		manager.openListeners(servers);
		for (const Listener &listener : manager.listeners()) {
			std::cout << "Opened " << listener.address.host << ':'
			 << listener.address.port << " for " << listener.candidates.size() << " server blok(s)\n";
		}
		std::cout << "Startup check passed.Listeners closed. Loop not implemented yet\n" << std::endl;
	}
	catch (const std::exception &error) {
		std::cerr << "wevserv: " << error.what() << '\n';
		return 1;
	}
	return 0;
}
