#!/bin/bash

set -euo pipefail

# on some Linux distributions gcc -print-file-name=libasan.so will not output
# the location to libasan, but a linker script instead that is a ASCII file and
# looks like this:
# INPUT ( /usr/lib64/libasan.so.6.0.0 )
# to accommodate for this, we check whether the file is a ASCII file and then
# extract the filename in between the braces

ASAN_LOCATION=$(readlink -f $(gcc -print-file-name=libasan.so))

if [[ $(file $ASAN_LOCATION) =~ "ASCII" ]]; then
    sed 's/.*(\(.*\))/\1/' $ASAN_LOCATION | tr -d '[:blank:]'
else
    echo $ASAN_LOCATION
fi
