#if !defined(REG_CLIENT_IMP_CHAD)
#define REG_CLIENT_IMP_CHAD

#include "../endpoint.h"

#define USER_INIT 1
#define SEND_COOKIE 2

typedef struct
{
    char* username;
    DHM* key_bundle;
    char* cookie;
    char status;
}user_profile;

#endif // REG_CLIENT_IMP_CHAD
