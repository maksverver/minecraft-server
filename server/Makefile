include ../base.mk
CFLAGS+=-I..
LDFLAGS+=-lz
LDLIBS+=../common/common.a

SERVER_OBJS=events.o hooks.o level.o server.o

all: server

server: $(SERVER_OBJS)	
	$(CC) $(LDFLAGS) -o server $(SERVER_OBJS) $(LDLIBS)

clean:
	rm -f *.o

distclean: clean
	rm -f server

.PHONY: all clean distclean
