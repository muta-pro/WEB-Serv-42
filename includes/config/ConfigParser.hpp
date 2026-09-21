#pragma once

#include <cstddef>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include "ServerConfig.hpp"

/*
thrown on any malformed or unusable config. the server refuses to start;
main() catches, prints what() and exits non-zero. carries the line number
because "expected ';'" without one is useless.
*/
class ConfigError : public std::runtime_error {
	public:
		ConfigError(size_t line, const std::string &message);

		size_t	line() const;

	private:
		size_t	_line;
};

/*
server-level values that exist ONLY to be copied into locations. parser
scratch - deliberately not part of ServerConfig, so no runtime code can
read a "server root" that was never meant to be read at runtime.
*/
struct ServerDefaults {
	std::string				root = "./www";
	std::string				index = "index.html";
	bool					autoindex = false;
	size_t					clientMaxBody = 1048576;
	std::set<std::string>	allowedMethods = {"GET", "HEAD"};
};

/*
reads a .conf and hands back FULLY RESOLVED servers (decision 5). nothing
downstream ever resolves inheritance, walks to a parent, or tests a field
for a sentinel.

	SYNTAX - nginx-flavoured, since the subject points at nginx:
		#           comment, runs to end of line
		{ }         block delimiters, self-delimiting tokens
		;           terminates every simple directive
		1M 512k 1G  size suffixes on client_max_body_size (k/K m/M g/G)
	the tokenizer is whitespace-splitting with '{', '}' and ';' as
	single-character tokens. that is the whole lexer.

	DIRECTIVES
		server level    listen, server_name, client_max_body_size,
		                error_page, root, index, autoindex, methods, location
		location level  root, index, autoindex, methods, client_max_body_size,
		                upload_store, cgi, return, internal
	root / index / autoindex / methods / client_max_body_size at server level
	are defaults for the locations under it, nothing more.

		error_page 500 502 503 504 /50x.html;
	one directive may bind several codes: every token but the last is a
	status code, the last is the URI.

	GUARANTEES on the returned vector - assume all of these downstream:
	  1. at least one server, each with at least one Listen
	  2. every server has at least one location. if the file declares no
	     "/" block, the parser SYNTHESISES one from the server defaults, so
	     "no location matched" is not a state the router can reach and there
	     is no fallback rule to remember
	  3. every LocationConfig field holds its final value - no sentinels
	  4. HEAD is present wherever GET is (LocationConfig::grantImplicitHead)
	  5. every error_page value is a URI beginning with '/', never a
	     filesystem path - validated here so the mistake cannot survive
	     startup
	  6. no location has an empty root
	  7. locations are kept in declaration order
*/
class ConfigParser {
	public:
		static std::vector<ServerConfig>	parseFile(const std::string &path);
		static std::vector<ServerConfig>	parseString(const std::string &text);
};
