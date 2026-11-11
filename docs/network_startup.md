# Network part 1: configuration-driven listener startup

## Goal

Read the existing configuration and ask the operating system to open the required
TCP listening sockets.

This version is a startup check: it reports success and then closes the sockets.
There is no event loop or HTTP service yet.

## Vocabulary

- **Host and port:** the address clients connect to, such as 127.0.0.1:8080.
  127.0.0.1 means this computer; 0.0.0.0 means all local IPv4 interfaces.
- **Socket:** a networking resource managed by the operating system.
- **File descriptor (FD):** the integer our process uses to refer to that socket.
  Port 8080 and FD 3 are different things.
- **Listener:** a socket prepared to receive new connections.
- **Owner:** the C++ object responsible for closing an FD exactly once.
- **Candidate servers:** configuration blocks sharing one listener. The router
  will later choose one using the request's Host header, or the first as default.

## Files and execution order

```text
main.cpp
  -> ConfigParser::parseFile(path)     returns existing configuration data
  -> ServerManager::openListeners()   groups settings by listening address
  -> Socket::createListener()         opens each listening socket
  -> print success
  -> destroy manager                 closes the listening sockets
```

- main.cpp selects the configuration path and reports startup errors.
- ServerManager groups configurations and retains the resulting listeners.
- Socket owns listening FDs and performs the operating-system calls.
- tests/listener_startup.cpp checks grouping, real sockets, and cleanup.

The existing parser, Connection, router, response-builder, and CGI interfaces
are unchanged.

## What opening a listener means

1. getaddrinfo translates the configured host and port into OS addresses.
   We request IPv4 TCP addresses and try them in resolver order until one works.
2. socket creates the resource and returns its FD.
3. SO_REUSEADDR helps restarting after previous connections. It does not replace
   grouping configuration blocks that share one listener.
4. O_NONBLOCK prepares the socket for a future event loop: operations must return
   control when they cannot make progress.
5. FD_CLOEXEC prevents the listener from surviving a later successful execution
   of a CGI program in a child process.
6. bind attaches the socket to a local address and port.
7. listen enables the OS queue of incoming connections.

listen does not accept a client or read HTTP. The next milestone will use accept
to obtain a separate socket for each client.

The checked helper converts failed system calls into exceptions containing
their OS error message. Resolver failures use gai_strerror instead.

## Why grouping happens before opening sockets

Two server blocks sharing 127.0.0.1:8080 produce one socket with two candidates.
One server block with two distinct listen directives participates in two groups.

The map uses the existing Listen comparison. Candidate lists preserve server
declaration order, even though the map sorts the listener addresses.

The first candidate is the default server for that listener.

Identical repeated listen directives inside one block do not add duplicate
candidates.

## Configuration ownership

Settings are copied once per listener group at startup, then moved into the
final Listener objects.

This uses some additional configuration memory but matches the current router
interface directly:

```cpp
routeRequest(request, listener.candidates);
```

Keep the manager alive and leave its candidate vectors unchanged while
RouteResult objects borrow pointers into them.

Grouping compares configured host strings and ports. It does not normalize DNS
aliases. Distinct groups that cannot bind because their resolved addresses
overlap cause startup failure and cleanup. IPv6 is outside this first version.

## Socket ownership and cleanup

Socket closes its FD in its destructor. This is RAII: cleanup follows the
C++ object's lifetime, including when an exception leaves a scope.

Copying a Socket is forbidden because copying an FD number would create two
objects trying to close the same socket.

Moving transfers ownership and sets the old object's FD to -1.
Move assignment first closes the destination's previous FD.

ServerManager first opens sockets into a temporary pending vector. Only after
every opening succeeds does it transfer them to its final listeners vector.

If a later listener fails, destroying pending closes all earlier sockets.
A failed Socket creation also closes its own partially prepared socket.

Accepted client sockets will belong to the existing Connection class.
The future event loop may borrow a listener's FD but must not close it separately.

## Build and checks

Run from the repository root:

```sh
make test
make -C tests/ConfigParser
./webserv
./webserv conf/multi_server.conf
./webserv conf/invalid/port_out_of_range.conf
```

The first two executable runs should exit 0 if their addresses are available.
The invalid configuration must exit 1.

A missing configuration file, unavailable address, occupied port, or extra
command-line argument must also fail with a diagnostic.

The multi-server example should report:
- port 8080: two server candidates;
- port 8081: one candidate;
- port 8082: one candidate.

The executable then closes its listeners. curl is not an acceptance check for
this milestone because the program does not stay running or process HTTP.

make test includes listener checks for:
- shared-address grouping and candidate declaration order;
- multiple listening addresses;
- nonblocking and close-on-exec flags;
- a real TCP connection;
- move ownership and destructor cleanup;
- rollback after a later bind failure.

Network tests use OS-assigned ports. Port 0 is used directly in those tests only;
the configuration parser still rejects it.

During preparation, the unchanged parser's 20,000-server stress test exceeded
its default two-second limit. All 469 cases passed using the runner's documented
bounded timeout override:

```sh
make -C tests/ConfigParser ARGS="--timeout 10"
```

No parser source or assertions were changed.

## Explanation for the team

The configuration parser already gives us server settings. My code groups
settings that share a listening address and creates one nonblocking TCP listener
per group. Each listener owns its FD and retains its candidate servers in order.

If any listener fails to open, the earlier sockets are closed automatically.

This PR validates startup and shutdown. Accepting clients and implementing the
event loop are the next milestone.