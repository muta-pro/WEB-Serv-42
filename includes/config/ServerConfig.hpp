#pragma once

#include <cstddef>
#include <map>
#include <string>
#include <vector>

#include "LocationConfig.hpp"

/*
one interface:port pair from a `listen` directive.

	ordered so ServerManager can use it as a std::map key. this matters:
	two server blocks may legally name the same host:port (that is how
	name-based virtual hosts work), and bind() on an already-bound pair
	fails. the pairs must be de-duplicated into ONE listening socket that
	several ServerConfigs sit behind.
*/
struct Listen {
	std::string	host = "0.0.0.0";
	int			port = 8080;

	bool	operator<(const Listen &other) const;
	bool	operator==(const Listen &other) const;
};

/*
one server block. Ravi parses these once at startup; everyone else holds
const pointers and never mutates them.

	every field here is either required by subject IV.3 at server level
	(listen / error pages / client_max_body_size) or one deliberate,
	defensible extra. there is intentionally NO root, index or autoindex:
	those exist only as parser-internal defaults (see ServerDefaults in
	ConfigParser.hpp) and are copied into every LocationConfig at parse
	time. downstream code therefore cannot read a server-level root by
	accident, because there isn't one. (decision 5.)

	CHOOSING A SERVER FOR A CONNECTION (decision 3):
		1. group the parsed servers by the Listen they were accepted on
		2. compare the request's Host header against serverNames with
		   matchesHost() below
		3. no match -> the FIRST block declared for that Listen wins. it is
		   the default server. an unknown Host is never an error.
	a missing Host header on HTTP/1.1 is a different matter - that is a 400
	from Liza's parser per RFC 9112 3.2, not a routing decision.
*/
struct ServerConfig {
	std::vector<Listen>			listens;		// at least one, parser enforces
	std::vector<std::string>	serverNames;	// matched against Host:, may be empty
	std::map<int, std::string>	errorPages;		// status code -> URI (see below)
	size_t						clientMaxBody = 1048576;	// 1 MiB
	std::vector<LocationConfig>	locations;		// declaration order preserved

	/*
	returns "" when no custom page is configured for this code; the caller
	falls back to HttpResponse::makeError()'s built-in generated page.

	the returned string is a URI, NOT a filesystem path (nginx-faithful).
	/errors/404.html is re-resolved through the normal location matching ->
	resolvePath() pipeline, exactly like any other request. that is why the
	example configs carry a `location /errors/` block: it is what turns the
	URI back into a file on disk.

	two consequences the response builder owns:
	  - RECURSION GUARD. serving an error page can itself fail (missing file,
	    405, another error_page pointing at it). handle an error page with a
	    flag already raised -> stop, emit the built-in page. one bool.
	  - `internal`. a location marked internalOnly is reachable only through
	    this re-entry, never from a client's own request line. a direct hit
	    on /errors/404.html is a 404, as in nginx.

	deliberately NOT supported: nginx's `=200` status override and external
	`http://` redirect targets. neither is needed and both add parser states.

	error pages live at server level ONLY - no per-location override
	(decision 6). errors happen before a location is ever matched (a 400 on a
	malformed request line), so the lookup has to work off the server anyway.
	*/
	std::string	errorPageUri(int code) const;

	/*
	true when hostHeader names this server. the port is stripped before
	comparing ("example.com:8080" -> "example.com") and the compare is
	case-insensitive, since DNS names are. a server with no serverNames
	matches nothing here and can only be reached as the default server.
	*/
	bool		matchesHost(const std::string &hostHeader) const;
};
