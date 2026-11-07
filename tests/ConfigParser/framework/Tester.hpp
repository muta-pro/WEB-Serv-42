#pragma once

#include <cstddef>
#include <cstdio>
#include <functional>
#include <ostream>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "config/ConfigParser.hpp"

/*
the runner behind the config parser tests.

	the sections (sections/NN_name.hpp) only call the CHECKS below. the rest
	of this class - the command line, running every test in its own process,
	the colours, the summary - is machinery, and you shouldn't need to touch
	it to add tests. how to add a test or a section is at the top of main.cpp.
*/

/*
what a test expects to get back:
	gives("1024")                               a value, written as text
	fails(7, "unknown size suffix in '10X'")    a ConfigError whose what() is
	                                            "config error, line 7: unknown size suffix in '10X'"
*/
struct Expect
{
	std::string	text;
};

Expect	gives(const std::string &value);
Expect	fails(size_t line, const std::string &message);

class Tester
{
	public:
		Tester(int argc, char **argv);
		~Tester();
		Tester(const Tester &) = delete;
		Tester	&operator=(const Tester &) = delete;

		/* main.cpp: register every section, then run the ones asked for */
		void	add(int step, const std::string &name, const std::string &title, void (*tests)(Tester &));
		int		run();

		/*
		CHECKS - every call is one test.

		test      body returns its result as text. a ConfigError thrown
		          inside becomes its what(). input is only shown, never used.
		parses    parseString(config) must succeed, and every fact
		          ("key = value") must hold. ./tester --facts FILE prints
		          all the facts of a config, ready to copy.
		rejects   parseString(config) must throw ConfigError(line, message).
		...File   the same for a file in the repo, e.g. "conf/default.conf",
		          read by ConfigParser::parseFile.
		*/
		void	test(const std::string &name, const std::string &input, const Expect &expect,
					const std::function<std::string()> &body);
		void	parses(const std::string &name, const std::string &config,
					const std::vector<std::string> &facts);
		void	rejects(const std::string &name, const std::string &config,
					size_t line, const std::string &message);
		void	parsesFile(const std::string &path, const std::vector<std::string> &facts);
		void	rejectsFile(const std::string &path, size_t line, const std::string &message);

		/*
		marks the NEXT check as a known issue: shown in yellow, listed in the
		summary, but not counted as a failure. when it starts passing, the
		summary tells you to delete the known() line.
		*/
		void	known(const std::string &note);

		/* "conf/default.conf" -> its absolute path in the repo */
		static std::string	repoPath(const std::string &relative);
		/* the whole file. throws std::runtime_error if it can't be read */
		static std::string	readFile(const std::string &path);

	private:
		enum Outcome { Pass, Fail, Crash, Timeout };

		struct Result
		{
			Outcome		outcome;
			std::string	got;
		};

		struct Section
		{
			int			step;
			std::string	name;
			std::string	title;
			void		(*tests)(Tester &);
			bool		selected;
			int			passed;
			int			failed;
			int			known;		// known issues that still fail
			int			fixed;		// known issues that pass now
			int			counted;	// --list
		};

		struct Note
		{
			std::string	section;
			std::string	test;
			std::string	note;
			bool		fixed;
		};

		/* options */
		bool						_verbose;
		bool						_quiet;
		bool						_exitFirst;
		bool						_list;
		bool						_help;
		bool						_fork;
		unsigned					_timeout;
		std::string					_filter;
		std::string					_factsPath;
		std::vector<std::string>	_wanted;
		std::string					_usageError;

		/* state while running */
		std::vector<Section>		_sections;
		Section						*_current;
		bool						_counting;
		bool						_headerShown;
		bool						_stopped;
		std::string					_knownNote;
		std::vector<Note>			_notes;
		int							_marks;		// --quiet: marks printed in this section
		std::string					_deferred;	// --quiet: failures, printed after the marks
		std::FILE					*_capture;	// collects what a test prints

		void	check(const std::string &name, const std::string &input, const std::string &expected,
					const std::function<Result()> &body);
		void	parsesWith(const std::string &name, const std::string &input,
					const std::function<std::vector<ServerConfig>()> &parse, const std::vector<std::string> &facts);

		Result			runIsolated(const std::function<Result()> &body, std::string &output);
		static Result	guarded(const std::function<Result()> &body);

		void		report(const std::string &name, const std::string &input, const std::string &expected,
						const Result &result, const std::string &output, const std::string &knownNote,
						double seconds);
		std::string	details(const std::string &input, const std::string &expected, const Result &result,
						const std::string &output, bool failed) const;
		void		sectionHeader();
		void		sectionFooter();
		bool		selectSections();
		int			listSections();
		int			printFacts(const std::string &path) const;
		void		printBanner() const;
		void		printUsage(std::ostream &out) const;
		void		printSummary(double seconds) const;
};

/* the parsed config as "key = value" facts, in file order (Checks.cpp) */
std::vector<std::pair<std::string, std::string>>	configFacts(const std::vector<ServerConfig> &servers);

/* "GET HEAD POST DELETE" - always in this order, whatever order the set keeps */
std::string	methodsText(const std::set<HttpMethod> &methods);
