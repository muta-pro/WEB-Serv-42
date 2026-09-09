# Webserv Base-Layer Architecture and Design Contracts

## Status and purpose

This document defines the common base layer from which the team creates feature branches. It describes responsibilities, ownership, lifetimes, shared data types, and the public interfaces between modules.

Once this document and the matching headers are merged into `main`, changing a public contract requires team agreement. Private implementation details may change inside the branch that owns them.

The words **must**, **should**, and **may** are used deliberately:

- **must**: required for correctness or safe integration;
- **should**: the default design unless there is a documented reason to differ;
- **may**: optional behavior that does not change another module's assumptions.

## Language-standard gate

The proposed contracts use C++17 features:

- `std::string_view` for temporary, non-owning function input;
- `std::optional` to represent a value that may be absent;
- `enum class` for scoped states and actions;
- deleted copy operations and explicit move operations;
- range-based loops and `std::move`.

Before adopting the contracts, the team must verify the C++ standard required by its assigned subject. If the subject does not permit C++17, do not merge C++17-only contracts. The design principles still apply, but the types must be translated to the required standard.

All team branches must use the same compiler settings. For C++17, the minimum development flags are:

```text
-std=c++17 -Wall -Wextra -Werror
```

## Architectural goal

The server is divided by responsibility:

1. **Configuration** reads and validates the server rulebook before networking starts.
2. **Network/runtime** owns sockets, waits for readiness, receives bytes, and sends bytes.
3. **HTTP parser** turns request bytes into an `HttpRequest`.
4. **Router** combines the request with immutable configuration and produces a `RouteResult`.
5. **Response builder** executes the route decision and produces an `HttpResponse`.
6. **Serializer** converts the response into bytes for the connection's write buffer.
7. **CGI runtime**, when needed, runs asynchronously and eventually supplies data to the response builder.

The normal data flow is:

```text
client socket
    |
    | recv()
    v
Connection read buffer
    |
    | HttpRequestParser::feed()
    v
HttpRequest
    |
    | routeRequest(request, candidate servers)
    v
RouteResult
    |
    | buildResponse(request, route)
    v
HttpResponse
    |
    | toBytes(response, headRequest)
    v
Connection write buffer
    |
    | one or more send() calls
    v
client socket
```

No stage should secretly perform the next stage's responsibility. For example, the parser does not select filesystem paths, and `HttpResponse` does not read configured error-page files.

## Dependency direction

Dependencies should point in one direction:

```text
common types
    ^
    |
HTTP models       configuration models
    ^                    ^
    |                    |
parser              router
       \              /
        \            /
         response builder
                ^
                |
        network integration
```

Rules:

- `HttpRequest` must remain an HTTP value object. It must not contain route or configuration pointers.
- `HttpResponse` must remain an HTTP value object. It must not perform filesystem, configuration, socket, or CGI operations.
- `RouteResult` may borrow pointers to immutable configuration objects.
- `Connection` must not parse HTTP or build responses. It stores runtime state and byte buffers.
- The event loop may call the public interfaces of the parser, router, builder, serializer, and CGI runtime, but it must not duplicate their logic.
- Shared headers must not depend on accidental include order. Every header includes the standard and project headers required by its own declarations.

## Ownership vocabulary

An **owner** is responsible for releasing a resource. A **borrower** may use it but must not release it.

Copying an integer file descriptor does not copy the underlying socket. Therefore ownership cannot be inferred from possession of the integer.

### File-descriptor ownership table

| Resource | Owner | Borrowers | How it is released |
|---|---|---|---|
| Accepted client socket | `Connection` | event loop and socket helpers | `Connection` destructor |
| Listening socket | listener/socket object | event loop | listener/socket destructor |
| CGI stdin/stdout pipe | `CgiProcess` or an equivalent RAII object | event loop | CGI cleanup/destructor |
| Temporarily opened static file | local RAII file object | response builder | local destructor |
| Integer inside `pollfd` | nobody; it is an observation | event loop | never closed through `pollfd` |

There must be exactly one owner for every open descriptor.

## C++ design choices

### RAII for owned operating-system resources

Owned file descriptors must be released by destructors. This guarantees cleanup on normal returns, error returns, and exceptions.

An owning type must either:

- be non-copyable and safely movable; or
- be non-copyable and non-movable, then stored through `std::unique_ptr`.

The chosen baseline is a non-copyable, movable `Connection`. Its move operation transfers the FD and assigns `-1` to the moved-from object.

### Value types for HTTP data

`HttpRequest`, `HttpResponse`, `ParseResult`, and `RouteResult` should be ordinary value types. They own their strings and may be returned by value. C++ move semantics make this practical without introducing shared ownership.

No inheritance or virtual functions are needed for these data objects.

### Owning strings versus `std::string_view`

Completed requests and responses use owning `std::string` values. Network buffers are mutable: appending, erasing, clearing, or moving a buffer may invalidate views into it.

`std::string_view` is appropriate for temporary parameters such as:

```cpp
ParseResult feed(std::string_view bytes);
```

The parser may inspect those bytes during the call, but it must not store a view whose backing storage can change. Parsed fields copied into `HttpRequest` use `std::string`.

`CaseInsensitiveLess` may accept `std::string_view` parameters because those views exist only during a comparison call.

### `std::optional` for absence

Use `std::optional<T>` when absence is different from a valid zero or empty value. For example:

```cpp
std::optional<std::size_t> contentLength() const;
```

- empty optional: there was no `Content-Length` header;
- value `0`: the client explicitly sent `Content-Length: 0`.

Invalid or overflowing values are parser errors. They are not represented as another optional state in a completed request.

### Scoped enums instead of flags and booleans

Use `enum class` when exactly one state or action is valid. This prevents contradictory combinations such as "redirect and CGI and delete".

Use booleans only for independent yes/no properties such as `keepAlive`.

### Borrowed configuration pointers

`RouteResult` may contain `const ServerConfig*` and `const LocationConfig*`. These are non-owning pointers.

They are safe only under this invariant:

> Configuration is completely parsed, validated, finalized, and placed in its permanent storage before the server accepts clients. It is not moved or mutated while requests are running.

`std::shared_ptr` should not be used merely to avoid documenting ownership. The server manager owns configuration; route results borrow it.

### Exceptions and runtime errors

Use exceptions for programmer errors and unrecoverable startup errors, such as invalid configuration or constructing a connection with an invalid FD.

Do not use exceptions as the normal representation of malformed client input. Parser and router failures become explicit HTTP status results, such as `400`, `404`, `405`, or `413`.

System-call failures are handled by the network layer using return values and `errno`.

Destructors and move operations that own descriptors must be `noexcept`.

## Shared HTTP header representation

The initial shared type is:

```cpp
using HeaderMap =
    std::map<std::string, std::string, CaseInsensitiveLess>;
```

HTTP header names are case-insensitive, so these names identify the same field:

```text
Content-Length
content-length
CONTENT-LENGTH
```

Header values are not automatically lowercased. Their original content is preserved.

Because a map stores one value per name, the parser must process duplicate request headers before insertion. It must never silently overwrite an existing value.

Minimum duplicate policy:

- more than one `Host` field is a `400 Bad Request`;
- conflicting `Content-Length` fields are a `400 Bad Request`;
- unsupported `Transfer-Encoding` is rejected explicitly;
- `Content-Length` together with `Transfer-Encoding` is rejected;
- any intentionally combined field follows a documented rule.

The response layer initially supports one value per header name. If repeated response fields such as multiple `Set-Cookie` lines become a requirement, introduce a separate response header-list type rather than weakening request validation.

## `HttpRequest` contract

`HttpRequest` owns the completed, parsed HTTP request:

```cpp
struct HttpRequest {
    std::string method;
    std::string target;
    std::string path;
    std::string query;
    std::string version;
    HeaderMap headers;
    std::string body;

    bool hasHeader(std::string_view name) const;
    std::string_view header(std::string_view name) const;
    std::optional<std::size_t> contentLength() const;
    bool isKeepAlive() const;
};
```

For this request line:

```http
GET /products?category=books&page=2 HTTP/1.1
```

the fields are:

```text
method  = "GET"
target  = "/products?category=books&page=2"
path    = "/products"
query   = "category=books&page=2"
version = "HTTP/1.1"
```

The parser preserves URL information. It does not create a filesystem path. The router performs percent-decoding and path normalization and verifies that the resulting filesystem path stays inside the configured root.

The method remains a string in `HttpRequest` so an unknown but syntactically valid method is not lost. The router maps supported methods to its internal method representation and decides between `405 Method Not Allowed` and `501 Not Implemented` according to the agreed project behavior.

### Keep-alive rule

`isKeepAlive()` applies these rules:

- HTTP/1.1 persists unless the `Connection` token list contains `close`;
- HTTP/1.0 closes unless the token list contains `keep-alive`;
- connection tokens are comma-separated and compared case-insensitively.

The result is copied to the current client's runtime state after parsing. It is recalculated for every request.

## Parser contract

### Public interface

```cpp
class HttpRequestParser {
public:
    ParseResult feed(std::string_view bytes);

    // Valid only after COMPLETE. Transfers the request and prepares
    // the parser to receive the next request.
    HttpRequest takeRequest();

    // Discards an incomplete or failed request and returns to the
    // initial parsing state.
    void reset();

    ParsePhase phase() const noexcept;
};
```

`ParsePhase` exists because the connection state distinguishes headers from body input:

```cpp
enum class ParsePhase {
    RequestLineAndHeaders,
    Body
};
```

Private parser state may change without affecting consumers. It will normally include the current phase, partial-token or partial-line storage, the expected body length, and the partially built request.

### Parse result

```cpp
enum class ParseStatus {
    Incomplete,
    Complete,
    Error
};

struct ParseResult {
    ParseStatus status = ParseStatus::Incomplete;
    std::size_t bytesConsumed = 0;
    int httpStatus = 0;
    std::string diagnostic;
};
```

Meanings:

- `Incomplete`: valid so far; more bytes are required.
- `Complete`: exactly one complete request, including its framed body, is available.
- `Error`: parsing cannot continue; `httpStatus` is the response status to generate.
- `bytesConsumed`: bytes consumed from this particular `feed()` argument.
- `diagnostic`: internal log information; it is not automatically returned to the client.

The caller must feed each byte exactly once. The parser must not consume bytes belonging to a second pipelined request.

Example input from one `recv()`:

```text
[80-byte request 1][35 bytes belonging to request 2]
```

The parser completes request 1 with `bytesConsumed == 80`. The connection retains the remaining 35 bytes. After `takeRequest()`, those remaining bytes may be fed as request 2.

Finding `\r\n\r\n` means the header section is complete, not necessarily that the request is complete. If a valid `Content-Length: 1000` is present, the result stays `Incomplete` until all 1000 body bytes are available.

### Parser safety limits

The parser must enforce hard limits for at least:

- request-line length;
- total header bytes;
- number of headers;
- framed body size or a global body safety ceiling.

The router additionally applies the effective configured body-size limit for the selected location. If a later implementation needs to reject a location-specific oversized body before buffering it, the parser contract may be extended with an explicit headers-complete pause. That optimization is not required for the first integrated lifecycle as long as a safe global cap exists.

## Connection and client-session contract

### Connection responsibility

`Connection` owns one accepted client socket and its network/runtime state:

```cpp
class Connection {
public:
    explicit Connection(int clientFd);
    ~Connection() noexcept;

    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;

    Connection(Connection&& other) noexcept;
    Connection& operator=(Connection&& other) noexcept;

    // Accessors and focused state-changing methods are omitted here.

private:
    int _fd;
    std::string _readBuffer;
    std::string _writeBuffer;
    ConnectionState _state;
    std::chrono::steady_clock::time_point _lastActivity;
    std::size_t _bytesSent;
    std::size_t _parseOffset;
    bool _keepAlive;
};
```

`std::chrono::steady_clock` is chosen for timeouts because it cannot jump when the wall clock is corrected. Calendar time may still be used separately for HTTP `Date` headers and logs.

`Connection` does not contain `HttpRequest` or `HttpResponse`. A response is serialized into `_writeBuffer`; the temporary `HttpResponse` can then be destroyed.

### FD move invariant

Moving a connection transfers its FD. A moved-from connection has `_fd == -1` and owns nothing.

```cpp
Connection::Connection(Connection&& other) noexcept
    : _fd(other._fd),
      _readBuffer(std::move(other._readBuffer)),
      _writeBuffer(std::move(other._writeBuffer)),
      _state(other._state),
      _lastActivity(other._lastActivity),
      _bytesSent(other._bytesSent),
      _parseOffset(other._parseOffset),
      _keepAlive(other._keepAlive)
{
    other._fd = -1;
    other._state = ConnectionState::Closed;
}
```

Move assignment first releases the destination's current FD, then transfers the source FD and assigns `-1` to the source.

The event loop never performs both `close(fd)` and container erasure. Removing the owning `Connection` closes the client socket through its destructor.

### One parser per client

Parsing state belongs to a particular TCP connection. Two clients may both have incomplete requests at the same time, so they cannot share one mutable parser.

The recommended aggregation is:

```cpp
struct ClientSession {
    Connection connection;
    HttpRequestParser parser;
};
```

The event loop stores sessions by client FD. The map key identifies the session but does not own or close the descriptor.

### Partial writes

`send()` may write fewer bytes than requested. `_bytesSent` is the offset of the next byte to send:

```text
remaining = writeBuffer.size() - bytesSent
```

After a successful send of `n` bytes, add `n` to `_bytesSent`. The write buffer must not be changed while a partial response is being sent.

`POLLOUT` is enabled only while unsent bytes exist. Leaving it enabled for an empty buffer causes a busy loop because sockets are usually writable.

### Keep-alive reset

Only one request is processed at a time on a connection. Pipelined bytes may already be present, but the server processes their requests sequentially.

After a response has been fully sent:

- if the current request is not persistent, transition to `Closed` and schedule removal;
- if it is persistent, remove only the bytes consumed by the completed request, preserve unread bytes, clear the write buffer, reset send/parse offsets, reset the parser, and return to the reading state.

Do not blindly clear the entire read buffer. It may already contain the next request.

## Connection state machine

```cpp
enum class ConnectionState {
    ReadingHeaders,
    ReadingBody,
    Processing,
    CgiRunning,
    WritingResponse,
    Closed
};
```

Allowed transitions:

```text
accept
  -> ReadingHeaders

ReadingHeaders
  -> ReadingBody       headers complete and a body is expected
  -> Processing        complete request without a body
  -> WritingResponse   parser error response prepared
  -> Closed            peer closed or unrecoverable I/O error

ReadingBody
  -> Processing        complete framed body received
  -> WritingResponse   parser/body-limit error response prepared
  -> Closed            peer closed or unrecoverable I/O error

Processing
  -> CgiRunning        asynchronous CGI started
  -> WritingResponse   response serialized
  -> Closed            unrecoverable internal failure

CgiRunning
  -> WritingResponse   CGI completed or timed out and response serialized
  -> Closed            client disconnected or unrecoverable CGI failure

WritingResponse
  -> ReadingHeaders    response complete and keep-alive selected
  -> Closed            response complete and connection should close

Closed
  -> container removal and destructor-driven FD close
```

Every state must have one event-loop handler. Build with switch-enum warnings and log transitions during development.

## Configuration contract

Configuration is processed before listening sockets enter the event loop:

```text
read text -> parse syntax -> validate values -> apply defaults/inheritance
          -> freeze effective configuration -> open listeners
```

The router consumes effective configuration. It should not repeatedly calculate whether a location inherits a value from its server.

Minimum concepts:

```text
ServerConfig
|- listen address and port
|- server names
|- default root
|- default maximum request-body size
|- error-page mappings
`- locations

LocationConfig
|- URL prefix
|- effective filesystem root
|- allowed methods
|- index filenames
|- autoindex setting
|- redirect, if configured
|- upload directory, if configured
|- CGI extension/interpreter mappings
|- effective maximum request-body size
`- effective error-page mappings
```

Use a scoped `HttpMethod` enum for configured supported methods, while retaining the raw method string in `HttpRequest`:

```cpp
enum class HttpMethod {
    Get,
    Post,
    Delete
};
```

Configuration parsing must reject impossible or ambiguous values at startup rather than allowing request-time surprises.

## Router and route-result contract

The router answers: "Given this HTTP request and these candidate virtual servers, what action should the server perform?"

It does not read file contents, run CGI, send bytes, or mutate configuration.

```cpp
enum class RouteAction {
    StaticFile,
    Directory,
    Redirect,
    Upload,
    DeleteResource,
    Cgi,
    Error
};

struct RouteResult {
    const ServerConfig* server = nullptr;
    const LocationConfig* location = nullptr;
    RouteAction action = RouteAction::Error;
    int statusCode = 500;
    std::string filesystemPath;
    std::string redirectLocation;
};
```

Field rules:

- `action` is the one selected operation;
- `statusCode` is the intended success, redirect, or error status;
- `filesystemPath` is meaningful for file, directory, upload, delete, or CGI-script actions;
- `redirectLocation` is meaningful only for `Redirect`;
- configuration pointers borrow immutable configuration;
- a default-constructed result is a safe internal-error result, not an accidental success.

Initial interface:

```cpp
RouteResult routeRequest(
    const HttpRequest& request,
    const std::vector<ServerConfig>& candidateServers);
```

The network/listener layer supplies only servers that are candidates for the local listening address and port. The router uses the request's `Host` field to select the virtual server, then performs location matching and method checks.

## Response and serialization contract

`HttpResponse` is the complete logical response:

```cpp
struct HttpResponse {
    int statusCode = 200;
    HeaderMap headers;
    std::string body;

    static HttpResponse make(int code, std::string body = "");
    static HttpResponse makeError(int code);
    static std::string reasonFor(int code);

    void setHeader(const std::string& name, const std::string& value);
    std::string getHeader(const std::string& name) const;
    bool hasHeader(const std::string& name) const;
};
```

The reason phrase is derived from `statusCode` during serialization. It is not stored as independently mutable state.

`setHeader()` rejects header names containing whitespace, colon, carriage return, or newline, and values containing carriage return or newline. This prevents malformed responses and response-header injection.

### Response builder

```cpp
HttpResponse buildResponse(
    const HttpRequest& request,
    const RouteResult& route);

HttpResponse buildErrorResponse(
    int statusCode,
    const RouteResult& route);
```

The builder may read files, generate autoindex pages, load configured error pages, or prepare CGI-derived content. Generic `HttpResponse::makeError()` remains the fallback when a configured error page is unavailable.

### Serializer

```cpp
std::string toBytes(
    const HttpResponse& response,
    bool headRequest);
```

Serializer invariants:

- output uses an HTTP status line followed by CRLF-delimited headers;
- exactly one authoritative `Content-Length` is calculated from the buffered body when allowed;
- informational responses, `204`, and `304` do not contain a body;
- a response to `HEAD` advertises the equivalent body length but does not append the body bytes;
- there is exactly one empty CRLF line between headers and body;
- the serializer does not read configuration or files.

Because the initial implementation buffers the whole response body in a string, it does not need chunked response encoding. This is a deliberate initial limitation.

## Body ownership and limits

The initial implementation uses `std::string` for request and response bodies. `std::string` can hold binary bytes, including zero bytes; it is not limited to human-readable text.

Rules:

- the parser owns a partial request body while parsing;
- `HttpRequest` owns the completed request body after `takeRequest()`;
- `HttpResponse` owns the completed response body;
- the serializer reads but does not take ownership of the response;
- the connection owns only serialized response bytes in its write buffer;
- configured and hard safety limits are checked before unbounded append operations.

File-backed or streaming bodies may be introduced later if required. They are not part of the first common contract.

## CGI boundary

CGI is asynchronous relative to the client socket. The CGI runtime must not block the event loop waiting for the child.

Before CGI branches integrate, define an owning runtime object containing at least:

```text
child PID
stdin pipe FD
stdout pipe FD
start time
output buffer
owning client/session identifier
completion state
```

The event loop observes the CGI pipe descriptors but does not own them. CGI cleanup must be idempotent, and a timed-out process must be terminated and reaped according to the project requirements.

Data required after the original request is destroyed must be copied or moved into the CGI runtime object. It must not retain views into a mutable connection buffer.

## Event-loop integration rules

The first implementation is single-threaded and non-blocking.

Only the network/runtime layer calls `accept()`, `recv()`, `send()`, and `poll()` for client sockets. Other modules operate on values and buffers.

Rules:

- accepted sockets are made non-blocking immediately;
- `recv() == 0` means the peer closed its side of the connection;
- `EAGAIN`/`EWOULDBLOCK` is normal for non-blocking I/O;
- one `recv()` is not assumed to contain one complete request;
- one `send()` is not assumed to transmit a complete response;
- collection removals are deferred until the current poll-event iteration ends;
- `POLLIN` and `POLLOUT` interest is updated from connection state;
- idle timeouts use `std::chrono::steady_clock`;
- closing a session removes it from poll tracking and destroys the owning `Connection` exactly once.

No locking or shared mutable state is required in this baseline because one event-loop thread owns all sessions.

## Suggested file ownership by concern

```text
includes/
|- ConnectionState.hpp
|- RouteResult.hpp
|- config/
|  `- ServerConfig.hpp
|- http/
|  |- CaseInsensitiveLess.hpp
|  |- HeaderMap.hpp
|  |- HttpRequest.hpp
|  |- HttpRequestParser.hpp
|  |- HttpResponse.hpp
|  |- ParseResult.hpp
|  `- ResponseBuilder.hpp
|- network/
|  |- Connection.hpp
|  `- ClientSession.hpp
|- routing/
|  `- Router.hpp
`- cgi/
   `- CgiProcess.hpp

src/
|- config/
|- http/
|- network/
|- routing/
`- cgi/
```

Branch ownership:

- network branch: `Connection`, sessions, listeners, event loop, timeouts, partial I/O;
- parser branch: `HttpRequestParser` implementation and parser tests;
- configuration branch: syntax parsing, validation, defaults, inheritance, immutable models;
- router branch: virtual-server selection, location matching, methods, path resolution;
- response branch: response builder, serialization, static files, errors, autoindex;
- CGI branch: process environment, pipes, execution, timeout, output parsing.

Branches may include shared headers but should not redefine shared types locally.

## Build and verification gate

Before the team branches from the common commit:

1. Every shared header is included by a clean translation unit and compiles independently.
2. Every committed `.cpp` compiles with the agreed standard and warning flags.
3. The repository contains one build command used by every teammate.
4. A minimal executable links successfully.
5. A contract smoke test exercises the complete fake lifecycle.

Minimum smoke scenarios:

- a request split across multiple parser feeds;
- two requests present in one input buffer, with only the first consumed;
- malformed syntax producing `400` and an internal diagnostic;
- case-insensitive header lookup;
- absent versus zero `Content-Length`;
- HTTP/1.1 and HTTP/1.0 keep-alive decisions;
- a route result selecting each action without contradictory flags;
- a `200` response containing exactly one correct `Content-Length`;
- a `HEAD` response containing no body bytes;
- a `204` response containing neither body nor `Content-Length`;
- partial response sending with the correct offset;
- moving a `Connection` without double-closing its FD;
- erasing a session closing its client FD exactly once.

## Common-baseline definition of done

The base layer is ready to merge to `main` and branch from when all of the following are true:

- [ ] The required C++ standard is confirmed.
- [ ] Shared header names, include guards, includes, and declarations compile cleanly.
- [ ] `HttpRequest` owns its stored data and remains protocol-only.
- [ ] `HeaderMap` and duplicate-header policy are documented.
- [ ] Parser status, byte-consumption, request transfer, and reset behavior are fixed.
- [ ] Every client has its own parser state.
- [ ] `Connection` is the sole owner of its client FD and has safe move operations.
- [ ] The state machine and allowed transitions are documented.
- [ ] Configuration types have agreed, immutable post-startup semantics.
- [ ] `RouteResult` contains exactly one action and safe defaults.
- [ ] Router and response-builder interfaces exist.
- [ ] Response serialization produces one valid message framing decision.
- [ ] Body ownership and maximum-size rules are defined.
- [ ] One shared build command and the contract smoke tests pass.

After this point, the common headers are treated as team contracts. A branch that needs a contract change first documents the need and coordinates the change, rather than silently introducing a private alternative.
