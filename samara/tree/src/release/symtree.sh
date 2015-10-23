#!/bin/sh

#
# 
#
#

# This script makes a copy of a tree of files using symlinks to the files.


PATH=/usr/bin:/bin:/usr/sbin:/sbin
export PATH

usage()
{
    echo "usage: $0 src_tree dest_tree"
    exit 1
}

if [ $# != 2 ]; then
    usage
fi

SRC_TREE=$1
DEST_TREE=$2

if [ ! -d ${SRC_TREE} ]; then
    echo "Invalid source tree: $SRC_TREE"
    exit 1
fi

if [ ! -d ${DEST_TREE} ]; then
    echo "Invalid dest tree: $DEST_TREE"
    exit 1
fi


pushd . > /dev/null

cd $SRC_TREE
FQ_SRC_TREE=`pwd`
echo "Finding files under ${FQ_SRC_TREE}"
fl=`find \( -name CVS -o -name .svn \) -prune -o -type f -print | sed 's,^\./,,'`

popd > /dev/null

cd $DEST_TREE
for fn in $fl; do
##    echo "fn= $fn"
    dn=`dirname $fn`
    if [ ! -d $dn ]; then
        echo "Making dir $dn"
        mkdir -p $dn
    fi
    ln -sf $FQ_SRC_TREE/$fn $fn
done
