PREFIX = /usr/local

all:

install:
	cp faketime.1 "${DESTDIR}${PREFIX}/share/man/man1/faketime.1"
	gzip "${DESTDIR}${PREFIX}/share/man/man1/faketime.1"

uninstall:
	rm -f "${DESTDIR}${PREFIX}/share/man/man1/faketime.1.gz"

.PHONY: all install uninstall
