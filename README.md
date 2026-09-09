
# WEB-Serv-42
building a small web server: communication machine
https://muta-pro.github.io/WEB-Serv-42/

Base-layer contracts and architecture: [docs/BASE_LAYER_ARCHITECTURE.md](docs/BASE_LAYER_ARCHITECTURE.md)

**Our architecture of responsabilites:**
- receiving requests
- understanding and deciding them
- preparing the final response

## 1st block : engine & traffic control/circulation

movement & timing management: open listening sockets, event loop (*alive and monitoring activity*), accepting clients, watching multiple connections, reading incoming data, writing outgoing data, keep track client's states (connected/sending/waiting/finished), closing/resetting connections

## 2nd block : decision making/meaning interpretation

server's brain: reading config file at startup, turning it into structured internal rules, reading an HTTP request runtime, 
understand method/path/headers/body,
check which server block/loc mathces, decide return object (file/error/redir/CGI)

## 3d block : response back to a clinet

final answer & delivery departement: after receiving instruction on what to send back (request/ routing decision/ resolved file path or error code) - builds **HTTP response**, format status line & headers, 
attach response body, serving static files(HTML, CSS, image, txt), generate *error pages(404/403)* and *autoindex pages*(folder content display), choose content type from file extention, prepare final output in clean format

## goal : we want to separate the project into three clear responsibilities, following the actual life cycle of a request

**Contract objects** Every layer of your server communicates only through these shared objects.
Never call across layer boundaries directly.

hpps: RouteResult, ConnectionState

Liza: http - request (parsing incoming bytes & decide routing)
Ravi: http - response (formatting final output)
	  cgi - parse CGI script's output
	  cofig - ConfigParser , ServerConfig (reads .conf at start to config server)
Ivan: cgi - CgiHandler exec.
	  network - runtime engine (socket/client/serverManager)

**Startup : first the server's rulebook, then listen**
**The config file.**
- which port to listen on
- where files are stored
- what routes exist
- which methods are allowed
- where error pages are
- when CGI should be used

**Runntime loop**
- wait for activity
- accept a client
- receive a data - ***HTTP request***
- understand the request
- decide what it means
- prepare the answer
- send it back
- keep or close the connection

## **work plan**

**august**:
1. make server boot and accept connections
2. request understanding
3. return correct answers

**september**:

4. integration: conncet three parts
	- plug request parsing into engine
	- plug route decision into repsonse builder
	- plug final response into writing stage
	- test end-to-end flow

**october**:

5. finalize
	- keep-alive feature
	- CGI
	- edge cases
	- cleanup
	- error handling
	- testing
	- final refactor
## 1st block : engine & traffic control/circulation

movement & timing management: open listening sockets, event loop (*alive and monitoring activity*), accepting clients, watching multiple connections, reading incoming data, writing outgoing data, keep track client's states (connected/sending/waiting/finished), closing/resetting connections

## 2nd block : decision making/meaning interpretation

server's brain: reading config file at startup, turning it into structured internal rules, reading an HTTP request runtime, 
understand method/path/headers/body,
check which server block/loc mathces, decide return object (file/error/redir/CGI)

## 3d block : response back to a clinet

final answer & delivery departement: after receiving instruction on what to send back (request/ routing decision/ resolved file path or error code) - builds **HTTP response**, format status line & headers, 
attach response body, serving static files(HTML, CSS, image, txt), generate *error pages(404/403)* and *autoindex pages*(folder content display), choose content type from file extention, prepare final output in clean format

## goal : we want to separate the project into three clear responsibilities, following the actual life cycle of a request

Contract objects Every layer of your server communicates only through these shared objects. Never call across layer boundaries directly.

hpps: RouteResult, ConnectionState

Liza: http - request (parsing incoming bytes & decide routing) Ravi: http - response (formatting final output) cgi - parse CGI script's output cofig - ConfigParser , ServerConfig (reads .conf at start to config server) Ivan: cgi - CgiHandler exec. network - runtime engine (socket/client/serverManager)

Startup : first the server's rulebook, then listen The config file.

which port to listen on
where files are stored
what routes exist
which methods are allowed
where error pages are
when CGI should be used
Runntime loop

wait for activity
accept a client
receive a data - HTTP request
understand the request
decide what it means
prepare the answer
send it back
keep or close the connection
work plan
august:

make server boot and accept connections
request understanding
return correct answers
september:

integration: conncet three parts
plug request parsing into engine
plug route decision into repsonse builder
plug final response into writing stage
test end-to-end flow
october:

finalize
keep-alive feature
CGI
edge cases
cleanup
error handling
testing
final refactor
