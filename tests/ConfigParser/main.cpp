/*
webserv - config parser tests. one section per step of the parser plan
(docs/config-work-context.md §10). `make help` shows how to run them.

ADDING A TEST
	open the section of the step you're working on, sections/NN_name.hpp,
	and call one of the checks on t. every call is one test:

	t.test("a name", "what to show as input", gives("1024"), [] {
		return (std::to_string(parseSize(Token{"1k", 7})));
	});
	t.test("a name", "10X", fails(7, "unknown size suffix in '10X'"), [] { ... });

	t.parses("a name", "server { listen 80; }", {"servers = 1", "s0 listen = 0.0.0.0:80"});
	t.rejects("a name", "server {", 1, "unexpected end of file");
	t.parsesFile("conf/default.conf", {"servers = 1"});
	t.rejectsFile("conf/invalid_syntax.conf", 28, "expected ';' before 'server_name'");

	the body of t.test runs in a child process, so a crash or an endless
	loop fails that one test and the rest still run. a ConfigError thrown
	inside the body is compared as its what(), which is what fails() builds.

	facts for parses() are "key = value" lines.
		./tester --facts conf/default.conf
	prints every fact the parser makes of a file; copy the ones you care
	about. the keys are listed above configFacts() in framework/Checks.cpp.

	t.known("why") just before a check marks it as a known issue: it shows
	in yellow and isn't counted as a failure. once it passes, the summary
	says so, and you delete the known() line.

ADDING A SECTION (one per step)
	1. copy a section file to sections/NN_name.hpp. the name after the
	   number is the section's name: `make name` and `./tester name` run
	   it. use no '_' in the name itself
	2. rename its function, include the file below, and add it to main()

the section files are only included here, which is also why their
functions are `inline`: a section you comment out then won't trip
-Werror with an unused function.
*/
#include "parser_under_test.hpp"
#include "framework/Tester.hpp"

#include "sections/01_error.hpp"
#include "sections/02_tokenizer.hpp"
#include "sections/03_cursor.hpp"
#include "sections/04_skeleton.hpp"
#include "sections/05_values.hpp"
#include "sections/06_server.hpp"
#include "sections/07_locations.hpp"
#include "sections/08_finalize.hpp"
#include "sections/09_file.hpp"

int	main(int argc, char **argv)
{
	Tester	t(argc, argv);

	t.add(1, "error", "ConfigError: the message and the line", testError);
	t.add(2, "tokenizer", "text -> tokens", testTokenizer);
	t.add(3, "cursor", "atEnd, peek, next, expect, readArgs", testCursor);
	t.add(4, "skeleton", "the top level and server blocks", testSkeleton);
	t.add(5, "values", "numbers, listen, sizes, on/off, status codes, methods", testValues);
	t.add(6, "server", "the remaining server-level directives", testServer);
	t.add(7, "locations", "location blocks", testLocations);
	t.add(8, "finalize", "defaults, the / location, the 7 guarantees", testFinalize);
	t.add(9, "file", "parseFile: reading the file", testFile);
	// t.add(10, "corpus", "every file in conf/", testCorpus);
	return (t.run());
}

// make -C tests/ConfigParser      # from the repo root: build and run everything
// cd tests/ConfigParser
// make values                     # one section: error, tokenizer, cursor, skeleton, values, server, locations, finalize, file
// make quiet                      # one ✓ or ✗ per test; details only for failures
// make verbose                    # input, expected and got for every test
// make test ARGS="-f listen -x"   # only tests whose name contains "listen"; stop at the first failure
// make facts FILE=conf/default.conf   # what the parser makes of a file
// make help                       # everything else

