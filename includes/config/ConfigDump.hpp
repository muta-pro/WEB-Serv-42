#ifndef CONFIGDUMP_HPP
#define CONFIGDUMP_HPP

#include <ostream>
#include <vector>

#include "ServerConfig.hpp"

/*
debug printer for the parser's output. prints every server and every
location with ALL fields, including the ones left at their defaults - the
point is to see exactly what the parser resolved, not what the file said.

	not part of the runtime pipeline. used by main() while the parser is
	being built, and by the test script to diff parsed output against an
	expected dump.
*/
void	dumpConfig(std::ostream &out, const std::vector<ServerConfig> &servers);

#endif
