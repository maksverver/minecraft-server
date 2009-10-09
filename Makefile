all:
	make -C common all
	make -C server all

clean:
	make -C common clean
	make -C server clean

distclean:
	make -C common distclean
	make -C server distclean

.PHONY: all clean distclean
