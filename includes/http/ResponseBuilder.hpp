#ifndef RESPONSEBUILDER_HPP
#define RESPONSEBUILDER_HPP

#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include "RouteResult.hpp"

HttpResponse buildResponse(const HttpRequest &req, const RouteResult &route);
HttpResponse buildErrorResponse(int statusCode, const RouteResult &route);

#endif
