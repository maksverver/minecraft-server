#include "heap.h"
#include <alloca.h>
#include <assert.h>
#include <string.h>

#define parent(i)         (((i) - 1)/2)
#define lchild(i)         (2*(i) + 1)
#define rchild(i)         (2*(i) + 2)
#define idx(b, s, i)      ((char*)(b) + (s)*(i))

void heap_push( void *base, size_t nmemb, size_t size, heap_cmp_t cmp,
                const void *new_elem )
{
    size_t i;

    assert( new_elem != NULL && (
                (char*)new_elem + size <= (char*)base ||
                (char*)new_elem >= (char*)base + (nmemb + 1)*size ) );

    i = nmemb;
    while (i > 0 && cmp(new_elem, idx(base, size, parent(i))) > 0)
    {
        memcpy(idx(base, size, i), idx(base, size, parent(i)), size);
        i = parent(i);
    }
    memcpy(idx(base, size, i), new_elem, size);
}

void heap_pop( void *base, size_t nmemb, size_t size, heap_cmp_t cmp,
               void *old_elem )
{
    void *new_elem = idx(base, size, nmemb - 1);
    size_t i = 0;

    assert(nmemb > 0);

    if (old_elem != NULL)
    {
        assert( (char*)old_elem + size <= (char*)base ||
                (char*)old_elem        >= (char*)base + nmemb*size );

        memcpy(old_elem, idx(base, size, i), size);
    }

    for (;;)
    {
        void *l = idx(base, size, lchild(i));
        void *r = idx(base, size, rchild(i));
        size_t j;

        if (l < new_elem && cmp(l, new_elem) > 0)  /* left > new_elem */
        {
            j = (r < new_elem && cmp(r, l) > 0) ? rchild(i) : lchild(i);
        }
        else
        if (r < new_elem && cmp(r, new_elem) > 0) /* right > new_elem */
        {
            j = rchild(i);
        }
        else
        {
            break;
        }

        memcpy(idx(base, size, i), idx(base, size, j), size);
        i = j;
    }
    memcpy(idx(base, size, i), new_elem, size);
}

void heap_create(void *base, size_t nmemb, size_t size, heap_cmp_t cmp)
{
    if (size > 1)
    {
        void *const temp = alloca(size);
        size_t i;

        memcpy(temp, base, size);

        for (i = 1; i < nmemb; ++i)
            heap_push(base, i - 1, size, cmp, idx(base, size, i));

        heap_push(base, i - 1, size, cmp, temp);
    }
}

void heap_to_ordered_array( void *base, size_t nmemb, size_t size,
                            heap_cmp_t cmp )
{
    if (size > 1)
    {
        void *const temp = alloca(size);
        size_t i;

        for (i = nmemb; i > 1; --i)
        {
            heap_pop(base, i, size, cmp, temp);
            memcpy((char*)base + (i - 1)*size, temp, size);
        }
    }
}

void heap_sort(void *base, size_t nmemb, size_t size, heap_cmp_t cmp)
{
    heap_create(base, nmemb, size, cmp);
    heap_to_ordered_array(base, nmemb, size, cmp);
}
