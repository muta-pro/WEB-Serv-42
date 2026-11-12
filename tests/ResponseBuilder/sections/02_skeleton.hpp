#pragma once

#include "Tester.hpp"
#include "../harness/Harness.hpp"
#include "http/ResponseBuilder.hpp"

#include <string>
#include <vector>

/*
STEP 2 - the skeleton. buildResponse gets its shape: a switch on
route.action with a handler per case, a default that answers 500, and one
try/catch of std::exception that answers makeError(500). nothing gets
served yet; what this section pins is that EVERY route gets an answer, and
that nothing ever escapes buildResponse.

	the groups:
	C already decided   an Error route: the code C chose comes back
	every action        each RouteAction reaches buildResponse and returns
	the default         an action the switch has no case for: 500
	the catch           a handler that throws: 500, nothing escapes

every test here stays true after later steps land: none of them names a
status a later step will change. step 7 (error pages) changes what the body
of an Error route is when the server has a page for it, so those tests ask
only for the status; the ones without a server (C's errors before routing)
keep the built-in page forever, because a null route.server goes straight
to makeError (handover §6, step 7).

how "nothing escapes" is checked: each test runs in its own forked process.
an exception that leaves buildResponse ends in std::terminate, the process
aborts, and the runner shows it as a crash instead of a wrong fact. a test
that gets its facts back has proved nothing escaped.
*/

/* the built-in page makeError writes: <html><body><h1>500 Internal Server Error</h1></body></html> */
inline std::string	builtInPage(int code)
{
	return ("<html><body><h1>" + std::to_string(code) + " " + HttpResponse::reasonFor(code)
		+ "</h1></body></html>");
}

/* the named facts of a response, one "key = value" per line, the way responds() reports them */
inline std::string	skeletonFacts(const HttpResponse &response, const HttpRequest &req,
						const std::vector<std::string> &keys)
{
	std::string	got;

	for (const std::string &key : keys)
		got += (got.empty() ? "" : "\n") + key + " = " + responseFact(key, response, req, nullptr);
	return (got);
}

inline void	testSkeleton(Tester &t)
{
	/* ------------------------------------------------ C already decided */

	/*
	RouteResult's defaults are action Error, StatusCode 500, no server, no
	location. the stub of step 1 answers it with a default HttpResponse:
	200 OK. this is the test that catches that lie.
	*/
	t.test("a default RouteResult gets 500", "GET / · RouteResult route; (Error, 500, no server, no location)",
		gives("status = 500\nheader Content-Type = text/html\nbody = " + builtInPage(500)), [] {
		HttpRequest		req = request("GET /");
		RouteResult		route;
		HttpResponse	response = buildResponse(req, route);

		return (skeletonFacts(response, req, {"status", "header Content-Type", "body"}));
	});

	/*
	errors C finds before routing: no server is known yet, so there is no
	custom page to look for. the code C chose is the code that goes out.
	*/
	for (int code : {400, 413, 414, 431, 505})
		responds(t, "an error before routing keeps C's code: " + std::to_string(code),
			Scene("GET /").errorBeforeRouting(code),
			{"status = " + std::to_string(code), "header Content-Type = text/html"});
	responds(t, "an error before routing gets the built-in page",
		Scene("GET /").errorBeforeRouting(400),
		{"body = " + builtInPage(400), "wire = HTTP/1.1 400 Bad Request"});

	/*
	an Error route inside a server: step 7 will look for the server's page
	for that code (404 has one in conf/default.conf). whatever the body
	becomes, the status stays C's.
	*/
	responds(t, "an Error route inside a server keeps C's code: 404",
		Scene("GET /nope").error(404, "/"), {"status = 404"});
	responds(t, "an Error route inside a server keeps C's code: 405",
		Scene("DELETE /").error(405, "/"), {"status = 405"});

	/* ------------------------------------------------ every action */

	/*
	no facts: each passes when buildResponse returns. today's stub returns
	for everything, so these pass already. they are here for later: a
	switch that forgets a case, or a handler whose exception gets past the
	catch, shows up as a crash in one of them.
	*/
	responds(t, "StaticFile gets an answer", Scene("GET /index.html").route(RouteAction::StaticFile, "/"), {});
	responds(t, "Directory gets an answer", Scene("GET /files/").route(RouteAction::Directory, "/files/"), {});
	responds(t, "Redirect gets an answer", Scene("GET /old/").route(RouteAction::Redirect, "/old/"), {});
	responds(t, "Upload gets an answer", Scene("POST /upload/").route(RouteAction::Upload, "/upload/"), {});
	responds(t, "DeleteResource gets an answer",
		Scene("DELETE /upload/cat.png").route(RouteAction::DeleteResource, "/upload/"), {});
	responds(t, "CGI gets an answer", Scene("GET /cgi-bin/hello.py").route(RouteAction::CGI, "/cgi-bin/"), {});

	/* ------------------------------------------------ the default */

	/*
	RouteAction is an enum class, so its underlying type is int and any int
	is a valid value: static_cast<RouteAction>(99) is legal C++, not
	undefined behaviour (UBSan agrees). it is what buildResponse would see
	if C added an action to RouteResult.hpp and nobody told the switch.
	*/
	responds(t, "t23 · an action the switch has no case for gets 500",
		Scene("GET /x").route(static_cast<RouteAction>(99), "/"),
		{"status = 500", "wire = HTTP/1.1 500 Internal Server Error"});

	/* ------------------------------------------------ the catch */

	/*
	a Location value with CR LF in it is header injection: setHeader throws
	std::invalid_argument on it (HttpResponse.cpp), and the catch in
	buildResponse turns that into a 500. the config parser never makes such
	a route, so the test writes one by hand.

	today (step 2) the Redirect handler is still a stub, so whatever the
	stub does decides this test: a stub that throws passes it, a stub that
	returns something else doesn't. from step 6 on, the real Redirect
	handler calls setHeader("Location", ...) and the throw is setHeader's.
	the test is the same either way: a 500, the built-in page, and nothing
	of the injected line anywhere in the response.
	*/
	t.test("a handler that throws gets 500, and nothing escapes",
		"GET /old/ · Redirect, location /old/ · redirectLocation \"/\\r\\nSet-Cookie: id=evil\"",
		gives("status = 500\nheader Content-Type = text/html\nheader Location = (none)\nbody = "
			+ builtInPage(500)), [] {
		Sandbox			box;
		Site			site(defaultConfig());
		HttpRequest		req = request("GET /old/");
		RouteResult		route = site.route(RouteAction::Redirect, req, "/old/");

		route.redirectLocation = "/\r\nSet-Cookie: id=evil";
		HttpResponse	response = buildResponse(req, route);

		return (skeletonFacts(response, req,
			{"status", "header Content-Type", "header Location", "body"}));
	});
	t.test("the 500 from a throw is a whole response on the wire",
		"GET /old/ · Redirect, location /old/ · redirectLocation \"/\\r\\nSet-Cookie: id=evil\"",
		gives("wire = HTTP/1.1 500 Internal Server Error\nwire header Content-Length = "
			+ std::to_string(builtInPage(500).size()) + "\nwire header Set-Cookie = (none)"), [] {
		Sandbox			box;
		Site			site(defaultConfig());
		HttpRequest		req = request("GET /old/");
		RouteResult		route = site.route(RouteAction::Redirect, req, "/old/");

		route.redirectLocation = "/\r\nSet-Cookie: id=evil";
		HttpResponse	response = buildResponse(req, route);

		return (skeletonFacts(response, req, {"wire", "wire header Content-Length", "wire header Set-Cookie"}));
	});
}
