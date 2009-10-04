#include "hooks.h"

static bool is_sponge(Type t)
{
    
}

static bool is_fluid(Type t)
{
    return t >= 8 && t <= 11;
}

/* Returns a type of fluid adjacent to (but not below) x/y/z or 0. */
static int adjacent_fluid(Level *level, int x, int y, int z)
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

int hook_authorize_update(Level *level, int x, int y, int z, Type t)
{
    /* TODO: restrict user-placable block types here */
    if (t > 40) return -1;
    if (t == 0) t = adjacent_fluid(level, x, y, z);
    return t;
}

Type hook_client_block_type(Type t)
{
    return t&0x3f;
}

int hook_on_neighbour_change( Level *level, int x, int y, int z, Type t,
                              int dir, Type old_t, Type new_t )
{
    if (!t)
    {
        if (DY[dir] >= 0 && is_fluid(new_t)) return new_t;
    }
    else
    if (is_fluid(t))
    {
    }

    return -1;
}
