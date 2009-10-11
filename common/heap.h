#ifndef HEAP_H_INCLUDED
#define HEAP_H_INCLUDED

#include <stdlib.h>

/* Functions to implements a max-heap, which can be used as a priority queue.

For the context of these functions, a heap is an array of elements stored in
max-heap order induced by an arbitrary comparator. The maximum element is
always stored in front of the array.

Elements can be added and removed with heap_push() and heap_pop() respectively.
Additionally, functions are provided to convert an unordered array into a heap,
a heap into an ordered array, and sort the elements in an array using the heap
functions.

The comparator used is similar to the one used by the qsort() function: it takes
pointers to two elements, and returns an integer less than, equal to, or greater
than zero when the first element is considered less than, equal to, org reater
than the second element (respectively). It need not induce a total order on the
elements; in this case, there may be multiple maximum elements in the heap and
an arbitrary one is placed in front. */

typedef int (*heap_cmp_t)(const void *, const void *);

/* Add an element to the heap.

   Precondition:
     `base' is an array containing `nmemb' elements of `size' bytes each and is
     in max-heap order induced by `cmp' and must have room for `size' more
     bytes.

     `new_elem' is a non-NULL pointer to a new element and is not in range of
     [base:base + (size + 1)*nmemb).

   Postcondition:
    `new_elem' is added to `base' maintaining max-heap order.
*/
void heap_push( void *base, size_t nmemb, size_t size, heap_cmp_t cmp,
                const void *new_elem );


/* Removes the maximum element from the heap.

   Precondition:
     `base' is an array containing `nmemb' elements of `size' bytes each and is
     in max-heap order induced by `cmp'. `nmemb' is greater than 0. `old_elem'
     is either NULL or a pointer to a buffer of at least `size' bytes.

   Postcondition:
     `base' is an array containing `nmemb` - 1 elements in max-heap order.
     If `old_elem' was non-NULL the element removed was copied to `old_elem'.
*/
void heap_pop( void *base, size_t nmemb, size_t size, heap_cmp_t cmp,
               void *old_elem );


/* Reorder an array into a heap.

   Precondition:
     `base' is an array containing `nmemb' elements of `size' bytes each.

   Postcondition:
     `base' is in max-heap order induced by `cmp'.
*/
void heap_create(void *base, size_t nmemb, size_t size, heap_cmp_t cmp);


/* Reorder a heap into a sorted array.

   Precondition:
     `base' is an array containing `nmemb' elements of `size' bytes each and is
     in max-heap order induced by `cmp'.

   Postcondition:
     `base' is an array with `nmemb' elements ordered according to `cmp'.

   N.B. allocates `size' bytes on the stack.
*/
void heap_to_ordered_array( void *base, size_t nmemb, size_t size,
                            heap_cmp_t cmp );


/* Sorts the elements of an array.

   Precondition:
     `base' is an array containing `nmemb' elements of `size' bytes each.

   Postcondition:
     `base' is an array with `nmemb' elements ordered according to `cmp'.
*/
void heap_sort(void *base, size_t nmemb, size_t size, heap_cmp_t cmp);

#endif /* def HEAP_H_INCLUDED */
