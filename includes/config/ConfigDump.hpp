#ifndef CONFIGDUMP_HPP
#define CONFIGDUMP_HPP

#include <ostream>
#include <vector>

#include "ServerConfig.hpp"

/*
debug printer for the parser's output. prints every server and every
location with ALL fields, including the ones left at their defaults - the
point is to see exactly what the parser resolved, not what the file said.

	not part of the runtime pipeline. main.cpp is A's (network & runtime),
	so whether and when main calls this - say, behind a debug flag - is
	theirs to decide. the parser's own tests don't need it: they compare
	facts, see tests/ConfigParser/framework/Checks.cpp.
*/
void	dumpConfig(std::ostream &out, const std::vector<ServerConfig> &servers);

#endif
