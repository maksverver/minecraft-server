#ifndef EVENTS_H_INCLUDED
#define EVENTS_H_INCLUDED

#include "common/timeval.h"
#include <stdlib.h>

typedef enum EventType {
    EVENT_TYPE_NONE = 0,
    EVENT_TYPE_TICK,
    EVENT_TYPE_SAVE,
    EVENT_TYPE_UPDATE,
    EVENT_TYPE_FLOW,
    EVENT_TYPE_GROW
} EventType;

typedef struct EventBase
{
    struct timeval  time;
    EventType       type;
} EventBase;

typedef struct TickEvent
{
    int dummy;
} TickEvent;

typedef struct SaveEvent
{
    int dummy;
} SaveEvent;

typedef struct UpdateEvent
{
    unsigned char x, y, z, old_t, new_t;
} UpdateEvent;

typedef struct FlowEvent
{
    unsigned char x, y, z;
} FlowEvent;

typedef struct GrowEvent
{
    unsigned char x, y, z;
} GrowEvent;

typedef struct Event
{
    EventBase base;
    union {
        UpdateEvent update_event;
        SaveEvent   save_event;
        FlowEvent   flow_event;
        GrowEvent   grow_event;
    };
} Event;

/* Count number of pending events */
size_t event_count();

/* Add an event to the global queue */
void event_push(const Event *event);

/* Return pointer to next event (or NULL if queue is empty) without removing
   it from the queue. */
Event *event_peek();

/* Remove the next event from the queue. If `event' is non-NULL, the event
   removed is copied to `event' first. */
void event_pop(Event *event);

#endif /* ndef EVENTS_H_INCLUDED */
