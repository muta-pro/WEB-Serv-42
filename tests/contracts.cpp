#include "CgiProcess.hpp"
#include "RouteResult.hpp"
#include "http/HttpRequest.hpp"
#include "http/HttpResponse.hpp"
#include "network/Connection.hpp"

#include <cassert>
#include <cerrno>
#include <fcntl.h>
#include <iostream>
#include <unistd.h>
#include <utility>
#include <vector>

static HttpRequest fakeParse(const std::string &bytes)
{
	HttpRequest request;
	assert(bytes.find("\r\n\r\n") != std::string::npos);
	request.method = "GET";
	request.target = "/hello";
	request.path = "/hello";
	request.version = "HTTP/1.1";
	request.headers["Host"] = "example.test";
	return request;
}

static RouteResult fakeRoute(const HttpRequest &request)
{
	RouteResult route;
	assert(request.path == "/hello");
	route.action = RouteAction::StaticFile;
	route.StatusCode = 200;
	route.filesystemPath = "./www/html/index.html";
	return route;
}

int main()
{
	HttpRequest request;
	assert(!request.contentLength().has_value());
	request.headers["content-length"] = "0";
	assert(request.contentLength().has_value() && *request.contentLength() == 0);
	request.headers["X-Test"] = "preserved Value";
	assert(request.header("x-test") == "preserved Value");

	request.version = "HTTP/1.1";
	assert(request.isKeepAlive());
	request.headers["Connection"] = "upgrade, CLOSE";
	assert(!request.isKeepAlive());
	request.version = "HTTP/1.0";
	request.headers["Connection"] = "Keep-Alive";
	assert(request.isKeepAlive());

	RouteResult safeDefault;
	assert(safeDefault.action == RouteAction::Error);
	assert(safeDefault.StatusCode == 500);
	const RouteAction actions[] = {RouteAction::StaticFile, RouteAction::Directory,
		RouteAction::Redirect, RouteAction::Upload, RouteAction::DeleteResource,
		RouteAction::CGI, RouteAction::Error};
	for (std::size_t i = 0; i < sizeof(actions) / sizeof(actions[0]); ++i)
	{
		RouteResult oneAction;
		oneAction.action = actions[i];
		assert(oneAction.action == actions[i]);
	}

	HttpResponse response = HttpResponse::make(200, "hello");
	response.setHeader("Content-Length", "999");
	const std::string wire = toBytes(response, false);
	assert(wire.find("Content-Length: 5\r\n") != std::string::npos);
	assert(wire.find("Content-Length:") == wire.rfind("Content-Length:"));
	assert(wire.substr(wire.find("\r\n\r\n") + 4) == "hello");
	const std::string head = toBytes(response, true);
	assert(head.find("Content-Length: 5\r\n") != std::string::npos);
	assert(head.substr(head.find("\r\n\r\n") + 4).empty());
	const std::string noContent = toBytes(HttpResponse::make(204, "ignored"), false);
	assert(noContent.find("Content-Length") == std::string::npos);
	assert(noContent.substr(noContent.find("\r\n\r\n") + 4).empty());

	int descriptors[2];
	assert(pipe(descriptors) == 0);
	close(descriptors[1]);
	const int ownedFd = descriptors[0];
	{
		std::vector<Connection> sessions;
		sessions.push_back(Connection(ownedFd));
		assert(sessions[0].fd() == ownedFd);
		sessions[0].readBuff() = "usednext";
		sessions[0].addParsedBytes(4);
		sessions[0].writeBuff() = wire;
		sessions[0].addBytesSent(3);
		assert(sessions[0].bytesSent() == 3);
		sessions[0].resetforNextRequest();
		assert(sessions[0].readBuff() == "next");
		assert(sessions[0].writeBuff().empty() && sessions[0].bytesSent() == 0);
	}
	errno = 0;
	assert(fcntl(ownedFd, F_GETFD) == -1 && errno == EBADF);

	int cgiInput[2];
	int cgiOutput[2];
	assert(pipe(cgiInput) == 0 && pipe(cgiOutput) == 0);
	close(cgiInput[0]);
	close(cgiOutput[1]);
	const int cgiWriteFd = cgiInput[1];
	const int cgiReadFd = cgiOutput[0];
	{
		CgiProcess process(123, cgiWriteFd, cgiReadFd, 7);
		process.output() = "Status: 200 OK\r\n\r\nhello";
		CgiProcess moved(std::move(process));
		assert(process.stdinFd() == -1 && process.stdoutFd() == -1);
		assert(moved.connectionId() == 7);
		assert(moved.output().find("hello") != std::string::npos);
	}
	errno = 0;
	assert(fcntl(cgiWriteFd, F_GETFD) == -1 && errno == EBADF);
	errno = 0;
	assert(fcntl(cgiReadFd, F_GETFD) == -1 && errno == EBADF);

	std::string fragmented = "GET /hello HTTP/1.1\r\n";
	fragmented += "Host: example.test\r\n\r\n";
	const HttpRequest lifecycleRequest = fakeParse(fragmented);
	const RouteResult lifecycleRoute = fakeRoute(lifecycleRequest);
	const HttpResponse lifecycleResponse = HttpResponse::make(lifecycleRoute.StatusCode, "hello");
	assert(toBytes(lifecycleResponse, false).find("HTTP/1.1 200 OK\r\n") == 0);

	std::cout << "Phase 0 contract smoke tests passed.\n";
}
