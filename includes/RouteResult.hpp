/*
Lisa produces this obj after parsing the request
Ivan decide if to run the script
Ravi reads targetPath to knwo what to sand back to the user
*/

#ifndef ROUTERESULT_HPP
#define ROUTERESULT_HPP

#include <string>

struct ServerConfig;
struct LocationConfig;

struct RouteResult {
	const ServerConfig		*server = nullptr;
	const LocationConfig	*loc = nullptr;

	RouteAction	action = RouteAction::Error;
	int					StatusCode = 500;

	std::string	filesystemPath; //used for files/dir/uploads/deletion/CGI;
	std::string	redirectLocation; //used only when action == redirect;
};

enum class RouteAction {
	StaticFile,
	Directory,
	Redirect,
	Upload,
	DeleteResource,
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
