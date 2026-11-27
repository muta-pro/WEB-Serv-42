#pragma once

#include "Tester.hpp"
#include "../harness/Harness.hpp"
#include "http/ResponseBuilder.hpp"
#include "02_skeleton.hpp"	// builtInPage(code)

#include <sys/types.h>
#include <unistd.h>
#include <functional>
#include <string>
#include <vector>

/*
STEP 7 - error pages. buildErrorResponse(code, route) answers with the
server's own page for that code when it has one, and with makeError's
built-in page otherwise:

	route.server null                  built-in (an error before routing)
	server->errorPageUri(code) is ""   built-in (no error_page for the code)
	match the URI to a location        the longest loc.path that is a prefix
	                                   of the URI; none: built-in
	resolvePath(that location, URI)    /errors/404.html -> ./www/errors/404.html
	stat, S_ISREG, access, the limit   as handleStaticFile; any failure: built-in
	read it                            make(code, page), Content-Type from its name

the code stays the one asked for: the 404 page goes out as a 404, never as
a 200. one hop: when the page can't be served, the built-in page answers.
there is no second lookup, and nothing that calls back into
buildErrorResponse.

then, on every error answer, custom page or built-in:
	Allow             on a 405, from route.loc->allowedMethods, in the set's
	                  order (the enum's: GET, POST, HEAD, DELETE), ", " between
	Connection: close on 400, 408 and 413. the header only announces it: A
	                  reads it and closes the socket after sending (to agree
	                  with Ivan)

and `internal` (conf/default.conf: location /errors/ { internal; }): a
request the router matched to an internal location gets a 404, whatever
the action. only the page lookup above may read from it.

decided 2026-11-25 (handover §6, step 7):
	D1 the URI is matched by a static longest-prefix helper of B's own until
	   Paola's shared one exists. the tests don't name it.
	D2 internal is B's: one check at the top of buildResponse, before the
	   switch. it wins over C's own error there (nginx checks internal when
	   it picks the location, before the methods). harmless if the router
	   checks too.
	D3 the page's location is used for its root only: its methods, cgi and
	   return don't matter (nginx serves the page as a GET).
	D4 a 405 without a location gets no Allow: there is nothing to build it
	   from, and the router only knows 405 after matching.
	D5 Connection: close on 400, 408 and 413 only.

the groups:
	the server's page       the page, its code, its location's root, its type
	one hop                 every way the page can fail: the built-in page, same code
	internal                /errors/ answers 404 to a client
	Allow                   on every 405, from the location
	Connection: close       400, 408, 413, and nothing else
	HEAD                    the page's length, none of its bytes
	file descriptors        none left open, page or no page

the sandbox is empty unless a test says otherwise: Scene::www() makes the
three pages of conf/default.conf, each with short contents of its own
("<h1>404 page</h1>\n", 18 bytes), so a body can't be the built-in page by
accident.
*/

/* a config of conf/default.conf's shape around one error_page line */
inline std::string	errorConfig(const std::string &errorPageLine, const std::string &rootLocation = "location / { }")
{
	return ("server { listen 8080; root ./www/html; " + errorPageLine + " " + rootLocation
		+ " location /errors/ { root ./www/errors; internal; } }");
}

inline void	testErrors(Tester &t)
{
	/* ------------------------------------------------ the server's page */

	responds(t, "/nope: 404 with the server's page, www/errors/404.html",
		Scene("GET /nope").route(RouteAction::StaticFile, "/").www(),
		{"status = 404", "wire = HTTP/1.1 404 Not Found", "header Content-Type = text/html",
		 "body same as www/errors/404.html = yes"});
	responds(t, "an Error route from C gets the page too",
		Scene("GET /nope").error(404, "/").www(),
		{"status = 404", "body same as www/errors/404.html = yes"});
	responds(t, "a folder sent as a file: 403 with www/errors/403.html",
		Scene("GET /docs").route(RouteAction::StaticFile, "/").www().dir("www/html/docs"),
		{"status = 403", "body same as www/errors/403.html = yes"});
	/* error_page 500 502 504 /errors/50x.html: one page, three codes. each keeps its own */
	for (int code : {500, 502, 504})
		responds(t, std::to_string(code) + ": www/errors/50x.html, and the code stays " + std::to_string(code),
			Scene("GET /").error(code, "/").www(),
			{"status = " + std::to_string(code),
			 "wire = HTTP/1.1 " + std::to_string(code) + " " + HttpResponse::reasonFor(code),
			 "body same as www/errors/50x.html = yes"});

	/*
	route.loc is where the error happened, not where the page lives.
	resolvePath(location /, "/errors/404.html") gives ./www/html/errors/404.html,
	and a decoy waits there. only the URI's own location, /errors/, finds the
	real page. "/" is declared first, so a first-match finds the decoy too.
	*/
	responds(t, "the page's location comes from its URI, not from route.loc",
		Scene("GET /nope").route(RouteAction::StaticFile, "/").www()
			.file("www/html/errors/404.html", "<h1>decoy</h1>\n"),
		{"status = 404", "body same as www/errors/404.html = yes", "body has decoy = no"});
	/* resolvePath(location /files/, "/errors/404.html") gives ./www/files/404.html: a decoy there too */
	responds(t, "an error in another location gets the same page: error pages belong to the server",
		Scene("GET /files/nope.txt").route(RouteAction::StaticFile, "/files/").www()
			.file("www/files/404.html", "<h1>decoy</h1>\n"),
		{"status = 404", "body same as www/errors/404.html = yes"});
	responds(t, "the page's Content-Type follows its file name",
		Scene("GET /nope")
			.config(errorConfig("error_page 404 /errors/404.txt;"))
			.route(RouteAction::StaticFile, "/")
			.file("www/errors/404.txt", "no such page\n"),
		{"status = 404", "header Content-Type = text/plain", "body same as www/errors/404.txt = yes"});
	/* C knows the server but no location yet (a 400 found after the Host header): the pages still apply */
	responds(t, "a server but no location: the page still applies",
		Scene("GET /")
			.config(errorConfig("error_page 400 /errors/400.html;"))
			.error(400)
			.file("www/errors/400.html", "<h1>400 page</h1>\n"),
		{"status = 400", "body same as www/errors/400.html = yes"});

	/* ------------------------------------------------ one hop */

	/*
	every way the page can fail ends in the built-in page, with the code
	that was asked for. a page reader that calls back into
	buildErrorResponse (handleStaticFile does, on a missing file) never
	ends: the stack overflows and ASan shows a crash. the runner's 2 s limit
	catches one that loops without growing the stack.

	today's fallback answers all of these already: they pass by accident
	until the lookup exists, and from then on they guard it.
	*/
	responds(t, "no error_page for the code: the built-in page",
		Scene("DELETE /").error(405, "/").www(),
		{"status = 405", "body = " + builtInPage(405)});
	responds(t, "no server (an error before routing): the built-in page",
		Scene("GET /").errorBeforeRouting(500).www(),
		{"status = 500", "body = " + builtInPage(500)});
	responds(t, "the page is missing: the built-in page, and the request still ends",
		Scene("GET /nope").route(RouteAction::StaticFile, "/"),
		{"status = 404", "header Content-Type = text/html", "body = " + builtInPage(404)});
	/* a recursive lookup would answer the missing 403.html with a 404 */
	responds(t, "a missing page doesn't change the code: 403 stays 403",
		Scene("GET /docs").route(RouteAction::StaticFile, "/").dir("www/html/docs"),
		{"status = 403", "body = " + builtInPage(403)});
	responds(t, "the page is a folder: the built-in page",
		Scene("GET /nope").route(RouteAction::StaticFile, "/").dir("www/errors/404.html"),
		{"status = 404", "body = " + builtInPage(404)});
	responds(t, "the page is a FIFO: the built-in page, and no hang",
		Scene("GET /nope").route(RouteAction::StaticFile, "/").fifo("www/errors/404.html"),
		{"status = 404", "body = " + builtInPage(404)});
	if (Sandbox::asRoot())
		t.known("running as root: access() ignores the mode, so this can't fail for the right reason");
	responds(t, "the page may not be read: the built-in page",
		Scene("GET /nope").route(RouteAction::StaticFile, "/").file("www/errors/404.html", "x", 0000),
		{"status = 404", "body = " + builtInPage(404)});
	/*
	no location claims /pages/. a lookup that falls back to route.loc
	(/files/) finds ./www/files/404.html: a decoy.
	*/
	responds(t, "a page URI no location claims: the built-in page",
		Scene("GET /files/nope.txt")
			.config("server { listen 8080; error_page 404 /pages/404.html; location /files/ { root ./www/files; } }")
			.route(RouteAction::StaticFile, "/files/")
			.file("www/files/404.html", "<h1>decoy</h1>\n")
			.file("www/html/pages/404.html", "<h1>decoy</h1>\n"),
		{"status = 404", "body = " + builtInPage(404)});
	/* the sparse 4 GiB file of section 3, as the page: the limit applies to pages too */
	t.test("the page is over the size limit: the built-in page, decided before reading",
		"GET /nope · StaticFile, location / · www/errors/404.html: 4 GiB, sparse",
		gives("status = 404\nbody = " + builtInPage(404)), [] {
		Sandbox			box;

		box.file("www/errors/404.html", "");
		if (truncate("www/errors/404.html", static_cast<off_t>(4) << 30) != 0)
			return (std::string("truncate() failed: the sandbox can't hold a sparse file"));

		Site			site(defaultConfig());
		HttpRequest		req = request("GET /nope");
		HttpResponse	response = buildResponse(req, site.route(RouteAction::StaticFile, req, "/"));

		return ("status = " + responseFact("status", response, req, nullptr)
			+ "\nbody = " + responseFact("body", response, req, nullptr));
	});

	/* ------------------------------------------------ internal */

	/*
	the router matches /errors/404.html to location /errors/ and sends a
	StaticFile, as for any file. the block is internal, so the client gets
	a 404, which then has its own page: the same file, now as a 404. nginx
	answers the same.
	*/
	responds(t, "GET /errors/404.html: 404, not the file as a 200",
		Scene("GET /errors/404.html").route(RouteAction::StaticFile, "/errors/").www(),
		{"status = 404", "body same as www/errors/404.html = yes"});
	responds(t, "GET /errors/403.html: 404 too, with the 404 page",
		Scene("GET /errors/403.html").route(RouteAction::StaticFile, "/errors/").www(),
		{"status = 404", "body same as www/errors/404.html = yes"});
	responds(t, "GET /errors/: the folder is internal too",
		Scene("GET /errors/").route(RouteAction::Directory, "/errors/").www(),
		{"status = 404"});
	/* D2: the location is internal before its methods count, so no 405 and no Allow either */
	responds(t, "internal wins over C's own error there: 404, not 405",
		Scene("DELETE /errors/404.html").error(405, "/errors/").www(),
		{"status = 404", "header Allow = (none)"});

	/* ------------------------------------------------ Allow */

	/*
	RFC 9110 §15.5.6: a 405 MUST list the methods the target allows. they
	come from the location the router matched. a std::set iterates in the
	enum's order, so GET, POST, HEAD, DELETE.
	*/
	responds(t, "DELETE /: 405 with Allow: GET, HEAD",
		Scene("DELETE /").error(405, "/").www(),
		{"status = 405", "header Allow = GET, HEAD", "wire header Allow = GET, HEAD"});
	responds(t, "/upload/: Allow lists its four methods",
		Scene("PUT /upload/cat.png").error(405, "/upload/").www(),
		{"status = 405", "header Allow = GET, POST, HEAD, DELETE"});
	/* no GET, so no implicit HEAD either: Allow is read from the location, never written out by hand */
	responds(t, "a POST-only location: Allow: POST",
		Scene("GET /")
			.config("server { listen 8080; root ./www/html; location / { methods POST; } }")
			.error(405, "/"),
		{"status = 405", "header Allow = POST"});
	responds(t, "a 405 with a page of its own still gets Allow",
		Scene("DELETE /")
			.config(errorConfig("error_page 405 /errors/405.html;"))
			.error(405, "/")
			.file("www/errors/405.html", "<h1>405 page</h1>\n"),
		{"status = 405", "body same as www/errors/405.html = yes", "header Allow = GET, HEAD"});
	for (int code : {404, 403, 500})
		responds(t, "no Allow on a " + std::to_string(code),
			Scene("GET /").error(code, "/").www(),
			{"status = " + std::to_string(code), "header Allow = (none)"});

	/* ------------------------------------------------ Connection: close */

	/*
	after a 400 the server can't tell where the request ended; after a 413
	the body was never read, so its bytes would be parsed as the next
	request; a 408 means the client is too slow to keep. for each, the
	connection ends after this answer.
	*/
	responds(t, "400 before routing: Connection: close",
		Scene("GET /").errorBeforeRouting(400),
		{"status = 400", "header Connection = close", "wire header Connection = close"});
	responds(t, "413 before routing: Connection: close",
		Scene("POST /upload/").errorBeforeRouting(413),
		{"status = 413", "header Connection = close"});
	/* the location's own limit, checked by the router after matching */
	responds(t, "413 from a location: Connection: close",
		Scene("POST /upload/").error(413, "/upload/").www(),
		{"status = 413", "header Connection = close"});
	responds(t, "400 with a page of its own: Connection: close too",
		Scene("GET /")
			.config(errorConfig("error_page 400 /errors/400.html;"))
			.error(400)
			.file("www/errors/400.html", "<h1>400 page</h1>\n"),
		{"status = 400", "body same as www/errors/400.html = yes", "header Connection = close"});
	/*
	a timeout is A's to notice, so A calls buildErrorResponse itself,
	outside buildResponse's try: nothing may escape from it.
	*/
	t.test("408, called the way A would: Connection: close",
		"buildErrorResponse(408, errorBeforeRouting(408))",
		gives("status = 408\nheader Content-Type = text/html\nheader Connection = close"), [] {
		HttpRequest		req = request("GET /");
		HttpResponse	response = buildErrorResponse(408, errorBeforeRouting(408));

		return ("status = " + responseFact("status", response, req, nullptr)
			+ "\nheader Content-Type = " + responseFact("header Content-Type", response, req, nullptr)
			+ "\nheader Connection = " + responseFact("header Connection", response, req, nullptr));
	});
	for (int code : {404, 405, 500})
		responds(t, "no Connection header on a " + std::to_string(code),
			Scene("GET /").error(code, "/").www(),
			{"status = " + std::to_string(code), "header Connection = (none)"});

	/* ------------------------------------------------ HEAD */

	responds(t, "HEAD of a 404: the page's Content-Length, none of its bytes",
		Scene("HEAD /nope").route(RouteAction::StaticFile, "/").www(),
		{"status = 404", "wire header Content-Length = 18", "wire body size = 0"});

	/* ------------------------------------------------ file descriptors */

	/* as in section 3: the count right around buildResponse, a 404 for a missing /nope */
	auto	noFdLeft = [&t](const std::string &name, std::function<void(const Sandbox &)> disk) {
		t.test(name, "GET /nope · StaticFile, location /", gives("as many open fds after buildResponse as before"),
			[disk] {
			Sandbox			box;

			disk(box);
			Site			site(defaultConfig());
			HttpRequest		req = request("GET /nope");
			RouteResult		route = site.route(RouteAction::StaticFile, req, "/");
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

	noFdLeft("no fd left open after serving the page",
		[](const Sandbox &box) { box.file("www/errors/404.html", "<h1>404 page</h1>\n"); });
	noFdLeft("no fd left open when the page is missing",
		[](const Sandbox &) {});
	noFdLeft("no fd left open when the page is a folder",
		[](const Sandbox &box) { box.dir("www/errors/404.html"); });
	noFdLeft("no fd left open when the page is a FIFO",
		[](const Sandbox &box) { box.fifo("www/errors/404.html"); });
}
