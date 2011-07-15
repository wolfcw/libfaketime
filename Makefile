all:
	$(MAKE) -C src all
	$(MAKE) -C test all

test:
	$(MAKE) -C test all

install:
	$(MAKE) -C src install
	$(MAKE) -C man install
	$(INSTALL) -dm0755 "${DESTDIR}${PREFIX}/share/doc/faketime/"
	$(INSTALL) -m0644 README "${DESTDIR}${PREFIX}/share/doc/faketime/README"
	$(INSTALL) -m0644 NEWS "${DESTDIR}${PREFIX}/share/doc/faketime/NEWS"

uninstall:
	$(MAKE) -C src uninstall
	$(MAKE) -C man uninstall
	rm -f "${DESTDIR}${PREFIX}/share/doc/faketime/README"
	rm -f "${DESTDIR}${PREFIX}/share/doc/faketime/NEWS"
	rmdir "${DESTDIR}${PREFIX}/share/doc/faketime"

clean:
	$(MAKE) -C src clean
	$(MAKE) -C test clean

distclean:
	$(MAKE) -C src distclean
	$(MAKE) -C test distclean

.PHONY: all test install uninstall clean distclean
