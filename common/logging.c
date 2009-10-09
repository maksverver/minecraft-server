#include "logging.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

static void write_log(const char *prefix, const char *fmt, va_list ap)
{
    fputs(prefix, stderr);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    fflush(stderr);
}

void info(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    write_log("", fmt, ap);
    va_end(ap);
}

void warn(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    write_log("WARNING: ", fmt, ap);
    va_end(ap);
}

void error(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    write_log("ERROR: ", fmt, ap);
    va_end(ap);
}

void fatal(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    write_log("FATAL ERROR: ", fmt, ap);
    va_end(ap);
    exit(1);
}
