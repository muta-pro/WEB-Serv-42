#pragma once

#include "../framework/Tester.hpp"
#include "../parser_under_test.hpp"

/*
STEP 1 - ConfigError, the one exception the parser throws. main() prints
what() exactly as it is, so its format is part of the contract:
	config error, line N: <message>
*/
inline void	testError(Tester &t)
{
	t.test("what() reads 'config error, line N: message'",
		"ConfigError(28, \"expected ';' before 'server_name'\")",
		gives("config error, line 28: expected ';' before 'server_name'"),
		[] {
			return (std::string(ConfigError(28, "expected ';' before 'server_name'").what()));
		});

	t.test("line() returns the line it was given", "ConfigError(28, \"x\").line()",
		gives("28"),
		[] {
			return (std::to_string(ConfigError(28, "x").line()));
		});

	t.test("big line numbers are printed in full", "ConfigError(1000000, \"x\")",
		gives("config error, line 1000000: x"),
		[] {
			return (std::string(ConfigError(1000000, "x").what()));
		});

	t.test("an empty message keeps the prefix", "ConfigError(3, \"\")",
		gives("config error, line 3: "),
		[] {
			return (std::string(ConfigError(3, "").what()));
		});

	t.test("it can be caught as a std::runtime_error", "throw ConfigError(5, \"boom\")",
		gives("config error, line 5: boom"),
		[] {
			try
			{
				throw ConfigError(5, "boom");
			}
			catch (const std::runtime_error &e)
			{
				return (std::string(e.what()));
			}
			return (std::string("not caught"));
		});

	t.test("it can be caught as a std::exception", "throw ConfigError(6, \"boom\")",
		gives("config error, line 6: boom"),
		[] {
			try
			{
				throw ConfigError(6, "boom");
			}
			catch (const std::exception &e)
			{
				return (std::string(e.what()));
			}
			return (std::string("not caught"));
		});

	t.test("a copy keeps the message and the line", "ConfigError copy(ConfigError(9, \"copied\"))",
		gives("config error, line 9: copied | line() 9"),
		[] {
			ConfigError	original(9, "copied");
			ConfigError	copy(original);

			return (std::string(copy.what()) + " | line() " + std::to_string(copy.line()));
		});

	t.test("from the parser, line() matches the line in what()", "}",
		gives("line() 1 | config error, line 1: unexpected '}'"),
		[] {
			try
			{
				ConfigParser::parseString("}");
			}
			catch (const ConfigError &e)
			{
				return ("line() " + std::to_string(e.line()) + " | " + e.what());
			}
			return (std::string("no error thrown"));
		});
}
