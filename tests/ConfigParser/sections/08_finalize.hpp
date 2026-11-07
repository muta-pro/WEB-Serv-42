#pragma once

#include "../framework/Tester.hpp"
#include "../parser_under_test.hpp"

/*
STEP 8 - finalize: the RawLocations become LocationConfigs, and the parser
keeps the 7 guarantees of ConfigParser.hpp. from here on the locations are
in ServerConfig, so every test goes through parseString() and reads facts
like "s0 /a/ root = ./www" (./tester --facts FILE shows them all).
*/

inline void	testFinalize(Tester &t)
{
	/* value_or: the block's own value, or the server's default */
	t.parses("an empty location inherits every server default",
		"server { listen 80; root ./www/html; index home.html; autoindex on; methods GET POST;"
		" client_max_body_size 2M; location /a/ { } }",
		{"s0 /a/ root = ./www/html", "s0 /a/ index = home.html", "s0 /a/ autoindex = on",
		"s0 /a/ methods = GET HEAD POST", "s0 /a/ body = 2097152"});
	t.parses("no server-level lines: the built-in defaults", "server { listen 80; location /a/ { } }",
		{"s0 /a/ root = ./www", "s0 /a/ index = index.html", "s0 /a/ autoindex = off",
		"s0 /a/ methods = GET HEAD", "s0 /a/ body = 1048576"});
	t.parses("a location's own values beat the defaults",
		"server { listen 80; root ./www; index a.html; client_max_body_size 1M;"
		" location /a/ { root ./a; index b.html; client_max_body_size 2k; } }",
		{"s0 /a/ root = ./a", "s0 /a/ index = b.html", "s0 /a/ body = 2048"});
	t.parses("autoindex off in a location beats autoindex on at server level (the optional at work)",
		"server { listen 80; autoindex on; location /a/ { autoindex off; } location /b/ { } }",
		{"s0 /a/ autoindex = off", "s0 /b/ autoindex = on"});
	t.parses("a default written after the location still applies (handover §5.2)",
		"server { listen 80; location /a/ { } root ./late; }", {"s0 /a/ root = ./late"});
	t.parses("each location inherits on its own",
		"server { listen 80; root ./www; location /a/ { root ./a; } location /b/ { } }",
		{"s0 /a/ root = ./a", "s0 /b/ root = ./www"});
	t.parses("a location's methods replace the default, they don't merge",
		"server { listen 80; methods GET POST DELETE; location /a/ { methods POST; } }",
		{"s0 /a/ methods = POST"});
	t.parses("the location-only fields come through from raw.own",
		"server { listen 80; location /c/ { cgi .py /usr/bin/python3; upload_store ./up; return 302 /x; internal; } }",
		{"s0 /c/ cgi = .py /usr/bin/python3", "s0 /c/ upload = ./up", "s0 /c/ return = 302 /x",
		"s0 /c/ internal = yes"});

	/* guarantee 4: HEAD wherever GET is */
	t.parses("GET in a location brings HEAD", "server { listen 80; location /a/ { methods GET POST; } }",
		{"s0 /a/ methods = GET HEAD POST"});
	t.parses("an inherited GET brings HEAD too", "server { listen 80; methods GET DELETE; location /a/ { } }",
		{"s0 /a/ methods = GET HEAD DELETE"});
	t.parses("no GET, no HEAD", "server { listen 80; location /a/ { methods POST DELETE; } }",
		{"s0 /a/ methods = POST DELETE"});
	t.parses("HEAD on its own is allowed (decision 5)", "server { listen 80; location /a/ { methods HEAD; } }",
		{"s0 /a/ methods = HEAD"});

	/* guarantees 2 and 7: a "/" always exists, the file's order is kept */
	t.parses("no locations: a / is made from the defaults", "server { listen 80; root ./www/html; autoindex on; }",
		{"s0 locations = /", "s0 / root = ./www/html", "s0 / autoindex = on", "s0 / methods = GET HEAD",
		"s0 / internal = no"});
	t.parses("no / among the locations: one is added after them", "server { listen 80; location /a/ { } location /b/ { } }",
		{"s0 locations = /a/ /b/ /"});
	t.parses("a declared / stays where it is and isn't added twice",
		"server { listen 80; location /a/ { } location / { root ./r; } location /b/ { } }",
		{"s0 locations = /a/ / /b/", "s0 / root = ./r"});
	t.parses("/a is not /, so / is still added", "server { listen 80; location /a { } }",
		{"s0 locations = /a /"});
	t.parses("each server gets its own /", "server { listen 80; location /a/ { } } server { listen 81; }",
		{"s0 locations = /a/ /", "s1 locations = /"});

	/* §9, B: the server's body limit is the largest of its locations */
	t.parses("no client_max_body_size anywhere: 1M", "server { listen 80; }", {"s0 body = 1048576"});
	t.parses("a bigger location raises the server's limit",
		"server { listen 80; client_max_body_size 1M; location /up/ { client_max_body_size 10M; } }",
		{"s0 body = 10485760", "s0 /up/ body = 10485760", "s0 / body = 1048576"});
	t.parses("the added / counts: a small location doesn't lower the limit",
		"server { listen 80; client_max_body_size 1M; location /small/ { client_max_body_size 1k; } }",
		{"s0 body = 1048576"});
	t.parses("the server-level size only counts through locations that inherit it",
		"server { listen 80; client_max_body_size 5M; location / { client_max_body_size 2k; }"
		" location /a/ { client_max_body_size 1k; } }",
		{"s0 body = 2048"});

	/* decision 1: every server needs a listen (guarantee 1) */
	t.rejects("a server without listen", "server { }", 1, "server has no 'listen'");
	t.rejects("a server with everything but listen", "server {\n\tserver_name a.test;\n\tlocation / { }\n}", 1,
		"server has no 'listen'");
	t.rejects("reported on the line of the server keyword", "server\n{\n\troot ./www;\n}", 1, "server has no 'listen'");
	t.rejects("the second server without listen", "server { listen 80; }\n\nserver {\n\troot ./www;\n}", 3,
		"server has no 'listen'");

	/* decision 4: one name per host:port */
	t.rejects("the same name on the same listen",
		"server { listen 80; server_name a.test; }\nserver { listen 80; server_name a.test; }", 2,
		"conflicting server_name 'a.test' on 0.0.0.0:80");
	t.rejects("names compare case-insensitively, the way matchesHost does",
		"server { listen 80; server_name a.test; }\nserver { listen 80; server_name A.TEST; }", 2,
		"conflicting server_name 'A.TEST' on 0.0.0.0:80");
	t.rejects("one shared name among several",
		"server { listen 80; server_name a.test b.test; }\nserver { listen 80; server_name c.test b.test; }", 2,
		"conflicting server_name 'b.test' on 0.0.0.0:80");
	t.rejects("one shared listen among several",
		"server { listen 80; listen 81; server_name a.test; }\nserver { listen 82; listen 81; server_name a.test; }", 2,
		"conflicting server_name 'a.test' on 0.0.0.0:81");
	t.rejects("a conflict with a server further back",
		"server { listen 80; server_name a.test; }\nserver { listen 80; server_name b.test; }\n"
		"server {\n\tlisten 80;\n\tserver_name a.test;\n}", 3, "conflicting server_name 'a.test' on 0.0.0.0:80");
	t.parses("different names on one listen: virtual hosts",
		"server { listen 80; server_name a.test; } server { listen 80; server_name b.test; }", {"servers = 2"});
	t.parses("the same name on different ports", "server { listen 80; server_name a.test; } server { listen 81; server_name a.test; }",
		{"servers = 2"});
	t.parses("a server may repeat its own name", "server { listen 80; server_name a.test A.test; }",
		{"s0 names = a.test A.test"});

	/* decision 8: a server without server_name must be the default on one of its listens */
	const std::string	unreachable = "server without server_name can never be reached: each of its listens already has a default server";
	t.rejects("two servers without names on one listen", "server { listen 80; }\nserver { listen 80; }", 2, unreachable);
	t.rejects("a nameless server after a named one", "server { listen 80; server_name a.test; }\nserver { listen 80; }", 2,
		unreachable);
	t.rejects("a nameless server whose every listen is taken",
		"server { listen 80; }\nserver { listen 81; server_name b.test; }\nserver {\n\tlisten 80;\n\tlisten 81;\n}", 3, unreachable);
	t.rejects("taken by a server further back",
		"server { listen 80; }\nserver { listen 81; }\nserver { listen 80; server_name c.test; }\nserver { listen 80; }", 4, unreachable);
	t.parses("nameless, but the default on one of its listens",
		"server { listen 80; server_name a.test; }\nserver { listen 80; listen 81; }", {"servers = 2"});
	t.parses("a nameless server first, then a named one", "server { listen 80; }\nserver { listen 80; server_name a.test; }",
		{"servers = 2"});
	t.parses("nameless servers on different ports", "server { listen 80; }\nserver { listen 81; }", {"servers = 2"});
	t.parses("a nameless server may repeat its own listen", "server { listen 80; listen 80; }",
		{"servers = 1", "s0 listen = 0.0.0.0:80 0.0.0.0:80"});

	/* guarantee 1: at least one server */
	t.rejects("an empty file", "", 1, "no server block");
	t.rejects("a file with only comments", "# nothing\n# here\n", 1, "no server block");

	/* the shipped files, fully resolved */
	t.parsesFile("conf/default.conf", {"servers = 1", "s0 body = 10485760",
		"s0 locations = / /errors/ /files/ /upload/ /cgi-bin/ /old/",
		"s0 / root = ./www/html", "s0 / methods = GET HEAD", "s0 / body = 1048576",
		"s0 /errors/ root = ./www/errors", "s0 /errors/ internal = yes",
		"s0 /files/ autoindex = on", "s0 /files/ index = index.html",
		"s0 /upload/ methods = GET HEAD POST DELETE", "s0 /upload/ body = 10485760",
		"s0 /upload/ upload = ./www/uploads",
		"s0 /cgi-bin/ cgi = .py /usr/bin/python3", "s0 /cgi-bin/ methods = GET HEAD POST",
		"s0 /old/ return = 301 /", "s0 /old/ root = ./www/html"});
	t.parsesFile("conf/edge_cases.conf", {"servers = 2", "s1 locations = /", "s1 / methods = GET HEAD"});
	t.parsesFile("conf/multi_server.conf", {"servers = 3"});
}
