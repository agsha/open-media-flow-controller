#!/bin/sh
#set -x

. $PKGVARTOPDIR/setup.sh

if [ $# -ne 5 ]; then
   echo $0 pkgoutdir pkgname release pkgfile pkgindir
   exit
fi

if [ ! -d $5 ]; then
   echo $5 is not a directory
   echo $0 pkgoutdir pkgname release pkgfile pkgindir
   exit
fi
if [ ! -d $1 ]; then
   echo $1 is not a directory
   echo $0 pkgoutdir pkgname release pkgfile pkgindir
   exit
fi

PKGOUTDIR=$1
PKGNAME=$2
PKGREL=$3
PKGFILE=$4
PKGINDIR=$5
MKISOFS=/usr/bin/mkisofs

# add-option block was copied from src/junos/host-utils/scripts
# ADD_OPTION_BLOCK=$DEV_VEE/src/junos/host-utils/scripts/add-option-block

ADD_OPTION_BLOCK=$PKGSRCTOPDIR/host-utils/scripts/add-option-block
OPTS="fstype=iso9660, compressed, packageName=$PKGNAME, release=$PKGREL"

$MKISOFS -sysid JUNOS -D -quiet -l -r -o ${PKGOUTDIR}/isofs-$PKGNAME $PKGINDIR
$GZIPVN ${PKGOUTDIR}/isofs-$PKGNAME ${PKGOUTDIR}/$PKGFILE
$ADD_OPTION_BLOCK "$OPTS" ${PKGOUTDIR}/$PKGFILE
#echo "This is where the ISO Image is"
#echo ${PKGOUTDIR}
/bin/rm ${PKGOUTDIR}/isofs-$PKGNAME
