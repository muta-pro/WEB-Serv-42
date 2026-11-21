#ifndef SERVER_MANAGER_HPP
#define SERVER_MANAGER_HPP

#include "config/ServerConfig.hpp"
#include "network/Socket.hpp"
#include <vector>

struct Listener {
	Listen address;
	Socket socket;
	std::vector<ServerConfig> candidates;
};

class ServerManager
{
public:
	ServerManager() = delete;
	ServerManager(const ServerManager &) = delete;
	ServerManager &operator=(const ServerManager &) = delete;

	void openListeners(const std::vector<ServerConfig> &servers);
	const std::vector<Listener> &listeners() const noexcept;
private:
	std::vector<Listener> _listeners;
};

#endif
