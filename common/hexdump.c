#include "hexdump.h"

void hexdump(const void *buf, size_t len, FILE *fp)
{
    const char *p = buf;
    size_t i;
    for (i = 0; i < len; ++i)
    {
        fputc("0123456789abcdef"[(p[i]&0xf0)>>4], fp);
        fputc("0123456789abcdef"[(p[i]&0x0f)>>0], fp);
        fputc((i+1)%16 ? ' ' : '\n', fp);
    }
    if (i%16) fputc('\n', fp);
}
