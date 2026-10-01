#include "config/ConfigParser.hpp"

ConfigError::ConfigError(size_t line, const std::string &message)
	: std::runtime_error("config error, line " + std::to_string(line) + ": " + message), _line(line)
{
}

size_t ConfigError::line() const
{
	return (_line);
}
