#ifndef SERVER_IMP_CHAD
#define SERVER_IMP_CHAD

#include "io.h"
#include <stdbool.h>

void on_client_connect(io_client_t* client);
void on_client_read(io_client_t* client, const char* data, size_t len);
bool client_write(void* client, const char* data, size_t len);
void close_io_client(void* client);


#endif