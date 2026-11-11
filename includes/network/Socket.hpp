#ifndef SOCKET_HPP
#define SOCKET_HPP

#include "config/ServerConfig.hpp"

//Owns one listening FD.
//The event loop may use fd(), but must not close it
class Socket
{
public:
	Socket(const Socket &) = delete;
	Socket &operator=(const Socket &) = delete;
	Socket(Socket &&other) noexcept;
	Socket &operator(Socket &&other) noexcept;
	~Socket() noexcept;

	static Socket createListener(const Listen &address);

	int fd() const noexcept;
private:
	explicit Socket(int fd) noexcept;
	int _fd;
};

#endif