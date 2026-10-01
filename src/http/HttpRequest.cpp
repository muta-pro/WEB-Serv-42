#include "http/HttpRequest.hpp"

#include <cctype>
#include <limits>

std::string_view HttpRequest::header(std::string_view name) const
{
	HeaderMap::const_iterator found = headers.find(std::string(name));
	if (found == headers.end())
		return std::string_view();
	return found->second;
}

bool HttpRequest::hasHeader(std::string_view name) const
{
	return headers.find(std::string(name)) != headers.end();
}

std::optional<std::size_t> HttpRequest::contentLength() const
{
	if (!hasHeader("Content-Length"))
		return std::nullopt;
	const std::string_view value = header("Content-Length");
	if (value.empty())
		return std::nullopt;
	std::size_t result = 0;
	for (std::size_t i = 0; i < value.size(); ++i)
	{
		const unsigned char character = static_cast<unsigned char>(value[i]);
		if (!std::isdigit(character))
			return std::nullopt;
		const std::size_t digit = static_cast<std::size_t>(character - '0');
		if (result > (std::numeric_limits<std::size_t>::max() - digit) / 10)
			return std::nullopt;
		result = result * 10 + digit;
	}
	return result;
}

static bool containsToken(std::string_view list, std::string_view wanted)
{
	std::size_t start = 0;
	while (start <= list.size())
	{
		std::size_t end = list.find(',', start);
		if (end == std::string_view::npos)
			end = list.size();
		while (start < end && std::isspace(static_cast<unsigned char>(list[start])))
			++start;
		while (end > start && std::isspace(static_cast<unsigned char>(list[end - 1])))
			--end;
		bool equal = end - start == wanted.size();
		for (std::size_t i = 0; equal && i < wanted.size(); ++i)
			equal = std::tolower(static_cast<unsigned char>(list[start + i]))
				== std::tolower(static_cast<unsigned char>(wanted[i]));
		if (equal)
			return true;
		if (end == list.size())
			break;
		start = end + 1;
	}
	return false;
}

bool HttpRequest::isKeepAlive() const
{
	const std::string_view connection = header("Connection");
	if (version == "HTTP/1.1")
		return !containsToken(connection, "close");
	if (version == "HTTP/1.0")
		return containsToken(connection, "keep-alive");
	return false;
}
