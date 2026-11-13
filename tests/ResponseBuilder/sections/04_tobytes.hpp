#pragma once

#include "Tester.hpp"
#include "../harness/Harness.hpp"
#include "http/HttpResponse.hpp"

#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <functional>
#include <string>
#include <utility>
#include <vector>

/*
STEP 4 - finish toBytes. decided 2026-11-13:
	Date      on every response, added by toBytes; the formatting lives in a
	          helper of Ravi's (its name and shape are his: these tests only
	          read toBytes' output)
	Server    "Server: webserv" on every response, added by toBytes
	phrases   303 See Other, 304 Not Modified, 307 Temporary Redirect,
	          308 Permanent Redirect; 413 becomes Content Too Large (RFC 9110)

	the groups:
	the tests' own date   the reference the Date tests compare against, checked first
	Date                  now, in GMT, whatever the server's time zone, exactly once
	Server                webserv, exactly once
	reason phrases        the four new ones, 413's new name, and the 19 that stay
	what toBytes did      Content-Length, HEAD and 204 rules, with Date and Server there

Date is an IMF-fixdate (RFC 9110 §5.6.7): "Fri, 13 Nov 2026 16:15:09 GMT".
always 29 bytes, always GMT, so it must come from gmtime, never localtime.
toBytes reads the clock, so a test can't know the exact second ahead: it
reads the clock just before and just after toBytes, and the Date must be
one of those two seconds.
*/

/*
what Date must say for the moment `when`. built by hand from gmtime_r's
fields, not with strftime, so it can't share a mistake with the code it
checks. the first four tests pin it.
*/
inline std::string	imfDate(std::time_t when)
{
	static const char *const	days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
	static const char *const	months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
									"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
	struct tm					t;
	char						text[64];

	gmtime_r(&when, &t);
	std::snprintf(text, sizeof(text), "%s, %02d %s %04d %02d:%02d:%02d GMT", days[t.tm_wday], t.tm_mday,
		months[t.tm_mon], t.tm_year + 1900, t.tm_hour, t.tm_min, t.tm_sec);
	return (text);
}

/* the header lines of the bytes called name (any case): their values, in order */
inline std::vector<std::string>	wireValues(const std::string &bytes, const std::string &name)
{
	std::vector<std::string>	values;
	size_t						end = bytes.find("\r\n\r\n");
	size_t						pos = bytes.find("\r\n");

	while (pos != std::string::npos && pos < end)
	{
		size_t		next = bytes.find("\r\n", pos + 2);
		std::string	line = bytes.substr(pos + 2, next - pos - 2);
		size_t		colon = line.find(':');
		bool		same = colon == name.size();

		for (size_t i = 0; same && i < colon; i++)
			same = std::tolower(static_cast<unsigned char>(line[i])) == std::tolower(static_cast<unsigned char>(name[i]));
		if (same)
			values.push_back(line.substr(colon + (line.size() > colon + 1 && line[colon + 1] == ' ' ? 2 : 1)));
		pos = next;
	}
	return (values);
}

/* toBytes(response) between two looks at the clock: "now, in GMT" if Date is one of those seconds */
inline std::string	dateIsNow(const HttpResponse &response, bool head)
{
	std::time_t					before = std::time(nullptr);
	std::string					bytes = toBytes(response, head);
	std::time_t					after = std::time(nullptr);
	std::vector<std::string>	dates = wireValues(bytes, "Date");

	if (dates.empty())
		return (std::string("no Date line"));
	if (dates.size() > 1)
		return (std::to_string(dates.size()) + " Date lines");
	if (dates[0] != imfDate(before) && dates[0] != imfDate(after))
		return ("Date: " + dates[0] + "   (now is " + imfDate(after) + ")");
	return (std::string("Date: now, in GMT"));
}

inline void	testToBytes(Tester &t)
{
	/* ------------------------------------------------ the tests' own date */

	const std::vector<std::pair<std::time_t, std::string>>	known = {
		{0, "Thu, 01 Jan 1970 00:00:00 GMT"},
		{951782400, "Tue, 29 Feb 2000 00:00:00 GMT"},
		{1794586509, "Fri, 13 Nov 2026 16:15:09 GMT"},
		{4102444799, "Thu, 31 Dec 2099 23:59:59 GMT"}};

	for (const std::pair<std::time_t, std::string> &k : known)
		t.test("the tests' reference date: " + k.second, "imfDate(" + std::to_string(k.first) + ")",
			gives(k.second), [k] {
			return (imfDate(k.first));
		});

	/* ------------------------------------------------ Date */

	const std::vector<std::pair<std::string, std::function<HttpResponse()>>>	kinds = {
		{"a 200", [] { return (HttpResponse::make(200, "hello")); }},
		{"a 404 page", [] { return (HttpResponse::makeError(404)); }},
		{"a 500 page", [] { return (HttpResponse::makeError(500)); }},
		{"a 204, which has no body", [] { return (HttpResponse::make(204)); }}};

	for (const std::pair<std::string, std::function<HttpResponse()>> &kind : kinds)
		t.test("Date on " + kind.first + ": now, in GMT", "toBytes(" + kind.first + ", false)",
			gives("Date: now, in GMT"), [kind] {
			return (dateIsNow(kind.second(), false));
		});
	t.test("Date on a HEAD response: now, in GMT", "toBytes(make(200, \"hello\"), true)",
		gives("Date: now, in GMT"), [] {
		return (dateIsNow(HttpResponse::make(200, "hello"), true));
	});
	/*
	"XYZ-14" is a POSIX time zone 14 hours ahead of GMT (the sign is the other
	way round in POSIX). at least half of every day, localtime() there gives a
	different date than gmtime(), and always a different hour.
	*/
	t.test("Date is GMT even where the server's clock is 14 hours ahead", "TZ=XYZ-14 · toBytes(make(200), false)",
		gives("Date: now, in GMT"), [] {
		setenv("TZ", "XYZ-14", 1);
		tzset();

		std::time_t	now = std::time(nullptr);
		struct tm	local;
		struct tm	gmt;

		localtime_r(&now, &local);
		gmtime_r(&now, &gmt);
		if (local.tm_hour == gmt.tm_hour)
			return (std::string("the time zone didn't take: localtime and gmtime agree"));
		return (dateIsNow(HttpResponse::make(200, "hello"), false));
	});
	/* decided 2026-11-13: toBytes replaces a Date the response already has, as it does Content-Length */
	t.test("a Date set by hand is replaced: one line, now, in GMT", "make(200) · setHeader(\"Date\", \"yesterday\")",
		gives("Date: now, in GMT"), [] {
		HttpResponse	response = HttpResponse::make(200, "hello");

		response.setHeader("Date", "yesterday");
		return (dateIsNow(response, false));
	});

	/* ------------------------------------------------ Server */

	for (const std::pair<std::string, std::function<HttpResponse()>> &kind : kinds)
		t.test("Server: webserv on " + kind.first, "toBytes(" + kind.first + ", false)",
			gives("Server: webserv"), [kind] {
			std::vector<std::string>	servers = wireValues(toBytes(kind.second(), false), "Server");

			if (servers.size() != 1)
				return (std::to_string(servers.size()) + " Server lines");
			return ("Server: " + servers[0]);
		});
	/* a CGI script may print its own Server line (step 9). decided 2026-11-13: toBytes's wins */
	t.test("a Server set by hand is replaced: one line, webserv", "make(200) · setHeader(\"Server\", \"Python/3.12\")",
		gives("Server: webserv"), [] {
		HttpResponse	response = HttpResponse::make(200, "hello");

		response.setHeader("Server", "Python/3.12");
		std::vector<std::string>	servers = wireValues(toBytes(response, false), "Server");

		if (servers.size() != 1)
			return (std::to_string(servers.size()) + " Server lines");
		return ("Server: " + servers[0]);
	});

	/* ------------------------------------------------ reason phrases */

	const std::vector<std::pair<int, std::string>>	added = {
		{303, "See Other"}, {304, "Not Modified"}, {307, "Temporary Redirect"},
		{308, "Permanent Redirect"}, {413, "Content Too Large"}};

	for (const std::pair<int, std::string> &p : added)
		t.test("reasonFor(" + std::to_string(p.first) + "): " + p.second, "HttpResponse::reasonFor(" + std::to_string(p.first) + ")",
			gives(p.second), [p] {
			return (HttpResponse::reasonFor(p.first));
		});
	t.test("307 on the wire: HTTP/1.1 307 Temporary Redirect", "toBytes(make(307), false)",
		gives("HTTP/1.1 307 Temporary Redirect"), [] {
		std::string	bytes = toBytes(HttpResponse::make(307), false);

		return (bytes.substr(0, bytes.find("\r\n")));
	});
	t.test("the built-in 413 page says Content Too Large", "HttpResponse::makeError(413).body",
		gives("<html><body><h1>413 Content Too Large</h1></body></html>"), [] {
		return (HttpResponse::makeError(413).body);
	});
	t.test("the 19 phrases that were already right stay as they are", "HttpResponse::reasonFor(...)",
		gives("all 19 unchanged"), [] {
		const std::vector<std::pair<int, std::string>>	kept = {
			{200, "OK"}, {201, "Created"}, {204, "No Content"}, {301, "Moved Permanently"}, {302, "Found"},
			{400, "Bad Request"}, {403, "Forbidden"}, {404, "Not Found"}, {405, "Method Not Allowed"},
			{408, "Request Timeout"}, {409, "Conflict"}, {411, "Length Required"}, {414, "URI Too Long"},
			{431, "Request Header Fields Too Large"}, {500, "Internal Server Error"}, {501, "Not Implemented"},
			{502, "Bad Gateway"}, {504, "Gateway Timeout"}, {505, "HTTP Version Not Supported"}};
		std::string	changed;

		for (const std::pair<int, std::string> &p : kept)
			if (HttpResponse::reasonFor(p.first) != p.second)
				changed += (changed.empty() ? "" : ", ") + std::to_string(p.first) + " is \""
					+ HttpResponse::reasonFor(p.first) + "\"";
		return (changed.empty() ? std::string("all 19 unchanged") : changed);
	});

	/* ------------------------------------------------ what toBytes did, with the new headers there */

	auto	wireFacts = [&t](const std::string &name, const std::string &input, std::function<HttpResponse()> make,
						const std::string &line, const std::vector<std::string> &facts) {
		std::string	expected;

		for (const std::string &fact : facts)
			expected += (expected.empty() ? "" : "\n") + fact;
		t.test(name, input, gives(expected), [make, line, facts] {
			HttpResponse	response = make();
			HttpRequest		req = request(line);
			std::string		got;

			for (const std::string &fact : facts)
			{
				std::string	key = fact.substr(0, fact.find(" = "));
				got += (got.empty() ? "" : "\n") + key + " = " + responseFact(key, response, req, nullptr);
			}
			return (got);
		});
	};

	wireFacts("204: Date and Server, still no Content-Length and no body", "DELETE /x · make(204, \"ignored\")",
		[] { return (HttpResponse::make(204, "ignored")); }, "DELETE /x",
		{"wire = HTTP/1.1 204 No Content", "wire header Server = webserv",
		 "wire header Content-Length = (none)", "wire body size = 0"});
	wireFacts("HEAD: Content-Length stays, the bytes go", "HEAD / · make(200, \"hello\")",
		[] { return (HttpResponse::make(200, "hello")); }, "HEAD /",
		{"wire header Server = webserv", "wire header Content-Length = 5", "wire body size = 0"});
	wireFacts("a Content-Length set by hand still loses to the real one", "make(200, \"hello\") · setHeader(\"Content-Length\", \"999\")",
		[] {
			HttpResponse	response = HttpResponse::make(200, "hello");

			response.setHeader("Content-Length", "999");
			return (response);
		}, "GET /",
		{"wire header Content-Length = 5", "wire body size = 5"});
	wireFacts("the response's own headers still go out", "make(301) · setHeader(\"Location\", \"/new/\")",
		[] {
			HttpResponse	response = HttpResponse::make(301);

			response.setHeader("Location", "/new/");
			return (response);
		}, "GET /old",
		{"wire = HTTP/1.1 301 Moved Permanently", "wire header Location = /new/", "wire header Server = webserv"});
}
