#include "harness/Harness.hpp"

#include "Tester.hpp"
#include "http/ResponseBuilder.hpp"

#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <sstream>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>

#ifndef SANDBOX_ROOT
# error "SANDBOX_ROOT is not defined: build the tester with its Makefile"
#endif

namespace fs = std::filesystem;

static std::string	plural(size_t count, const std::string &word)
{
	return (std::to_string(count) + " " + word + (count == 1 ? "" : "s"));
}

static std::string	orNone(const std::string &text)
{
	return (text.empty() ? "(none)" : text);
}

static std::string	orEmpty(const std::string &text)
{
	return (text.empty() ? "(empty)" : text);
}

static bool	startsWith(const std::string &text, const std::string &prefix)
{
	return (text.compare(0, prefix.size(), prefix) == 0);
}

std::string	oneLine(const std::string &bytes)
{
	std::string	out;

	for (unsigned char c : bytes)
	{
		if (c == '\r')
			out += "\\r";
		else if (c == '\n')
			out += "\\n";
		else if (c == '\t')
			out += "\\t";
		else if (c < 0x20 || c == 0x7f)
		{
			char	hex[8];
			std::snprintf(hex, sizeof(hex), "\\x%02x", c);
			out += hex;
		}
		else
			out += static_cast<char>(c);
	}
	return (orEmpty(out));
}

/*
=============================================================================
	the request
=============================================================================
*/

HttpRequest	request(const std::string &line)
{
	std::istringstream			in(line);
	std::vector<std::string>	words;
	std::string					word;

	while (in >> word)
		words.push_back(word);
	if (words.size() < 2 || words.size() > 3)
		throw std::invalid_argument("request(): expected \"METHOD TARGET [VERSION]\", not \"" + line + "\"");

	HttpRequest	req;
	size_t		question = words[1].find('?');

	req.method = words[0];
	req.target = words[1];
	req.path = words[1].substr(0, question);
	if (question != std::string::npos)
		req.query = words[1].substr(question + 1);
	req.version = (words.size() == 3) ? words[2] : "HTTP/1.1";
	req.headers["Host"] = "localhost";
	return (req);
}

/*
=============================================================================
	the config and the route
=============================================================================
*/

Site::Site(const std::string &config)
	: _servers(ConfigParser::parseString(config))
{
}

const std::vector<ServerConfig>	&Site::servers() const
{
	return (_servers);
}

const ServerConfig	&Site::server(size_t index) const
{
	if (index >= _servers.size())
		throw std::invalid_argument("Site: there is no server " + std::to_string(index)
			+ ", the config has " + plural(_servers.size(), "server"));
	return (_servers[index]);
}

const LocationConfig	&Site::location(const std::string &path, size_t server) const
{
	const ServerConfig	&chosen = this->server(server);
	std::string			paths;

	for (const LocationConfig &loc : chosen.locations)
		if (loc.path == path)
			return (loc);
	for (const LocationConfig &loc : chosen.locations)
		paths += " " + loc.path;
	throw std::invalid_argument("Site: no location '" + path + "' in server " + std::to_string(server)
		+ ". name the block exactly as the config writes it:" + paths);
}

RouteResult	Site::route(RouteAction action, const HttpRequest &request, const std::string &locationPath,
				size_t server) const
{
	if (action == RouteAction::Error)
		throw std::invalid_argument("Site::route: an Error route needs its code, use Site::error");

	RouteResult	route;

	route.server = &this->server(server);
	route.loc = &location(locationPath, server);
	route.action = action;
	route.StatusCode = 200;
	if (action == RouteAction::Redirect)
	{
		if (!route.loc->isRedirect())
			throw std::invalid_argument("Site::route: location '" + locationPath
				+ "' has no 'return', so the router would never send a Redirect from it");
		route.StatusCode = route.loc->redirectCode;
		route.redirectLocation = route.loc->redirect;
	}
	else
		route.filesystemPath = resolvePath(*route.loc, request.path);
	return (route);
}

RouteResult	Site::error(int code, const std::string &locationPath, size_t server) const
{
	RouteResult	route;

	route.server = &this->server(server);
	route.loc = locationPath.empty() ? nullptr : &location(locationPath, server);
	route.action = RouteAction::Error;
	route.StatusCode = code;
	return (route);
}

RouteResult	errorBeforeRouting(int code)
{
	RouteResult	route;

	route.action = RouteAction::Error;
	route.StatusCode = code;
	return (route);
}

std::string	defaultConfig()
{
	return (Tester::readFile(Tester::repoPath("conf/default.conf")));
}

std::string	actionName(RouteAction action)
{
	switch (action)
	{
		case RouteAction::StaticFile:		return ("StaticFile");
		case RouteAction::Directory:		return ("Directory");
		case RouteAction::Redirect:			return ("Redirect");
		case RouteAction::Upload:			return ("Upload");
		case RouteAction::DeleteResource:	return ("DeleteResource");
		case RouteAction::CGI:				return ("CGI");
		case RouteAction::Error:			return ("Error");
	}
	return ("RouteAction(" + std::to_string(static_cast<int>(action)) + ")");
}

std::string	routeFacts(const RouteResult &route, const Site *site)
{
	std::string	server = "null";

	if (route.server)
	{
		server = "a server outside this Site";
		if (site)
			for (size_t i = 0; i < site->servers().size(); i++)
				if (route.server == &site->servers()[i])
					server = "server " + std::to_string(i);
	}
	return ("action = " + actionName(route.action)
		+ "\nStatusCode = " + std::to_string(route.StatusCode)
		+ "\nserver = " + server
		+ "\nloc = " + (route.loc ? "location " + route.loc->path : std::string("null"))
		+ "\nfilesystemPath = " + orEmpty(route.filesystemPath)
		+ "\nredirectLocation = " + orEmpty(route.redirectLocation));
}

/*
=============================================================================
	the disk
=============================================================================
*/

/* every folder under path gets its owner's rights back, so remove_all can get in */
static void	unlock(const fs::path &path)
{
	std::error_code	ec;

	if (!fs::is_directory(fs::symlink_status(path, ec)))
		return;
	fs::permissions(path, fs::perms::owner_all, fs::perm_options::add, ec);
	for (const fs::directory_entry &entry : fs::directory_iterator(path, ec))
		unlock(entry.path());
}

static void	removeTree(const fs::path &path)
{
	std::error_code	ec;

	unlock(path);
	fs::remove_all(path, ec);
}

Sandbox::Sandbox()
{
	fs::create_directories(SANDBOX_ROOT);

	std::string			pattern = std::string(SANDBOX_ROOT) + "/test-XXXXXX";
	std::vector<char>	name(pattern.begin(), pattern.end());

	name.push_back('\0');
	if (mkdtemp(name.data()) == nullptr)
		throw std::runtime_error("Sandbox: cannot make a folder in " + std::string(SANDBOX_ROOT)
			+ ": " + std::strerror(errno));
	_root = name.data();
	_previous = fs::current_path().string();
	fs::current_path(_root);
}

Sandbox::~Sandbox()
{
	std::error_code	ec;

	fs::current_path(_previous, ec);
	removeTree(_root);
}

void	Sandbox::clearAll()
{
	removeTree(SANDBOX_ROOT);
}

bool	Sandbox::asRoot()
{
	return (geteuid() == 0);
}

const std::string	&Sandbox::root() const
{
	return (_root);
}

/* path inside the sandbox, absolute. refuses a way out */
std::string	Sandbox::inside(const std::string &path) const
{
	if (path.empty() || path[0] == '/')
		throw std::invalid_argument("Sandbox: '" + path + "': paths are relative to the sandbox");
	for (const fs::path &part : fs::path(path))
		if (part == "..")
			throw std::invalid_argument("Sandbox: '" + path + "': no '..', a test stays in its own folder");
	return (_root + "/" + path);
}

static void	makeParents(const std::string &full)
{
	fs::path	parent = fs::path(full).parent_path();

	if (!parent.empty())
		fs::create_directories(parent);
}

static void	setMode(const std::string &full, const std::string &path, mode_t mode)
{
	if (chmod(full.c_str(), mode) != 0)
		throw std::runtime_error("Sandbox: cannot chmod '" + path + "': " + std::strerror(errno));
}

void	Sandbox::file(const std::string &path, const std::string &content, mode_t mode) const
{
	const std::string	full = inside(path);

	makeParents(full);
	chmod(full.c_str(), 0600);	// it may exist already, unreadable
	int	fd = open(full.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
	if (fd < 0)
		throw std::runtime_error("Sandbox: cannot write '" + path + "': " + std::strerror(errno));

	size_t	done = 0;
	while (done < content.size())
	{
		ssize_t	written = write(fd, content.data() + done, content.size() - done);

		if (written <= 0)
		{
			close(fd);
			throw std::runtime_error("Sandbox: cannot write '" + path + "': " + std::strerror(errno));
		}
		done += static_cast<size_t>(written);
	}
	close(fd);
	setMode(full, path, mode);
}

void	Sandbox::dir(const std::string &path, mode_t mode) const
{
	const std::string	full = inside(path);

	fs::create_directories(full);
	setMode(full, path, mode);
}

void	Sandbox::fifo(const std::string &path) const
{
	const std::string	full = inside(path);

	makeParents(full);
	if (mkfifo(full.c_str(), 0644) != 0)
		throw std::runtime_error("Sandbox: cannot make the FIFO '" + path + "': " + std::strerror(errno));
}

std::string	Sandbox::kind(const std::string &path) const
{
	struct stat	info;

	if (lstat(inside(path).c_str(), &info) != 0)
		return (errno == ENOENT || errno == ENOTDIR ? "missing" : "can't stat: " + std::string(std::strerror(errno)));
	if (S_ISREG(info.st_mode))
		return ("file, " + plural(static_cast<size_t>(info.st_size), "byte"));
	if (S_ISDIR(info.st_mode))
		return ("folder");
	if (S_ISFIFO(info.st_mode))
		return ("fifo");
	return ("something else");
}

std::string	Sandbox::read(const std::string &path) const
{
	return (Tester::readFile(inside(path)));
}

/*
=============================================================================
	a scene
=============================================================================
*/

Scene::Scene(const std::string &requestLine)
	: _line(requestLine), _hasBody(false), _routeKind(NoRoute), _action(RouteAction::Error),
	  _server(0), _code(0), _ownPath(false)
{
}

Scene	&Scene::header(const std::string &name, const std::string &value)
{
	_headers.push_back({name, value});
	return (*this);
}

Scene	&Scene::body(const std::string &bytes)
{
	_body = bytes;
	_hasBody = true;
	return (*this);
}

Scene	&Scene::config(const std::string &text)
{
	_config = text;
	return (*this);
}

Scene	&Scene::route(RouteAction action, const std::string &location, size_t server)
{
	_routeKind = Routed;
	_action = action;
	_location = location;
	_server = server;
	return (*this);
}

Scene	&Scene::error(int code, const std::string &location, size_t server)
{
	_routeKind = Errored;
	_code = code;
	_location = location;
	_server = server;
	return (*this);
}

Scene	&Scene::errorBeforeRouting(int code)
{
	_routeKind = BeforeRouting;
	_code = code;
	return (*this);
}

Scene	&Scene::filesystemPath(const std::string &path)
{
	_ownPath = true;
	_path = path;
	return (*this);
}

Scene	&Scene::file(const std::string &path, const std::string &content, mode_t mode)
{
	_disk.push_back(DiskItem{DiskItem::File, path, content, mode});
	return (*this);
}

Scene	&Scene::dir(const std::string &path, mode_t mode)
{
	_disk.push_back(DiskItem{DiskItem::Dir, path, "", mode});
	return (*this);
}

Scene	&Scene::fifo(const std::string &path)
{
	_disk.push_back(DiskItem{DiskItem::Fifo, path, "", 0});
	return (*this);
}

Scene	&Scene::www()
{
	file("www/html/index.html", "<h1>webserv</h1>\n");
	file("www/errors/404.html", "<h1>404 page</h1>\n");
	file("www/errors/403.html", "<h1>403 page</h1>\n");
	file("www/errors/50x.html", "<h1>50x page</h1>\n");
	dir("www/files");
	dir("www/uploads");
	dir("www/cgi-bin");
	return (*this);
}

HttpRequest	Scene::makeRequest() const
{
	HttpRequest	req = request(_line);

	for (const std::pair<std::string, std::string> &header : _headers)
		req.headers[header.first] = header.second;
	if (_hasBody)
		req.body = _body;
	return (req);
}

RouteResult	Scene::makeRoute(const Site &site, const HttpRequest &request) const
{
	RouteResult	route;

	if (_routeKind == NoRoute)
		throw std::invalid_argument("Scene: no route. say what the router decided: route(), error() or errorBeforeRouting()");
	if (_routeKind == Routed)
		route = site.route(_action, request, _location, _server);
	else if (_routeKind == Errored)
		route = site.error(_code, _location, _server);
	else
		route = ::errorBeforeRouting(_code);
	if (_ownPath)
		route.filesystemPath = _path;
	return (route);
}

static std::string	modeText(mode_t mode)
{
	char	text[8];

	std::snprintf(text, sizeof(text), "%03o", static_cast<unsigned>(mode & 07777));
	return (text);
}

/* the route as the input shows it. parses the config to say where the pointers go */
std::string	Scene::routeText() const
{
	if (_routeKind == NoRoute)
		return ("(none yet: route(), error() or errorBeforeRouting())");
	if (_routeKind == BeforeRouting)
		return ("Error " + std::to_string(_code) + ", before routing: server and loc null");

	std::string	server = "server " + std::to_string(_server);
	if (_routeKind == Errored)
		return ("Error " + std::to_string(_code) + ", "
			+ (_location.empty() ? server + ", no location" : "location " + _location + " of " + server));

	std::string	text = actionName(_action) + ", location " + _location + " of " + server;
	try
	{
		Site		site(_config.empty() ? defaultConfig() : _config);
		RouteResult	route = makeRoute(site, makeRequest());

		if (_action == RouteAction::Redirect)
			text += "\n" + std::to_string(route.StatusCode) + " to " + route.redirectLocation;
		else
			text += "\nfilesystemPath " + route.filesystemPath + (_ownPath ? " (set by the test)" : "");
	}
	catch (const std::exception &e)
	{
		text += "\n(can't be made: " + std::string(e.what()) + ")";
	}
	return (text);
}

/* each line of block under its label: "route     StaticFile..." */
static std::string	labelled(const std::string &label, const std::string &block)
{
	std::istringstream	in(block);
	std::string			line;
	std::string			out;
	bool				first = true;

	while (std::getline(in, line))
	{
		out += (first ? label + std::string(9 - label.size(), ' ') : std::string(9, ' ')) + line + "\n";
		first = false;
	}
	return (out);
}

std::string	Scene::describe() const
{
	std::string	req = _line + (_line.find(" HTTP/") == std::string::npos ? " HTTP/1.1" : "");
	std::string	disk;

	for (const std::pair<std::string, std::string> &header : _headers)
		req += "\n" + header.first + ": " + header.second;
	if (_hasBody)
		req += "\n(body: " + plural(_body.size(), "byte") + ")";
	for (const DiskItem &item : _disk)
	{
		if (item.kind == DiskItem::File)
			disk += item.path + "   file, " + plural(item.content.size(), "byte")
				+ (item.mode != 0644 ? ", mode " + modeText(item.mode) : "") + "\n";
		else if (item.kind == DiskItem::Dir)
			disk += item.path + "/   folder" + (item.mode != 0755 ? ", mode " + modeText(item.mode) : "") + "\n";
		else
			disk += item.path + "   fifo\n";
	}
	return (labelled("request", req) + labelled("route", routeText())
		+ labelled("disk", disk.empty() ? "(nothing)" : disk)
		+ labelled("config", _config.empty() ? "conf/default.conf" : "its own"));
}

std::string	Scene::check(const std::vector<std::string> &facts) const
{
	Sandbox	box;

	for (const DiskItem &item : _disk)
	{
		if (item.kind == DiskItem::File)
			box.file(item.path, item.content, item.mode);
		else if (item.kind == DiskItem::Dir)
			box.dir(item.path, item.mode);
		else
			box.fifo(item.path);
	}

	Site			site(_config.empty() ? defaultConfig() : _config);
	HttpRequest		req = makeRequest();
	RouteResult		route = makeRoute(site, req);
	HttpResponse	response = buildResponse(req, route);

	if (facts.empty())
		return ("buildResponse returns");

	std::string	got;
	for (const std::string &fact : facts)
	{
		size_t		equals = fact.find(" = ");
		std::string	line;

		if (equals == std::string::npos)
			line = fact + "   <- not a fact: write it as 'key = value'";
		else
		{
			std::string	key = fact.substr(0, equals);
			line = key + " = " + responseFact(key, response, req, &box);
		}
		got += (got.empty() ? "" : "\n") + line;
	}
	return (got);
}

/*
=============================================================================
	the check
=============================================================================
*/

void	responds(Tester &t, const std::string &name, const Scene &scene, const std::vector<std::string> &facts)
{
	std::string	expected;

	for (const std::string &fact : facts)
		expected += (expected.empty() ? "" : "\n") + fact;
	if (facts.empty())
		expected = "buildResponse returns";
	t.test(name, scene.describe(), gives(expected), [&scene, &facts] {
		return (scene.check(facts));
	});
}

/* the header block of toBytes' output: "Name" -> value, (none) if it isn't there */
static std::string	wireHeader(const std::string &wire, const std::string &name)
{
	size_t	end = wire.find("\r\n\r\n");
	size_t	start = wire.find("\r\n");

	while (start != std::string::npos && start < end)
	{
		size_t		next = wire.find("\r\n", start + 2);
		std::string	line = wire.substr(start + 2, next - start - 2);
		size_t		colon = line.find(':');

		if (colon != std::string::npos && colon == name.size())
		{
			bool	same = true;
			for (size_t i = 0; i < colon; i++)
				if (std::tolower(static_cast<unsigned char>(line[i])) != std::tolower(static_cast<unsigned char>(name[i])))
					same = false;
			if (same)
				return (line.substr(colon + (line.size() > colon + 1 && line[colon + 1] == ' ' ? 2 : 1)));
		}
		start = next;
	}
	return ("(none)");
}

std::string	responseFact(const std::string &key, const HttpResponse &response, const HttpRequest &request,
				const Sandbox *box)
{
	if (key == "status")
		return (std::to_string(response.statusCode));
	if (key == "reason")
		return (orNone(HttpResponse::reasonFor(response.statusCode)));
	if (key == "headers")
	{
		std::string	names;
		for (const std::pair<const std::string, std::string> &header : response.headers)
			names += (names.empty() ? "" : " ") + header.first;
		return (orNone(names));
	}
	if (startsWith(key, "header "))
	{
		std::string	name = key.substr(7);
		return (response.hasHeader(name) ? oneLine(response.getHeader(name)) : "(none)");
	}
	if (key == "body")
		return (oneLine(response.body));
	if (key == "body size")
		return (std::to_string(response.body.size()));
	if (startsWith(key, "body same as "))
	{
		std::string	path = key.substr(13);
		if (!box)
			return ("(no sandbox)");
		try
		{
			std::string	content = box->read(path);
			if (content == response.body)
				return ("yes");
			return ("no: the body has " + plural(response.body.size(), "byte") + ", the file "
				+ plural(content.size(), "byte"));
		}
		catch (const std::exception &)
		{
			return ("no: " + path + " can't be read (" + box->kind(path) + ")");
		}
	}
	if (key == "wire" || startsWith(key, "wire "))
	{
		std::string	wire = toBytes(response, request.method == "HEAD");
		size_t		blank = wire.find("\r\n\r\n");

		if (key == "wire")
			return (oneLine(wire.substr(0, wire.find("\r\n"))));
		if (key == "wire body size")
			return (blank == std::string::npos ? "(no empty line)" : std::to_string(wire.size() - blank - 4));
		if (startsWith(key, "wire header "))
			return (oneLine(wireHeader(wire, key.substr(12))));
	}
	if (startsWith(key, "disk "))
		return (box ? box->kind(key.substr(5)) : "(no sandbox)");
	return ("(no such fact: the keys are listed above responds() in harness/Harness.hpp)");
}
