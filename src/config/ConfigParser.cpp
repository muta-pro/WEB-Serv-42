#include "config/ConfigParser.hpp"
#include <cctype>
#include <fstream>
#include <iostream> //for TESTING
#include <limits>
#include <optional>
#include <utility>

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
	if (args.size() > max)
	{
		const Token &before = (max == 0) ? name : args[max - 1];
		if (args[max].line != before.line)
			throw ConfigError(args[max].line, "expected ';' before '" + args[max].text + "'");
	}
	if (args.size() < min || args.size() > max)
		throw ConfigError(name.line, "invalid number of arguments in '" + name.text + "'");
	return (args);
}

/*
STEP 5 - value parsers. each takes the Token, not just its text, so an
error points at the line of the bad argument.
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
static size_t	parseSize(const Token &arg)
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
	size_t size = parseNumber(arg, digits, std::numeric_limits<size_t>::max() / multiplier) * multiplier;
	if (size == 0)
		throw ConfigError(arg.line, "size 0 is not allowed in '" + arg.text + "'");
	return (size);
}

static bool	parseOnOff(const Token &arg)
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
static int	parseStatusCode(const Token &arg, int lo, int hi)
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
static std::set<HttpMethod>	parseMethods(const std::vector<Token> &args)
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
STEP 7 - one location block as the file wrote it, before the server's
defaults are known (handover §8). the five inheritable fields are
std::optional: empty means "this block didn't say", and only those get
the server's value in step 8 (value_or). `autoindex off;` is NOT empty -
it holds false, and that false must survive.
own holds what only a location has: path, cgi, upload_store, return,
internal. own's root/index/autoindex/allowedMethods/clientMaxBody stay
untouched here; step 8 fills them from the optionals.
*/
struct RawLocation
{
	std::optional<std::string>			root;
	std::optional<std::string>			index;
	std::optional<bool>					autoindex;
	std::optional<size_t>				clientMaxBody;
	std::optional<std::set<HttpMethod>>	methods;
	LocationConfig						own;
};

/*
STEP 7 - one simple directive inside a location. called with the cursor
just past the directive's name; reads its arguments and the ';'.
	root, index, autoindex, methods,    -> the optionals in raw
	client_max_body_size
	upload_store, cgi, return, internal -> raw.own
	listen, server_name, error_page,    -> refused: server level only
	location                               (no nested blocks)
a repeated directive: the last one wins, as at server level.
*/
static void	parseLocationDirective(Cursor &cur, const Token &name, RawLocation &raw)
{
	if (name.text == "root")
	{
		std::vector<Token> args = cur.readArgs(name, 1, 1);
		raw.root = args[0].text;
	}
	else if (name.text == "index")
	{
		std::vector<Token> args = cur.readArgs(name, 1, 1);
		raw.index = args[0].text;
	}
	else if (name.text == "autoindex")
	{
		std::vector<Token> args = cur.readArgs(name, 1, 1);
		raw.autoindex = parseOnOff(args[0]);
	}
	else if (name.text == "methods")
	{
		std::vector<Token> args = cur.readArgs(name, 1, Cursor::NO_MAX);
		raw.methods = parseMethods(args);
	}
	else if (name.text == "client_max_body_size")
	{
		std::vector<Token> args = cur.readArgs(name, 1, 1);
		raw.clientMaxBody = parseSize(args[0]);
	}
	else if (name.text == "upload_store")
	{
		std::vector<Token> args = cur.readArgs(name, 1, 1);
		raw.own.uploadStore = args[0].text;
	}
	else if (name.text == "cgi")
	{
		std::vector<Token> args = cur.readArgs(name, 2, 2);
		if (args[0].text[0] != '.')
			throw ConfigError(args[0].line , "expected an extension starting with '.', not '" + args[0].text + "'");
		raw.own.cgiExt = args[0].text;
		raw.own.cgiPath = args[1].text;
	}
	else if (name.text == "return")
	{
		std::vector<Token> args = cur.readArgs(name, 2, 2);
		raw.own.redirectCode = parseStatusCode(args[0], 300, 399);
		raw.own.redirect = args[1].text;
	}
	else if (name.text == "internal")
	{
		cur.readArgs(name, 0, 0);	// only the ';'
		raw.own.internalOnly = true;
	}
	else if (name.text == "listen" || name.text == "server_name" || name.text == "error_page" || name.text == "location")
	{
		throw ConfigError(name.line , "directive '" + name.text + "' is not allowed inside a location");
	}
	else
	{
		throw ConfigError(name.line , "unknown directive '" + name.text + "'");
	}
}

/*
STEP 7 - one location block. called with the cursor just past the
"location" keyword, returns with it just past the block's '}'.
siblings: the locations this server already has, for the duplicate check.
*/
static RawLocation	parseLocation(Cursor &cur, const std::vector<RawLocation> &siblings)
{
	RawLocation	raw;
	const Token	&path = cur.next();

	if (path.text[0] != '/')
		throw ConfigError(path.line, "expected a path after 'location'");
	for (size_t i = 0; i < siblings.size(); i++)
	{
		if (siblings[i].own.path == path.text)
			throw ConfigError(path.line, "duplicate location '" + path.text + "'");
	}
	raw.own.path = path.text;
	cur.expect("{");
	while (cur.peek().text != "}")
	{
		const Token &name = cur.next();
		parseLocationDirective(cur, name, raw);
	}
	cur.next();	// the '}'
	return (raw);
}

/*
STEP 6 - one simple (non-block) directive at server level. called with the
cursor just past the directive's name; reads its arguments and the ';'.
	listen, server_name, error_page     -> server
	root, index, autoindex, methods,    -> defaults, which are only copied
	client_max_body_size                   into the locations (step 8)
a repeated directive: the last one wins.
kept out of parseServer so the step 6 tests can call it and read the
defaults back - nothing else sees them before finalizeServer.
*/
static void	parseServerDirective(Cursor &cur, const Token &name, ServerConfig &server,
	ServerDefaults &defaults)
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
		/*
		every arg but the last is a code (300-599), the last is the URI
		(guarantee 5). errorPages[code] inserts or overwrites, so a code
		bound twice keeps its last URI.
		*/
		std::vector<Token> args = cur.readArgs(name, 2, Cursor::NO_MAX);
		const Token &uri = args.back();

		if (uri.text[0] != '/')
			throw ConfigError(uri.line, "expected a URI starting with '/', not '" + uri.text + "'");
		for (size_t i = 0; i < args.size() - 1; i++)
			server.errorPages[parseStatusCode(args[i], 300, 599)] = uri.text;
	}
	else if (name.text == "client_max_body_size")
	{
		// parseSize refuses 0 (decision 7). ServerConfig::clientMaxBody is set in finalizeServer (§9, B)
		std::vector<Token> args = cur.readArgs(name, 1, 1);
		defaults.clientMaxBody = parseSize(args[0]);
	}
	else if (name.text == "root")
	{
		std::vector<Token> args = cur.readArgs(name, 1, 1);
		defaults.root = args[0].text;
	}
	else if (name.text == "index")
	{
		std::vector<Token> args = cur.readArgs(name, 1, 1);
		defaults.index = args[0].text;
	}
	else if (name.text == "autoindex")
	{
		std::vector<Token> args = cur.readArgs(name, 1, 1);
		defaults.autoindex = parseOnOff(args[0]);
	}
	else if (name.text == "methods")
	{
		// assigning replaces the whole set: a second methods line doesn't add to the first
		std::vector<Token> args = cur.readArgs(name, 1, Cursor::NO_MAX);
		defaults.allowedMethods = parseMethods(args);
	}
	else if (name.text == "upload_store" || name.text == "cgi" || name.text == "return" || name.text == "internal")
	{
		throw ConfigError(name.line , "directive '" + name.text + "' is only allowed inside a location");
	}
	else
		throw ConfigError(name.line, "unknown directive '" + name.text + "'");
}

/*
STEP 8 - one location, final. starts from raw.own (path, cgi, upload_store,
return, internal), then gives each inheritable field the block's own value
if it had one, the server's default if not:
	loc.root = raw.root.value_or(defaults.root);
guarantee 6 (no empty root) needs no check here: a token is never empty
and readArgs wants one argument, so `root ;` is already an error.
*/
static LocationConfig	finalizeLocation(const RawLocation &raw,
	const ServerDefaults &defaults)
{
	LocationConfig	loc = raw.own;

	loc.root = raw.root.value_or(defaults.root);
	loc.index = raw.index.value_or(defaults.index);
	loc.autoindex = raw.autoindex.value_or(defaults.autoindex);
	loc.clientMaxBody = raw.clientMaxBody.value_or(defaults.clientMaxBody);
	loc.allowedMethods = raw.methods.value_or(defaults.allowedMethods);
	loc.grantImplicitHead();
	return (loc);
}

/*
STEP 8 - at the server's '}'. the defaults are final now, so the
RawLocations can become the server's LocationConfigs. line is the line of
the `server` keyword, for errors about the block as a whole.
*/
static void	finalizeServer(ServerConfig &server, const ServerDefaults &defaults,
	const std::vector<RawLocation> &locations, size_t line)
{
	if (server.listens.empty())
		throw ConfigError(line, "server has no 'listen'");
	bool hasSlash = false;
	for (size_t i = 0; i < locations.size(); i++)
	{
		if (locations[i].own.path == "/")
			hasSlash = true;
		server.locations.push_back(finalizeLocation(locations[i], defaults));
	}
	if (!hasSlash)
	{
		RawLocation slash;
		slash.own.path = "/";
		server.locations.push_back(finalizeLocation(slash, defaults));
	}
	server.clientMaxBody = 0;
	for (size_t i = 0; i < server.locations.size(); i++)
	{
		if (server.locations[i].clientMaxBody > server.clientMaxBody)
			server.clientMaxBody = server.locations[i].clientMaxBody;
	}
}

/*
one server block. called with the cursor just past "server {", returns
with the cursor just past its '}'. location is the only block directive;
everything else is a simple directive.
line: the line of the `server` keyword.
*/
static ServerConfig	parseServer(Cursor &cur, size_t line)
{
	ServerConfig				server;
	ServerDefaults				defaults;	// copied into the locations by finalizeServer
	std::vector<RawLocation>	locations;	// become server.locations in finalizeServer

	while (cur.peek().text != "}")
	{
		const Token &name = cur.next();

		if (name.text == "location")
			locations.push_back(parseLocation(cur, locations));
		else
			parseServerDirective(cur, name, server, defaults);
	}
	cur.next();
	finalizeServer(server, defaults, locations, line);
	return (server);
}

static std::string lowercase_name(std::string text)
{
	for (size_t i = 0; i < text.length(); i++)
	{
		text[i] = std::tolower(static_cast<unsigned char>(text[i]));
	}
	return (text);
}
/*
STEP 8 - open decision 4. two servers that share a host:port can't share a
name: the first one would win every request for it, so the second could
never be reached by that name.
claimed holds every {listen, name} the earlier servers took, names in
lowercase: hosts compare case-insensitively (see matchesHost).
a std::set and not a loop over the earlier servers: with the 20,000-server
stress test, that loop would be 200 million comparisons. Listen has an
operator< so it can be part of a set key.
*/
static void	claimNames(const ServerConfig &server,
	std::set<std::pair<Listen, std::string>> &claimed, size_t line)
{
	for (size_t i = 0; i < server.listens.size(); i++)
	{
		for (size_t j = 0; j < server.serverNames.size(); j++)
		{
			const Listen					&listen = server.listens[i];
			const std::string				&name = server.serverNames[j];
			std::pair<Listen, std::string>	key = {listen, lowercase_name(name)};

			if (claimed.count(key) == 1)
				throw ConfigError(line, "conflicting server_name '" + name + "' on "
				+ listen.host + ":" + std::to_string(listen.port));
		}
	}
	for (size_t i = 0; i < server.listens.size(); i++)
	{
		for (size_t j = 0; j < server.serverNames.size(); j++)
		{
			const Listen					&listen = server.listens[i];
			const std::string				&name = server.serverNames[j];
			std::pair<Listen, std::string>	key = {listen, lowercase_name(name)};

			claimed.insert(key);
		}
	}
}

/*
STEP 8 - decision 8. a server without server_name matches no Host header,
so it can only be reached as the default server of one of its listens, and
the default is the FIRST server declared on that listen. taken holds every
listen an earlier server already has. a nameless server whose listens are
ALL in taken can never be reached. one listen that is still free is enough:
it is the default there.
a server with names is reached by name, so it only adds its listens.
*/
static void	claimDefaults(const ServerConfig &server, std::set<Listen> &taken,
	size_t line)
{
	bool all_taken = true;
	if (server.serverNames.empty())
	{
		for (size_t i = 0; i < server.listens.size(); i++)
		{
			if (taken.count(server.listens[i]) == 0)
				all_taken = false;
		}
		if (all_taken)
			throw ConfigError(line, "server without server_name can never be reached: each of its listens already has a default server");
	}
	for (size_t i = 0; i < server.listens.size(); i++)
	{
		taken.insert(server.listens[i]);
	}
}

/*
the top level holds server blocks and nothing else.
*/
std::vector<ServerConfig>	ConfigParser::parseString(const std::string &text)
{
	std::vector<Token>							tokens = tokenize(text);
	Cursor										cur{tokens, 0};
	std::vector<ServerConfig>					servers;
	std::set<std::pair<Listen, std::string>>	claimed;	// see claimNames
	std::set<Listen>							taken;		// see claimDefaults

	while (!cur.atEnd())
	{
		const Token &t = cur.next();

		if (t.text == "}")
			throw ConfigError(t.line, "unexpected '}'");
		if (t.text != "server")
			throw ConfigError(t.line, "directive '" + t.text + "' is outside any block");
		cur.expect("{");
		servers.push_back(parseServer(cur, t.line));
		claimNames(servers.back(), claimed, t.line);
		claimDefaults(servers.back(), taken, t.line);
	}
	if (servers.empty())
		throw ConfigError(1, "no server block");
	return (servers);
}

/*
STEP 9 - the file. parseFile only reads: it puts the whole file into one
string and hands that to parseString, which does all the rest. the
tokenizer counts the lines by their '\n's, so every '\n' has to survive
the read.
an error here has no line to point at, so it gets line 0.
*/
std::vector<ServerConfig>	ConfigParser::parseFile(const std::string &path)
{
	std::ifstream file(path);
	if (!file)
		throw ConfigError(0, "cannot open '" + path + "'");
	std::string line;
	std::string text;
	while(std::getline(file, line))
	{
		text += line + "\n";
	}
	if(file.bad())
		throw ConfigError(0, "cannot read '" + path + "'");
	return (parseString(text));
}
