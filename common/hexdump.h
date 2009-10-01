#ifndef HEXDUMP_H_INCLUDED
#define HEXDUMP_H_INCLUDED

#include <stdio.h>
#include <stdint.h>

void hexdump(const void *buf, size_t len, FILE *fp);

#endif /* ndef HEXDUMP_H_INCLUDED */

