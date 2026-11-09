/*
webserv - response builder tests. one section per step of the build path
(docs/http-response-context.md §6). `make help` shows how to run them.

A TEST IS A TRIAGE CARD
	the Lab's Triage cards each give a request, the route C hands over, the
	facts on disk and the answer. a test says the same, with a Scene:

	responds(t, "t2 · a missing file is 404",
		Scene("GET /logo.png").route(RouteAction::StaticFile, "/"),
		{"status = 404"});

	responds(t, "t3 · a file the server may not read is 403",
		Scene("GET /secret.txt").route(RouteAction::StaticFile, "/")
			.file("www/html/secret.txt", "s", 0000),
		{"status = 403"});

	responds(t, "t5 · a folder without its slash redirects, query kept",
		Scene("GET /files/docs?sort=name").route(RouteAction::Directory, "/files/")
			.dir("www/files/docs"),
		{"status = 301", "header Location = /files/docs/?sort=name"});

	the scene runs in the test's own process, in a sandbox folder of its
	own: its files are made, conf/default.conf is parsed, the route points
	into it, and buildResponse gets the request and the route. then every
	fact must hold. the facts a test can name are listed above responds()
	in harness/Harness.hpp; route() names the location block the router
	matched, exactly as the config writes it.

	for anything a scene doesn't cover (two requests in a row, a helper of
	ResponseBuilder.cpp on its own), use the four pieces directly in a
	t.test. 01_harness.hpp has examples:

	t.test("a name", "what to show as input", gives("204 then 404"), [] {
		Sandbox      box;
		Site         site(defaultConfig());
		HttpRequest  req = request("DELETE /upload/cat.png");
		RouteResult  route = site.route(RouteAction::DeleteResource, req, "/upload/");
		...
	});

	t.known("why") just before a check marks it as a known issue: it shows
	in yellow and isn't counted as a failure. once it passes, the summary
	says so, and you delete the known() line.

ADDING A SECTION (one per step)
	1. copy a section file to sections/NN_name.hpp, NN the step number. the
	   name after the number is the section's name: `make name` and
	   `./tester name` run it. use no '_' in the name itself
	2. rename its function, include the file below, and add it to main()

the section files are only included here, which is also why their
functions are `inline`: a section you comment out then won't trip -Werror
with an unused function.

the runner is the config suite's (tests/ConfigParser/framework): the same
options, colours and summary. only the checks of harness/ are new.
*/
#include "builder_under_test.hpp"
#include "Tester.hpp"
#include "harness/Harness.hpp"

#include "sections/01_harness.hpp"

static const char	*const EXAMPLES =
	"    ./tester                                   everything\n"
	"    ./tester harness -f sandbox                only the sandbox tests of step 1\n"
	"    ./tester 1 -q                              step 1, compact\n"
	"    ./tester -f t3 -v                          triage card t3, input and got\n"
	"    ./tester --facts conf/default.conf         the location blocks a scene can name\n";

int	main(int argc, char **argv)
{
	Tester	t(argc, argv);

	t.suite("response builder tests", EXAMPLES);
	t.add(1, "harness", "B's inputs, faked: request, config + route, disk; buildResponse links", testHarness);
	Sandbox::clearAll();
	return (t.run());
}

// make -C tests/ResponseBuilder      # from the repo root: build and run everything
// cd tests/ResponseBuilder
// make harness                       # one section
// make quiet                         # one ✓ or ✗ per test; details only for failures
// make verbose                       # input, expected and got for every test
// make test ARGS="-f t3 -x"          # only tests whose name contains "t3"; stop at the first failure
// make facts FILE=conf/default.conf  # the locations and roots a scene can use
// make help                          # everything else
