#if !defined(REG_CLIENT_IMP_CHAD)
#define REG_CLIENT_IMP_CHAD

#include "../endpoint.h"

#define USER_INIT 0
#define USER_OPEN 1
#define USER_END 2

#define COOKIE_LEN 64

typedef struct
{
    char* username;
    DHM* key_bundle;
    char* cookie;
    char status;
    int task_number;
}user_profile;

#endif // REG_CLIENT_IMP_CHAD
