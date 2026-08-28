#ifndef HTTPREQUEST_HPP
#define HTTPREQUEST_HPP

#include "HttpRequest.hpp"
#include <string_view>

enum class ParseStatus {
	INCOMPLETE, 
	COMPLETE, 
	ERROR
};

struct ParseResult { // to be figured out later - Liza
	ParseStatus	status;
	size_t		bytesConsumed;
	int			errorCode;
	std::string	errorMsg;
};

#endif