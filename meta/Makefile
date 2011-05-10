PREFIX = /usr/local

all:

install:
	-mkdir -p "${DESTDIR}${PREFIX}/share/doc/faketime"
	cp ../README "${DESTDIR}${PREFIX}/share/doc/faketime/README"
	cp Changelog "${DESTDIR}${PREFIX}/share/doc/faketime/Changelog"

uninstall:
	rm -f "${DESTDIR}${PREFIX}/share/doc/faketime/README"
	rm -f "${DESTDIR}${PREFIX}/share/doc/faketime/Changelog"
	rmdir "${DESTDIR}${PREFIX}/share/doc/faketime"

.PHONY: all install uninstall
