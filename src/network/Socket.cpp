#include "network/Socket"

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
		throw std::runtime_error("resolve " + endpoint + ": " + ::gai_strerror(result));

	std::unique_ptr<addrinfo, decltype(&::freeaddrinfo)> addresses(raw, &::freeaddrinfo);
	std::string lastError = "no IPv4 address available";

	for (addrinfo *entry = address.get(); entry; entry = entry->ai_next) {
		try {

		Socket listener(::socket(entry->ai_family, entry->ai_socktype, entry->ai_protocol));
		checked(listener.fd(), "socket");
		const int yes = 1;
		checked(::setsockopt(listener.fd(), SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)), "setsockopt(SO_REUSEADDR");
		int flags = ::fcntl(listener.fd(), F_GETFL, 0);
		checked(flags, "fcntl(F_GETFL)");
		checked(::fcntl(listener.fd(), F_SETFL, flags | O_NONBLOCK), "fcntl(F_SETFL)");
		flags = ::fcntl(listener.fd(), F_GETFD, 0);
		checked(flags, "fcntl(F_GETFD)");
		checked(::fcntl(listener.fd(), F_SETFD, flags | FD_CLOEXEC), "fcntl)F_SETFD");
		checked(::bind(listener.fd(), entry->ai_addr, entry->ai_addrlen), "bind");
		checked(::listen(listener.fd(), SOMAXCONN), "listen");
		return listener;
		} catch (const std::system_error &error) {
			lastError = error.what();
		}
	}
	throw std::runtime_error("listen " + endpoint + ": " + lastError);
}
