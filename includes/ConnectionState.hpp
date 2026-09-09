#ifndef CONNECT_STATE
#define CONNECT_STATE

enum ConnectionState {
    ReadingHeaders,
    ReadingBody,
    Processing,
    CgiRunning,
    WritingResponse,
    Closed
};

#endif
