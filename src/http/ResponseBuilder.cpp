#include "http/ResponseBuilder.hpp"
#include <stdexcept>
#include <iostream>

static HttpResponse handleCGI(const HttpRequest &req, const RouteResult &route)
{
	(void)req;
	(void)route;
	throw std::runtime_error("not implemented yet");
}

static HttpResponse handleDeleteResource(const HttpRequest &req, const RouteResult &route)
{
	(void)req;
	(void)route;
	throw std::runtime_error("not implemented yet");
}

static HttpResponse handleDirectory(const HttpRequest &req, const RouteResult &route)
{
	(void)req;
	(void)route;
	throw std::runtime_error("not implemented yet");
}

static HttpResponse handleRedirect(const HttpRequest &req, const RouteResult &route)
{
	(void)req;
	(void)route;
	throw std::runtime_error("not implemented yet");
}

static HttpResponse handleStaticFile(const HttpRequest &req, const RouteResult &route)
{
	(void)req;
	(void)route;
	throw std::runtime_error("not implemented yet");
}

static HttpResponse handleUpload(const HttpRequest &req, const RouteResult &route)
{
	(void)req;
	(void)route;
	throw std::runtime_error("not implemented yet");
}

/*a switch on route.action with a stub per case, no default, so if Paola adds an
unknown RouteAction it will error, and one try/catch of std::exception answering
makeError(500).
*/
HttpResponse buildResponse(const HttpRequest &req, const RouteResult &route)
{
	try
	{
		switch (route.action)
		{
			case RouteAction::CGI:
				return (handleCGI(req, route));
			case RouteAction::DeleteResource:
				return (handleDeleteResource(req, route));
			case RouteAction::Directory:
				return (handleDirectory(req, route));
			case RouteAction::Redirect:
				return (handleRedirect(req, route));
			case RouteAction::StaticFile:
				return (handleStaticFile(req, route));
			case RouteAction::Upload:
				return (handleUpload(req, route));
			case RouteAction::Error:
				return (buildErrorResponse(route.StatusCode, route));
		}
	}
	catch(const std::exception& e)
	{
		std::cerr << "buildResponse: " << e.what() << '\n';
		return HttpResponse::makeError(500);
	}
	return HttpResponse::makeError(500);
}

/* TODO step 7: errorPageUri, match the location, resolvePath, read the file */
HttpResponse buildErrorResponse(int statusCode, const RouteResult &route)
{
	(void)route;
	return(HttpResponse::makeError(statusCode));
}

// //added for later response builder
// HttpResponse buildErrorResponse(int code, const RouteResult &route) {
//   std::string customBody;

//   if (loadConfiguredErrorPage(route, code, customBody))
//     return HttpResponse::make(code, customBody);
//   return HttpResponse::makeError(code);
// }
