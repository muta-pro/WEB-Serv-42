#include "network/Socket.hpp"

#include <cerrno>
#include <fcntl.h>
#include <memory>
#include <netdb.h>
#include <netinet/in.h>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <system_error>
#include <unistd.h>

Socket::Socket(int fd) noexcept : _fd(fd) {}

Socket::~Socket() noexcept {
	if (fd >= 0)
		::close(_fd);
}

Socket::Socket(Socket &&other) noexcept : _fd(other._fd)
{
	other._fd = -1;
}

Socket &Socket::operator=(Socket &&other) noexcept
{
	if (this != &other) {
		if (_fd >= 0)
			::close(_fd);
		_fd = other._fd;
		other._fd = -1;
	}
	return *this;
}

int Socket::fd() const noexcept {
	return _fd;
}

static void checked(int result, const char *operation)
{
	if (result == -1) {
		const int error = errno;
		throw std::system_error(error, std::generic_category(), operation);
	}
}

Socket Socket::createListener(const Listen &address)
{
	const std::string port = std::to_string(address.port);
	const std::string endpoint = address.host + ":" + port;

	addrinfo hints{};
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_NUMERICSERV;

	addrinfo *raw = nullptr;
	const int result = ::getaddrinfo(address.host.c_str(), port.c_str(), &hints, &raw);
	if (result != 0)
		throw std::system_error("resolve " + endpoint + ": " + ::gai_strerror(result));

	std::unique_ptr<addrinfo, decltype(&::freeaddrinfo)> addresses(raw, &::freeaddrinfo);
	std::string lastError = "no IPv4 address available";

	for (addrinfo *entry = addresses.get(); entry; entry = entry->ai_next) {
		try {
		Socket listener(::socket(entry->ai_family, entry->ai_socktype, entry->ai_protocol));
		checked(listener.fd(), "socket");
		const int yes = 1;
		checked(::setsockopt(listener.fd(),
			SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)), "setsockopt(SO_REUSEADDR)");
		int flags = ::fcntl(listener.fd(), F_GETFL, 0);
		checked(flags, "fcntl(F_GETFL)");
		checked(::fcntl(listener.fd(), F_SETFL, flags | O_NONBLOCK), "fcntl(F_SETFL)");
		flags = ::fcntl(listener.fd(), F_GETFD, 0);
		checked(flags, "fcntl(F_GETFD)");
		checked(::fcntl(listener.fd(), F_SETFD, flags | FD_CLOEXEC), "fcntl(F_SETFD)");
		checked(::bind(listener.fd(), entry->ai_addr, entry->ai_addrlen), "bind");
		checked(::listen(listener.fd(), SOMAXCONN), "listen");
		return listener;
		} catch (const std::system_error &error) {
			lastError = error.what();
		}
	}
	throw std::runtime_error("listen " + endpoint + ": " + lastError);
}

