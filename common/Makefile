include ../base.mk

OBJS=gzip.o heap.o hexdump.o level.o logging.o protocol.o timeval.o

all: common.a

common.a: $(OBJS)
	ar crs $@ $(OBJS)

clean:
	rm -f $(OBJS)

distclean: clean
	rm -f common.a
