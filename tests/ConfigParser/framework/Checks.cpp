#include "Tester.hpp"

#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>

#ifndef REPO_ROOT
# define REPO_ROOT "../.."
#endif

/*
=============================================================================
	what a test expects
=============================================================================
*/

Expect	gives(const std::string &value)
{
	return (Expect{value});
}

/* the same text ConfigError's constructor builds */
Expect	fails(size_t line, const std::string &message)
{
	return (Expect{"config error, line " + std::to_string(line) + ": " + message});
}

/*
=============================================================================
	files
=============================================================================
*/

std::string	Tester::repoPath(const std::string &relative)
{
	return (std::string(REPO_ROOT) + "/" + relative);
}

std::string	Tester::readFile(const std::string &path)
{
	std::ifstream		in(path.c_str(), std::ios::binary);
	std::ostringstream	text;

	if (!in)
		throw std::runtime_error("cannot read " + path);
	text << in.rdbuf();
	return (text.str());
}

/*
=============================================================================
	the checks
=============================================================================
*/

void	Tester::test(const std::string &name, const std::string &input, const Expect &expect,
			const std::function<std::string()> &body)
{
	check(name, input, expect.text, [&expect, &body]() {
		std::string	got;

		try
		{
			got = body();
		}
		catch (const ConfigError &e)
		{
			got = e.what();
		}
		return (Result{got == expect.text ? Pass : Fail, got});
	});
}

void	Tester::parses(const std::string &name, const std::string &config, const std::vector<std::string> &facts)
{
	parsesWith(name, config, [&config]() { return (ConfigParser::parseString(config)); }, facts);
}

void	Tester::parsesFile(const std::string &path, const std::vector<std::string> &facts)
{
	parsesWith(path, "file " + path, [&path]() { return (ConfigParser::parseFile(repoPath(path))); }, facts);
}

/*
runs the parse, turns the result into facts, then looks up every fact the
test names. "got" lists the same keys with the values the parser produced,
so the two columns line up and the wrong line shows in red.
*/
void	Tester::parsesWith(const std::string &name, const std::string &input,
			const std::function<std::vector<ServerConfig>()> &parse, const std::vector<std::string> &facts)
{
	std::string	expected;

	for (const std::string &fact : facts)
		expected += (expected.empty() ? "" : "\n") + fact;
	if (facts.empty())
		expected = "parses without an error";

	check(name, input, expected, [&]() {
		std::vector<std::pair<std::string, std::string>>	actual;

		try
		{
			actual = configFacts(parse());
		}
		catch (const ConfigError &e)
		{
			return (Result{Fail, e.what()});
		}
		if (facts.empty())
			return (Result{Pass, expected});

		std::map<std::string, std::string>	byKey(actual.begin(), actual.end());
		std::string							got;
		bool								ok = true;

		for (const std::string &fact : facts)
		{
			size_t		equals = fact.find(" = ");
			std::string	line;

			if (equals == std::string::npos)
				line = fact + "   <- not a fact: write it as 'key = value'";
			else
			{
				std::string	key = fact.substr(0, equals);
				std::map<std::string, std::string>::const_iterator	it = byKey.find(key);

				line = key + " = " + (it == byKey.end() ? "(no such fact - see ./tester --facts)" : it->second);
			}
			if (line != fact)
				ok = false;
			got += (got.empty() ? "" : "\n") + line;
		}
		return (Result{ok ? Pass : Fail, got});
	});
}

static std::string	parsedWithoutError(const std::vector<ServerConfig> &servers)
{
	return ("parsed without an error (" + std::to_string(servers.size())
		+ (servers.size() == 1 ? " server)" : " servers)"));
}

void	Tester::rejects(const std::string &name, const std::string &config, size_t line, const std::string &message)
{
	test(name, config, fails(line, message), [&config]() {
		return (parsedWithoutError(ConfigParser::parseString(config)));
	});
}

void	Tester::rejectsFile(const std::string &path, size_t line, const std::string &message)
{
	test(path, "file " + path, fails(line, message), [&path]() {
		return (parsedWithoutError(ConfigParser::parseFile(repoPath(path))));
	});
}

/*
=============================================================================
	a parsed config, as facts
=============================================================================
*/

std::string	methodsText(const std::set<HttpMethod> &methods)
{
	static const std::pair<HttpMethod, const char *>	order[] = {
		{HttpMethod::Get, "GET"}, {HttpMethod::Head, "HEAD"}, {HttpMethod::Post, "POST"},
		{HttpMethod::Delete, "DELETE"}, {HttpMethod::Unknown, "UNKNOWN"}};
	std::string	out;

	for (const std::pair<HttpMethod, const char *> &method : order)
		if (methods.count(method.first))
			out += (out.empty() ? "" : " ") + std::string(method.second);
	return (out.empty() ? "(none)" : out);
}

static std::string	listOrNone(const std::vector<std::string> &items)
{
	std::string	out;

	for (const std::string &item : items)
		out += (out.empty() ? "" : " ") + item;
	return (out.empty() ? "(none)" : out);
}

static std::string	textOrQuotes(const std::string &text)
{
	return (text.empty() ? "\"\"" : text);
}

/*
	servers                 2
	s0 listen               0.0.0.0:80 127.0.0.1:81      (none) if empty
	s0 names                a.test b.test
	s0 body                 1048576
	s0 errors               404=/errors/404.html 500=/50x.html
	s0 locations            / /errors/                    declaration order
	s0 /errors/ root        ./www/errors                  "" if empty
	s0 /errors/ index       index.html
	s0 /errors/ autoindex   on | off
	s0 /errors/ methods     GET HEAD
	s0 /errors/ body        1048576
	s0 /errors/ cgi         .py /usr/bin/python3          (none) if not CGI
	s0 /errors/ upload      ./www/uploads
	s0 /errors/ return      301 /new/
	s0 /errors/ internal    yes | no
*/
std::vector<std::pair<std::string, std::string>>	configFacts(const std::vector<ServerConfig> &servers)
{
	std::vector<std::pair<std::string, std::string>>	facts;

	facts.push_back({"servers", std::to_string(servers.size())});
	for (size_t i = 0; i < servers.size(); i++)
	{
		const ServerConfig			&server = servers[i];
		const std::string			s = "s" + std::to_string(i) + " ";
		std::vector<std::string>	listens;
		std::vector<std::string>	errors;
		std::vector<std::string>	paths;

		for (const Listen &listen : server.listens)
			listens.push_back(listen.host + ":" + std::to_string(listen.port));
		for (const std::pair<const int, std::string> &page : server.errorPages)
			errors.push_back(std::to_string(page.first) + "=" + page.second);
		for (const LocationConfig &location : server.locations)
			paths.push_back(location.path);

		facts.push_back({s + "listen", listOrNone(listens)});
		facts.push_back({s + "names", listOrNone(server.serverNames)});
		facts.push_back({s + "body", std::to_string(server.clientMaxBody)});
		facts.push_back({s + "errors", listOrNone(errors)});
		facts.push_back({s + "locations", listOrNone(paths)});

		for (const LocationConfig &location : server.locations)
		{
			const std::string	l = s + location.path + " ";

			facts.push_back({l + "root", textOrQuotes(location.root)});
			facts.push_back({l + "index", textOrQuotes(location.index)});
			facts.push_back({l + "autoindex", location.autoindex ? "on" : "off"});
			facts.push_back({l + "methods", methodsText(location.allowedMethods)});
			facts.push_back({l + "body", std::to_string(location.clientMaxBody)});
			facts.push_back({l + "cgi", location.cgiExt.empty() ? "(none)" : location.cgiExt + " " + location.cgiPath});
			facts.push_back({l + "upload", location.uploadStore.empty() ? "(none)" : location.uploadStore});
			facts.push_back({l + "return", location.redirect.empty() ? "(none)"
				: std::to_string(location.redirectCode) + " " + location.redirect});
			facts.push_back({l + "internal", location.internalOnly ? "yes" : "no"});
		}
	}
	return (facts);
}
