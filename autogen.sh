#!/bin/sh

aclocal
libtoolize --copy --force
automake --add-missing --copy
autoconf
