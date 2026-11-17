# Response side (builder + serializer): work context (Ravi)

> **For Claude:** this is a handover file from earlier sessions, the response
> side's twin of `config-work-context.md`. Read all of it before helping. It
> describes what Ravi is building, what has been decided, what is still open,
> and how Ravi works. The repo is the source of truth for code. If anything
> here disagrees with the code, trust the code and say so.

Last updated: 2026-11-17 · Branch: `response` (made from `main`) · Issue: #9

---

## 1. Who and how

- **Team since 2026-10-28:** Ivan (A · network & runtime), Ravi (B · config &
  response), Paola (C · HTTP parsing & routing). The full split and the
  handoffs are in `config-work-context.md` §1. A request goes A → C → B → A.
  B's per-request job: `buildResponse` / `buildErrorResponse` turn C's
  `HttpRequest` + `RouteResult` into an `HttpResponse`, and `toBytes` turns
  that into the bytes A sends. For CGI, A runs the script and B turns its
  output into a response.
- **Ravi's files here:** `includes/http/HttpResponse.hpp`,
  `src/http/HttpResponse.cpp`, `includes/http/ResponseBuilder.hpp`,
  `src/http/ResponseBuilder.cpp`, new B files as they come (MIME table,
  autoindex, CGI output parsing), and `tests/ResponseBuilder/`. CGI output
  parsing has no file yet: `includes/cgi/CgiHandler.hpp` is empty and
  `src/cgi/CgiHandler.cpp` holds A's `CgiProcess`.
- **Not Ravi's:** A: `network/`, `main.cpp`, `CgiProcess`. C: `HttpRequest`,
  `HttpRequestParser`, `ParseResult`, `Router`, `RouteResult`. Shared:
  `HeaderMap`, `CaseInsensitiveLess`, `HttpMethod.hpp`. **Shared headers and
  the root Makefile change only after the team agrees.** Check with Ravi
  before editing anything outside B.
- **Tutor, not coder** (42 policy, restated by Ravi on 2026-11-07). Explain
  the mechanics, ask guiding questions, point out what to think about. Don't
  write implementation code or hand over solutions: Ravi writes all of it,
  test helpers and mocks included. Review his code by reading it and running
  the suite; when a test fails, start with a guiding question, not the fix.
  The config side ran differently (Claude wrote scaffolds and test sections,
  see `config-work-context.md` §1). Ask Ravi before falling back to that.
- **2026-11-07, later the same day:** Ravi asked Claude to build the step 1
  harness and the pipeline page "the same way as for the config parser".
  Claude built both (§7–§9), plus the `buildResponse` stub the suite needs
  to link. Ravi still writes the builder itself.
- **Test sections: settled 2026-11-09.** Ravi chose the config way for the
  build path: Claude writes each step's test section first, walks Ravi
  through what it checks and why, and Ravi writes the builder code until it
  passes. Claude still writes no builder code (no reference implementation
  either, not even a throwaway one: on 2026-11-09 Ravi declined one that was
  meant only to prove the step 2 tests passable).
- **Ravi had never used a `switch`** (2026-11-09). Don't assume a C++
  construct is known: when a step needs one, explain it from zero, with an
  example from this repo.
- **Ravi learns by doing.** Reading-only material doesn't stick, which is why
  the Lab exists. Keep each explanation short and pair it with something to
  do: a Lab drill, a test to write, a `curl` to run. Recap C++ with examples
  from this repo when something may have faded. Pace: one build step at a
  time, at the pace of the C++ lessons.
- **Don't fix contradictions** between `docs/BASE_LAYER_ARCHITECTURE.md` and
  the code without asking. Flag them (§4 lists the known ones).
- **C++ standard:** the contracts and every Makefile (the root one and both
  test suites) use `-std=c++17`. Ravi has said C++17+ is allowed for the team
  despite the subject's C++98 line. Don't raise it again.
- **Study pages** (claude.ai):
  - Response Builder Field Guide, the reference; the build path in §6 comes
    from it: https://claude.ai/artifact/XR2b2TsUJRtEz6qveC5Eyj
  - Response Builder Lab: Build path tab with drills per step, Wire bench,
    Triage (23 cards), CGI parser, Bug hunt (12 snippets), Decision log:
    https://claude.ai/artifact/UWS9QXTssMdwWNMs633Xgc
  - Webserv Base Layer, the whole learning book (Part 8 is the response
    chapter): https://claude.ai/artifact/RP1y7UW3KjCghvB2ycFXfL. Its copy of
    the Field Guide is the older one, without the build path. The standalone
    page wins.
  - ResponseBuilder Pipeline: step through a Triage card or the wire bench
    in the real C++, like ConfigParser Pipeline (§9):
    https://claude.ai/artifact/8sH3WeiDjF2iYymd7dPmSB

## 2. Git workflow

Response work lives on its **own branch, `response`**, made from `main`.
It was first made from `config`, because the tests need the parser
(`ConfigParser::parseString` builds their configs) and the test framework.
The config PR (#8) was approved by Ivan and merged on 2026-11-07
(`80123d8`), so on 2026-11-09 `response` was moved onto `origin/main`.
It had no commits of its own yet and `main`'s files were identical to
`config`'s, so the uncommitted work came along unchanged
(`git reset --keep origin/main`). Same habits as `config`:

```sh
# first push only (done: a374447, 2026-11-09)
git push -u origin response        # tracks origin/response from now on

# every session
git pull
# ... work ...
git add <files> && git commit -m "response: ..."
git push

# now and then, to pick up Ivan's and Paola's merged work
git fetch origin
git merge origin/main              # conflicts, if any, show up here, on Ravi's branch
```

- Never set `response` to track `origin/main`: `git pull` would then pull
  `main` into it unasked. It tracks `origin/response` only.
- The PR from `response` into `main` now shows only response work, so it
  can be opened whenever Ravi wants a review.
- `Closes #9` in that PR's description closes the tracking issue on
  merge. This file lives in `docs/`, so it goes into `main` with the PR, like
  the config file.
- The `config` branch (local and `origin/config`) is merged and has no
  further use. Deleting it is Ravi's call: `git branch -d config` and
  `git push origin --delete config`.

## 3. Pipeline position

```
HttpRequest + RouteResult             from C
  -> buildResponse(request, route)    reads files, folders, error pages, CGI output
  -> HttpResponse                     a plain value: statusCode, headers, body
  -> toBytes(response, headRequest)   the only place bytes get written
  -> A's write buffer -> send()
```

| | Type | From | What matters to B |
|---|---|---|---|
| In | `const HttpRequest &` | C | `method` as a **string** ("GET", "HEAD"…), `path`, `query`, `headers`, `body` |
| In | `const RouteResult &` | C | `action`, `StatusCode`, `filesystemPath`, `redirectLocation`, `server`, `loc` |
| In | `ServerConfig`, `LocationConfig` | B, at startup | through `route.server` / `route.loc`; every field already final |
| In | `CgiProcess` | A | CGI only: `output()` and `state()` (Running, Complete, Failed, TimedOut) |
| Out | `HttpResponse` | B | `statusCode`, `headers`, `body` |
| Out | `std::string` | B | `toBytes(response, headRequest)`, into A's write buffer |

Three rules from the architecture doc:
1. `HttpResponse` stays a plain value. It never reads files, config, sockets
   or CGI; the builder does the reading.
2. `toBytes` never reads config or files.
3. Bad client input is a status code, never an exception. Exceptions are for
   programmer errors and startup errors.

`filesystemPath` arrives decoded, normalized and checked against the root by
C. That is the precondition of `resolvePath`; B doesn't redo it.

## 4. State of the response side

From the Field Guide's inventory (repo at `main` 37e497c, `config` a2dbb72).
Rows updated 2026-11-17 for steps 2–6:

| Piece | State | Notes |
|---|---|---|
| `HttpResponse` | done | `statusCode`, `headers` (case-insensitive `HeaderMap`), `body` (`std::string`, binary-safe) |
| `make(code, body)` | done | |
| `makeError(code)` | done | the built-in `<html><body><h1>404 Not Found</h1></body></html>` with `Content-Type: text/html`. The fallback whenever a custom page fails |
| `reasonFor(code)` | done | 24 codes. Step 4 added 303, 304, 307 and 308 and renamed 413 Content Too Large |
| `setHeader(name, value)` | done | throws `std::invalid_argument` on an empty name, a name with whitespace, `:`, CR or LF, or a value with CR or LF. Blocks header injection, and it is a throw B must catch |
| `toBytes(response, headRequest)` | done, tested | status line, CRLF headers, one `Content-Length` computed from the body (a supplied one is removed), no body and no length for 1xx, 204 and 304, HEAD keeps the length and drops the bytes. Pinned by `tests/contracts.cpp` lines 67–78 |
| `buildResponse`, `buildErrorResponse` | partly | the switch and catch from step 2. StaticFile, Directory and Redirect are done (steps 3, 5, 6); CGI, DeleteResource and Upload still throw. `buildErrorResponse` is only its `makeError` fallback until step 7 |
| `Date`, `Server` headers | done | added by `toBytes` (step 4) |
| error pages, uploads, DELETE, CGI output | missing | steps 7–9, §6 |

Flagged, not fixed:
- **Doc vs header.** The architecture doc calls three `RouteResult` members
  `location`, `statusCode` and `RouteAction::Cgi`; `includes/RouteResult.hpp`
  says `loc`, `StatusCode` and `RouteAction::CGI`. Write against the header.
- **`headers` is a public map**, so assigning into `headers[...]` skips
  `setHeader`'s injection check (book, Part 8.1). Either B's code only ever
  uses `setHeader` (Ravi's own rule), or `headers` goes private (a shared
  header, so a team decision). Open.
- **The book's Part 8 (2026-09-23)** shows three `toBytes` bugs: no
  `Content-Length`, a supplied one going out unchanged, and 304 without a
  reason phrase. The Field Guide's newer inventory has the first two fixed
  and pinned by the contract tests; step 4 added the 304 phrase.

## 5. Subject rules that shape B

From `en_subject_11.pdf` (version 24.0): §IV.1 Requirements, §IV.3 and the
function list. Paraphrased; check the PDF for the exact words.

- **No `errno` after a read or write** ("strictly forbidden"). So 404 or 403
  is decided with `stat()` and `access()` before reading, never from `errno`
  afterwards.
- **Regular disk files are exempt from `poll()`**: `read()` and `write()` on
  them need no readiness. Pipes and FIFOs are not exempt, and reading one
  without readiness is a 0. So the builder may read regular files directly,
  and must refuse anything that isn't one (a FIFO under the root would block
  the only thread). The Field Guide's step 0 says to agree this with Ivan:
  tell him the builder reads regular files synchronously.
- **Functions B leans on:** `access`, `stat`, `open`, `read`, `write`,
  `close`, `opendir`, `readdir`, `closedir`. `unlink` is not on the list, so
  DELETE needs another route; the Field Guide suggests `std::remove`. Settle
  it at step 8.
- Also in §IV.1: accurate status codes, default error pages when none are
  configured, a request never hangs, a fully static website, uploads, GET,
  POST and DELETE, nginx as the reference for headers and behaviour.
- §IV.3: CGI output without a content length ends at EOF.

## 6. Build path

From the Field Guide. One thin slice at a time. **A step is done when its
check passes**, not when its reading is ticked. Each step's reading and
drills are in the Field Guide and in the Lab's Build path tab.

- [ ] **0. Before you code** (about 35 min). The contracts
  (`RouteResult.hpp`, `ResponseBuilder.hpp`, `HttpResponse.hpp/.cpp` and the
  "Gaps" comment above `toBytes`), the subject rules in §5, and the open
  points for Ivan and Paola (the decision log at the end; the GitHub issue
  lists them too). Done when Ravi can say what `buildResponse` takes and
  returns, and what `toBytes` must never do.
- [x] **1. Test harness.** `tests/ResponseBuilder/` on the
  same framework, with a hand-made `HttpRequest`, a `ServerConfig` from
  `ConfigParser::parseString`, and a `RouteResult` into a temporary `www`.
  Done when one test builds a request and a route, calls `buildResponse`,
  and the suite runs green. See §7. **Done 2026-11-07:** 67 tests, green
  against the stub (built by Claude on Ravi's request, see §1).
- [x] **2. Skeleton.** A switch on `route.action` with a stub per case, a
  `default` that answers 500, and one `try`/`catch` of `std::exception`
  answering `makeError(500)`. Done when an unexpected action gets 500, and a
  handler that throws gets 500 with nothing escaping. **Tests:** section 2
  `skeleton` (`make skeleton`), written by Claude on 2026-11-09: 18 tests,
  12 failing against the step 1 stub for the right reason (it answers 200).
  The throwing test is a Redirect whose `redirectLocation` holds CR LF: at
  step 2 the Redirect stub decides it, from step 6 on `setHeader`'s throw.
  **Done 2026-11-09:** 85/85. Ravi's shape: the six handlers throw
  `std::runtime_error("not implemented yet")`; no `default:`, the 500 for
  an unknown action is the `return` after the `try`/`catch` (so `-Wswitch`
  still catches a missing case); the catch prints
  `buildResponse: <what()>` to `std::cerr`; `buildErrorResponse` is only
  its fallback, `makeError(statusCode)`, until step 7 adds the custom page
  above it.
- [x] **3. StaticFile.** `stat()` fails: 404; not `S_ISREG`, or
  `access(R_OK)` fails: 403. The whole file read in binary, appended by byte
  count; a MIME table with a default type (`application/octet-stream`,
  decided 2026-11-09); a size limit for files read into memory (over it:
  500, decided 2026-11-09; the limit itself is set in the code). Done when tests give 200, 404, and 403 for a folder and a FIFO,
  and live `curl -i localhost:8080/` gives 200, `curl -I` the same headers
  with no body, and an image opens in a browser. **Tests:** section 3
  `static` (`make static`), written by Claude on 2026-11-09: 33 tests, 27
  failing against step 2 for the right reason (the StaticFile stub throws,
  so 500). The 6 that pass already are guards: four "no fd left open", "a
  GET leaves the file as it was", and the size-limit test, which passes by
  accident until the handler reads (the stub's 500 is the answer it wants).
  **Done 2026-11-13** (the tests; the live `curl` checks wait for A's
  server): 33/33, 118/118 overall. Ravi's shape: `handleStaticFile` checks
  `stat` (404), `!S_ISREG` (403), `access(R_OK)` (403) and `st_size` against
  a 64 MB `static const off_t LIMIT` (500), then `readFile(path, body)` (an
  `open`/`read` loop that appends by count and closes on both ways out) and
  `mimeType(path)` (last dot after the last slash, a `static const std::map`
  of ten types, `application/octet-stream` otherwise). Every error goes
  through `buildErrorResponse`. Claude filled in the table's ten entries on
  Ravi's request.
- [x] **4. Finish `toBytes`.** `Date` as an IMF-fixdate (RFC 9110 §5.6.7),
  from `gmtime`, never `localtime`; 303, 307 and 308 in `reasonFor`; decide
  where `Date` and `Server` get added. Done when `curl -i` shows a `Date`
  ending in GMT, a test pins `reasonFor(307)`, and the contract tests still
  pass. **Decided 2026-11-13:** `toBytes` adds both, on its copy of the
  headers like `Content-Length`; the date formatting lives in a helper of
  Ravi's (the tests don't name it); `Server: webserv`; 304 Not Modified
  joins 303, 307 and 308; 413 becomes Content Too Large. **Tests:** section
  4 `tobytes` (`make tobytes`), written by Claude on 2026-11-13: 28 tests,
  20 failing against step 3 for the right reason. A Date test reads the
  clock just before and after `toBytes` and accepts either second; the
  reference date is built by hand from `gmtime_r` (pinned by four known
  dates), and one test sets `TZ=XYZ-14` so `localtime` would show.
  **Done 2026-11-13** (the tests; the live `curl -i` check waits for A's
  server): 28/28, 146/146 overall, root contract tests pass. Ravi's shape:
  `static std::string httpDate(std::time_t)` (`gmtime` + `strftime`), and
  `headers["Date"]` / `headers["Server"]` on `toBytes`'s copy, before the
  `Content-Length` `if`. Decided the same day: they **replace** a Date or
  Server the response already has (`=`, not `emplace`), as Content-Length
  does, so a CGI script's own never goes out; two tests pin that. Claude
  rewrote the "Gaps" comment above `toBytes` and made the `=` change on
  Ravi's request.
- [x] **5. Directory.** No trailing slash: 301 to the path plus "/", query
  kept. An index file: served like a StaticFile. Autoindex on: a listing
  that hides dot-files, HTML-escapes names, percent-encodes links, adds "/"
  to folders and a `../` link. Otherwise 403. The `DIR*` is closed on every
  way out. Done when `curl -i localhost:8080/files/docs` gives 301 with
  `Location: /files/docs/` and `/files/` gives a listing. **Decided
  2026-11-13:** relative links (`href="a.txt"`, `href="docs/"`), as nginx
  writes them; folders first, then files, each group in byte order; `%XX`
  with uppercase hex. The page's layout itself stays open. **Tests:**
  section 5 `directory` (`make directory`), written by Claude on
  2026-11-13: 27 tests, 22 failing against step 4 for the right reason.
  The listing tests pin links, their order, hidden entries, escaping and
  encoding through `links(body)` and `body has`, never the markup around
  them. The harness gained `body has <text>` and `openFds()` (moved out
  of section 3) for them.
  **Done 2026-11-17** (`76efd01`; the live `curl` checks wait for A's
  server): 27/27, 173/173 overall. Ravi's shape: `handleDirectory` answers
  the slash redirect itself (301, `Location` = path + "/" plus `?query`, the
  page written out), then `stat` (404), `dir + loc->index` served by
  `handleStaticFile` on a copy of the route, autoindex off or
  `access(R_OK)` failing: 403, `listDirectory` failing: 500.
  `listDirectory(dir, page)` reads with `opendir`/`readdir`, skips names
  starting with a dot, splits folders (by `stat`) from files, sorts each
  with `std::sort`, and builds the page with `percentEncode` (unreserved
  characters kept, `%XX` otherwise) for links and `htmlEscape`
  (`& < > "`) for names.
- [x] **6. Redirects.** `Location` from `route.redirectLocation`, the
  configured code, a short HTML body. Done when
  `curl -i localhost:8080/old/` gives 301 with `Location: /`. **Decided
  2026-11-17:** the code and the target come from the route
  (`route.StatusCode`, `route.redirectLocation`), never from `route.loc`:
  RouteResult's field rules make StatusCode the intended status, and the
  router may one day send a Redirect from a location with no `return` (the
  folder slash redirect, still open with Paola). `return` takes only 301,
  302, 303, 307 and 308; the parser refuses the rest of 3xx at startup (300
  offers a choice, 304 is a cache answer with no body, 305 and 306 are
  dead), so the handler never sees them. The target goes out as
  configured, relative or absolute, with nothing of the request added (no
  query, no rest of the path), as nginx's `return` does; `handleDirectory`'s
  slash redirect is the opposite, built from the request. No shared helper
  with `handleDirectory`'s 301 (Ravi's call). The page is built in the
  handler, not by `makeError`: the bytes would be the same today, but the
  comments above `makeError` plan error-only behaviour there (a custom
  `error_page`, `Connection: close`). **Tests:** section 6 `redirect`
  (`make redirect`), written by Claude on 2026-11-17: 13 tests, 12 failing
  against step 5 for the right reason (the Redirect stub throws, so 500).
  The one that passes does so by accident until the handler exists: a bare
  LF in the target must give 500, which the stub already gives. The config
  suite gained five failing `return` tests in section 7 (300, 304, 305,
  306, 399) and `conf/invalid/return_not_a_redirect.conf`; 200 and 404 keep
  the old "not between 300 and 399" message. **Done 2026-11-17** (the live
  `curl -i` check waits for A's server): 13/13, 186/186 overall, config
  suite 477/477. Ravi's shape: `handleRedirect` does `(void)req;`, builds
  `<html><body><h1>CODE PHRASE</h1></body></html>` with `reasonFor`, then
  `make`, `setHeader("Location", ...)` and `Content-Type: text/html`. The
  parser's `return` branch keeps `parseStatusCode(args[0], 300, 399)`, then
  throws `ConfigError` with "status code 'N' is not a redirect (301, 302,
  303, 307 or 308)" for anything outside the five.
- [ ] **7. Error pages. ← current step.** `buildErrorResponse`: `errorPageUri`, match the
  location, `resolvePath`, read the file. Keep the original code; one hop,
  then `makeError`; a null `route.server` goes straight to `makeError`.
  `Allow` on every 405 (from `loc->allowedMethods`, enum values mapped to
  names); `Connection: close` after 400, 408 and 413. Done when `/nope` gives
  404 with `www/errors/404.html`, `/errors/404.html` gives 404 (internal),
  and `curl -i -X DELETE localhost:8080/` gives 405 with `Allow: GET, HEAD`.
- [ ] **8. DELETE, then Upload.** DELETE: 204, 404, and 403 for folders,
  never recursive. Upload: the boundary from `Content-Type`, split the
  parts, keep only the last path part of the file name (refuse "", "." and
  ".."), 201 with `Location`, 409 or overwrite on a clash. Done when
  `curl -i -F "file=@cat.png" localhost:8080/upload/` gives 201 with a
  `Location`, and DELETE on it gives 204, then 404.
- [ ] **9. CGI output.** The entry point agreed with Ivan. TimedOut: 504;
  Failed or no output: 502. Headers end at `\r\n\r\n` or `\n\n`; at least one
  of Content-Type, Location or Status; `Status` sets the code; an absolute
  `Location` gives 302; other headers pass through; the script's
  `Content-Length` is dropped. Done when
  `curl -i localhost:8080/cgi-bin/hello.py` gives the page, a script that
  never ends gives 504 and one that crashes gives 502.
- [ ] **10. Under load.** Nothing new: the Field Guide's evaluation table, a
  browser pass, `siege -b localhost:8080/` above 99.5% availability with
  flat memory, and a leak check on every error path, with the Field Guide's
  "What can go wrong" as the checklist.

## 7. Step 1: the harness and the mocks

Building without Paola's parser means faking B's inputs: a request filled in
by hand, a config from `parseString`, a route pointing into it, and a `www`
folder on disk. These were Ravi's four questions; on 2026-11-07 Ravi asked
Claude to build the harness (§1), so each one is answered below, dated. The
comments in `tests/ResponseBuilder/harness/Harness.hpp` explain the same
choices next to the code. They are worth reading as answers to check, not
as given: each question still holds a lesson (the pointer one above all).

1. **Make the suite link.** *Copy the framework, or share it? What is the
   least that links? Which first assertion fails against an empty stub?*
   **Decided 2026-11-07.** Shared, without moving: the response suite builds
   the config suite's runner from `tests/ConfigParser/framework/`. The only
   change to it is `Tester::suite(title, examples)`, so each suite names
   itself; the config suite's output is unchanged (469 still pass). Moving
   it into `tests/framework/` can wait until both PRs are in `main`. The
   least that links: definitions of `buildResponse` and
   `buildErrorResponse`, so `ResponseBuilder.cpp` got a stub that returns a
   default `HttpResponse`. That default is a 200 OK, while a default
   `RouteResult` is an Error 500: the stub "succeeds" at everything. So the
   first assertion that fails against it and passes after step 2 is
   "a default `RouteResult` gets 500". Card t23 is a different test (an
   action the switch has no case for, also 500); this file called the
   first one t23 until 2026-11-09.
2. **The mock request.** *Which fields? Parse a target string, or take
   `path` and `query` separately?* **Decided 2026-11-07.**
   `request("GET /files/docs?sort=name")` fills `method`, `target`, `path`,
   `query` (split at the first `?`), `version` (HTTP/1.1 unless the line
   names one) and `Host: localhost`, all from one string, so `target` and
   `path` can't disagree. That split is all of C's parser it takes; nothing
   is percent-decoded. Headers and body are set on the result, or with
   `Scene::header()` / `body()`.
3. **The mock route and its config.** *Who owns the vector? How to pick
   `loc`? `filesystemPath` hardcoded or derived?* **Decided 2026-11-07.** A
   `Site` owns the vector `parseString` returns, as a `const` member, so it
   can never grow and reallocate. A `Site` can be neither copied nor moved,
   and it outlives every route it hands out. A route from a `Site` that is
   gone shows up in ASan as `heap-use-after-free` (or as a SEGV when the
   first bad read is inside `std::string`'s copy). Both were checked with
   deliberately broken tests. `loc` is picked by the block's exact path
   (`site.location("/files/")`), never by matching: the test names the block
   the router picked, as a Triage card does. `filesystemPath` is derived
   with `resolvePath`, the function C calls too, so the tests follow the
   config's roots and the alias rule; step 1's section pins `resolvePath`
   (nothing else tested it). For an Upload it is the folder until its
   meaning is agreed with Paola, and a test can always set it by hand.
4. **The `www` folder.** *Committed or made at test time? Who cleans up?
   Root?* **Decided 2026-11-07.** Nothing is committed. Every test gets a
   fresh folder in `build/sandbox/` and works in it as its working
   directory, so `./www/...` roots work as in the server, and one test's
   DELETE can't reach another's files. Files, folders, FIFOs and modes
   (exact, whatever the umask) are made by the test that needs them. The
   destructor removes the folder, unreadable parts included. A crashed
   test leaves its folder to be looked at, and the next run (or
   `make clean`) removes it. As root, the mode-000 test is marked
   `t.known()` instead of failing for the wrong reason.

**Scenarios:** every Triage card in the Lab is a request, a `RouteResult`,
the facts on disk and the expected status. Turn a card into a test when its
step comes up: t23 for step 2; t1–t4, t18 and t19 for step 3. Each card's
test is ready on the pipeline page (its "As a test" box), as a `responds()`
call for that step's section. All of them compiled and ran against the stub
on 2026-11-07, failing for the right reason. t18, t21 and t22 have none yet:
a read that fails halfway can't be staged in a sandbox, and the CGI cards
wait for the entry point agreed with Ivan.

## 8. Test suite: `tests/ResponseBuilder/`

One test program for the response side, one section per build path step,
on the config suite's runner. Its own Makefile, so the root Makefile and
`tests/contracts.cpp` are untouched.

```sh
make -C tests/ResponseBuilder          # from the repo root: build, run everything
make                                   # same, from inside tests/ResponseBuilder
make harness                           # one section (names come from sections/NN_name.hpp)
make quiet | make verbose | make list | make help
make facts FILE=conf/default.conf      # the location blocks and roots a scene can name
make test ARGS="-f t3 -x"              # ./tester options; ./tester --help lists them
```

- **Sections so far:** 5 `directory` (27 tests, 2026-11-13: cards t5–t7,
  the redirect, the index, the listing, 404 or 403, HEAD, no fd left
  open). 4 `tobytes` (28 tests, 2026-11-13: Date now and in
  GMT on every kind of response, once; Server: webserv, once; the new
  phrases and the 19 that stay; Content-Length, HEAD and 204 with the new
  headers there). 3 `static` (33 tests, 2026-11-09: cards t1–t4 and
  t19, 404 against 403, every byte back in order, the MIME table and its
  default, the size limit, no fd left open). 2 `skeleton` (18 tests, 2026-11-09: Error routes keep
  C's code, every action gets an answer, an unknown action gets 500, a
  throwing handler gets 500 and the injected header never reaches the
  wire; all green since 2026-11-09). 1 `harness` (67 tests: the request, `resolvePath`,
  the site and its routes, the sandbox, the facts, scenes, and the step 1
  check). All green against the stub.
- **A test is a Triage card:**
  `responds(t, name, Scene("GET /logo.png").route(RouteAction::StaticFile, "/"), {"status = 404"})`.
  The facts a test can name are listed above `responds()` in
  `harness/Harness.hpp`: `status`, `reason`, `headers`, `header X`, `body`,
  `body size`, `body same as P`, `wire`, `wire header X`, `wire body size`,
  `disk P`. For anything a scene can't say (two requests in a row, a helper
  on its own), use the four pieces in a `t.test`; `01_harness.hpp` has
  examples.
- **How it reaches the internals:** `main.cpp` includes
  `ResponseBuilder.cpp` itself, through `builder_under_test.hpp`, so a
  section can test a static helper on its own. A new B file goes into
  `REPO_SRCS` in the Makefile, or is included there too.
- **Each test runs in its own forked process**, with a 2 s time limit and
  AddressSanitizer + UBSan (`SAN=0` turns them off), as in the config
  suite. A builder that reads a FIFO times out instead of hanging the run.
- `t.known("why")` works as in the config suite.

## 9. ResponseBuilder Pipeline page: keep it in sync

- Page: https://claude.ai/artifact/8sH3WeiDjF2iYymd7dPmSB (private until
  shared from its Share menu). Pick a scenario, a Triage card or the wire
  bench, and step through it as a mindmap. The Pipeline view has one block
  per function, with the handlers not written yet as dashed blocks that
  carry their contract from §6. The Call tree view has one node per call.
  Each step shows the C++ lines that ran, the variables after it, and the
  bytes `toBytes` has written so far.
- Built 2026-11-07, at step 1. It traces the harness (`request()`,
  `Site::route`/`error`, `errorBeforeRouting`), `resolvePath`, the
  `buildResponse` stub, and `HttpResponse.cpp` (`make`, `errorPage`,
  `makeError`, `setHeader`, `reasonFor`, `toBytes`). Each card shows what it
  expects, what the stub answers today, and its test.
- **Updated 2026-11-09 for step 2** (version 2). The embedded
  `ResponseBuilder.cpp` is Ravi's step 2. The JS copy has `buildResponse`'s
  try, switch, catch and the return after them, the six handler stubs that
  throw, and `buildErrorResponse`'s fallback. In the Pipeline view, `switch`,
  `catch` and "return after the catch" are real blocks; the handlers hang
  under `switch`, still dashed (labelled "a stub that throws"); `makeError`
  called from inside `buildResponse` has its own blocks. A card whose status
  is already right before its step says what that step still adds (t14,
  t17, t18, t20) or that it is right for good (t15, t16). Checked the same
  way as on 2026-11-07: all 30 scenarios gave identical status, bytes and
  route in the C++ and in the JS copy; all 137 anchors and 19 functions were
  found; 60 walks (2,642 steps, every block clicked, both views, at 1400 and
  390 px wide) ran without a JavaScript error. The scripts lived in the
  session's scratchpad: a Node script that runs the engine in `vm` and
  writes a C++ main from the page's own scenario data, and a walker appended
  to a copy of the page, run with `google-chrome --headless=new --dump-dom`.
- **Updated 2026-11-13 for steps 3 and 4** (version 3). Embedded: Ravi's
  `ResponseBuilder.cpp` and `HttpResponse.cpp` at step 4, and the harness
  with `body has` and `openFds`. The JS copy runs `handleStaticFile`,
  `readFile`, `mimeType` (its table read from the source, like
  `reasonFor`'s), `httpDate`, and the Date and Server lines of `toBytes`.
  Each StaticFile card carries an `fs` model of its sandbox, for example
  `fs: { 'www/html/pipe': { kind: 'fifo' } }`, which the copy's `stat`,
  `access` and `read` consult and the check script turns into `Sandbox`
  calls. t18's failing read is staged on the page only (`noScene: true`);
  the check skips it. `handleStaticFile()` is a real block, with
  `readFile()`, `mimeType()`, `make()`, `setHeader()` and a dashed
  `buildErrorResponse()` under it; `httpDate()` sits under `toBytes()`.
  `toBytes` reads the clock (`opts.now` in the check, the page's load time
  otherwise); the check masks the Date line on both sides and verifies the
  C++ one is an IMF-fixdate. Results: 29 of 30 scenarios identical (t18
  page only), 180 anchors and 22 functions found, 24 phrases and 10 MIME
  types read from the source, 60 walks (3,096 steps, 1,110 block clicks,
  both views, 1400 and 390 px) without a JavaScript error. The scratchpad
  is wiped between sessions, so each update re-reads the page from the
  artifact and rewrites the scripts: an update script whose every
  replacement asserts a unique anchor, `check.js` (the JS copy in Node, a
  generated C++ main through the harness, the comparison), `walker.js`
  (appended to a copy of the page, run with
  `google-chrome --headless=new --dump-dom`).
- **Not yet updated for steps 5 and 6** (2026-11-17). The page still shows
  step 4: `handleDirectory` and `handleRedirect` are dashed blocks, and cards
  t5–t8 still answer as the step 4 stubs do.
- How it works: like the ConfigParser Pipeline page. It embeds the C++ files
  and runs a JavaScript copy of them that records every call. Line numbers
  come from snippets (`ANCHORS` in the trace engine), so code that only
  moves keeps working. `reasonFor`'s table is read from the embedded
  `HttpResponse.cpp`, so a phrase added there shows up by itself.
- To update after a step: `Artifact read` the URL (it comes back with the
  publish wrapper, `<!doctype ...><body>`; strip it before publishing
  again). Replace the embedded files that changed, port the new C++ to the
  JS copy (a function per C++ function, `FUNCS` and `ANCHORS` entries, an
  `FN` description), and turn the step's dashed block into a real one.
- Then check the copy against the real code: every scenario through the C++
  (harness, `buildResponse`, `toBytes`) and through the JS copy in Node,
  comparing the route, the status and the bytes; then every step of every
  scenario in both views in headless Chrome. On 2026-11-07 all 30 scenarios
  (23 cards, 7 bench) gave identical bytes, all 115 anchors were found, and
  60 walks (every step, both views) ran without a JavaScript error.

## 10. Next up (2026-11-17)

Done: steps 1–6 pass (§6). Steps 1–5 are committed (`76efd01` is step 5).

1. Commit step 6: `src/http/ResponseBuilder.cpp`,
   `tests/ResponseBuilder/sections/06_redirect.hpp` and `main.cpp`; the
   config side (`src/config/ConfigParser.cpp`,
   `tests/ConfigParser/sections/07_locations.hpp`,
   `conf/invalid/return_code_range.conf`,
   `conf/invalid/return_not_a_redirect.conf`); this file and
   `config-work-context.md`.
2. Update the pipeline page for steps 5 and 6 (§9): Directory's and
   Redirect's blocks, `listDirectory` and its helpers, cards t5–t8 with `fs`
   models of their folders.
3. Step 7, error pages: Claude writes the test section first.
4. Step 0, still open: the "Needs agreement" list for Ivan and Paola. The
   slash redirect (Paola) now touches steps 5 and 6: if the router sends it
   as a Redirect, `handleRedirect` already serves it (a test pins that), and
   `handleDirectory`'s own 301 becomes unreachable.

Later: a pipeline page for the whole server, following one request from
`recv()` through the parser, the router, `buildResponse` and `toBytes` to
`send()`, once A's and C's code exists. The ResponseBuilder Pipeline page
(§9) is the B part of it.

---

## Response side: decisions

Edit this list directly; it is the only copy. (Until 2026-11-17 it was meant
to be pasted from the Lab's Decision log tab, which was never kept up.)

- Redirect codes in return: 301, 302, 303, 307 and 308 only, the rest of 3xx refused at startup; the handler takes the code and the target from the route, and the target goes out as configured (2026-11-17)
- Reason phrases to add: 303 See Other, 304 Not Modified, 307 Temporary Redirect, 308 Permanent Redirect; 413 becomes Content Too Large (2026-11-13)
- Where Date and Server get added: in toBytes, the date formatted by a helper; Server: webserv; both replace one the response already has (2026-11-13)
- Default MIME type: application/octet-stream (2026-11-09)
- Autoindex layout: relative links, folders first then files, each in byte order, uppercase %XX; the markup around them is open (2026-11-13)
- Upload name clash: (open)
- Size limit for files read into memory: over it, 500; the limit itself is set in the code (2026-11-09)
- Local CGI redirect: (open)

## Agreed with the team

- [ ] Who calls toBytes, and who sets Connection: close (Ivan)
- [ ] How CGI output reaches you, for example buildCgiResponse(const CgiProcess &, const RouteResult &) (Ivan)
- [ ] Who passes headRequest (request.method == "HEAD") (Ivan)
- [ ] The port’s default server in the RouteResult for errors before routing, so custom pages work for 400, 413, 414, 431 and 505 (Ivan)
- [ ] Location matching for error pages: a shared matchLocation(server, uri), or a GET through routeRequest (Paola)
- [ ] Uploads and DELETE are yours to carry out, and what filesystemPath means for an Upload: the folder or the final file (Paola)
- [ ] The slash redirect for folders: the router, or you on RouteAction::Directory (Paola)
