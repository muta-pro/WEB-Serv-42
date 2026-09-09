/*
runntime state;
explicit keyword:pervets creating copies of fds,
to avoid double-close when the objs are destroyed;
lastActivity useful to check for timeout, to disconnect the client
*/

#ifndef CONNECTION_HPP
#define CONNECTION_HPP

#include <string>
#include <ctime>
#include <unistd.h>
#include "HttpResponse.http"


enum ConnectionState;

class Connection {
	public:
			explicit Connection(int client_fd);
			Connection(const Connection &copy) = delete;
			Connection &operator=(const Connection &assign) = delete;
			~Connection() { close(fd); }
			//allowing ::move operators
			Connection(Connection&&) noexcept = default;
			Connection& operator=(Connection&&) noexcept = default;

			//getters
			//setters

	private:
		int							fd; //socket ID for recv()/send()
		std::string			readBuff; //for raw bytes from recv()
		std::string			writeBuff; //empty the buffer using send()
		ConnectionState	state; //communicates to the loop
		time_t					lastActivity; //update time on every clients send/recv
		HttpResponse		resp; //serialized raw txt fills writeBuff
		std::size_t			byteSent;
		std::size_t			parseOffset;
		bool						keepAlive;
};

#endif
