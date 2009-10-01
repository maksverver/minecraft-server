/* Contains definitions for the network protocol used.*/
#ifndef PROTOCOL_H_INCLUDED
#define PROTOCOL_H_INCLUDED

#include <stdarg.h>

typedef unsigned char  Byte;
typedef unsigned short Short;
typedef unsigned int   Long;

#define DEFAULT_PORT    25565
#define STRING_LEN         64
#define ARRAY_LEN        1024
#define MAX_MESSAGE      4096

#define PROTO_HELO     0
#define PROTO_TICK     1
#define PROTO_STRT     2
#define PROTO_DATA     3
#define PROTO_SIZE     4
#define PROTO_MODR     5
#define PROTO_MODN     6
#define PROTO_PLYC     7
#define PROTO_PLYU     8
#define PROTO_UNK9     9
#define PROTO_UNKA    10
#define PROTO_UNKB    11
#define PROTO_DISC    12
#define PROTO_CHAT    13
#define PROTO_KICK    14
#define PROTO_NMSG    15

int proto_msg_len(int type);
int proto_msg_vbuild(int type, va_list ap, Byte *buf);

#endif /* ndef PROTOCOL_H_INCLUDED */
