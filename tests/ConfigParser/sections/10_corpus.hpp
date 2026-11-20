#pragma once

#include "../framework/Tester.hpp"
#include "../parser_under_test.hpp"
#include "02_tokenizer.hpp"	// confFiles()

#include <cctype>
#include <sstream>

/*
STEP 10 - the corpus: every config file in the repo, read by parseFile the
way ./webserv will read it.
	every .conf in conf/          must parse. invalid_syntax.conf is the
	                              exception: the skeleton section checks it
	every .conf in conf/invalid/  must be rejected, each with the error its
	                              "# expect: config error, line N: message"
	                              line names
a new file in either place is a new test: nothing here has to change.
*/

/*
the file's "# expect: config error, line N: message" line, as N and the
message. false when it has no such line, or N isn't a number.
*/
inline bool	expectedError(const std::string &path, size_t &line, std::string &message)
{
	const std::string	prefix = "# expect: config error, line ";
	std::istringstream	in(Tester::readFile(Tester::repoPath(path)));
	std::string			text;

	while (std::getline(in, text))
	{
		if (text.compare(0, prefix.size(), prefix) != 0)
			continue;
		size_t	colon = text.find(": ", prefix.size());
		if (colon == std::string::npos || colon == prefix.size())
			return (false);
		line = 0;
		for (size_t i = prefix.size(); i < colon; i++)
		{
			if (!std::isdigit(static_cast<unsigned char>(text[i])))
				return (false);
			line = line * 10 + static_cast<size_t>(text[i] - '0');
		}
		message = text.substr(colon + 2);
		return (true);
	}
	return (false);
}

inline void	testCorpus(Tester &t)
{
	const std::string			invalidDir = "conf/invalid/";
	std::vector<std::string>	invalid;

	/* the valid files */
	for (const std::string &path : confFiles())
	{
		if (path.compare(0, invalidDir.size(), invalidDir) == 0)
			invalid.push_back(path);
		else if (path != "conf/invalid_syntax.conf")
			t.parsesFile(path, {});
	}

	/* the rejection cases, one error per file */
	t.test("conf/invalid has the rejection cases", "conf/invalid/*.conf", gives("at least 30 files"), [&invalid] {
		return (invalid.size() >= 30 ? std::string("at least 30 files") : std::to_string(invalid.size()) + " files");
	});
	for (const std::string &path : invalid)
	{
		size_t		line;
		std::string	message;

		if (expectedError(path, line, message))
			t.rejectsFile(path, line, message);
		else
			t.test(path, "file " + path, gives("a line '# expect: config error, line N: message'"), [] {
				return (std::string("no such line"));
			});
	}
}
