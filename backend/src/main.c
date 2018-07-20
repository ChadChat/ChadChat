#include "io.h"
#include <stdio.h>
#include <string.h>
#include "server.h"

int main(int argc, char* argv[]) {
    io_loop_t loop;
    int port = 12345;

    if (!io_loop_init(&loop, port)) {
        fprintf(stderr, "Failed to initialize io loop on port %d. Try a different port\n", port);
        return -1;
    }
    server_init();
    printf("Server started on :%d\n", port);
    loop.on_connect = on_client_connect;
    return io_loop_run(&loop);
}