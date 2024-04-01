#!/bin/sh

VERSION=`grep "version:" meson.build | head -1 | awk '{print $2}' | tr -d "',"`

sed -i -E -e '/^#define[ \t]+PACKAGE_VERSION/c\'\
"#define PACKAGE_VERSION \"$VERSION\"" \
windows/config_windows.h macos/config_macos.h
