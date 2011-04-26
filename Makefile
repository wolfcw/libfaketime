all:
	$(MAKE) -C src all
	$(MAKE) -C test all

test:
	$(MAKE) -C test all

install:
	$(MAKE) -C src install

uninstall:
	$(MAKE) -C src uninstall

clean:
	$(MAKE) -C src clean
	$(MAKE) -C test clean

distclean:
	$(MAKE) -C src distclean
	$(MAKE) -C test distclean

.PHONY: all test install uninstall clean distclean
