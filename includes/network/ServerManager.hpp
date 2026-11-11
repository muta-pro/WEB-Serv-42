#ifndef SERVER_MANAGER_HPP
#define SERVER_MANAGER_HPP

#include "config/ServerConfig.hpp"
#include "network/Socket.hpp"
#include <vector>

struct Listener {
	Listen address;
	Socket socket;
	std::vector<ServerConfig> candidates;
}

#endif