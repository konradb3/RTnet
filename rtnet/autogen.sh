#!/bin/sh

# Clean up the generated crud
rm -f configure config.log config.guess
rm -f config.status aclocal.m4
rm -f `find . -name 'Makefile.in'`
rm -f `find . -name 'Makefile'`
rm -f config/autoconf/*
mkdir -p config/autoconf

aclocal
libtoolize --force --copy
autoheader
automake --add-missing --copy --gnu -Wall
autoconf -Wall
