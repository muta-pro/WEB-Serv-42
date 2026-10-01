#ifndef HTTPREQUESTPARSER_HPP
#define HTTPREQUESTPARSER_HPP

#include "HttpRequest.hpp"
#include "ParseResult.hpp"

class HttpRequestParser {
  public:
    ParseResult feed(std::string_view bytes);
    HttpRequest takeRequest(); //valide after feed() returns COMPLETE
    void        reset();

  private:
    HttpRequest _request; //this added by Ivan(read below)
};

#endif

/*

_request - gives parser complete control over it's partial state;
          obj request belongs to parser while parsing -
          takeRequest() moves it to caller after COMPLETE
          and prepares parser for next req
          RESET() discards incoomplete or faild request

*/
