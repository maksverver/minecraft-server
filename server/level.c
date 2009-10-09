#include "level.h"
#include "common/logging.h"
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>
#include <arpa/inet.h>

const const int DX[6] = { -1,  0,  0, +1,  0,  0 };
const const int DY[6] = {  0, -1,  0,  0, +1,  0 };
const const int DZ[6] = {  0,  0, -1,  0,  0, +1 };

bool level_index_valid(const Level *level, int x, int y, int z)
{
    return (unsigned)x < level->size.x &&
           (unsigned)y < level->size.y &&
           (unsigned)z < level->size.z;
}

static size_t idx(const Level *level, int x, int y, int z)
{
    return x + (size_t)level->size.x*(z + (size_t)level->size.z*y);
}

void level_free(Level *level)
{
    if (!level) return;
    free(level->blocks);
    free(level->name);
    free(level->creator);
}

Level *level_load(const char *path)
{
    Level *level = NULL;
    gzFile fp = Z_NULL;
    Long size;

    fp = gzopen(path, "rb");
    if (fp == Z_NULL)
    {
        error("could not open %s for reading", path);
        goto failure;
    }


    /* Read level size */
    size = 0;
    gzread(fp, &size, sizeof(size));
    size = ntohl(size);
    if (size != LEVEL_SIZE_X * LEVEL_SIZE_Y * LEVEL_SIZE_Z)
    {
        error("recorded world contains %d blocks; %d expected", size,
              LEVEL_SIZE_X * LEVEL_SIZE_Y * LEVEL_SIZE_Z);
        goto failure;
    }

    /* Allocate and initialize level structure */
    level = malloc(sizeof(Level));
    if (level == NULL) goto failure;
    memset(level, 0, sizeof(level));
    level->size.x     = LEVEL_SIZE_X;
    level->size.y     = LEVEL_SIZE_Y;
    level->size.z     = LEVEL_SIZE_Z;
    level->blocks     = malloc(size);
    level->name       = strdup(LEVEL_NAME);
    level->creator    = strdup(LEVEL_CREATOR);
    level->spawn.x    = LEVEL_SIZE_X/2;
    level->spawn.y    = LEVEL_SIZE_Y - 5;
    level->spawn.z    = LEVEL_SIZE_Z/2;
    level->save_time  = time(NULL);
    if (!level->blocks || !level->name || !level->creator) goto failure;

    /* Read in blocks */
    if (gzread(fp, level->blocks, size) != size)
    {
        error("failed to read block data");
        goto failure;
    }

    gzclose(fp);
    return level;

failure:
    if (fp != Z_NULL) gzclose(fp);
    level_free(level);
    return NULL;
}

bool level_save(Level *level, const char *path)
{
    gzFile fp = Z_NULL;
    Long size;

    /* Open output file for writing */
    fp = gzopen(path, "wb");
    if (fp == Z_NULL)
    {
        error("could not open %s for writing", path);
        goto failure;
    }
    size = htonl(level->size.x * level->size.y * level->size.z);
    if (gzwrite(fp, &size, sizeof(size)) != sizeof(size))
    {
        error("failed to write level size");
        goto failure;
    }
    size = ntohl(size);

    /* Write out blocks */
    if (gzwrite(fp, level->blocks, size) != size)
    {
        error("failed to write block data");
        goto failure;
    }

    level->dirty     = false;
    level->save_time = time(NULL);
    gzclose(fp);
    return true;

failure:
    if (fp != Z_NULL) gzclose(fp);
    return false;
}

Type level_get_block(const Level *level, int x, int y, int z)
{
    if (!level_index_valid(level, x, y, z)) return 0;
    return level->blocks[idx(level, x, y, z)];
}

Type level_set_block(Level *level, int x, int y, int z, Type new_t /* ,
                     block_update_cb *on_update */ )
{
    if (!level_index_valid(level, x, y, z))
    {
        warn("invalid level index %d,%d,%d", x, y, z);
        return new_t;
    }
    else
    {
        size_t i = idx(level, x, y, z);
        Type old_t = level->blocks[i];
        if (old_t != new_t)
        {
            /* (*on_update)(x, y, z, old_t, new_t); */
            level->blocks[i] = new_t;
            level->dirty     = true;
        }
        return old_t;
    }
}

void level_tick(Level *level)
{
    ++level->tick_count;
}
