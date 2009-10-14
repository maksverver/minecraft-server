#include "hooks.h"
#include "common/logging.h"
#include "common/timeval.h"
#include <stdio.h>
#include <string.h>

#define GROW_DELAY_MIN_SEC       3
#define GROW_DELAY_MAX_SEC      60

const struct timeval
    zero_delay        = { 0,      0 },
    water_flow_delay  = { 0, 300000 }, /* 300ms */
    lava_flow_delay   = { 3,      0 }, /* 3s */
    supersponge_delay = { 0, 250000 }; /* 250ms */

static int min(int i, int j) { return i < j ? i : j; }
static int max(int i, int j) { return i > j ? i : j; }
static bool is_fluid(Type t) { return t >= BLOCK_WATER1 && t <= BLOCK_LAVA2; }
static bool is_water(Type t) { return t >= BLOCK_WATER1 && t <= BLOCK_WATER2; }
static bool is_lava(Type t) { return t >= BLOCK_LAVA1 && t <= BLOCK_LAVA2; }

static bool is_plant(Type t)
{
    switch (t)
    {
    case BLOCK_SAPLING:
    case BLOCK_FLOWER_YELLOW:
    case BLOCK_FLOWER_RED:
    case BLOCK_MUSHROOM:
    case BLOCK_TOADSTOOL:
        return true;

    default:
        return false;
    }
}

static bool is_light_blocker(Type t)
{
    return t != BLOCK_EMPTY  && t != BLOCK_GLASS &&
           t != BLOCK_LEAVES && !is_plant(t);
}

static bool is_soil(Type t)
{
    return t == BLOCK_DIRT || t == BLOCK_GRASS;
}


static bool is_player_placeable(Type t, bool admin)
{
    switch (t)
    {
    case BLOCK_STONE_GREY:
    case BLOCK_DIRT:
    case BLOCK_ROCK:
    case BLOCK_WOOD:
    case BLOCK_SAPLING:
    case BLOCK_STONE_YELLOW:
    case BLOCK_STONE_MIXED:
    case BLOCK_TRUNK:
    case BLOCK_LEAVES:
    case BLOCK_SPONGE:
    case BLOCK_GLASS:
    case BLOCK_COLORED1:
    case BLOCK_COLORED2:
    case BLOCK_COLORED3:
    case BLOCK_COLORED4:
    case BLOCK_COLORED5:
    case BLOCK_COLORED6:
    case BLOCK_COLORED7:
    case BLOCK_COLORED8:
    case BLOCK_COLORED9:
    case BLOCK_COLORED10:
    case BLOCK_COLORED11:
    case BLOCK_COLORED12:
    case BLOCK_COLORED13:
    case BLOCK_COLORED14:
    case BLOCK_COLORED15:
    case BLOCK_COLORED16:
    case BLOCK_FLOWER_YELLOW:
    case BLOCK_FLOWER_RED:
    case BLOCK_MUSHROOM:
    case BLOCK_TOADSTOOL:
    case BLOCK_GOLD:
        return true;

    case BLOCK_SUPERSPONGE:
    case BLOCK_LAVA2:
    case BLOCK_WATER2:
    case BLOCK_ADMINIUM:
        return admin;

    default:
        return false;
    }
}

static bool is_player_deletable(Type t, bool admin)
{
    switch (t)
    {
    case BLOCK_GRASS:
    case BLOCK_ORE1:
    case BLOCK_ORE2:
    case BLOCK_ORE3:
        return true;

    default:
        return is_player_placeable(t, admin);
    }
}

static bool is_player_replacable(Type t, bool admin)
{
    (void)admin; /* unused */

    switch (t)
    {
    case BLOCK_WATER1:
    case BLOCK_WATER2:
    case BLOCK_LAVA1:
    case BLOCK_LAVA2:
        return true;

    default:
        return false;
    }
}

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

static void update_block_delayed( int x, int y, int z, Type new_t,
                                  const struct timeval *delay )
{
    void server_update_block( int x, int y, int z, Type new_t,
                              const struct timeval *delay );

    server_update_block(x, y, z, new_t, delay);
}


static void update_block(int x, int y, int z, Type new_t)
{
    update_block_delayed(x, y, z, new_t, &zero_delay);
}

int hook_authorize_update( const Level *level, const Player *player,
                           int x, int y, int z, Type old_t, Type new_t )
{
    /* NB. new_t is a client block type at this point! */

    /* Reject update if it doesn't change anything: */
    if (old_t == new_t) return -1;

    /* Handle tileset mapping: */
    switch (player->tileset)
    {
    case 1:
        switch (new_t)
        {
        case BLOCK_COLORED1:  new_t = BLOCK_LAVA2;       break; /* red */
        case BLOCK_COLORED3:  new_t = BLOCK_SUPERSPONGE; break; /* yellow */
        case BLOCK_COLORED8:  new_t = BLOCK_WATER2;      break; /* blue */
        case BLOCK_COLORED14: new_t = BLOCK_ADMINIUM;    break; /* grey */
        }
        break;
    }

    if (old_t != BLOCK_EMPTY && new_t != BLOCK_EMPTY)
    {
        /* replacing a block: */
        if (!is_player_replacable(old_t, player->admin) ||
            !is_player_placeable(new_t, player->admin)) return -1;
    }
    else
    if (old_t != BLOCK_EMPTY) /* new_t == BLOCK_EMPTY */
    {
        /* deleting a block: */
        if (!is_player_deletable(old_t, player->admin)) return -1;
    }
    else
    if (new_t != BLOCK_EMPTY) /* old_t == BLOCK_EMPTY */
    {
        /* placing a block: */
        if (!is_player_placeable(new_t, player->admin)) return -1;
    }

    /* Plants must be placed on soil: */
    if (is_plant(new_t) && !is_soil(level_get_block(level, x, y - 1, z)))
        return -1;

    info("User placing block of type %d at (%d,%d,%d)\n", (int)new_t, x, y, z);

    return new_t;
}

Type hook_client_block_type(Type t)
{
    return t&0x3f;
}

static void post_flow_event(int x, int y, int z, const struct timeval *delay)
{
    Event new_ev;
    tv_now(&new_ev.base.time);
    tv_add_tv(&new_ev.base.time, delay);
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

static void activate_block(const Level *level, int x, int y, int z)
{
    Type t = level_get_block(level, x, y, z); 
    switch (t)
    {
    case BLOCK_WATER1:
    case BLOCK_WATER2:
        post_flow_event(x, y, z, &water_flow_delay);
        break;

    case BLOCK_LAVA1:
    case BLOCK_LAVA2:
        post_flow_event(x, y, z, &lava_flow_delay);
        break;

    case BLOCK_DIRT:
        post_grow_event(x, y, z);
        break;

    case BLOCK_GRASS:
        if (is_light_blocker(level_get_block(level, x, y + 1, z)))
            update_block(x, y, z, BLOCK_DIRT);
        break;

    default:
        if (is_plant(t) && !is_soil(level_get_block(level, x, y - 1, z)))
            update_block(x, y, z, BLOCK_EMPTY);
        break;
    }
}

/* Activates blocks in the 2d+1 sized cube centered at x/y/z */
static void activate_blocks_nearby(const Level *level,
                                   int x, int y, int z, int d)
{
    int x1 = max(x - d, 0), x2 = min(x + d + 1, level->size.x);
    int y1 = max(y - d, 0), y2 = min(y + d + 1, level->size.y);
    int z1 = max(z - d, 0), z2 = min(z + d + 1, level->size.z);

    /* activate nearby blocks */
    for (x = x1; x < x2; ++x)
    {
        for (y = y1; y < y2; ++y)
        {
            for (z = z1; z < z2; ++z)
            {
                activate_block(level, x, y, z);
            }
        }
    }
}

static void activate_neighbours(const Level *level, int x, int y, int z)
{
    int d;

    for (d = 0; d < 6; ++d)
    {
        activate_block(level, x + DX[d], y + DY[d], z + DZ[d]);
    }
}

static void on_update(const Level *level, UpdateEvent *ev)
{
    if (level_get_block(level, ev->x, ev->y, ev->z) != ev->new_t) return;

    switch (ev->new_t)
    {
    case BLOCK_SPONGE:
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
                        if (is_fluid(level_get_block(level, x, y, z)))
                            update_block(x, y, z, BLOCK_EMPTY);
                    }
                }
            }
        }
        break;

    case BLOCK_SUPERSPONGE:
        {
            /* Flood-filling super sponge */
            int d;
            for (d = 0; d < 6; ++d)
            {
                int nx = ev->x + DX[d];
                int ny = ev->y + DY[d];
                int nz = ev->z + DZ[d];
                Type t = level_get_block(level, nx, ny,nz);
                if (is_fluid(t))
                {
                    update_block_delayed(nx, ny, nz, BLOCK_SUPERSPONGE,
                                         &supersponge_delay);
                }
            }
            update_block(ev->x, ev->y, ev->z, BLOCK_EMPTY);
        }
        break;
    }

    if (ev->old_t == BLOCK_SPONGE)
    {
        activate_blocks_nearby(level, ev->x, ev->y, ev->z, 3);
    }
    else
    {
        activate_block(level, ev->x, ev->y, ev->z);
        activate_neighbours(level, ev->x, ev->y, ev->z);
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
        if (level_index_valid(level, nx, ny, nz))
        {
            Type u = level_get_block(level, nx, ny, nz);
            if ( u == BLOCK_EMPTY &&
                 !type_nearby(level, nx, ny, nz, BLOCK_SPONGE, 3))
            {
                /* Propagate fluid */
                update_block(nx, ny, nz, t);
            }
            else
            if ((is_water(t) && is_lava(u)) || (is_lava(t) && is_water(u)))
            {
                /* Water and lava make stone */
                update_block(nx, ny, nz, BLOCK_STONE_GREY);
            }
        }
    }
}

static void on_grow(const Level *level, GrowEvent *ev)
{
    if (level_get_block(level, ev->x, ev->y, ev->z) == BLOCK_DIRT &&
        !is_light_blocker(level_get_block(level, ev->x, ev->y + 1, ev->z)))
    {
        update_block(ev->x, ev->y, ev->z, BLOCK_GRASS);
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

/* TODO:
    grow tree trunk/leaves, but only if trunk is planted on dirt :=)

    lava/water combine to form rock
*/

int hook_on_chat(Player *pl, const char *in, char *out, size_t out_size)
{
    char arg_s[33];
    int arg_i;

    if (sscanf(in, "/auth %32s", arg_s) == 1)
    {
        pl->admin = strcmp(arg_s, "fiets") == 0; /* FIXME */
        snprintf(out, out_size, "access %s", pl->admin ? "granted" : "denied");
        return 1;
    }

    if (sscanf(in, "/set tileset %d", &arg_i) == 1)
    {
        if (arg_i >= 0 && arg_i < 2) pl->tileset = arg_i;
        return 0;
    }

    if (strcmp(in, "/set tileset") == 0)
    {
        snprintf(out, out_size, "current tileset: %d", pl->tileset);
        return 1;
    }

    /* Normal chat message: */
    snprintf(out, out_size, "%s: %s", pl->name, in);
    return 2;
}
