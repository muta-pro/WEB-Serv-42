# WEB-Serv-42
building a small web server: communication machine

**Our architecture of responsabilites:**
- receiving requests
- understanding and deciding them
- preparing the final response

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
- receive a data - ***HTTP request**
- understand the request
- decide what it means
- prepare the answer
- send it back
- keep or close the connection

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

## **work plan**

**MAY-JULY**:
1. make server boot and accept connections
2. request understanding
3. return correct answers

**JULY-AUGUST**:

4. integration: conncet three parts
	- plug request parsing into engine
	- plug route decision into repsonse builder
	- plug final response into writing stage
	- test end-to-end flow

**SEPTEMBER**:

5. finalize
	- keep-alive feature
	- CGI
	- edge cases
	- cleanup
	- error handling
	- testing
	- final refactor
