#include "http/HttpResponse.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <utility>
#include <ctime>

/*
suggested cahnge
validate setHeader()*/

std::string HttpResponse::reasonFor(int code)
{
	static const std::map<int, std::string> lookupTable{
		{200, "OK"},
		{201, "Created"},
		{204, "No Content"},
		{301, "Moved Permanently"},
		{302, "Found"},
		{303, "See Other"},
		{304, "Not Modified"},
		{307, "Temporary Redirect"},
		{308, "Permanent Redirect"},
		{400, "Bad Request"},
		{403, "Forbidden"},
		{404, "Not Found"},
		{405, "Method Not Allowed"},
		{408, "Request Timeout"},
		{409, "Conflict"},
		{411, "Length Required"},
		{413, "Content Too Large"},
		{414, "URI Too Long"},
		{431, "Request Header Fields Too Large"},
		{500, "Internal Server Error"},
		{501, "Not Implemented"},
		{502, "Bad Gateway"},
		{504, "Gateway Timeout"},
		{505, "HTTP Version Not Supported"}};
	auto it = lookupTable.find(code);
	if (it != lookupTable.end())
		return (it->second);
	else
		return ("");
}

HttpResponse HttpResponse::make(int code, std::string body)
{
	HttpResponse response;
	response.body = std::move(body);
	response.statusCode = code;
	// response.statusText = reasonFor(code); remove this line
	return response;
}

/*	the config file has an error_page directive. When that's wired up, makeError
	needs to first check whether the matched ServerConfig/LocationConfig
	specifies a custom error page file for that code, read it from disk,
	and only fall back to this generated page if there isn't one or it can't
	be read.
  suggestrion: separate reposnabilities for httpResponse ->
  ResponseBuilder:
  - reads ServerConfig
  - finds and loads custom error page
  - creates HttpResponse
HttpResponse:
- stores status/headers and body;
makeError remain generic fallback for now';
  */
static	std::string errorPage(int code)
{
	std::string defaultPage = R"(<html><body><h1>)" + std::to_string(code) + " "
		+ HttpResponse::reasonFor(code) + R"(</h1></body></html>)";
	return defaultPage;
}


/*	it doesn't set Connection: close. For 400, 413, and the 5xx family you
	generally want to close*/
HttpResponse HttpResponse::makeError(int code)
{
	HttpResponse response = make(code, errorPage(code));
	response.setHeader("Content-Type", "text/html");
	// response.statusCode = code;
	// response.statusText = reasonFor(code); remove this line fixed in toBytes();
	// response.body = errorPage(code);
	return response;
}

//added a basic guard line -ivan
//a complite HTTP header-name validator comes later-
//this throws then higher-level response builder eventually converts failures to a 500 Internal Server Error
void HttpResponse::setHeader(const std::string &name,const std::string &value)
{
  if (name.empty() || name.find_first_of(" \t\r\n:") != std::string::npos
    || value.find_first_of("\r\n") != std::string::npos) {
    throw std::invalid_argument("invalid HTTP header");
  }
	headers[name] = value;
}

std::string HttpResponse::getHeader(const std::string &name) const
{
	auto it = headers.find(name);
	if (it != headers.end())
		return (it->second);
	else
		return ("");
}

bool HttpResponse::hasHeader(const std::string &name) const
{
	auto it = headers.find(name);
	return it != headers.end();
}

static std::string httpDate(std::time_t when)
{
	std::tm *parts = std::gmtime(&when);
	char	buf[64];
	std::strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S GMT", parts);
	return std::string(buf);
}

/*	toBytes as of build path step 4 (2026-11-13).
	Done:
	1xx, 204 and 304 carry no body and no Content-Length; every other response
	gets exactly one Content-Length, counted from the body.
	Date (from httpDate, always GMT, RFC 9110 §6.6.1) and Server: webserv go on
	every response. Like Content-Length, they replace whatever the response
	already had, so a CGI script's own Date or Server never goes out.
	Still open:
	No Connection header. Keep-alive is decided from the request, so the caller
	has to setHeader("Connection", "keep-alive" | "close") before serializing;
	toBytes can't know. Who does it is to agree with Ivan.
	Header injection: setHeader throws std::invalid_argument on CR or LF in a
	value, and buildResponse's catch turns that into a 500. headers is a public
	map, though, and headers["X"] = ... skips that check: B's code only uses
	setHeader.
	By choice:
	No chunked transfer encoding. The whole body is in a std::string, so toBytes
	always knows its length. CGI output without a Content-Length is read to EOF
	first (subject §IV.3), then counted.
*/
std::string toBytes(const HttpResponse& response, bool headRequest)
{
	HeaderMap headers = response.headers;

	const bool stautsAllowaBody = !(response.statusCode >= 100 && response.statusCode < 200)
		&& response.statusCode != 204 && response.statusCode != 304;

	headers.erase("Content-Length");
	headers["Date"] = httpDate(std::time(nullptr));
	headers["Server"] = "webserv";
	if (stautsAllowaBody)
	{
		headers["Content-Length"] = std::to_string(response.body.size());
	}
//added here line - pervet statusCode/Text disagreement
//store only real info(Status code) then derive text during serialization;
	std::string reason = HttpResponse::reasonFor(response.statusCode);
	if (reason.empty())
		reason = "Unknown";

	std::string output;
	output += "HTTP/1.1 ";
	output += std::to_string(response.statusCode);
	output += " ";
	output += reason;
	output += "\r\n";

	for (const auto& header : headers)
	{
		output += header.first;
		output += ": ";
		output += header.second;
		output += "\r\n";
	}
	//if a response already has a Content-Length in its map (a CGI response might), you'd emit it twice. Later you can guard with if (!response.hasHeader("Content-Length"))
	// output += "Content-Length: ";
	// output += std::to_string(response.body.size());
	output += "\r\n";
	if (stautsAllowaBody && !headRequest) {
		output += response.body;
	}
	return output;
}

/* improvements from Ivan:
goal- make toBytes produce exactly one calculated Content-Length;

make a copy of a header map - remove any supplied lenght, insert the correct lenght;
I added the lines: so the result won't containt two values
that usually happens when CGI responses are added;

HeaderMap headers = response.headers;

headers.erase("Content-Lenght");
headers["Content-Lenght"] = std::to_string(response.body.size());

because server stores entire body in std::String, toBytes know the
final body size before serialization;

this guarantees exactly one Content-Lenght;
*/
