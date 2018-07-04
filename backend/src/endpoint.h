#ifndef ENDPT_IMP_CHAD
#define ENDPT_IMP_CHAD

#include "utils/strmap.h"

typedef short (*endpoint_cb)(const void *req, void** res);

typedef struct
{
    StrMap* hmap;
}endp;

#endif