#!/bin/sh

#
# (C) Copyright 2013-2015 Juniper Networks, Inc. All Rights reserved.
# (C) Copyright 2002-2012 Tall Maple Systems, Inc.
# All rights reserved.
# The contents of this file are subject to the terms of the MIT License.
# You may not use this file except in compliance with the License.
# See the file License.txt at the top of this source tree.
# You can obtain a copy of the license at https://opensource.org/licenses/MIT
#

#
# This script is run the first time an image is booted, and does any
# post-installation tweaking that needs to be done.
#
# Note that this script is run from rc.sysinit, so there are no daemons running
# Disks are mounted rw when this script is called
#

PATH=/usr/bin:/bin:/usr/sbin:/sbin
export PATH

umask 0022


KEYGEN=/usr/bin/ssh-keygen
RSA1_KEY=/var/tmp/ssh_host_key
RSA_KEY=/var/tmp/ssh_host_rsa_key
DSA_KEY=/var/tmp/ssh_host_dsa_key
SMART_CONF_TEMPLATE=/etc/smartd.conf.template
SMART_CONF=/var/opt/tms/output/smartd.conf
SERVICE_DIR=/etc/rc.d/init.d
USE_WIZARD_PATH=/var/opt/tms/.usewizard

MFG_DB_DIR=/config/mfg
MFG_DB_PATH=${MFG_DB_DIR}/mfdb
MDDBREQ=/opt/tms/bin/mddbreq

. /etc/init.d/functions
. /etc/build_version.sh

#
# As the management database has one semantic version number per module,
# the firstboot script has two semantic version numbers for /var: one
# for upgrades  pertaining to the baseline code, and one for 
# customer-specific upgrades.  The two groups of defines below reflect
# these two upgrade paths.  One set of customer-specific files are
# installed from whichever customer directory we are building out of.
#

PATH_VAR_VERSION_CURR_GENERIC=/var/var_version.sh
PATH_VAR_VERSION_NEW_GENERIC=/etc/var_version.sh
PATH_VAR_UPGRADE_SCRIPT_GENERIC=/sbin/var_upgrade.sh

PATH_VAR_VERSION_CURR_CUSTOMER=/var/var_version_${BUILD_PROD_CUSTOMER_LC}.sh
PATH_VAR_VERSION_NEW_CUSTOMER=/etc/var_version_${BUILD_PROD_CUSTOMER_LC}.sh
PATH_VAR_UPGRADE_SCRIPT_CUSTOMER=/sbin/var_upgrade_${BUILD_PROD_CUSTOMER_LC}.sh

# Set to 1 if a reboot is needed before the system can be fully started
REBOOT_NEEDED=0
export REBOOT_NEEDED
# If a reboot is going to be done, should we re-run firstboot after this reboot
REBOOT_RERUN_FIRSTBOOT=0
export REBOOT_RERUN_FIRSTBOOT

# Do this before anything else, or we might not be able to run binaries
/sbin/ldconfig /lib /usr/lib /usr/kerberos/lib /opt/tms/lib /opt/tms/lib64

trap "do_firstboot_cleanup_exit" HUP INT QUIT PIPE TERM

do_firstboot_startup() {
    return 0
}

do_firstboot_cleanup() {
    # If this is the first boot, and they have no config db, use the wizard
    if [ "x$BUILD_PROD_FEATURE_WIZARD" = "x1" ]; then
        if [ ! -f /config/db/active ]; then
            touch ${USE_WIZARD_PATH}
        else
            if [ ! -f /config/db/`cat /config/db/active` ]; then 
                touch ${USE_WIZARD_PATH}
            fi
        fi
    fi

    # This runs at the very end, after everything has finished, though
    # before a potential reboot.  One possible application would be to
    # remove ${USE_WIZARD_PATH} if you never want the Wizard to run 
    # automatically.
    if [ "$HAVE_FIRSTBOOT_GRAFT_5" = "y" ]; then
        firstboot_graft_5
    fi

    if [ ${REBOOT_NEEDED} -ne 0 ]; then
            logger -p user.warn "Reboot needed before system can be started"
            if [ ${REBOOT_RERUN_FIRSTBOOT} -eq 0 ]; then
                rm -f /etc/.firstboot
            fi

            umount -a
            mount -n -o remount,ro /
            /sbin/reboot -f
    fi

    unset REBOOT_NEEDED
    unset REBOOT_RERUN_FIRSTBOOT
}

do_firstboot_cleanup_exit() {
    do_firstboot_cleanup
    exit 1
}

######################################################################

do_firstboot_startup

# Define graft functions
if [ -f /etc/customer.sh ]; then
    . /etc/customer.sh
fi

FIRSTMFG_FILE=/var/opt/tms/.firstmfg
if [ -f ${FIRSTMFG_FILE} ]; then 
    if [ "$HAVE_FIRSTBOOT_GRAFT_4" = "y" ]; then
        firstboot_graft_4
    fi
    rm -f ${FIRSTMFG_FILE}
fi

if [ "$HAVE_FIRSTBOOT_GRAFT_1" = "y" ]; then
    firstboot_graft_1
fi

#
# This function is the place to fix up things in /var that have changed
# between versions.
#
# It takes three parameters:
#   - The path to the variable var_version file that reflects the
#     system's current version level
#   - The path to the static var_version file that reflects the
#     currently-booted image's version level
#   - The path to the script containing the upgrade logic
#
do_image_upgrade() {
    PATH_VAR_VERSION_CURR=$1
    PATH_VAR_VERSION_NEW=$2
    PATH_VAR_UPGRADE_SCRIPT=$3

    if [ -f ${PATH_VAR_VERSION_CURR} ]; then
        . ${PATH_VAR_VERSION_CURR}
        if [ -z "${IMAGE_VAR_VERSION}" ]; then
            IMAGE_VAR_VERSION=1
        fi
    else
        IMAGE_VAR_VERSION=1
    fi
    VAR_VERSION_CURR=${IMAGE_VAR_VERSION}

    if [ -f ${PATH_VAR_VERSION_NEW} ]; then
        . ${PATH_VAR_VERSION_NEW}
        if [ -z "${IMAGE_VAR_VERSION}" ]; then
            IMAGE_VAR_VERSION=1
        fi
    else
        IMAGE_VAR_VERSION=1
    fi
    VAR_VERSION_NEW=${IMAGE_VAR_VERSION}
    unset IMAGE_VAR_VERSION

    if [ ${VAR_VERSION_NEW} -ne ${VAR_VERSION_CURR} ]; then
        logger "Image upgrade required from version ${VAR_VERSION_CURR} to version ${VAR_VERSION_NEW}"

        if [ ${VAR_VERSION_NEW} -lt ${VAR_VERSION_CURR} ]; then
            logger -p user.warn "Image upgrade failed: current version too recent"
            return 1
        fi

        # If there is no upgrade script, there's trouble, since by now we
        # have determined that we need to do an upgrade.
        if [ ! -f ${PATH_VAR_UPGRADE_SCRIPT} ]; then
            logger -p user.err "Image upgrade failed: upgrade script ${PATH_VAR_UPGRADE_SCRIPT} not found"
            return 1
        fi

        curr_version=${VAR_VERSION_CURR}

        error=0
        while [ $curr_version -lt ${VAR_VERSION_NEW} ]; do
            next_version=$(( $curr_version + 1 ))
            logger -p user.info "Image upgrade: version $curr_version to version $next_version"

            ugs="${curr_version}_${next_version}"

            # XXX It would be better to update /var/var_version.sh as we go

            . ${PATH_VAR_UPGRADE_SCRIPT} ${ugs}

            curr_version=$next_version
        done

        if [ $error -eq 0 ]; then
            cp ${PATH_VAR_VERSION_NEW} ${PATH_VAR_VERSION_CURR}
            logger "Image upgrade complete."
        else
            logger -p user.err "Image upgrade failed."
        fi
        
    fi

}


do_image_upgrade ${PATH_VAR_VERSION_CURR_GENERIC} ${PATH_VAR_VERSION_NEW_GENERIC} ${PATH_VAR_UPGRADE_SCRIPT_GENERIC}

do_image_upgrade ${PATH_VAR_VERSION_CURR_CUSTOMER} ${PATH_VAR_VERSION_NEW_CUSTOMER} ${PATH_VAR_UPGRADE_SCRIPT_CUSTOMER}

# -----------------------------------------------------------------------------
# Handle forced fallback reboot: when their /var upgrade script has
# encountered a problem, and touched a special file which indicates that
# they want to do a fallback reboot. 
#
# XXX/EMT: these paths, as well as the logic that follows them are
# duplicated from mdinit.
#
eval `/sbin/aiget.sh`

# XXX/EMT: should be in tpaths.sh (see bug 12164)
ATTEMPTING_FALLBACK_FILE=/var/opt/tms/.attempting_fallback_reboot
ATTEMPTING_FIR_FALLBACK_FILE=/var/opt/tms/.attempting_fir_fallback_reboot
ATTEMPTING_IPV6_FALLBACK_FILE=/var/opt/tms/.attempting_ipv6_fallback_reboot
SKIP_FALLBACK_FILE=/var/opt/tms/.skip_fallback_reboot
FORCE_FALLBACK_FILE=/var/opt/tms/.force_fallback_reboot
UNEXPECTED_SHUTDOWN_FILE=/var/opt/tms/.unexpected_shutdown

if [ -f ${FORCE_FALLBACK_FILE} ]; then
    if [ ! -z "${AIG_THIS_BOOT_ID}" -a ! -z "${AIG_FALLBACK_BOOT_ID}" ]; then
        if [ ! -f ${ATTEMPTING_FALLBACK_FILE} ]; then
            if [ "${AIG_THIS_BOOT_ID}" -ne "${AIG_FALLBACK_BOOT_ID}" ]; then
                if [ -f ${SKIP_FALLBACK_FILE} ]; then
                    echo $"Fallback reboot overridden (firstboot) -- not forcing reboot from fallback image ${AIG_FALLBACK_BOOT_ID}"
                    logger -p user.warning -t firstboot.sh $"Fallback reboot overridden (firstboot) -- not forcing reboot from fallback image ${AIG_FALLBACK_BOOT_ID}"
                    # The file will get removed below.
                else
                    echo $"Forcing reboot from fallback image ${AIG_FALLBACK_BOOT_ID} (after /var upgrade)"
                    logger -p user.err -t firstboot.sh "Forcing reboot from fallback image ${AIG_FALLBACK_BOOT_ID} (after /var upgrade)"
                    /sbin/aiset.sh -i -l ${AIG_FALLBACK_BOOT_ID}
                    
                    touch ${ATTEMPTING_FALLBACK_FILE}
                    touch ${ATTEMPTING_FIR_FALLBACK_FILE}
                    rm -f ${ATTEMPTING_IPV6_FALLBACK_FILE}
                    rm -f ${UNEXPECTED_SHUTDOWN_FILE}
                    rm -f ${FORCE_FALLBACK_FILE}
                    
                    umount -a
                    mount -n -o remount,ro /
                    /sbin/reboot -f
                fi
            fi
        fi
    fi
fi
# -----------------------------------------------------------------------------

# If there is no image layout settings file, generate one
LAYOUT_FILE=/etc/image_layout.sh
if [ ! -f "${LAYOUT_FILE}" ]; then
    # XXX #dep/parse: mount
    BOOTED_ROOT_DEV=`/bin/mount | grep '^.* on / type .*$' | awk '{print $1}'`
    BOOTED_DEV=`echo ${BOOTED_ROOT_DEV} | sed 's/^\(.*[^0-9]*\)[0-9][0-9]*$/\1/'`
    touch ${LAYOUT_FILE}
    chmod 644 ${LAYOUT_FILE}

    echo "IL_LAYOUT=STD" >> ${LAYOUT_FILE}
    echo "export IL_LAYOUT" >> ${LAYOUT_FILE}
    echo "IL_LO_STD_TARGET_DISK1_DEV=${BOOTED_DEV}" >> ${LAYOUT_FILE}
    echo "export IL_LO_STD_TARGET_DISK1_DEV" >> ${LAYOUT_FILE}
fi

#
# If virtualization is enabled, make sure all of the storage pools are 
# as they should be.  This has to run after do_image_uprgade (/var upgrade)
# so it can fix up the mfdb if necessary.
#
if [ "x$BUILD_PROD_FEATURE_VIRT" = "x1" ]; then
    /sbin/virt_sync_pools.sh
fi

# XXXXX Should call writeimage to make it remake the fstab, etc, but
# writeimage does not currently support this, so we hack it in here.
#
# The thing we are changing from below is what writeimage had for static
# /etc/fstab additions from 2004 to now (2012/Ginkgo), so at least this
# content changes very infrequently, and so is amenable to this type of
# static fixup.  It is still unfortunate to have to do after each image
# install from here on out.

rm -f /tmp/fstab.updated
cat /etc/fstab | sed \
    -e 's_none		/proc           proc    defaults        0 0_proc            /proc           proc    defaults        0 0_' \
    -e 's_none		/dev/pts        devpts  gid=5,mode=620  0 0_devpts          /dev/pts        devpts  gid=5,mode=620  0 0_' \
    -e 's_none		/dev/shm        tmpfs   defaults        0 0_tmpfs           /dev/shm        tmpfs   defaults        0 0_' > /tmp/fstab.updated
# Add /sys if it is missing from /etc/fstab
grep -q -w "/sys" /tmp/fstab.updated
if [ $? -eq 1 ]; then
    echo 'sysfs           /sys            sysfs   defaults        0 0' >> /tmp/fstab.updated
fi
# Move into place only if changes would be made
cmp -s /etc/fstab /tmp/fstab.updated
if [ $? -eq 1 ]; then
    mv /tmp/fstab.updated /etc/fstab.new
    cp -p /etc/fstab /etc/fstab.prev
    sync
    mv /etc/fstab.new /etc/fstab
    sync
fi
rm -f /tmp/fstab.updated /etc/fstab.new

do_regen_grub_conf() {
    eval `/sbin/aiget.sh`
    if [ ! -z "${AIG_NEXT_BOOT_ID}" ]; then
        /sbin/aiset.sh -i -l $AIG_NEXT_BOOT_ID 
    fi
}

do_regen_grub_conf

do_rsa1_keygen() {
        HKC=$(${MDDBREQ} -l ${MFG_DB_PATH} query get - /ssh/server/hostkey/public/rsa1 /ssh/server/hostkey/private/rsa1 | grep '^Name: ' | wc -l)
        if [ "${HKC}" != "2" ]; then
                echo -n $"Generating SSH1 RSA host key: "
                rm -f $RSA1_KEY.pub
                rm -f $RSA1_KEY
                if $KEYGEN -q -t rsa1 -f $RSA1_KEY -C '' -N '' >&/dev/null; then
                        chmod 600 $RSA1_KEY
                        chmod 644 $RSA1_KEY.pub
                        success $"RSA1 key generation"
                        ${MDDBREQ} -c -f ${MFG_DB_PATH} set modify "" /ssh/server/hostkey/public/rsa1 string $RSA1_KEY.pub
                        ${MDDBREQ} -c -f ${MFG_DB_PATH} set modify "" /ssh/server/hostkey/private/rsa1 binary $RSA1_KEY
                        echo
                else
                        failure $"RSA1 key generation"
                        echo
                fi
                rm -f $RSA1_KEY.pub
                rm -f $RSA1_KEY
        fi
}

do_rsa_keygen() {
        HKC=$(${MDDBREQ} -l ${MFG_DB_PATH} query get - /ssh/server/hostkey/public/rsa2 /ssh/server/hostkey/private/rsa2 | grep '^Name: ' | wc -l)
        if [ "${HKC}" != "2" ]; then
                echo -n $"Generating SSH2 RSA host key: "
                rm -f $RSA_KEY.pub
                rm -f $RSA_KEY
                if $KEYGEN -q -t rsa -f $RSA_KEY -C '' -N '' >&/dev/null; then
                        chmod 600 $RSA_KEY
                        chmod 644 $RSA_KEY.pub
                        success $"RSA key generation"
                        ${MDDBREQ} -c -f ${MFG_DB_PATH} set modify "" /ssh/server/hostkey/public/rsa2 string $RSA_KEY.pub
                        ${MDDBREQ} -c -f ${MFG_DB_PATH} set modify "" /ssh/server/hostkey/private/rsa2 string $RSA_KEY
                        echo
                else
                        failure $"RSA key generation"
                        echo
                fi
                rm -f $RSA_KEY.pub
                rm -f $RSA_KEY
        fi
}

do_dsa_keygen() {
        HKC=$(${MDDBREQ} -l ${MFG_DB_PATH} query get - /ssh/server/hostkey/public/dsa2 /ssh/server/hostkey/private/dsa2 | grep '^Name: ' | wc -l)
        if [ "${HKC}" != "2" ]; then
                echo -n $"Generating SSH2 DSA host key: "
                rm -f $DSA_KEY.pub
                rm -f $DSA_KEY
                if $KEYGEN -q -t dsa -f $DSA_KEY -C '' -N '' >&/dev/null; then
                        chmod 600 $DSA_KEY
                        chmod 644 $DSA_KEY.pub
                        success $"DSA key generation"
                        ${MDDBREQ} -c -f ${MFG_DB_PATH} set modify "" /ssh/server/hostkey/public/dsa2 string $DSA_KEY.pub
                        ${MDDBREQ} -c -f ${MFG_DB_PATH} set modify "" /ssh/server/hostkey/private/dsa2 string $DSA_KEY
                        echo
                else
                        failure $"DSA key generation"
                        echo
                fi
                rm -f $DSA_KEY.pub
                rm -f $DSA_KEY
        fi
}

# Different versions of the platform may have different MIBs,
# and so we need to cause the MIB cache to be regenerated.
do_clean_snmp_mib_cache() {
        rm -f /var/net-snmp/mib_indexes/*
}


if [ "$HAVE_FIRSTBOOT_GRAFT_2" = "y" ]; then
    firstboot_graft_2
fi

#
# Make a note of the system version we are booting now.
# We log to /var/log/systemlog, a log file in the syslog format
# which is permanent (for now) because we never roll it over.
#
# XXX/EMT: PERMLOG should come from tpaths.sh
#
PERMLOG=/var/log/systemlog
DATE=$(date "+%Y/%m/%d %H:%M:%S")
HOSTNAME=$(hostname | sed 's/\./ /' | awk '{print $1}')
printf "%s %s firstboot.sh: booting new image: %s\n" "$DATE" "$HOSTNAME" "$BUILD_PROD_VERSION" >> $PERMLOG

#
# Store the whole build_version.txt file from the image we're booting.
# This provides more detail than what we put in the log file above,
# e.g. the mix of PROD_FEATUREs included.
#
DATETIME=$(date "+%Y%m%d-%H%M%S")
FILEPATH="/var/log/build_versions/version-first-booted-${DATETIME}.txt"
cp /etc/build_version.txt $FILEPATH

chkconfig --add syslog
chkconfig --add mdinit
chkconfig --add shutdown_check
chkconfig --add internal_startup
chkconfig --add pm

# Some customer-specific systems don't want SMART, and if they
# do, they worry about adding it themselves.
if [ "x$BUILD_PROD_FEATURE_SMARTD" = "x1" ]; then
    if [ -d ${MFG_DB_DIR} ]; then

        dev_list=`${MDDBREQ} -v ${MFG_DB_PATH} query iterate "" /mfg/mfdb/smartd/device`
        rm -f ${SMART_CONF}
        for dev in ${dev_list}; do
           dev_name=`echo ${dev} | sed 's/\//\\\\\//g'`
           addl_dev_opts=`${MDDBREQ} -v ${MFG_DB_PATH} query get "" /mfg/mfdb/smartd/device/${dev_name}/opts`
           cat ${SMART_CONF_TEMPLATE} | sed -e "s,@IMAGE_DISK@,${dev},g" -e "s,@ADDL_OPTS@,${addl_dev_opts},g" >> ${SMART_CONF}
        done

        enabled=`${MDDBREQ} -v ${MFG_DB_PATH} query get "" /mfg/mfdb/smartd/enable`
        if [ "${enabled}" = "true" ]; then
            chkconfig --add smartd


        fi
    else
        # If no mfg db, assume they wanted it if the feature is enabled
        chkconfig --add smartd
    fi
fi

# Only use our interface renaming support if turned on
if [ "x$BUILD_PROD_FEATURE_RENAME_IFS" = "x1" ]; then
    chkconfig --add rename_ifs
fi

if [ "x$BUILD_PROD_FEATURE_VIRT" = "x1" ]; then
    chkconfig --add tun
    chkconfig --add iptables

    if [ "x$BUILD_TARGET_PLATFORM_FULL" != "xLINUX_EL_EL5" ]; then
        chkconfig --add messagebus
    fi
fi

if [ "x$BUILD_PROD_FEATURE_VIRT_KVM" = "x1" ]; then
    chkconfig --add kvm
fi

if [ "x$BUILD_PROD_FEATURE_HW_RNG" = "x1" ]; then
    chkconfig --add rngd
fi

do_clean_snmp_mib_cache

# Generate host keys that are solely used to populate bindings
# in the manufacture database
do_rsa1_keygen
do_rsa_keygen
do_dsa_keygen

if [ "$HAVE_FIRSTBOOT_GRAFT_3" = "y" ]; then
    firstboot_graft_3
fi

# Make sure that the services that are on for level 3 are not on for any
# of level 2, 4, or 5 .  Rename your service to have "-rl" at the end of
# it if you need to break this rule, keeping in mind the impact on
# remanufacture.
# XXX #dep/parse: chkconfig
LSVCS=$(chkconfig --list | grep '3:on' | egrep '[245]:on' | awk '{print $1}' | grep -v -- '-rl$')
for lsvc in ${LSVCS}; do
    chkconfig --level 245 ${lsvc} off
done

if [ -x /sbin/vpart.sh ]; then
    # Write any modified virtual partitions to backing store
    /sbin/vpart.sh -w
fi

# This may reboot the system
do_firstboot_cleanup
