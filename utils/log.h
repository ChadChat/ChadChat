#ifndef LOG_IMPL_CHADCHAT
#define LOG_IMPL_CHADCHAT

#include <stdint.h>

enum LOG_FLAGS: char
{
	LOG_INFO,
	LOG_WARN,
	LOG_ERR
};

void set_logging(char *filename);
void log(enum LOG_FLAGS flags, const char *errstr, ...);

#endif /* ifndef LOG_IMPL_CHADCHAT */
