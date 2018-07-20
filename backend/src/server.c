#include "server.h"
#include "ws.h"
#include "io.h"
#include "endpoint.h"
#include "endpoints/index.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

//@TODO: make code for e2ee

static uint16_t clients_connected = 0;
static endp* ep;    // usefu

void server_init()
{
    // initialize the endp struct.
    ep = init_endpoint(client_write);
    register_endpoint(ep, "/", ALL_TYPE, "GET", get_index); // This is how we add callback functions to a url.
}

void on_client_connect(io_client_t* client)
{
    client->on_close = NULL;
    client->on_read = on_client_read;
    ws_client_t* ws_client = malloc(sizeof(ws_client_t));
    memset(ws_client, 0, sizeof(ws_client_t));
    ws_client->state = CL_HANDSHAKE;
    ws_client->id = ++clients_connected;
    ws_client->client_data = (void* )client;
    client->user_data = ws_client;
    printf("New client connected: %d\n", ws_client->id);
}

bool client_write(void* client, const char* data, size_t len)
{
    return io_client_write((io_client_t*)client, data, len);
}

void close_io_client(void* client)
{
    io_client_close((io_client_t*)client);
}

void on_client_read(io_client_t* client, const char* data, size_t len)
{
    handle_data(ep, client->user_data, data, len);
}
