#include "hooks.h"
#include "common/logging.h"
#include "common/timeval.h"

#define EMPTY          0
#define STANDARD       1
#define GRASS          2
#define DIRT           3
#define FLUID1         8
#define FLUID2         9
#define FLUID3        10
#define FLUID4        11
#define SPONGE        19

#define FLOW_DELAY_USEC         300000  /* 300ms */

#define GROW_DELAY_MIN_SEC       3
#define GROW_DELAY_MAX_SEC      60

static int min(int i, int j) { return i < j ? i : j; }
static int max(int i, int j) { return i > j ? i : j; }
static bool is_fluid(Type t) { return t >= 8 && t <= 11; }

/* Returns whether the cube with sides of length 2*`d' - 1 centered around
   (x,y,z) contains a block of type `t'. */
static bool type_nearby(const Level *level, int x, int y, int z, Type t, int d)
{
    int x1 = max(x - d + 1, 0), x2 = min(x + d, level->size.x);
    int y1 = max(y - d + 1, 0), y2 = min(y + d, level->size.y);
    int z1 = max(z - d + 1, 0), z2 = min(z + d, level->size.z);

    for (x = x1; x < x2; ++x)
    {
        for (y = y1; y < y2; ++y)
        {
            for (z = z1; z < z2; ++z)
            {
                if (level_get_block(level, x, y, z) == t) return true;
            }
        }
    }

    return false;
}

/* Returns a type of fluid adjacent to (but not below) x/y/z or 0. */
static int adjacent_fluid(const Level *level, int x, int y, int z)
{
    Type t;
    int d;

    for (d = 0; d < 6; ++d)
    {
        if (DY[d] < 0) continue;  /* below doesn't count */
        t = level_get_block(level, x + DX[d], y + DY[d], z + DZ[d]);
        if (is_fluid(t)) return t;
    }
    return 0;
}

static void update_block(int x, int y, int z, Type new_t)
{
    void server_update_block( int x, int y, int z, Type new_t,
                              struct timeval *event_delay );

    struct timeval delay = { 0, 0 };
    server_update_block(x, y, z, new_t, &delay);
}


int hook_authorize_update( const Level *level, int x, int y, int z,
                           Type old_t, Type new_t )
{
    if (new_t > 40 || is_fluid(new_t)) return -1;

    if (new_t == EMPTY && !type_nearby(level, x, y, z, SPONGE, 3))
    {
        new_t = adjacent_fluid(level, x, y, z);
    }

    info("User placed block of type %d at (%d,%d,%d)\n", (int)new_t, x, y, z);

    return new_t;
}

Type hook_client_block_type(Type t)
{
    return t&0x3f;
}

static void post_flow_event(int x, int y, int z)
{
    Event new_ev;
    tv_now(&new_ev.base.time);
    tv_add_us(&new_ev.base.time, FLOW_DELAY_USEC);
    new_ev.base.type = EVENT_TYPE_FLOW;
    new_ev.flow_event.x = x;
    new_ev.flow_event.y = y;
    new_ev.flow_event.z = z;
    event_push(&new_ev);
}

static void post_grow_event(int x, int y, int z)
{
    Event new_ev;
    tv_now(&new_ev.base.time);
    tv_add_s( &new_ev.base.time, GROW_DELAY_MIN_SEC +
        rand()%(GROW_DELAY_MAX_SEC - GROW_DELAY_MIN_SEC + 1) );
    new_ev.base.type = EVENT_TYPE_GROW;
    new_ev.grow_event.x = x;
    new_ev.grow_event.y = y;
    new_ev.grow_event.z = z;
    event_push(&new_ev);
}

static void on_update(const Level *level, UpdateEvent *ev)
{
    if (level_get_block(level, ev->x, ev->y, ev->z) != ev->new_t) return;

    switch (ev->new_t)
    {
    case SPONGE:
        {
            int x1 = max(ev->x - 3 + 1, 0), x2 = min(ev->x + 3, level->size.x);
            int y1 = max(ev->y - 3 + 1, 0), y2 = min(ev->y + 3, level->size.y);
            int z1 = max(ev->z - 3 + 1, 0), z2 = min(ev->z + 3, level->size.z);
            int x, y, z;

            for (x = x1; x < x2; ++x)
            {
                for (y = y1; y < y2; ++y)
                {
                    for (z = z1; z < z2; ++z)
                    {
                        if (x != ev->x || y != ev->y || z != ev->z)
                            update_block(x, y, z, EMPTY);
                    }
                }
            }
        }
        break;

    case FLUID1:
    case FLUID2:
    case FLUID3:
    case FLUID4:
        {
            post_flow_event(ev->x, ev->y, ev->z);
        } break;

    case EMPTY:
        if (ev->old_t == SPONGE)
        {
            int x1 = max(ev->x - 4 + 1, 0), x2 = min(ev->x + 4, level->size.x);
            int y1 = max(ev->y - 4 + 1, 0), y2 = min(ev->y + 4, level->size.y);
            int z1 = max(ev->z - 4 + 1, 0), z2 = min(ev->z + 4, level->size.z);
            int x, y, z;

            for (x = x1; x < x2; ++x)
            {
                for (y = y1; y < y2; ++y)
                {
                    for (z = z1; z < z2; ++z)
                    {
                        if (x != ev->x || y != ev->y || z != ev->z)
                        {
                            if (is_fluid(level_get_block(level, x, y, z)))
                            {
                                post_flow_event(x, y, z);
                            }
                        }
                    }
                }
            }
        } break;

    case DIRT:
        post_grow_event(ev->x, ev->y, ev->z);
        break;
    }
}

static void on_flow(const Level *level, FlowEvent *ev)
{
    int d;
    Type t = level_get_block(level, ev->x, ev->y, ev->z);

    if (!is_fluid(t)) return;

    for (d = 0; d < 6; ++d)
    {
        int nx, ny, nz;

        if (DY[d] > 0) continue;  /* don't flow upward */

        nx = ev->x + DX[d];
        ny = ev->y + DY[d];
        nz = ev->z + DZ[d];
        if (level_index_valid(level, nx, ny, nz) &&
            level_get_block(level, nx, ny, nz) == EMPTY &&
            !type_nearby(level, nx, ny, nz, SPONGE, 3))
        {
            update_block(nx, ny, nz, t);
        }
    }
}


static void on_grow(const Level *level, GrowEvent *ev)
{
    Type t = level_get_block(level, ev->x, ev->y, ev->z);

    if (t == DIRT && level_get_block(level, ev->x, ev->y + 1, ev->z) == EMPTY)
    {
        update_block(ev->x, ev->y, ev->z, GRASS);
    }
}

void hook_on_event(const Level *level, Event *ev)
{
    switch (ev->base.type)
    {
    case EVENT_TYPE_UPDATE:
        on_update(level, &ev->update_event);
        break;

    case EVENT_TYPE_FLOW:
        on_flow(level, &ev->flow_event);
        break;

    case EVENT_TYPE_GROW:
        on_grow(level, &ev->grow_event);
        break;

    default: break;
    }
}

/* TODO: grow tree trunk/leaves, but only if trunk is planted on dirt :=) */
