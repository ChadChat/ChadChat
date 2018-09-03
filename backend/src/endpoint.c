#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>  // For the htons
#include <stdbool.h>
#include "endpoint.h"
#include "ws_parser.h"
#include "ws.h"
#include "io.h"

// Useful Status code description is defined here
static const char STATUS_101[] = "Switching Protocols\r\n";
static const char STATUS_301[] = "Moved Permanently\r\n";
static const char STATUS_304[] = "Not Modified\r\n";
static const char STATUS_400[] = "Bad Request\r\n";
static const char STATUS_401[] = "Unauthorized\r\n";
static const char STATUS_403[] = "Forbidden\r\n";
static const char STATUS_404[] = "Not Found\r\n";
static const char STATUS_410[] = "Wrong Username\r\n";

static const char VALID_WS_RESPONSE[] = "Upgrade: websocket\r\nConnection: Upgrade\r\n";
static const char WS_FAILURE_RESPONSE[] = "HTTP/1.1 400 Bad Request\r\n";

#define STRLEN_WS_FAIL  26
#define STRLEN_VALID_WS 41

// @TODO: we need some kind of internal value to keep track of the last time the client interacted
//        If it's too long then send a ping and if we dont get the pong back in some time then close the client.
// @TODO: handle fragmentation in send_ws_frame if the payload size is greater than some value split it up and send as a fragmented message.

endp* init_endpoint(write_cb cb)
{
    endp* ep = malloc(sizeof(endp));
    ep->cb_funcs = hmap_init(50, destroy_member);
    ep->write_to = cb;
    return ep;
}

void register_endpoint(endp* ep, const char* url, char type, const char* method, endpoint_cb cb_func)
{
    cb_list* member = malloc(sizeof(cb_list));
    member->cb = cb_func;
    member->method = strdup(method);
    member->data_type = type;
    member->next = NULL;

    if(!hmap_exists(ep->cb_funcs, url))
        hmap_insert(ep->cb_funcs, url, (void*) member);
    else
    {
        // adds it to the tail of the list.
        cb_list* list;
        hmap_get(ep->cb_funcs, url, (void**) &list);
        for(; list->next != NULL; list = list->next) {}
        list->next = member;
    }
}

void destroy_endpoint(endp* ep)
{
    hmap_delete(ep->cb_funcs);
    free(ep);
}

// This function will be called by the hashmap to destroy the linked list data that we put in it.
void destroy_member(void* member)
{
    if (member == NULL)
        return;
    cb_list* mem = (cb_list*) member;
    free(mem->method);
    destroy_member((void*) mem->next);
    free(mem);
    return;
}

// There's kind of a bug here where it just returns 1 function with even when other functions register for multiple data types.
static endpoint_cb get_endpoint_cb(const endp* ep, const char* url, const char* method, char data_type)
{
    if(!hmap_exists(ep->cb_funcs, url))
        return NULL;
    cb_list* list;
    hmap_get(ep->cb_funcs, url, (void**)&list);

    // i used a binary trick to check if they call back function takes in the data type it gave, consult me if you are still confused.
    for(;list != NULL; list = list->next)
        if(!strcmp(method, list->method) && (list->data_type & data_type))
            return list->cb;
    return NULL;
}

void send_response(const endp* ep, ws_client_t* client, response* res)
{
    if(res->res_type == HNDL_HSHAKE)
    {
        char status_line[50];
        char other_hdrs[MAX_REPLY_SIZE-300];
        char status_code[7];
        strcpy(status_line, "HTTP/1.1 ");
        sprintf(status_code, "%d ", res->res_data.hshake.status);
        strcat(status_line, status_code);
        switch(res->res_data.hshake.status)
        {
            case SUCCESS:
                strcat(status_line, STATUS_101);
                break;
            case MOVED_PERM:
                strcat(status_line, STATUS_301);
                break;
            case BAD_REQUEST:
                strcat(status_line, STATUS_400);
                break;
            case UNAUTHORIZED:
                strcat(status_line, STATUS_401);
                break;
            case FORBIDDEN:
                strcat(status_line, STATUS_403);
                break;
            case NOT_FOUND:
                strcat(status_line, STATUS_404);
                break;
            case USERNAME_NOT_AVAIL:
                strcat(status_line, STATUS_410);
            case NOT_MODIFIED:
            default:
                strcat(status_line, STATUS_304);

        }
        if(res->res_data.hshake.headers != NULL)
            map_to_str(res->res_data.hshake.headers, other_hdrs);
        else
            other_hdrs[0] = 0;
        char* reply = handshake_srvr_reply(res->res_data.hshake.accept, status_line, other_hdrs);
        ep->write_to(client->client_data, reply, strlen(reply));
        free(reply);
    }
    else if(res->res_type == HNDL_FRAME)
    {
        if(res->res_data.frame.is_utf8)
            send_ws_frame(ep, client, INCL_UTF8, res->res_data.frame.data, res->res_data.frame.data_len);
        else
            send_ws_frame(ep, client, INCL_DATA, res->res_data.frame.data, res->res_data.frame.data_len);
    }

    if(res->close_client)
    {
        client->state = CL_CLOSED;
        destroy_ws_client(client);
    }
}

void send_ws_frame(const endp* ep, ws_client_t* client, uint8_t opcode, void* payload, size_t payload_len)
{
    ws_frame_t* ws_frame = construct_ws_frame(opcode, (void*)payload, payload_len, true, 0);
    // if the payload_len is 0 when the opcode is BINARY DATA or UTF8 DATA it does nothing.
    if (!ws_frame)
        return;
    if((opcode == INCL_DATA || opcode == INCL_UTF8) && !payload_len)
    {
        free(ws_frame);
        return;
    }
    uint8_t frame_size = get_len_ws_frame(ws_frame);
    void* to_send = malloc(frame_size + payload_len);
    memcpy(to_send, ws_frame, frame_size);
    memcpy(to_send + frame_size, payload, payload_len);
    ep->write_to(client->client_data, to_send, frame_size + payload_len);
    free(to_send);
    free(ws_frame);
}

// handle the close correctly otherwise freeing already free memory can have unpredictable effects.
void handle_data(const endp* ep, ws_client_t* client, const char* data, size_t len)
{
    switch(client->state)
    {
        case CL_HANDSHAKE:
            handle_handshake(ep, client, data);
            break;
        case CL_OPEN:
            handle_open(ep, client, data, len);
            break;
        case CL_CLOSED:
        default:
            // some code to close and give back the resource allocated.
            close_client(client);
            destroy_ws_client(client);
    }
}

void handle_handshake(const endp* ep, ws_client_t* client, const char* data)
{
    ws_handshake* hshake = parse_handshake(data);
    if(strcmp(hshake->http_version, "HTTP/1.1") != 0)
    {
        char* resp = (char*) malloc(STRLEN_WS_FAIL+STRLEN_VALID_WS+10);
        strcat(resp, WS_FAILURE_RESPONSE);
        strcat(resp, VALID_WS_RESPONSE);
        strcat(resp, "\r\n");
        ep->write_to(client->client_data, resp, STRLEN_WS_FAIL+STRLEN_VALID_WS);
        client->state = CL_CLOSED;
        free(resp);
        destroy_handshake(hshake);
        return;
    }
    endpoint_handshake(ep, client, hshake);
    destroy_handshake(hshake);
}

void handle_open(const endp *ep, ws_client_t* client, const void* data, size_t len)
{
    ws_frame_t* frame;
    if((frame = client->last_frame) == NULL)
    {
        if((frame = parse_frame(data, len, &(client->data_read))) == NULL)
        {
            // this means that the data was much bigger than we expeceted. So just delete this cunt without sending the TERMINATE message.
            client->state = CL_CLOSED;
            close_client(client);
            destroy_ws_client(client);
            return;
        }
    }
    else
    {
        void* write_to = get_actual_payload(frame) + client->data_read;
        memcpy(write_to, data, len);
        client->data_read += len;
        client->last_frame = NULL;
    }

    if(len == READ_BUFFER_SIZE && client->data_read != get_actual_pay_len(frame)) // we have more to read.
    {
        return;
    }
    endpoint_data_frame(ep, client, &frame);
    destroy_ws_frame(frame);
}

void endpoint_handshake(const endp* ep, ws_client_t* client, const ws_handshake* hshake)
{
    endpoint_cb cb = get_endpoint_cb(ep, hshake->resource_name, hshake->method, ALL_TYPE);
    if(cb == NULL)
    {
        // send a resource not found error (404)
        send_not_found(ep, client);
        return;
    }

    // free the resource, if any, we allocated previously.
    if(client->method != NULL)
        free(client->method);
    if(client->uri != NULL)
        free(client->uri);
    
    client->method = strdup(hshake->method);
    client->uri = strdup(hshake->resource_name);
    request* req = malloc(sizeof(request));
    req->req_type = HNDL_HSHAKE;
    req->req_data.hshake.headers = hshake->headers;
    req->req_data.hshake.data = hshake->data;
    endp_client* ep_client = (endp_client *)client->id;
    response* res = cb(req, ALL_TYPE, ep_client);
    res->res_data.hshake.accept = malloc(WS_KEY_LEN+12);
    sm_get(req->req_data.hshake.headers, "sec-websocket-key", res->res_data.hshake.accept, WS_KEY_LEN+11);
    if(!res->close_client)
        client->state = CL_OPEN;
    send_response(ep, client, res);
    destroy_request(req);
    destroy_response(res);
}

// ************* handle the ping, pong using some kind of scheduler to notify of our state.
// Average IQ of 1000 to read this function
void endpoint_data_frame(const endp* ep, ws_client_t* client, ws_frame_t** frame_mem)
{
    ws_frame_t* frame = *frame_mem;
    if(!frame->fin)
    {
        // Here we handle the Websocket payload fragmentation in the WS spec level.
        // make sure to unmask the frame as you go along.
        if(frame->opcode != PAY_CONTN)
        {
            // This means that this is first frame when they send a fragmented frame.
            if((client->fragmented_frame = cpy_ws_frame(frame)) == NULL)
                return; // This is bad.
        }
        else  // This means that we have a payload that's been fragmented into more than or equal 3 So copy this payload into our old frame.
            expand_payload_ws_frame(client->fragmented_frame, get_actual_payload(frame), get_actual_pay_len(frame));
        return;
    }

    // NOTE: since we need a way to destroy this frame and the unfragmented frame without causing a memory leak, i did the **frame_mem cancer.
    if(frame->opcode == PAY_CONTN)
    {
        // This means that this is last frame send by the client of a fragmented message.
        expand_payload_ws_frame(client->fragmented_frame, get_actual_payload(frame), get_actual_pay_len(frame));
        destroy_ws_frame(frame);        
        *frame_mem = client->fragmented_frame;
        frame = *frame_mem;
        client->fragmented_frame = NULL;
    }
    endpoint_cb cb;
    switch(frame->opcode)
    {
        case INCL_UTF8:
            cb = get_endpoint_cb(ep, client->uri, client->method, UTF8_TYPE);
            if (cb == NULL)
                break;  // ********** Do some code here *************
            handle_ws_req(ep, client, frame, UTF8_TYPE, cb);
            break;
        case INCL_DATA:
            cb = get_endpoint_cb(ep, client->uri, client->method, BIN_TYPE);
            if (cb == NULL)
                break;  // ********** Do some code here *************
            handle_ws_req(ep, client, frame, BIN_TYPE, cb);
            break;
        case TERMINATE:
            if(client->expect_close)
            {
                close_client(client);
                destroy_ws_client(client);
                break;
            }
            client->state = CL_CLOSED;
            send_ws_frame(ep, client, TERMINATE, get_actual_payload(frame), get_actual_pay_len(frame));
            // close_client(client); SOMETIMES i think the io.c closes the client automatically Valgrind was showing some stuff.
            destroy_ws_client(client);
            break;
        case PING:
            if((time(NULL) - client->last_ping) > PING_TIMEOUT) // Only processes the Ping if the last ping is longer than the timeout.
            {
                size_t pay_len = get_actual_pay_len(frame);
                // MAX_PAYLOAD length for PING and a PONG is 125: https://developer.mozilla.org/en-US/docs/Web/API/WebSockets_API/Writing_WebSocket_servers.
                if(pay_len > 125)
                    break;
                send_ws_frame(ep, client, PONG, get_actual_payload(frame), pay_len);
                // Update the client data to check the last time we replied
                client->last_ping = time(NULL);
            }
            break;
        case PONG:
            client->pong = 1;
            break;
    }

}

inline void handle_ws_req(const endp* ep, ws_client_t* client, const ws_frame_t* frame, const char data_type, endpoint_cb cb)
{
    request* req = malloc(sizeof(request));
    req->req_type = HNDL_FRAME;
    req->req_data.frame.data = get_actual_payload(frame);
    req->req_data.frame.data_len = get_actual_pay_len(frame);
    endp_client* ep_client = (endp_client *)client->id;
    response* res = cb(req, data_type, ep_client);
    send_response(ep, client, res);
    destroy_request(req);
    destroy_response(res);
}

inline void send_ping(const endp* ep, ws_client_t* client, void* payload, size_t pay_len)
{
    send_ws_frame(ep, client, PING, payload, pay_len);
}


// These are the closing status code defined in ws.h
void send_close(const endp* ep, ws_client_t* client, uint16_t status_code, const void* reason, size_t reason_len)
{
    size_t pay_len = sizeof(uint16_t) + reason_len;
    void* payload = malloc(pay_len);
    status_code = htons(status_code);
    memcpy(payload, (void*) &status_code, sizeof(uint16_t));
    memcpy(payload + sizeof(uint16_t), reason, reason_len);
    send_ws_frame(ep, client, TERMINATE, payload, pay_len);
    client->expect_close = 1;
}

void send_not_found(const endp* ep, ws_client_t* client)
{
    response* resp = malloc(sizeof(response));
    resp->res_type = HNDL_HSHAKE;
    resp->res_data.hshake.status = NOT_FOUND;
    resp->res_data.hshake.headers = NULL;
    resp->res_data.hshake.accept = strdup("Nope");
    resp->close_client = true;
    send_response(ep, client, resp);
    destroy_response(resp);
}

inline void destroy_request(request* req)
{
    /*
    if(req->req_type == HNDL_FRAME)
        free(req->req_data.frame.data);
    */
    free(req);
}

void destroy_response(response* resp)
{
    if(resp->res_type == HNDL_HSHAKE)
    {
        free(resp->res_data.hshake.accept);
        if(resp->res_data.hshake.headers != NULL)
            sm_delete(resp->res_data.hshake.headers);
    }
    else if(resp->res_data.frame.data_len)
            free(resp->res_data.frame.data);
    free(resp);
}
