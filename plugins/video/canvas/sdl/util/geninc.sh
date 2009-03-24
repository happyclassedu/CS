#!/bin/sh

if test "$1" = "-p"; then
    while test -n "$2"; do
        ../util/img2c `echo $2 | sed y/\./_/` <$2 >>../util/img.inc
	shift
    done
elif test -n "$1"; then
    echo "/* GENERATED BY geninc.sh */" >../util/img.inc
    ../util/geninc.sh -p $@
else
    cd ../img
    ../util/geninc.sh *
    cd ../util
fi