before end phase 0:

1. HttpMethod representation: enum for supported methods or a normalized string if extension methods must remain possible.

2. Shared header type, e.g. typedef std::map<std::string, std::string, CaseInsensitiveLess> HeaderMap;.

3. HttpStatus/error contract covering parser, router, configuration, CGI, and filesystem failures.

4. Body ownership and limits: decide whether request/response bodies are strings, buffers, or files, and who owns temporary files.

5. Configuration inheritance rules: define how server- and location-level values combine. This is a common integration conflict.

Also document these decisions before branching:
- Are header names case-insensitive but values preserved?
- Are duplicate headers combined or stored separately?
- Does HttpRequest contain parsed route/config references, or remain purely HTTP?
- Are configuration objects immutable after startup?
- Who owns pointers/references inside Connection and RouteResult?
- Can one connection contain multiple pipelined requests?
- Does the parser stop exactly after one request and return consumed bytes?

I would keep HttpRequest protocol-only and introduce a small per-request execution object such as RequestContext containing the selected server/location, connection metadata, and resolved filesystem/CGI information. That prevents networking, parsing, and routing branches from all adding unrelated fields to HttpRequest.

After that, the team can safely branch into:
1. socket/event loop and connections;
2. incremental HTTP parser;
3. configuration parser and inheritance;
4. routing, response generation, static files, and CGI.

Before branching, commit one “contracts” baseline containing the headers plus a few shared integration examples: fragmented request, malformed request, virtual host selection, location match, keep-alive, and CGI route. That will prevent most merge-time surprises.

//??? new file?
The largest missing piece: interfaces
You have data objects, but the branches still do not know how to call each other. Freeze these signatures before branching:
class HttpRequestParser {
public:
    ParseResult feed(std::string_view bytes);
    const HttpRequest& request() const;
    void reset();
};

RouteResult routeRequest(
    const HttpRequest& request,
    const std::vector<ServerConfig>& candidates
);

HttpResponse buildResponse(
    const HttpRequest& request,
    const RouteResult& route
);

std::string serializeResponse(const HttpResponse& response);
The exact names can change; stable producer/consumer behavior is what matters.
You also need a CGI runtime contract if CGI and networking will be developed separately: PID, stdin/stdout FDs, start time, output buffer, owning connection identifier, timeout, and explicit FD ownership.

!!!!!!!!!!!!Readiness gate
You can safely create separate branches once:
- every shared header is independently includable;
- ServerConfig and LocationConfig have stable immutable shapes;
- FD ownership is documented;
- request fields own their data;
- RouteResult has one unambiguous action;
- parser/router/response entry-point signatures are committed;
- a tiny fake lifecycle compiles: bytes → request → route → response → serialized bytes
+++++++++++++++++++++++++++++++++++++++++

connection: 
Connection FD ownership
The default move operations are unsafe:
Connection(Connection&&) noexcept = default;
A default move copies the integer FD. The moved-from object still closes it, leaving the moved-to object with an invalid descriptor and potentially causing a second close.
Choose one ownership rule:
- Recommended: Connection owns the client FD, closes it in its destructor, and implements a custom move that sets other.fd = -1.
- Alternatively, the event loop owns and closes FDs, and Connection must not close them.

	-IVAN 4.SEPT;