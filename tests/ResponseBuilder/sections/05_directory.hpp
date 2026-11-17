#pragma once

#include "Tester.hpp"
#include "../harness/Harness.hpp"
#include "http/ResponseBuilder.hpp"

#include <functional>
#include <string>
#include <vector>

/*
STEP 5 - Directory. the router says "this is a folder": route.filesystemPath
is the folder. the handler answers one of four ways, checked in this order:

	req.path doesn't end in "/"    301 to req.path + "/", the query kept    t5
	the folder has loc->index      that file, served like a StaticFile      (index)
	loc->autoindex is on           200, an HTML listing of the folder       t6
	otherwise                      403                                      t7

decided 2026-11-13:
	RELATIVE links in the listing, href="a.txt" and href="docs/", as nginx
	writes them: the browser joins each one to the folder's URL, which the
	301 guarantees ends in "/". no location prefix needed.
	FOLDERS FIRST, then files, each group in byte order (std::sort on
	std::string, so "Zeta.txt" comes before "a.txt"), as nginx lists them.

what the listing must do whatever its layout (the layout itself is open):
	hide dot-files: ".hidden", and readdir's own "." and ".."
	one "../" link, so the client can go up
	"/" after a folder's name, in the link and in the text
	HTML-escape every name in the text: < > & become &lt; &gt; &amp;
	percent-encode every name in the link: every byte but A-Z a-z 0-9 - . _ ~
	becomes %XX, uppercase hex (RFC 3986 §2.1); a folder's "/" is added
	after the encoding
	escape the request path too, if the page shows it: it comes from the client

the groups:
	the cards           t5 t6 t7 from the Lab
	the redirect        the query kept, the path as the client sent it, first of all
	the index           found with and without "/" on filesystemPath, named by
	                    the config, served with StaticFile's rules
	the listing         links and their order, hidden entries, escaping, encoding
	404 or 403          a folder that is gone, a folder it may not read
	HEAD                the listing's length, none of its bytes
	file descriptors    opendir's fd is closed on every way out

still to agree with Paola (handover, decision log): whether the router or
B sends the slash redirect. these tests say B does it on a Directory route.
if the router does it too, B's check never fires, which is harmless.
*/

/* the targets of the page's links, in order: "../ docs/ a.txt". (no links) if there are none */
inline std::string	links(const std::string &body)
{
	std::string	out;
	size_t		pos = 0;

	while ((pos = body.find("href=\"", pos)) != std::string::npos)
	{
		size_t	end = body.find('"', pos + 6);

		if (end == std::string::npos)
			break;
		out += (out.empty() ? "" : " ") + body.substr(pos + 6, end - pos - 6);
		pos = end + 1;
	}
	return (out.empty() ? "(no links)" : out);
}

/* the four pieces: the disk, the site, a request on a Directory route into location, buildResponse */
inline HttpResponse	directoryResponse(const std::string &line, const std::string &location,
						const std::function<void(const Sandbox &)> &disk, const std::string &config = "")
{
	Sandbox		box;

	disk(box);
	Site		site(config.empty() ? defaultConfig() : config);
	HttpRequest	req = request(line);
	RouteResult	route = site.route(RouteAction::Directory, req, location);

	return (buildResponse(req, route));
}

inline void	testDirectory(Tester &t)
{
	/* ------------------------------------------------ the cards */

	responds(t, "t5 · a folder without its slash: 301, query kept",
		Scene("GET /files/docs?sort=name").route(RouteAction::Directory, "/files/")
			.dir("www/files/docs"),
		{"status = 301", "header Location = /files/docs/?sort=name"});
	responds(t, "t6 · no index, autoindex on: a listing",
		Scene("GET /files/").route(RouteAction::Directory, "/files/")
			.file("www/files/a.txt", "a"),
		{"status = 200", "header Content-Type = text/html", "body has href=\"a.txt\" = yes"});
	responds(t, "t7 · no index, autoindex off: 403",
		Scene("GET /private/").route(RouteAction::Directory, "/")
			.dir("www/html/private"),
		{"status = 403"});

	/* ------------------------------------------------ the redirect */

	responds(t, "no query: Location is the path plus the slash, nothing more",
		Scene("GET /files/docs").route(RouteAction::Directory, "/files/")
			.dir("www/files/docs"),
		{"status = 301", "header Location = /files/docs/", "wire = HTTP/1.1 301 Moved Permanently"});
	/*
	req.path is the path as the client sent it, still percent-encoded (C's
	router decodes a copy to make filesystemPath). Location must send it
	back the same way: decoding it here would put a space in a header.
	*/
	responds(t, "the redirect keeps the path as the client sent it",
		Scene("GET /files/my%20docs?x=1").route(RouteAction::Directory, "/files/")
			.dir("www/files/my%20docs"),
		{"status = 301", "header Location = /files/my%20docs/?x=1"});
	responds(t, "the missing slash is checked first: 301, not 403, with autoindex off",
		Scene("GET /private").route(RouteAction::Directory, "/")
			.dir("www/html/private"),
		{"status = 301", "header Location = /private/"});

	/* ------------------------------------------------ the index */

	/*
	resolvePath gives "./www/files" for GET /files/ (nothing after the
	prefix) and "./www/files/docs/" for GET /files/docs/: the index must be
	found behind both shapes.
	*/
	responds(t, "the index is served, and wins over autoindex: GET /files/ (filesystemPath ./www/files)",
		Scene("GET /files/").route(RouteAction::Directory, "/files/")
			.file("www/files/index.html", "<h1>files</h1>\n").file("www/files/a.txt", "a"),
		{"status = 200", "header Content-Type = text/html", "body same as www/files/index.html = yes"});
	responds(t, "the index is served: GET /files/docs/ (filesystemPath ./www/files/docs/)",
		Scene("GET /files/docs/").route(RouteAction::Directory, "/files/")
			.file("www/files/docs/index.html", "<h1>docs</h1>\n"),
		{"status = 200", "body same as www/files/docs/index.html = yes"});
	responds(t, "the index's name comes from the config, and its type from that name",
		Scene("GET /files/")
			.config("server { listen 8080; location /files/ { root ./www/files; index home.txt; autoindex on; } }")
			.route(RouteAction::Directory, "/files/")
			.file("www/files/home.txt", "home").file("www/files/index.html", "wrong"),
		{"status = 200", "header Content-Type = text/plain", "body = home"});
	if (Sandbox::asRoot())
		t.known("running as root: access() ignores the mode, so a 403 test can't fail for the right reason");
	responds(t, "an index it may not read: 403, as a StaticFile would be",
		Scene("GET /files/").route(RouteAction::Directory, "/files/")
			.file("www/files/index.html", "x", 0000),
		{"status = 403"});

	/* ------------------------------------------------ the listing */

	t.test("every entry is a link: ../ first, then folders, then files, each group in byte order",
		"GET /files/ · Directory, location /files/ · b.txt a.txt Zeta.txt docs/ images/",
		gives("../ docs/ images/ Zeta.txt a.txt b.txt"), [] {
		return (links(directoryResponse("GET /files/", "/files/", [](const Sandbox &box) {
			box.file("www/files/b.txt", "b");
			box.file("www/files/a.txt", "a");
			box.file("www/files/Zeta.txt", "z");
			box.dir("www/files/docs");
			box.dir("www/files/images");
		}).body));
	});
	t.test("dot-files are hidden, and so are readdir's own . and ..: one ../ link",
		"GET /files/ · Directory, location /files/ · .hidden .git/ a.txt",
		gives("../ a.txt"), [] {
		return (links(directoryResponse("GET /files/", "/files/", [](const Sandbox &box) {
			box.file("www/files/.hidden", "h");
			box.dir("www/files/.git");
			box.file("www/files/a.txt", "a");
		}).body));
	});
	responds(t, "an empty folder: a listing with only ../",
		Scene("GET /files/empty/").route(RouteAction::Directory, "/files/")
			.dir("www/files/empty"),
		{"status = 200", "body has href=\"../\" = yes", "body has href=\"./\" = no"});
	/* a name is text on the page: < > & must be entities, or the name becomes markup */
	responds(t, "a name is HTML-escaped in the text",
		Scene("GET /files/").route(RouteAction::Directory, "/files/")
			.file("www/files/<marquee>&co.txt", "x"),
		{"status = 200", "body has &lt;marquee&gt;&amp;co.txt = yes", "body has <marquee> = no"});
	/* a name is a URL in the link: space ? # % mean something there, so they must be %XX */
	responds(t, "a name is percent-encoded in the link, and left as it is in the text",
		Scene("GET /files/").route(RouteAction::Directory, "/files/")
			.file("www/files/a b?c#d%e.txt", "x"),
		{"status = 200", "body has href=\"a%20b%3Fc%23d%25e.txt\" = yes", "body has a b?c#d%e.txt = yes"});
	responds(t, "quotes and angle brackets can't break out of the link",
		Scene("GET /files/").route(RouteAction::Directory, "/files/")
			.file("www/files/x\"y<z>.txt", "x"),
		{"status = 200", "body has href=\"x%22y%3Cz%3E.txt\" = yes"});
	responds(t, "a folder's link is encoded, then the slash is added",
		Scene("GET /files/").route(RouteAction::Directory, "/files/")
			.dir("www/files/my docs"),
		{"status = 200", "body has href=\"my%20docs/\" = yes"});
	/* "café" is c a f 0xC3 0xA9 in UTF-8: bytes outside ASCII are encoded too, with uppercase hex */
	responds(t, "bytes outside ASCII are encoded, uppercase hex",
		Scene("GET /files/").route(RouteAction::Directory, "/files/")
			.file("www/files/caf\xC3\xA9.txt", "x"),
		{"status = 200", "body has href=\"caf%C3%A9.txt\" = yes"});
	/* the request path comes from the client. a page that shows it must escape it; one that doesn't passes too */
	responds(t, "the request path, if the page shows it, is escaped",
		Scene("GET /files/<marquee>/").route(RouteAction::Directory, "/files/")
			.dir("www/files/<marquee>"),
		{"status = 200", "body has <marquee> = no"});

	/* ------------------------------------------------ 404 or 403 */

	responds(t, "the folder is gone: 404",
		Scene("GET /files/gone/").route(RouteAction::Directory, "/files/"),
		{"status = 404"});
	if (Sandbox::asRoot())
		t.known("running as root: access() ignores the mode, so a 403 test can't fail for the right reason");
	responds(t, "a folder it may not read, autoindex on: 403",
		Scene("GET /files/locked/").route(RouteAction::Directory, "/files/")
			.dir("www/files/locked", 0000),
		{"status = 403"});

	/* ------------------------------------------------ HEAD */

	t.test("HEAD on a listing: the page is built, its Content-Length goes out, its bytes don't",
		"HEAD /files/ · Directory, location /files/ · a.txt", gives("a page, its length, no bytes"), [] {
		HttpResponse	response = directoryResponse("HEAD /files/", "/files/", [](const Sandbox &box) {
			box.file("www/files/a.txt", "a");
		});
		std::string		wire = toBytes(response, true);
		size_t			blank = wire.find("\r\n\r\n");

		if (response.statusCode != 200)
			return ("status " + std::to_string(response.statusCode));
		if (response.body.empty())
			return (std::string("an empty body"));
		if (blank == std::string::npos)
			return (std::string("no empty line on the wire"));
		if (wire.find("Content-Length: " + std::to_string(response.body.size()) + "\r\n") == std::string::npos)
			return (std::string("Content-Length isn't the page's size"));
		if (blank + 4 != wire.size())
			return (std::to_string(wire.size() - blank - 4) + " bytes after the empty line");
		return (std::string("a page, its length, no bytes"));
	});

	/* ------------------------------------------------ file descriptors */

	/*
	opendir() takes a file descriptor and closedir() gives it back. every
	way out of the handler must close it: a leak of one fd per listing
	stops the server after about a thousand folder views.
	*/
	auto	noFdLeft = [&t](const std::string &name, const std::string &line, const std::string &location,
						std::function<void(const Sandbox &)> disk) {
		t.test(name, line + " · Directory, location " + location, gives("as many open fds after buildResponse as before"),
			[line, location, disk] {
			Sandbox			box;

			disk(box);
			Site			site(defaultConfig());
			HttpRequest		req = request(line);
			RouteResult		route = site.route(RouteAction::Directory, req, location);
			int				before = openFds();
			HttpResponse	response = buildResponse(req, route);
			int				after = openFds();

			(void)response;
			if (before < 0)
				return (std::string("can't list /dev/fd"));
			if (after != before)
				return (std::to_string(after - before) + " more open after buildResponse than before");
			return (std::string("as many open fds after buildResponse as before"));
		});
	};

	noFdLeft("no fd left open after a listing", "GET /files/", "/files/",
		[](const Sandbox &box) { box.file("www/files/a.txt", "a"); box.dir("www/files/docs"); });
	noFdLeft("no fd left open after the index is served", "GET /files/", "/files/",
		[](const Sandbox &box) { box.file("www/files/index.html", "<h1>files</h1>"); });
	noFdLeft("no fd left open after a 403 (autoindex off)", "GET /private/", "/",
		[](const Sandbox &box) { box.dir("www/html/private"); });
	noFdLeft("no fd left open after a 404 (the folder is gone)", "GET /files/gone/", "/files/",
		[](const Sandbox &) {});
	noFdLeft("no fd left open after a 301", "GET /files/docs", "/files/",
		[](const Sandbox &box) { box.dir("www/files/docs"); });
}
