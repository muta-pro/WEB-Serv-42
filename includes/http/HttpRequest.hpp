#include <string>
#include <map>

struct HttpRequest {
	std::string							method;  // GET POST DELETE
	std::string							path;    // /index.html
	std::string							version; // HTTP/1.1
	std::map<std::string,std::string>	headers;
	std::string							body;

	std::string							header(const std::string& k) const;
	size_t								contentLength() const;
	bool								isKeepAlive() const;
} ;

/* I don't know what else to put here yet, this struct goes inside
connection (according to the contracts page of the plan) - Liza

*/