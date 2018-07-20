#include "ws.h"
#include "ws_parser.h"
#include "io.h"
#include "server.h"
#include "utils/strmap.h"
#include "endpoint.h"
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include "utils/b64.h"
#include <openssl/sha.h>
#include <sys/random.h>

static const char GUID[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
static const char VALID_WS_RESPONSE[] = "Upgrade: websocket\r\nConnection: Upgrade\r\n";

/*
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
    free(ws_key);
    free(resp);
    destroy_handshake(hshake);
}
*/

/*
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
                return;
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
*/

char* ws_create_accept_hdr(const char* ws_key, const size_t len)
{
    if (len == 0)
        return NULL;
    // Some code to create the Sec-WebSocket-Accept header depending on the clients Sec-WebSocket-Key.
    unsigned char sha1[SHA_DIGEST_LENGTH+1];
    SHA_CTX context;
    SHA1_Init(&context);
    SHA1_Update(&context, ws_key, len);
    SHA1_Update(&context, GUID, GUID_LEN);
    SHA1_Final(sha1, &context);
    return b64_encode(sha1, SHA_DIGEST_LENGTH);
}

char* handshake_srvr_reply(const char* ws_key_hdr, const char* status_line, const char* other_hdrs)
{
    char* reply = (char* ) malloc(sizeof(char) * (MAX_REPLY_SIZE+1));
    strlcpy(reply, status_line, 70);
    strcat(reply, VALID_WS_RESPONSE);
    strcat(reply, "Sec-WebSocket-Accept: ");
    char *temp = ws_create_accept_hdr(ws_key_hdr, strnlen(ws_key_hdr, 50));
    strcat(reply, temp);
    if(other_hdrs != NULL)
    {
        strcat(reply, "\r\n");
        strlcat(reply, other_hdrs, MAX_REPLY_SIZE-strlen(reply));
    }
    strcat(reply, "\r\n\r\n");
    free(temp);
    return reply;
}

// if you want the server to be as fast as possible remove the masking of the payload data.
// @NOTE: this wont allocate memory for the payload, use free(ws_frame) after getting a frame from this function and the
//        memory management of the payload should be handled by the caller.
ws_frame_t* construct_ws_frame(uint8_t opcode, void* payload, uint64_t payload_len, bool mask_data, uint32_t mask_val)
{
    // This is a check to return if the payload len is much biggger than we expected.
    if(payload_len > 0xfffffff)
        return NULL;
    ws_frame_t* ws_frame = malloc(sizeof(ws_frame_t));
    ws_frame->fin = 1;
    if(mask_data)
    {
        ws_frame->mask = 1;
        if(!mask_val)
            mask_val = get_rand_maskingkey();
        unmask_data(mask_val, payload, payload_len); // unmasking and masking does the same thing. XOR some value.
    }
    ws_frame->rsv1 = ws_frame->rsv2 = ws_frame->rsv3 = 0;
    ws_frame->opcode = opcode & 0x0f;
    if(payload_len > 0xffff)
    {
        ws_frame->payload_len = 127;
        ws_frame->inner.op3.payload_len = 0x7fffffffffffffff & payload_len; // to make the most significat byte zero.
        ws_frame->inner.op3.masking_key = mask_val;
        ws_frame->inner.op3.payload = payload;
    }
    else if(payload_len < 126)
    {
        ws_frame->payload_len = payload_len;
        ws_frame->inner.op1.masking_key = mask_val;
        ws_frame->inner.op1.payload = payload;
    }
    else
    {
        ws_frame->payload_len = 126;
        ws_frame->inner.op2.payload_len = payload_len;
        ws_frame->inner.op2.masking_key = mask_val;
        ws_frame->inner.op2.payload = payload;
    }
    return ws_frame;
}

// NOTE: This wont fully copy the frame because this wont mask the data.
ws_frame_t* cpy_ws_frame(const ws_frame_t* to_cpy)
{
    size_t len = get_actual_pay_len(to_cpy);
    void* payload = memdup(get_actual_payload(to_cpy), len);
    if(payload == NULL)
        return NULL;
    return construct_ws_frame(to_cpy->opcode, payload, len, false, 0);
}

// This function is hella messy.
bool expand_payload_ws_frame(ws_frame_t* cur_frame, const void* new_payload, size_t len)
{
    size_t old_len;
    size_t new_len; 
    void *new_mem, *old_mem;
    if(cur_frame->payload_len <= 125)
    {
        old_len = cur_frame->payload_len;
        new_len = len + old_len;
        if(new_len > 16000000)    // we dont want a frame greater than 16 mb.
            return false;
        old_mem = cur_frame->inner.op1.payload;
        new_mem = realloc(old_mem, new_len);
        if(new_mem == NULL)
            return false;
        cur_frame->inner.op1.payload = new_mem;
        memcpy(new_mem+old_len, new_payload, len);
        // Do other stuff to change the payload_len in the second option or the third option
        if(new_len > 0xffff)
        {
            cur_frame->payload_len = 127;
            cur_frame->inner.op3.payload_len = new_len;
            cur_frame->inner.op3.payload = new_mem;
        }
        else if(new_len > 125)
        {
            cur_frame->payload_len = 126;
            cur_frame->inner.op2.payload_len = new_len;
            cur_frame->inner.op2.payload = new_mem;
        }
        else
            cur_frame->payload_len = new_len;

    }
    else if(cur_frame->payload_len == 126)
    {
        old_len = cur_frame->inner.op2.payload_len;
        new_len = len + old_len;
        if(new_len > 16000000)    // we dont want a frame greater than 16 mb.
            return false;
        old_mem = cur_frame->inner.op2.payload;
        new_mem = realloc(old_mem, new_len);
        if(new_mem == NULL)
            return false;
        cur_frame->inner.op2.payload = new_mem;
        memcpy(new_mem+old_len, new_payload, len);
        // Do other stuff to change the payload_len in the second option or the third option
        if(new_len > 0xffff)
        {
            cur_frame->payload_len = 127;
            cur_frame->inner.op3.payload_len = new_len;
            cur_frame->inner.op3.payload = new_mem;
        }
        else
            cur_frame->inner.op2.payload_len = new_len;
    }
    else
    {
        old_len = cur_frame->inner.op3.payload_len;
        new_len = len + old_len;
        if(new_len > 16000000)    // we dont want a frame greater than 16 mb.
            return false;
        old_mem = cur_frame->inner.op3.payload;
        new_mem = realloc(old_mem, new_len);
        if(new_mem == NULL)
            return false;
        cur_frame->inner.op3.payload = new_mem;
        memcpy(new_mem+old_len, new_payload, len);
        // No need to change the payload_len options because 16 Mb is far less than what a 8 byte memory can hold.
    }
    return true;
}

void destroy_ws_frame(ws_frame_t* ws_frame)
{
    if(get_actual_pay_len(ws_frame))
        free(get_actual_payload(ws_frame));
    free(ws_frame);
}

inline bool valid_client_frame(ws_frame_t* frame)
{
    return frame->mask;
}

// gives back the resource allocated for the ws client.
void destroy_ws_client(ws_client_t* client)
{
    if(client->last_frame != NULL)
        destroy_ws_frame(client->last_frame);
    if(client->method != NULL)
        free(client->method);
    if(client->uri != NULL)
        free(client->uri);
    close_io_client(client->client_data);
    free(client);
}

inline bool transmition_complete(ws_frame_t* frame)
{
    return frame->fin;
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

inline void* memdup(const void* src, size_t len)
{
    void* memory = malloc(len);
    if(memory == NULL)
        return NULL;
    memcpy(memory, src, len);
    return memory;
}