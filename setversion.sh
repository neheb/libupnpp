#!/bin/sh

VERSION=`cat VERSION_LIBUPNPP`

sed -i -E -e '/^#define[ \t]+PACKAGE_VERSION/c\'\
"#define PACKAGE_VERSION \"$VERSION\"" \
windows/config_windows.h macos/config_macos.h
