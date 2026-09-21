#pragma once

#include <cstddef>
#include <set>
#include <string>

/*
one location block, FULLY RESOLVED by ConfigParser at startup.

	"fully resolved" is the load-bearing word. every field below already holds
	its final value by the time main() returns from parsing. there is no
	"0 means inherit from the server", no sentinel, no pointer back to the
	parent. if you are holding a LocationConfig you have everything you need.
	the parser copies server-level defaults down; nobody walks upwards at
	runtime. (decision 5 - parse-time inheritance.)

	held by const pointer inside RouteResult. immutable after startup.
	location MATCHING lives in Liza's router, not here - this struct only
	answers questions about its own data.
*/
struct LocationConfig {
	std::string				path;			// URI prefix this block claims, e.g. "/cgi-bin/"
	std::string				root;			// filesystem base - ALIAS semantics, see resolvePath()
	std::set<std::string>	allowedMethods = {"GET", "HEAD"};
	std::string				index = "index.html";	// tried when target resolves to a directory
	bool					autoindex = false;		// listing when index is missing
	std::string				cgiExt;			// ".py" - empty means no CGI in this block
	std::string				cgiPath;		// execve target, e.g. "/usr/bin/python3"
	std::string				uploadStore;	// empty means uploads are REFUSED here (decision 1)
	std::string				redirect;		// empty means this block does not redirect
	int						redirectCode = 301;
	size_t					clientMaxBody = 1048576;	// already resolved, never "inherit"
	bool					internalOnly = false;		// nginx `internal;` - see ServerConfig errorPages

	bool	allowsMethod(const std::string &method) const;
	bool	isCGI() const;
	bool	isRedirect() const;
	bool	acceptsUploads() const;

	/*
	RFC 9110 9.3.2 - HEAD is GET with the body suppressed, so it is not a
	separate permission. nginx couples them the same way in limit_except.
	the parser calls this once after reading an explicit `methods` line so
	the router never has to special-case HEAD: it just looks in the set.

	body suppression is NOT a config concern. build the response exactly as
	if it were a GET - Content-Length included and correct - then drop the
	body bytes in toBytes(). never recompute Content-Length as zero.
	*/
	void	grantImplicitHead();
};

/*
URL path -> filesystem path. THE contract between Liza's router and Ravi's
file loader. both call this; neither reimplements it.

	ALIAS SEMANTICS (decision 2). the subject says:
		"if URL /kapouet is rooted to /tmp/www, URL /kapouet/pouic/toto/pouet
		will search for /tmp/www/pouic/toto/pouet"
	note what is missing from the result: /kapouet itself. nginx `root` would
	have produced /tmp/www/kapouet/pouic/toto/pouet by plain concatenation.
	the subject strips the location prefix first, which is nginx `alias`,
	whatever the word "rooted" suggests.

	exact rule:
		1. remainder = urlPath with loc.path's leading characters removed
		2. loc.root with any trailing '/' trimmed
		3. join with exactly one '/', or return the root alone if the
			remainder is empty (request targeted the location root itself)

	PRECONDITION - urlPath must ALREADY be percent-decoded and normalized by
	the router: no "..", no "%2e%2e", no "//". this function does no security
	checking whatsoever, it is pure string arithmetic. the traversal guard is
	the router's job and happens BEFORE this call.
*/
std::string	resolvePath(const LocationConfig &loc, const std::string &urlPath);
