#include "protocol.h"
#include <assert.h>
#include <string.h>

static const const char *g_prototypes[PROTO_NMSG] = {
    "bttb",     /*  0: HELO  hello (C->S, S->C) */
    "",         /*  1: TICK  tick (S->C) */
    "",         /*  2: STRT  level data start */
    "sab",      /*  3: DATA  level data (S->C) */
    "sss",      /*  4: SIZE  level size (S->C)*/
    "sssbb",    /*  5: MODR  modification request (C->S) */
    "sssb",     /*  6: MODN  modification notification (S->C) */
    "btsssbb",  /*  7: PLYC  new player announcement (S->C) */
    "bsssbb",   /*  8: PLYU  player update (C->S, S->C) */
    "bbbbbb",   /*  9: UNK9 */
    "bbbb",     /* 10: UNKA */
    "bbb",      /* 11: UNKB */
    "b",        /* 12: DISC  player disconnected (S->C) */
    "bt",       /* 13: CHAT  chat message */
    "t" };      /* 14: KICK  kicked (S->C) */


static int component_length(const char c)
{
    switch (c)
    {
    case 'a': return 1024;
    case 'b': return    1;
    case 's': return    2;
    case 't': return   64;
    }
    assert(0);
    return -1;
}

int proto_msg_len(int type)
{
    assert(type >= 0 && type < PROTO_NMSG);

    {
        const char *proto = g_prototypes[type];
        int res = 1;
        while (*proto) res += component_length(*proto++);
        return res;
    }
}

int proto_msg_vbuild(int type, va_list ap, Byte *buf)
{
    assert(type >= 0 && type < PROTO_NMSG);

    {
        int pos = 0;
        const char *proto = g_prototypes[type];

        buf[pos++] = type;

        while (*proto)
        {
            switch (*proto++)
            {
            case 'b':
                {
                    int i = va_arg(ap, int);
                    buf[pos++] = i&0xff;
                } break;
            case 's':
                {
                    int i = va_arg(ap, int);
                    buf[pos++] = (i&0xff00)>>8;
                    buf[pos++] = (i&0x00ff)>>0;
                } break;
            case 't':
                {
                    char *t = va_arg(ap, char*);
                    int i;
                    for (i = 0; i < STRING_LEN; ++i)
                        buf[pos++] = (*t) ? *t++ : ' ';
                } break;
            case 'a':
                {
                    void *p = va_arg(ap, void*);
                    memcpy(buf + pos, p, ARRAY_LEN);
                    pos += ARRAY_LEN;
                } break;
            default:
                assert(0);
            }
        }

        return pos;
    }
}

/*
static int build_message(Byte *buf, int type, ...)
{
    int len;
    va_list ap;

    va_start(ap, type);
    len = vbuild_message(type, ap, buf);
    va_end(ap);

    return len;
}
*/
