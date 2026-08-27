#pragma once

#include <string>
#include <map>
#include <string_view>
#include <cctype> // added a header
#include "CaseInsensitiveLess.hpp"

struct HttpRequest {
	std::string_view							method;  // GET POST DELETE
	std::string_view							path;    // /index.html
	std::string_view							version; // HTTP/1.1
	std::map<std::string_view,std::string_view, CaseInsensitiveLess>	headers;
	std::string_view							body;

	std::string_view							header(const std::string_view k) const;
	size_t										contentLength() const;
	bool										isKeepAlive() const;
};

/* 

this is gonna most probably need major changes later - Liza

*/
