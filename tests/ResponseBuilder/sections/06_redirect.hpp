#pragma once

#include "Tester.hpp"
#include "../harness/Harness.hpp"
#include "http/ResponseBuilder.hpp"

#include <string>
#include <utility>
#include <vector>

/*
STEP 6 - Redirect. the router says "this location answers with a redirect":
the block's `return 301 /;`, copied into the route. the handler reads no
disk at all:

	status      route.StatusCode
	Location    route.redirectLocation, exactly as configured
	body        a short HTML page naming the code, Content-Type text/html

decided 2026-11-17:
	THE ROUTE, NOT THE CONFIG. RouteResult's field rules (architecture doc,
	"Field rules") make StatusCode "the intended success, redirect, or error
	status". the router may one day send a Redirect from a location with no
	`return` (the folder slash redirect, still open with Paola): there
	loc->redirect is empty and loc->redirectCode is only the default 301.
	ONLY THE FIVE REDIRECT CODES. the parser takes 301, 302, 303, 307 and
	308 in `return` and refuses the rest of 3xx at startup (config suite,
	section 7), so the handler never sees 300 or 304.
	NO SHARED HELPER with handleDirectory's 301: Ravi's call. the tests
	don't care either way.

the target goes out as configured, relative ("/") or absolute
("http://example.com/new"), and nothing of the request is added to it: not
the query, not the rest of the path. nginx's `return` does the same.
handleDirectory's slash redirect is the opposite: it is built FROM the
request, so it keeps the query.

the groups:
	the card            t8 from the Lab
	the codes           each one, its phrase on the wire and on the page
	the route decides   the code and the target from the route, not from loc
	as configured       an absolute target; query and path not added
	HEAD                the page's length, none of its bytes
	header injection    a bare LF in the target: 500, as CR LF is (section 2)
*/

/* "status = 301\nheader Location = /": the facts named, as responds() writes them */
inline std::string	redirectFacts(const HttpResponse &response, const HttpRequest &req,
						const std::vector<std::string> &keys)
{
	std::string	got;

	for (const std::string &key : keys)
		got += (got.empty() ? "" : "\n") + key + " = " + responseFact(key, response, req, nullptr);
	return (got);
}

inline void	testRedirect(Tester &t)
{
	/* ------------------------------------------------ the card */

	responds(t, "t8 · return 301 /: 301 with Location: /",
		Scene("GET /old/").route(RouteAction::Redirect, "/old/"),
		{"status = 301", "header Location = /"});

	/* ------------------------------------------------ the codes */

	/*
	each code in a config of its own, so it really travels from `return`
	through the route. the page names the code too: a handler that copies
	handleDirectory's page says "301 Moved Permanently" on a 307.
	*/
	const std::vector<std::pair<int, std::string>>	codes = {
		{301, "Moved Permanently"}, {302, "Found"}, {303, "See Other"},
		{307, "Temporary Redirect"}, {308, "Permanent Redirect"}};

	for (const auto &code : codes)
	{
		std::string	number = std::to_string(code.first);
		std::string	status = number + " " + code.second;

		responds(t, "return " + status + ": the code, its phrase on the wire and on the page, text/html",
			Scene("GET /old/")
				.config("server { listen 8080; location /old/ { return " + number + " /new/; } }")
				.route(RouteAction::Redirect, "/old/"),
			{"status = " + number, "wire = HTTP/1.1 " + status, "header Location = /new/",
			 "header Content-Type = text/html", "body has " + status + " = yes"});
	}

	/* ------------------------------------------------ the route decides */

	/*
	the router copies the block's `return` into the route, and the handler
	reads the route. here the two disagree on purpose: only a handler that
	reads route.StatusCode and route.redirectLocation passes.
	*/
	t.test("the code and the target come from the route, not from loc",
		"GET /old/ · Redirect, location /old/ (return 301 /) · StatusCode 307, redirectLocation /elsewhere/",
		gives("status = 307\nheader Location = /elsewhere/"), [] {
		Sandbox			box;
		Site			site(defaultConfig());
		HttpRequest		req = request("GET /old/");
		RouteResult		route = site.route(RouteAction::Redirect, req, "/old/");

		route.StatusCode = 307;
		route.redirectLocation = "/elsewhere/";
		return (redirectFacts(buildResponse(req, route), req, {"status", "header Location"}));
	});
	/*
	the folder slash redirect, if the router ends up sending it: a Redirect
	from /files/, a block with no `return`. Site::route won't make one (no
	router would from a config), so the route is set by hand.
	*/
	t.test("a Redirect the router built itself, from a location with no return",
		"GET /files/docs · Redirect, location /files/ (no return) · StatusCode 301, redirectLocation /files/docs/",
		gives("status = 301\nheader Location = /files/docs/"), [] {
		Sandbox			box;
		Site			site(defaultConfig());
		HttpRequest		req = request("GET /files/docs");
		RouteResult		route = site.route(RouteAction::StaticFile, req, "/files/");

		route.action = RouteAction::Redirect;
		route.StatusCode = 301;
		route.filesystemPath.clear();
		route.redirectLocation = "/files/docs/";
		return (redirectFacts(buildResponse(req, route), req, {"status", "header Location"}));
	});

	/* ------------------------------------------------ as configured */

	responds(t, "an absolute target goes out as it is",
		Scene("GET /old/")
			.config("server { listen 8080; location /old/ { return 302 http://example.com/new; } }")
			.route(RouteAction::Redirect, "/old/"),
		{"status = 302", "header Location = http://example.com/new"});
	responds(t, "the query is not added to the target",
		Scene("GET /old/?a=1").route(RouteAction::Redirect, "/old/"),
		{"status = 301", "header Location = /"});
	responds(t, "nor is the rest of the path: everything under /old/ goes to /",
		Scene("GET /old/deeper/page.html").route(RouteAction::Redirect, "/old/"),
		{"status = 301", "header Location = /"});

	/* ------------------------------------------------ HEAD */

	t.test("HEAD: Location and the page's Content-Length go out, the page's bytes don't",
		"HEAD /old/ · Redirect, location /old/",
		gives("status = 301\nwire header Location = /\nwire body size = 0\nContent-Length is the page's size = yes"), [] {
		Sandbox			box;
		Site			site(defaultConfig());
		HttpRequest		req = request("HEAD /old/");
		HttpResponse	response = buildResponse(req, site.route(RouteAction::Redirect, req, "/old/"));
		std::string		length = responseFact("wire header Content-Length", response, req, nullptr);
		std::string		size = std::to_string(response.body.size());

		return (redirectFacts(response, req, {"status", "wire header Location", "wire body size"})
			+ "\nContent-Length is the page's size = "
			+ (!response.body.empty() && length == size ? "yes" : "no (" + length + " for a " + size + "-byte page)"));
	});

	/* ------------------------------------------------ header injection */

	/*
	section 2 sends CR LF in the target. many clients end a header line at
	a bare LF too, so setHeader refuses one on its own, and a handler that
	strips only the CR still has to fail here.
	*/
	t.test("a bare LF in the target: 500, and nothing of it on the wire",
		"GET /old/ · Redirect, location /old/ · redirectLocation \"/\\nSet-Cookie: id=evil\"",
		gives("status = 500\nheader Location = (none)\nwire header Set-Cookie = (none)"), [] {
		Sandbox			box;
		Site			site(defaultConfig());
		HttpRequest		req = request("GET /old/");
		RouteResult		route = site.route(RouteAction::Redirect, req, "/old/");

		route.redirectLocation = "/\nSet-Cookie: id=evil";
		return (redirectFacts(buildResponse(req, route), req,
			{"status", "header Location", "wire header Set-Cookie"}));
	});
}
