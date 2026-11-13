#include "http/ResponseBuilder.hpp"
#include <stdexcept>
#include <iostream>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <map>

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

static bool readFile(const std::string &path, std::string &body)
{
	int	fd = open (path.c_str(), O_RDONLY);
	if (fd == -1)
		return false;
	char	buf[4096];
	ssize_t n = read(fd, buf, sizeof(buf));
	while (n > 0)
	{
		body.append(buf, n);
		n = read(fd, buf, sizeof(buf));
	}
	close(fd);
	if (n == -1)
		return false;
	return true;
}

/* "./www/html/f.png" -> "image/png" */
static std::string mimeType(const std::string &path)
{
	static const std::map<std::string, std::string> lookupTable {
		{"html", "text/html"},
		{"css", "text/css"},
		{"txt", "text/plain"},
		{"png", "image/png"},
		{"jpg", "image/jpeg"},
		{"jpeg", "image/jpeg"},
		{"gif", "image/gif"},
		{"svg", "image/svg+xml"},
		{"pdf", "application/pdf"},
		{"json", "application/json"},
	};
	size_t slash = path.rfind('/');
	size_t dot = path.rfind('.');
	if (dot == std::string::npos || dot < slash)
		return "application/octet-stream";
	std::string ext = path.substr(dot + 1);
	auto it = lookupTable.find(ext);
	if (it != lookupTable.end())
		return (it->second);
	else
		return ("application/octet-stream");
}

static HttpResponse handleStaticFile(const HttpRequest &req, const RouteResult &route)
{
	(void)req;
	struct stat info;
	if (stat(route.filesystemPath.c_str(), &info) != 0)
		return (buildErrorResponse(404, route));
	if (!S_ISREG(info.st_mode))
		return (buildErrorResponse(403, route));
	if (access(route.filesystemPath.c_str(), R_OK) != 0)
		return (buildErrorResponse(403, route));
	static const off_t LIMIT = (64*1024*1024);
	if (info.st_size > LIMIT)
		return buildErrorResponse(500, route);
	std::string body;
	if(!readFile(route.filesystemPath, body))
		return buildErrorResponse(500, route);
	HttpResponse response = HttpResponse::make(200, body);
	response.setHeader("Content-Type", mimeType(route.filesystemPath));
	return response;
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
