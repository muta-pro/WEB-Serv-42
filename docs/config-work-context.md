# Config parser: work context (Ravi)

> **For Claude:** this is a handover file from earlier sessions. Read all of it
> before helping. It describes what Ravi is building, what has been decided,
> what is still open, and how Ravi likes to work. The repo is the source of
> truth for code. If anything here disagrees with the code, trust the code and
> say so.

Last updated: 2026-10-01 · Branch: `feature/config`

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
- **Don't fix contradictions** between `docs/BASE_LAYER_ARCHITECTURE.md` and
  the code without asking. Ravi wants to understand the doc first. Flag them
  instead.
- Study pages from the C++ walkthrough (private claude.ai pages):
  - Webserv Config Layer: https://claude.ai/artifact/E3esNMLboixvvvF8cCR9Ne
  - Mindmap of all parts: https://claude.ai/artifact/CP1HjycyU2bJNwTaJD6YBq
  - Part 4 (HeaderMap/HttpRequest): https://claude.ai/artifact/YMZuPsKgpEBHAEG46p73oW

## 2. Git workflow

Ravi's config work lives on its **own branch, `feature/config`**, created from
`feature/phase-0` (Ivan's working branch). That way Ravi can commit and push
freely without Ivan ever getting a merge conflict.

```sh
# at school, first time
git fetch origin
git switch feature/config          # creates a local branch tracking origin/feature/config

# every session
git pull                           # get what was pushed from home
# ... work ...
git add <files> && git commit -m "config: ..."
git push

# now and then, pick up Ivan's changes so the branches don't drift apart
git fetch origin
git merge origin/feature/phase-0   # conflicts, if any, show up HERE, on Ravi's branch
```

When the parser is done, open a PR from `feature/config` into
`feature/phase-0`. Before that, decide whether this context file should be
merged too, or removed first.

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
- added `includes/http/HttpMethod.hpp`: `enum class HttpMethod { Get, Post, Delete };`
  (no Head). The config layer stores methods as `std::set<std::string>`,
  which includes "HEAD". Settle this with Ivan; see open decision 6.
- added an empty `includes/CgiProcess.hpp`

## 5. Problems found in the code review

1. **The Makefile doesn't compile `config/ServerConfig.cpp`** (or the new
   `config/ConfigDump.cpp`). Ravi will fix this when it's time to test; tell
   Ivan, since the Makefile is shared.
2. **Inheritance can't be applied line by line.** `root` may appear *after* a
   `location` block in the same server and must still apply to it. Solution
   in §8.
3. **`ServerConfig::clientMaxBody` has no defined meaning.** Nothing in the
   headers, the doc or the code says who reads it. It overlaps with
   `ServerDefaults::clientMaxBody`. Options in §9; **decision pending.**
4. **Architecture doc vs headers.** Build against the **headers**, which
   record the later numbered decisions ("decision 5", "decision 6").

   | | Doc | Headers |
   |---|---|---|
   | Server-level root | yes | no |
   | Error pages per location | yes | no |
   | Methods stored as | `HttpMethod` enum | string set |
   | Index files per location | several | one |
   | CGI mappings per location | several | one |

   Don't edit the doc yet.
5. Not config: `includes/Router.hpp:1` has a broken include guard
   (`#ifndef ROUTER_HPPROUTER_HPP` but `#define ROUTER_HPP`). Tell its owner.

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
    std::string                             path;
    std::optional<std::string>              root;
    std::optional<std::string>              index;
    std::optional<bool>                     autoindex;
    std::optional<size_t>                   clientMaxBody;
    std::optional<std::set<std::string>>    methods;
    LocationConfig                          own;   // location-only fields: cgi, upload_store, return, internal
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

## 9. Pending decision: what `ServerConfig::clientMaxBody` means

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

Recommendation: B, or C to keep the config smaller. **Agree it with Ivan**,
because the HTTP parser and connection code would be the readers.

## 10. Step-by-step plan

Each step ends with something you can run.

- [x] **0. Scaffolding** (stubs only): `main.cpp`, `ConfigDump.hpp/.cpp`.
      Still to do: the Makefile entries and the implementations.
- [ ] **1. `ConfigError`.** Constructor builds `config error, line N: <msg>`,
      plus the `line()` getter.
- [ ] **2. Tokenizer.** `struct Token { std::string text; size_t line; };`
      and a function from file text to `std::vector<Token>`. The rules:
      whitespace separates tokens; `{ } ;` are always their own token; `#`
      starts a comment to the end of the line, even when glued to a token;
      count lines. Test by printing the tokens of edge_cases.conf.
- [ ] **3. Parser cursor.** A struct holding the tokens and a position.
      Helpers:
      - `peek()` / `next()`, which throw "unexpected end of file" at EOF
      - `expect("{")`
      - `readArgs()`: collects tokens until `;`, and throws if it hits `{` or
        `}` first. This is what produces "expected ';' before X".
- [ ] **4. Top level and the server skeleton.**
      - The top level only accepts `server {`; anything else is "directive
        outside any block".
      - Inside a server, dispatch directives until `}`. Anything not on the
        list is "unknown directive".
      - Start with only `listen`, `server_name` and `location`, so
        default.conf runs early.
- [ ] **5. Value parsers.**
      - `parseListen`: `host:port` or `port`, port 1–65535, digits only
      - `parseSize`: k/m/g suffixes, rejects `10X`, guards against overflow
      - `parseOnOff`
      - `parseStatusCode(lo, hi)`: 300–599 for error_page, 3xx for return
      - `parseMethods`: GET/POST/DELETE (HEAD? see open decisions)
- [ ] **6. The remaining server-level directives.**
      - `error_page`: last argument is the URI and must start with `/`
      - `client_max_body_size`
      - root/index/autoindex/methods, which go into ServerDefaults
      - Last one wins when a directive is duplicated.
- [ ] **7. Location blocks** (into a RawLocation, see §8). Reject:
      - a nested `location`
      - server-only directives inside a location
      - location-only directives at server level
      - duplicate location paths

      `internal;` takes no arguments; `cgi` takes exactly two (extension,
      interpreter).
- [ ] **8. Finalize at the server's `}`.**
      1. Fill in the defaults with `value_or`.
      2. Create `/` if it's missing.
      3. Call `grantImplicitHead()` on every location.
      4. Reject an empty root.
      5. Handle a missing listen (open decision 1).
      6. Set `ServerConfig::clientMaxBody` (§9).
      7. After the end of the file: reject a file with zero servers.
- [ ] **9. `parseFile`.** Read with `std::ifstream` (throw ConfigError if it
      can't be opened), then call `parseString`.
- [ ] **10. Test corpus and script.**
      - Split the rejection cases into `conf/invalid/<case>.conf`.
      - Write a script that runs `./webserv` on each file. Valid files must
        parse and dump; invalid files must exit non-zero with the right line
        number.

**Next up:** Step 1, then Step 2 (tokenizer). Ravi asked to go through the
tokenizer at the same pace as the C++ lessons.

## 11. Open decisions

1. A server with no `listen`: error, or default to `0.0.0.0:8080`?
2. `listen localhost:8080`: accept hostnames, or IP addresses only?
   (`bind()` needs an IP address.)
3. The meaning of `ServerConfig::clientMaxBody` (§9). Agree with Ivan.
4. The same `server_name` on the same listen in two servers: error, or first
   one wins?
5. Can `HEAD` be written explicitly in `methods`, or is it only granted
   implicitly with GET?
6. Ivan's `HttpMethod` enum (`Get, Post, Delete`, no Head) vs the config's
   `std::set<std::string>` with "HEAD". Keep the strings in config, or switch
   to the enum? Decide with Ivan.
