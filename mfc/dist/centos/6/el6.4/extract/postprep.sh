#!/bin/sh

PATH=/usr/bin:/bin:/usr/sbin:/sbin
export PATH

forced_name=${1}


# Rename the kernel directory, only if forced
if [ ! -z "${forced_name}" ]; then
    kfn=`ls -d kernel-* | head -1`
    new_kfn=`echo kernel-${1}`
    if [ -z "${kfn}" -o -z "${new_kfn}" ]; then
        exit 1
    fi
    mv "${kfn}" "${new_kfn}" || exit 1
fi

# Rename the linux directory inside it
cd kernel-2.6.*
rm -rf vanilla vanilla-*
lfn=`ls -d linux-* | head -1`
if [ -z "${forced_name}" ]; then
    new_lfn=`echo ${lfn} | sed 's/\(linux-.*\)\.[^.]*$/\1/'`
else
    new_lfn=`echo linux-${1}`
fi
if [ -z "${lfn}" -o -z "${new_lfn}" ]; then
    exit 1
fi
mv "${lfn}" "${new_lfn}" || exit 1

exit 0
