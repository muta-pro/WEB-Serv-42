#pragma once

#include "../framework/Tester.hpp"
#include "../parser_under_test.hpp"

/*
STEP 7 - location blocks, read into a RawLocation.
	the locations only reach ServerConfig::locations in step 8, so most of
	these tests call parseLocation() directly and read the RawLocation
	back. the inheritable fields show "(unset)" while their optional is
	empty: that is the state step 8 fills from the server defaults.
*/

/* one location block, as parseServer would read it: `text` starts at the path */
inline RawLocation	locationAfter(const std::string &text)
{
	std::vector<Token>			tokens = tokenize(text);
	Cursor						cur{tokens, 0};
	std::vector<RawLocation>	noSiblings;

	return (parseLocation(cur, noSiblings));
}

/* one field of a RawLocation, as text */
inline std::string	locationField(const RawLocation &raw, const std::string &field)
{
	const LocationConfig	&own = raw.own;

	if (field == "path")
		return (own.path);
	if (field == "root")
		return (raw.root ? *raw.root : "(unset)");
	if (field == "index")
		return (raw.index ? *raw.index : "(unset)");
	if (field == "autoindex")
		return (raw.autoindex ? (*raw.autoindex ? "on" : "off") : "(unset)");
	if (field == "methods")
		return (raw.methods ? methodsText(*raw.methods) : "(unset)");
	if (field == "body")
		return (raw.clientMaxBody ? std::to_string(*raw.clientMaxBody) : "(unset)");
	if (field == "upload")
		return (own.uploadStore.empty() ? "(none)" : own.uploadStore);
	if (field == "cgi")
		return (own.cgiExt.empty() ? "(none)" : own.cgiExt + " " + own.cgiPath);
	if (field == "return")
		return (own.redirect.empty() ? "(none)" : std::to_string(own.redirectCode) + " " + own.redirect);
	if (field == "internal")
		return (own.internalOnly ? "yes" : "no");
	return ("(no field '" + field + "' - see locationField)");
}

inline void	testLocations(Tester &t)
{
	auto	field = [&t](const std::string &name, const std::string &text, const Expect &expect) {
		t.test(name + " of: location " + text, text, expect, [&] {
			return (locationField(locationAfter(text), name));
		});
	};

	/* an empty block: nothing set, so step 8 will inherit everything */
	field("path", "/ { }", gives("/"));
	field("root", "/ { }", gives("(unset)"));
	field("index", "/ { }", gives("(unset)"));
	field("autoindex", "/ { }", gives("(unset)"));
	field("methods", "/ { }", gives("(unset)"));
	field("body", "/ { }", gives("(unset)"));
	field("upload", "/ { }", gives("(none)"));
	field("cgi", "/ { }", gives("(none)"));
	field("return", "/ { }", gives("(none)"));
	field("internal", "/ { }", gives("no"));

	/* the inheritable fields: set means "the block said so" */
	field("root", "/a/ { root ./www/a; }", gives("./www/a"));
	field("root", "/a/ { root ./x; root ./y; }", gives("./y"));
	field("index", "/a/ { index home.html; }", gives("home.html"));
	field("autoindex", "/a/ { autoindex on; }", gives("on"));
	field("autoindex", "/a/ { autoindex off; }", gives("off"));	// set to false - NOT unset, must not inherit
	field("autoindex", "/a/ { autoindex maybe; }", fails(1, "expected 'on' or 'off', not 'maybe'"));
	field("methods", "/a/ { methods GET POST DELETE; }", gives("GET POST DELETE"));
	field("methods", "/a/ { methods GET; methods POST; }", gives("POST"));	// replaced, not merged
	field("methods", "/a/ { methods PATCH; }", fails(1, "unknown method 'PATCH'"));
	field("body", "/a/ { client_max_body_size 10M; }", gives("10485760"));
	field("body", "/a/ { client_max_body_size 10X; }", fails(1, "unknown size suffix in '10X'"));
	field("body", "/a/ { client_max_body_size 0; }", fails(1, "size 0 is not allowed in '0'"));	// decision 7
	field("root", "/a/ {\n\troot ./x;\n\tautoindex on;\n}", gives("./x"));	// one field doesn't touch the others
	field("index", "/a/ {\n\troot ./x;\n\tautoindex on;\n}", gives("(unset)"));

	/* the location-only fields */
	field("upload", "/up/ { upload_store ./www/uploads; }", gives("./www/uploads"));
	field("upload", "/up/ { upload_store ./a; upload_store ./b; }", gives("./b"));
	field("cgi", "/cgi/ { cgi .py /usr/bin/python3; }", gives(".py /usr/bin/python3"));
	field("cgi", "/cgi/ { cgi .py /a; cgi .php /b; }", gives(".php /b"));	// one mapping per location
	field("cgi", "/cgi/ { cgi py /usr/bin/python3; }", fails(1, "expected an extension starting with '.', not 'py'"));
	field("return", "/old/ { return 301 /; }", gives("301 /"));
	field("return", "/old/ { return 302 http://example.com/new; }", gives("302 http://example.com/new"));
	field("return", "/old/ { return 300 /a; }", gives("300 /a"));
	field("return", "/old/ { return 399 /a; }", gives("399 /a"));
	field("return", "/old/ { return 200 /a; }", fails(1, "status code '200' is not between 300 and 399"));
	field("return", "/old/ { return 404 /a; }", fails(1, "status code '404' is not between 300 and 399"));
	field("return", "/old/ { return 30x /a; }", fails(1, "invalid number in '30x'"));
	field("internal", "/errors/ { internal; }", gives("yes"));
	field("internal", "/errors/ { internal; root ./www/errors; }", gives("yes"));

	/* the number of arguments */
	t.rejects("upload_store without a path", "server { location / { upload_store; } }", 1,
		"invalid number of arguments in 'upload_store'");
	t.rejects("cgi with only an extension", "server { location / { cgi .py; } }", 1,
		"invalid number of arguments in 'cgi'");
	t.rejects("cgi with three arguments", "server { location / { cgi .py /usr/bin/python3 -u; } }", 1,
		"invalid number of arguments in 'cgi'");
	t.rejects("return with only a code", "server { location / { return 301; } }", 1,
		"invalid number of arguments in 'return'");
	t.rejects("return with three arguments", "server { location / { return 301 /a /b; } }", 1,
		"invalid number of arguments in 'return'");
	t.rejects("internal with an argument", "server { location / { internal on; } }", 1,
		"invalid number of arguments in 'internal'");
	t.rejects("index with two files in a location", "server { location / { index a.html b.html; } }", 1,
		"invalid number of arguments in 'index'");

	/* what may go where */
	t.rejects("listen inside a location", "server { location / { listen 80; } }", 1,
		"directive 'listen' is not allowed inside a location");
	t.rejects("server_name inside a location", "server { location / { server_name a.test; } }", 1,
		"directive 'server_name' is not allowed inside a location");
	t.rejects("error_page inside a location (server level only)", "server { location / { error_page 404 /x; } }", 1,
		"directive 'error_page' is not allowed inside a location");
	t.rejects("a nested location", "server { location / { location /a/ { } } }", 1,
		"directive 'location' is not allowed inside a location");
	t.rejects("a nested location, on its own line", "server {\n\tlocation / {\n\t\tlocation /a/ {\n\t\t}\n\t}\n}", 3,
		"directive 'location' is not allowed inside a location");
	t.rejects("an unknown directive inside a location", "server { location / { proxy_pass x; } }", 1,
		"unknown directive 'proxy_pass'");
	t.rejects("upload_store at server level", "server { upload_store ./up; }", 1,
		"directive 'upload_store' is only allowed inside a location");
	t.rejects("cgi at server level", "server { cgi .py /usr/bin/python3; }", 1,
		"directive 'cgi' is only allowed inside a location");
	t.rejects("return at server level", "server { return 301 /; }", 1,
		"directive 'return' is only allowed inside a location");
	t.rejects("internal at server level", "server { internal; }", 1,
		"directive 'internal' is only allowed inside a location");
	t.rejects("a missing ';' inside a location", "server { location / { root ./www } }", 1, "expected ';' before '}'");

	/* duplicate paths */
	t.rejects("two locations with the same path", "server { location /a/ { } location /a/ { } }", 1,
		"duplicate location '/a/'");
	t.rejects("a duplicate is reported on its own line",
		"server {\n\tlocation / { }\n\tlocation /a/ { }\n\tlocation / { }\n}", 4, "duplicate location '/'");
	t.parses("/a and /a/ are different paths", "server { listen 80; location /a { } location /a/ { } }", {"servers = 1"});
	t.parses("the same path in two servers is fine", "server { listen 80; location / { } } server { listen 81; location / { } }",
		{"servers = 2"});

	/* through parseString */
	t.parses("every location-level directive is accepted",
		"server {\n\tlisten 80;\n\tlocation /x/ {\n\t\troot ./www;\n\t\tindex i.html;\n\t\tautoindex on;\n"
		"\t\tmethods GET POST;\n\t\tclient_max_body_size 2M;\n\t\tupload_store ./up;\n\t\tcgi .py /usr/bin/python3;\n"
		"\t\treturn 301 /;\n\t\tinternal;\n\t}\n}",
		{"servers = 1", "s0 listen = 0.0.0.0:80"});

	/* the shipped files, now that their location blocks are read for real */
	t.parsesFile("conf/default.conf", {"servers = 1", "s0 listen = 0.0.0.0:8080"});
	t.parsesFile("conf/edge_cases.conf", {"servers = 2", "s1 listen = 127.0.0.1:9090"});
	t.parsesFile("conf/multi_server.conf", {"servers = 3"});
}
