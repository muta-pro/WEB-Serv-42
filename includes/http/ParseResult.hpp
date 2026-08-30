#pragma once

#include "HttpRequest.hpp"

enum class ParseStatus {
	INCOMPLETE, 
	COMPLETE, 
	ERROR
};


struct ParseResult { // to be figured out later - Liza
	ParseStatus	status = ParseStatus::INCOMPLETE;
	size_t		bytesConsumed = 0;
	int			httpStatus = 0; // same as errorCode before, possible codes: 400, 413, 414, 431, or 505
	std::string	diagnostic; // same as errorMessage before
};

class HttpRequestParser {
	public:
		ParseResult feed(std::string_view bytes);
		HttpRequest takeRequest();
		void 		reset();

	private:
		HttpRequest	_request; //this added by Ivan(read below)
};


/*ownership of httprequest
this way is decided that Parser owns the 
partially built request and transferes when complete
behaviour:
_request -belongs to parser while parsing
takeRequest() -moves it to the caller after COMPLETE
reset() -prepares parser for next request
*/