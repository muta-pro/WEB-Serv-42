# WEB-Serv-42

An educational HTTP server built in C++. The project focuses on non-blocking network I/O, incremental HTTP parsing, configuration-driven routing, static responses, and CGI.

Project page: <https://muta-pro.github.io/WEB-Serv-42/>

## Current status

The project is in **Phase 0: shared contracts and architecture** on `feature/phase-0`.

The shared types are being stabilized before the team creates implementation branches. The current branch is not ready to merge into `main` until all shared headers and committed source files compile and the contract smoke test passes.

The canonical design reference is [Base-Layer Architecture and Design Contracts](docs/BASE_LAYER_ARCHITECTURE.md).

## Request lifecycle

```text
configuration startup
        |
        v
accept client -> receive bytes -> parse HttpRequest
                                  |
                                  v
                         produce RouteResult
                                  |
                                  v
                         build HttpResponse
                                  |
                                  v
                         serialize and send
                                  |
                                  v
                          keep alive or close
```

TCP provides a stream of bytes, not complete HTTP messages. A request may arrive across several `recv()` calls, and one `recv()` may contain bytes from more than one request. Likewise, one `send()` may transmit only part of a response.

## Responsibility areas

| Area | Responsibility |
|---|---|
| Configuration | Parse, validate, apply defaults and inheritance, then freeze server and location settings before accepting clients. |
| Network/runtime | Own listeners and client sessions, run the readiness loop, perform non-blocking reads and writes, track timeouts, and remove closed clients. |
| HTTP parser | Incrementally turn client bytes into one owning `HttpRequest`, without touching sockets or choosing filesystem paths. |
| Router | Select the virtual server and location, validate the method and path, and return exactly one `RouteAction`. |
| Response builder | Execute the route decision: load files, create redirects and errors, generate autoindex output, or use CGI output. |
| Serializer | Convert `HttpResponse` into one correctly framed HTTP byte string. |
| CGI runtime | Own the child process and pipe descriptors, enforce timeouts, and return buffered CGI output without blocking the event loop. |

Current team areas, to be confirmed at the branch kickoff:

- Liza: request parsing and routing;
- Ravi: configuration, response building, and CGI-output parsing;
- Ivan: network/runtime and CGI process execution.

## Core baseline decisions

- `Connection` owns exactly one accepted client FD. Destroying the connection closes it.
- The event loop may call `recv()`, `send()`, and the selected readiness API, but it does not separately close a `Connection`-owned FD.
- Each client session has its own `HttpRequestParser` because different clients can hold different partial requests.
- Completed `HttpRequest` and `HttpResponse` objects own their strings.
- `std::string_view` is used only for temporary non-owning inputs where the backing data remains valid for the call.
- `HttpRequest` contains protocol information only; routing and filesystem information belongs in `RouteResult`.
- Configuration is immutable after startup so route results may safely borrow `const` pointers to it.
- `RouteResult` contains one scoped `RouteAction`, not several potentially contradictory booleans.
- The response serializer derives the reason phrase, emits one authoritative `Content-Length` when allowed, and handles bodyless responses and `HEAD` correctly.
- The first implementation is single-threaded. One event-loop owner means no locking is required for client-session state.

## Shared contracts

The common baseline includes:

```text
ConnectionState
Connection
ClientSession
CaseInsensitiveLess
HeaderMap
HttpRequest
HttpRequestParser
ParseResult
ServerConfig / LocationConfig
RouteResult / RouteAction
HttpResponse
Router interface
ResponseBuilder interface
CGI runtime contract
```

Shared headers are contracts between branches. A branch may change private implementation details, but public contract changes require team agreement.

## Event-readiness API

The exact readiness API must match the assigned subject and evaluation platform. `select()`, `poll()`, `epoll`, and `kqueue` have different portability and scaling properties; none should be selected solely from a simplified Big-O claim.

The first server should implement one approved API directly. A template abstraction for several backends is intentionally deferred until the project genuinely needs a second backend.

## Build policy

Before the common baseline is merged:

- confirm the C++ standard required by the assigned subject;
- use the same standard and warning flags on every branch;
- include each shared header from a clean translation unit;
- compile every committed `.cpp` file;
- link a minimal executable;
- run the contract smoke tests listed in the architecture document.

The proposed contracts currently use C++17 features. If the assigned subject requires another standard, adapt the contracts before merging them.

## Team workflow

1. Complete and review the Phase 0 contracts on the shared feature branch.
2. Open a pull request; do not bypass review by pushing directly to protected `main`.
3. Merge only after the base-layer definition of done passes.
4. Every teammate creates their concern branch from the same merged commit.
5. Branches use shared types and interfaces rather than creating private alternatives.
6. Any required contract change is discussed and merged separately so all branches can rebase onto it.

## Next milestone

The first integration milestone is deliberately small:

```text
accept -> receive -> fake/incremental parse -> fake route
       -> build "hello" response -> partial send -> close
```

Correctness features such as full routing, keep-alive, configured error pages, CGI, autoindex, and edge cases are added after this end-to-end path works.
