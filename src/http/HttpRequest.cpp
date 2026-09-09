#include "http/HttpRequest.hpp"

// to be implemented

// std::string	HttpRequest::header(const std::string k) const{

// }

// size_t		HttpRequest::contentLength() const{

// }

// bool		HttpRequest::isKeepAlive() const{

// }

/*
using HeaderMap = std::map<std::string, std::string, CaseInsensitiveLess>;
  //is a type alias (for complete map type)

HeaderMap headers;

HeaderMap::iterator existing = request.headers.find(name);

if (existing != request.headers.end()) {
  //duplicate header policy
  //duplicate Host - 400 bad req
  //dup Content-Len or conflicting - reject, 400
  // never siletnly overwrite existing header
  // permitted repeatable headers may be combined or preserved
      //MULTIMAP would perserve duplicates;
} else {
  request.headers.insert(std::make_pair(name, value));
}
*/
