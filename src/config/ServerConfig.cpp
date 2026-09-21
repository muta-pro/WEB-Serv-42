#include "config/ServerConfig.hpp"

#include <cctype>

/* ------------------------------------------------------------------ Listen */

bool Listen::operator<(const Listen &other) const
{
	if (host != other.host)
		return (host < other.host);
	return (port < other.port);
}

bool Listen::operator==(const Listen &other) const
{
	return (host == other.host && port == other.port);
}

/* ----------------------------------------------------------- LocationConfig */

bool LocationConfig::allowsMethod(const std::string &method) const
{
	return (allowedMethods.count(method) != 0);
}

bool LocationConfig::isCGI() const
{
	return (!cgiExt.empty());
}

bool LocationConfig::isRedirect() const
{
	return (!redirect.empty());
}

bool LocationConfig::acceptsUploads() const
{
	return (!uploadStore.empty());
}

void LocationConfig::grantImplicitHead()
{
	if (allowedMethods.count("GET") != 0)
		allowedMethods.insert("HEAD");
}

/*
alias-style join. see the rule spelled out in LocationConfig.hpp.

	/kapouet + /tmp/www + /kapouet/pouic/toto/pouet -> /tmp/www/pouic/toto/pouet

trailing slashes on root are trimmed so "./www/" and "./www" behave alike,
and a root of "/" collapses to "" so the join does not produce "//pouic".
*/
std::string resolvePath(const LocationConfig &loc, const std::string &urlPath)
{
	std::string	base = loc.root;
	std::string	remainder;

	if (base.empty())
		base = ".";
	while (!base.empty() && base[base.size() - 1] == '/')
		base.erase(base.size() - 1);
	if (urlPath.size() >= loc.path.size())
		remainder = urlPath.substr(loc.path.size());
	if (remainder.empty())
		return (base.empty() ? "/" : base);
	if (remainder[0] != '/')
		remainder.insert(0, "/");
	return (base + remainder);
}

/* ------------------------------------------------------------- ServerConfig */

std::string ServerConfig::errorPageUri(int code) const
{
	std::map<int, std::string>::const_iterator	it = errorPages.find(code);

	if (it != errorPages.end())
		return (it->second);
	return ("");
}

/* "example.com:8080" -> "example.com", "[::1]:8080" -> "[::1]" */
static std::string stripPort(const std::string &hostHeader)
{
	std::string	host = hostHeader;
	size_t		colon = host.rfind(':');
	size_t		bracket = host.rfind(']');

	if (colon == std::string::npos)
		return (host);
	if (bracket != std::string::npos && colon < bracket)
		return (host);
	if (colon + 1 == host.size())
		return (host);
	for (size_t i = colon + 1; i < host.size(); i++)
	{
		if (!std::isdigit(static_cast<unsigned char>(host[i])))
			return (host);
	}
	host.erase(colon);
	return (host);
}

static bool equalsIgnoreCase(const std::string &a, const std::string &b)
{
	if (a.size() != b.size())
		return (false);
	for (size_t i = 0; i < a.size(); i++)
	{
		if (std::tolower(static_cast<unsigned char>(a[i]))
			!= std::tolower(static_cast<unsigned char>(b[i])))
			return (false);
	}
	return (true);
}

bool ServerConfig::matchesHost(const std::string &hostHeader) const
{
	std::string	host = stripPort(hostHeader);

	for (size_t i = 0; i < serverNames.size(); i++)
	{
		if (equalsIgnoreCase(serverNames[i], host))
			return (true);
	}
	return (false);
}
