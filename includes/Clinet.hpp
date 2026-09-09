#ifndef CLINET_HPP
#define CLINET_HPP
 //COMPLETE PER-CLIENT SESSION - aggregation object;

#include "http/HttpRequestParser.hpp"
#include "network/Connection.hpp"

struct Client {
  Connection connection;
  HttpRequestParser parser;
}

#endif
