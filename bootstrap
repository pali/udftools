#!/bin/sh
find . \( -name autom4te.cache -o -name libtool \) -exec rm -r {} \;
aclocal
libtoolize --force --copy
autoheader
automake --add-missing --copy
autoconf
