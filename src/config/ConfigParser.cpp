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

std::vector<ServerConfig>	ConfigParser::parseString(const std::string &text)
{
	std::vector<Token> tokens = tokenize(text);
	for (size_t i = 0; i < tokens.size(); i++)
		std::cout << tokens[i].line << "\t" << tokens[i].text << "\n";   // TEMPORARY, needs <iostream>
	return {};
}
