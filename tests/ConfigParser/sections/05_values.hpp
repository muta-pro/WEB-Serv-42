#pragma once

#include <limits>

#include "../framework/Tester.hpp"
#include "../parser_under_test.hpp"

/*
STEP 5 - the value parsers. each is called with a Token on line 7, so
every error below also proves the parser reports the token's own line.
*/
inline void	testValues(Tester &t)
{
	/* parseNumber(token, digits, max): digits only, refused above max before it can overflow */
	auto	number = [&t](const std::string &digits, size_t max, const Expect &expect) {
		t.test("parseNumber(\"" + digits + "\", max " + std::to_string(max) + ")", digits, expect, [&] {
			return (std::to_string(parseNumber(Token{digits, 7}, digits, max)));
		});
	};
	const size_t	BIGGEST = std::numeric_limits<size_t>::max();

	number("0", 999, gives("0"));
	number("42", 999, gives("42"));
	number("00080", 65535, gives("80"));
	number("65535", 65535, gives("65535"));
	number("65536", 65535, fails(7, "number too large in '65536'"));
	number("99999999999999999999999", 65535, fails(7, "number too large in '99999999999999999999999'"));
	number("18446744073709551615", BIGGEST, gives("18446744073709551615"));
	number("18446744073709551616", BIGGEST, fails(7, "number too large in '18446744073709551616'"));
	number("", 999, fails(7, "invalid number in ''"));
	number("8a", 999, fails(7, "invalid number in '8a'"));
	number("-1", 999, fails(7, "invalid number in '-1'"));
	number("+1", 999, fails(7, "invalid number in '+1'"));
	number("1.5", 999, fails(7, "invalid number in '1.5'"));
	number(" 1", 999, fails(7, "invalid number in ' 1'"));
	t.test("an error names the whole token, not just its digits", "token 0.0.0.0:80x, digits 80x",
		fails(7, "invalid number in '0.0.0.0:80x'"), [] {
			return (std::to_string(parseNumber(Token{"0.0.0.0:80x", 7}, "80x", 65535)));
		});

	/* parseListen: "host:port" or "port"; any non-empty host (open decision 2) */
	auto	listen = [&t](const std::string &text, const Expect &expect) {
		t.test("parseListen(\"" + text + "\")", text, expect, [&] {
			Listen	result = parseListen(Token{text, 7});

			return (result.host + ":" + std::to_string(result.port));
		});
	};
	listen("8080", gives("0.0.0.0:8080"));
	listen("0.0.0.0:8080", gives("0.0.0.0:8080"));
	listen("127.0.0.1:65535", gives("127.0.0.1:65535"));
	listen("localhost:80", gives("localhost:80"));
	listen("1", gives("0.0.0.0:1"));
	listen("0", fails(7, "port 0 is not allowed in '0'"));
	listen("0.0.0.0:0", fails(7, "port 0 is not allowed in '0.0.0.0:0'"));
	listen("65536", fails(7, "number too large in '65536'"));
	listen(":8080", fails(7, "missing host in ':8080'"));
	listen("::", fails(7, "missing host in '::'"));
	listen("localhost:", fails(7, "invalid number in 'localhost:'"));
	listen("8080abc", fails(7, "invalid number in '8080abc'"));
	listen("eighty", fails(7, "invalid number in 'eighty'"));
	listen("-80", fails(7, "invalid number in '-80'"));
	listen("+80", fails(7, "invalid number in '+80'"));
	listen("1.2.3.4:80:90", fails(7, "invalid number in '1.2.3.4:80:90'"));
	t.parses("every listen line is kept, in order",
		"server {\n\tlisten 80;\n\tlisten 127.0.0.1:81;\n\tlisten localhost:8080;\n}",
		{"s0 listen = 0.0.0.0:80 127.0.0.1:81 localhost:8080"});
	t.rejects("a bad port is reported on its own line", "server {\n\tlisten 80;\n\tlisten 0.0.0.0:99999;\n}", 3,
		"number too large in '0.0.0.0:99999'");

	/* parseSize: digits with an optional k/m/g suffix, 1k = 1024 */
	auto	size = [&t](const std::string &text, const Expect &expect) {
		t.test("parseSize(\"" + text + "\")", text, expect, [&] {
			return (std::to_string(parseSize(Token{text, 7})));
		});
	};
	size("0", gives("0"));	// what 0 should mean is open decision 7
	size("1", gives("1"));
	size("512", gives("512"));
	size("1k", gives("1024"));
	size("1K", gives("1024"));
	size("512k", gives("524288"));
	size("1m", gives("1048576"));
	size("10M", gives("10485760"));
	size("1G", gives("1073741824"));
	size("2g", gives("2147483648"));
	size("18446744073709551615", gives("18446744073709551615"));
	size("18014398509481983K", gives("18446744073709550592"));	// the biggest that fits with K
	size("18014398509481984K", fails(7, "number too large in '18014398509481984K'"));
	size("17179869183G", gives("18446744072635809792"));		// the biggest that fits with G
	size("17179869184G", fails(7, "number too large in '17179869184G'"));
	size("10X", fails(7, "unknown size suffix in '10X'"));
	size("10MB", fails(7, "unknown size suffix in '10MB'"));
	size("10b", fails(7, "unknown size suffix in '10b'"));
	size("k", fails(7, "invalid number in 'k'"));
	size("10%", fails(7, "invalid number in '10%'"));
	size("1.5M", fails(7, "invalid number in '1.5M'"));
	size("-1M", fails(7, "invalid number in '-1M'"));
	size("M10", fails(7, "invalid number in 'M10'"));

	/* parseOnOff: on/ON and off/OFF only */
	auto	onOff = [&t](const std::string &text, const Expect &expect) {
		t.test("parseOnOff(\"" + text + "\")", text, expect, [&] {
			return (std::string(parseOnOff(Token{text, 7}) ? "true" : "false"));
		});
	};
	onOff("on", gives("true"));
	onOff("ON", gives("true"));
	onOff("off", gives("false"));
	onOff("OFF", gives("false"));
	onOff("On", fails(7, "expected 'on' or 'off', not 'On'"));
	onOff("oFF", fails(7, "expected 'on' or 'off', not 'oFF'"));
	onOff("yes", fails(7, "expected 'on' or 'off', not 'yes'"));
	onOff("true", fails(7, "expected 'on' or 'off', not 'true'"));
	onOff("1", fails(7, "expected 'on' or 'off', not '1'"));

	/* parseStatusCode(token, lo, hi): 300-599 for error_page, 300-399 for return */
	auto	code = [&t](const std::string &text, int lo, int hi, const Expect &expect) {
		t.test("parseStatusCode(\"" + text + "\", " + std::to_string(lo) + ", " + std::to_string(hi) + ")", text,
			expect, [&] {
				return (std::to_string(parseStatusCode(Token{text, 7}, lo, hi)));
			});
	};
	code("300", 300, 599, gives("300"));
	code("404", 300, 599, gives("404"));
	code("599", 300, 599, gives("599"));
	code("299", 300, 599, fails(7, "status code '299' is not between 300 and 599"));
	code("600", 300, 599, fails(7, "status code '600' is not between 300 and 599"));
	code("200", 300, 599, fails(7, "status code '200' is not between 300 and 599"));
	code("1000", 300, 599, fails(7, "number too large in '1000'"));
	code("40x", 300, 599, fails(7, "invalid number in '40x'"));
	code("301", 300, 399, gives("301"));
	code("399", 300, 399, gives("399"));
	code("400", 300, 399, fails(7, "status code '400' is not between 300 and 399"));
	code("404", 300, 399, fails(7, "status code '404' is not between 300 and 399"));

	/* parseMethods: GET HEAD POST DELETE, case-sensitive. the args come from tokenize() */
	auto	methods = [&t](const std::string &text, const Expect &expect) {
		t.test("parseMethods(" + text + ")", text, expect, [&] {
			return (methodsText(parseMethods(tokenize(text))));
		});
	};
	methods("GET", gives("GET"));
	methods("GET POST DELETE", gives("GET POST DELETE"));
	methods("DELETE POST GET", gives("GET POST DELETE"));	// a set: the order doesn't matter
	methods("HEAD", gives("HEAD"));							// open decision 5: HEAD may be written out
	methods("GET HEAD", gives("GET HEAD"));
	methods("GET GET GET", gives("GET"));					// repeats are harmless
	methods("get", fails(1, "unknown method 'get'"));		// RFC 9110 9.1: methods are case-sensitive
	methods("Post", fails(1, "unknown method 'Post'"));
	methods("GET PUT", fails(1, "unknown method 'PUT'"));
	methods("OPTIONS", fails(1, "unknown method 'OPTIONS'"));
	t.test("a bad method is reported on its own line", "GET\nPOST\nPATCH",
		fails(3, "unknown method 'PATCH'"), [] {
			return (methodsText(parseMethods(tokenize("GET\nPOST\nPATCH"))));
		});
}
