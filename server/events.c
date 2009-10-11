#include "events.h"
#include "assert.h"
#include "common/heap.h"
#include "common/logging.h"

#define QUEUE_CAP 1000000

static Event  g_queue[QUEUE_CAP];
static size_t g_queue_size = 0;

/* Returns +1 if a's time is less than b's time */
int event_cmp(const void *a, const void *b)
{
    return -tv_cmp(&((EventBase*)a)->time, &((EventBase*)b)->time);
}

size_t event_count()
{
    return g_queue_size;
}

void event_push(const Event *event)
{
    if (g_queue_size == QUEUE_CAP)
    {
        error( "Can't push an event of type %d; queue full!",
               (int)event->base.type );
    }
    else
    {
        heap_push(g_queue, g_queue_size, sizeof(*g_queue), event_cmp, event);
        ++g_queue_size;
    }
}

Event *event_peek()
{
    return (g_queue_size > 0) ? &g_queue[0] : NULL;
}

void event_pop(Event *event)
{
    assert(g_queue_size > 0);
    if (g_queue_size > 0)
    {
        heap_pop(g_queue, g_queue_size, sizeof(*g_queue), event_cmp, event);
        --g_queue_size;
    }
}
