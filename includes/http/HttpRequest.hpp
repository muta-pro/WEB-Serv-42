#ifndef HTTPREQUEST_HPP
#define HTTPREQUEST_HPP

#include <string>
#include <map>
#include <cctype> // added a header
#include "CaseInsensitiveLess.hpp"
#include "HeaderMap.hpp"

struct HttpRequest {
	std::string							method;  // GET POST DELETE
	std::string							target; //exactley what happened between method and HTTP version
	std::string							path;    // /index.html URL path without ? or query
	std::string							query; //everything after ?, without the ?
	std::string							version; // HTTP/1.1
	HeaderMap								headers;
	std::string							body;

	std::string_view						header(const std::string_view k) const;
	std::optional<std::size_t>	contentLength() const;
	bool												isKeepAlive() const;
	bool												hasHeader(std::string_view name) const;
			//the caller can call hasHeader() before retrieving the value;
};
//one shared alias
typedef std::map<
	std::string,
	std::string,
	CaseInsensitiveLess
>HeaderMap;

#endif
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

++++
contentLenght fix: replaced size_t with std::optional
A plain size_t cannot distinguish:
- no Content-Length;
- Content-Length: 0;
- an invalid value;
- an overflowing value.
The parser should reject invalid or overflowing 
values before returning COMPLETE.
Empty optional means absent header;
 +++++
isKeepAlive()
Keep-alive does not only depend on whether the header equals "keep-alive":
- HTTP/1.1 defaults to keep-alive unless Connection: close is present.
- HTTP/1.0 defaults to close unless Connection: keep-alive is present.
- Connection can contain a comma-separated list of case-insensitive tokens.
This behavior should be documented in the helper so the network branch does 
not implement a second, different version.


second thing to decide - detect duplicates inside parser inmplementation;
using map can insert duplicate headers blindly overwriting values:
	policy for the parser detecting duplicates before insertion is missing;
	-reject conflicting Content-Length values;
	-reject unsupported combinations such as Content-Length with Transfer-Encoding;
	-deliberately combine headers where combining is allowed;
	-never silently overwrite an existing value.



*/
