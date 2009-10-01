include ../base.mk

OBJS=gzip.o logging.o protocol.o hexdump.o

all: common.a

common.a: $(OBJS)
	ar crs $@ $(OBJS)

clean:
	rm -f $(OBJS)

distclean: clean
