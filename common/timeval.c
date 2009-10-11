#include "timeval.h"
#include <stdlib.h>
#include <assert.h>

#define USEC_PER_SEC    1000000

static void normalize(tv_sec_t *s, tv_usec_t *us)
{
    if (*us < 0)
    {
        int n = (-*us + USEC_PER_SEC - 1)/USEC_PER_SEC;
        *s  += n;
        *us += n*USEC_PER_SEC;
    }
    else
    if (*us > USEC_PER_SEC)
    {
        int n = *us/USEC_PER_SEC;
        *s  += n;
        *us -= n*USEC_PER_SEC;
    }
}

static void add_normalized(struct timeval *dest, tv_sec_t s, tv_usec_t us)
{
    dest->tv_sec  += s;
    dest->tv_usec += us;
    if (dest->tv_usec > USEC_PER_SEC)
    {
        dest->tv_sec  += 1;
        dest->tv_usec -= USEC_PER_SEC;
    }
}

static void sub_normalized(struct timeval *dest, tv_sec_t s, tv_usec_t us)
{
    /* assume dest and src are both normalized */
    dest->tv_sec  -= s;
    dest->tv_usec -= us;
    if (dest->tv_usec < 0)
    {
        dest->tv_sec  -= 1;
        dest->tv_usec += USEC_PER_SEC;
    }
}

void tv_now(struct timeval *tv)
{
    (void)gettimeofday(tv, NULL);
}

void tv_normalize(struct timeval *tv)
{
    assert(sizeof(tv->tv_sec) == sizeof(tv_sec_t));
    assert(sizeof(tv->tv_usec) == sizeof(tv_usec_t));
    normalize((tv_sec_t*)&tv->tv_sec, (tv_usec_t*)&tv->tv_usec);
}

void tv_add(struct timeval *dest, tv_sec_t s, tv_usec_t us)
{
    normalize(&s, &us);
    add_normalized(dest, s, us);
}

void tv_add_tv(struct timeval *dest, const struct timeval *src)
{
    add_normalized(dest, src->tv_sec, src->tv_usec);
}

void tv_sub(struct timeval *dest, tv_sec_t s, tv_usec_t us)
{
    normalize(&s, &us);
    sub_normalized(dest, s, us);
}

void tv_sub_tv(struct timeval *dest, const struct timeval *src)
{
    sub_normalized(dest, src->tv_sec, src->tv_usec);
}

int tv_cmp(const struct timeval *a, const struct timeval *b)
{
    if (a->tv_sec < b->tv_sec) return -1;
    if (a->tv_sec > b->tv_sec) return +1;
    if (a->tv_usec < b->tv_usec) return -1;
    if (a->tv_usec > b->tv_usec) return +1;
    return 0;
}
