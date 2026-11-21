#include "network/ServerManager.hpp"
#include <map>
#include <set>
#include <stdexcept>
#include <utility>
#include <vector>

void ServerManager::openListeners(const std::vector<ServerConfig> &servers) {
  if (!_listeners.empty())
    throw std::logic_error("listeners already open");
  if (servers.empty())
    throw std::invalid_argument("no configured servers");
  std::map<Listen, std::vector<ServerConfig>> groups;
  for (const ServerConfig &server : servers) {
    std::set<Listen> seen;
    for (const Listen &address : server.listens) {
      if (seen.insert(address).second)
        groups[address].push_back(server);
    }
  }
  std::vector<Listener> pending;
  pending.reserve(groups.size());
  for (auto &group : groups) {
    pending.push_back(Listener{
      group.first,
      Socket::createListener(group.first),
      std::move(group.second)
    });
  }
  _listeners.swap(pending);
}

const std::vector<Listener> &ServerManager::listeners() const noexcept {
  return _listeners;
}
