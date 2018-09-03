#ifndef ENDPT_IMP_CHAD
#define ENDPT_IMP_CHAD

#include "ws_parser.h"
#include "ws.h"
#include "utils/hashmap.h"
#include "utils/strmap.h"
#include <stdbool.h>

// These are the possible data types the frame can have
#define UTF8_TYPE 1
#define BIN_TYPE  2
#define ALL_TYPE  3

// These are the types of request/response the endpoint can handle
#define HNDL_HSHAKE 1
#define HNDL_FRAME  2

// These are the status codes the handshake can send back
#define SUCCESS 101
#define MOVED_PERM 301
#define NOT_MODIFIED 304
#define BAD_REQUEST 400
#define UNAUTHORIZED 401
#define FORBIDDEN 403
#define NOT_FOUND 404
#define USERNAME_NOT_AVAIL 410 // This is a user defined status code.

#define PING_TIMEOUT 10

typedef struct {
    uint16_t client_id;
    StrMap* key_vals;
    void* data;
}endp_client;

typedef struct
{
    char req_type;
    union _req_data
    {
        struct _hshake_req
        {
            StrMap* headers;
            StrMap* data;
        }hshake;
        struct _frame_req
        {
            size_t data_len;
            void* data;
        }frame;
    }req_data;
}request;

typedef struct
{
    char res_type;
    union _res_data
    {
        struct _hshake_res
        {
            short status;
            char* accept;
            StrMap* headers; // This could be useful when you want to give out other headers.
        }hshake;
        struct _frame_res
        {    
            size_t data_len;
            bool is_utf8;
            void* data;
        }frame;
    }res_data;
    bool close_client;
}response;

typedef response* (*endpoint_cb)(const request* req, const char data_type, endp_client* ep_client);
typedef bool (*write_cb)(void* client, const char* data, size_t len);

typedef struct
{
    hmap* cb_funcs;
    write_cb write_to;
}endp;

typedef struct _cb_list
{
    char* method;
    endpoint_cb cb;
    char data_type;
    struct _cb_list* next;
}cb_list;


endp* init_endpoint(write_cb cb);
void register_endpoint(endp* ep, const char* url, char type, const char* method, endpoint_cb cb_func);
void destroy_endpoint(endp* ep);
void destroy_member(void* member);
void destroy_member(void* member);
static endpoint_cb get_endpoint_cb(const endp* ep, const char* url, const char* method, char data_type);
void handle_data(const endp* ep, ws_client_t* client, const char* data, size_t len);
void handle_handshake(const endp* ep, ws_client_t* client, const char* data);
void handle_open(const endp *ep, ws_client_t* client, const void* data, size_t len);
void send_response(const endp* ep, ws_client_t* client, response* res);
void send_ws_frame(const endp* ep, ws_client_t* client, uint8_t opcode, void* payload, size_t payload_len);
void endpoint_handshake(const endp* ep, ws_client_t* client, const ws_handshake* hshake);
void endpoint_data_frame(const endp* ep, ws_client_t* client, ws_frame_t** frame_mem);
void handle_ws_req(const endp* ep, ws_client_t* client, const ws_frame_t* frame, const char data_type, endpoint_cb cb);
void send_ping(const endp* ep, ws_client_t* client, void* payload, size_t pay_len);
void send_close(const endp* ep, ws_client_t* client, uint16_t status_code, const void* reason, size_t reason_len);
void send_not_found(const endp* ep, ws_client_t* client);
void destroy_request(request* req);
void destroy_response(response* resp);

#endif
