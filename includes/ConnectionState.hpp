#ifndef CONNECT_STATE
#define CONNECT_STATE

enum CnnectionState {
    READING,
    PARSING,
    ROUTING,
    CGI_WAITING,
    BUILDING,
    SENDING,
    DONE
};

#endif
