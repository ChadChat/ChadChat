#include "ws_parser.h"
#include "ws.h"
#include <stdbool.h>
#include <string.h>
#include <ctype.h>

#ifdef DEBUG_ON
#include <stdio.h>
#endif

#define MAX_FOR_VAL 128

#define WS_FRAME_SIZE_OP1 6
#define WS_FRAME_SIZE_OP2 8
#define WS_FRAME_SIZE_OP3 14

static const char WS_PROTOCOL_HDR[] = "chat";

// need to check if host header is valid.
// even though the subprotocol header was optional i added the check anyway.
// @TODO: modify the function to indicate the reason for invalidity by sending some different codes. so that the server can reply appropriately.
bool valid_ws_req(StrMap* sm)
{
    char* val = malloc(MAX_FOR_VAL * sizeof(char));
    if(sm_get(sm, "upgrade", val, MAX_FOR_VAL) && strcasecmp(val, "websocket") != 0)
        return false;
    if(sm_get(sm, "connection", val, MAX_FOR_VAL) && !contains_comma_sep(val, "upgrade"))
        return false;
    if(sm_get(sm, "sec-websocket-version", val, MAX_FOR_VAL) && strcasecmp(val, "13") != 0)
        return false;
    if(!sm_exists(sm, "host") || !sm_exists(sm, "sec-websocket-key"))
        return false;
    char temp[WS_KEY_LEN+11];
    sm_get(sm, "sec-websocket-key", temp, WS_KEY_LEN+10);
    if(!valid_ws_key(temp))
        return false;
    if(sm_exists(sm, "sec-websocket-protocol"))
    {
        sm_get(sm, "sec-websocket-protocol", val, MAX_FOR_VAL);
        if(!contains_comma_sep(val, WS_PROTOCOL_HDR))
            return false;
    }
    return true;
}

// will need to create some hash table for storing key:value pairs for the request headers
// Note: the GET / HTTP/1.1 will not be parsed by this
StrMap* parse_headers(char* request, char** saveptr)
{
    StrMap* sm = sm_new(30);
    char* next = request;
    for(;next != NULL;next = strtok_r(NULL, "\r\n", saveptr))
    {
        trim(next);
        if(*next == '\0')
            break;
        char* value = strchr(next, ':');    // Find ':'
        if(value == NULL)
            continue;
        *value = '\0';                      // Set the value at ':' to null, so that next will be key.
        value++;
        trim(value);
        if(strnlen(next, MAX_FOR_VAL) == MAX_FOR_VAL || strnlen(value, MAX_FOR_VAL) == MAX_FOR_VAL)
            continue;
        trim(next);
        to_str_lower(next);
        if(strcmp(next, "sec-websocket-key") != 0)
            to_str_lower(value);
        // printf("KEY: %s; VALUE: %s\n", next, value);
        sm_put(sm, next, value);
    }
    return sm;
}

// Thread-Safe (have to check if parse_headers is thread safe)
ws_handshake* parse_handshake(const char* req)
{
    char request[1024];
    strncpy(request, req, 1023);
    char* saveptr;
    ws_handshake* hshake = malloc(sizeof(ws_handshake));
    char* begin = strtok_r(request, "\r\n", &saveptr);
    char* mtd_end = strchr(request, ' ');
    *(mtd_end++) = 0;
    hshake->method = strndup(request, 6);
    char* ver_beg = strrchr(mtd_end, ' ');
    *(ver_beg++) = 0;
    hshake->http_version = strndup(ver_beg, 12);
    hshake->resource_name = strndup(mtd_end, 64);
    begin = strtok_r(NULL, "\r\n", &saveptr);
    hshake->headers = parse_headers(begin, &saveptr);
    return hshake;
}

ws_frame_t* parse_frame(const void* data, size_t len, size_t* payload_read)
{
    ws_frame_t* ws_frame = malloc(sizeof(ws_frame_t));
    *payload_read = 0;
    uint64_t actual_pay_len = get_actual_pay_len(ws_frame);
    memcpy(ws_frame, data, sizeof(ws_frame_t));  // Have to check for endianess.
    if(!ws_frame->mask || actual_pay_len > 160000000)   // i dont want a single frame bigger than this. 16 MB is enough.
    {
        free(ws_frame);
        return NULL;
    }

    *payload_read = get_data_read(ws_frame, len);

    if(!actual_pay_len)
    {
        return ws_frame;
    }

    if(ws_frame->payload_len < 126)
    {
        ws_frame->inner.op1.payload = malloc(ws_frame->payload_len);
        memcpy(ws_frame->inner.op1.payload, data + WS_FRAME_SIZE_OP1, *payload_read);
    }
    else if(ws_frame->payload_len == 126)
    {
        ws_frame->inner.op2.payload = malloc(ws_frame->inner.op2.payload_len);
        memcpy(ws_frame->inner.op2.payload, data + WS_FRAME_SIZE_OP2, *payload_read);
    }
    else
    {
        ws_frame->inner.op3.payload = malloc(ws_frame->inner.op3.payload_len);
        memcpy(ws_frame->inner.op3.payload, data + WS_FRAME_SIZE_OP3, *payload_read);
    }
    unmask_data(get_masking_key(ws_frame), get_actual_payload(ws_frame), *payload_read);
#ifdef DEBUG_ON
    printf("WS_FRAME:\n\tFIN: %d\tRESV1: %d\tRESV2: %d\tRESV3: %d\n"
            "\tOPCODE: %d\tMASK: %d\tPAYLOAD_LEN: %d\n", 
        ws_frame->fin, ws_frame->rsv1, ws_frame->rsv2, ws_frame->rsv3,
        ws_frame->opcode ,ws_frame->mask, ws_frame->payload_len);
    if(ws_frame->payload_len < 126)
        printf("\tMASKING KEY: %d\tDATA: %s\n", ws_frame->inner.op1.masking_key, ws_frame->inner.op1.payload);
    else if(ws_frame->payload_len == 126)
        printf("\tACTUAL PAYLOAD LEN: %d\tMASKING KEY: %d\tDATA: %s\n", ws_frame->inner.op2.payload_len,
        ws_frame->inner.op2.masking_key, ws_frame->inner.op2.payload);
    else
        printf("\tACTUAL PAYLOAD LEN: %ld\tMASKING KEY: %d\tDATA: %s\n", ws_frame->inner.op3.payload_len,
        ws_frame->inner.op3.masking_key, ws_frame->inner.op3.payload);
#endif
    return ws_frame;
}

uint64_t get_data_read(ws_frame_t* ws_frame, size_t len_total)
{
    uint64_t len = len_total - get_len_ws_frame(ws_frame);
    len = len > get_actual_pay_len(ws_frame) ? get_actual_pay_len(ws_frame) : len;
    return len;
}

uint64_t get_actual_pay_len(ws_frame_t* ws_frame)
{
    if(ws_frame->payload_len < 126)
        return ws_frame->payload_len;
    else if(ws_frame->payload_len == 126)
        return ws_frame->inner.op2.payload_len;
    else
        return ws_frame->inner.op3.payload_len;
}

uint8_t get_len_ws_frame(ws_frame_t* ws_frame)
{
    if(ws_frame->payload_len < 126)
        return WS_FRAME_SIZE_OP1;
    else if(ws_frame->payload_len == 126)
        return WS_FRAME_SIZE_OP2;
    else
        return WS_FRAME_SIZE_OP3;
}

uint32_t get_masking_key(ws_frame_t* ws_frame)
{
    if(ws_frame->payload_len < 126)
        return ws_frame->inner.op1.masking_key;
    else if(ws_frame->payload_len == 126)
        return ws_frame->inner.op2.masking_key;
    else
        return ws_frame->inner.op3.masking_key;
}

void* get_actual_payload(ws_frame_t* ws_frame)
{
    if(ws_frame->payload_len < 126)
        return ws_frame->inner.op1.payload;
    else if(ws_frame->payload_len == 126)
        return ws_frame->inner.op2.payload;
    else
        return ws_frame->inner.op3.payload;
}

// gives back the resource allocated for the ws_handshake to the OS.
// Thread-Safe.
void destroy_handshake(ws_handshake* hshake)
{
    free(hshake->method);
    free(hshake->resource_name);
    free(hshake->http_version);
    sm_delete(hshake->headers);
    free(hshake);
}

// Thread-Safe
// checks if a substring is found in a string that has comma separated members.
int contains_comma_sep(char* str, const char* sub_str)
{
    char* saveptr;
    char* next = strtok_r(str, ",", &saveptr);
    while(next != NULL)
    {
        if(strstr(next, sub_str) != NULL)
            return 1;
        next = strtok_r(NULL, "\r\n", &saveptr);
    }
    return 0;
}

int valid_ws_key(const char* key)
{
    if(!ValidBase64(key) || calcDecodeLength(key) != WS_KEY_LEN)
        return 0;
    return 1;
}

// Thread-Safe
void to_str_lower(char* string)
{
    for(;*string;++string)
        *string = tolower(*string);
}

// Thread-Safe
void to_str_upper(char* string)
{
    for(;*string;++string)
        *string = toupper(*string);
}


// Thread-Safe
void skip_whitespace(char **word)
{
    if(**word == '\0')
        return;
    for(;isspace(**word);(*word)++)
    {}
}

// Thread-Safe
void trim(char *str)
{
    int i;
    int begin = 0;
    int end = strlen(str) - 1;

    while (isspace((unsigned char) str[begin]))
        begin++;

    while ((end >= begin) && isspace((unsigned char) str[end]))
        end--;

    // Shift all characters back to the start of the string array.
    for (i = begin; i <= end; i++)
        str[i - begin] = str[i];

    str[i - begin] = '\0'; // Null terminate string.
}