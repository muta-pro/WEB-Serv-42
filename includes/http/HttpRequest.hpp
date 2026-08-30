#pragma once

#include <string>
#include <map>
#include <cctype> // added a header
#include "CaseInsensitiveLess.hpp"

struct HttpRequest {
	std::string							method;  // GET POST DELETE
	std::string							target; //exactley what happened between method and HTTP version
	std::string							path;    // /index.html URL path without ? or query
	std::string							query; //everything after ?, without the ?
	std::string							version; // HTTP/1.1
	std::map<std::string,std::string, CaseInsensitiveLess>	headers; 
	std::string							body;

	std::string							header(const std::string k) const;
	size_t								contentLength() const;
	bool								isKeepAlive() const;
};

/* 

this is gonna most probably need major changes later - Liza

I added target/path/query -Ivan + some explenation notes

the parser should perserve these as received. it should not turn URL into a filesystem path
 
 if you have
 GET /products/list?category=books&page=2 HTTP/1.1

 the parser separates
 method - GET
 target - /products/list?category=books=2
 path - /products/list
 query - category=books&page=2
 version - HTTP/1.1


## about the router role: path normalization

if you have instead:
GET /public/ ../text.txt?download=1 HTTP/1.1

the parser only understand HTTP syntax but not what is: /public

so ".." are also to figure out :D 

so router selects location and server configuration
decodes normalizes URL path safely
maps it to filesystem path
verifies it remains inside the configured root 

if you have this

GET /public/%2e%2e/text.txt HTTP/1.1

%2e%2e represents ".."

the router must account for encoded traversal when resolves the path

RULE: parser produces URL info - router produces filesystem information


*/
