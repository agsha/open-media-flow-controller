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
# This script is used to scrub a system clean to its default/initial state.
#
# XXX/EMT: this is fragile because we just have a hardwired list of things
# that need to be deleted.  It would be more reliable if we used the manifest
# that came with the image, to delete anything not in the manifest, and
# complain if anything that is supposed to be present is no longer.
#

PATH=/usr/bin:/bin:/usr/sbin:/sbin
export PATH

USE_WIZARD_PATH=/var/opt/tms/.usewizard

umask 0022

PM_STOPPED=0
trap "error_cleanup" HUP INT QUIT PIPE TERM

# If we are being launched from the CLI, give it a chance to exit before
# we do our stuff.  If we are not, give the caller a short chance to change
# his mind.
sleep 1

usage()
{
    echo "usage: $0 [-r] [-p] [-a] [-v] [-c]"
    echo "   -r  Reboot system when finished, instead of halting"
    echo ""
    echo "Preservation options:"
    echo "   -p  Preserve licenses in active configuration"
    echo "   -a  Preserve all configuration (includes -p)"
    echo "   -v  Preserve virtualization volumes (from 'show virt volume')"
    echo ""
    echo "Scrub-only options: (mutually exclusive with preservation options)"
    echo "   -c  Scrub only configuration, leave all else"
    echo ""
    exit 1
}

error_cleanup()
{
    # See bug 14507
    if [ ! -z "${new_cfg}" ]; then
        rm -f /config/db/${new_cfg}
    fi

    if [ $PM_STOPPED -eq 0 ]; then
        ERRMSG="Scrub.sh halted with error before stopping PM; exiting"
        logger $ERRMSG
        echo $ERRMSG
        exit 1
    else
        ERRMSG="Scrub.sh halted with error after stopping PM; rebooting"
        logger $ERRMSG
        echo $ERRMSG
        /sbin/reboot
    fi
}

ALL_ARGS="$@"
PARSE=`/usr/bin/getopt 'rpavc' "$@"`

if [ $? != 0 ] ; then
    usage
fi

eval set -- "$PARSE"

REBOOT=0
PRESERVE_BASIC=0
PRESERVE_ALL=0
PRESERVE_VIRT_VOL=0
ONLY_CONFIG=0

while true ; do
    case "$1" in
        -r) REBOOT=1; shift ;;
        -p) PRESERVE_BASIC=1; shift ;;
        -a) PRESERVE_ALL=1; shift ;;
        -v) PRESERVE_VIRT_VOL=1; shift ;;
        -c) ONLY_CONFIG=1; shift ;;
        --) shift ; break ;;
        *) echo "$0: parse failure" >&2 ; usage ;;
    esac
done

if [ $ONLY_CONFIG -eq 1 ]; then
    if [ $PRESERVE_BASIC -eq 1 -o $PRESERVE_ALL -eq 1 -o $PRESERVE_VIRT_VOL -eq 1 ]; then
        echo "ERROR: -c option may not be used with -p, -a, or -v options."
        echo ""
        usage
    fi
fi

# Define graft functions
if [ -f /etc/customer.sh ]; then
    . /etc/customer.sh
fi

. /etc/build_version.sh

#
# This variable tells us whether or not to recreate /etc/.firstboot, which
# would cause /sbin/firstboot.sh to be rerun on our next boot.  We do not
# do this by default, but either of the graft points may set this to 1
# to get us to do it.
#
RERUN_FIRSTBOOT=0

#
# Perform customer-specific cleanup phase 1 (with all daemons running)
# Set RERUN_FIRSTBOOT=1 from this graft point (or the other one) to
# recreate .firstboot file when we're done.
#
if [ "$HAVE_SCRUB_GRAFT_1" = "y" ]; then
    scrub_graft_1
fi

#
# Unless we are preserving all configuration, create a new file
# (possibly with some subset of config preserved), and delete all others.
#
if [ $PRESERVE_ALL -eq 0 ]; then
    conf_new_args=""
    if [ $PRESERVE_BASIC -eq 0 ]; then
        conf_new_args="factory"
    fi
    new_cfg="temp_scrub_config.$$"
    /opt/tms/bin/cli -t "enable" "configure terminal" "configuration new $new_cfg $conf_new_args" "configuration switch-to $new_cfg"

    #
    # Now remove all the other config files
    #
    cfg_files=`ls /config/db`
    for cfg_file in $cfg_files; do
        if [ $cfg_file = "active" -o $cfg_file = $new_cfg ]; then
            continue
        fi

        rm -f /config/db/$cfg_file
    done

    #
    # Now rename the config file to 'initial'
    #
    /opt/tms/bin/cli -t "enable" "configure terminal" "configuration write to initial" "configuration delete $new_cfg"
    rm -f /config/db/active.bak
fi

#
# Now that we're done with the config files, we don't need mgmtd
# running anymore.  We don't want at least some of the other daemons
# running, like statsd whose files we're about to delete, and who knows
# what else.  Easiest to just stop everything.
#
PM_STOPPED=1
service pm stop
sleep 1

#
# Now that all daemons are stopped, it's safe to write to the systemlog.
# We couldn't do this before, since mgmtd might write to it too, and
# there is currently no file locking.
#
PERMLOG=/var/log/systemlog
DATE=$(date "+%Y/%m/%d %H:%M:%S")
HOSTNAME=$(hostname | sed 's/\./ /' | awk '{print $1}')
printf "%s %s scrub.sh: resetting system to factory defaults (arguments: %s)\n" "$DATE" "$HOSTNAME" "$ALL_ARGS" >> $PERMLOG


#
# Perform customer-specific cleanup phase 2 (with all daemons stopped)
# Set RERUN_FIRSTBOOT=1 from this graft point (or the other one) to
# recreate .firstboot file when we're done.
#
if [ "$HAVE_SCRUB_GRAFT_2" = "y" ]; then
    scrub_graft_2
fi

if [ $ONLY_CONFIG -eq 1 ]; then
    DELETE_LOGS=0

    # XXX/EMT: should we also do this in all cases where -a was NOT
    # specified?  See bug 13822.
    rm -rf /config/db/*

    # We could rotate the logs here...

else
    DELETE_LOGS=1

# -----------------------------------------------------------------------------
# This block of cleansing code is done only if -c was NOT specified...

#
# Kill all the stats, images, and snapshots
#
rm -f /var/opt/tms/stats/*.dat
rm -rf /var/opt/tms/stats/reports/*

rm -rf /var/opt/tms/images/*

rm -rf /var/opt/tms/snapshots/*

#
# Kill all the history files
#
# XXX/EMT: this won't get the history for any bash shells currently
# running, since they will exit and write their history files during
# the reboot.  We should kill them first (without killing ourselves,
# and we are also a bash process...)
#
find /var/home -name \*history\* | xargs rm -f
find /var/home -name \.lesshst | xargs rm -f

#
# Kill any non-dotfiles in the home directories
#
home_dirs=`ls /var/home`
for home_dir in $home_dirs; do
    rm -rf /var/home/$home_dir/*
done

#
# Kill .ssh directories, which may have known hosts, etc.
#
rm -rf `find /var/home -name .ssh`

#
# Kill various other things
#
rm -f /var/lib/logrotate.status
rm -f /var/lock/logrotate
rm -f /var/racoon/racoon.sock
rm -rf /var/run/pluto
rm -rf /var/lock/subsys/ipsec
rm -rf /var/lib/ssh/*
rm -rf /var/lib/misc/*
rm -rf /var/net-snmp/*
rm -rf /var/run/dhclient-*
rm -rf /var/run/dhclient6-*
rm -rf /tmp/*
rm -rf /var/opt/tms/sched/*
rm -rf /var/opt/tms/sysdumps/*
rm -rf /var/opt/tms/tcpdumps/*
rm -rf /var/opt/tms/web/graphs/*
rm -rf /var/opt/tms/web/sysimages/*
rm -rf /var/opt/tms/cmc/ssh/keys/*
rm -rf /var/opt/tms/text_configs/*

#
# We don't usually kill things in /var/opt/tms/output, because
# (a) some of it might be needed during boot, and
# (b) the theory is that the rest will be overwritten anyway by
# the new mgmtd, so we needn't bother sorting them out.
#
# However, there are some cases of files which are only written
# conditionally.  Those are typically not neede for boot, and should
# be removed lest they be leftover.
#
rm -f /var/opt/tms/output/ssh_known_hosts

#
# Kill virtualization-related stuff
#
if [ $PRESERVE_VIRT_VOL -eq 0 ]; then
    VIRT_POOL_ROOT=/var/opt/tms/virt/pools
    POOLS=`ls -1 ${VIRT_POOL_ROOT}`
    for POOL in ${POOLS}; do
        POOL_PATH=${VIRT_POOL_ROOT}/${POOL}
        rm -rf ${POOL_PATH}/*
        rm -rf ${POOL_PATH}/.temp/*
    done
fi
rm -rf /var/opt/tms/virt/vm_save/*
rm -rf /var/opt/tms/virt/vm_save/.temp/*
rm -rf /var/cache/libvirt/*
find /var/lib/libvirt -type f | xargs rm -f
find /var/run/libvirt -type f | xargs rm -f

rm -rf /var/opt/tms/output/libvirt/storage/*
rm -rf /var/opt/tms/output/libvirt/storage/autostart/*
rm -rf /var/opt/tms/output/libvirt/qemu/*
rm -rf /var/opt/tms/output/libvirt/qemu/networks/*
rm -rf /var/opt/tms/output/libvirt/qemu/networks/autostart/*
rm -rf /var/opt/tms/output/libvirt/qemu/autostart/*

fi

# End cleaning for the case when -c was NOT specified
# -----------------------------------------------------------------------------

#
# Kill all the backup output files.
#
# We should not have to delete any of the main files, since above we switched
# to whatever config file we will have when we reboot.  This should have
# overwritten or removed any residual information in the main files.
# The backup files will probably be overwritten when mdinit runs on the
# next boot, but we remove them here because there's no possible benefit in
# keeping them, and someone could still remove the hard disk and look at the
# files before booting the appliance.
#
find /var/opt/tms/output -name \*.bak | xargs rm -f


#
# Reinstate the wizard, unless we're saving all configuration, in which
# case the Wizard probably isn't needed.
#
if [ "x$BUILD_PROD_FEATURE_WIZARD" = "x1" ]; then
    if [ $PRESERVE_ALL -eq 0 ]; then
        touch ${USE_WIZARD_PATH}
    fi
fi

#
# If the customer wants to, recreate /etc/.firstboot
#
if [ $RERUN_FIRSTBOOT -eq 1 ]; then
    /bin/mount -o remount,rw /
    touch /etc/.firstboot
    /bin/mount -o remount,ro /
fi

#
# Finally, kill all the logs.  We do this last in case something goes
# wrong in the earlier stages, we might be able to get something out of
# the logs about it.  Note that this won't happen if -c was specified.
#
if [ $DELETE_LOGS -eq 1 ]; then
    service syslog stop
    rm -f /var/log/messages*
    rm -rf /var/log/sa/*

    # See bugs 12977 and 13332.  We don't remove either build_versions
    # or the systemlog, since they both may have valuable information
    # for troubleshooting.  This makes a rerun of firstboot.sh
    # irrelevant, at least as far as these files are concerned.
    # rm -rf /var/log/build_versions/*
    # rm -f ${PERMLOG}
    # touch ${PERMLOG}

    rm -f /var/log/lastlog
    touch /var/log/lastlog
    rm -f /var/log/web_*
    rm -f /var/log/timechangelog
    rm -rf /var/log/libvirt

    rm -f /var/log/wtmp*
    touch /var/log/wtmp
    chmod 0600 /var/log/wtmp
    chown root.utmp /var/log/wtmp

    # Having a 'btmp' file is optional.  But if there is one, we need
    # to scrub it (and any archives leftover from log rotation), and
    # recreate it so the records will continue being kept.
    if [ -f /var/log/btmp ]; then
        rm -f /var/log/btmp*
        touch /var/log/btmp
        chmod 0600 /var/log/btmp
        chown root.utmp /var/log/btmp
    fi
fi

#
# Any final custom cleanup before we're done.  One possible use for this
# would be to restore anything that you didn't want to get scrubbed,
# which you had saved (by moving to a different directory) from one of
# the earlier graft points.
#
if [ "$HAVE_SCRUB_GRAFT_3" = "y" ]; then
    scrub_graft_3
fi

#
# Halt or reboot the box
#
if [ $REBOOT -eq 1 ]; then
    /sbin/reboot
else
    /sbin/halt
fi
