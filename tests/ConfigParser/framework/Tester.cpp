#include "Tester.hpp"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <cxxabi.h>
#include <iostream>
#include <sstream>
#include <sys/resource.h>
#include <sys/wait.h>
#include <typeinfo>
#include <unistd.h>
#ifdef __linux__
# include <sys/prctl.h>
#endif

/*
=============================================================================
	text and colours
=============================================================================
*/

static bool	g_colour = false;

static std::string	paint(const char *code, const std::string &text)
{
	if (!g_colour || text.empty())
		return (text);
	return (std::string("\033[") + code + "m" + text + "\033[0m");
}

static std::string	green(const std::string &text)		{ return (paint("32", text)); }
static std::string	red(const std::string &text)		{ return (paint("31", text)); }
static std::string	yellow(const std::string &text)		{ return (paint("33", text)); }
static std::string	magenta(const std::string &text)	{ return (paint("35", text)); }
static std::string	cyan(const std::string &text)		{ return (paint("36", text)); }
static std::string	bold(const std::string &text)		{ return (paint("1", text)); }
static std::string	dim(const std::string &text)		{ return (paint("2", text)); }

static std::string	padRight(const std::string &text, size_t width)
{
	if (text.size() >= width)
		return (text);
	return (text + std::string(width - text.size(), ' '));
}

static std::string	padLeft(const std::string &text, size_t width)
{
	if (text.size() >= width)
		return (text);
	return (std::string(width - text.size(), ' ') + text);
}

static std::string	lower(std::string text)
{
	for (size_t i = 0; i < text.size(); i++)
		text[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(text[i])));
	return (text);
}

static std::string	repeat(const std::string &piece, size_t count)
{
	std::string	out;

	for (size_t i = 0; i < count; i++)
		out += piece;
	return (out);
}

static std::string	plural(size_t count, const std::string &word)
{
	return (std::to_string(count) + " " + word + (count == 1 ? "" : "s"));
}

static std::string	secondsText(double seconds)
{
	char	buffer[32];

	std::snprintf(buffer, sizeof(buffer), "%.2fs", seconds);
	return (buffer);
}

/* the lines of text. a final '\n' does not start another line */
static std::vector<std::string>	splitLines(const std::string &text)
{
	std::vector<std::string>	lines;
	size_t						start = 0;

	while (true)
	{
		size_t	newline = text.find('\n', start);

		if (newline == std::string::npos)
		{
			lines.push_back(text.substr(start));
			break;
		}
		lines.push_back(text.substr(start, newline - start));
		start = newline + 1;
	}
	if (lines.size() > 1 && lines.back().empty())
		lines.pop_back();
	return (lines);
}

static const size_t	MAX_WIDTH = 150;	// characters shown of one line
static const size_t	MAX_LINES = 15;		// lines shown of one input / expected / got

/*
one line, the way it should look on screen: tabs, \r and other control
bytes made visible (a tokenizer test is about exactly those), and a long
line cut short without splitting a UTF-8 character in two.
*/
static std::string	shown(const std::string &line, bool colour)
{
	size_t		end = line.size();
	std::string	out;

	if (end > MAX_WIDTH)
	{
		end = MAX_WIDTH;
		while (end > 0 && (static_cast<unsigned char>(line[end]) & 0xC0) == 0x80)
			end--;
	}
	for (size_t i = 0; i < end; i++)
	{
		unsigned char	c = static_cast<unsigned char>(line[i]);
		std::string		mark;

		if (c == '\t')
			mark = "→   ";
		else if (c == '\r')
			mark = "\\r";
		else if (c < 0x20 || c == 0x7f)
		{
			char	hex[8];
			std::snprintf(hex, sizeof(hex), "\\x%02x", c);
			mark = hex;
		}
		if (mark.empty())
			out += line[i];
		else
			out += colour ? dim(mark) : mark;
	}
	if (end < line.size())
	{
		std::string	more = " … " + std::to_string(line.size() - end) + " more characters";
		out += colour ? dim(more) : more;
	}
	return (out);
}

/* the left column of a detail line: "        input   1 │ " */
static std::string	gutter(const std::string &label, size_t number)
{
	std::string	column;

	if (number == 0)
		column = padRight(label, 10);
	else
		column = padRight(label, 6) + padLeft(std::to_string(number), 3) + " ";
	return ("        " + dim(column + "│ "));
}

static std::string	showInput(const std::string &input)
{
	if (input.empty())
		return (gutter("input", 0) + dim("(empty)") + "\n");
	if (input.find('\n') == std::string::npos)
		return (gutter("input", 0) + shown(input, true) + "\n");

	std::vector<std::string>	lines = splitLines(input);
	std::string					out;

	for (size_t i = 0; i < lines.size() && i < MAX_LINES; i++)
		out += gutter(i == 0 ? "input" : "", i + 1) + shown(lines[i], true) + "\n";
	if (lines.size() > MAX_LINES)
		out += gutter("", 0) + dim("… " + plural(lines.size() - MAX_LINES, "more line")) + "\n";
	return (out);
}

/* expected / got. with `against`, the lines that differ from it are red */
static std::string	showText(const std::string &label, const std::string &text, const std::string *against)
{
	if (text.empty())
		return (gutter(label, 0) + dim("(empty)") + "\n");

	std::vector<std::string>	lines = splitLines(text);
	std::vector<std::string>	other;
	std::string					out;

	if (against)
		other = splitLines(*against);
	for (size_t i = 0; i < lines.size() && i < MAX_LINES; i++)
	{
		bool	differs = against && (i >= other.size() || other[i] != lines[i]);

		out += gutter(i == 0 ? label : "", 0);
		out += differs ? red(shown(lines[i], false)) : shown(lines[i], true);
		out += "\n";
	}
	if (lines.size() > MAX_LINES)
		out += gutter("", 0) + dim("… " + plural(lines.size() - MAX_LINES, "more line")) + "\n";
	return (out);
}

/* what the test printed: parser debug output, or a sanitizer report */
static std::string	showOutput(const std::string &output, size_t maxLines)
{
	std::vector<std::string>	lines = splitLines(output);
	std::string					out;

	for (size_t i = 0; i < lines.size() && i < maxLines; i++)
		out += gutter(i == 0 ? "output" : "", 0) + dim(shown(lines[i], false)) + "\n";
	if (lines.size() > maxLines)
		out += gutter("", 0) + dim("… " + plural(lines.size() - maxLines, "more line") + " (-v shows more)") + "\n";
	return (out);
}

/*
=============================================================================
	running one test in its own process
=============================================================================
*/

static std::string	demangle(const char *name)
{
	int			status = 0;
	char		*readable = abi::__cxa_demangle(name, nullptr, nullptr, &status);
	std::string	result = (status == 0 && readable) ? readable : name;

	std::free(readable);
	return (result);
}

/*
one frame of a sanitizer stack trace, e.g.
	"    #3 0x55d0 in parseNumber(Token const&, ...) /home/x/src/config/ConfigParser.cpp:152:9"
gives "parseNumber" and "ConfigParser.cpp:152". place is empty when the
report doesn't know the source line.
*/
static bool	splitFrame(const std::string &line, std::string &function, std::string &place)
{
	size_t	hash = line.find_first_not_of(' ');

	if (hash == std::string::npos || line[hash] != '#')
		return (false);
	size_t	in = line.find(" in ", hash);
	if (in == std::string::npos)
		return (false);
	std::string	rest = line.substr(in + 4);
	size_t		buildId = rest.find(" (BuildId: ");
	if (buildId != std::string::npos)
		rest = rest.substr(0, buildId);
	size_t		space = rest.rfind(' ');
	if (space == std::string::npos)
		return (false);
	function = rest.substr(0, space);
	place = rest.substr(space + 1);
	if (place.empty() || place[0] == '(')
		place.clear();
	else
	{
		size_t	slash = place.rfind('/');
		if (slash != std::string::npos)
			place = place.substr(slash + 1);
		size_t	column = place.find(':', place.find(':') + 1);
		if (column != std::string::npos)
			place = place.substr(0, column);
	}
	size_t	paren = function.find('(');
	if (paren != std::string::npos && paren > 0)
		function = function.substr(0, paren);
	return (true);
}

/* where the runner's own frames start: the test body is called through std::function */
static bool	isRunnerFrame(const std::string &function)
{
	return (function.find("std::function") != std::string::npos
		|| function.find("_Function_handler") != std::string::npos
		|| function.find("__invoke") != std::string::npos
		|| function.find("_M_invoke") != std::string::npos);
}

/*
an AddressSanitizer report runs to 60 lines, most of them the runner's own
frames. keep the error, then the frames of the parser and the test body -
the ones before the runner takes over. -v shows the whole report.
*/
static std::string	shortReport(const std::string &output)
{
	std::string	out;
	size_t		frames = 0;
	bool		stackDone = false;

	for (const std::string &line : splitLines(output))
	{
		std::string	function;
		std::string	place;

		if (line.find("ERROR: AddressSanitizer") != std::string::npos)
		{
			std::string	error = line.substr(line.find("AddressSanitizer"));
			out += error.substr(0, std::min(error.find(" at pc"), error.find(" (pc"))) + "\n";
		}
		else if (line.compare(0, 8, "READ of ") == 0 || line.compare(0, 9, "WRITE of ") == 0)
			out += line.substr(0, line.find(" at ")) + "\n";
		else if (!stackDone && splitFrame(line, function, place))
		{
			if (isRunnerFrame(function))
				stackDone = true;
			else
				out += "  #" + std::to_string(frames++) + " " + function + (place.empty() ? "" : "   " + place) + "\n";
		}
	}
	return (out + "(-v shows the whole report)");
}

/*
where it crashed: among the frames before the runner's, the first one in a
source file rather than a system header - usually the culprit. failing
that, the first frame's function alone.
*/
static std::string	crashSite(const std::string &output)
{
	std::string	first;

	for (const std::string &line : splitLines(output))
	{
		std::string	function;
		std::string	place;

		if (!splitFrame(line, function, place))
			continue;
		if (isRunnerFrame(function))
			break;
		if (!place.empty() && place.find(".h:") == std::string::npos)
			return (" in " + function + " (" + place + ")");
		if (first.empty())
			first = " in " + function;
	}
	return (first);
}

/* the one line of a sanitizer report worth putting next to the test */
static std::string	sanitizerLine(const std::string &output)
{
	size_t	at = output.find("ERROR: AddressSanitizer: ");

	if (at != std::string::npos)
	{
		std::string	line = output.substr(at + 7, output.find('\n', at) - at - 7);
		return (line.substr(0, line.find(" on ")) + crashSite(output));
	}
	at = output.find("runtime error: ");
	if (at != std::string::npos)
	{
		size_t	start = output.rfind('\n', at);
		start = (start == std::string::npos) ? 0 : start + 1;
		std::string	line = output.substr(start, output.find('\n', at) - start);
		size_t		slash = line.rfind('/', at - start);
		if (slash != std::string::npos)
			line = line.substr(slash + 1);
		return ("UndefinedBehaviorSanitizer: " + line);
	}
	return ("");
}

static void	writeAll(int fd, const std::string &data)
{
	size_t	done = 0;

	while (done < data.size())
	{
		ssize_t	written = write(fd, data.data() + done, data.size() - done);

		if (written < 0 && errno == EINTR)
			continue;
		if (written <= 0)
			return;
		done += static_cast<size_t>(written);
	}
}

static std::string	readAll(int fd)
{
	std::string	data;
	char		buffer[4096];

	while (true)
	{
		ssize_t	got = read(fd, buffer, sizeof(buffer));

		if (got < 0 && errno == EINTR)
			continue;
		if (got <= 0)
			break;
		data.append(buffer, static_cast<size_t>(got));
	}
	return (data);
}

/* runs body, turning anything it throws into a failed result */
Tester::Result	Tester::guarded(const std::function<Result()> &body)
{
	try
	{
		return (body());
	}
	catch (const std::exception &e)
	{
		return (Result{Fail, "unexpected exception " + demangle(typeid(e).name()) + ": " + e.what()});
	}
	catch (...)
	{
		return (Result{Fail, "unexpected exception of an unknown type"});
	}
}

/*
fork, run body in the child, send the result back through a pipe. the
child's stdout and stderr go to a scratch file, so whatever the parser
prints (or a sanitizer reports) ends up under the right test. a crash or
an endless loop only takes the child down: alarm() kills it after
_timeout seconds, and the parent reads why it died from its exit status.
*/
Tester::Result	Tester::runIsolated(const std::function<Result()> &body, std::string &output)
{
	int	channel[2];
	int	capture = _capture ? fileno(_capture) : -1;

	output.clear();
	if (pipe(channel) == -1)
		return (Result{Crash, std::string("pipe() failed: ") + std::strerror(errno)});
	if (capture != -1 && (ftruncate(capture, 0) == -1 || lseek(capture, 0, SEEK_SET) == -1))
		capture = -1;
	std::cout.flush();
	std::fflush(nullptr);

	pid_t	pid = fork();

	if (pid == -1)
	{
		close(channel[0]);
		close(channel[1]);
		return (Result{Crash, std::string("fork() failed: ") + std::strerror(errno)});
	}
	if (pid == 0)
	{
		// a crash here is a test result: no core dump, and on Ubuntu no
		// second-long visit from the system crash reporter (apport)
		struct rlimit	noCore = {0, 0};
		setrlimit(RLIMIT_CORE, &noCore);
#ifdef __linux__
		prctl(PR_SET_DUMPABLE, 0);
#endif
		close(channel[0]);
		if (capture != -1)
		{
			dup2(capture, STDOUT_FILENO);
			dup2(capture, STDERR_FILENO);
		}
		if (_timeout > 0)
			alarm(_timeout);
		Result	result = guarded(body);
		std::cout.flush();
		std::cerr.flush();
		std::fflush(nullptr);
		writeAll(channel[1], std::string(1, result.outcome == Pass ? 'P' : 'F') + result.got);
		_exit(0);
	}
	close(channel[1]);
	std::string	reply = readAll(channel[0]);
	close(channel[0]);

	int	status = 0;
	while (waitpid(pid, &status, 0) == -1)
		if (errno != EINTR)
			break;
	if (capture != -1 && lseek(capture, 0, SEEK_SET) != -1)
		output = readAll(capture);

	if (WIFSIGNALED(status) && WTERMSIG(status) == SIGALRM)
		return (Result{Timeout, "still running after " + std::to_string(_timeout) + "s - an endless loop?"});
	if (WIFSIGNALED(status))
		return (Result{Crash, std::string("crashed: ") + strsignal(WTERMSIG(status))});
	if (WIFEXITED(status) && WEXITSTATUS(status) != 0)
	{
		std::string	line = sanitizerLine(output);
		return (Result{Crash, line.empty() ? "exited with status " + std::to_string(WEXITSTATUS(status)) : line});
	}
	if (reply.empty())
		return (Result{Crash, "ended without sending a result"});
	return (Result{reply[0] == 'P' ? Pass : Fail, reply.substr(1)});
}

/*
=============================================================================
	setup
=============================================================================
*/

Tester::Tester(int argc, char **argv)
	: _verbose(false), _quiet(false), _exitFirst(false), _list(false), _help(false), _fork(true),
	  _timeout(2), _current(nullptr), _counting(false), _headerShown(false), _stopped(false),
	  _marks(0), _capture(nullptr)
{
	g_colour = isatty(STDOUT_FILENO) && std::getenv("NO_COLOR") == nullptr;
	for (int i = 1; i < argc; i++)
	{
		std::string	arg = argv[i];

		if (arg == "-v" || arg == "--verbose")
			_verbose = true;
		else if (arg == "-q" || arg == "--quiet")
			_quiet = true;
		else if (arg == "-x" || arg == "--exitfirst")
			_exitFirst = true;
		else if (arg == "-l" || arg == "--list")
			_list = true;
		else if (arg == "-h" || arg == "--help")
			_help = true;
		else if (arg == "--no-fork")
			_fork = false;
		else if (arg == "--no-color" || arg == "--no-colour")
			g_colour = false;
		else if (arg == "--color" || arg == "--colour")
			g_colour = true;
		else if (arg == "-f" || arg == "--filter" || arg == "-t" || arg == "--timeout" || arg == "--facts")
		{
			if (i + 1 == argc)
			{
				_usageError = arg + " needs a value";
				break;
			}
			std::string	value = argv[++i];
			if (arg == "-f" || arg == "--filter")
				_filter = lower(value);
			else if (arg == "--facts")
				_factsPath = value;
			else if (!value.empty() && value.size() < 6
				&& value.find_first_not_of("0123456789") == std::string::npos)
				_timeout = static_cast<unsigned>(std::stoul(value));
			else
				_usageError = "--timeout takes whole seconds, not '" + value + "'";
		}
		else if (arg.size() > 1 && arg[0] == '-')
			_usageError = "unknown option '" + arg + "'";
		else
			_wanted.push_back(arg);
	}
	if (_verbose && _quiet)
		_usageError = "--verbose and --quiet don't go together";
}

Tester::~Tester()
{
	if (_capture)
		std::fclose(_capture);
}

void	Tester::add(int step, const std::string &name, const std::string &title, void (*tests)(Tester &))
{
	_sections.push_back(Section{step, name, title, tests, true, 0, 0, 0, 0, 0});
}

void	Tester::known(const std::string &note)
{
	_knownNote = note;
}

/* the sections named on the command line, by name or step number */
bool	Tester::selectSections()
{
	if (_wanted.empty())
		return (true);
	for (Section &s : _sections)
		s.selected = false;
	for (const std::string &want : _wanted)
	{
		bool	found = false;

		for (Section &s : _sections)
			if (lower(want) == s.name || want == std::to_string(s.step))
			{
				s.selected = true;
				found = true;
			}
		if (!found)
		{
			std::cerr << red("tester: ") << "there is no section '" << want << "'. there are:";
			for (const Section &s : _sections)
				std::cerr << " " << s.name;
			std::cerr << "\n";
			return (false);
		}
	}
	return (true);
}

/*
=============================================================================
	running everything
=============================================================================
*/

int	Tester::run()
{
	if (!_usageError.empty())
	{
		std::cerr << red("tester: ") << _usageError << "\n";
		printUsage(std::cerr);
		return (2);
	}
	if (_help)
	{
		printUsage(std::cout);
		return (0);
	}
	if (!_factsPath.empty())
		return (printFacts(_factsPath));
	if (!selectSections())
		return (2);
	if (_list)
		return (listSections());
	if (_fork)
		_capture = std::tmpfile();

	printBanner();
	auto	start = std::chrono::steady_clock::now();
	for (Section &s : _sections)
	{
		if (!s.selected)
			continue;
		if (_stopped)
			break;
		_current = &s;
		_headerShown = false;
		try
		{
			s.tests(*this);
		}
		catch (const std::exception &e)
		{
			sectionHeader();
			std::cout << "    " << red("✗ the section itself threw while setting up its tests: ") << e.what() << "\n";
			s.failed++;
		}
		sectionFooter();
	}
	std::chrono::duration<double>	took = std::chrono::steady_clock::now() - start;
	printSummary(took.count());

	int	ran = 0;
	int	failed = 0;
	for (const Section &s : _sections)
	{
		ran += s.passed + s.failed + s.known + s.fixed;
		failed += s.failed;
	}
	return ((failed > 0 || ran == 0) ? 1 : 0);
}

void	Tester::check(const std::string &name, const std::string &input, const std::string &expected,
			const std::function<Result()> &body)
{
	std::string	knownNote = _knownNote;

	_knownNote.clear();
	if (!_current || _stopped)
		return;
	if (!_filter.empty() && lower(name).find(_filter) == std::string::npos)
		return;
	if (_counting)
	{
		_current->counted++;
		return;
	}
	sectionHeader();

	std::string	output;
	auto		start = std::chrono::steady_clock::now();
	Result		result = _fork ? runIsolated(body, output) : guarded(body);
	std::chrono::duration<double>	took = std::chrono::steady_clock::now() - start;

	report(name, input, expected, result, output, knownNote, took.count());
}

/*
=============================================================================
	reporting
=============================================================================
*/

void	Tester::printBanner() const
{
#ifdef TESTER_SANITIZERS
	std::string	mode = "ASan + UBSan";
#else
	std::string	mode = "no sanitizers";
#endif
	if (_fork)
		mode += " · each test in its own process"
			+ (_timeout ? " · " + std::to_string(_timeout) + "s limit" : std::string(" · no time limit"));
	else
		mode += " · all tests in this process (--no-fork)";
	std::cout << "\n  " << bold("webserv") << dim(" · ") << bold("config parser tests") << "   " << dim(mode) << "\n";
	if (!_filter.empty())
		std::cout << "  " << dim("only tests whose name contains ") << "'" << _filter << "'\n";
}

void	Tester::sectionHeader()
{
	if (_headerShown)
		return;
	_headerShown = true;
	_marks = 0;
	_deferred.clear();
	if (_quiet)
		std::cout << "  " << paint("1;36", padLeft(std::to_string(_current->step), 2) + "  "
			+ padRight(_current->name, 10)) << "  " << std::flush;
	else
		std::cout << "\n  " << paint("1;36", std::to_string(_current->step) + " · " + _current->name)
			<< dim("  " + _current->title) << "\n";
}

void	Tester::sectionFooter()
{
	if (!_headerShown || !_quiet)
		return;

	const Section	&s = *_current;
	std::string		count = "  " + std::to_string(s.passed + s.fixed) + "/"
						+ std::to_string(s.passed + s.fixed + s.failed);

	std::cout << (s.failed ? red(count) : dim(count)) << "\n" << _deferred;
}

std::string	Tester::details(const std::string &input, const std::string &expected, const Result &result,
				const std::string &output, bool failed) const
{
	std::string	out = showInput(input) + showText("expected", expected, nullptr)
					+ showText("got", result.got, failed ? &expected : nullptr);

	if (output.empty())
		return (out);
	if (!_verbose && result.outcome == Crash && output.find("ERROR: AddressSanitizer") != std::string::npos)
		return (out + showOutput(shortReport(output), 25));
	return (out + showOutput(output, _verbose ? 400 : 25));
}

void	Tester::report(const std::string &name, const std::string &input, const std::string &expected,
			const Result &result, const std::string &output, const std::string &knownNote, double seconds)
{
	enum Kind { Ok, Bad, Known, Fixed };

	bool	passed = result.outcome == Pass;
	bool	broke = result.outcome == Crash || result.outcome == Timeout;
	Kind	kind = Ok;

	if (knownNote.empty() || broke)
		kind = passed ? Ok : Bad;
	else
		kind = passed ? Fixed : Known;

	std::string	mark;
	std::string	title = name;
	std::string	tag;

	switch (kind)
	{
		case Ok:
			mark = green("✓");
			_current->passed++;
			break;
		case Bad:
			mark = broke ? magenta("✗") : red("✗");
			title = paint("1;31", name);
			if (result.outcome == Crash)
				tag = magenta("  CRASH");
			else if (result.outcome == Timeout)
				tag = magenta("  TIMEOUT");
			_current->failed++;
			break;
		case Known:
			mark = yellow("!");
			tag = yellow("  known issue") + dim(" · " + knownNote);
			_current->known++;
			_notes.push_back(Note{_current->name, name, knownNote, false});
			break;
		case Fixed:
			mark = cyan("✓");
			tag = cyan("  passes now - delete its t.known() line");
			_current->fixed++;
			_notes.push_back(Note{_current->name, name, knownNote, true});
			break;
	}
	if (seconds >= 0.5)
		tag += dim("  (" + secondsText(seconds) + ")");

	std::string	shownDetails;
	if (kind == Bad || _verbose)
		shownDetails = details(input, expected, result, output, kind == Bad);

	if (_quiet)
	{
		if (_marks > 0 && _marks % 50 == 0)
			std::cout << "\n" << std::string(18, ' ');
		std::cout << mark << std::flush;
		_marks++;
		if (kind == Bad || kind == Fixed)
			_deferred += "      " + mark + " " + title + tag + "\n" + shownDetails;
	}
	else
		std::cout << "    " << mark << " " << title << tag << "\n" << shownDetails << std::flush;

	if (kind == Bad && _exitFirst)
		_stopped = true;
}

void	Tester::printSummary(double seconds) const
{
	const std::string	rule = "  " + dim(repeat("─", 62));
	int					passed = 0;
	int					failed = 0;
	int					known = 0;

	std::cout << "\n" << rule << "\n";
	for (const Section &s : _sections)
	{
		int	ran = s.passed + s.failed + s.known + s.fixed;

		if (!s.selected || ran == 0)
			continue;
		passed += s.passed + s.fixed;
		failed += s.failed;
		known += s.known;

		std::string	count = padLeft(std::to_string(s.passed + s.fixed), 4) + " / "
						+ padRight(std::to_string(s.passed + s.fixed + s.failed), 4);
		std::cout << "  " << padLeft(std::to_string(s.step), 2) << "  " << padRight(s.name, 10) << count
			<< "  " << (s.failed ? red("✗ " + std::to_string(s.failed) + " failed") : green("✓"));
		if (s.known)
			std::cout << yellow("  ! " + std::to_string(s.known) + " known");
		if (s.fixed)
			std::cout << cyan("  " + std::to_string(s.fixed) + " fixed?");
		std::cout << "\n";
	}
	std::cout << rule << "\n";
	std::cout << "  " << green(std::to_string(passed) + " passed") << dim(" · ")
		<< (failed ? paint("1;31", std::to_string(failed) + " failed") : dim("0 failed"));
	if (known)
		std::cout << dim(" · ") << yellow(plural(static_cast<size_t>(known), "known issue"));
	std::cout << dim("  ·  " + secondsText(seconds)) << "\n";

	if (!_notes.empty())
	{
		std::cout << "\n  " << yellow("known issues") << dim(" - not failures, but not forgotten either") << "\n";
		for (const Note &n : _notes)
		{
			std::cout << "    " << (n.fixed ? cyan("✓") : yellow("!")) << " " << n.section << dim(" · ") << n.test << "\n";
			std::cout << "      " << dim(n.note) << (n.fixed ? cyan("  → passes now: delete its t.known() line") : "") << "\n";
		}
	}
	if (_stopped)
		std::cout << "\n  " << yellow("stopped at the first failure (-x)") << "\n";
	if (passed + failed + known == 0)
		std::cout << "\n  " << yellow("no tests ran")
			<< (_filter.empty() ? "" : " - nothing matches -f '" + _filter + "'") << "\n\n";
	else if (failed)
		std::cout << "\n  " << paint("1;31", "✗ " + std::to_string(failed) + " FAILED") << "\n\n";
	else
		std::cout << "\n  " << paint("1;32", "✓ ALL PASSED") << "\n\n";
}

int	Tester::listSections()
{
	_counting = true;
	std::cout << "\n  " << dim("step  section     tests") << "\n";
	for (Section &s : _sections)
	{
		if (!s.selected)
			continue;
		_current = &s;
		try
		{
			s.tests(*this);
		}
		catch (const std::exception &)
		{
		}
		std::cout << "  " << padLeft(std::to_string(s.step), 4) << "  " << cyan(padRight(s.name, 10))
			<< padLeft(std::to_string(s.counted), 7) << "   " << dim(s.title) << "\n";
	}
	std::cout << "\n";
	_counting = false;
	return (0);
}

/*
--facts FILE: every fact the parser makes of FILE, grouped by server and
location, ready to copy into a parses() call.
*/
int	Tester::printFacts(const std::string &path) const
{
	std::string	text;

	try
	{
		if (path == "-")
		{
			std::ostringstream	in;
			in << std::cin.rdbuf();
			text = in.str();
		}
		else
			text = readFile(access(path.c_str(), R_OK) == 0 ? path : repoPath(path));
	}
	catch (const std::exception &e)
	{
		std::cerr << red("tester: ") << e.what() << "\n";
		return (2);
	}

	std::vector<ServerConfig>	servers;
	try
	{
		servers = ConfigParser::parseString(text);
	}
	catch (const ConfigError &e)
	{
		std::cout << "  " << red(e.what()) << "\n";
		return (1);
	}

	std::string	group;
	for (const std::pair<std::string, std::string> &fact : configFacts(servers))
	{
		std::string	thisGroup = fact.first.substr(0, fact.first.rfind(' ') + 1);

		if (thisGroup != group)
			std::cout << "\n";
		group = thisGroup;
		std::cout << "  " << cyan(fact.first) << " = " << fact.second << "\n";
	}
	std::cout << "\n";
	return (0);
}

void	Tester::printUsage(std::ostream &out) const
{
	out << "\n" << bold("usage") << ": ./tester [options] [section ...]\n\n";
	out << "  " << bold("sections") << dim("   a name or a step number - none means all") << "\n";
	for (const Section &s : _sections)
		out << "    " << padLeft(std::to_string(s.step), 2) << "  " << cyan(padRight(s.name, 11)) << dim(s.title) << "\n";
	out << "\n  " << bold("options") << "\n"
		<< "    -v, --verbose        input, expected and got for every test, not only failures\n"
		<< "    -q, --quiet          one mark per test; details only for failures\n"
		<< "    -f, --filter TEXT    only tests whose name contains TEXT (any case)\n"
		<< "    -x, --exitfirst      stop at the first failure\n"
		<< "    -l, --list           how many tests each section has\n"
		<< "    -t, --timeout SEC    time limit per test, default 2 (0 = none)\n"
		<< "        --facts FILE     what the parser makes of FILE, as facts for parses()\n"
		<< "                         (\"-\" reads the config from stdin)\n"
		<< "        --no-fork        run every test in this process, for gdb/lldb\n"
		<< "                         (a crash or an endless loop then stops everything)\n"
		<< "        --no-color       plain output (also NO_COLOR=1, or when piped)\n"
		<< "    -h, --help           this\n\n"
		<< "  " << bold("examples") << "\n"
		<< "    ./tester                                   everything\n"
		<< "    ./tester values -f listen                  only the listen tests of step 5\n"
		<< "    ./tester 2 3 -q                            tokenizer and cursor, compact\n"
		<< "    ./tester --facts conf/default.conf\n"
		<< "    echo 'server { listen 80; }' | ./tester --facts -\n\n";
}
