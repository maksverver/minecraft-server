#include "events.h"
#include "hooks.h"
#include "common/gzip.h"
#include "common/heap.h"
#include "common/level.h"
#include "common/logging.h"
#include "common/protocol.h"
#include "common/timeval.h"
#include <assert.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>

#define MAX_CLIENTS           32
#define FRAME_USEC        250000    /* microseconds */
#define SAVE_INTERVAL        120    /* seconds */

#define MIN_BUFFER_SIZE  4000


typedef struct Buffer
{
    struct Buffer *next;
    Byte *data;
    int len, pos;
} Buffer;

typedef struct Client
{
    int fd;             /* file descriptor for socket; >0 if connected */
    bool loaded;        /* true after the client has been sent the world map */

    Byte buf[4096];     /* incoming data buffer */
    int buf_pos;        /* incoming data buffer position */
    Buffer *output;     /* pending output buffers */
    Buffer *output_end; /* last output buffer */

    Player pl;          /* player state */

} Client;


static Level    *g_level;                   /* loaded level */
static int      g_listen_fd;                /* TCP listen socket */
static Client   g_clients[MAX_CLIENTS];     /* client slots */
static int      g_num_clients;              /* number of connected clients */

static volatile bool g_quit_requested;

static void write_client(Client *cl, Byte *buf, int len)
{
    ssize_t written = (cl->output) ? 0 : send(cl->fd, buf, len, MSG_NOSIGNAL);

    if (written < 0)
    {
        warn("write to client %d failed", cl - g_clients);
        written = 0;
    }

    if (written < len)
    {
        Buffer *out = cl->output_end;

        buf += written;
        len -= written;

        if (!out || out->len + len > MIN_BUFFER_SIZE)
        {
            /* Allocate new buffer */
            out = malloc(sizeof(Buffer) +
                         (len < MIN_BUFFER_SIZE ? MIN_BUFFER_SIZE : len));
            if (out == NULL)
            {
                error("failed to allocate output buffer for %d bytes", len);
                return;
            }

            out->next = NULL;
            out->data = (Byte*)(out + 1);
            out->len  = 0;
            out->pos  = 0;

            if (!cl->output)
                cl->output = cl->output_end = out;
            else
                cl->output_end = cl->output_end->next = out;
        }

        /* Append to buffer */
        memcpy(out->data + out->len, buf, len);
        out->len += len;
    }
}

static void send_message(Client *cl, int type, ...)
{
    Byte buf[MAX_MESSAGE];
    int len;
    va_list ap;

    va_start(ap, type);
    len = proto_msg_vbuild(type, ap, buf);
    assert(len < sizeof(buf));
    va_end(ap);

    write_client(cl, buf, len);
}

static void broadcast_message(int type, ...)
{
    Byte buf[MAX_MESSAGE];
    int len, c;
    va_list ap;

    va_start(ap, type);
    len = proto_msg_vbuild(type, ap, buf);
    assert(len < sizeof(buf));
    va_end(ap);

    for (c = 0; c < MAX_CLIENTS; ++c)
    {
        if (g_clients[c].loaded)
            write_client(&g_clients[c], buf, len);
    }
}

static void server_message(const char *fmt, ...)
{
    char buf[STRING_LEN + 1];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    broadcast_message(PROTO_CHAT, -1, buf);
}

static void save_if_dirty()
{
    if (event_queue_is_dirty())
    {
        info("saving event queue");
        event_queue_write(EVENT_FILE);
    }

    if (g_level->dirty)
    {
        info("saving level");
        level_save(g_level, LEVEL_FILE);
    }
}

static void disconnect(Client *cl)
{
    Buffer *list, *next;

    assert(cl->fd);

    if (cl->loaded)
    {
        broadcast_message(PROTO_DISC, cl - g_clients);
        server_message("%s left the game", cl->pl.name);
    }

    for (list = cl->output; list != NULL; list = next)
    {
        next = list->next;
        free(list);
    }
    close(cl->fd);

    memset(cl, 0, sizeof(Client));
    --g_num_clients;

    info("disconnected client %d", cl - g_clients);

    /* If last client exits, save the level immediately, since it will not be
       modified until a new client connects when we need to save it anyway. */
    if (g_num_clients == 0) save_if_dirty();
}

static void read_byte(Byte **buf, int *len, Byte *out)
{
    *out = (*buf)[0];
    *buf += 1;
    *len -= 1;
}

static void read_short(Byte **buf, int *len, Short *out)
{
    *out = ((*buf)[0] << 8) | ((*buf)[1] << 0);
    *buf += 2;
    *len -= 2;
}

static void read_text(Byte **buf, int *len, char *out)
{
    int n = STRING_LEN;
    memcpy(out, *buf, n);
    *buf += n;
    *len -= n;
    while (n > 0 && out[n - 1] == ' ') --n;
    out[n] = '\0';
}

static bool send_world_data(Client *cl)
{
    size_t  input_size;
    char    *input;
    size_t  level_size;
    Type    *client_blocks;
    int     x, y, z;
    size_t  data_size;
    char    *data;
    int     nmsg, i;

    /* Allocate client data */
    level_size = g_level->size.x * g_level->size.y * g_level->size.z;
    input_size = 4 + level_size;
    input = malloc(input_size);
    if (input == NULL)
    {
        error("couldn't allocate memory for client data");
        return false;
    }

    /* Create client data */
    input[0] = (level_size >> 24);
    input[1] = (level_size >> 16);
    input[2] = (level_size >>  8);
    input[3] = (level_size >>  0);
    client_blocks = (Type*)(input + 4);
    for (y = 0; y < g_level->size.y; ++y)
    {
        for (z = 0; z < g_level->size.z; ++z)
        {
            for (x = 0; x < g_level->size.x; ++x)
            {
                Type t = level_get_block(g_level, x, y, z);
                t = hook_client_block_type(t);
                client_blocks[x + g_level->size.x*(z + g_level->size.z*y)] = t;
            }
        }
    }

    /* Compress block data */
    data = gzip_compress(input, input_size, &data_size);
    free(input);
    if (data == NULL)
    {
        error("couldn't compress client block data");
        return false;
    }

    /* Send client block data in a number of separate chunks */
    nmsg = (data_size + 1023)/1024;
    for (i = 0; i < nmsg; ++i)
    {
        char block_data[1024];
        int block_len = data_size - i*1024;
        if (block_len >= 1024) block_len = 1024;
        memcpy(block_data, data + 1024*i, block_len);
        memset(block_data + block_len, 0, 1024 - block_len);
        send_message(cl, PROTO_DATA, block_len, block_data, 100*(i+1)/nmsg);
    }
    free(data);

    return true;
}

static void send_initial_position(Client *dest, Client *subj)
{
    send_message( dest, PROTO_PLYC,
                  (dest == subj) ? 255 : (subj - g_clients),
                  subj->pl.name,
                  (int)(32*subj->pl.pos.x),
                  (int)(32*subj->pl.pos.y),
                  (int)(32*subj->pl.pos.z),
                  (int)(255*subj->pl.yaw),
                  (int)(64*subj->pl.pitch)&0xff );
}

static void send_updated_position(Client *dest, Client *subj)
{
    send_message( dest, PROTO_PLYU,
                  (dest == subj) ? 255 : (subj - g_clients),
                  (int)(32*subj->pl.pos.x),
                  (int)(32*subj->pl.pos.y),
                  (int)(32*subj->pl.pos.z),
                  (int)(255*subj->pl.yaw),
                  (int)(64*subj->pl.pitch)&0xff );
}

static void handle_player_HELO(Client *cl,
    Byte b0, char *name, char *s1, Byte b1)
{
    Client *subj;

    (void)b1;  /* unused (purpose unknown) */

    if (cl->loaded)
    {
        error("client %d already identified", cl - g_clients);
        return;
    }

    strcpy(cl->pl.name, name);

    cl->pl.pos.x    = g_level->spawn.x;
    cl->pl.pos.y    = g_level->spawn.y;
    cl->pl.pos.z    = g_level->spawn.z;
    cl->pl.yaw      = g_level->rot_spawn;
    cl->pl.pitch    = 0.0f;
    cl->pl.tileset  = 0;
    cl->pl.admin    = false;

    send_message(cl, PROTO_HELO, b0, g_level->name, g_level->creator, 100);
    send_message(cl, PROTO_STRT);
    send_world_data(cl);
    send_message(cl, PROTO_SIZE, g_level->size.x, g_level->size.y, g_level->size.z);

    /* Send other player's positions to player, and vice versa */
    server_message("%s joined the game", name);
    for (subj = &g_clients[0]; subj != &g_clients[MAX_CLIENTS]; ++subj)
    {
        if (subj->loaded)
        {
            send_initial_position(cl, subj);
            send_initial_position(subj, cl);
        }
    }

    send_initial_position(cl, cl);
    cl->loaded = true;

    info("client %d hailed with name `%s'", cl - g_clients, name);
}

bool server_update_block( int x, int y, int z, Type new_t,
                          const struct timeval *event_delay )
{
    bool res = false;  /* have clients been notified? */

    /* Try to update level: */
    Type old_t = level_set_block(g_level, x, y, z, new_t);
    if (old_t != new_t)
    {
        Type cl_old_t = hook_client_block_type(old_t);
        Type cl_new_t = hook_client_block_type(new_t);

        /* Notify clients of update: */
        if (cl_old_t != cl_new_t)
        {
            broadcast_message(PROTO_MODN, x, y, z, cl_new_t);
            res = true;
        }

        /* Handle implicit update event: */
        if (event_delay != NULL)
        {
            Event ev;
            tv_now(&ev.base.time);
            ev.base.type = EVENT_TYPE_UPDATE;
            ev.update_event.x     = x;
            ev.update_event.y     = y;
            ev.update_event.z     = z;
            ev.update_event.old_t = old_t;
            ev.update_event.new_t = new_t;
            if (event_delay->tv_sec == 0 && event_delay->tv_usec == 0)
            {
                hook_on_event(g_level, &ev);
            }
            else
            {
                tv_add_tv(&ev.base.time, event_delay);
                event_push(&ev);
            }
        }
    }

    return res;
}

static void handle_player_MODR(Client *cl,
    Short x, Short y, Short z, Byte action, Byte type)
{
    if (level_index_valid(g_level, x, y, z) && (action == 0 || action == 1))
    {
        struct timeval delay = { 0, 0 };
        Type t = level_get_block(g_level, x, y, z);
        int v = hook_authorize_update(g_level, &cl->pl,
                                      x, y, z, t, action ? type : 0);
        if (v < 0 || !server_update_block(x, y, z, v, &delay))
        {
            /* Client may have updated the block locally, so send a notification
               to put the correct type back: */
            broadcast_message(PROTO_MODN, x, y, z, hook_client_block_type(t));
        }
    }
}

static float clip(float i, float a, float b)
{
    return (i < a) ? a : (i > b) ? b : i;
}

static void handle_player_PLYU(Client *cl,
    Byte player, Short x, Short y, Short z, Byte yaw, Byte pitch)
{
    (void)player;  /* unused */
    cl->pl.pos.x = clip(x/32.0f, 0.0f, g_level->size.x);
    cl->pl.pos.y = y/32.0f;  /* don't clip height */
    cl->pl.pos.z = clip(z/32.0f, 0.0f, g_level->size.z);
    cl->pl.yaw   = clip(yaw/255.0f, 0.0f, 1.0f);
    cl->pl.pitch = clip(((signed char)pitch)/64.0f, -1.0f, 1.0f);
}

static void handle_player_CHAT(Client *cl, Byte player, char *message)
{
    char buf[STRING_LEN + 1];

    (void)player;  /* ignored */

    switch (hook_on_chat(&cl->pl, message, buf, sizeof(buf)))
    {
        case 0: break;
        case 1: send_message(cl, PROTO_CHAT, -1, buf); break;
        case 2: broadcast_message(PROTO_CHAT, cl - g_clients, buf); break;
        default: assert(0);
    }
}

static int parse_data(Client *cl, Byte *buf, int len)
{
    Byte b0, b1, b2;
    char t0[STRING_LEN], t1[STRING_LEN];
    Short s0, s1, s2;

    /*
    printf("Input buffer:\n");
    hex_dump(buf, len);
    fflush(stdout);
    */

    while (len > 0)
    {
        int type = buf[0], msg_len;
        if (type < 0 || type >= PROTO_NMSG)
        {
            error("invalid message type: %d", type);
            disconnect(cl);
            return 0;
        }
        msg_len = proto_msg_len(type);
        if (len < msg_len) break;

        --len, ++buf;

        switch (type)
        {
        case PROTO_HELO:
            {
                read_byte(&buf, &len, &b0);
                read_text(&buf, &len,  t0);
                read_text(&buf, &len,  t1);
                read_byte(&buf, &len, &b1);
                handle_player_HELO(cl, b0, t0, t1, b1);
            } break;

        case PROTO_MODR:
            {
                read_short(&buf, &len, &s0);
                read_short(&buf, &len, &s1);
                read_short(&buf, &len, &s2);
                read_byte(&buf, &len, &b0);
                read_byte(&buf, &len, &b1);
                handle_player_MODR(cl, s0, s1, s2, b0, b1);
            } break;

        case PROTO_PLYU:
            {
                read_byte(&buf, &len, &b0);
                read_short(&buf, &len, &s0);
                read_short(&buf, &len, &s1);
                read_short(&buf, &len, &s2);
                read_byte(&buf, &len, &b1);
                read_byte(&buf, &len, &b2);
                handle_player_PLYU(cl, b0, s0, s1, s2, b1, b2);
            } break;

        case PROTO_CHAT:
            {
                read_byte(&buf, &len, &b0);
                read_text(&buf, &len,  t0);
                handle_player_CHAT(cl, b0, t0);
            } break;

        default:
            warn("client message with type %d ignored", type);
            len -= msg_len - 1;
            buf += msg_len - 1;
            break;
        }
    }
    return len;
}

static void server_tick()
{
    int c, d;

    /* Simulate a frame */
    level_tick(g_level);

    /* Send player position updates */
    for (c = 0; c < MAX_CLIENTS; ++c)
    {
        if (g_clients[c].loaded)
        {
            for (d = 0; d < MAX_CLIENTS; ++d)
            {
                if (c != d && g_clients[d].loaded)
                {
                    send_updated_position(&g_clients[c], &g_clients[d]);
                }
            }
        }
    }

    broadcast_message(PROTO_TICK);
}

static void transmit_pending_messages(struct timeval *time_left)
{
    fd_set readfds, writefds;
    int c, nfds;

    FD_ZERO(&readfds);
    FD_ZERO(&writefds);

    FD_SET(g_listen_fd, &readfds);
    nfds = g_listen_fd + 1;

    for (c = 0; c < MAX_CLIENTS; ++c)
    {
        int fd = g_clients[c].fd;
        if (fd > 0)
        {
            FD_SET(fd, &readfds);
            if (g_clients[c].output) FD_SET(fd, &writefds);
            if (fd >= nfds) nfds = fd + 1;
        }
    }

    nfds = select(nfds, &readfds, &writefds, NULL, time_left);
    if (nfds < 0)
    {
        error("select() failed");
        return;
    }

    if (FD_ISSET(g_listen_fd, &readfds))
    {
        struct sockaddr_in sa;
        socklen_t sl = sizeof(sa);
        long nbio = 1;
        int fd;

        fd = accept(g_listen_fd, (struct sockaddr*)&sa, &sl);
        if (fd < 0)
        {
            error("couldn't accept connection");
        }
        else
        {
            assert(sl == sizeof(sa));

            if (ioctl(fd, FIONBIO, &nbio) != 0)
                error("failed to select non-blocking I/O");

            for (c = 0; c < MAX_CLIENTS; ++c) if (!g_clients[c].fd) break;
            if (c == MAX_CLIENTS)
            {
                warn("closing connection from %s:%d because server is full",
                    inet_ntoa(sa.sin_addr), ntohs(sa.sin_port) );

                close(fd);
            }
            else
            {
                g_clients[c].fd = fd;
                ++g_num_clients;
                info("accepted connection from %s:%d in client slot %d",
                    inet_ntoa(sa.sin_addr), ntohs(sa.sin_port), c );
            }
        }
    }

    for (c = 0; c < MAX_CLIENTS; ++c)
    {
        Client * const cl = &g_clients[c];

        if (FD_ISSET(cl->fd, &readfds))
        {
            int left;
            ssize_t nread = read(cl->fd, cl->buf + cl->buf_pos,
                                        sizeof(cl->buf) - cl->buf_pos);
            if (nread <= 0)
            {
                warn("read from client %d failed", c);
                disconnect(cl);
            }
            else
            {
                cl->buf_pos += nread;
                assert(cl->buf_pos > 0 && cl->buf_pos <= sizeof(cl->buf));
                left = parse_data(cl, cl->buf, cl->buf_pos);
                if (cl->fd)  /* NB: client may have been disconnected! */
                {
                    memmove(cl->buf, cl->buf + cl->buf_pos - left, left);
                    cl->buf_pos = left;
                }
            }
        }

        if (FD_ISSET(cl->fd, &writefds))
        {
            while (cl->output)
            {
                ssize_t nwritten = send(cl->fd,
                    cl->output->data + cl->output->pos,
                    cl->output->len - cl->output->pos, MSG_NOSIGNAL);

                if (nwritten < 0)
                {
                    warn("write to client %d failed", cl - g_clients);
                    nwritten = 0;
                }

                if (nwritten < cl->output->len - cl->output->pos)
                {
                    cl->output->pos += nwritten;
                    break;
                }
                else
                {
                    Buffer *next = cl->output->next;
                    free(cl->output);
                    cl->output = next;
                    if (!next) cl->output_end = NULL;
                }
            }
        }
    }
}

static void wait_for_next_event(const struct timeval *end)
{
    for (;;)
    {
        struct timeval now, left;
        int usec_left;

        gettimeofday(&now, NULL);
        usec_left = 1000000*(end->tv_sec - now.tv_sec) +
                            ((int)end->tv_usec - (int)now.tv_usec);
        if (usec_left < 0) break;

        left.tv_sec  = usec_left/1000000;
        left.tv_usec = usec_left%1000000;

        transmit_pending_messages(&left);
    }
}

static void run_server()
{
    Event event;

    /* Schedule initial tick event */
    tv_now(&event.base.time);
    tv_add_us(&event.base.time, FRAME_USEC);
    event.base.type = EVENT_TYPE_TICK;
    event_push(&event);

    /* Schedule initial save event */
    tv_now(&event.base.time);
    tv_add_s(&event.base.time, SAVE_INTERVAL);
    event.base.type = EVENT_TYPE_SAVE;
    event_push(&event);

    /* Run indefinitely */
    while (!g_quit_requested)
    {
        Event ev;

        wait_for_next_event(&event_peek()->base.time);
        event_pop(&ev);

        switch (ev.base.type)
        {
        case EVENT_TYPE_TICK:
            {
                struct timeval now;

                /* Execute tick */
                server_tick();

                printf( "%s (%d clients; %d events)\n",
                        (g_level->tick_count%2) ? "*tick*    " : "    *tock*",
                        g_num_clients, (int)event_count() );

                /* Schedule next tick event */
                tv_now(&now);
                tv_add_us(&ev.base.time, FRAME_USEC);
                if (tv_cmp(&ev.base.time, &now) < 0)
                {
                    struct timeval d = now;
                    tv_sub_tv(&d, &ev.base.time);
                    warn("tick delayed by %d.%06ds", d.tv_sec, d.tv_usec);
                    ev.base.time = now;
                }
                event_push(&ev);
            }
            break;

        case EVENT_TYPE_SAVE:
            save_if_dirty();

            /* Schedule next save event */
            tv_now(&ev.base.time);
            tv_add_s(&ev.base.time, SAVE_INTERVAL);
            event_push(&ev);
            break;

        default: break;
        }

        hook_on_event(g_level, &ev);
    }
    info("quit requested");
}

static void open_server_socket()
{
    struct sockaddr_in sa;

    g_listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (g_listen_fd < 0) fatal("couldn't create server socket");

    sa.sin_family = AF_INET;
    sa.sin_port   = htons(DEFAULT_PORT);
    sa.sin_addr.s_addr = INADDR_ANY;

    if (bind(g_listen_fd, (struct sockaddr*)&sa, sizeof(sa)) != 0)
        fatal("couldn't bind server socket");

    if (listen(g_listen_fd, 1) != 0)
        fatal("couldn't listen on server socket");

    info("listening on port %d", ntohs(sa.sin_port));
}

static void sigint_handler()
{
    g_quit_requested = true;
}

static void register_signal_handlers()
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = &sigint_handler;
    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGQUIT, &sa, NULL);
}

int main()
{
    g_level = level_load(LEVEL_FILE);
    if (!g_level) fatal("couldn't load level");

    if (!event_queue_read(EVENT_FILE))
        warn("couldn't restore event queue");
    else
        info("%d events restored to event queue", event_count());

    register_signal_handlers();

    open_server_socket();
    run_server();
    save_if_dirty();
    info("exiting");
    return 0;
}
