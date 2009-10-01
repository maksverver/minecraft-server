#include "level.h"
#include "common/logging.h"
#include "common/protocol.h"
#include <assert.h>
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

#define MAX_CLIENTS        32
#define FRAME_USEC     250000   /* microseconds */
#define SAVE_INTERVAL      30   /* seconds */

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

    Player pl;          /* player state */

} Client;

static Level    *g_level;                   /* loaded level */
static int      g_listen_fd;                /* TCP listen socket */
static Client   g_clients[MAX_CLIENTS];     /* client slots */
static int      g_num_clients;              /* number of connected clients */

static void write_client(Client *cl, Byte *buf, int len)
{
    ssize_t written = (cl->output) ? 0 : write(cl->fd, buf, len);

    if (written < 0)
    {
        error("write to client %d failed\n", cl - g_clients);
        written = 0;
    }

    if (written < len)
    {
        Buffer **list = &cl->output;
        while (*list) list = &(*list)->next;
        *list = malloc(sizeof(Buffer) + len - written);
        if (list == NULL)
        {
            error("failed to allocate output buffer of size %d", len);
        }
        else
        {
            (*list)->next = NULL;
            (*list)->data = (Byte*)((*list) + 1);
            (*list)->len  = len - written;
            (*list)->pos  = 0;
            memcpy((*list)->data, buf + written, len - written);
        }
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

static void save_if_dirty()
{
    if (g_level->dirty)
    {
        info("saving level...");
        level_save(g_level, LEVEL_FILE);
    }
}

static void disconnect(Client *cl)
{
    Buffer *list, *next;
    bool loaded = cl->loaded;

    assert(cl->fd);
    for (list = cl->output; list != NULL; list = next)
    {
        next = list->next;
        free(list);
    }
    close(cl->fd);
    memset(cl, 0, sizeof(Client));
    --g_num_clients;

    if (loaded) broadcast_message(PROTO_DISC, cl - g_clients);

    info("disconnected client %d\n", cl - g_clients);

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
    FILE *fp;
    char block[1024];
    int data_len, nmsg, i;

    save_if_dirty();

    fp = fopen(LEVEL_FILE, "rb");
    assert(fp != NULL);
    fseek(fp, 0, SEEK_END);
    data_len = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    nmsg = data_len/1024 + (data_len%1024 ? 1 : 0);
    for (i = 0; i < nmsg; ++i)
    {
        const int block_len = (1024*(i + 1) <= data_len) ? 1024 : data_len%1024;
        const int perc_done = 100*(1024*i + block_len)/data_len;

        memset(block, 0, sizeof(block));
        if (fread(block, block_len, 1, fp) != 1)
        {
            error("couldn't read world data");
            return false;
        }

        send_message(cl, PROTO_DATA, block_len, block, perc_done);
    }
    return true;
}

static void send_initial_position(Client *dest, Client *subj)
{
    send_message(dest, PROTO_PLYC,
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
    send_message(dest, PROTO_PLYU,
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

    if (cl->loaded)
    {
        error("client %d already identified\n", cl - g_clients);
        return;
    }

    strcpy(cl->pl.name, name);

    cl->pl.pos.x  = g_level->spawn.x;
    cl->pl.pos.y  = g_level->spawn.y;
    cl->pl.pos.z  = g_level->spawn.z;
    cl->pl.yaw    = g_level->rot_spawn;
    cl->pl.pitch  = 0.0f;

    send_message(cl, PROTO_HELO, b0, g_level->name, g_level->creator, b1);
    send_message(cl, PROTO_STRT);
    send_world_data(cl);
    send_message(cl, PROTO_SIZE, g_level->size.x, g_level->size.y, g_level->size.z);

    for (subj = &g_clients[0]; subj != &g_clients[MAX_CLIENTS]; ++subj)
    {
        if (subj->loaded)
        {
            send_initial_position(cl, subj);
            send_initial_position(subj, cl);
        }
    }

    cl->loaded = true;

    info("client %d hailed with name `%s'", cl - g_clients, name);
}

static void on_block_update(int x, int y, int z, Type t)
{
    broadcast_message(PROTO_MODN, x, y, z, t);
}

static void handle_player_MODR(Client *cl,
    Short x, Short y, Short z, Byte action, Byte type)
{
    if (x >= 0 && x < g_level->size.x &&
        y >= 0 && y < g_level->size.y &&
        z >= 0 && z < g_level->size.z && (action == 0 || action == 1))
    {
        int v = (action == 0) ? 0 : type;
        level_set_block(g_level, x, y, z, (Type)v, &on_block_update);
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
    cl->pl.pos.y = clip(y/32.0f, 0.0f, g_level->size.y);
    cl->pl.pos.z = clip(z/32.0f, 0.0f, g_level->size.z);
    cl->pl.yaw   = clip(yaw/255.0f, 0.0f, 1.0f);
    cl->pl.pitch = clip(((signed char)pitch)/64.0f, -1.0f, 1.0f);
}

static void handle_player_CHAT(Client *cl, Byte player, char *message)
{
    char buf[STRING_LEN + 1];
    (void)player;  /* ignored */

    snprintf(buf, sizeof(buf), "%s: %s", cl->pl.name, message);
    broadcast_message(PROTO_CHAT, cl - g_clients, buf);
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

static void server_frame()
{
    int c, d;

    /* Simulate a frame */
    level_tick(g_level);

    /* Save level every SAVE_INTERVAL seconds */
    if (g_level->save_time < time(NULL) - SAVE_INTERVAL) save_if_dirty();

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
    printf("%s\n", (g_level->tick_count%2) ? "*tick*" : "\t*tock*");
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

    select(nfds, &readfds, &writefds, NULL, time_left);

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
                ssize_t nwritten = write(cl->fd,
                    cl->output->data + cl->output->pos,
                    cl->output->len - cl->output->pos);

                if (nwritten < 0)
                {
                    error("write to client %d failed\n", cl - g_clients);
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
                }
            }
        }
    }
}

static void wait_for_next_frame(const struct timeval *end)
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
    for (;;)
    {
        struct timeval end;
        gettimeofday(&end, NULL);
        end.tv_sec += (end.tv_usec + FRAME_USEC)/1000000;
        end.tv_usec = (end.tv_usec + FRAME_USEC)%1000000;
        server_frame();
        wait_for_next_frame(&end);
    }
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

int main()
{
    g_level = level_load(LEVEL_FILE);
    if (!g_level) fatal("couldn't load level");
    open_server_socket();
    run_server();
    return 0;
}
