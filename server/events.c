#include "events.h"
#include "assert.h"
#include "common/heap.h"
#include "common/logging.h"

#include <zlib.h>
#include <stdio.h>
#include <string.h>

#define QUEUE_CAP 1000000

static Event  g_queue[QUEUE_CAP];
static size_t g_queue_size = 0;
static bool   g_dirty = false;

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
        g_dirty = true;
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
        g_dirty = true;
    }
}

bool event_queue_is_dirty()
{
    return g_dirty;
}

bool event_queue_write(const char *path)
{
    struct timeval now;
    size_t n;
    gzFile fp;

    tv_now(&now);

    fp = gzopen(path, "wt");
    if (fp == Z_NULL) return false;

    for (n = 0; n < g_queue_size; ++n)
    {
        struct timeval tv = g_queue[n].base.time;
        int sec, usec;
        tv_sub_tv(&tv, &now);
        sec  = (int)tv.tv_sec;
        usec = (int)tv.tv_usec;

        switch (g_queue[n].base.type)
        {
        case EVENT_TYPE_TICK:   /* don't save tick or save events */
        case EVENT_TYPE_SAVE:
            break;

        case EVENT_TYPE_UPDATE:
            {
                UpdateEvent *ev = &g_queue[n].update_event;
                gzprintf(fp, "update %d %d %d %d %d %d %d\n", sec, usec,
                             ev->x, ev->y, ev->z, ev->old_t, ev->new_t);
            } break;

        case EVENT_TYPE_FLOW:
            {
                FlowEvent *ev = &g_queue[n].flow_event;
                gzprintf(fp, "flow %d %d %d %d %d\n", sec, usec,
                             ev->x, ev->y, ev->z);
            } break;

        case EVENT_TYPE_GROW:
            {
                GrowEvent *ev = &g_queue[n].grow_event;
                gzprintf(fp, "grow %d %d %d %d %d\n", sec, usec,
                             ev->x, ev->y, ev->z);
            } break;

        default:
            fatal("cannot write event with unrecognized type %d\n",
                  g_queue[n].base.type);
        }
    }

    gzclose(fp);
    g_dirty = false;
    return true;
}

bool event_queue_read(const char *path)
{
    struct timeval now;
    char line[1024];
    gzFile fp;

    tv_now(&now);
    fp = gzopen(path, "rt");
    if (fp == Z_NULL) return false;

    while (gzgets(fp, line, sizeof(line)))
    {
        int sec, usec, x, y, z, u, t;
        Event ev;

        if (sscanf(line, "update %d %d %d %d %d %d %d",
                         &sec, &usec, &x, &y, &z, &t, &u) == 7)
        {
            ev.base.type = EVENT_TYPE_UPDATE;
            ev.update_event.x     = x;
            ev.update_event.y     = y;
            ev.update_event.z     = z;
            ev.update_event.old_t = t;
            ev.update_event.new_t = u;
        }
        else
        if (sscanf(line, "flow %d %d %d %d %d",
                         &sec, &usec, &x, &y, &z) == 5)
        {
            ev.base.type = EVENT_TYPE_FLOW;
            ev.update_event.x = x;
            ev.update_event.y = y;
            ev.update_event.z = z;
        }
        else
        if (sscanf(line, "grow %d %d %d %d %d",
                          &sec, &usec, &x, &y, &z) == 5)
        {
            ev.base.type = EVENT_TYPE_GROW;
            ev.update_event.x = x;
            ev.update_event.y = y;
            ev.update_event.z = z;
        }
        else
        {
            error("could not parse event line: %s", line);
            continue;
        }

        /* Set absolute timestamp: */
        ev.base.time = now;
        tv_add(&ev.base.time, sec, usec);

        /* Push event into queue: */
        event_push(&ev);
    }

    gzclose(fp);
    return true;
}
