#pragma once

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <system_error>

#include "../framework/Tester.hpp"
#include "../parser_under_test.hpp"

/*
STEP 2 - the tokenizer: file text -> tokens, each with its line.
	whitespace separates tokens; '{', '}' and ';' are always tokens of their
	own; '#' starts a comment that runs to the end of the line, even in the
	middle of a word.
*/

/* every .conf under conf/, sorted, as "conf/default.conf" */
inline std::vector<std::string>	confFiles()
{
	std::vector<std::string>	files;
	std::error_code				error;
	const std::string			root = Tester::repoPath("conf");

	for (std::filesystem::recursive_directory_iterator it(root, error), end; !error && it != end; it.increment(error))
		if (it->path().extension() == ".conf")
			files.push_back("conf/" + std::filesystem::relative(it->path(), root, error).string());
	std::sort(files.begin(), files.end());
	return (files);
}

inline void	testTokenizer(Tester &t)
{
	/* the token texts tokenize() makes of text */
	auto	tokens = [&t](const std::string &name, const std::string &text, const std::string &expected) {
		t.test(name, text, gives(expected), [&] {
			return (tokenTexts(tokenize(text)));
		});
	};
	/* the same, with the line of every token: "listen@1 80@2" */
	auto	lines = [&t](const std::string &name, const std::string &text, const std::string &expected) {
		t.test(name, text, gives(expected), [&] {
			return (tokenLines(tokenize(text)));
		});
	};
	/* the tokens tokenize() finds on one line of a file in the repo */
	auto	onLine = [&t](const std::string &path, size_t line, const std::string &expected) {
		t.test(path + " line " + std::to_string(line), "file " + path, gives(expected), [&] {
			std::vector<Token>	here;

			for (const Token &token : tokenize(Tester::readFile(Tester::repoPath(path))))
				if (token.line == line)
					here.push_back(token);
			return (tokenTexts(here));
		});
	};

	/* whitespace */
	tokens("words are split on spaces", "listen 8080;", "listen 8080 ;");
	tokens("a run of whitespace counts as one separator", "listen  \t  8080   ;", "listen 8080 ;");
	tokens("tabs separate tokens", "listen\t\t8080;", "listen 8080 ;");
	tokens("\\r is whitespace, so CRLF files work", "listen 80;\r\nroot x;\r\n", "listen 80 ; root x ;");
	tokens("\\v and \\f are whitespace too", "a\vb\fc", "a b c");
	tokens("leading and trailing whitespace is ignored", "   \t listen 80;  \n\n", "listen 80 ;");

	/* { } ; are always tokens of their own */
	tokens("';' glued to words is its own token", "a;b", "a ; b");
	tokens("'{' glued to a word is its own token", "location /a/{root ./www/html;}",
		"location /a/ { root ./www/html ; }");
	tokens("'}' glued to a word is its own token", "internal;}", "internal ; }");
	tokens("back-to-back { } ; are separate tokens", "{};;}", "{ } ; ; }");
	tokens("a whole server on one line", "server{listen 127.0.0.1:9090;location /{methods GET;}}",
		"server { listen 127.0.0.1:9090 ; location / { methods GET ; } }");

	/* comments */
	tokens("a comment runs to the end of the line", "listen 80; # the port\nroot x;", "listen 80 ; root x ;");
	tokens("a glued comment ends the token before it", "listen 0.0.0.0:8080;# comment", "listen 0.0.0.0:8080 ;");
	tokens("'#' in the middle of a word starts a comment", "abc#def ghi\nnext", "abc next");
	tokens("{ } ; inside a comment are ignored", "a # { } ; b\nc", "a c");
	tokens("a comment at the very end, with no newline", "a # the end", "a");
	tokens("a file of only comments has no tokens", "# one\n# two\n", "(no tokens)");

	/* odd input */
	tokens("an empty file has no tokens", "", "(no tokens)");
	tokens("only whitespace has no tokens", "  \n\t\n  \r\n", "(no tokens)");
	tokens("no newline at the end of the file", "server{}", "server { }");
	tokens("non-ASCII bytes stay inside their token", "server_name café.test;", "server_name café.test ;");
	lines("quotes are not special: \"a b\" is two tokens", "server_name \"a b\";",
		"server_name@1 \"a@1 b\"@1 ;@1");

	/* line numbers */
	lines("the first line is line 1", "listen 80;", "listen@1 80@1 ;@1");
	lines("every newline moves to the next line", "a\nb\nc", "a@1 b@2 c@3");
	lines("blank lines are counted", "a\n\n\nb", "a@1 b@4");
	lines("a comment does not swallow its newline", "a # note\nb", "a@1 b@2");
	lines("comment-only lines are counted", "# one\n# two\nb", "b@3");
	lines("a word right before a newline keeps its line", "abc\ndef\n", "abc@1 def@2");
	lines("{ } ; get the line they are on", "a\n{\n}\n;", "a@1 {@2 }@3 ;@4");
	lines("CRLF counts one line per \\r\\n", "a\r\nb\r\n\r\nc", "a@1 b@2 c@4");
	lines("one directive spread over three lines", "listen\n80\n;", "listen@1 80@2 ;@3");

	/* conf/edge_cases.conf exists to pin the tokenizer down */
	onLine("conf/edge_cases.conf", 1, "(no tokens)");
	onLine("conf/edge_cases.conf", 16, "listen 0.0.0.0:8080 ;");
	onLine("conf/edge_cases.conf", 18, "server_name edge.test ;");
	onLine("conf/edge_cases.conf", 29, "location / {");
	onLine("conf/edge_cases.conf", 33, "location /a/ { root ./www/html ; methods GET ; }");
	onLine("conf/edge_cases.conf", 38, "methods GET POST ;");
	onLine("conf/edge_cases.conf", 44,
		"server { listen 127.0.0.1:9090 ; root ./www/html ; index index.html ; location / { methods GET ; } }");

	/* every config in the repo: no token holds whitespace or '#', and { } ; are never glued */
	const std::vector<std::string>	files = confFiles();

	t.test("conf/ holds the test configs", "directory conf/", gives("yes"), [&files] {
		return (std::string(files.empty() ? "no .conf file found" : "yes"));
	});
	for (const std::string &path : files)
		t.test(path + ": every token is clean", "file " + path, gives("clean"), [&path] {
			for (const Token &token : tokenize(Tester::readFile(Tester::repoPath(path))))
			{
				bool	spaced = false;
				for (char c : token.text)
					if (std::isspace(static_cast<unsigned char>(c)))
						spaced = true;
				bool	glued = token.text.size() > 1 && token.text.find_first_of("{};") != std::string::npos;
				if (token.text.empty() || spaced || glued || token.text.find('#') != std::string::npos)
					return ("line " + std::to_string(token.line) + ": bad token '" + token.text + "'");
			}
			return (std::string("clean"));
		});
}
