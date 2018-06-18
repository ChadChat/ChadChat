#include "io.h"
#include <stdio.h>
#include <string.h>

static size_t clients_connected = 0;

static void on_client_close(io_client_t* client) {
    size_t client_id = (size_t) client->user_data;
    printf("Client %lu disconnected\n", client_id);
}

static void on_client_read(io_client_t* client, const char* data, size_t len) {
    size_t client_id = (size_t) client->user_data;
    if (strstr(data, "\r\n\r\n") != NULL) {
        printf("Client %lu received http packet\n", client_id);
        const char* resp = "HTTP/1.1 200 Ok\r\nContent-Length: 11\r\nHello world\r\n\r\n";
        io_client_write(client, resp, sizeof(resp) - 1);
    }
}

static void on_client_connect(io_client_t* client) {
    size_t client_id = clients_connected++;
    client->user_data = (void*) client_id;
    client->on_close = on_client_close;
    client->on_read = on_client_read;
    printf("Client %lu connected\n", client_id);
}

int main(int argc, char* argv[]) {
    io_loop_t loop;
    int port = 12345;

    if (!io_loop_init(&loop, port)) {
        fprintf(stderr, "Failed to initialize io loop on port %d. Try a different port\n", port);
        return -1;
    }

    printf("Server started on :%d\n", port);
    loop.on_connect = on_client_connect;
    return io_loop_run(&loop);
}