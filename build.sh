#!/bin/sh
#autoreconf --install

# Thanks, https://github.com/raywill/automake

export AUTOM4TE="autom4te"
export AUTOCONF="autoconf"

# On OSX, it's glibtoolize
LIBTOOLIZE=$(which libtoolize)
if [ "$LIBTOOLIZE" = "" ]; then
    LIBTOOLIZE=$(which glibtoolize)
fi

case "x$1" in
xinit)
        set -x
        aclocal
        $LIBTOOLIZE --force --copy --automake
        autoconf --force
        automake --foreign --copy --add-missing -Woverride
        ;;
xclean)
        echo 'cleaning...'
        make distclean >/dev/null 2>&1
        rm -rf autom4te.cache
        for fn in aclocal.m4 configure config.guess config.sub depcomp install-sh \
                ltmain.sh libtool missing mkinstalldirs config.log config.status Makefile; do
                rm -f $fn
        done

        find . -name Makefile.in -exec rm -f {} \;
        find . -name Makefile -exec rm -f {} \;
        find . -name .deps -prune -exec rm -rf {} \;
        echo 'done'
        ;;
*)
        ./configure
        ;;
esac
