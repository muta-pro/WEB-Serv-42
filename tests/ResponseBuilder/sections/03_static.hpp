#pragma once

#include "Tester.hpp"
#include "../harness/Harness.hpp"
#include "http/ResponseBuilder.hpp"

#include <dirent.h>
#include <sys/types.h>
#include <unistd.h>
#include <functional>
#include <string>
#include <utility>
#include <vector>

/*
STEP 3 - StaticFile. the router says "this is a file, here is its path"
(route.filesystemPath); the handler decides 200, 404 or 403 and reads it.

	the groups:
	the cards           t1 t2 t3 t4 t19 from the Lab, as on the pipeline page
	404 or 403          missing is 404; there but not servable is 403
	the bytes           every byte, in order, NULs included, however big
	Content-Type        from the file name's extension
	file descriptors    none left open, whatever the answer

how the handler decides, before it reads a byte (handover §5):
	stat() fails                          404   nothing there
	stat() ok, not S_ISREG                403   a folder, a FIFO, a device
	access(R_OK) fails                    403   there, but not ours to read
	otherwise                             read it all, 200
the subject forbids looking at errno after a read or a write, so the
answer is decided by stat() and access(), never by why read() failed.

a FIFO is the dangerous one: open() on a FIFO for reading waits until
someone opens the other end, and there is one thread for every client.
the runner gives each test 2 s, so a handler that opens it shows up here
as a timeout instead of freezing the run.

for 404 and 403 the tests ask only for the status. those answers should go
through buildErrorResponse(code, route), not straight to makeError: step 7
then gives them the server's custom pages with no change here.

Content-Length is never the handler's job: toBytes counts the body. the
HEAD test (t19) checks that the handler builds the full GET response and
toBytes drops the bytes.
*/

/* how many file descriptors this process has open. /dev/fd lists them, on Linux and on macOS */
inline int	openFds()
{
	DIR	*dir = opendir("/dev/fd");
	int	count = 0;

	if (!dir)
		return (-1);
	while (readdir(dir))
		count++;
	closedir(dir);
	return (count);
}

/* the 256 byte values, 0 to 255, once each: NUL, CR, LF and 0xff included */
inline std::string	everyByte()
{
	std::string	bytes;

	for (int i = 0; i < 256; i++)
		bytes += static_cast<char>(i);
	return (bytes);
}

/* size bytes that don't repeat with any short period, so a chunk read twice or skipped shows */
inline std::string	patterned(size_t size)
{
	std::string	bytes(size, '\0');

	for (size_t i = 0; i < size; i++)
		bytes[i] = static_cast<char>((i * 7 + i / 251) % 256);
	return (bytes);
}

inline void	testStatic(Tester &t)
{
	/* ------------------------------------------------ the cards */

	responds(t, "t1 · a readable file is 200, with its bytes",
		Scene("GET /index.html").route(RouteAction::StaticFile, "/")
			.file("www/html/index.html", std::string(1337, 'x')),
		{"status = 200", "header Content-Type = text/html",
		 "body same as www/html/index.html = yes"});
	responds(t, "t2 · a missing file is 404",
		Scene("GET /logo.png").route(RouteAction::StaticFile, "/"),
		{"status = 404"});
	if (Sandbox::asRoot())
		t.known("running as root: access() ignores the mode, so a 403 test can't fail for the right reason");
	responds(t, "t3 · a file the server may not read is 403",
		Scene("GET /secret.txt").route(RouteAction::StaticFile, "/")
			.file("www/html/secret.txt", "s", 0000),
		{"status = 403"});
	responds(t, "t4 · a FIFO is not a file: 403, and no hang",
		Scene("GET /pipe").route(RouteAction::StaticFile, "/")
			.fifo("www/html/pipe"),
		{"status = 403"});
	responds(t, "t19 · HEAD: the GET response, toBytes drops the bytes",
		Scene("HEAD /index.html").route(RouteAction::StaticFile, "/")
			.file("www/html/index.html", std::string(1337, 'x')),
		{"status = 200", "body size = 1337",
		 "wire header Content-Length = 1337", "wire body size = 0"});

	/* ------------------------------------------------ 404 or 403 */

	/*
	the router sends Directory for a folder, so a folder reaching StaticFile
	is a bug upstream. it still needs an answer: not S_ISREG, 403.
	*/
	responds(t, "a folder is not a file: 403",
		Scene("GET /docs").route(RouteAction::StaticFile, "/")
			.dir("www/html/docs"),
		{"status = 403"});
	responds(t, "a missing folder on the way: 404",
		Scene("GET /nope/logo.png").route(RouteAction::StaticFile, "/"),
		{"status = 404"});
	/* stat() on www/html/index.html/logo.png fails (ENOTDIR): nothing is there, so 404, as nginx answers */
	responds(t, "a file used as a folder: 404",
		Scene("GET /index.html/logo.png").route(RouteAction::StaticFile, "/")
			.file("www/html/index.html", "<h1>hi</h1>"),
		{"status = 404"});

	/* ------------------------------------------------ the bytes */

	responds(t, "an empty file: 200, with an empty body",
		Scene("GET /empty.txt").route(RouteAction::StaticFile, "/")
			.file("www/html/empty.txt", ""),
		{"status = 200", "body size = 0", "wire header Content-Length = 0"});
	/* a body built with c_str() or strlen() stops at the first NUL: byte 0 here */
	responds(t, "every byte value, NUL included, comes back as it is on disk",
		Scene("GET /bytes.bin").route(RouteAction::StaticFile, "/")
			.file("www/html/bytes.bin", everyByte()),
		{"status = 200", "body size = 256", "body same as www/html/bytes.bin = yes"});
	/* read() may return fewer bytes than asked: a single read, or a buffer appended twice, shows here */
	responds(t, "a file bigger than one read() comes back whole",
		Scene("GET /big.bin").route(RouteAction::StaticFile, "/")
			.file("www/html/big.bin", patterned(300000)),
		{"status = 200", "body size = 300000", "body same as www/html/big.bin = yes"});
	responds(t, "a file in another location's root",
		Scene("GET /files/a.txt").route(RouteAction::StaticFile, "/files/")
			.file("www/files/a.txt", "a"),
		{"status = 200", "body = a"});
	/*
	route.filesystemPath is C's decision: decoded, normalised, checked
	against the root. the handler serves that path and never rebuilds one
	from request.path.
	*/
	responds(t, "the handler serves route.filesystemPath, not the URL",
		Scene("GET /anything").route(RouteAction::StaticFile, "/")
			.filesystemPath("./www/html/real.txt").file("www/html/real.txt", "real"),
		{"status = 200", "body = real"});
	responds(t, "a GET leaves the file as it was",
		Scene("GET /index.html").route(RouteAction::StaticFile, "/")
			.file("www/html/index.html", std::string(1337, 'x')),
		{"disk www/html/index.html = file, 1337 bytes"});

	/* ------------------------------------------------ Content-Type */

	const std::vector<std::pair<std::string, std::string>>	types = {
		{"html", "text/html"}, {"css", "text/css"}, {"txt", "text/plain"},
		{"png", "image/png"}, {"jpg", "image/jpeg"}, {"jpeg", "image/jpeg"},
		{"gif", "image/gif"}, {"svg", "image/svg+xml"},
		{"pdf", "application/pdf"}, {"json", "application/json"}};

	for (const std::pair<std::string, std::string> &type : types)
		responds(t, "Content-Type of a ." + type.first + " file: " + type.second,
			Scene("GET /f." + type.first).route(RouteAction::StaticFile, "/")
				.file("www/html/f." + type.first, "x"),
			{"status = 200", "header Content-Type = " + type.second});
	responds(t, "the extension is what follows the last dot",
		Scene("GET /page.backup.html").route(RouteAction::StaticFile, "/")
			.file("www/html/page.backup.html", "x"),
		{"status = 200", "header Content-Type = text/html"});

	/*
	the default type, decided 2026-11-09: application/octet-stream, as in
	nginx's stock nginx.conf. the browser offers a download instead of
	guessing what the bytes are.
	*/
	responds(t, "no extension: the default type, application/octet-stream",
		Scene("GET /README").route(RouteAction::StaticFile, "/")
			.file("www/html/README", "x"),
		{"status = 200", "header Content-Type = application/octet-stream"});
	responds(t, "an extension the table doesn't know: the default type",
		Scene("GET /f.webserv").route(RouteAction::StaticFile, "/")
			.file("www/html/f.webserv", "x"),
		{"status = 200", "header Content-Type = application/octet-stream"});
	/* the extension belongs to the file name: the dot in v1.2 is part of a folder's name */
	responds(t, "a dot in a folder's name is not an extension",
		Scene("GET /v1.2/readme").route(RouteAction::StaticFile, "/")
			.file("www/html/v1.2/readme", "x"),
		{"status = 200", "header Content-Type = application/octet-stream"});

	/* ------------------------------------------------ too big to read */

	/*
	the whole file goes into one std::string, so the handler needs a limit,
	decided 2026-11-09: over it, 500. the file and the request are fine; it
	is the server that can't serve it. the limit itself is Ravi's to set in
	the code.

	the test file is sparse: truncate() makes it 4 GiB long without writing
	a byte, so it costs no disk. a handler that checks st_size before it
	reads answers at once; one that reads first tries to allocate 4 GiB,
	which AddressSanitizer stops with "allocation-size-too-big" (a crash),
	and which without it would stall the only thread for every client.
	*/
	t.test("a file over the size limit: 500, decided before reading",
		"GET /huge.iso · StaticFile, location / · www/html/huge.iso: 4 GiB, sparse",
		gives("status 500"), [] {
		Sandbox			box;

		box.file("www/html/huge.iso", "");
		if (truncate("www/html/huge.iso", static_cast<off_t>(4) << 30) != 0)
			return (std::string("truncate() failed: the sandbox can't hold a sparse file"));

		Site			site(defaultConfig());
		HttpRequest		req = request("GET /huge.iso");
		RouteResult		route = site.route(RouteAction::StaticFile, req, "/");
		HttpResponse	response = buildResponse(req, route);

		return ("status " + std::to_string(response.statusCode));
	});

	/* ------------------------------------------------ file descriptors */

	/*
	every way out of the handler closes what it opened: a server that leaks
	one fd per request stops accepting connections after about a thousand.
	the count is taken right around buildResponse, so nothing else in the
	test can move it.
	*/
	auto	noFdLeft = [&t](const std::string &name, const std::string &line,
						std::function<void(const Sandbox &)> disk) {
		t.test(name, line + " · StaticFile, location /", gives("as many open fds after buildResponse as before"),
			[line, disk] {
			Sandbox			box;

			disk(box);
			Site			site(defaultConfig());
			HttpRequest		req = request(line);
			RouteResult		route = site.route(RouteAction::StaticFile, req, "/");
			int				before = openFds();
			HttpResponse	response = buildResponse(req, route);
			int				after = openFds();

			(void)response;
			if (before < 0)
				return (std::string("can't list /dev/fd"));
			if (after != before)
				return (std::to_string(after - before) + " more open after buildResponse than before");
			return (std::string("as many open fds after buildResponse as before"));
		});
	};

	noFdLeft("no fd left open after a 200", "GET /index.html",
		[](const Sandbox &box) { box.file("www/html/index.html", "<h1>hi</h1>"); });
	noFdLeft("no fd left open after a 404", "GET /logo.png",
		[](const Sandbox &) {});
	noFdLeft("no fd left open after a 403 for a folder", "GET /docs",
		[](const Sandbox &box) { box.dir("www/html/docs"); });
	noFdLeft("no fd left open after a 403 for a FIFO", "GET /pipe",
		[](const Sandbox &box) { box.fifo("www/html/pipe"); });
}
