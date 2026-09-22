#ifndef ROUTER_HPPROUTER_HPP
#define ROUTER_HPP

#include "RouteResult.hpp"
#include "http/HttpRequest.hpp"
#include <vector>

RouteResult routeRequest(const HttpRequest &req, const std::vector<ServerConfig> &candidates);

#endif
