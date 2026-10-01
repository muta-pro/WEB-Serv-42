#ifndef CLIENT_HPP
#define CLIENT_HPP
 //COMPLETE PER-CLIENT SESSION - aggregation object;

#include "http/HttpRequestParser.hpp"
#include "network/Connection.hpp"

struct Client {
  Connection connection;
  HttpRequestParser parser;
};

#endif
