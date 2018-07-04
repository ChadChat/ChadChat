#include "ws.h"
#include "ws_parser.h"
#include "server.h"
#include "utils/strmap.h"
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include "utils/b64.h"
#include <openssl/sha.h>
#include <sys/random.h>

// @TODO: make it so that the websocket can take in both GET & POST parameters.
// @TODO: make reply to ping/pong to be send only in a limited time.
// @TODO: add a last active member to remove clients by first sending them a ping and if they dont respond in a limited time delete the memory allocated for these cunts.
// @TODO: handle fragmentation in the WS spec use a linked list or just send it to the end point handler.
// @TODO: handle closing so that you can specify a status code indicating the reason for closing.

static const char GUID[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
static const char VALID_WS_RESPONSE[] = "Upgrade: websocket\r\nConnection: Upgrade\r\n";
static const char WS_SUCCESS_RESPONSE[] = "HTTP/1.1 101 Switching Protocols\r\n";
static const char WS_FAILURE_RESPONSE[] = "HTTP/1.1 400 Bad Request\r\n";

#define STRLEN_WS_FAIL  26
#define STRLEN_WS_SUCC  34
#define STRLEN_VALID_WS 41

// handle the close correctly otherwise freeing already free memory can have unpredictable effects.
void handle_data(ws_client_t* client, const char* data, size_t len)
{
    // printf("state: %d\n", client->state); // DEBUG
    switch(client->state)
    {
        case CL_HANDSHAKE:
            handle_handshake(client, data);
            break;
        case CL_OPEN:
            handle_open(client, data, len);
            break;
        case CL_CLOSED:
        default:
            // some code to close and give back the resource allocated.
            destroy_ws_client(client);
    }
}

// modify the handshake with sending different codes according to the reply of the function valid_ws_req.
void handle_handshake(ws_client_t* client, const char* data)
{
    // printf("client handshake begun\n");
    ws_handshake* hshake = parse_handshake(data);
    if(!valid_ws_req(hshake->headers) ||
        strcmp(hshake->resource_name, "/") != 0 ||
        strcmp(hshake->method, "GET") != 0 ||
        strcmp(hshake->http_version, "HTTP/1.1") != 0
        )
    {
        // printf("bad handshake closing\n");
        // code to send invalid request here.
        char* resp = (char*) malloc(STRLEN_WS_FAIL+STRLEN_VALID_WS+10);
        strcat(resp, WS_FAILURE_RESPONSE);
        strcat(resp, VALID_WS_RESPONSE);
        strcat(resp, "\r\n");
        client->write(client->client_data, resp, STRLEN_WS_FAIL+STRLEN_VALID_WS);
        client->state = CL_CLOSED;
        free(resp);
        destroy_handshake(hshake);
        return;
    }
    // printf("good handshake, getting ready for data frames\n");
    char* ws_key = malloc(WS_KEY_LEN+11);
    //printf("exists: %d\n", sm_exists(hshake->headers, "sec-websocket-key"));
    sm_get(hshake->headers, "sec-websocket-key", ws_key, WS_KEY_LEN+10);
    char* resp = handshake_srvr_reply(SUCCESS_STATUS, ws_key, NULL, NULL);  // We should be adding the subprotocol specs in the other_hdrs arg.
    client_write(client->client_data, resp, strnlen(resp, 512));
    // printf("DATA SEND: %s\n", resp);
    client->state = CL_OPEN;
    client->endpoint = strdup(hshake->resource_name);
    client->method = strdup(hshake->method);
    free(ws_key);
    free(resp);
    destroy_handshake(hshake);
}

// this is not a good idea. enable debug to see if this is even working.
// make sure to check for the pong flag and if we are expecting a pong before processing any other thing.
void handle_open(ws_client_t* client, const void* data, size_t len)
{
    ws_frame_t* ws_frame;
    if(client->more_to_read)
    {
        // This means that the payload was cut off due to the buffer limit in the read section. continue parsing the frame here.
        ws_frame_t* ws_frame = client->last_frame;
        size_t to_cpy = get_actual_pay_len(ws_frame) - client->data_read;
        to_cpy = to_cpy > len ? len : to_cpy;
        // copies the data remaining to read due to the buffer limit into the allocated space
        if(ws_frame->payload_len < 126)
            memcpy(ws_frame->inner.op1.payload + client->data_read, data, to_cpy);
        else if(ws_frame->payload_len == 126)
            memcpy(ws_frame->inner.op2.payload + client->data_read, data, to_cpy);
        else
            memcpy(ws_frame->inner.op3.payload + client->data_read, data, to_cpy);
        client->data_read += to_cpy;
    }
    else 
    {
        if((ws_frame = parse_frame(data, len, &(client->data_read))) == NULL)
        {
            // this means that the data was much bigger than we expeceted. so send an unpleasant message.
            char payload[] = "Eat a dick cunt.";
            send_ws_frame(client, INCL_UTF8, payload, strlen(payload));
            // other code to close this cunt and such.
            client->state = CL_CLOSED;
            destroy_ws_client(client);
            return;
        }
        client->last_frame = ws_frame;
    }
    // some code to do some math and figure out the payload that we got and the bytes that we read.
    if(client->data_read < get_actual_pay_len(client->last_frame))
    {
        client->more_to_read = 1;
        return;
    }
    client->more_to_read = 0;
    char msg[] = "Hello!";
    // handle the PAY_CONTN and INCL_DATA;
    switch(ws_frame->opcode)
    {
        case PAY_CONTN:
            break;
        case INCL_UTF8:
            send_ws_frame(client, INCL_UTF8, msg, 6);
            break;
        case INCL_DATA:
            break;
        case TERMINATE:
            if(client->expect_close)
            {
                destroy_ws_client(client);
                break;
            }
            // printf("client close initiated\n");
            client->state = CL_CLOSED;
            handle_close(client, get_actual_payload(ws_frame), get_actual_pay_len(ws_frame));
            destroy_ws_client(client);
            return;
        case PING:
            // printf("client send ping\n");
            handle_ping(client, get_actual_payload(ws_frame), get_actual_pay_len(ws_frame));
            break;
        case PONG:
            client->pong = 1;
            client->expect_pong = 0;
            break;
    }
    // Delete the payload etc here.
    destroy_ws_frame(ws_frame);
}

char* ws_create_accept_hdr(const char* ws_key, const size_t len)
{
    if (len == 0)
        return NULL;
    // Some code to create the Sec-WebSocket-Accept header depending on the clients Sec-WebSocket-Key.
    /* @TODO: Error handling for the SHA1. */
    unsigned char sha1[SHA_DIGEST_LENGTH+1];
    SHA_CTX context;
    SHA1_Init(&context);
    SHA1_Update(&context, ws_key, len);
    SHA1_Update(&context, GUID, GUID_LEN);
    SHA1_Final(sha1, &context);
    return b64_encode(sha1, SHA_DIGEST_LENGTH);
}

char* handshake_srvr_reply(int success, const char* ws_key_hdr, const char* status_line, const char* other_hdrs)
{
    char* reply = (char* ) malloc(sizeof(char) * (MAX_REPLY_SIZE+1));
    if(success == SUCCESS_STATUS)
        strcpy(reply, WS_SUCCESS_RESPONSE);
    else if(success == CUSTOM_STATUS)
        strncpy(reply, status_line, 50);
    else
        strcpy(reply, WS_FAILURE_RESPONSE);
    strcat(reply, VALID_WS_RESPONSE);
    strcat(reply, "Sec-WebSocket-Accept: ");
    char *temp = ws_create_accept_hdr(ws_key_hdr, strnlen(ws_key_hdr, 64));
    strcat(reply, temp);
    strcat(reply, "\r\n\r\n");
    if(other_hdrs != NULL)
        strncat(reply, other_hdrs, MAX_REPLY_SIZE-strlen(reply));
    return reply;
}

// if you want the server to be as fast as possible remove the masking of the payload data.
// @NOTE: this wont allocate memory for the payload, use free(ws_frame) after getting a frame from this function and the
//        memory management of the payload should be handled by the caller.
ws_frame_t* construct_ws_frame(uint8_t opcode, void* payload, uint64_t payload_len)
{
    // This is a check to return if the payload len is much biggger than we expected.
    if(payload_len > 0xfffffff)
        return NULL;
    ws_frame_t* ws_frame = malloc(sizeof(ws_frame_t));
    ws_frame->mask = 1;
    ws_frame->rsv1 = ws_frame->rsv2 = ws_frame->rsv3 = 0;
    ws_frame->opcode = opcode;
    uint32_t masking_key = get_rand_maskingkey();
    unmask_data(masking_key, payload, payload_len); // unmasking and masking does the same thing. XOR some value.
    if(payload_len > 0xffff)
    {
        ws_frame->payload_len = 127;
        ws_frame->inner.op3.payload_len = 0x7fffffffffffffff & payload_len; // to make the most significat byte zero.
        ws_frame->inner.op3.masking_key = masking_key;
        ws_frame->inner.op3.payload = payload;
    }
    else if(payload_len < 126)
    {
        ws_frame->payload_len = payload_len;
        ws_frame->inner.op1.masking_key = masking_key;
        ws_frame->inner.op1.payload = payload;
    }
    else
    {
        ws_frame->payload_len = 126;
        ws_frame->inner.op2.payload_len = payload_len;
        ws_frame->inner.op2.masking_key = masking_key;
        ws_frame->inner.op2.payload = payload;
    }
    return ws_frame;
}

bool send_ws_frame(ws_client_t* client, uint8_t opcode, void* payload, size_t payload_len)
{
    ws_frame_t* ws_frame = construct_ws_frame(opcode, (void*)payload, payload_len);
    // if the payload_len is 0 when the opcode is BINARY DATA or UTF8 DATA it does nothing.
    if (!ws_frame)
        return false;
    if((opcode == INCL_DATA || opcode == INCL_UTF8) && !payload_len)
    {
        free(ws_frame);
        return false;
    }
    bool ret;
    uint8_t frame_size = get_len_ws_frame(ws_frame);
    void* to_send = malloc(frame_size + payload_len);
    memcpy(to_send, ws_frame, frame_size);
    memcpy(to_send + frame_size, payload, payload_len);
    ret = client_write(client->client_data, to_send, frame_size + payload_len);
    free(to_send);
    free(ws_frame);
    return ret;
}

void destroy_ws_frame(ws_frame_t* ws_frame)
{
    if(get_actual_pay_len(ws_frame))
        free(get_actual_payload(ws_frame));
    free(ws_frame);
}

// handles ping by sending a pong opcode checks if the client is valid otherwise it just ignores :dab:.
// MAX_PAYLOAD length for PING and a PONG is 125: https://developer.mozilla.org/en-US/docs/Web/API/WebSockets_API/Writing_WebSocket_servers.
void handle_ping(ws_client_t* client, void* payload, uint64_t payload_len)
{
    if(payload_len > 125)
        return;
    if(client->state != CL_OPEN)
        return;
    send_ws_frame(client, PONG, payload, payload_len);
}

// send a ping when u get bored or something xd.
void send_ping(ws_client_t* client, void* payload, uint64_t payload_len)
{
    client->expect_pong = 1;
    send_ws_frame(client, PING, payload, payload_len);
}

bool valid_client_frame(ws_frame_t* frame)
{
    if(!frame->mask)
        return false;
    return true;
}

// more stuff here to do the status code etc and the expect close.
void handle_close(ws_client_t* client, void* payload, uint64_t payload_len)
{
    send_ws_frame(client, TERMINATE, payload, payload_len);
}

// gives back the resource allocated for the ws client.
void destroy_ws_client(ws_client_t* client)
{
    free(client->endpoint);
    free(client->method);
    if(client->last_frame != NULL)
        destroy_ws_frame(client->last_frame);
    close_io_client(client->client_data);
    free(client);
}

uint32_t get_rand_maskingkey()
{
    uint32_t ret;
    if(getrandom(&ret, sizeof(uint32_t), 0) < 0)
        return 0xdeadbeef;
    return ret;
}

void unmask_data(uint32_t masking_key, unsigned char* masked, uint64_t payload_len)
{
    uint8_t mask[4];
    mask[0] = (masking_key >> 24) & 0xff;
    mask[1] = (masking_key >> 16) & 0xff;
    mask[2] = (masking_key >> 8) & 0xff;
    mask[3] = masking_key & 0xff;    
    for(uint64_t i = 0; i < payload_len; i++)
    {
        masked[i] ^= mask[i % 4];
    }
}

void* memdup(const void* src, size_t len)
{
    void* memory = malloc(len);
    memcpy(memory, src, len);
    return memory;
}