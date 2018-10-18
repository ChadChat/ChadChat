#include "register_client.h"
#include <string.h>
#include "../utils/list.h"
#include "../utils/dh.h"

// TODO: Make the client connected which is binded to the socket or something.
// TODO: When the client closes free all the resources allocated to it.
// TODO: Make the client'd id last only a hours before it gets destroyed.

extern hmap* globals;

response* get_register_client(const request* req, const char data_type, endp_client* ep_client)
{
    response* res = malloc(sizeof(response));
    LIST* clients;
    memset(res, 0, sizeof(response));
    if(req->req_type == HNDL_HSHAKE)
    {
        if((req->req_data.hshake.data == NULL) || !(sm_exists(req->req_data.hshake.data, "username")))
        {
            res->res_type = HNDL_HSHAKE;
            res->res_data.hshake.status = BAD_REQUEST;
            res->res_data.hshake.headers = NULL;
            res->close_client = true;
        }
        char* username = malloc(256);
        sm_get(req->req_data.hshake.data, "username", username, 255);
        hmap_get(globals, "clients", &clients);
        int user_index = LIST_find(clients, username, user_exists);  // Check if username exists
        if(user_index == -1)
        {
            res->res_type = HNDL_HSHAKE;
            res->res_data.hshake.status = USERNAME_NOT_AVAIL;
            res->res_data.hshake.headers = NULL;
            res->close_client = true;
            free(username);
            return res;
        }
        user_profile* user = malloc(sizeof(user_profile));
        user->username = username;
        user->key_bundle = NULL;
        user->cookie = NULL;
        user->status = USER_INIT;
        LIST_insert(clients, user);
        ep_client->data = user;
        res->res_type = HNDL_HSHAKE;
        res->res_data.hshake.status = SUCCESS;
        res->res_data.hshake.headers = NULL;
    }
    else
    {
        user_profile* user = (user_profile*)ep_client->data;
        void* data = req->req_data.frame.data;
        unsigned char *B = data + IV_LEN_BYTES + 2;
        unsigned char *A;
        int temp;
        unsigned short B_len, A_len;
        switch (user->status)
        {
            case USER_INIT:
                user->key_bundle = DHM_new();
                DHM_setIV(user->key_bundle, data);   // This only copies the first 16 bytes
                B_len = data[IV_LEN_BYTES] | (data[IV_LEN_BYTES+1] << 8);
                DHM_generate_secret_key(user->key_bundle, B, (int)B_len);
                user->status = SEND_COOKIE;
                A = DHM_get_key_bundle(user->key_bundle, &temp);
                A_len = temp & 0xffff;
                res->res_data.frame.data_len = 2 + A_len;
                res->res_data.frame.data = malloc(res->res_data.frame.data_len);
                memcpy(res->res_data.frame.data, &A_len, sizeof(unsigned short));
                memcpy(res->res_data.frame.data+2, A, A_len);
                free(A);
                break;
            case SEND_COOKIE:
                break;
            default:
               res->res_data.frame.data_len = 4;
               res->res_data.frame.data = strdup("Nope");   // This is just a dummy.
               res->close_client = true;
        }
        res->res_type = HNDL_FRAME;
        res->res_data.frame.is_utf8 = false;
    }
    /*
    A = DHM_get_key_bundle(alice, &A_len);
    B = DHM_get_key_bundle(bob, &B_len);
    DHM_generate_secret_key(alice, B, B_len);
    DHM_generate_secret_key(bob, A, A_len);
    DHM_destroy(alice);
    DHM_destroy(bob);
    free(A);
    free(B);
    */
    return res;
}

int user_exists(void* _username, const void* list_item)
{
   user_profile* user = (user_profile*)list_item;
   char* username = (char*)_username;
   if(strcmp(username, user->username))
      return 0;
   return 1;
}

/* Client Registration protocol Overview
 *
 * In the WS handshake protocol the client starts by sending username
 * in the "username" parameter of the GET request.
 *
 * After the username had been decided, now the endpoints can start exchanging the keys
 * using DiffieHellman
 * First the client sends the keys defined below
 *
 * +-----------+-------------------------+--------+
 * |  16 Bytes |         2 Bytes         |  SIZE  |
 * +-----------+-------------------------+--------+
 * |    IV     | SIZE IN BYTES OF PUBKEY | PUBKEY |
 * +-----------+-------------------------+--------+
 *
 */
