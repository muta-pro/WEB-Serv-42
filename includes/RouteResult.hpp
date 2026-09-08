/*
Lisa produces this obj after parsing the request
Ivan decide if to run the script
Ravi reads targetPath to knwo what to sand back to the user
*/

#ifndef ROUTERESULT_HPP
#define ROUTERESULT_HPP

#include <string>


struct RouteResult {
	const ServerConfig		*server; //Lisa
	const LocationConfig	*loc;		//Lisa
};

enum class RouteAction {
	staticFile,
	directory,
	redirect,
	upload,
	deleteResource,
	CGI,
	Error
};

#endif

/*
CHECK THIS;
RouteResult should then contain one action, an HTTP status, 
the selected immutable configuration, and only the action-specific paths/data.
If it stores configuration pointers, establish 
that all configurations are fully constructed 
before the event loop starts and never moved or mutated afterward.
*/