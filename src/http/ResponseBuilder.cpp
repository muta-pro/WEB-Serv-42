#include "http/ResponseBuilder.hpp"
#include "config/LocationConfig.hpp"
#include <stdexcept>
#include <iostream>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <map>
#include <dirent.h>
#include <algorithm>
#include <vector>
#include <cctype>

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

/* "<b>&" -> "&lt;b&gt;&amp;": a name as text on the page */
static std::string htmlEscape(const std::string &text)
{
	std::string out;
	char c;
	for (size_t i = 0; i < text.length(); i++)
	{
		c = text[i];
		if (c == '&')
			out += "&amp;";
		else if (c == '<')
			out += "&lt;";
		else if (c == '>')
			out += "&gt;";
		else if (c == '"')
			out += "&quot;";
		else
			out += c;
	}
	return out;
}

/* "a b.txt" -> "a%20b.txt": a name as a URL in a link */
static std::string percentEncode(const std::string &name)
{
	static const char HEX[] = "0123456789ABCDEF";
	std::string out;
	for (unsigned char c : name)
	{
		if (std::isalnum(c) || c == '-' || c == '.' || c == '_' || c == '~')
			out += c;
		else
		{
			out += '%';
			out += HEX[c >> 4];
			out += HEX[c & 15];
		}
	}
	return out;
}

/* the folder dir (ends in "/") as an HTML page in `page`; false if it can't be read */
static bool listDirectory(const std::string &dir, std::string &page)
{
	DIR *d = opendir(dir.c_str());
	if (!d)
		return false;
	std::vector<std::string> folders, files;
	struct dirent *entry;
	while ((entry = readdir(d)) != NULL)
	{
		std::string name = entry->d_name;
		if (name[0] == '.')
			continue;
		struct stat info;
		if (stat((dir + name).c_str(), &info) == 0 && S_ISDIR(info.st_mode))
			folders.push_back(name);
		else
			files.push_back(name);
	}
	closedir(d);
	std::sort(folders.begin(), folders.end());
	std::sort(files.begin(), files.end());
	page += "<html><head><title>Index</title></head><body><h1>Index</h1>";
	page += "<a href=\"../\">../</a><br>";
	for (const std::string &name : folders)
		page += "<a href=\"" + percentEncode(name) + "/\">" + htmlEscape(name) + "/</a><br>";
	for (const std::string &name : files)
		page += "<a href=\"" + percentEncode(name) + "\">"  + htmlEscape(name) + "</a><br>";
	page += "</body></html>";
	return true;
}

static HttpResponse handleDirectory(const HttpRequest &req, const RouteResult &route)
{
	if (req.path.empty() || req.path.back() != '/')
	{
		std::string location = req.path + "/" + (req.query.empty() ? "" : "?" + req.query);
		HttpResponse r = HttpResponse::make(301, "<html><body><h1>301 Moved Permanently</h1></body></html>");
		r.setHeader("Location", location);
		r.setHeader("Content-Type", "text/html");
		return r;
	}
	struct stat info;
	if (stat(route.filesystemPath.c_str(), &info) != 0)
		return (buildErrorResponse(404, route));
	std::string dir = route.filesystemPath;
	if (dir.back() != '/')
		dir += '/';
	std::string indexPath = dir + route.loc->index;
	if (stat(indexPath.c_str(), &info) == 0)
	{
		RouteResult asFile = route;
		asFile.filesystemPath = indexPath;
		return handleStaticFile(req, asFile);
	}
	if (!route.loc->autoindex)
		return buildErrorResponse(403, route);
	if (access(dir.c_str(), R_OK) != 0)
		return buildErrorResponse(403, route);
	std::string page;
	if (!listDirectory(dir, page))
		return buildErrorResponse(500, route);
	HttpResponse r = HttpResponse::make(200, page);
	r.setHeader("Content-Type", "text/html");
	return r;
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
