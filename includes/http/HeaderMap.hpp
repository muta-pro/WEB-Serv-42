#ifndef HEADERMAPP_HPP
#define HEADERMAPP_HPP

#include <map>
#include <string>
#include "CaseInsensitiveLess.hpp"

using HeaderMap = std::map<std::string, std::string, CaseInsensitiveLess>;
  //is a type alias (for complete map type)

#endif
