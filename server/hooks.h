#ifndef HOOKS_H_INCLUDED
#define HOOKS_H_INCLUDED

#include "common/level.h"
#include "common/blocks.h"
#include "events.h"
#include <stdbool.h>

/* Called when a player requests to set block at x/y/z to client type t.

   Hook should returns the server block type to set the block to, or -1 to
   prevent updating the target block.
*/
int hook_authorize_update( const Level *level, const Player *player,
                           int x, int y, int z, Type old_t, Type new_t );

/* Called to map server block types to client block types. */
Type hook_client_block_type(Type t);

/* Callback for each event. Used to propagate changes on updates etc. */
void hook_on_event(const Level *level, Event *event);

/* Callback for each chat messages.

   Return 0 if no messages are to be sent, 1 to reply to the sender only, or 2
   to broadcast the message to all players. */
int hook_on_chat(Player *player, const char *in, char *out, size_t out_size);

#endif /* ndef HOOKS_H_INCLUDED */
