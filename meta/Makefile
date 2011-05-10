INSTALL = install

PREFIX = /usr/local

all:

install:
	$(INSTALL) -dm0755 "${DESTDIR}${PREFIX}/share/doc/faketime/"
	$(INSTALL) -m0644 ../README "${DESTDIR}${PREFIX}/share/doc/faketime/README"
	$(INSTALL) -m0644 Changelog "${DESTDIR}${PREFIX}/share/doc/faketime/Changelog"

uninstall:
	rm -f "${DESTDIR}${PREFIX}/share/doc/faketime/README"
	rm -f "${DESTDIR}${PREFIX}/share/doc/faketime/Changelog"
	rmdir "${DESTDIR}${PREFIX}/share/doc/faketime"

.PHONY: all install uninstall
