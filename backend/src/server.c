#include "server.h"
#include "ws.h"
#include "io.h"
#include "endpoint.h"
#include "endpoints/index.h"
#include "utils/list.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static uint32_t clients_connected = 0;
static endp* ep;
hmap* globals;

void server_init()
{
    // initialize the endp struct.
    ep = init_endpoint(client_write);
    globals = hmap_init(50, free);
    LIST* list = LIST_new(20);
    hmap_insert(globals, "clients", list);
    register_endpoint(ep, "/", ALL_TYPE, "GET", get_index); // This is how we add callback functions to a url.
}

void on_client_connect(io_client_t* client)
{
    client->on_close = NULL;
    client->on_read = on_client_read;
    ws_client_t* ws_client = malloc(sizeof(ws_client_t));
    memset(ws_client, 0, sizeof(ws_client_t));
    ws_client->state = CL_HANDSHAKE;
    ws_client->id = malloc(sizeof(endp_client));
    // ws_client->id_cb = destroy_endp_client;
    ((endp_client*)(ws_client->id))->client_id = 0;
    // ((endp_client*)(ws_client->id))->key_vals = sm_new(20);
    ((endp_client*)(ws_client->id))->data = NULL;
    // ws_client->id = ++clients_connected;
    ws_client->client_data = (void* )client;
    client->user_data = ws_client;
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

void destroy_server()
{
   /* MORE TO GO HERE, LIKE A SHIT TON OF CALLBACKS */
}
