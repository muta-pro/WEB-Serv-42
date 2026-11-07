#pragma once

/*
the parser's own source file, compiled INTO the test program.

	ConfigParser.cpp keeps most of its parts to itself: Token, Cursor,
	tokenize() and the value parsers are static or only defined inside the
	.cpp, so no header can reach them. including the .cpp here puts all of
	it into this translation unit, and the sections can test every part on
	its own instead of only through parseString().

	the catch: ConfigParser.cpp must NOT also be compiled separately into
	the tester, or everything in it would be defined twice. the Makefile
	leaves it out, and main.cpp is the only file that includes this one.
*/
#include "../../src/config/ConfigParser.cpp"

#include <string>
#include <vector>

/* "listen 8080 ;" - the token texts, one space apart */
inline std::string	tokenTexts(const std::vector<Token> &tokens)
{
	std::string	out;

	for (const Token &token : tokens)
		out += (out.empty() ? "" : " ") + token.text;
	return (out.empty() ? "(no tokens)" : out);
}

/* "listen@1 8080@1 ;@2" - each token with the line it was found on */
inline std::string	tokenLines(const std::vector<Token> &tokens)
{
	std::string	out;

	for (const Token &token : tokens)
		out += (out.empty() ? "" : " ") + token.text + "@" + std::to_string(token.line);
	return (out.empty() ? "(no tokens)" : out);
}
