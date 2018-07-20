#ifndef _IO_H
#define _IO_H

#include <stddef.h>
#include <stdbool.h>

#define NUM_EPOLL_EVENTS 64
#define READ_BUFFER_SIZE 4096

typedef struct io_loop_t io_loop_t;
typedef struct io_client_t io_client_t;
typedef void (*io_client_cb) (io_client_t* client);
typedef void (*io_client_iter_cb) (io_client_t* client, void* state);
typedef void (*io_client_data_cb) (io_client_t* client, const char* data, size_t size);

struct io_client_t {
    int fd;
    io_loop_t* loop;
    void* user_data;
    io_client_t* last;
    io_client_t* next;
    io_client_cb on_close;
    io_client_data_cb on_read;
};

typedef struct {
    io_client_t* head;
    io_client_t* tail;
} io_client_list_t;

struct io_loop_t {
    int poll_fd;
    int server_fd;
    io_client_cb on_connect;
    io_client_list_t client_list;
};

// create server socket and poll handle for io loop 
bool io_loop_init(io_loop_t* loop, int port);

// start polling for io and handling tasks
int io_loop_run(io_loop_t* loop);

// iterate through connected clients on the io loop
void io_loop_iter_clients(io_loop_t* loop, io_client_iter_cb callback, void* state);

// close an active client connection
void io_client_close(io_client_t* client);

// write to an active client connection
bool io_client_write(io_client_t* client, const char* data, size_t size);

#endif // _IO_H