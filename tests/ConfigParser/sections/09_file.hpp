#pragma once

#include "../framework/Tester.hpp"
#include "../parser_under_test.hpp"

#include <cstdlib>
#include <stdexcept>
#include <unistd.h>

/*
STEP 9 - parseFile: read the file, hand its text to parseString. the
parsing itself is tested in steps 1-8, so these tests are about the read:
every line arrives, on the line number it has in the file, and a path that
can't be read is an error instead of an empty config.
*/

/* "servers = 2" */
inline std::string	serverCount(const std::vector<ServerConfig> &servers)
{
	return ("servers = " + std::to_string(servers.size()));
}

/*
text written to a file of its own in /tmp, then read back through
parseFile. the file is removed again, also when the parser throws.
*/
inline std::vector<ServerConfig>	parseThroughFile(const std::string &text)
{
	char	path[] = "/tmp/webserv_config_XXXXXX";
	int		fd = mkstemp(path);

	if (fd < 0)
		throw std::runtime_error("cannot create a temporary file");
	if (write(fd, text.data(), text.size()) != static_cast<ssize_t>(text.size()))
	{
		close(fd);
		unlink(path);
		throw std::runtime_error("cannot write the temporary file");
	}
	close(fd);
	try
	{
		std::vector<ServerConfig>	servers = ConfigParser::parseFile(path);

		unlink(path);
		return (servers);
	}
	catch (...)
	{
		unlink(path);
		throw;
	}
}

/* parseFile and parseString on the same file must agree, fact for fact */
inline std::string	sameAsParseString(const std::string &relative)
{
	const std::string									path = Tester::repoPath(relative);
	std::vector<std::pair<std::string, std::string>>	fromFile = configFacts(ConfigParser::parseFile(path));
	std::vector<std::pair<std::string, std::string>>	fromText = configFacts(ConfigParser::parseString(Tester::readFile(path)));

	for (size_t i = 0; i < fromFile.size() && i < fromText.size(); i++)
		if (fromFile[i] != fromText[i])
			return ("parseFile gives '" + fromFile[i].first + " = " + fromFile[i].second
				+ "', parseString gives '" + fromText[i].first + " = " + fromText[i].second + "'");
	if (fromFile.size() != fromText.size())
		return ("parseFile gives " + std::to_string(fromFile.size()) + " facts, parseString gives "
			+ std::to_string(fromText.size()));
	return ("the same facts as parseString");
}

/* \r shown as the two characters \r, so a test's input doesn't garble the terminal */
inline std::string	showCarriageReturns(const std::string &text)
{
	std::string	out;

	for (char c : text)
		out += (c == '\r' ? std::string("\\r") : std::string(1, c));
	return (out);
}

inline void	testFile(Tester &t)
{
	auto	viaFile = [&t](const std::string &name, const std::string &text, const Expect &expect) {
		t.test(name, showCarriageReturns(text), expect, [&text] {
			return (serverCount(parseThroughFile(text)));
		});
	};

	/* a file gives exactly what its text gives */
	for (const std::string path : {"conf/default.conf", "conf/edge_cases.conf", "conf/multi_server.conf"})
		t.test("parseFile agrees with parseString on " + path, "file " + path, gives("the same facts as parseString"),
			[&path] { return (sameAsParseString(path)); });

	/* every line arrives, on its own line number */
	viaFile("the whole file is read, not just the first line", "server { listen 80; }\n\nserver { listen 81; }\n",
		gives("servers = 2"));
	viaFile("an error is reported on its line in the file", "server {\n\tlisten 80;\n\n# a comment\n\tproxy_pass x;\n}\n",
		fails(5, "unknown directive 'proxy_pass'"));
	viaFile("no '\\n' at the end of the file", "server { listen 80; }", gives("servers = 1"));
	viaFile("a comment on the last line, no '\\n' after it", "server { listen 80; }\n# the end", gives("servers = 1"));
	viaFile("an error on the last line, no '\\n' after it", "server { listen 80; }\n\nlisten 81;", fails(3,
		"directive 'listen' is outside any block"));
	viaFile("Windows line endings: \\r is whitespace", "server {\r\n\tlisten 80;\r\n}\r\n", gives("servers = 1"));
	viaFile("Windows line endings keep the line numbers", "server {\r\n\tlisten 80;\r\n\tproxy_pass x;\r\n}\r\n",
		fails(3, "unknown directive 'proxy_pass'"));

	/* nothing in the file */
	viaFile("an empty file", "", fails(1, "no server block"));
	viaFile("a file of empty lines", "\n\n\n", fails(1, "no server block"));

	/* a path that can't be read: line 0, there is no line to blame */
	const std::string	missing = Tester::repoPath("conf/no_such_file.conf");
	const std::string	directory = Tester::repoPath("conf");

	t.test("a file that doesn't exist", missing, fails(0, "cannot open '" + missing + "'"), [&missing] {
		return (serverCount(ConfigParser::parseFile(missing)));
	});
	t.test("an empty path", "\"\"", fails(0, "cannot open ''"), [] {
		return (serverCount(ConfigParser::parseFile("")));
	});
	t.test("a directory opens, but can't be read", directory, fails(0, "cannot read '" + directory + "'"), [&directory] {
		return (serverCount(ConfigParser::parseFile(directory)));
	});
}
