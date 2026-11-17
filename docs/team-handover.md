# Team handover

The shared notebook for the webserv team. If something here would save a
teammate from asking you, or from a wrong assumption, add it. Keep each
entry short, give it a date, and link to the longer doc rather than copying it.

The code is the source of truth. If this file disagrees with it, fix this file.

Last updated: 2026-11-17

---

## 1. Team and ownership

The split comes from the **three-person architecture blueprint** (a .docx,
not in the repo). Its zones are A, B and C:

| Zone | Who | Owns |
|---|---|---|
| **A · Network & runtime** | Ivan | Sockets and listening ports, client connections and the event loop, read/write traffic, timeouts, CGI process execution, `main.cpp` and integration |
| **B · Config & response** | Ravi | Config parsing and validation, the final server/location models, static files, errors, redirects, autoindex, response building and serialization, interpreting CGI output |
| **C · HTTP & routing** | Paola | Incremental HTTP request parsing, request validation and body framing, virtual-host and location routing. Returns a decision (`RouteResult`), not a response |

**Core rule:** A moves the data, C understands it and chooses, B prepares the
answer. Each module has one primary owner.

- Shared headers and the root `Makefile` change only after the team agrees
  on the interface.
- Paola took over the parser and router from Liza, who left in September
  2026. Comments signed "- Liza" in `includes/http/` are Liza's old notes,
  not ownership.

## 2. How the parts connect

**Startup:** B reads the configuration. A then starts the server, and C
uses the same rules to interpret each request.

**Each request:** A → C → B → A.

| Handoff | What passes | Meaning |
|---|---|---|
| B → A + C | Finalized config (`ServerConfig` / `LocationConfig`) | A learns where to listen; C learns which rules to apply |
| A → C | Incoming request bytes | C reads pieces of a request until it has a valid `HttpRequest` |
| C → B | `HttpRequest` + `RouteResult` | C identifies the action; B builds the matching `HttpResponse` |
| B → A | `HttpResponse` / serialized bytes | A sends it, even if it takes several `send()` calls |
| A + B · CGI | Process output → response | A runs the CGI process and collects its output; B turns that output into a response |

The producer of each object owns it: config and `HttpResponse` are Ravi's,
`HttpRequest` and `RouteResult` are Paola's. Changing one still needs the
team's agreement.

## 3. Build plan: four passes

1. **Agree on the doors.** Freeze the shared objects and interfaces (config,
   `HttpRequest`, `RouteResult`, `HttpResponse`) and who can change each.
   The Phase 0 baseline merged into `main` on 2026-10-01 (PR #3) covers this.
2. **Build in parallel.** A tests connections with fake responses, B tests
   config and responses with sample data, C tests parsing and routing with
   sample requests and config. The config parser is pass 2 work.
3. **Connect one real journey.** A receives `GET /` → C parses and routes →
   B serves a small page → A sends it. Keep this first test simple.
4. **Complete the building.** Add keep-alive, larger bodies, errors,
   uploads/DELETE, CGI, timeouts and integration tests, one at a time.

## 4. Where the docs are

| Doc | What it's for |
|---|---|
| [README.md](../README.md) | Project overview, request lifecycle, build policy, team workflow |
| [BASE_LAYER_ARCHITECTURE.md](BASE_LAYER_ARCHITECTURE.md) | The design contracts between modules. Where it disagrees with a header, the header's numbered decisions are newer |
| [config-work-context.md](config-work-context.md) | Config parser: plan, decisions, open questions, test suite (Ravi) |
| [http-response-context.md](http-response-context.md) | Response builder and serializer: build path, decisions, test suite (Ravi) |
| [notes.md](notes.md) | HEAD / 204 / 304 rules for the response serializer |
| [toDo.md](toDo.md) | Phase 0 checklist and readiness gate (historical) |
| Three-person blueprint (.docx, outside the repo) | Source of §1–§3 |
| this file | Anything the whole team needs to know |

## 5. Branches and workflow

- `main`: merged work only. Don't push to it directly; open a PR.
- `config`: Ravi's config work.
- `network`: on origin. Ivan to confirm whether it's his working branch.
- Paola: add your branch here.
- Merge `origin/main` into your branch now and then, so conflicts show up
  on your branch and not in the PR.
- Public headers are contracts. Agree on a change first, then merge it on
  its own so everyone can pick it up.

## 6. Build, test, editor setup

```sh
make                        # build ./webserv (C++17, -Wall -Wextra -Werror)
make test                   # build + check-headers + tests/contracts.cpp
make check-headers          # every header under includes/ compiles on its own
make -C tests/ConfigParser  # config parser suite (see config-work-context.md §12)
```

**VS Code IntelliSense** (added 2026-10-21): `.vscode/c_cpp_properties.json`
sets C++17 and `includes/` as the include path. Without it, IntelliSense
falls back to clang 12's default standard (C++14). It then doesn't recognise
`std::optional`, shows false errors, and marks `#include <optional>` as
unused. `make` was never affected. If you see that again, check that the
file is still there and reload the window.

## 7. Decisions that cross module boundaries

Decisions one zone made that another zone's code depends on.

**B · Config & response (Ravi)**: details and reasoning in config-work-context.md
and http-response-context.md.
- **Redirects (2026-11-17).** `return` accepts only 301, 302, 303, 307 and
  308; the config refuses the rest of 3xx at startup. For
  `RouteAction::Redirect` the response builder reads only `route.StatusCode`
  and `route.redirectLocation`, never `route.loc`, so the router (C) copies
  the location's `redirectCode` and `redirect` into them. `Location` goes
  out as configured, with nothing of the request added.
- **Body size cap (2026-10-21).** `ServerConfig::clientMaxBody` is the
  largest limit of any location in that server. The HTTP parser (C) compares
  `Content-Length` with it before reading the body (413 if over). The router
  (C) then checks the exact per-location limit.
- **`listen` hosts (2026-10-14).** The config accepts any non-empty host
  string and doesn't validate it. The socket code (A) resolves it with
  `getaddrinfo`, so `localhost` works. A bad host only fails at bind time.
- **HEAD.** Granted wherever GET is allowed. The response is built as for
  GET; `toBytes()` drops the body but keeps the GET `Content-Length`.
  Rules for 204 and 304 are in notes.md.
- **Methods** are `std::set<HttpMethod>`. The config never produces
  `HttpMethod::Unknown`.
- **Root is alias-style.** For `location /kapouet { root /tmp/www; }`,
  `/kapouet/pouic` maps to `/tmp/www/pouic`. See `LocationConfig::resolvePath`.
- **Virtual hosts.** Servers are grouped by `Listen` and chosen with
  `ServerConfig::matchesHost`. If no name matches, the first server declared
  for that listen wins.
- **Error pages** are server-level URIs, re-routed like a normal request.
  `internal;` locations can only be reached that way.
- **Config is immutable after startup.** Downstream code holds
  `const ServerConfig*` / `const LocationConfig*` and never changes them.

**A · Network & runtime (Ivan)**: add yours here.

**C · HTTP & routing (Paola)**: add yours here.

## 8. Open questions for the whole team

- Which readiness API (`poll`, `select`, `epoll`, `kqueue`)? It has to match
  the subject and the evaluation machines.
- **`main.cpp`.** The blueprint gives it to Ivan (A), but it holds Ravi's
  commented-out config-loading TODO. Agree on how config loading plugs into
  Ivan's startup.
- The root `Makefile` builds `config/ServerConfig.cpp` but not
  `config/ConfigDump.cpp`. Add it when `dumpConfig` is implemented.

## 9. Log

Newest first. One line each: date, who, what changed or was decided.

- 2026-11-17 · Ravi · `return` takes only the five redirect codes; a Redirect reads its code and target from `RouteResult` (§7).
- 2026-10-21 · Ravi · Adopted the three-person blueprint: A Ivan, B Ravi, C Paola.
- 2026-10-21 · Ravi · Paola joined; HTTP parser and router assigned to Paola.
- 2026-10-21 · Ravi · Added `.vscode/c_cpp_properties.json` (C++17 for IntelliSense) and this file.
