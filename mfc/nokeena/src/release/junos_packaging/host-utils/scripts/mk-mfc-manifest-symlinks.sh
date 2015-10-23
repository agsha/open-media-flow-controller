#!/bin/sh
#set -x

. $PKGVARTOPDIR/pkgvars

# validate the input prefix and dir
if [ $# -ne 4 ]; then
   echo $0 prefix srcdir manifest pkg.symlinks
   exit 1
fi

if [ ! -d $1/$2 ]; then
   echo $1/$2 is not a directory 
   echo $0 prefix dir
   exit 1
fi
prefix=$1
srcdir=$2
manifest=$3
symlinks=$4

> $manifest
> $symlinks

echo "pkg/manifest uid=0 gid=0 mode=444" >> $manifest
echo "pkg/manifest.sha1 uid=0 gid=0 mode=444" >> $manifest
echo "pkg/manifest.sig uid=0 gid=0 mode=444" >> $manifest
echo "pkg/manifest.certs uid=0 gid=0 mode=444" >> $manifest

# MFC Image Components :
# elilo.efi
# elilo.conf
# initrd.img
# mfcrootfs.img
# rc.sysinit
# vmlinuz0
# dhclient.conf
# dhclient-exit-hooks
# juniper-banner.msg 
# version.txt

# manifest
# $srcdir/elilo.efi sha1=aaa uid=0 gid=0 mode=444
# $srcdir/elilo.conf sha1=aaa uid=0 gid=0 mode=444
# $srcdir/initrd.img sha1=aaa uid=0 gid=0 mode=444
# $srcdir/mfcrootfs.img sha1=aaa uid=0 gid=0 mode=444
# $srcdir/rc.sysinit sha1=aaa uid=0 gid=0 mode=444
# $srcdir/vmlinuz0 sha1=aaa uid=0 gid=0 mode=444
# $srcdir/dhclient.conf sha1=aaa uid=0 gid=0 mode=444
# $srcdir/dhclient-exit-hooks sha1=aaa uid=0 gid=0 mode=444
# $srcdir/juniper-banner.msg sha1=aaa uid=0 gid=0 mode=444
# $srcdir/version.txt sha1=aaa uid=0 gid=0 mode=444

# symlinks
# /packages/mnt//$srcdir/elilo.efi  /$tftproot/$PACKAGENAME/elilo.efi
# /packages/mnt//$srcdir/elilo.conf  /$tftproot/$PACKAGENAME/elilo.conf
# /packages/mnt//$srcdir/initrd.img  /$tftproot/$PACKAGENAME/initrd.img
# /packages/mnt//$srcdir/mfcrootfs.img  /$tftproot/$PACKAGENAME/mfcrootfs.img
# /packages/mnt//$srcdir/rc.sysinit  /$tftproot/$PACKAGENAME/rc.sysinit
# /packages/mnt//$srcdir/vmlinuz0  /$tftproot/$PACKAGENAME/vmlinuz0
# /packages/mnt//$srcdir/dhclient.conf  /$tftproot/$PACKAGENAME/dhclient.conf
# /packages/mnt//$srcdir/dhclient-exit-hooks  /$tftproot/$PACKAGENAME/dhclient-exit-hooks
# /packages/mnt//$srcdir/juniper-banner.msg  /$tftproot/$PACKAGENAME/juniper-banner.msg
# /packages/mnt//$srcdir/version.txt  /$tftproot/$PACKAGENAME/version.txt


pre=/packages/mnt/$PKGNAME

# find the type f (regular) files
files=`find $prefix/$srcdir -type f`
eliloe=
eliloc=
initrd=
mfcrootfs=
rcsysinit=
vmlinuz=
dhclientc=
dhclientex=
juniperbanner=
versiontxt=

for file in $files; do
    name=`basename $file`
    echo Found ${name}
    linkname=
    case $name in
    dhclient.c*) dhclientc=:
        linkname=$name
        # Optional ?
        ;;
    dhclient-e*) dhclientex=:
        linkname=$name
        # Optional ?
        ;;
    elilo.c*) eliloc=:
        linkname=$name
        ;;
    elilo.e*) eliloe=:
        linkname=$name
        ;;
    filesystem.rootfs*) rootfs=:
        linkname=$name
        ;;
    initrd*)  initrd=:
        linkname=$name
        ;;
    juniper*) juniperbanner=:
        linkname=$name
        ;;
    mfcrootfs*) rootfs=:
        linkname=$name
        ;;
    pkgfilename*) pkgfilenametxt=:
        linkname=$name
        # Optional ?
        ;;
    rc.sys*) rcsysinit=:
        linkname=$name
        # Optional ?
        ;;
    version*) versiontxt=:
        linkname=$name
        ;;
    vmlinuz*) vmlinuz=:
        linkname=$name
        ;;
    esac
    if [ -z "${linkname}" ] ; then
      echo Unexpected: $file
      continue
    fi

    # manifest entry
    echo -n "$srcdir/$name sha1=" >> $manifest
    echo -n `$sha1 $prefix/$srcdir/$name` >> $manifest
    echo " uid=0 gid=0 mode=444" >> $manifest

    # symlinks
    # Package-specific link
    echo "$pre/$srcdir/$name $SYMLINKDIR/$linkname" >> $symlinks

done
MISSING=
# Check for required items
for i in initrd vmlinuz eliloe eliloc rootfs juniperbanner versiontxt 
do
  V=`eval echo '$'${i}`
  if [ -z "${V}" ] ; then
    MISSING="${MISSING} ${i}"
    echo ERROR, did not find a ${i} file
  fi
done
[ -z "${MISSING}" ] && exit 0
exit 1
