#!/bin/sh

#
# (C) Copyright 2013-2015 Juniper Networks, Inc. All Rights reserved.
# (C) Copyright 2002-2006 Tall Maple Systems, Inc.  
# All rights reserved.
# The contents of this file are subject to the terms of the MIT License.
# You may not use this file except in compliance with the License.
# See the file License.txt at the top of this source tree.
# You can obtain a copy of the license at https://opensource.org/licenses/MIT
#

PATH=/usr/bin:/bin:/usr/sbin:/sbin
export PATH

umask 0022


usage()
{
    echo "usage: $0 [uni|smp]"
    echo ""
    exit 1
}

if [ $# != 1 ] ; then
    usage
fi

NEW_KERNEL=$1

# Get which /boot paritition we did not start from.
eval `/sbin/aiget.sh`
if [ ${AIG_THIS_BOOT_ID} -eq 1 ]; then
    mlabel=BOOT_2
else
    mlabel=BOOT_1
fi

# Fixup the /etc/image_layout.sh 
mount -oremount,rw /
cat /etc/image_layout.sh | sed s'/^IL_KERNEL_TYPE=.*/IL_KERNEL_TYPE='${NEW_KERNEL}'/' > /etc/image_layout.sh.new
mv /etc/image_layout.sh.new /etc/image_layout.sh

# Change the symlinks on this location
. /etc/image_layout.sh
mount -oremount,rw /boot
cd /boot

# Is it "msdos"?  That doesn't support symlinks, so copy instead
fstype=$(stat -c '%T' -f /boot/)
if [ "${fstype}" = "msdos" ]; then
    lnk_cmd="cp -p"
else
    lnk_cmd="ln -sf"
fi

${lnk_cmd} vmlinuz-${IL_KERNEL_TYPE} vmlinuz
${lnk_cmd} System.map-${IL_KERNEL_TYPE} System.map
if [ -e config-${IL_KERNEL_TYPE} ]; then
    ${lnk_cmd} config-${IL_KERNEL_TYPE} config
fi
if [ -e fdt-${IL_KERNEL_TYPE} ]; then
    ${lnk_cmd} fdt-${IL_KERNEL_TYPE} fdt
fi

cd /
sync
mount -oremount,ro /boot
mount -oremount,ro /

# Change the symlinks on the other location
mount LABEL=${mlabel} /mnt
cd /mnt
${lnk_cmd} vmlinuz-${IL_KERNEL_TYPE} vmlinuz
${lnk_cmd} System.map-${IL_KERNEL_TYPE} System.map
if [ -e config-${IL_KERNEL_TYPE} ]; then
    ${lnk_cmd} config-${IL_KERNEL_TYPE} config
fi
if [ -e fdt-${IL_KERNEL_TYPE} ]; then
    ${lnk_cmd} fdt-${IL_KERNEL_TYPE} fdt
fi

cd /
sync
umount /mnt

exit 0
