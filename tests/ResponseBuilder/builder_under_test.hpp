#pragma once

/*
the response builder's own source file, compiled INTO the test program.

	ResponseBuilder.cpp will keep most of its parts to itself: a handler per
	RouteAction, the MIME table, the folder listing... static functions that
	no header can reach. including the .cpp here puts all of it into this
	translation unit, so a section can test one helper on its own instead
	of only through buildResponse(). the config suite does the same with
	the parser (tests/ConfigParser/parser_under_test.hpp).

	the catch: ResponseBuilder.cpp must NOT also be compiled separately into
	the tester, or everything in it would be defined twice. the Makefile
	leaves it out, and main.cpp is the only file that includes this one.

	a new B file (a MimeTypes.cpp, the CGI output parser...): include it
	here too when a test needs its static parts, or add it to REPO_SRCS in
	the Makefile when its public functions are enough.
*/
#include "../../src/http/ResponseBuilder.cpp"
