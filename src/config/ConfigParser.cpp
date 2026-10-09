#include "config/ConfigParser.hpp"
#include <cctype>
#include <iostream> //for TESTING

struct Token
{
	std::string	text;
	size_t		line;
};


ConfigError::ConfigError(size_t line, const std::string &message)
	: std::runtime_error("config error, line " + std::to_string(line) + ": " + message), _line(line)
{
}

size_t ConfigError::line() const
{
	return (_line);
}

static void	flushWord(std::string &current, size_t line, std::vector<Token> &tokens)
{
	if (current.empty())
		return;
	tokens.push_back(Token{current, line});
	current.clear();
}

static std::vector<Token>	tokenize(const std::string &text)
{
	std::vector<Token>	tokens;
	std::string			current;
	size_t				line = 1;

	for (size_t i = 0; i < text.size(); i++)
	{
		char c = text[i];
		if (c == '\n')
		{
			flushWord(current, line, tokens);
			line++;
		}
		else if(std::isspace(static_cast<unsigned char>(c)))
			flushWord(current, line, tokens);
		else if(c == '#')
		{
			flushWord(current, line, tokens);
			while (i + 1 < text.size() && text[i + 1] != '\n')
				i++;
		}
		else if(c == '{' || c == '}' || c == ';')
		{
			flushWord(current, line, tokens);
			current += c;
			flushWord(current, line, tokens);
		}
		else
			current += c;
	}
	flushWord(current, line, tokens);
	return (tokens);
}

struct Cursor {
	static constexpr size_t		NO_MAX = static_cast<size_t>(-1);	// readArgs: no upper limit

	const std::vector<Token>	&tokens;
	size_t						pos;

	bool					atEnd() const;
	const Token				&peek() const;
	const Token				&next();
	void					expect(const std::string &text);
	std::vector<Token>		readArgs(const Token &name, size_t min, size_t max);
};

bool	Cursor::atEnd() const
{
	return (pos >= tokens.size());
}

const Token	&Cursor::peek() const
{
	if (atEnd())
		throw ConfigError(tokens.empty() ? 1 : tokens.back().line, "unexpected end of file");
	return (tokens[pos]);
}

const Token	&Cursor::next()
{
	const Token &t = peek();
	pos++;
	return (t);
}

void	Cursor::expect(const std::string &text)
{
	const Token &t = next();
	if (t.text != text)
		throw ConfigError(t.line, "expected '" + text + "' before '" + t.text + "'");
}

/*
arguments of a simple directive, consuming its ';'. a '{' or '}' first means
the ';' was forgotten. returns Tokens, not strings, so the value parsers can
still point at the line of a bad argument.
*/
std::vector<Token>	Cursor::readArgs(const Token &name, size_t min, size_t max)
{
	std::vector<Token>	args;

	while (peek().text != ";")
	{
		const Token &t = next();
		if (t.text == "{" || t.text == "}")
			throw ConfigError(t.line, "expected ';' before '" + t.text + "'");
		args.push_back(t);
	}
	next();
	if (args.size() < min || args.size() > max)
		throw ConfigError(name.line, "invalid number of arguments in '" + name.text + "'");
	return (args);
}

std::vector<ServerConfig>	ConfigParser::parseString(const std::string &text)
{
	std::vector<Token> tokens = tokenize(text);
	for (size_t i = 0; i < tokens.size(); i++)
		std::cout << tokens[i].line << "\t" << tokens[i].text << "\n";   // TEMPORARY, needs <iostream>
	return {};
}
