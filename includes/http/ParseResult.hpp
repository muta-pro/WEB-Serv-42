#ifndef PARSERESULT_HPP
#define PARSERESULT_HPP


#include <cstddef>
#include <string>

enum class ParseStatus {
	INCOMPLETE, //valid so far, more bytes are needed
	COMPLETE,  //all bytes (content-lenght) arrived - request-line/headers/body available
	ERROR //request invalid and httpStatus explaines needed response
};

struct ParseResult { // to be figured out later - Liza
	ParseStatus	status = ParseStatus::INCOMPLETE;
	std::size_t			bytesConsumed = 0; //n of bytes from the curr feed() arg; *read-below
	int					httpStatus = 0; // same as errorCode before, possible codes: 400, 413, 414, 431, or 505
	std::string	diagnostic; // same as errorMessage before - for internal logs; not sent automatically to caller;
};

#endif

/*ownership of httprequest
this way is decided that Parser owns the 
partially built request and transferes when complete
behaviour:
_request -belongs to parser while parsing
takeRequest() -moves it to the caller after COMPLETE
reset() -prepares parser for next request

bytesConsumed - when one network has two requests  if first request
was X bytes - the caller retains everything after byte 80
adn feeds it to parser after processing and resetting request 1; caller must feed every byte excatly once;

PARSER -must retain parsing state between feed() calls;
	1- ONE REQUEST can arriver in multiple pieces
	2- one read can contain more tha one request
*/
