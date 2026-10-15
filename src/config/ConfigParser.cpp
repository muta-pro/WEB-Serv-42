#include "config/ConfigParser.hpp"
#include <cctype>
#include <iostream> //for TESTING
#include <limits>

struct  Token
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

/*
STEP 5 - value parsers. each takes the Token, not just its text, so an
error points at the line of the bad argument.
[[maybe_unused]] stops -Werror failing on a static function nobody calls
yet. remove it once step 6 calls the function.
*/

/*
a plain decimal: digits only, no sign, no spaces, nothing after. a value
above max is refused BEFORE it is computed, so it can never overflow.
std::stoi / std::stoul are not used: they accept "+80", " 80" and "80abc",
and throw their own exceptions instead of a ConfigError.
digits is the part of tok.text being read ("8080" out of "0.0.0.0:8080");
tok is only there for the error message.
*/
static size_t	parseNumber(const Token &tok, const std::string &digits, size_t max)
{
	size_t	value = 0;

	if (digits.empty())
		throw ConfigError(tok.line, "invalid number in '" + tok.text + "'");
	for (size_t i = 0; i < digits.length(); i++)
	{
		if (!std::isdigit(static_cast<unsigned char>(digits[i])))
			throw ConfigError(tok.line, "invalid number in '" + tok.text + "'");
		size_t d = static_cast<size_t>(digits[i] - '0');
		if (value > (max - d) / 10)
			throw ConfigError(tok.line, "number too large in '" + tok.text + "'");
		value = value * 10 + d;
	}
	return (value);
}

/*
"host:port" or a bare "port". the host can be any non-empty string (open
decision 2): the socket code resolves it with getaddrinfo. a bare port
keeps Listen's default host, 0.0.0.0, meaning every interface.
*/
static Listen	parseListen(const Token &arg)
{
	Listen		result;
	std::string	port = arg.text;
	size_t		colon = arg.text.find(':');

	if (colon != std::string::npos)
	{
		result.host = arg.text.substr(0, colon);
		port = arg.text.substr(colon + 1);
		if (result.host.empty())
			throw ConfigError(arg.line, "missing host in '" + arg.text + "'");
	}
	result.port = static_cast<int>(parseNumber(arg, port, 65535));
	if (result.port == 0)
		throw ConfigError(arg.line, "port 0 is not allowed in '" + arg.text + "'");
	return (result);
}

/*
client_max_body_size: digits with an optional k/K, m/M or g/G suffix, in
binary units (1k = 1024). the max given to parseNumber leaves room for the
multiply, so "99999999999G" is refused instead of wrapping around.
*/
[[maybe_unused]] static size_t	parseSize(const Token &arg)
{
	std::string	digits = arg.text;
	size_t		multiplier = 1;
	char		suffix = digits.back();

	if (std::isalpha(static_cast<unsigned char>(suffix)))
	{
		if (suffix == 'k' || suffix == 'K' )
			multiplier = 1024;
		else if (suffix == 'm' || suffix == 'M' )
			multiplier = 1024 * 1024;
		else if (suffix == 'g' || suffix == 'G' )
			multiplier = 1024 * 1024 * 1024;
		else
			throw ConfigError(arg.line, "unknown size suffix in '" + arg.text + "'");
		digits.pop_back();
	}
	return (parseNumber(arg, digits, std::numeric_limits<size_t>::max() / multiplier) * multiplier);
}

[[maybe_unused]] static bool	parseOnOff(const Token &arg)
{
	if (arg.text == "on" || arg.text == "ON")
		return (true);
	if (arg.text == "off" || arg.text == "OFF")
		return (false);
	throw ConfigError(arg.line, "expected 'on' or 'off', not '" + arg.text + "'");
}

/*
error_page takes 300-599, return takes 300-399. a status code is always
three digits, so 999 is ceiling enough for parseNumber.
*/
[[maybe_unused]] static int	parseStatusCode(const Token &arg, int lo, int hi)
{
	int	code = static_cast<int>(parseNumber(arg, arg.text, 999));

	if (code < lo || code > hi)
		throw ConfigError(arg.line, "status code '" + arg.text + "' is not between "
			+ std::to_string(lo) + " and " + std::to_string(hi));
	return (code);
}

/*
methods GET POST DELETE; -> that set. methods are case-sensitive
(RFC 9110 9.1), so "get" is refused. HEAD may be written out (open
decision 5); grantImplicitHead() also adds it wherever GET is, in step 8.
a repeated method is harmless: a set holds each value once.
*/
[[maybe_unused]] static std::set<HttpMethod>	parseMethods(const std::vector<Token> &args)
{
	std::set<HttpMethod>	methods;

	for (size_t i = 0; i < args.size(); i++)
	{
		const std::string &m = args[i].text;

		if (m == "GET")
			methods.insert(HttpMethod::Get);
		else if (m == "HEAD")
			methods.insert(HttpMethod::Head);
		else if (m == "POST")
			methods.insert(HttpMethod::Post);
		else if (m == "DELETE")
			methods.insert(HttpMethod::Delete);
		else
			throw ConfigError(args[i].line, "unknown method '" + m + "'");
	}
	return (methods);
}

/*
STEP 4 PLACEHOLDER. step 7 replaces this with the real location parser
(RawLocation). for now it only walks over the block, so a server that
contains locations can be parsed end to end.
called with the cursor just past the "location" keyword.
*/
static void	parseLocation(Cursor &cur)
{
	const Token &path = cur.next();

	if (path.text[0] != '/')
		throw ConfigError(path.line, "expected a path after 'location'");
	cur.expect("{");
	while (cur.peek().text != "}")
	{
		const Token &name = cur.next();
		cur.readArgs(name, 0, Cursor::NO_MAX);	// thrown away until step 7
	}
	cur.next();	// the '}'
}

/*
STEP 6 - one simple (non-block) directive at server level. called with the
cursor just past the directive's name; reads its arguments and the ';'.
	listen, server_name, error_page     -> server
	root, index, autoindex, methods,    -> defaults, which are only copied
	client_max_body_size                   into the locations (step 8)
a repeated directive: the last one wins.
kept out of parseServer so the step 6 tests can call it and read the
defaults back - nothing else can see them until step 8.
*/
static void	parseServerDirective(Cursor &cur, const Token &name, ServerConfig &server,
	[[maybe_unused]] ServerDefaults &defaults)
{
	if (name.text == "listen")
	{
		std::vector<Token> args = cur.readArgs(name, 1, 1);
		server.listens.push_back(parseListen(args[0]));
	}
	else if (name.text == "server_name")
	{
		std::vector<Token> args = cur.readArgs(name, 1, Cursor::NO_MAX);
		for (size_t i = 0; i < args.size(); i++)
			server.serverNames.push_back(args[i].text);
	}
	else if (name.text == "error_page")
	{
		std::vector<Token> args = cur.readArgs(name, 2, Cursor::NO_MAX);
		for (size_t i = 0; i < args.size() - 1; i++)
			server.errorPages<parseStatusCode(args[i], ??, ??),args.back().text>;
		if (args.back().text[0] != '/')
			throw ConfigError(args.back().line, "expected a URI starting with '/', not 'X'");
	}
	else if (name.text == "client_max_body_size")
	{
		std::vector<Token> args = cur.readArgs(name, 1, 1);
		server.clientMaxBody = args[0];
	}
	else if (name.text == "root")
	{

	}
	else if (name.text == "index")
	{

	}
	else if (name.text == "autoindex")
	{
		std::vector<Token> args = cur.readArgs(name, 1, 1);
		parseOnOff(args[0]);
	}
	/*
	TODO step 6 - one branch each:
	- error_page CODE... URI      2 args or more. every arg but the last is
	                              a code, 300-599; the last is the URI and
	                              must start with '/' (guarantee 5)
	- client_max_body_size SIZE   -> defaults. what 0 means: open decision 7.
	                              ServerConfig::clientMaxBody is step 8 (§9)
	- root PATH, index FILE       -> defaults
	- autoindex on|off            -> defaults
	- methods M...                -> defaults. a second methods line
	                              replaces the first, it doesn't add to it
	then drop [[maybe_unused]] from defaults and from each value parser
	you now call.
	*/
	else
		throw ConfigError(name.line, "unknown directive '" + name.text + "'");
}

/*
one server block. called with the cursor just past "server {", returns
with the cursor just past its '}'. location is the only block directive;
everything else is a simple directive.
*/
static ServerConfig	parseServer(Cursor &cur)
{
	ServerConfig	server;
	ServerDefaults	defaults;	// step 8 copies these into the locations, at the '}'

	while (cur.peek().text != "}")
	{
		const Token &name = cur.next();

		if (name.text == "location")
			parseLocation(cur);
		else
			parseServerDirective(cur, name, server, defaults);
	}
	cur.next();
	return (server);
}

/*
the top level holds server blocks and nothing else.
*/
std::vector<ServerConfig>	ConfigParser::parseString(const std::string &text)
{
	std::vector<Token>			tokens = tokenize(text);
	Cursor						cur{tokens, 0};
	std::vector<ServerConfig>	servers;

	while (!cur.atEnd())
	{
		const Token &t = cur.next();

		if (t.text == "}")
			throw ConfigError(t.line, "unexpected '}'");
		if (t.text != "server")
			throw ConfigError(t.line, "directive '" + t.text + "' is outside any block");
		cur.expect("{");
		servers.push_back(parseServer(cur));
	}
	return (servers);
}
