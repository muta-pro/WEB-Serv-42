# Config parser: work context (Ravi)

> **For Claude:** this is a handover file from earlier sessions. Read all of it
> before helping. It describes what Ravi is building, what has been decided,
> what is still open, and how Ravi likes to work. The repo is the source of
> truth for code. If anything here disagrees with the code, trust the code and
> say so.

Last updated: 2026-10-28 · Branch: `config`

---

## 1. Who and how

- **Team:** Ravi and Ivan. Liza (spelled "Lisa" in some code comments) left.
  Her work has not been redistributed, so don't assume who owns a module
  because a comment or the architecture doc names someone.
- **Ravi owns the config layer** (`includes/config/`, `src/config/`, `conf/`).
  Ravi wrote the config headers, and the headers name Ravi as the parser.
- **Avoid touching Ivan's code.** The Makefile, `main.cpp`, and the http/,
  network/ and cgi/ modules are shared or Ivan's. Check with Ravi before
  editing them.
- **Ravi is relearning C++** (hadn't written any since June 2026). Explain
  with concrete examples from this repo, and briefly recap anything that may
  have faded. Ravi wants to understand and write the code, so don't implement
  things unless asked. ("Create the files, don't implement them yet" means
  stubs with TODO comments.)
- **How a step goes:** Claude writes the scaffold (signatures, plumbing,
  TODO comments naming the exact error messages) and the step's test
  section first. Ravi writes the code against the failing tests, and Claude
  reviews it by running the suite. Steps 6 and 7 went this way.
- **Tests go in the suite, not in throwaway scripts.** Every step adds its
  section to `tests/ConfigParser/` (see §12). Ravi asked for this so the
  tests build up into one suite for the finished parser.
- **Don't fix contradictions** between `docs/BASE_LAYER_ARCHITECTURE.md` and
  the code without asking. Ravi wants to understand the doc first. Flag them
  instead.
- Study pages from the C++ walkthrough (private claude.ai pages):
  - Webserv Config Layer: https://claude.ai/artifact/E3esNMLboixvvvF8cCR9Ne
  - Mindmap of all parts: https://claude.ai/artifact/CP1HjycyU2bJNwTaJD6YBq
  - Part 4 (HeaderMap/HttpRequest): https://claude.ai/artifact/YMZuPsKgpEBHAEG46p73oW
  - ConfigParser Pipeline (step through a .conf as a mindmap, linked to the
    C++): https://claude.ai/artifact/A7jfoArEgnKW58K7mHokKT. Matches the
    parser as of 2026-10-28 (steps 1-9). Update it when the parser changes,
    see §13.

## 2. Git workflow

Ravi's config work lives on its **own branch, `config`** (tracks
`origin/config`), so Ravi can commit and push freely without Ivan ever
getting a merge conflict. The old `feature/config` and `feature/phase-0`
branches were merged and deleted on origin. `origin/main` holds the merged
phase-0 work (PR #3) and is already merged into `config`. Origin also has a
`network` branch; check with Ivan which branch he works on.

Pushing goes over SSH (origin's push URL is `git@github.com:...`). The machine
you push from needs a GitHub SSH key.

```sh
# at school, first time
git fetch origin
git switch config                  # creates a local branch tracking origin/config

# every session
git pull                           # get what was pushed from home
# ... work ...
git add <files> && git commit -m "config: ..."
git push

# now and then, pick up Ivan's changes so the branches don't drift apart
git fetch origin
git merge origin/main              # conflicts, if any, show up HERE, on Ravi's branch
```

When the parser is done, open a PR from `config` into `main`. Before that,
decide whether this context file should be merged too, or removed first.

## 3. Pipeline position

Config is the **first stage** of the webserv pipeline:

```
read .conf -> tokenize -> parse syntax -> validate -> apply defaults/inheritance
           -> freeze (immutable from here on) -> ServerManager opens listeners
```

Everything downstream holds `const ServerConfig*` / `const LocationConfig*`
pointers, which `RouteResult` borrows, and never mutates them.

## 4. State of the config layer

| File | State |
|---|---|
| `includes/config/LocationConfig.hpp` | Done. Every field is final after parsing; there are no "inherit" sentinels. Alias-style `resolvePath` contract. |
| `includes/config/ServerConfig.hpp` | Done. `Listen` (has `operator<`, so it works as a map key), `errorPageUri`, `matchesHost`. |
| `src/config/ServerConfig.cpp` | Done, compiles clean: Listen operators, the LocationConfig predicates, `grantImplicitHead`, `resolvePath`, `errorPageUri`, `matchesHost` (strips the port, case-insensitive). |
| `includes/config/ConfigParser.hpp` | Done. `ConfigError(line, msg)`, `ServerDefaults` (parser-only scratch), `parseFile` / `parseString`, the syntax rules, and **7 guarantees** (see §6). |
| `src/config/ConfigParser.cpp` | **Empty. This is the main job.** |
| `includes/config/ConfigDump.hpp` + `src/config/ConfigDump.cpp` | Step 0 stubs: `dumpConfig(std::ostream&, const std::vector<ServerConfig>&)`. Not implemented. |
| `src/main.cpp` | Step 0 stub. Currently **all commented out** (Ravi's choice). The TODO says the default path is `"default.conf"`, but the file is now at `conf/default.conf`, so update the path when implementing. |
| `conf/default.conf` | Realistic config. Ravi moved it from `docs/default.conf` (where Ivan had put it) to `conf/`. |
| `conf/edge_cases.conf` | Adversarial but valid. Pins the tokenizer's behaviour (see §7). |
| `conf/multi_server.conf` | Several servers, shared ports, virtual hosts. |
| `conf/invalid_syntax.conf` | Must be rejected. Its header lists 12 more rejection cases, each of which needs its own file. |

Ivan's recent changes (on `feature/phase-0`, 2026-09-22 to 09-28):
- moved `notes.md` and `toDo.md` into `docs/`
- added `includes/http/HttpMethod.hpp`. Since 2026-10-01 it reads
  `enum class HttpMethod { Get, Post, Head, Delete, Unknown };` (Ivan added
  `Head` and `Unknown` then). The config layer uses it: methods are a
  `std::set<HttpMethod>` (open decision 6, settled).
- added an empty `includes/CgiProcess.hpp`

## 5. Problems found in the code review

1. **The Makefile doesn't compile `config/ConfigDump.cpp`.** (It does build
   `ConfigParser.cpp` and `ServerConfig.cpp` since Ivan's 2026-10-01 commit.)
   Add it when `dumpConfig` is implemented; tell Ivan, since the Makefile is
   shared.
2. **Inheritance can't be applied line by line.** `root` may appear *after* a
   `location` block in the same server and must still apply to it. Solution
   in §8.
3. **`ServerConfig::clientMaxBody` has no defined meaning.** Nothing in the
   headers, the doc or the code says who reads it. It overlaps with
   `ServerDefaults::clientMaxBody`. **Decided 2026-10-21: option B** (§9).
4. **Architecture doc vs headers.** Build against the **headers**, which
   record the later numbered decisions ("decision 5", "decision 6").

   | | Doc | Headers |
   |---|---|---|
   | Server-level root | yes | no |
   | Error pages per location | yes | no |
   | `HttpMethod` values | `Get, Post, Delete` | also `Head` and `Unknown` (`HttpMethod.hpp`) |
   | Index files per location | several | one |
   | CGI mappings per location | several | one |

   Don't edit the doc yet.
5. Not config: `includes/Router.hpp:1` has a broken include guard
   (`#ifndef ROUTER_HPPROUTER_HPP` but `#define ROUTER_HPP`). Tell its owner.
6. **`readArgs` reports a missing `;` badly** (known, accepted for now). In
   `invalid_syntax.conf` the `listen` on line 27 has no `;`, so `readArgs`
   swallows `server_name broken.test` up to the next `;` and throws
   `line 27: invalid number of arguments in 'listen'`. The file expects
   `line 28: expected ';' before 'server_name'`. Possible fix: when there are
   too many arguments, throw at `args[max]` (the first extra one) with
   "expected ';' before ...". Not a grading issue, so it's parked.

## 6. The parser's contract (from ConfigParser.hpp)

**Syntax:** nginx-flavoured.
- `#` starts a comment that runs to the end of the line.
- `{`, `}` and `;` are always tokens of their own.
- `;` ends every simple directive.
- `client_max_body_size` accepts the size suffixes `k/K m/M g/G`.

**Directives**
- Server level: `listen`, `server_name`, `client_max_body_size`,
  `error_page`, `root`, `index`, `autoindex`, `methods`, `location`.
- Location level: `root`, `index`, `autoindex`, `methods`,
  `client_max_body_size`, `upload_store`, `cgi`, `return`, `internal`.
- At server level, root/index/autoindex/methods/client_max_body_size are
  **defaults for the locations only**.
- `error_page 500 502 503 504 /50x.html;` binds several codes at once: every
  argument except the last is a code, the last is the URI.

**Guarantees on the returned vector**
1. At least one server, and each server has at least one Listen.
2. Every server has at least one location. If the file declares no `/`
   block, the parser **creates** one from the defaults.
3. Every LocationConfig field holds its final value, with no sentinels.
4. HEAD is present wherever GET is (`grantImplicitHead`).
5. Every error_page value is a URI starting with `/`.
6. No location has an empty root.
7. Locations are kept in declaration order.

**Other settled design decisions** (see the header comments):
- **Alias-style root.** For `location /kapouet { root /tmp/www; }`, the URL
  `/kapouet/pouic` maps to `/tmp/www/pouic`.
- **Virtual hosts.** Servers are grouped by Listen and chosen with
  `matchesHost`. If no name matches, the first server declared for that
  Listen wins.
- **Error pages** are server-level only, and their values are URIs that get
  re-routed like any request. `internal;` locations can only be reached that
  way.
- **HEAD** is granted implicitly with GET. The body is dropped in
  `toBytes()`, but Content-Length is kept as if it were a GET.

## 7. Behaviour the test corpus pins down

- A glued comment ends the token: `listen 0.0.0.0:8080;# comment`.
- `{` glued to a word is still its own token: `location /a/{root ./www/html;methods GET;}`.
- A whole server block may be on one line.
- A duplicated scalar directive: **the last one wins, silently**
  (`client_max_body_size 1M; ... 4M;` resolves to 4M).
- The file may have no newline at the end.
- Expected error format: `config error, line 28: expected ';' before 'server_name'`
  (from invalid_syntax.conf, where the `listen` on line 27 is missing its `;`).
- Rejection cases that each need their own file:
  - unterminated block
  - stray `}`
  - unknown directive
  - directive outside a block
  - bad or out-of-range port
  - error_page value that is a path instead of a URI
  - error_page code outside 300–599
  - empty root
  - two locations with the same path
  - unknown size suffix (`10X`)
  - `return` code outside 3xx
  - no server block at all

## 8. How `std::optional` solves inheritance order

The problem: while parsing a location you don't know the final server
defaults yet. Plain `LocationConfig` defaults can't tell "the file said
`autoindex off`" apart from "autoindex was never mentioned". Only the second
one should inherit.

`std::optional<T>` either holds a `T` or holds nothing, which gives you the
missing "not set" state. Use it in a private struct inside ConfigParser.cpp:

```cpp
struct RawLocation {
    std::optional<std::string>              root;
    std::optional<std::string>              index;
    std::optional<bool>                     autoindex;
    std::optional<size_t>                   clientMaxBody;
    std::optional<std::set<HttpMethod>>     methods;
    LocationConfig                          own;   // path, cgi, upload_store, return, internal
};
```

1. **Parsing a location:** `autoindex on;` sets `raw.autoindex = true;`.
   Anything not mentioned stays empty.
2. **Parsing a server:** collect the RawLocations in a vector and fill in
   `ServerDefaults` as you go. Order doesn't matter, because nothing is
   combined yet.
3. **At the server's `}`**, the defaults are final, so finalize:
   ```cpp
   LocationConfig loc = raw.own;
   loc.root      = raw.root.value_or(defaults.root);
   loc.autoindex = raw.autoindex.value_or(defaults.autoindex);
   ```
   `value_or(x)` means "the value if set, otherwise `x`."

Only the inheritable fields are `optional`. Downstream code never sees an
`optional`, which is how guarantee 3 holds.

## 9. Decided (B): what `ServerConfig::clientMaxBody` means

The timing problem: the location is only known after routing, and routing
runs after the whole request (body included) is parsed. Until then, only a
cap checked **before routing** can stop a huge body. The doc calls it the
"global safety cap" (BASE_LAYER_ARCHITECTURE.md, around line 358).

Example from default.conf: the server sets `1M`, while `/upload/` sets `10M`.

- **A. The value written at server level (1M).** If it's used as the
  pre-routing cap, a 5 MB upload to `/upload/` gets a wrong 413. If nobody
  uses it, it's just a duplicate of ServerDefaults that runtime code can read
  by mistake. **Avoid.**
- **B. The largest limit of any location in the server (10M).** Safe as an
  early cap: it never rejects what some location would accept. The router
  does the exact per-location check. The connection can refuse
  `Content-Length: 5G` immediately. Costs one loop in finalize.
- **C. Delete the field.** The HTTP parser uses a hardcoded ceiling (for
  example 100M) and the router checks per location. Simplest; the downside is
  that over-limit bodies under the ceiling get buffered before rejection.

**Decided 2026-10-21: B.** Step 8 sets it to the largest limit of the
server's locations. Tell Ivan, because his HTTP parser is the reader: it
compares `Content-Length` with `server.clientMaxBody` before reading the
body (413 if over), and the router does the exact per-location check after.
Decision 7 (no `0`) keeps this simple: every location has a real limit.

## 10. Step-by-step plan

Each step ends with something you can run, and adds its section to the
test suite (§12).

- [x] **0. Scaffolding** (stubs only): `main.cpp`, `ConfigDump.hpp/.cpp`.
      Still to do: the Makefile entries and the implementations.
- [x] **1. `ConfigError`.** Constructor builds `config error, line N: <msg>`,
      plus the `line()` getter.
- [x] **2. Tokenizer.** `struct Token { std::string text; size_t line; };`
      and a function from file text to `std::vector<Token>`. The rules:
      whitespace separates tokens; `{ } ;` are always their own token; `#`
      starts a comment to the end of the line, even when glued to a token;
      count lines. Test by printing the tokens of edge_cases.conf.
- [x] **3. Parser cursor.** A struct holding the tokens and a position.
      Helpers:
      - `peek()` / `next()`, which throw "unexpected end of file" at EOF
      - `expect("{")`
      - `readArgs()`: collects tokens until `;`, and throws if it hits `{` or
        `}` first. This is what produces "expected ';' before X".
- [x] **4. Top level and the server skeleton.**
      - The top level only accepts `server {`; anything else is "directive
        outside any block".
      - Inside a server, dispatch directives until `}`. Anything not on the
        list is "unknown directive".
      - Start with only `listen`, `server_name` and `location`, so
        default.conf runs early.
- [x] **5. Value parsers.**
      - `parseListen`: `host:port` or `port`, port 1–65535, digits only
      - `parseSize`: k/m/g suffixes, rejects `10X`, guards against overflow
      - `parseOnOff`
      - `parseStatusCode(lo, hi)`: 300–599 for error_page, 3xx for return
      - `parseMethods`: GET/HEAD/POST/DELETE into a `std::set<HttpMethod>`,
        case-sensitive
- [x] **6. The remaining server-level directives.**
      - `error_page`: last argument is the URI and must start with `/`
      - `client_max_body_size`
      - root/index/autoindex/methods, which go into ServerDefaults
      - Last one wins when a directive is duplicated.
- [x] **7. Location blocks** (into a RawLocation, see §8). Reject:
      - a nested `location`
      - server-only directives inside a location
      - location-only directives at server level
      - duplicate location paths

      `internal;` takes no arguments; `cgi` takes exactly two (extension,
      interpreter). The exact error messages are pinned in
      `tests/ConfigParser/sections/07_locations.hpp`.
- [x] **8. Finalize at the server's `}`.** Done 2026-10-22.
      - `finalizeLocation`: the 5 inheritable fields with `value_or`, then
        `grantImplicitHead()`.
      - `finalizeServer`, in order: reject a missing listen (decision 1,
        "server has no 'listen'" on the `server` keyword's line); finalize
        every RawLocation in file order; add `/` if missing (at the end);
        set `ServerConfig::clientMaxBody` to the largest location limit
        (§9, B).
      - `claimNames` (decision 4): a `std::set<std::pair<Listen,
        std::string>>` of lowercase names already claimed; check this
        server's pairs first, then insert them. A set because a loop over
        all earlier servers is ~200 million comparisons in the 20,000-server
        stress test.
      - `claimDefaults` (decision 8, done 2026-10-28): a
        `std::set<Listen> taken`; a nameless server whose listens are all
        in `taken` is rejected, then every server inserts its listens.
      - `parseString`: reject a file with zero servers ("no server block",
        line 1).
      - Empty root (guarantee 6) needs no check: tokens are never empty and
        `root ;` already fails on the argument count.
      - The meaning of `clientMaxBody` (§9, B) is written next to the field
        in ServerConfig.hpp, for Ivan.
      - Decision 1 meant adding `listen 80;` to 12 older tests that had none.
- [x] **9. `parseFile`.** Done 2026-10-28. Read with `std::ifstream`, line by line with
      `std::getline` (adding back each `'\n'`, or every token lands on line
      1), then call `parseString`. Errors get line 0, since no line was read:
      - "cannot open '<path>'": `!file` after opening (missing, no
        permission, empty path).
      - "cannot read '<path>'": `file.bad()` after the loop. That's a
        directory: on Linux an ifstream opens one fine, and only the first
        read fails. (Reading with `<< file.rdbuf()` can't tell a directory
        from an empty file, which is why the plan uses getline.)
      - Tests: the `file` section (`09_file.hpp`, 15 tests). The suite's
        `parsesFile` / `rejectsFile` now read through `parseFile` too.
- [ ] **10. Test corpus.**
      - Split the rejection cases into `conf/invalid/<case>.conf`.
      - Add a `corpus` section to the suite (§12): `parsesFile` for every
        valid file, `rejectsFile` with the right line for every invalid one.
        That replaces the script this step used to plan.
      - Still to decide: whether to also run `./webserv` on each file once
        `main.cpp` and `dumpConfig` work.
        `invalid_syntax.conf` is a known issue in the suite until §5.6 is
        fixed.
- [x] **When the parser is done:** update the ConfigParser Pipeline page
      to the finished code (§13). Done 2026-10-28, steps 1-9. Update it again
      if the parser still changes (for example when §5.6 is fixed).

**Next up (2026-10-28):** step 10, the test corpus. Steps 1-9 are done
and all 382 tests pass. Claude writes the scaffold first, as before: the
`conf/invalid/<case>.conf` files and a `corpus` section. Ravi asked to go
through the parser at the same pace as the C++ lessons.

The visualizer page "ConfigParser Pipeline"
(https://claude.ai/artifact/A7jfoArEgnKW58K7mHokKT) embeds ConfigParser.cpp
and a JavaScript copy of the parser that steps through it. When the parser
changes, update both, so the page matches the code.

**Tell Ivan** (collected from the sections above):
- §9 / decision 3 (B): his HTTP parser compares `Content-Length` with
  `server.clientMaxBody` before reading the body (413 if over). The router
  does the exact per-location check.
- Decision 2: `listen` hosts aren't validated; his socket code resolves them
  with `getaddrinfo`.
- §5.1: the shared Makefile doesn't compile `ConfigDump.cpp` yet.
- §5.5: the include guard in `includes/Router.hpp:1` is broken.

## 11. Open decisions

1. ~~A server with no `listen`: error, or default to `0.0.0.0:8080`?~~
   **Decided 2026-10-22: error**, "server has no 'listen'" on the line
   of the `server` keyword.
2. ~~`listen localhost:8080`: accept hostnames, or IP addresses only?~~
   **Decided 2026-10-14: any non-empty host string.** The parser doesn't
   validate it; the socket code resolves it with `getaddrinfo`, so
   `localhost` works. A bad host is only caught at bind time, without a line
   number. Tell Ivan.
3. ~~The meaning of `ServerConfig::clientMaxBody`?~~ **Decided 2026-10-21:
   B**, the largest limit of the server's locations (§9). Tell Ivan.
4. ~~The same `server_name` on the same listen in two servers: error, or
   first one wins?~~ **Decided 2026-10-22: error**, "conflicting server_name
   'a.test' on 0.0.0.0:80" on the second server's line. Case-insensitive,
   like `matchesHost`. A server may repeat its own name.
5. ~~Can `HEAD` be written explicitly in `methods`?~~ **Decided
   2026-10-14: yes, accepted.** `methods GET HEAD;` is the same as
   `methods GET;`.
6. ~~Ivan's `HttpMethod` enum vs a string set in config?~~ **Settled
   2026-10-01:** Ivan added `Head` and `Unknown` to the enum, and the config
   switched to `std::set<HttpMethod>` the same day. `parseMethods` never
   produces `Unknown`: an unknown method is a ConfigError.
7. ~~What does `client_max_body_size 0;` mean?~~ **Decided 2026-10-21:
   rejected**, with `size 0 is not allowed in '0'` (also `0k`, `0M`...). The
   check goes in `parseSize`, so the server and location levels both get it.
   nginx reads 0 as "no limit", read literally it refuses every body, and
   the architecture doc says to reject ambiguous values at startup.
8. ~~Two servers **without** a `server_name` on the same listen?~~
   **Decided 2026-10-27: reject every unreachable server.** A server with no
   `server_name` matches no Host, so it can only be reached as the default
   (the first server) on one of its listens. If an earlier server, named or
   not, already has every one of its listens, it is rejected on its `server`
   line: "server without server_name can never be reached: each of its
   listens already has a default server". One free listen is enough.
   Checked by `claimDefaults` with a `std::set<Listen> taken` in
   `parseString`, next to `claimNames`.

## 12. Test suite: `tests/ConfigParser/`

One test program for the whole parser, with one section per step. Its own
Makefile, so the shared root Makefile and Ivan's `tests/contracts.cpp` are
untouched.

```sh
make -C tests/ConfigParser            # from the repo root: build, run everything
make                                  # same, from inside tests/ConfigParser
make values                           # one section (names come from sections/NN_name.hpp)
make quiet | make verbose | make list | make help
make facts FILE=conf/default.conf     # what the parser makes of a file, as facts
make test ARGS="-f listen -x"         # ./tester options; ./tester --help lists them
```

- **Sections so far:** 1 `error`, 2 `tokenizer`, 3 `cursor`, 4 `skeleton`,
  5 `values`, 6 `server`, 7 `locations`, 8 `finalize`, 9 `file` (382
  tests, all passing). Step 10 is commented out in `main.cpp`, ready to
  add.
- **How it reaches the internals:** `main.cpp` includes `ConfigParser.cpp`
  itself, through `parser_under_test.hpp`. That's how the tests see `Token`,
  `Cursor` and the `static` helpers. When a function's signature changes,
  its tests have to change with it.
- **Each test runs in its own forked process**, with a 2 s time limit and
  AddressSanitizer + UBSan (`SAN=0` turns them off). A crash or an endless
  loop fails that one test, with the line it crashed on, and the rest still
  run.
- **Checks:** `t.test` (a value or a ConfigError), `t.parses` (facts like
  `"s0 listen = 0.0.0.0:80"`), `t.rejects` (line + message), and the file
  versions `t.parsesFile` / `t.rejectsFile`. The top of `main.cpp`
  explains how to add a test or a section.
- **Known issues:** `t.known("why")` before a check shows it in yellow and
  in the summary, but doesn't count it as a failure. §5.6 is in there twice
  (cursor + `invalid_syntax.conf`). When it passes, the summary says to
  delete the `known()` line.
- Some tests pin behaviour that later steps will change on purpose, and
  their names say so. For example, an empty file gives 0 servers until
  step 8 rejects it.

## 13. ConfigParser Pipeline page: keep it in sync with the parser

- Page: https://claude.ai/artifact/A7jfoArEgnKW58K7mHokKT (private). Load a
  .conf (a sample from `conf/`, your own file, or typed in) and step through
  the parse as a mindmap. The Pipeline view has one block per function; the
  Call tree view has one node per call. Each block shows the C++ lines that
  ran, the variables after the step, and the tokens the cursor read.
- Built from `ConfigParser.cpp` as of 2026-10-28: steps 1-9, the whole
  parser. It starts at `parseFile` (the browser already has the text, so
  "cannot open" / "cannot read" can't happen there) and includes
  `claimDefaults` (decision 8).
- How it works: the page embeds the C++ files and runs a JavaScript copy of
  the parser that records every call. It finds C++ line numbers by
  searching the embedded source for snippets (`ANCHORS` in the page's trace
  engine), so code that only moves keeps working.
- To update: `Artifact read` the URL (the read comes back with the publish
  wrapper, `<!doctype ...><body>`; strip it before publishing again).
  Replace the embedded `ConfigParser.cpp` (and the headers and confs, if
  they changed). Port the new code to the JS copy, add `FUNCS` / `ANCHORS`
  entries, an `FN` description and a pipeline `BLOCKS` entry for each new
  function, and fix any anchors that no longer match.
- Then check the copy against the real parser: run the copy's
  `traceParse` in Node and compare its facts or error with
  `./tester --facts -` (stdin) on the `conf/` files, the strings in the test
  sections and random mutations of the confs; then open the page in
  headless Chrome and step through every sample in both views. On
  2026-10-28 the two agreed on all 447 inputs (31 of them rejected by
  decision 8), all 271 anchors were found, and the page ran without a
  JavaScript error.
