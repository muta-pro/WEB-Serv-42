#ifndef CONNECT_STATE
#define CONNECT_STATE

enum ConnectionState {
    readingHeaders,
    readingBody,
    processing,
    cgiRunning,
    writingResponse,
    closed
};

#endif
