=/*
runntime state;
explicit keyword:pervets creating copies of fds,
to avoid double-close when the objs are destroyed;
lastActivity useful to check for timeout, to disconnect the client

EACH accepted client FD is owned by exactly one connection;
Destroying that connection closes the FD.
other modules may use FD with recv(), send(), epoll(), but necer call close() on it;
event loop owns connection obj, which owns the FD;
listening sockets and CGI pipe FDs have separate owners; dont belong to client Connection;
*/

#ifndef CONNECTION_HPP
#define CONNECTION_HPP

#include <cstddef>
#include <functional>
#include <string>
#include <ctime>
#include <unistd.h>
#include "ConnectionState.hpp"

class Connection {
	public:
			explicit Connection(int client_fd);
			~Connection() noexcept;

			Connection(const Connection &copy) = delete;
			Connection &operator=(const Connection &assign) = delete;

			//allowing ::move operators
			Connection(Connection&& other) noexcept;
			Connection& operator=(Connection&& other) noexcept;

			int fd() const noexcept;

			Connection state() const noexcept;
			void setState(ConnectionState newState) noexcept;

			std::string &readBuff() noexcept;
			const std::string &readBuff() const noexcept;

			std::string &writeBuff() noexcept;
			const std::string &writeBuff() const noexcept;

			std::size_t bytesSent() const noexcept;
			void addBytesSent(std::size_t amount) noexcept;

			std::size_t parseOffset() const noexcept;
			void addParsedBytes(std::size_t amount) noexcept;

			bool KeepAlive() const noexcept;
			void setKeepAlive(bool value) noexcept;

			std::time_t lastActivity() const noexcept;
			void touch() noexcept;

			void resetforNextRequest() noexcept;

			//getters
			//setters

	private:
		void closeOwnedFd() noexcept;

		int							_fd; //socket ID for recv()/send()
		std::string			_readBuff; //for raw bytes from recv()
		std::string			_writeBuff; //empty the buffer using send()
		ConnectionState	_stateLoop; //communicates to the loop
		time_t					_lastActivity; //update time on every clients send/recv
		std::size_t			_bytesSent;
		std::size_t			_parseOffset;
		bool						_keepAlive;
};

#endif
