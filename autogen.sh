#!/bin/sh
set -e
aclocal
autoheader
autoconf
automake --add-missing --foreign
./configure "$@"