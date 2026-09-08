#ifndef HTTPRESPONSE_HPP
# define HTTPRESPONSE_HPP

# include <string>
# include <map>
# include "CaseInsensitiveLess.hpp"
#include "HeaderMap.hpp"

struct HttpResponse {
	int					statusCode = 200;
	std::string	statusText;
	HeaderMap		headers;
	std::string	body;

	static HttpResponse make(int code, std::string body = "");
	static HttpResponse makeError(int code);
	static std::string reasonFor(int code);
	void setHeader(const std::string &name,const std::string &value);
	std::string getHeader(const std::string &name) const;
	bool hasHeader(const std::string &name) const;
};

std::string toBytes(const HttpResponse& response);

#endif
