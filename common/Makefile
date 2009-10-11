include ../base.mk

OBJS=gzip.o logging.o protocol.o hexdump.o heap.o timeval.o

all: common.a

common.a: $(OBJS)
	ar crs $@ $(OBJS)

clean:
	rm -f $(OBJS)

distclean: clean
	rm -f common.a
