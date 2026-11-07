#pragma once

#include "../framework/Tester.hpp"
#include "../parser_under_test.hpp"

/*
STEP 6 - the remaining server-level directives.
	error_page lands in the ServerConfig, so parseString() shows it. the
	others only fill ServerDefaults, which nothing outside parseServer can
	see until step 8 copies them into the locations - so those tests call
	parseServerDirective() directly and read the defaults back.
*/

/* runs every directive in `text` through parseServerDirective(), as parseServer would */
inline ServerDefaults	defaultsAfter(const std::string &text)
{
	std::vector<Token>	tokens = tokenize(text);
	Cursor				cur{tokens, 0};
	ServerConfig		server;
	ServerDefaults		defaults;

	while (!cur.atEnd())
	{
		const Token &name = cur.next();
		parseServerDirective(cur, name, server, defaults);
	}
	return (defaults);
}

/* one field of ServerDefaults, as text */
inline std::string	defaultsField(const ServerDefaults &defaults, const std::string &field)
{
	if (field == "root")
		return (defaults.root);
	if (field == "index")
		return (defaults.index);
	if (field == "autoindex")
		return (defaults.autoindex ? "on" : "off");
	if (field == "methods")
		return (methodsText(defaults.allowedMethods));
	if (field == "body")
		return (std::to_string(defaults.clientMaxBody));
	return ("(no field '" + field + "' - see defaultsField)");
}

inline void	testServer(Tester &t)
{
	/* the defaults: what the directives write into ServerDefaults */
	auto	defaults = [&t](const std::string &field, const std::string &text, const Expect &expect) {
		t.test(field + " after: " + text, text, expect, [&] {
			return (defaultsField(defaultsAfter(text), field));
		});
	};

	defaults("root", "listen 80;", gives("./www"));		// untouched without a root line
	defaults("root", "root /srv/www;", gives("/srv/www"));
	defaults("root", "root ./www/html;", gives("./www/html"));
	defaults("root", "root ./a; root ./b;", gives("./b"));
	defaults("index", "index home.html;", gives("home.html"));
	defaults("index", "index a.html; index b.html;", gives("b.html"));
	defaults("autoindex", "autoindex on;", gives("on"));
	defaults("autoindex", "autoindex OFF;", gives("off"));
	defaults("autoindex", "autoindex on; autoindex off;", gives("off"));
	defaults("autoindex", "autoindex maybe;", fails(1, "expected 'on' or 'off', not 'maybe'"));
	defaults("methods", "methods GET POST;", gives("GET POST"));	// HEAD only comes with step 8
	defaults("methods", "methods DELETE;", gives("DELETE"));
	defaults("methods", "methods GET; methods POST;", gives("POST"));	// replaced, not merged
	defaults("methods", "methods GET PUT;", fails(1, "unknown method 'PUT'"));
	defaults("body", "client_max_body_size 10M;", gives("10485760"));
	defaults("body", "client_max_body_size 512k;", gives("524288"));
	defaults("body", "client_max_body_size 1M; client_max_body_size 4M;", gives("4194304"));
	defaults("body", "client_max_body_size 10X;", fails(1, "unknown size suffix in '10X'"));
	defaults("body", "client_max_body_size 0;", fails(1, "size 0 is not allowed in '0'"));	// open decision 7

	/* the number of arguments */
	t.rejects("root without a path", "server { root; }", 1, "invalid number of arguments in 'root'");
	t.rejects("root with two paths", "server { root ./a ./b; }", 1, "invalid number of arguments in 'root'");
	t.rejects("index with two files (one per location)", "server { index a.html b.html; }", 1,
		"invalid number of arguments in 'index'");
	t.rejects("autoindex without a value", "server { autoindex; }", 1, "invalid number of arguments in 'autoindex'");
	t.rejects("autoindex with two values", "server { autoindex on off; }", 1,
		"invalid number of arguments in 'autoindex'");
	t.rejects("methods without a method", "server { methods; }", 1, "invalid number of arguments in 'methods'");
	t.rejects("client_max_body_size without a size", "server { client_max_body_size; }", 1,
		"invalid number of arguments in 'client_max_body_size'");
	t.rejects("client_max_body_size with two sizes", "server { client_max_body_size 1M 2M; }", 1,
		"invalid number of arguments in 'client_max_body_size'");
	t.rejects("error_page without anything", "server { error_page; }", 1,
		"invalid number of arguments in 'error_page'");
	t.rejects("error_page without a code", "server { error_page /404.html; }", 1,
		"invalid number of arguments in 'error_page'");

	/* error_page: codes 300-599, then one URI */
	t.parses("error_page binds one code", "server { listen 80; error_page 404 /404.html; }",
		{"s0 errors = 404=/404.html"});
	t.parses("error_page binds several codes at once", "server { listen 80; error_page 500 502 503 504 /50x.html; }",
		{"s0 errors = 500=/50x.html 502=/50x.html 503=/50x.html 504=/50x.html"});
	t.parses("error_page lines add up", "server {\n\tlisten 80;\n\terror_page 500 /50x.html;\n\terror_page 404 /404.html;\n}",
		{"s0 errors = 404=/404.html 500=/50x.html"});
	t.parses("a code bound twice: the last one wins", "server { listen 80; error_page 404 /a.html; error_page 404 /b.html; }",
		{"s0 errors = 404=/b.html"});
	t.parses("one code of a group rebound", "server { listen 80; error_page 500 502 /50x.html; error_page 502 /502.html; }",
		{"s0 errors = 500=/50x.html 502=/502.html"});
	t.parses("error_page codes 300 and 599 are the limits", "server { listen 80; error_page 300 599 /edge.html; }",
		{"s0 errors = 300=/edge.html 599=/edge.html"});
	t.parses("the URI may be just '/'", "server { listen 80; error_page 404 /; }", {"s0 errors = 404=/"});
	t.parses("no error_page: no errors", "server { listen 80; }", {"s0 errors = (none)"});
	t.rejects("an error_page code below 300", "server { error_page 200 /ok.html; }", 1,
		"status code '200' is not between 300 and 599");
	t.rejects("an error_page code above 599", "server { error_page 600 /x.html; }", 1,
		"status code '600' is not between 300 and 599");
	t.rejects("an error_page code that isn't a number", "server { error_page 40x /x.html; }", 1,
		"invalid number in '40x'");
	t.rejects("an error_page target that is a path, not a URI", "server { error_page 404 404.html; }", 1,
		"expected a URI starting with '/', not '404.html'");
	t.rejects("an error_page target relative to the server", "server { error_page 404 errors/404.html; }", 1,
		"expected a URI starting with '/', not 'errors/404.html'");
	t.rejects("an error_page with codes only", "server { error_page 404 500; }", 1,
		"expected a URI starting with '/', not '500'");
	t.rejects("a bad URI is reported on its own line", "server {\n\terror_page 404\n\t\t404.html;\n}", 3,
		"expected a URI starting with '/', not '404.html'");
	t.rejects("a bad code is reported on its own line", "server {\n\terror_page 404\n\t\t200\n\t\t/x.html;\n}", 3,
		"status code '200' is not between 300 and 599");

	/* through parseString */
	t.parses("every server-level directive is accepted",
		"server {\n\tlisten 80;\n\tserver_name a.test;\n\tclient_max_body_size 1M;\n\troot ./www/html;\n"
		"\tindex index.html;\n\tautoindex off;\n\tmethods GET;\n\terror_page 404 /404.html;\n\tlocation / { }\n}",
		{"servers = 1", "s0 listen = 0.0.0.0:80", "s0 errors = 404=/404.html"});
	t.parses("server-level directives after a location", "server { listen 80; location / { } root ./late; error_page 404 /x; }",
		{"servers = 1", "s0 errors = 404=/x"});
	t.parses("client_max_body_size reaches s0 body through the / location (step 8, §9)",
		"server { listen 80; client_max_body_size 10M; }", {"s0 body = 10485760"});
	t.rejects("a bad value is reported on its own line", "server {\n\tlisten 80;\n\tautoindex yes;\n}", 3,
		"expected 'on' or 'off', not 'yes'");
	t.rejects("a missing ';' after a server-level directive", "server { root ./www }", 1, "expected ';' before '}'");

	/* the shipped files that only needed step 6 */
	t.parsesFile("conf/default.conf", {"servers = 1", "s0 listen = 0.0.0.0:8080", "s0 names = localhost",
		"s0 errors = 403=/errors/403.html 404=/errors/404.html 500=/errors/50x.html 502=/errors/50x.html "
		"504=/errors/50x.html"});
	t.parsesFile("conf/edge_cases.conf", {"servers = 2", "s0 names = edge.test",
		"s0 errors = 400=/errors/generic.html 403=/errors/generic.html 404=/errors/generic.html "
		"405=/errors/generic.html 413=/errors/generic.html 500=/errors/generic.html 502=/errors/generic.html "
		"504=/errors/generic.html", "s1 listen = 127.0.0.1:9090"});
}
