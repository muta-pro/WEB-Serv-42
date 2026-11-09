#pragma once

#include "Tester.hpp"
#include "../harness/Harness.hpp"
#include "http/ResponseBuilder.hpp"

#include <cstdio>
#include <filesystem>
#include <functional>
#include <stdexcept>
#include <sys/stat.h>
#include <type_traits>
#include <unistd.h>

/*
STEP 1 - the harness. every later test stands on these fakes, so they get
tested first: if request(), Site, Sandbox or a Scene lie, every test after
them lies too. the groups follow handover §7:
	the request          §7.2   what C's parser would hand over
	resolvePath          §7.3   where a route's filesystemPath comes from
	the site and routes  §7.3   pointers into a config that outlives them
	the sandbox          §7.4   a folder per test, made at test time
	facts, scenes               how a test reads a response
and last, the step's own check: a request and a route reach buildResponse.
*/

/* these hold at compile time, or the tester doesn't build */
static_assert(!std::is_copy_constructible<Site>::value,
	"a Site is never copied: the routes of the original would still point into the original");
static_assert(!std::is_copy_assignable<Site>::value,
	"a Site is never assigned over: the routes made from it would point at servers that are gone");
static_assert(!std::is_copy_constructible<Sandbox>::value,
	"a Sandbox is one folder: two copies would both remove it");

/* the request as facts, one per line */
inline std::string	requestFacts(const HttpRequest &req)
{
	return ("method = " + req.method + "\ntarget = " + req.target + "\npath = " + req.path
		+ "\nquery = " + (req.query.empty() ? "(empty)" : req.query) + "\nversion = " + req.version);
}

inline std::string	joinLines(const std::vector<std::string> &lines)
{
	std::string	out;

	for (const std::string &line : lines)
		out += (out.empty() ? "" : "\n") + line;
	return (out);
}

/* "640" */
inline std::string	modeOf(const std::string &path)
{
	struct stat	info;
	char		text[8];

	if (stat(path.c_str(), &info) != 0)
		return ("can't stat " + path);
	std::snprintf(text, sizeof(text), "%03o", static_cast<unsigned>(info.st_mode & 0777));
	return (text);
}

inline void	testHarness(Tester &t)
{
	/* ------------------------------------------------ the request (§7.2) */

	auto	parsesLine = [&t](const std::string &line, const std::string &expected) {
		t.test("request(\"" + line + "\")", line, gives(expected), [&line] {
			return (requestFacts(request(line)));
		});
	};
	auto	refusesLine = [&t](const std::string &line, const std::string &why) {
		t.test("request(\"" + line + "\") is a bug in the test: " + why, line, gives("std::invalid_argument"), [&line] {
			try
			{
				request(line);
			}
			catch (const std::invalid_argument &)
			{
				return (std::string("std::invalid_argument"));
			}
			return (std::string("no exception"));
		});
	};

	parsesLine("GET /index.html",
		"method = GET\ntarget = /index.html\npath = /index.html\nquery = (empty)\nversion = HTTP/1.1");
	parsesLine("GET /files/docs?sort=name",
		"method = GET\ntarget = /files/docs?sort=name\npath = /files/docs\nquery = sort=name\nversion = HTTP/1.1");
	parsesLine("GET /search?",
		"method = GET\ntarget = /search?\npath = /search\nquery = (empty)\nversion = HTTP/1.1");
	parsesLine("GET /a?x=1?y=2",
		"method = GET\ntarget = /a?x=1?y=2\npath = /a\nquery = x=1?y=2\nversion = HTTP/1.1");
	parsesLine("HEAD / HTTP/1.0",
		"method = HEAD\ntarget = /\npath = /\nquery = (empty)\nversion = HTTP/1.0");
	parsesLine("DELETE /upload/cat.png",
		"method = DELETE\ntarget = /upload/cat.png\npath = /upload/cat.png\nquery = (empty)\nversion = HTTP/1.1");
	t.test("request() sets Host, as every HTTP/1.1 request has one", "GET /", gives("Host: localhost"), [] {
		return ("Host: " + std::string(request("GET /").header("host")));
	});
	refusesLine("GET", "no target");
	refusesLine("", "an empty line");
	refusesLine("GET / HTTP/1.1 extra", "a fourth word");

	/* ------------------------------------------------ resolvePath (§7.3) */

	/*
	a route's filesystemPath is derived with resolvePath, the function C
	calls too, so its rule is pinned here: the subject's alias rule
	(LocationConfig.hpp, decision 2). nothing else in the repo tests it.
	*/
	auto	resolves = [&t](const std::string &locPath, const std::string &root, const std::string &url,
					const std::string &expected) {
		t.test("resolvePath: location " + locPath + ", root " + root + ", URL " + url, url, gives(expected), [&] {
			LocationConfig	loc;

			loc.path = locPath;
			loc.root = root;
			return (resolvePath(loc, url));
		});
	};

	resolves("/kapouet", "/tmp/www", "/kapouet/pouic/toto/pouet", "/tmp/www/pouic/toto/pouet");	// the subject's own
	resolves("/kapouet", "/tmp/www", "/kapouet", "/tmp/www");
	resolves("/", "./www/html", "/index.html", "./www/html/index.html");
	resolves("/", "./www/html", "/", "./www/html");
	resolves("/files/", "./www/files", "/files/", "./www/files");
	resolves("/files/", "./www/files", "/files/docs", "./www/files/docs");
	resolves("/files/", "./www/files", "/files/docs/", "./www/files/docs/");	// a Directory route sees the '/'
	resolves("/", "./www/html/", "/a.txt", "./www/html/a.txt");				// a '/' at the end of root is trimmed
	resolves("/", "/", "/a.txt", "/a.txt");									// root "/" doesn't make "//a.txt"
	resolves("/", "/", "/", "/");

	/* ------------------------------------------------ the site and its routes (§7.3) */

	t.test("conf/default.conf: one server and its six location blocks", "conf/default.conf",
		gives("1 server: / /errors/ /files/ /upload/ /cgi-bin/ /old/"), [] {
		Site		site(defaultConfig());
		std::string	paths;

		for (const LocationConfig &loc : site.server().locations)
			paths += " " + loc.path;
		return (std::to_string(site.servers().size()) + " server:" + paths);
	});
	t.test("location() finds a block by its exact path", "site.location(\"/files/\")",
		gives("root ./www/files, autoindex on"), [] {
		Site					site(defaultConfig());
		const LocationConfig	&loc = site.location("/files/");

		return ("root " + loc.root + ", autoindex " + (loc.autoindex ? "on" : "off"));
	});
	t.test("location() never matches a prefix: \"/files\" is no block (the router's job)", "site.location(\"/files\")",
		gives("Site: no location '/files' in server 0. name the block exactly as the config writes it:"
			" / /errors/ /files/ /upload/ /cgi-bin/ /old/"), [] {
		Site	site(defaultConfig());

		try
		{
			site.location("/files");
		}
		catch (const std::invalid_argument &e)
		{
			return (std::string(e.what()));
		}
		return (std::string("found one"));
	});
	t.test("a route's server and loc point into the Site's own servers", "site.route(StaticFile, GET /files/, \"/files/\")",
		gives("server: the Site's server 0 · loc: the Site's /files/"), [] {
		Site		site(defaultConfig());
		RouteResult	route = site.route(RouteAction::StaticFile, request("GET /files/"), "/files/");

		return (std::string("server: ") + (route.server == &site.server(0) ? "the Site's server 0" : "somewhere else")
			+ " · loc: " + (route.loc == &site.location("/files/") ? "the Site's /files/" : "somewhere else"));
	});

	auto	routes = [&t](const std::string &name, const std::string &input, const std::vector<std::string> &expected,
					const std::function<RouteResult(const Site &)> &make) {
		t.test(name, input, gives(joinLines(expected)), [&make] {
			Site	site(defaultConfig());

			return (routeFacts(make(site), &site));
		});
	};

	routes("a StaticFile route: 200, and the path resolvePath gives", "GET /index.html, location /",
		{"action = StaticFile", "StatusCode = 200", "server = server 0", "loc = location /",
		 "filesystemPath = ./www/html/index.html", "redirectLocation = (empty)"},
		[](const Site &site) { return (site.route(RouteAction::StaticFile, request("GET /index.html"), "/")); });
	routes("a Directory route keeps the request's trailing '/'", "GET /files/docs/, location /files/",
		{"action = Directory", "StatusCode = 200", "server = server 0", "loc = location /files/",
		 "filesystemPath = ./www/files/docs/", "redirectLocation = (empty)"},
		[](const Site &site) { return (site.route(RouteAction::Directory, request("GET /files/docs/"), "/files/")); });
	routes("a Redirect route: the block's return code and target, no path", "GET /old/, location /old/",
		{"action = Redirect", "StatusCode = 301", "server = server 0", "loc = location /old/",
		 "filesystemPath = (empty)", "redirectLocation = /"},
		[](const Site &site) { return (site.route(RouteAction::Redirect, request("GET /old/"), "/old/")); });
	routes("a DeleteResource route: the file to remove", "DELETE /upload/cat.png, location /upload/",
		{"action = DeleteResource", "StatusCode = 200", "server = server 0", "loc = location /upload/",
		 "filesystemPath = ./www/uploads/cat.png", "redirectLocation = (empty)"},
		[](const Site &site) { return (site.route(RouteAction::DeleteResource, request("DELETE /upload/cat.png"), "/upload/")); });
	routes("an Upload route: the folder, until its meaning is agreed with Paola", "POST /upload/, location /upload/",
		{"action = Upload", "StatusCode = 200", "server = server 0", "loc = location /upload/",
		 "filesystemPath = ./www/uploads", "redirectLocation = (empty)"},
		[](const Site &site) { return (site.route(RouteAction::Upload, request("POST /upload/"), "/upload/")); });
	routes("a CGI route: the script", "GET /cgi-bin/hello.py, location /cgi-bin/",
		{"action = CGI", "StatusCode = 200", "server = server 0", "loc = location /cgi-bin/",
		 "filesystemPath = ./www/cgi-bin/hello.py", "redirectLocation = (empty)"},
		[](const Site &site) { return (site.route(RouteAction::CGI, request("GET /cgi-bin/hello.py"), "/cgi-bin/")); });
	routes("an Error route: C's code and the server, no location", "site.error(404)",
		{"action = Error", "StatusCode = 404", "server = server 0", "loc = null",
		 "filesystemPath = (empty)", "redirectLocation = (empty)"},
		[](const Site &site) { return (site.error(404)); });
	routes("an Error route with the location the router had matched", "site.error(405, \"/\")",
		{"action = Error", "StatusCode = 405", "server = server 0", "loc = location /",
		 "filesystemPath = (empty)", "redirectLocation = (empty)"},
		[](const Site &site) { return (site.error(405, "/")); });

	t.test("errorBeforeRouting(400): no server, no location", "errorBeforeRouting(400)",
		gives("action = Error\nStatusCode = 400\nserver = null\nloc = null\nfilesystemPath = (empty)\nredirectLocation = (empty)"), [] {
		return (routeFacts(errorBeforeRouting(400), nullptr));
	});
	t.test("a default RouteResult fails safe: Error, 500, no pointers", "RouteResult route;",
		gives("action = Error\nStatusCode = 500\nserver = null\nloc = null\nfilesystemPath = (empty)\nredirectLocation = (empty)"), [] {
		return (routeFacts(RouteResult(), nullptr));
	});
	t.test("Site::route won't make an Error route without its code", "site.route(Error, GET /, \"/\")",
		gives("Site::route: an Error route needs its code, use Site::error"), [] {
		Site	site(defaultConfig());

		try
		{
			site.route(RouteAction::Error, request("GET /"), "/");
		}
		catch (const std::invalid_argument &e)
		{
			return (std::string(e.what()));
		}
		return (std::string("made one"));
	});
	t.test("Site::route won't make a Redirect from a block without return", "site.route(Redirect, GET /files/, \"/files/\")",
		gives("Site::route: location '/files/' has no 'return', so the router would never send a Redirect from it"), [] {
		Site	site(defaultConfig());

		try
		{
			site.route(RouteAction::Redirect, request("GET /files/"), "/files/");
		}
		catch (const std::invalid_argument &e)
		{
			return (std::string(e.what()));
		}
		return (std::string("made one"));
	});
	t.test("a config of the test's own: return 307", "server { listen 8080; location /old/ { return 307 /new/form; } }",
		gives("action = Redirect\nStatusCode = 307\nserver = server 0\nloc = location /old/\nfilesystemPath = (empty)\nredirectLocation = /new/form"), [] {
		Site	site("server { listen 8080; location /old/ { return 307 /new/form; } }");

		return (routeFacts(site.route(RouteAction::Redirect, request("POST /old/form"), "/old/"), &site));
	});
	t.test("a route into the second server", "two servers on 8080, a.test and b.test",
		gives("server 1"), [] {
		Site		site("server { listen 8080; server_name a.test; } server { listen 8080; server_name b.test; }");
		RouteResult	route = site.route(RouteAction::StaticFile, request("GET /"), "/", 1);

		return (std::string(route.server == &site.server(1) ? "server 1" : "another server"));
	});
	t.test("a config that doesn't parse is the test's own ConfigError", "server {", fails(1, "unexpected end of file"), [] {
		Site	site("server {");

		(void)site;
		return (std::string("parsed"));
	});

	/* ------------------------------------------------ the sandbox (§7.4) */

	t.test("a new sandbox is empty, and the working directory", "Sandbox box;",
		gives("empty, and the working directory"), [] {
		Sandbox	box;
		bool	empty = std::filesystem::is_empty(box.root());
		bool	here = std::filesystem::equivalent(std::filesystem::current_path(), box.root());

		return (std::string(empty ? "empty" : "not empty") + ", and " + (here ? "the working directory" : "somewhere else"));
	});
	t.test("file() makes the folders on its way", "box.file(\"www/html/index.html\", \"<h1>hi</h1>\")",
		gives("file, 11 bytes"), [] {
		Sandbox	box;

		box.file("www/html/index.html", "<h1>hi</h1>");
		return (box.kind("www/html/index.html"));
	});
	t.test("a test's paths are the server's: ./www/html/index.html", "box.file(\"www/html/index.html\", \"<h1>hi</h1>\")",
		gives("<h1>hi</h1>"), [] {
		Sandbox	box;

		box.file("www/html/index.html", "<h1>hi</h1>");
		return (Tester::readFile("./www/html/index.html"));
	});
	t.test("file() keeps every byte, zero bytes too", "box.file(\"www/html/logo.png\", \"\\x89PNG\\0\\x01\")",
		gives("6 bytes, the same"), [] {
		const std::string	png("\x89PNG\0\x01", 6);
		Sandbox				box;

		box.file("www/html/logo.png", png);
		std::string	back = box.read("www/html/logo.png");
		return (std::to_string(back.size()) + " bytes, " + (back == png ? "the same" : "different"));
	});
	t.test("dir() makes a folder", "box.dir(\"www/files/docs\")", gives("folder"), [] {
		Sandbox	box;

		box.dir("www/files/docs");
		return (box.kind("www/files/docs"));
	});
	t.test("fifo() makes a named pipe: it exists, but it isn't a regular file", "box.fifo(\"www/html/pipe\")",
		gives("fifo"), [] {
		Sandbox	box;

		box.fifo("www/html/pipe");
		return (box.kind("www/html/pipe"));
	});
	t.test("a path nobody made is missing", "box.kind(\"www/html/logo.png\")", gives("missing"), [] {
		Sandbox	box;

		return (box.kind("www/html/logo.png"));
	});
	t.test("a mode is set exactly, whatever the umask", "box.file(\"a.txt\", \"\", 0640) · box.dir(\"d\", 0700)",
		gives("640 700"), [] {
		Sandbox	box;

		box.file("a.txt", "", 0640);
		box.dir("d", 0700);
		return (modeOf("a.txt") + " " + modeOf("d"));
	});
	if (Sandbox::asRoot())
		t.known("running as root: access() ignores the mode, so a 403 test can't fail for the right reason");
	t.test("mode 000: access(R_OK) fails, so a 403 test fails for the right reason", "box.file(\"secret.txt\", \"s\", 0000)",
		gives("exists, not readable"), [] {
		Sandbox	box;

		box.file("secret.txt", "s", 0000);
		return (std::string(access("secret.txt", F_OK) == 0 ? "exists" : "missing")
			+ ", " + (access("secret.txt", R_OK) == 0 ? "readable" : "not readable"));
	});
	t.test("an absolute path is refused: a test never writes outside its folder", "box.file(\"/tmp/x\")",
		gives("Sandbox: '/tmp/x': paths are relative to the sandbox"), [] {
		Sandbox	box;

		try
		{
			box.file("/tmp/x");
		}
		catch (const std::invalid_argument &e)
		{
			return (std::string(e.what()));
		}
		return (std::string("wrote it"));
	});
	t.test("a path with '..' is refused", "box.file(\"www/../../x\")",
		gives("Sandbox: 'www/../../x': no '..', a test stays in its own folder"), [] {
		Sandbox	box;

		try
		{
			box.file("www/../../x");
		}
		catch (const std::invalid_argument &e)
		{
			return (std::string(e.what()));
		}
		return (std::string("wrote it"));
	});
	t.test("when it goes, its folder goes, and the working directory is back", "{ Sandbox box; ... }",
		gives("removed, back"), [] {
		std::filesystem::path	before = std::filesystem::current_path();
		std::string				root;

		{
			Sandbox	box;

			box.file("www/html/index.html", "x");
			box.fifo("www/html/pipe");
			root = box.root();
		}
		return (std::string(std::filesystem::exists(root) ? "still there" : "removed")
			+ ", " + (std::filesystem::current_path() == before ? "back" : "not back"));
	});
	t.test("an unreadable folder goes too", "box.file(\"locked/in.txt\", \"x\", 0000) · box.dir(\"locked\", 0000)",
		gives("removed"), [] {
		std::string	root;

		{
			Sandbox	box;

			box.file("locked/in.txt", "x", 0000);
			box.dir("locked", 0000);
			root = box.root();
		}
		return (std::string(std::filesystem::exists(root) ? "still there" : "removed"));
	});
	t.test("two sandboxes are two folders", "Sandbox a; Sandbox b;", gives("two folders"), [] {
		Sandbox	a;
		Sandbox	b;

		return (std::string(a.root() != b.root() ? "two folders" : "one folder"));
	});

	/* ------------------------------------------------ reading a response: the facts */

	auto	factsOf = [&t](const std::string &name, const std::string &input, const HttpResponse &response,
					const std::string &line, const std::vector<std::string> &facts) {
		t.test(name, input, gives(joinLines(facts)), [&] {
			HttpRequest	req = request(line);
			std::string	got;

			for (const std::string &fact : facts)
			{
				std::string	key = fact.substr(0, fact.find(" = "));
				got += (got.empty() ? "" : "\n") + key + " = " + responseFact(key, response, req, nullptr);
			}
			return (got);
		});
	};

	factsOf("the facts of makeError(404)", "GET /nope · HttpResponse::makeError(404)", HttpResponse::makeError(404),
		"GET /nope", {"status = 404", "reason = Not Found", "headers = Content-Type",
		"header Content-Type = text/html", "header Location = (none)",
		"body = <html><body><h1>404 Not Found</h1></body></html>", "body size = 48",
		"wire = HTTP/1.1 404 Not Found", "wire header Content-Length = 48", "wire body size = 48"});
	factsOf("a header name in any case", "HttpResponse::makeError(404)", HttpResponse::makeError(404),
		"GET /nope", {"header content-type = text/html", "wire header CONTENT-LENGTH = 48"});
	factsOf("the wire of a HEAD request: the length stays, the bytes go", "HEAD / · HttpResponse::make(200, \"hello\")",
		HttpResponse::make(200, "hello"), "HEAD /", {"body size = 5", "wire header Content-Length = 5", "wire body size = 0"});
	factsOf("a code reasonFor doesn't know", "HttpResponse::make(299)", HttpResponse::make(299),
		"GET /", {"reason = (none)", "wire = HTTP/1.1 299 Unknown"});
	factsOf("control bytes are written out, so a fact stays one line", "HttpResponse::make(200, \"a\\r\\nb\\t\\x01\")",
		HttpResponse::make(200, "a\r\nb\t\x01"), "GET /", {"body = a\\r\\nb\\t\\x01", "body size = 6"});
	factsOf("204: no body, and toBytes sends no Content-Length", "HttpResponse::make(204)", HttpResponse::make(204),
		"DELETE /x", {"body = (empty)", "wire header Content-Length = (none)", "wire body size = 0"});
	factsOf("a default HttpResponse says 200 OK (the default RouteResult says 500)", "HttpResponse response;",
		HttpResponse(), "GET /", {"status = 200", "reason = OK", "headers = (none)", "body = (empty)"});
	factsOf("an unknown fact says so", "HttpResponse::make(200)", HttpResponse::make(200),
		"GET /", {"colour = (no such fact: the keys are listed above responds() in harness/Harness.hpp)"});

	/* ------------------------------------------------ scenes */

	{
		Scene	scene("GET /files/docs?sort=name");

		scene.route(RouteAction::Directory, "/files/").dir("www/files/docs").file("www/html/secret.txt", "s", 0000);
		t.test("a scene describes itself: request, route, disk, config", "scene.describe()", gives(
			"request  GET /files/docs?sort=name HTTP/1.1\n"
			"route    Directory, location /files/ of server 0\n"
			"         filesystemPath ./www/files/docs\n"
			"disk     www/files/docs/   folder\n"
			"         www/html/secret.txt   file, 1 byte, mode 000\n"
			"config   conf/default.conf\n"), [&scene] {
			return (scene.describe());
		});
	}
	{
		Scene	scene("POST /upload/");

		scene.header("Content-Type", "multipart/form-data; boundary=x").body("hello").error(413, "/upload/");
		t.test("a scene's headers, body and an Error route", "scene.describe()", gives(
			"request  POST /upload/ HTTP/1.1\n"
			"         Content-Type: multipart/form-data; boundary=x\n"
			"         (body: 5 bytes)\n"
			"route    Error 413, location /upload/ of server 0\n"
			"disk     (nothing)\n"
			"config   conf/default.conf\n"), [&scene] {
			return (scene.describe());
		});
	}
	t.test("a scene without a route is a bug in the test", "Scene(\"GET /\").check({})",
		gives("Scene: no route. say what the router decided: route(), error() or errorBeforeRouting()"), [] {
		try
		{
			Scene("GET /").check({});
		}
		catch (const std::invalid_argument &e)
		{
			return (std::string(e.what()));
		}
		return (std::string("it ran"));
	});
	responds(t, "www() makes the tree conf/default.conf expects, and a GET leaves it as it was",
		Scene("GET /index.html").route(RouteAction::StaticFile, "/").www(),
		{"disk www/html/index.html = file, 17 bytes", "disk www/errors/404.html = file, 18 bytes",
		 "disk www/files = folder", "disk www/uploads = folder", "disk www/cgi-bin = folder"});

	/* ------------------------------------------------ the step 1 check */

	t.test("the step 1 check: a request and a route reach buildResponse",
		"GET /index.html · StaticFile, location / · www/html/index.html", gives("a response, status 100 to 599"), [] {
		Sandbox			box;
		box.file("www/html/index.html", "<h1>hi</h1>");
		Site			site(defaultConfig());
		HttpRequest		req = request("GET /index.html");
		RouteResult		route = site.route(RouteAction::StaticFile, req, "/");
		HttpResponse	response = buildResponse(req, route);

		if (response.statusCode < 100 || response.statusCode > 599)
			return ("status " + std::to_string(response.statusCode));
		return (std::string("a response, status 100 to 599"));
	});
	responds(t, "the same through a Scene", Scene("GET /index.html").route(RouteAction::StaticFile, "/").www(), {});
}
