#include <stdlib.h>
#include <string.h>
#include "zlib.h"

void *gzip_compress(void *buf_in, size_t len_in, size_t *len_out)
{
    void *buf_out = NULL, *buf_new;
    size_t pos = 0, len;
    int res;
    z_stream zs;

    /* Estimate required buffer size (N.B. this will be doubled below!) */
    len = len_in/32;
    if (len < 256) len = 256;

    memset(&zs, 0, sizeof(zs));
    zs.next_in  = buf_in;
    zs.avail_in = len_in;
    res = deflateInit2 (&zs, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 0x1f /* gzip */,
                             8 /* default mem level */, Z_DEFAULT_STRATEGY);
    while (res == Z_OK)
    {
        len *= 2;
        buf_new = realloc(buf_out, len);
        if (!buf_new) goto failed;
        buf_out = buf_new;
        zs.next_out  = buf_out + pos;
        zs.avail_out = len - pos;
        res = deflate(&zs, Z_FINISH);
        pos = len - zs.avail_out;
    }
    if (res != Z_STREAM_END) goto failed;

    buf_new = realloc(buf_out, pos);
    if (buf_new) buf_out = buf_new;
    *len_out = pos;
    return buf_out;

failed:
    deflateEnd(&zs);
    free(buf_out);
    return NULL;
}
