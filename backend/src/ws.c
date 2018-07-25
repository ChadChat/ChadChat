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
    if(*other_hdrs != '\0')
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
// @NOTE: there's a bug in here where the mask struct is going to fuck everthing up.
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
    if(client->fragmented_frame != NULL)
        destroy_ws_frame(client->fragmented_frame);
    if(client->method != NULL)
        free(client->method);
    if(client->uri != NULL)
        free(client->uri);
    free(client);
}

inline void close_client(ws_client_t* client)
{
    close_io_client(client->client_data);
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