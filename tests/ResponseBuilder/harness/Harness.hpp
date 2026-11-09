#pragma once

#include <cstddef>
#include <string>
#include <sys/types.h>
#include <utility>
#include <vector>

#include "RouteResult.hpp"
#include "config/ConfigParser.hpp"
#include "http/HttpRequest.hpp"
#include "http/HttpResponse.hpp"

class Tester;

/*
the harness: B's inputs, faked, so buildResponse can be tested before
Paola's parser and router exist. one piece per question of handover §7
(the decisions are recorded there, dated):

	request()   the HttpRequest C's parser would hand over        §7.2
	Site        the config, parsed once, owning what routes point at  §7.3
	Sandbox     a folder of its own per test: the files on disk   §7.4
	Scene       all of it as one description: a Triage card, a test

everything here runs inside the test's own process (the runner forks once
per test), so whatever a test does to the disk or the working directory is
undone when it ends.
*/

/*
=============================================================================
	the request (§7.2)
=============================================================================

"GET /files/docs?sort=name" -> the HttpRequest C's parser would produce:
	method   GET                     a string, as C delivers it
	target   /files/docs?sort=name   as written on the request line
	path     /files/docs             target up to the first '?'
	query    sort=name               after it, without the '?'
	version  HTTP/1.1                unless the line names one
	headers  Host: localhost
target, path and query come from one string, so they can't disagree: a test
that set target and forgot path would send a Directory redirect to "/".
splitting at the first '?' is all of C's parser this takes. nothing is
percent-decoded or normalised: by the time B sees a request, C has done it.
headers and body: set them on the result (req.headers["..."], req.body).
a line that isn't "METHOD TARGET [VERSION]" is a bug in the test:
std::invalid_argument.
*/
HttpRequest	request(const std::string &line);

/*
=============================================================================
	the config and the route (§7.3)
=============================================================================

the servers ConfigParser::parseString makes of a config, kept alive for as
long as the routes that point into them.

	RouteResult only BORROWS server and loc: two raw pointers into a
	std::vector<ServerConfig> someone else owns. in the server, that is the
	vector main() parses once and never changes. here it is _servers, and
	the rules are the same:
	  - a Site outlives every route it hands out. a route from a Site that
	    is gone points into freed memory, and ASan stops the test at the
	    first read through it: "heap-use-after-free" for route.loc->autoindex.
	    copying route.loc->path says "SEGV" instead: the first read then
	    happens inside std::string's copy, in a library ASan doesn't watch.
	  - _servers never changes after the constructor (it is const), so it
	    never reallocates. push_back on a full vector moves every element
	    to a new buffer, and a pointer taken before then points into the
	    old one: the same use-after-free.
	  - a Site can't be copied: the copy would have servers of its own,
	    while the routes of the original still point into the original.
	    (no move either, for the same reason: it never has to move.)

	location(path) finds a location block by its EXACT path. the harness
	never matches a URL to a location: that is Paola's router, where the
	longest prefix wins. a test names the block the router picked, the way
	a Triage card does, and a path no block has is a bug in the test.
*/
class Site
{
	public:
		explicit Site(const std::string &config);	// a ConfigError goes to the test
		Site(const Site &) = delete;
		Site	&operator=(const Site &) = delete;

		const std::vector<ServerConfig>	&servers() const;
		const ServerConfig				&server(size_t index = 0) const;
		const LocationConfig			&location(const std::string &path, size_t server = 0) const;

		/*
		the RouteResult the router would hand over for request, matched to
		the location block at locationPath:
			server, loc        into this Site's servers
			action             action
			StatusCode         200; for a Redirect, the block's return code
			filesystemPath     resolvePath(loc, request.path), the function
			                   C calls too; empty for a Redirect
			redirectLocation   for a Redirect, the block's return target
		filesystemPath is derived, not written into each test: it then
		follows the location's root and resolvePath's alias rule, exactly
		as C's will. what an Upload's filesystemPath means is still open
		(the folder or the final file, to agree with Paola): until then it
		is what resolvePath gives, and a test can overwrite it.
		an Error route needs a code: use error().
		*/
		RouteResult	route(RouteAction action, const HttpRequest &request,
						const std::string &locationPath, size_t server = 0) const;
		/* C's decision to answer with code. loc is null unless locationPath names one */
		RouteResult	error(int code, const std::string &locationPath = "", size_t server = 0) const;

	private:
		const std::vector<ServerConfig>	_servers;
};

/* an error C found before routing (400, 413, 414, 431, 505): no server, no location */
RouteResult	errorBeforeRouting(int code);

/* conf/default.conf, the config a Scene uses unless it is given its own */
std::string	defaultConfig();

/* "StaticFile", "Error"... */
std::string	actionName(RouteAction action);

/*
a route as facts, one per line:
	action = Directory
	StatusCode = 200
	server = server 0            null | server N (of site) | a server outside this Site
	loc = location /files/       null | location <its path>
	filesystemPath = ./www/files/docs
	redirectLocation = (empty)
*/
std::string	routeFacts(const RouteResult &route, const Site *site);

/*
=============================================================================
	the disk (§7.4)
=============================================================================

a folder of its own for one test, under build/sandbox/, and the working
directory for as long as it exists.

	one per test: the tests run one after the other on one disk. a DELETE
	test that removes www/html/index.html from a shared folder breaks every
	StaticFile test after it, depending on the order they run in. a fresh
	folder per test makes the order irrelevant.

	the working directory: the configs say root ./www/html, relative to
	wherever the server runs. the real server runs from the repo root; a
	test runs from its sandbox. conf/default.conf works unchanged, and the
	paths in a test read like the paths in the server.

	nothing committed: git stores neither a FIFO nor a file with mode 000
	(it keeps only the executable bit). every file is made by the test that
	needs it, from the facts on its Triage card.

	cleaning up: the destructor goes back to the old working directory and
	removes the folder, unreadable parts included. a test that crashes
	never reaches its destructor: its folder stays in build/sandbox/ to be
	looked at, and the next run (or make clean) removes it.

	paths are relative to the sandbox. an absolute path, or one with "..",
	is refused: a test never writes outside its own folder.

	ROOT: access(R_OK) succeeds for root whatever the mode says. as root (in
	a container, say) a mode-000 file is readable, and a 403 test fails for
	the wrong reason. asRoot() lets a test say so with t.known().
*/
class Sandbox
{
	public:
		Sandbox();
		~Sandbox();
		Sandbox(const Sandbox &) = delete;
		Sandbox	&operator=(const Sandbox &) = delete;

		/* the folders on the way are made too. a mode is set exactly, umask or not */
		void	file(const std::string &path, const std::string &content = "", mode_t mode = 0644) const;
		void	dir(const std::string &path, mode_t mode = 0755) const;
		void	fifo(const std::string &path) const;

		/* "file, 12 bytes" | "folder" | "fifo" | "missing" | "something else" */
		std::string			kind(const std::string &path) const;
		/* the whole file, byte for byte. std::runtime_error if it can't be read */
		std::string			read(const std::string &path) const;
		/* the sandbox folder, as an absolute path */
		const std::string	&root() const;

		static bool	asRoot();
		/* main(), before the run: the folders crashed tests left behind */
		static void	clearAll();

	private:
		std::string	_root;
		std::string	_previous;	// the working directory before

		std::string	inside(const std::string &path) const;
};

/*
=============================================================================
	a scene: one Triage card
=============================================================================

the request, the route C hands over, and the facts on disk - a description,
cheap and safe to copy. check() makes it real, in the process of the test
that runs it:
	1. a Sandbox: the working directory from then on
	2. the files, folders and FIFOs, in the order the scene lists them
	3. a Site from the config (conf/default.conf unless config() says otherwise)
	4. the request, and the route into the Site
	5. buildResponse(request, route)
	6. the facts the test names, read while the Sandbox and the Site still
	   exist, so a fact about the disk sees it as the request left it
*/
class Scene
{
	public:
		explicit Scene(const std::string &requestLine);

		/* the request */
		Scene	&header(const std::string &name, const std::string &value);
		Scene	&body(const std::string &bytes);

		/* the config, when conf/default.conf won't do */
		Scene	&config(const std::string &text);

		/* the route: what the router decided, and the location block it matched */
		Scene	&route(RouteAction action, const std::string &location, size_t server = 0);
		Scene	&error(int code, const std::string &location = "", size_t server = 0);
		Scene	&errorBeforeRouting(int code);
		Scene	&filesystemPath(const std::string &path);	// instead of what resolvePath gives

		/* the facts on disk */
		Scene	&file(const std::string &path, const std::string &content = "", mode_t mode = 0644);
		Scene	&dir(const std::string &path, mode_t mode = 0755);
		Scene	&fifo(const std::string &path);
		/*
		the folders and pages conf/default.conf expects, with short contents
		of their own (the repo's www/ can change; these can't):
			www/html/index.html     "<h1>webserv</h1>\n"
			www/errors/404.html     "<h1>404 page</h1>\n"
			www/errors/403.html     "<h1>403 page</h1>\n"
			www/errors/50x.html     "<h1>50x page</h1>\n"
			www/files/ www/uploads/ www/cgi-bin/    empty folders
		*/
		Scene	&www();

		/* the input the runner shows: request, route, disk, config */
		std::string	describe() const;
		/* steps 1 to 6 above. returns one "key = value" line per fact */
		std::string	check(const std::vector<std::string> &facts) const;

	private:
		enum RouteKind { NoRoute, Routed, Errored, BeforeRouting };

		struct DiskItem
		{
			enum Kind { File, Dir, Fifo };

			Kind		kind;
			std::string	path;
			std::string	content;
			mode_t		mode;
		};

		std::string									_line;
		std::vector<std::pair<std::string, std::string>>	_headers;
		std::string									_body;
		bool										_hasBody;
		std::string									_config;	// empty: conf/default.conf
		RouteKind									_routeKind;
		RouteAction									_action;
		std::string									_location;
		size_t										_server;
		int											_code;
		bool										_ownPath;
		std::string									_path;
		std::vector<DiskItem>						_disk;

		HttpRequest	makeRequest() const;
		RouteResult	makeRoute(const Site &site, const HttpRequest &request) const;
		std::string	routeText() const;
};

/*
=============================================================================
	the check
=============================================================================

responds(t, name, scene, facts): one test. scene runs in the test's own
process, then every fact named must hold - the way t.parses works for a
config. "got" lists the same keys with what the response really had, so the
wrong line shows in red. no facts: it passes when buildResponse returns.

	status                  404
	reason                  Not Found          HttpResponse::reasonFor; (none) if it has no phrase
	headers                 Content-Type Location   the names set, in the map's order; (none)
	header <Name>           text/html          (none) if not set. any case: the map ignores it
	body                    <h1>hi</h1>\n      the bytes; \r \n \t and other control bytes
	                                           written out, so a fact stays one line
	body size               48
	body same as <path>     yes                the file in the sandbox, after the request
	wire                    HTTP/1.1 404 Not Found   toBytes' first line. a HEAD request
	                                           gets toBytes(response, true)
	wire header <Name>      48                 a header line of toBytes' output, e.g.
	                                           Content-Length, which only toBytes adds
	wire body size          0                  the bytes after the empty line
	disk <path>             file, 3 bytes      the sandbox after the request:
	                                           file, N bytes | folder | fifo | missing
*/
void		responds(Tester &t, const std::string &name, const Scene &scene,
				const std::vector<std::string> &facts);

/* one fact of a response, by key. box may be null when no fact needs the disk */
std::string	responseFact(const std::string &key, const HttpResponse &response,
				const HttpRequest &request, const Sandbox *box);

/* bytes written out on one line: "a\r\nb" -> "a\\r\\nb", "" -> "(empty)" */
std::string	oneLine(const std::string &bytes);
