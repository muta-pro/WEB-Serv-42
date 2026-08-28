#pragma once

#include <string>
#include <map>
#include <string_view>
#include <cctype> // added a header
#include "CaseInsensitiveLess.hpp"

struct HttpRequest {
	std::string							method;  // GET POST DELETE
	std::string							path;    // /index.html
	std::string							version; // HTTP/1.1
	std::map<std::string,std::string, CaseInsensitiveLess>	headers; 
	std::string							body;

	std::string							header(const std::string k) const;
	size_t										contentLength() const;
	bool										isKeepAlive() const;
};

/* 

this is gonna most probably need major changes later - Liza

*/
