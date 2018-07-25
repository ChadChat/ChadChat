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

// Note: the GET / HTTP/1.1 will not be parsed by this
// After the call saveptr will point to end of the headers so you can parse any post data that's been sent to the server. 
StrMap* parse_headers(char* request, char** saveptr)
{
    StrMap* sm = sm_new(30);
    char* next = request;
    for(;next != NULL;next = strtok_r(NULL, "\r\n", saveptr))
    {
        trim(next);
        if(next == NULL)
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
        sm_put(sm, next, value);
    }
    *saveptr = next+2;
    return sm;
}

// Note: we terminate the resource name with NULL but there will still be data after, idk if it's a good practice
void parse_get_data(ws_handshake* hshake)
{
    char *params;
    if((params = strchr(hshake->resource_name, '?')) == NULL)
    {
        hshake->data = NULL;
        return;
    }
    *params++ = 0;  // we set the pointer to '?' now the string resource_name is the uri the user requested
    parse_http_data(hshake, params);
}

void parse_http_data(ws_handshake* hshake, char* data)
{
    if(*data == 0)
    {
        hshake->data = NULL;
        return;
    }
    StrMap* sm = sm_new(20);
    char *saveptr, *next;
    bool first = true;
    for(next = strtok_r(data, "&", &saveptr); first || next != NULL; next = strtok_r(NULL, "&", &saveptr))
    {
        char* value = strchr(next, '=');
        if(value == NULL)
            break;
        *value++ = 0;
        sm_put(sm, next, value);
        first = false;
    }
        
    if(sm_get_count(sm))
    {
        hshake->data = sm;
        return;
    }
    hshake->data = NULL;
    sm_delete(sm);
}

// Thread-Safe (have to check if parse_headers is thread safe)
ws_handshake* parse_handshake(const char* req)
{
    char request[2048];
    strlcpy(request, req, 2047);
    char* saveptr;
    ws_handshake* hshake = malloc(sizeof(ws_handshake));
    char* begin = strtok_r(request, "\r\n", &saveptr);
    char* mtd_end = strchr(request, ' ');
    *(mtd_end++) = 0;
    hshake->method = strndup(request, 6);
    to_str_upper(hshake->method);
    char* ver_beg = strrchr(mtd_end, ' ');
    *(ver_beg++) = 0;
    hshake->http_version = strndup(ver_beg, 12);
    hshake->resource_name = strndup(mtd_end, 256);
    begin = strtok_r(NULL, "\r\n", &saveptr);
    hshake->headers = parse_headers(begin, &saveptr);
    if(!strcmp("GET", hshake->method))
        parse_get_data(hshake);
    else
        parse_http_data(hshake, saveptr);
    return hshake;
}

ws_frame_t* parse_frame(const void* data, size_t len, size_t* payload_read)
{
    ws_frame_t* ws_frame = malloc(sizeof(ws_frame_t));
    *payload_read = 0;
    memcpy(ws_frame, data, sizeof(ws_frame_t));
    uint64_t actual_pay_len = get_actual_pay_len(ws_frame);
    if(!ws_frame->mask || actual_pay_len > 5000000)   // i dont want a single frame bigger than this. 5 MB is enough.
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
        ws_frame->inner.op1.payload = malloc(actual_pay_len);
        memcpy(ws_frame->inner.op1.payload, data + WS_FRAME_SIZE_OP1, *payload_read);
    }
    else if(ws_frame->payload_len == 126)
    {
        ws_frame->inner.op2.payload = malloc(actual_pay_len);
        memcpy(ws_frame->inner.op2.payload, data + WS_FRAME_SIZE_OP2, *payload_read);
    }
    else
    {
        ws_frame->inner.op3.payload = malloc(actual_pay_len);
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

inline uint64_t get_data_read(ws_frame_t* ws_frame, size_t len_total)
{
    uint64_t len = len_total - get_len_ws_frame(ws_frame);
    len = len > get_actual_pay_len(ws_frame) ? get_actual_pay_len(ws_frame) : len;
    return len;
}

inline uint64_t get_actual_pay_len(const ws_frame_t* ws_frame)
{
    if(ws_frame->payload_len < 126)
        return ws_frame->payload_len;
    else if(ws_frame->payload_len == 126)
        return ws_frame->inner.op2.payload_len;
    else
        return ws_frame->inner.op3.payload_len;
}

inline uint8_t get_len_ws_frame(const ws_frame_t* ws_frame)
{
    if(ws_frame->payload_len < 126)
        return WS_FRAME_SIZE_OP1;
    else if(ws_frame->payload_len == 126)
        return WS_FRAME_SIZE_OP2;
    else
        return WS_FRAME_SIZE_OP3;
}

inline uint32_t get_masking_key(const ws_frame_t* ws_frame)
{
    if(ws_frame->payload_len < 126)
        return ws_frame->inner.op1.masking_key;
    else if(ws_frame->payload_len == 126)
        return ws_frame->inner.op2.masking_key;
    else
        return ws_frame->inner.op3.masking_key;
}

inline void* get_actual_payload(const ws_frame_t* ws_frame)
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
    if(hshake->data != NULL)
        sm_delete(hshake->data);
    free(hshake);
}

// a callback function to be used from the sm_enum.
void append_key_val(const char* key, const char* value, char* dest)
{
    strcat(dest, key);
    strcat(dest, ": ");
    strcat(dest, value);
    strcat(dest, "\r\n");
}

// a useful function to fill a buffer with http headers given a strmap.
void map_to_str(StrMap* sm, char* buf)
{
    *buf = 0;
    sm_enum(sm, append_key_val, buf);
    strcat(buf, "\r\n");
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

/*	$OpenBSD: strlcpy.c,v 1.11 2006/05/05 15:27:38 millert Exp $	*/

/*
 * Copyright (c) 1998 Todd C. Miller <Todd.Miller@courtesan.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Copy src to string dst of size siz.  At most siz-1 characters
 * will be copied.  Always NUL terminates (unless siz == 0).
 * Returns strlen(src); if retval >= siz, truncation occurred.
 */
size_t
strlcpy(char *dst, const char *src, size_t siz)
{
	char *d = dst;
	const char *s = src;
	size_t n = siz;

	/* Copy as many bytes as will fit */
	if (n != 0) {
		while (--n != 0) {
			if ((*d++ = *s++) == '\0')
				break;
		}
	}

	/* Not enough room in dst, add NUL and traverse rest of src */
	if (n == 0) {
		if (siz != 0)
			*d = '\0';		/* NUL-terminate dst */
		while (*s++)
			;
	}

	return(s - src - 1);	/* count does not include NUL */
}
 /*      $OpenBSD: strlcat.c,v 1.2 1999/06/17 16:28:58 millert Exp $     */
 
 /*-
  * SPDX-License-Identifier: BSD-3-Clause
  *
  * Copyright (c) 1998 Todd C. Miller <Todd.Miller@courtesan.com>
  * All rights reserved.
  *
  * Redistribution and use in source and binary forms, with or without
  * modification, are permitted provided that the following conditions
  * are met:
  * 1. Redistributions of source code must retain the above copyright
  *    notice, this list of conditions and the following disclaimer.
  * 2. Redistributions in binary form must reproduce the above copyright
  *    notice, this list of conditions and the following disclaimer in the
  *    documentation and/or other materials provided with the distribution.
  * 3. The name of the author may not be used to endorse or promote products
  *    derived from this software without specific prior written permission.
  *
  * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
  * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
  * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
  * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
  * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
  * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
  * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
  * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
  * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
  * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  */
 
 /*
  * Appends src to string dst of size siz (unlike strncat, siz is the
  * full size of dst, not space left).  At most siz-1 characters
  * will be copied.  Always NUL terminates (unless siz <= strlen(dst)).
  * Returns strlen(src) + MIN(siz, strlen(initial dst)).
  * If retval >= siz, truncation occurred.
  */
 size_t
 strlcat(char *dst, const char *src, size_t siz)
 {
         char *d = dst;
         const char *s = src;
         size_t n = siz;
         size_t dlen;
 
         /* Find the end of dst and adjust bytes left but don't go past end */
         while (n-- != 0 && *d != '\0')
                 d++;
         dlen = d - dst;
         n = siz - dlen;
 
         if (n == 0)
                 return(dlen + strlen(s));
         while (*s != '\0') {
                 if (n != 1) {
                         *d++ = *s;
                         n--;
                 }
                 s++;
         }
         *d = '\0';
 
         return(dlen + (s - src));       /* count does not include NUL */
 }