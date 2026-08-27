#pragma once

#include <string>
#include <map>
#include <string_view>
#include <cctype> // added a header

struct CaseInsensitiveLess { // a struct for normalizing strings in the map
	bool operator()(std::string_view a, std::string_view b) const {
		size_t n = a.size() < b.size() ? a.size() : b.size();
		for (size_t i = 0; i < n; ++i) {
			unsigned char x = tolower(static_cast<unsigned char>(a[i]));
			unsigned char y = tolower(static_cast<unsigned char>(b[i]));
			if (x != y)
				return x < y;
		}
		return a.size() < b.size();
	}
};


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
