#include "http/ResponseBuilder.hpp"

/*
STEP 1 scaffold: the least that lets tests/ResponseBuilder link. both
functions answer a default HttpResponse - 200 OK, no headers, no body -
whatever they are asked.

	that is the "empty stub" of handover §7.1, and a trap worth seeing: a
	default HttpResponse says success, a default RouteResult says failure
	(Error, 500). step 1's check only asks that buildResponse can be called.
	step 2's first test - a default RouteResult must get 500 - fails
	against this stub, and passes once the switch below exists.

TODO step 2: a switch on route.action with a stub per case, a default that
answers 500, and one try/catch of std::exception answering makeError(500).
*/
HttpResponse buildResponse(const HttpRequest &req, const RouteResult &route)
{
	(void)req;
	(void)route;
	return (HttpResponse());
}

/* TODO step 7: errorPageUri, match the location, resolvePath, read the file */
HttpResponse buildErrorResponse(int statusCode, const RouteResult &route)
{
	(void)statusCode;
	(void)route;
	return (HttpResponse());
}

// //added for later response builder
// HttpResponse buildErrorResponse(int code, const RouteResult &route) {
//   std::string customBody;

//   if (loadConfiguredErrorPage(route, code, customBody))
//     return HttpResponse::make(code, customBody);
//   return HttpResponse::makeError(code);
// }
