#include "log.h"

#include <stdio.h>
#include <stdarg.h>

static FILE *log_fd = stderr;	/* default is stderr */

void set_logging(char *filename)
{
	FILE *fp;
	if (filename && (fp = fopen(filename, "w+")))
		log_fd = fp;
	else
		log_fd = stderr;
	log(LOG_INFO, "Logger started @ %d", time(NULL));
}

void log(enum LOG_FLAGS flags, const char *errstr, ...)
{
	/* TODO: Add colors and shit */
	va_list ap;
	char *type;
	switch (flags) {
		case LOG_INFO:
			type = "[INFO]: ";
			break;
		case LOG_WARN:
			type = "[WARN]: ";
			break;
		default:
			type = "[ERROR]: ";
	}
	fprintf(log_fd, type);
	va_start(ap, errstr);
	vfprintf(log_fd, errstr, ap);
	fprintf(log_fd, "\n");
	va_end(ap);
}

