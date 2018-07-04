#ifndef WS_IMP_CHAD
#define WS_IMP_CHAD

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define GUID_LEN 36
#define MAX_REPLY_SIZE 1024

// WS-Opcodes
#define PAY_CONTN 0x00
#define INCL_UTF8 0x01
#define INCL_DATA 0x02
#define TERMINATE 0x08
#define PING 0x09
#define PONG 0x0a

// Client States
#define CL_HANDSHAKE 1
#define CL_OPEN 2
#define CL_CLOSED 3

// Server Indiaction STATUS CODEs
#define SUCCESS_STATUS 1
#define CUSTOM_STATUS 2

// Status codes defined by the RFC: https://tools.ietf.org/html/rfc6455#section-7.4.1
#define NORMAL_CLOSURE  1000
#define GOING_AWAY      1001
#define PROTOCOL_ERROR  1002
#define UNKNOWN_DATA    1003
#define DATA_INCONSIST  1007
#define VIOLATES_POLICY 1008
#define ITS_TOO_BIG     1009
#define CLIENT_EXT_ERR  1010
#define UNEXP_CONDITION 1011

#define UNMASKED_FRAME  PROTOCOL_ERROR

// if 0 <= payload_len <= 125
struct  __attribute__((packed, scalar_storage_order("big-endian"))) ws_frame_op1
{
    uint32_t masking_key;
    void* payload;
};

// if payload_len == 126
struct  __attribute__((packed, scalar_storage_order("big-endian"))) ws_frame_op2
{
    uint16_t payload_len;
    uint32_t masking_key;
    void* payload;
};

// if payload_len == 127
struct  __attribute__((packed, scalar_storage_order("big-endian"))) ws_frame_op3
{
    uint64_t payload_len;
    uint32_t masking_key;
    void* payload;
};

typedef struct
{
    uint8_t fin: 1, rsv1: 1 , rsv2: 1, rsv3: 1, opcode: 4;
    uint8_t mask: 1, payload_len: 7;
    union _inner
    {
        struct ws_frame_op1 op1;
        struct ws_frame_op2 op2;
        struct ws_frame_op3 op3;
    }inner;    // @TODO: make a better name rather than using inner

}  __attribute__((packed, scalar_storage_order("big-endian"))) ws_frame_t;  // this attribute thing will only work above < GCC 6.2

typedef bool (*ws_write_cb)(void* client, const char* data, size_t len);

// Struct for handling each client.
typedef struct
{
    uint16_t id;
    uint8_t state: 4, more_to_read: 1, expect_pong: 1, pong: 1, expect_close: 1;   // A state value indicating the state of the client and a flag to see if there's more to read.
    ws_frame_t* last_frame;
    size_t data_read;  // pronouned red, past tense of read.
    char* endpoint;
    char* method;
    ws_write_cb write;
    void* client_data;
} ws_client_t;
// the pong flag of the struct indicates that we got the pong back.

void handle_data(ws_client_t* client, const char* data, size_t len);
void handle_handshake(ws_client_t* client, const char* data);
void handle_open(ws_client_t* client, const void* data, size_t len);
char* ws_create_accept_hdr(const char* ws_key, const size_t len);
char* handshake_srvr_reply(int success, const char* ws_key_hdr, const char* status_line, const char* other_hdrs);
ws_frame_t* construct_ws_frame(uint8_t opcode, void* payload, uint64_t payload_len);
bool send_ws_frame(ws_client_t* client, uint8_t opcode, void* payload, size_t payload_len);
void destroy_ws_frame(ws_frame_t* ws_frame);
void handle_ping(ws_client_t* client, void* payload, uint64_t payload_len);
void send_ping(ws_client_t* client, void* payload, uint64_t payload_len);
bool valid_client_frame(ws_frame_t* frame);
void handle_close(ws_client_t* client, void* payload, uint64_t payload_len);
void destroy_ws_client(ws_client_t* client);
uint32_t get_rand_maskingkey();
void unmask_data(uint32_t masking_key, unsigned char* masked, uint64_t payload_len);
void* memdup(const void* src, size_t len);

#endif