/*
Lisa produces this obj after parsing the request
Ivan decide if to run the script
Ravi reads targetPath to knwo what to sand back to the user
*/

#ifndef ROUTERESULT_HPP
#define ROUTERESULT_HPP


struct RouteResult {
	const ServerConfig		*server; //Lisa
	const LocationConfig	*loc;		//Lisa
	bool					useCGI; //ivan
	std::string				targetPath; //ravi
	std::string				redirectURL;
	bool					isUpload;
	bool					isDelete;
};

#endif