#!/bin/sh

aclocal
#libtoolize --force --copy
autoheader
automake --add-missing --copy --gnu -Wall
autoconf -Wall
