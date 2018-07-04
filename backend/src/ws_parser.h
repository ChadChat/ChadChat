#ifndef WSPAR_IMP_CHAD
#define WSPAR_IMP_CHAD

#include <stdbool.h>

#include "utils/strmap.h"
#include "utils/b64.h"
#include "ws.h"

#define WS_KEY_LEN 16

typedef struct
{
    char* method;
    char* resource_name;
    char* http_version;
    StrMap* headers;
} ws_handshake;

bool valid_ws_req(StrMap* sm);
StrMap* parse_headers(char* request, char** saveptr);
ws_handshake* parse_handshake(const char* request);
ws_frame_t* parse_frame(const void* data, size_t len, size_t* payload_read);
uint64_t get_data_read(ws_frame_t* ws_frame, size_t len_total);
uint64_t get_actual_pay_len(ws_frame_t* ws_frame);
uint8_t get_len_ws_frame(ws_frame_t* ws_frame);
uint32_t get_masking_key(ws_frame_t* ws_frame);
void* get_actual_payload(ws_frame_t* ws_frame);
void destroy_handshake(ws_handshake* hshake);
int contains_comma_sep(char* str, const char* sub_str);
int valid_ws_key(const char* key);
void to_str_lower(char* string);
void to_str_upper(char* string);
void skip_whitespace(char **word);
void trim(char *str);

#endif