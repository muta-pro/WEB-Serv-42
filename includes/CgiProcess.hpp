#ifndef CGIPROCESS_HPP
#define CGIPROCESS_HPP

#include <chrono>
#include <cstddef>
#include <string>
#include <sys/types.h>

enum class CgiState { Running, Complete, Failed, TimedOut };

// Shared runtime boundary. It owns the parent-side pipe descriptors; the CGI
// feature branch remains responsible for spawning, timeout policy and reaping.
class CgiProcess {
	public:
		CgiProcess(pid_t childPid, int stdinFd, int stdoutFd,
			std::size_t connectionId);
		~CgiProcess() noexcept;
		CgiProcess(const CgiProcess &) = delete;
		CgiProcess &operator=(const CgiProcess &) = delete;
		CgiProcess(CgiProcess &&other) noexcept;
		CgiProcess &operator=(CgiProcess &&other) noexcept;

		pid_t childPid() const noexcept;
		int stdinFd() const noexcept;
		int stdoutFd() const noexcept;
		std::size_t connectionId() const noexcept;
		std::chrono::steady_clock::time_point startedAt() const noexcept;
		CgiState state() const noexcept;
		void setState(CgiState state) noexcept;
		std::string &output() noexcept;
		const std::string &output() const noexcept;
		void closeStdin() noexcept;
		void closeStdout() noexcept;

	private:
		pid_t _childPid;
		int _stdinFd;
		int _stdoutFd;
		std::size_t _connectionId;
		std::chrono::steady_clock::time_point _startedAt;
		CgiState _state;
		std::string _output;
};

#endif
