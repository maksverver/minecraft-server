#ifndef HOOKS_H_INCLUDED
#define HOOKS_H_INCLUDED

#include "level.h"
#include "events.h"
#include <stdbool.h>

/* Called when a player requests to set block at x/y/z to client type t.

   Hook should returns the server block type to set the block to, or -1 to
   prevent updating the target block.
*/
int hook_authorize_update( const Level *level, int x, int y, int z,
                           Type old_t, Type new_t );

/* Called to map server block types to client block types. */
Type hook_client_block_type(Type t);

/* Callback for each event. Used to propagate changes on updates etc. */
void hook_on_event(const Level *level, Event *event);

#endif /* ndef HOOKS_H_INCLUDED */
