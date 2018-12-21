#include "register_client.h"
#include "../utils/list.h"
#include "../utils/dh.h"
#include <string.h>
#include <openssl/rand.h>

// NOTE: remove all the reference to the user instance in the remove_task_user function.

// TODO: Make the client connected which is binded to the socket or something.
// TODO: When the client closes free all the resources allocated to it.
// TODO: Make the client'd id last only a hours before it gets destroyed.

extern hmap* globals;
char valid_cookie[] = "0123456789"
                        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                        "abcdefghijklmnopqrstuvwxyz"
                        "!@#$%^&*()_-+=[]{};:<>,./?"

static char* generate_cookie(void)
{
    char* cookie = malloc(COOKIE_LEN+1);
    int cookie_bob = sizeof(valid_cookie) - 1;
    unsigned int rand_int;
    for (int i = 0; i < COOKIE_LEN; i++) {
        if(RAND_bytes(&rand_int, sizeof(int)) == 0)
            RAND_pseudo_bytes(&rand_int, sizeof(int));   // if the other fails.
        cookie[i] = valid_cookie[rand_int % cookie_bob];
    }
    cookie[COOKIE_LEN] = 0;   // NULL

    return cookie;
}

void remove_user_task(void* _user)
{
    user_profile* user = (user_profile*) _user;
    LIST* clients;
    int user_index;
    // also remove it from the globals here
    hmap_get(globals, "clients", &clients);
    user_index = LIST_find(clients, username, user_exists);  // Check if username exists
    if(user_index != -1)
      LIST_remove(clients, user_index);
    free(user->username);
    free(user->cookie);
    DHM_destroy(user->key_bundle);
    free(user);
}

response* get_register_client(const request* req, const char data_type, endp_client* ep_client)
{
    response* res = malloc(sizeof(response));
    LIST* clients;
    memset(res, 0, sizeof(response));
    if(req->req_type == HNDL_HSHAKE)
    {
        // if the username is not in the get request return bad_request.
        if((req->req_data.hshake.data == NULL) || !(sm_exists(req->req_data.hshake.data, "username")))
        {
            res->res_type = HNDL_HSHAKE;
            res->res_data.hshake.status = BAD_REQUEST;
            res->res_data.hshake.headers = NULL;
            res->close_client = true;
            return res;
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
        // add task here to remove the client resources if the client doesn't reply back in 5 seconds
        user->task_number = task_add_new(5, user, remove_user_task);
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
        unsigned char *B;
        unsigned char *A;
        int temp, B_len;
        unsigned short A_len;
        unsigned char* cookie_cipher;
        size_t cipher_len;


        // also send the encrypted cookie inside the INIT
        switch (user->status)
        {
            case USER_INIT:
                user->key_bundle = DHM_new();
                DHM_setIV(user->key_bundle, data);   // This only copies the first 16 bytes

                B_len = req->req_data.frame.data_len - IV_LEN_BYTES;
                B = data + IV_LEN_BYTES;    // Set the offset onto B's pub key
                DHM_generate_secret_key(user->key_bundle, B, B_len);
                user->status = USER_OPEN;
                A = DHM_get_key_bundle(user->key_bundle, &temp);
                A_len = temp & 0xffff;

                user->cookie = generate_cookie();
                cookie_cipher = malloc(GET_MAX_CIPHER_LEN);
                cipher_len = DHM_encrypt(user->key_bundle, user->cookie, COOKIE_LEN, cookie_cipher);

                res->res_data.frame.data_len = 2 + A_len + cipher_len;
                res->res_data.frame.data = malloc(res->res_data.frame.data_len);
                memcpy(res->res_data.frame.data, &A_len, sizeof(unsigned short));
                memcpy(res->res_data.frame.data+2, A, A_len);
                memcpy(res->res_data.frame.data+2+A_len, cookie_cipher, cipher_len);

                free(A);
                free(cookie_cipher);
                break;
            case USER_OPEN:
                // idk what to do with this yet.
                break;
            case USER_END:
                break;
            default:
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
 * =====================
 *    CLIENT TO SERVER
 * =====================
 * +-----------+--------+
 * |  16 Bytes |  REST  |
 * +-----------+--------+
 * |    IV     | PUBKEY |
 * +-----------+--------+
 *
 * ======================
 *    SERVER TO CLIENT
 * ======================
 * +------------------+---------+----------+
 * |     2 Bytes      |   SIZE  |   REST   |
 * +------------------+---------+----------+
 * |  SIZE of PUBKEY  |  PUBKEY |  COOKIE  |
 * +------------------+---------+----------+
 *
 */
