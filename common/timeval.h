#ifndef TIMEVAL_H_INCLUDED
#define TIMEVAL_H_INCLUDED

/* The add/subtract functions assume timeval's passed are normalized; i.e.
   tv_usec is in range [0,1000000). */

#include <sys/time.h>

typedef time_t tv_sec_t;
typedef int tv_usec_t;

/* Get current time */
void tv_now(struct timeval *tv);

/* Update a timeval so tv_usec is normalized into range [0,1000000) */
void tv_normalize(struct timeval *tv);

/* Add `s' seconds and `us' microseconds to `dest' */
void tv_add(struct timeval *dest, tv_sec_t s, tv_usec_t us);
void tv_add_tv(struct timeval *dest, const struct timeval *src);
#define tv_add_s(dest, s)   tv_add(dest, s, 0)
#define tv_add_us(dest, us) tv_add(dest, 0, us)

/* Subtract `s' seconds and `us' microseconds to `dest' */
void tv_sub(struct timeval *dest, tv_sec_t s, tv_usec_t us);
void tv_sub_tv(struct timeval *dest, const struct timeval *src);
#define tv_sub_s(dest, s)   tv_sub(dest, s, 0)
#define tv_sub_us(dest, us) tv_sub(dest, 0, us)

/* Compare to timevals, and return -1, 0 or +1 depending on whether a is less
   than, equal to, or greater than b. */
int tv_cmp(const struct timeval *a, const struct timeval *b);

#endif /* ndef TIMEVAL_H_INCLUDED */
