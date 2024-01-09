#!/bin/sh

VERSION=`cat VERSION_LIBUPNPP`

sed -i -E -e '/^#define[ \t]+PACKAGE_VERSION/c\'\
"#define PACKAGE_VERSION \"$VERSION\"" \
-e '/^#define[ \t]+PACKAGE_STRING/c\'\
"#define PACKAGE_STRING \"libupnpp $VERSION\"" \
windows/config_windows.h macos/config_macos.h

