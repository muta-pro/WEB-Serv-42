#pragma once

#include "../framework/Tester.hpp"
#include "../parser_under_test.hpp"

/*
STEP 3 - the parser cursor: a position in the token list, and the moves
every parse function is built from: atEnd, peek, next, expect, readArgs.
*/

/* the token the cursor is on, or (end) */
inline std::string	cursorAt(const Cursor &cur)
{
	return (cur.atEnd() ? std::string("(end)") : cur.peek().text);
}

inline void	testCursor(Tester &t)
{
	/* atEnd */
	t.test("atEnd() with no tokens at all", "", gives("true"), [] {
		std::vector<Token>	tokens = tokenize("");
		Cursor				cur{tokens, 0};

		return (std::string(cur.atEnd() ? "true" : "false"));
	});
	t.test("atEnd() while tokens are left", "a b", gives("false"), [] {
		std::vector<Token>	tokens = tokenize("a b");
		Cursor				cur{tokens, 1};

		return (std::string(cur.atEnd() ? "true" : "false"));
	});
	t.test("atEnd() after the last token", "a b", gives("true"), [] {
		std::vector<Token>	tokens = tokenize("a b");
		Cursor				cur{tokens, 2};

		return (std::string(cur.atEnd() ? "true" : "false"));
	});

	/* peek and next */
	t.test("peek() looks without moving", "a b", gives("a a | pos 0"), [] {
		std::vector<Token>	tokens = tokenize("a b");
		Cursor				cur{tokens, 0};
		std::string			first = cur.peek().text;
		std::string			second = cur.peek().text;

		return (first + " " + second + " | pos " + std::to_string(cur.pos));
	});
	t.test("next() returns the token, then moves on", "a b", gives("a b | then (end)"), [] {
		std::vector<Token>	tokens = tokenize("a b");
		Cursor				cur{tokens, 0};
		std::string			first = cur.next().text;
		std::string			second = cur.next().text;

		return (first + " " + second + " | then " + cursorAt(cur));
	});
	t.test("peek() at the end of the file throws", "a\nb\n\n", fails(2, "unexpected end of file"), [] {
		std::vector<Token>	tokens = tokenize("a\nb\n\n");
		Cursor				cur{tokens, 2};

		return (cur.peek().text);
	});
	t.test("end of file is reported on the last token's line", "a\n\n\nb # last", fails(4, "unexpected end of file"), [] {
		std::vector<Token>	tokens = tokenize("a\n\n\nb # last");
		Cursor				cur{tokens, 2};

		return (cur.peek().text);
	});
	t.test("an empty file reports end of file on line 1", "", fails(1, "unexpected end of file"), [] {
		std::vector<Token>	tokens = tokenize("");
		Cursor				cur{tokens, 0};

		return (cur.peek().text);
	});
	t.test("next() at the end of the file throws", "a", fails(1, "unexpected end of file"), [] {
		std::vector<Token>	tokens = tokenize("a");
		Cursor				cur{tokens, 0};

		cur.next();
		return (cur.next().text);
	});

	/* expect */
	t.test("expect() consumes the token it wanted", "{ x", gives("then x"), [] {
		std::vector<Token>	tokens = tokenize("{ x");
		Cursor				cur{tokens, 0};

		cur.expect("{");
		return ("then " + cursorAt(cur));
	});
	t.test("expect() names what it wanted and what it found", "server listen",
		fails(1, "expected '{' before 'listen'"), [] {
		std::vector<Token>	tokens = tokenize("server listen");
		Cursor				cur{tokens, 1};

		cur.expect("{");
		return (std::string("no error"));
	});
	t.test("expect() reports the line of the token it found", "server\n\nlisten",
		fails(3, "expected '{' before 'listen'"), [] {
		std::vector<Token>	tokens = tokenize("server\n\nlisten");
		Cursor				cur{tokens, 1};

		cur.expect("{");
		return (std::string("no error"));
	});
	t.test("expect() at the end of the file", "server", fails(1, "unexpected end of file"), [] {
		std::vector<Token>	tokens = tokenize("server");
		Cursor				cur{tokens, 1};

		cur.expect("{");
		return (std::string("no error"));
	});

	/*
	readArgs, with the cursor just past the directive's name - where
	parseServer calls it from. shows the arguments, then where the cursor
	ended up: just past the ';'.
	*/
	auto	args = [&t](const std::string &name, const std::string &text, size_t min, size_t max,
					const Expect &expect) {
		t.test(name, text, expect, [&] {
			std::vector<Token>	tokens = tokenize(text);
			Cursor				cur{tokens, 1};
			std::vector<Token>	found = cur.readArgs(tokens[0], min, max);

			return (tokenTexts(found) + " | then " + cursorAt(cur));
		});
	};
	const size_t	ANY = Cursor::NO_MAX;

	args("collects the arguments and consumes the ';'", "listen 80 ; next", 1, 1, gives("80 | then next"));
	args("several arguments", "methods GET POST DELETE ;", 1, ANY, gives("GET POST DELETE | then (end)"));
	args("no arguments, when min is 0", "internal ;", 0, 0, gives("(no tokens) | then (end)"));
	args("arguments may span lines", "methods\nGET\nPOST\n;", 1, ANY, gives("GET POST | then (end)"));
	t.test("every argument keeps its own line", "methods\nGET\nPOST ;", gives("GET@2 POST@3"), [] {
		std::vector<Token>	tokens = tokenize("methods\nGET\nPOST ;");
		Cursor				cur{tokens, 1};

		return (tokenLines(cur.readArgs(tokens[0], 1, Cursor::NO_MAX)));
	});
	args("too few arguments", "listen ;", 1, 1, fails(1, "invalid number of arguments in 'listen'"));
	// fixing handover §5.6 changes this one to: expected ';' before '81'
	args("too many arguments", "listen 80 81 ;", 1, 1, fails(1, "invalid number of arguments in 'listen'"));
	args("the count error points at the directive's line", "server_name\n\n;", 1, ANY,
		fails(1, "invalid number of arguments in 'server_name'"));
	args("'{' before ';' means the ';' was forgotten", "listen 80 {", 1, 1, fails(1, "expected ';' before '{'"));
	args("'}' before ';' means the ';' was forgotten", "listen 80 }", 1, 1, fails(1, "expected ';' before '}'"));
	args("the brace error is on the brace's line", "listen 80\n}", 1, 1, fails(2, "expected ';' before '}'"));
	args("end of file before the ';'", "listen 80", 1, 1, fails(1, "unexpected end of file"));

	t.known("handover §5.6 - readArgs blames the directive's line when a ';' is missing");
	args("a missing ';' is reported where the ';' belongs", "listen 0.0.0.0:8080\nserver_name broken.test;", 1, 1,
		fails(2, "expected ';' before 'server_name'"));
}
