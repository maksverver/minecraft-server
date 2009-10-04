#ifndef HOOKS_H_INCLUDED
#define HOOKS_H_INCLUDED

#include "level.h"
#include <stdbool.h>

/* Called when a player requests to set block at x/y/z to type t.

   Hook should returns the server block type to set the block to, or -1 to
   prevent updating the target block.
*/
int hook_authorize_update(Level *level, int x, int y, int z, Type t);

/* Called to map server block types to client block types. */
Type hook_client_block_type(Type t);

/* Called when a block's neighbour is updated.

   Returns the new server block type for this block or -1 to prevent updating
   the block. */
int hook_on_neighbour_change( Level *level, int x, int y, int z, Type t,
                              int dir, Type old_t, Type new_t );

#endif /* ndef HOOKS_H_INCLUDED */
