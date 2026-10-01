#include "config/ConfigDump.hpp"

/*
TODO
	- one header line per server: index, listens, serverNames
	- server fields: clientMaxBody, errorPages (code -> uri)
	- one indented block per location, in declaration order, every field
	- print empty strings visibly (e.g. "") so "unset" is distinguishable
*/
void dumpConfig(std::ostream &out, const std::vector<ServerConfig> &servers)
{
	(void)out;
	(void)servers;
}
