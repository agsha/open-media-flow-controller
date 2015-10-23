#!/bin/sh -

# To use this filter with less, define LESSOPEN:
# export LESSOPEN="|/usr/bin/lesspipe.sh %s"

# LESSOPEN filters either output nothing (and less uses the original
# file) or output the full text of the processed file on stdout.
#
# This is a simple filter that only looks at files which exist with the
# extension of *.{z,Z,gz,bz2} , and uncompresses them.  It ignores
# .tar.* files, so as not to confuse users who might think less would
# un-tar a file for them.

lesspipe() {
    DECOMPRESSOR=
    case "$1" in
            *.tar.*)     ;;
            *.gz|*.[zZ]) DECOMPRESSOR="gunzip -c" ;;
            *.bz2)       DECOMPRESSOR="bunzip2 -c" ;;
    esac
    if [ ! -z "$DECOMPRESSOR" ] ; then
        $DECOMPRESSOR -- "$1" ;
    fi
}

if [ ! -z "$1" -a ! "$1" = "-" -a -f "$1" ] ; then
        lesspipe "$1" 2> /dev/null
fi
