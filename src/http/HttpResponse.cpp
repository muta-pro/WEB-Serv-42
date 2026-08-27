#include "http/HttpResponse.hpp"
#include <iostream>

std::string HttpResponse::reasonFor(int code)
{
	static const std::map<int, std::string> lookupTable{
		{200, "OK"},
		{201, "Created"},
		{204, "No Content"},
		{301, "Moved Permanently"},
		{302, "Found"},
		{400, "Bad Request"},
		{403, "Forbidden"},
		{404, "Not Found"},
		{405, "Method Not Allowed"},
		{408, "Request Timeout"},
		{409, "Conflict"},
		{411, "Length Required"},
		{413, "Payload Too Large"},
		{414, "URI Too Long"},
		{431, "Header Block Too Large"},
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
	response.statusText = reasonFor(code);
	return response;
}

/*	the config file has an error_page directive. When that's wired up, makeError
	needs to first check whether the matched ServerConfig/LocationConfig
	specifies a custom error page file for that code, read it from disk,
	and only fall back to this generated page if there isn't one or it can't
	be read.*/
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
	HttpResponse response;
	response.statusCode = code;
	response.statusText = reasonFor(code);
	response.body = errorPage(code);
	response.setHeader("Content-Type", "text/html");
	return response;
}

void HttpResponse::setHeader(const std::string &name,const std::string &value)
{
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

/*	Gaps, needs to be fixed down the line:
	204 and 304 must not carry a body or a Content-Length. Right now a make(204)
	still emits Content-Length: 0. Some clients tolerate it, some don't.
	No Date header. RFC 7231 requires origin servers with a clock to send one.
	Cheap to add: strftime with "%a, %d %b %Y %H:%M:%S GMT" on gmtime.
	No Server header. Optional, but conventional and free.
	No Connection header. Keep-alive is decided from the request, so the caller
	has to setHeader("Connection", "keep-alive" | "close") before serializing.
	toBytes can't know.
	No CRLF sanitization on header values. If you ever build a Location: header
	from the request path — which you will, for the trailing-slash-directory 301
	redirect — and the client sends a path containing %0d%0a, they can inject
	arbitrary headers into your response. Strip \r and \n from values in
	setHeader.
	No chunked transfer encoding. If a CGI script produces output without
	declaring a length, HTTP/1.1 wants chunked. Since you buffer the whole body
	in a std::string anyway, you can dodge this by always computing the length
	yourself — worth knowing it's a deliberate simplification and not an
	oversight.
*/
std::string toBytes(const HttpResponse& response)
{
	std::string output;
	output += "HTTP/1.1 ";
	output += std::to_string(response.statusCode);
	output += " ";
	output += response.statusText;
	output += "\r\n";
	for (const auto& h : response.headers)
	{
		output += h.first + ": " + h.second + "\r\n";
	}
	//if a response already has a Content-Length in its map (a CGI response might), you'd emit it twice. Later you can guard with if (!response.hasHeader("Content-Length"))
	output += "Content-Length: ";
	output += std::to_string(response.body.size());
	output += "\r\n";
	output += "\r\n";
	output += response.body;
	return output;
}
