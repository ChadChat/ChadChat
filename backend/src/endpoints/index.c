#include "index.h"
#include "../endpoint.h"
#include <stdio.h>

void print_data(const char* key, const char* val, const char something)
{
    printf("%s=%s\n", key, val);
}

response* get_index(const request* req, const char data_type)
{
    response* res = malloc(sizeof(response));
    if(req->req_type == HNDL_HSHAKE)
    {
        if(req->req_data.hshake.data != NULL)
            sm_enum(req->req_data.hshake.data, print_data, NULL);
        res->res_type = HNDL_HSHAKE;
        res->res_data.hshake.status = SUCCESS;
        res->res_data.hshake.headers = NULL;
        res->close_client = false;
    }
    else
    {
        res->res_type = HNDL_FRAME;
        res->res_data.frame.data_len = 6;
        res->res_data.frame.data = strdup("Hello!");
        res->res_data.frame.is_utf8 = true;
        res->close_client = false;
    }
    return res;
}

/*
    How to handle callbacks
        There are two arguments that the function will take in one is the request struct defined in endpoint.h
        and the other is the data_type of the request, you can get two data_types UTF8_TYPE(utf-8) and BIN_TYPE(binary) it doesn't really matter what you choose
        but there is an option on what you want to use.
        The function should return a response, struct defined in endpoint.h

        How to handle request struct:
            -> There are two types of request the client can make, the handshake and the frame
            -> the client starts with a handshake(HNDL_HSHAKE) it's in this handshake the client will send the cookies and other GET/POST data
            you can access all the headers by using a hashmap this member is in `req_data.hshake.headers`
            to access the GET/POST data the client sent you can use this hashmap member called `req_data.hshake.data`
            -> The other request type of the client can make is the HNDL_FRAME this is the actual websocket part.
                there are two members you can access with the frame, the data of the payload and the payload itself
                to access the length you can use the member called `req_data.frame.data_len` and the payload with `req_data.frame.data`
            -> You * dont't * need to handle any memory stuff of the request struct, NOTE: it's `const`
            oh and also the frame part and the handshake are unions so you can access them based on the data_type you got.
        
        How to handle the reponse struct:
            -> There are again two type of responses you can give one is for the client handshake and the other is for the frame, ** send what req_type you got **
            -> In the handshake part of the response you have 3 members although you can only access two.
                -> The first member is the status code (res_data.hshake.status) ** Dont use any integer literals use the status codes given to you in endpoint.h:20, because some are different.
                -> The second member is the accept header you dont have to do anything to it because it's handled by your caller.
                -> The third member is any extra headers (res_data.hshake.headers) you want to send back. the memebr is a Strmap type or a hashmap in other words you can give key value pairs to it
                NOTE: the maximum header length allocated is 1300 bytes otherwise it will get truncated
            -> The second part is the frame:
                -> The first member is the length of the payload you are sending res_data.frame.data_len.
                -> The second memebr is a flag indicating if the payload you are sending is utf-8 type or not this can be useful in WS.
                -> The third member is the actual payload itself which ** should be malloc'ed ** 
            -> Also in any time if you want to close the client if it's the handshake or while in the frame part you can set the close_client member to true, otherwise make
                sure to set it false, ** dont leave it uninitialized **.
            -> The response struct itself ** should be malloc'ed **
    
    HAPPY CHADCHATING
*/