#include "CgiProcess.hpp"

#include <unistd.h>

#include <utility>

CgiProcess::CgiProcess(pid_t childPid, int stdinFd, int stdoutFd,
	std::size_t connectionId)
	: _childPid(childPid), _stdinFd(stdinFd), _stdoutFd(stdoutFd),
	  _connectionId(connectionId), _startedAt(std::chrono::steady_clock::now()),
	  _state(CgiState::Running) {}

CgiProcess::~CgiProcess() noexcept { closeStdin(); closeStdout(); }

CgiProcess::CgiProcess(CgiProcess &&other) noexcept
	: _childPid(other._childPid), _stdinFd(other._stdinFd),
	  _stdoutFd(other._stdoutFd), _connectionId(other._connectionId),
	  _startedAt(other._startedAt), _state(other._state),
	  _output(std::move(other._output))
{
	other._childPid = -1;
	other._stdinFd = -1;
	other._stdoutFd = -1;
}

CgiProcess &CgiProcess::operator=(CgiProcess &&other) noexcept
{
	if (this != &other)
	{
		closeStdin();
		closeStdout();
		_childPid = other._childPid;
		_stdinFd = other._stdinFd;
		_stdoutFd = other._stdoutFd;
		_connectionId = other._connectionId;
		_startedAt = other._startedAt;
		_state = other._state;
		_output = std::move(other._output);
		other._childPid = -1;
		other._stdinFd = -1;
		other._stdoutFd = -1;
	}
	return *this;
}

pid_t CgiProcess::childPid() const noexcept { return _childPid; }
int CgiProcess::stdinFd() const noexcept { return _stdinFd; }
int CgiProcess::stdoutFd() const noexcept { return _stdoutFd; }
std::size_t CgiProcess::connectionId() const noexcept { return _connectionId; }
std::chrono::steady_clock::time_point CgiProcess::startedAt() const noexcept { return _startedAt; }
CgiState CgiProcess::state() const noexcept { return _state; }
void CgiProcess::setState(CgiState state) noexcept { _state = state; }
std::string &CgiProcess::output() noexcept { return _output; }
const std::string &CgiProcess::output() const noexcept { return _output; }
void CgiProcess::closeStdin() noexcept { if (_stdinFd >= 0) { close(_stdinFd); _stdinFd = -1; } }
void CgiProcess::closeStdout() noexcept { if (_stdoutFd >= 0) { close(_stdoutFd); _stdoutFd = -1; } }
