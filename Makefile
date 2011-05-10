all:
	$(MAKE) -C src all
	$(MAKE) -C test all

test:
	$(MAKE) -C test all

install:
	$(MAKE) -C src install
	$(MAKE) -C man install
	$(MAKE) -C meta install

uninstall:
	$(MAKE) -C src uninstall
	$(MAKE) -C man uninstall
	$(MAKE) -C meta uninstall

clean:
	$(MAKE) -C src clean
	$(MAKE) -C test clean

distclean:
	$(MAKE) -C src distclean
	$(MAKE) -C test distclean

.PHONY: all test install uninstall clean distclean
