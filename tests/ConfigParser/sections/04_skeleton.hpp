#pragma once

#include "../framework/Tester.hpp"
#include "../parser_under_test.hpp"

/*
STEP 4 - the top level and the server skeleton, through parseString().
	the top level only takes "server {"; inside a server, directives are
	dispatched until its '}'. a location block is only walked over until
	step 7 keeps it, so no test here looks inside one.
*/

/* `count` small servers, one after another */
inline std::string	manyServers(size_t count)
{
	std::string	text;

	for (size_t i = 0; i < count; i++)
		text += "server {\n\tlisten " + std::to_string(1024 + i % 60000) + ";\n\tserver_name s"
			+ std::to_string(i) + ".test;\n\tlocation / { }\n}\n";
	return (text);
}

/* one server with `count` locations, all on one line */
inline std::string	manyLocations(size_t count)
{
	std::string	text = "server { listen 80;";

	for (size_t i = 0; i < count; i++)
		text += " location /l" + std::to_string(i) + "/ { internal; }";
	return (text + " }");
}

inline void	testSkeleton(Tester &t)
{
	/* what parses */
	t.parses("one server with a listen", "server { listen 80; }",
		{"servers = 1", "s0 listen = 0.0.0.0:80"});
	t.parses("server_name keeps every name, in order",
		"server {\n\tlisten 8080;\n\tserver_name a.test b.test c.test;\n}",
		{"s0 names = a.test b.test c.test"});
	t.rejects("an empty server block has no listen (step 8, decision 1)", "server {}", 1, "server has no 'listen'");
	t.parses("two servers, in file order",
		"server {\n\tlisten 80;\n\tserver_name first;\n}\nserver {\n\tlisten 81;\n\tserver_name second;\n}\n",
		{"servers = 2", "s0 names = first", "s1 names = second"});
	t.parses("two servers glued together", "server{listen 80;}server{listen 81;}",
		{"servers = 2", "s0 listen = 0.0.0.0:80", "s1 listen = 0.0.0.0:81"});
	t.parses("a server spread over many lines", "server\n{\nlisten\n80\n;\n}\n",
		{"servers = 1", "s0 listen = 0.0.0.0:80"});
	t.parses("directives after a location block still count",
		"server {\n\tlisten 80;\n\tlocation / {\n\t}\n\tlocation /upload/ {\n\t\troot ./www;\n\t\tmethods GET POST;\n"
		"\t\tinternal;\n\t}\n\tserver_name after.locations;\n}\n",
		{"servers = 1", "s0 names = after.locations"});
	t.parses("comments everywhere", "# top\nserver { # a\n\tlisten 80;# b\n\tlocation / { # c\n\t}\n} # d",
		{"servers = 1", "s0 listen = 0.0.0.0:80"});
	t.parses("CRLF line endings", "server {\r\n\tlisten 80;\r\n\tserver_name crlf.test;\r\n}\r\n",
		{"s0 names = crlf.test"});
	t.rejects("an empty file has no servers (step 8 rejects it)", "", 1, "no server block");
	t.rejects("only comments: no servers (step 8 rejects it)", "# nothing\n# here\n", 1, "no server block");

	/* the top level */
	t.rejects("a '}' after the last server", "server {\n\tlisten 80;\n}\n}\n", 4, "unexpected '}'");
	t.rejects("a lone '}'", "}", 1, "unexpected '}'");
	t.rejects("a directive before any server", "listen 80;\nserver {}", 1,
		"directive 'listen' is outside any block");
	t.rejects("a location outside a server", "location / {}", 1, "directive 'location' is outside any block");
	t.rejects("a typo in 'server'", "sever {\n}", 1, "directive 'sever' is outside any block");
	t.rejects("keywords are case-sensitive", "SERVER {}", 1, "directive 'SERVER' is outside any block");
	t.rejects("a stray ';' at the top level", "server { listen 80; }\n;", 2, "directive ';' is outside any block");
	t.rejects("'server' without '{'", "server listen 80; }", 1, "expected '{' before 'listen'");
	t.rejects("'server' at the end of the file", "server { listen 80; }\nserver", 2, "unexpected end of file");

	/* inside a server */
	t.rejects("a server that is never closed", "server {\n\tlisten 80;\n", 2, "unexpected end of file");
	t.rejects("a location that is never closed", "server {\n\tlocation / {\n\t\tinternal;\n", 3,
		"unexpected end of file");
	t.rejects("an unknown directive", "server {\n\tlisten 80;\n\tbogus on;\n}", 3, "unknown directive 'bogus'");
	t.rejects("directive names are case-sensitive", "server { Listen 80; }", 1, "unknown directive 'Listen'");
	t.rejects("a server inside a server", "server { server { } }", 1, "unknown directive 'server'");
	t.rejects("a ';' where a directive should be", "server { listen 80; ; }", 1, "unknown directive ';'");
	t.rejects("a missing ';' before '}'", "server { listen 80 }", 1, "expected ';' before '}'");
	t.rejects("listen without a value", "server { listen; }", 1, "invalid number of arguments in 'listen'");
	t.rejects("listen with two values", "server { listen 80 81; }", 1, "invalid number of arguments in 'listen'");
	t.rejects("server_name without a name", "server { server_name; }", 1,
		"invalid number of arguments in 'server_name'");
	t.rejects("the error is on its own line", "server {\n\n\n\n\tlisten 80;\n\tbogus x;\n}", 6,
		"unknown directive 'bogus'");
	t.rejects("an error in the second server", "server {\n\tlisten 80;\n}\nserver {\n\tlisten 81;\n\tlisten;\n}", 6,
		"invalid number of arguments in 'listen'");

	/* the location header - the block itself is step 7 */
	t.rejects("location without a path", "server { location { } }", 1, "expected a path after 'location'");
	t.rejects("location followed by ';'", "server { location ; }", 1, "expected a path after 'location'");
	t.rejects("location followed by '}'", "server { location }", 1, "expected a path after 'location'");
	t.rejects("a location path must start with '/'", "server { location errors/ { } }", 1,
		"expected a path after 'location'");
	t.rejects("location with two paths", "server { location /a /b { } }", 1, "expected '{' before '/b'");
	t.rejects("location at the end of the file", "server {\n\tlocation", 2, "unexpected end of file");

	/* size */
	t.parses("20,000 servers (stress)", manyServers(20000),
		{"servers = 20000", "s19999 names = s19999.test", "s19999 listen = 0.0.0.0:21023"});
	t.parses("3,000 locations on one line (stress)", manyLocations(3000),
		{"servers = 1", "s0 listen = 0.0.0.0:80"});

	/* the shipped broken file */
	t.rejectsFile("conf/invalid_syntax.conf", 28, "expected ';' before 'server_name'");
}
