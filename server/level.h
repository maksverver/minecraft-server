#ifndef LEVEL_H_INCLUDED
#define LEVEL_H_INCLUDED

#include <time.h>
#include <stdbool.h>
#include "common/protocol.h"  /* for STRING_LEN */

#define LEVEL_SIZE_X        256
#define LEVEL_SIZE_Y         64
#define LEVEL_SIZE_Z        256
#define LEVEL_FILE      "world.gz"
#define LEVEL_NAME      "Level Name Goes Here"
#define LEVEL_CREATOR   "Level Creator Goes Here"

/* The six principal directions: */
extern const int DX[6], DY[6], DZ[6];

typedef unsigned char Type;

typedef struct Vec3f
{
    float x, y, z;
} Vec3f;

typedef struct Vec3i
{
    int x, y, z;
} Vec3i;

/* Modeled after official Level.java */
typedef struct Level
{
    Vec3i           size;               /* width, height, depth */
    Type            *blocks;            /* blocks */
    char            *name;              /* level name  */
    char            *creator;           /* level creator/description */
    time_t          create_time;        /* creation time */
    Vec3i           spawn;              /* spawn point */
    float           rot_spawn;          /* spawn yaw */
    unsigned        tick_count;         /* total number of simulated frames */
    bool            dirty;              /* modified since last save? */
    time_t          save_time;          /* last save time */
} Level;

/* Player state */
typedef struct Player
{
    char name[STRING_LEN + 1];
    Vec3f pos;
    float yaw, pitch;
} Player;

/*
typedef void (block_update_cb)(int x, int y, int z, Type old_t, Type new_t);
*/

/* Level functions */
void level_free(Level *level);
Level *level_load(const char *path);
bool valid_index(const Level *level, int x, int y, int z);
bool level_save(Level *level, const char *path);
Type level_get_block(const Level *level, int x, int y, int z);
Type level_set_block(Level *level, int x, int y, int z, Type t /*,
                     block_update_cb *on_update */ );
void level_tick(Level *level);


#endif /* def LEVEL_H_INCLUDED */
