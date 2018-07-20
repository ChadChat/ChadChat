#include "io.h"

#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

static inline int change_event(io_loop_t* loop, int fd, int op, int events, void* data) {
    // register an epoll event on the io loop's epoll handle
    struct epoll_event event = { 0 };
    event.data.fd = fd;
    event.events = events;
    if(data != NULL)
        event.data.ptr = data;
    return epoll_ctl(loop->poll_fd, op, fd, &event);
}

static inline int create_server(int port) {
    // create server socket
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0)
        return -1;

    // create address to bind server to
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((unsigned short) port);

    // bind server address & prep for non blocking listening
    int enable = 1;
    fcntl(server_fd, F_SETFL, fcntl(server_fd, F_GETFL, 0) | O_NONBLOCK); // set non blocking
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)); // allow address reuse
    if ((bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) || (listen(server_fd, 128) < 0)) {
        close(server_fd);
        return -1;
    }

    return server_fd;
}

//////////////////////////////////////////////////////

bool io_client_write(io_client_t* client, const char* data, size_t size) {
    ssize_t written = 0;
    if (client->fd > 0) {

        // make sure all bytes that can be written, are written
        do {
            if ((written = write(client->fd, &data[written], size - written)) == -1)
                break;
        } while (written > 0);

        // close client if socket error
        if (written == -1 && !(errno == EAGAIN || errno == EWOULDBLOCK)) {
            io_client_close(client);
            return false;
        }
    }

    return true;
}

void io_client_close(io_client_t* client) {
    
    // fix client list
    io_client_list_t* client_list = &client->loop->client_list;
    if (client == client_list->head) { // remove if the client is at the head.
        if (client->next) { // if the next ones not null.
            client_list->head = client->next;
            client->next->last = NULL;  // removed the reference to this client.
        } else {    // this means that there's only one element in the list.
            client_list->head = client_list->tail = NULL;
        }
    } else if (client == client_list->tail) {  // remove if the client is at the end.
        client_list->tail = client->last;
        client_list->tail->next = NULL;
    } else {    // This means that the client is between the head and the tail.
        io_client_t* temp;
        for (temp = client_list->head; temp != client && temp != NULL; temp = temp->next) {}    // make sure the client is in the list, if not return.
        if(temp == NULL)
            return;
        client->last->next = client->next;
        client->next->last = client->last;
    }

    // close socket and set client to closed
    if (client->fd < 0)
        return;
    close(client->fd);
    client->fd = -1;

    // call callback & free client data
    if (client->on_close)
        client->on_close(client);
    free(client);
}

bool io_loop_init(io_loop_t* loop, int port) {
    // create epoll handle
    if ((loop->poll_fd = epoll_create1(0)) < 0)
        return false;

    // create server socket
    if ((loop->server_fd = create_server(port)) < 0) {
        close(loop->poll_fd);
        return false;
    }

    loop->client_list.head = NULL;
    loop->client_list.tail = NULL;

    // register server socket to epoll handle
    if (change_event(loop, loop->server_fd, EPOLL_CTL_ADD, EPOLLIN | EPOLLET, NULL) < 0) {
        close(loop->server_fd);
        close(loop->poll_fd);
        return false;
    }

    return true;
}

void io_loop_iter_clients(io_loop_t* loop, io_client_iter_cb callback, void* state) {
    // iterate over all active clients
    if(loop->client_list.head == NULL)
        return;
    for (io_client_t* client = loop->client_list.head; client != NULL; client = client->next)
        callback(client, state);
}

int io_loop_run(io_loop_t* loop) {
    io_client_t* client;
    struct epoll_event event;
    char read_buffer[READ_BUFFER_SIZE + 1];
    struct epoll_event events[NUM_EPOLL_EVENTS];

    // main event loop
    while (true) {

        // iterate through received io events
        int num_events = epoll_wait(loop->poll_fd, events, NUM_EPOLL_EVENTS, -1);
        while (num_events--) {
            event = events[num_events];
            client = (io_client_t*) event.data.ptr;

            // close client (if any) on socket error
            if ((event.events & (EPOLLERR | EPOLLHUP)) || !(event.events & EPOLLIN)) {
                if (client != NULL)
                    io_client_close(client);

            // handle incoming clients on the server
            } else if (event.data.fd == loop->server_fd) {
                while (true) {
                    
                    // accept a client
                    struct sockaddr addr;
                    socklen_t addr_len = sizeof(addr);
                    int client_fd = accept(loop->server_fd, &addr, &addr_len);
                    if (client_fd == -1)
                        break;

                    // setup the client & create a client structure
                    int enable = 1;
                    fcntl(client_fd, F_SETFL, fcntl(client_fd, F_GETFL, 0) | O_NONBLOCK);
                    setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(enable));
                    io_client_t* client = malloc(sizeof(io_client_t));

                    // bail and close all client data if not able to register to epoll handle
                    if (change_event(loop, client_fd, EPOLL_CTL_ADD, EPOLLIN | EPOLLET, (void*) client) < 0) {
                        free(client);
                        close(client_fd);
                        continue;
                    }

                    // load client values & add to servers client list
                    client->loop = loop;
                    client->next = NULL;
                    client->fd = client_fd;
                    client->on_read = NULL;
                    client->on_close = NULL;
                    if(loop->client_list.head == NULL)  // insert at the head.
                    {
                        client->last = NULL;
                        loop->client_list.head = client;
                        loop->client_list.tail = client;
                    }
                    else    // insert at the tail.
                    {
                        client->last = loop->client_list.tail;
                        loop->client_list.tail = client;
                    }

                    // call connect callback on newly created client
                    if (loop->on_connect)
                        loop->on_connect(client);
                }

            // client has data to be read
            } else if ((event.events & EPOLLIN) && client != NULL) {
                ssize_t bytes = 1;
                while (bytes > 0) {
                    switch (bytes = read(client->fd, read_buffer, READ_BUFFER_SIZE)) {
                    // client closed the connection
                    case 0:
                        bytes = 0;
                        break;
                    // no more data to read or error
                    case -1:
                        if (errno != EAGAIN)
                            bytes = 0;
                        break;
                    // client read some data
                    default:
                        if (client->on_read) {
                            read_buffer[bytes] = '\0';
                            client->on_read(client, read_buffer, (size_t) bytes);
                        }
                    }
                }

                // check if connection was closed & close client
                if (bytes == 0)
                    io_client_close(client);
            }
        }
    }

    return 0;
}