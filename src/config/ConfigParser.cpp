#include "config/ConfigParser.hpp"

ConfigError::ConfigError(size_t line, const std::string &message)
	: std::runtime_error( ), _line()
{
}

size_t ConfigError::line() const
{
	return ();
}
