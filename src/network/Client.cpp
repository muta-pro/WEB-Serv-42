#include "network/Connection.hpp"

#include <stdexcept>
#include <utility>

Connection::Connection(int client_fd)
	: _fd(client_fd), _stateLoop(ReadingHeaders), _lastActivity(std::time(NULL)),
	  _bytesSent(0), _parseOffset(0), _keepAlive(false)
{
	if (client_fd < 0)
		throw std::invalid_argument("Connection requires a valid file descriptor");
}

Connection::~Connection() noexcept { closeOwnedFd(); }

Connection::Connection(Connection &&other) noexcept
	: _fd(other._fd), _readBuff(std::move(other._readBuff)),
	  _writeBuff(std::move(other._writeBuff)), _stateLoop(other._stateLoop),
	  _lastActivity(other._lastActivity), _bytesSent(other._bytesSent),
	  _parseOffset(other._parseOffset), _keepAlive(other._keepAlive)
{
	other._fd = -1;
	other._stateLoop = Closed;
}

Connection &Connection::operator=(Connection &&other) noexcept
{
	if (this != &other)
	{
		closeOwnedFd();
		_fd = other._fd;
		_readBuff = std::move(other._readBuff);
		_writeBuff = std::move(other._writeBuff);
		_stateLoop = other._stateLoop;
		_lastActivity = other._lastActivity;
		_bytesSent = other._bytesSent;
		_parseOffset = other._parseOffset;
		_keepAlive = other._keepAlive;
		other._fd = -1;
		other._stateLoop = Closed;
	}
	return *this;
}

void Connection::closeOwnedFd() noexcept { if (_fd >= 0) { close(_fd); _fd = -1; } }
int Connection::fd() const noexcept { return _fd; }
ConnectionState Connection::state() const noexcept { return _stateLoop; }
void Connection::setState(ConnectionState value) noexcept { _stateLoop = value; }
std::string &Connection::readBuff() noexcept { return _readBuff; }
const std::string &Connection::readBuff() const noexcept { return _readBuff; }
std::string &Connection::writeBuff() noexcept { return _writeBuff; }
const std::string &Connection::writeBuff() const noexcept { return _writeBuff; }
std::size_t Connection::bytesSent() const noexcept { return _bytesSent; }
void Connection::addBytesSent(std::size_t amount) noexcept { _bytesSent += amount; }
std::size_t Connection::parseOffset() const noexcept { return _parseOffset; }
void Connection::addParsedBytes(std::size_t amount) noexcept { _parseOffset += amount; }
bool Connection::KeepAlive() const noexcept { return _keepAlive; }
void Connection::setKeepAlive(bool value) noexcept { _keepAlive = value; }
std::time_t Connection::lastActivity() const noexcept { return _lastActivity; }
void Connection::touch() noexcept { _lastActivity = std::time(NULL); }

void Connection::resetforNextRequest() noexcept
{
	if (_parseOffset <= _readBuff.size())
		_readBuff.erase(0, _parseOffset);
	else
		_readBuff.clear();
	_writeBuff.clear();
	_bytesSent = 0;
	_parseOffset = 0;
	_stateLoop = ReadingHeaders;
}
