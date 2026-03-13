// These macros MUST be defined exactly once in the entire project
// before including the headers to avoid linker errors.
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include "server.h"

int main() {
    // 1. Create our server object (Instantiating the AppServer class)
    // Using 0.0.0.0 allows it to be accessed from your frontend, even in Docker.
    AppServer myStegoServer("0.0.0.0", 8080);

    // 2. Start listening for HTTP requests
    myStegoServer.start();

    return 0;
}