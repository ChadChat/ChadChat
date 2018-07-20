#include "index.h"
#include "../endpoint.h"

response* get_index(const request* req, const char data_type)
{
    response* res = malloc(sizeof(response));
    if(req->req_type == HNDL_HSHAKE)
    {
        res->res_type = HNDL_HSHAKE;
        res->res_data.hshake.status = SUCCESS;
    }
    else
    {
        res->res_type = HNDL_FRAME;
        res->res_data.frame.data_len = 6;
        res->res_data.frame.data = strdup("Hello!");
        res->res_data.frame.is_utf8 = true;
    }
    return res;
}