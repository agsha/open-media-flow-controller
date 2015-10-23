#!/bin/sh

#
# (C) Copyright 2013-2015 Juniper Networks, Inc. All Rights reserved.
# (C) Copyright 2002-2013 Tall Maple Systems, Inc.
# All rights reserved.
# The contents of this file are subject to the terms of the MIT License.
# You may not use this file except in compliance with the License.
# See the file License.txt at the top of this source tree.
# You can obtain a copy of the license at https://opensource.org/licenses/MIT
#

#
# Note: PM is responsible for cleaning up excess sysdumps, according to
# its configuration.  It supports an action with which you can tell it
# to check and enforce these constraints, for cases when a sysdump is 
# generated outside the context of a crash snapshot.  Why don't we
# automatically send that action here?  Because when we ARE invoked from
# afail.sh, this would be an extra redundant check.  And sometimes, mgmtd
# and/or PM may not be up when we are run.
#
# So instead, our caller may choose to tell PM to enforce constraints,
# depending on the circumstances.
#

PATH=/usr/bin:/bin:/usr/sbin:/sbin:/opt/tms/bin
export PATH

#
# We want these files to have very restrictive permissions as they are
# being created.  When creation is complete, we'll relax the permissions
# so users in the 'diag' group can read them too.
#
# The 3000 here matches mgt_diag in md_client.h.
#
umask 0077
MODE=0640
OWNER=0
GROUP=3000


LOGGER="/usr/bin/logger"

usage()
{
    echo "usage: $0 [extra file list]"
    echo ""
    exit 1
}

PARSE=`/usr/bin/getopt '' "$@"`

if [ $? != 0 ] ; then
    usage
fi

eval set -- "$PARSE"

while true ; do
    case "$1" in
        --) shift ; break ;;
        *) echo "sysdump.sh: parse failure" >&2 ; usage ;;
    esac
done

if [ ! -z "$*" ] ; then
    EXTRA_FILES="$*"
fi



DATE_STR=`date '+%Y-%m-%d %H:%M:%S'`
DATE_STR_FILE=`echo ${DATE_STR} | sed 's/-//g' | sed 's/://g' | sed 's/ /-/g'`
OUTPUT_PREFIX=sysdump-`uname -n`-${DATE_STR_FILE}
FINAL_DIR=/var/opt/tms/sysdumps/
WORK_DIR=/var/tmp/${OUTPUT_PREFIX}-$$
STAGE_DIR_REL=${OUTPUT_PREFIX}
STAGE_DIR=${WORK_DIR}/${STAGE_DIR_REL}
SYSINFO_FILENAME=${WORK_DIR}/sysinfo-${OUTPUT_PREFIX}.txt
DUMP_FILENAME=${WORK_DIR}/${OUTPUT_PREFIX}.tgz
CMD_TIMEOUT_SEC=10

HAS_IPV6=0
if [ -f /proc/net/if_inet6 ]; then
    HAS_IPV6=1
fi

# Make the sysinfo.txt, which has output from useful commands


# Function to dump command output
do_command_output()
{
    local command_name="$1"
    echo "==================================================" >> ${SYSINFO_FILENAME}
    echo "Output of '${command_name}':"  >> ${SYSINFO_FILENAME}
    echo "" >> ${SYSINFO_FILENAME}
    ${command_name} >> ${SYSINFO_FILENAME} 2>&1
    echo "" >> ${SYSINFO_FILENAME}
    echo "==================================================" >> ${SYSINFO_FILENAME}
    echo "" >> ${SYSINFO_FILENAME}
}

# Function to dump command output, but with a timeout
do_command_output_timed()
{
    local command_name="$1"
    echo "==================================================" >> ${SYSINFO_FILENAME}
    echo "Output of '${command_name}':"  >> ${SYSINFO_FILENAME}
    echo "" >> ${SYSINFO_FILENAME}
    /bin/timeout.sh -t ${CMD_TIMEOUT_SEC} -- ${command_name} >> ${SYSINFO_FILENAME} 2>&1
    echo "" >> ${SYSINFO_FILENAME}
    echo "==================================================" >> ${SYSINFO_FILENAME}
    echo "" >> ${SYSINFO_FILENAME}
}


mkdir -p ${WORK_DIR}
mkdir -p ${STAGE_DIR}
touch ${SYSINFO_FILENAME}
chmod 644 ${SYSINFO_FILENAME}

if [ -f /opt/tms/release/build_version.sh ]; then
    . /opt/tms/release/build_version.sh
fi

# Define graft functions
if [ -f /etc/customer.sh ]; then
    . /etc/customer.sh
fi

echo "==================================================" >> ${SYSINFO_FILENAME}
echo "System information:" >> ${SYSINFO_FILENAME}
echo ""  >> ${SYSINFO_FILENAME}
echo "Hostname:      "`uname -n` >> ${SYSINFO_FILENAME}

if [ "$HAVE_SYSDUMP_GRAFT_1" = "y" ]; then
    sysdump_graft_1
fi

echo "Version:       ${BUILD_PROD_VERSION}"  >> ${SYSINFO_FILENAME}

LAST_DOBINCP_FILE=/opt/tms/release/last_dobincp
if [ -e ${LAST_DOBINCP_FILE} ]; then
    LAST_DOBINCP=`stat -c '%y' ${LAST_DOBINCP_FILE}`
    LAST_DOBINCP_FMT=`echo ${LAST_DOBINCP} | sed 's/^\(.* .*\)\.[0-9]*.*$/\1/'`
    echo "Last dobincp:  ${LAST_DOBINCP_FMT}" >> ${SYSINFO_FILENAME}
fi
# We'd like to report the other case: if there has never been a dobincp
# it would be nice to say so explicitly.  But there's no way of knowing
# that, since someone could always be using an older version of dobincp.sh
# (since it's a tool, not part of the image); or they could always manually
# use scp, or otherwise mess up the image.  So it's best to say nothing,
# rather than be potentially misleading.

echo "Current time:  ${DATE_STR}" >> ${SYSINFO_FILENAME}

UPTIME_STRING=`cat /proc/uptime | awk '{fs=$1; d=int(fs/86400); fs=fs-d*86400; h=int(fs/3600); fs=fs-h*3600; m=int(fs/60); fs=fs-m*60; s=int(fs); fs=fs-s; print d "d " h "h " m "m " s "s\n"}'`

echo "System uptime: ${UPTIME_STRING}" >> ${SYSINFO_FILENAME}
echo "" >> ${SYSINFO_FILENAME}

if [ "$HAVE_SYSDUMP_GRAFT_2" = "y" ]; then
    sysdump_graft_2
fi
echo "==================================================" >> ${SYSINFO_FILENAME}
echo "" >> ${SYSINFO_FILENAME}

do_command_output 'uname -a'
do_command_output 'uptime'

echo "==================================================" >> ${SYSINFO_FILENAME}
echo "Target information:" >> ${SYSINFO_FILENAME}
echo "" >> ${SYSINFO_FILENAME}
if [ "${BUILD_TARGET_ARCH_LC}" = "${BUILD_TARGET_CPU_LC}" ]; then
    echo "Arch:      ${BUILD_TARGET_ARCH_LC}" >> ${SYSINFO_FILENAME}
else
    echo "Arch:      ${BUILD_TARGET_ARCH_LC} / ${BUILD_TARGET_CPU_LC}" >> ${SYSINFO_FILENAME}
fi
if [ ! -z "${BUILD_TARGET_HWNAME_LC}" ]; then
    echo "Hw name:   ${BUILD_TARGET_HWNAME_LC}" >> ${SYSINFO_FILENAME}
fi
if [ ! -z "${BUILD_TARGET_HWNAMES_COMPAT_LC}" -a "${BUILD_TARGET_HWNAME_LC}" != "${BUILD_TARGET_HWNAMES_COMPAT_LC}" ]; then
    echo "Hw compat: ${BUILD_TARGET_HWNAMES_COMPAT_LC}" >> ${SYSINFO_FILENAME}
fi
echo "Platform:  ${BUILD_TARGET_PLATFORM_FULL_LC}" >> ${SYSINFO_FILENAME}
basever=$(echo "${BUILD_TMS_SRCS_ID}" | sed 's/samara_release_//')
echo "Base ver:  ${basever:-${BUILD_TMS_SRCS_ID}}" >> ${SYSINFO_FILENAME}
echo "" >> ${SYSINFO_FILENAME}
echo "==================================================" >> ${SYSINFO_FILENAME}
echo "" >> ${SYSINFO_FILENAME}

do_command_output 'df -Pkal'
do_command_output 'swapon -s'
do_command_output 'free'
do_command_output 'lsmod'
do_command_output 'lspci'

if [ -x /usr/sbin/dmidecode -a -x /sbin/dmidump.sh ]; then
    do_command_output '/sbin/dmidump.sh -t -f -z'
fi

do_command_output 'ifconfig -a'
do_command_output 'route -n'
if [ "${HAS_IPV6}" = "1" ]; then
    do_command_output 'route -n -Ainet6'
fi
do_command_output 'netstat -T -ap --numeric-hosts'
do_command_output 'netstat -n -g'

if [ "${BUILD_PROD_FEATURE_VIRT}" = "1" ]; then
    do_command_output_timed 'virsh capabilities'
fi

# .............................................................................
# Iptables (for IPv4)
#
# If we have iptables, save dumps of its tables.  There are potentially
# four tables to list: filter, nat, mangle, and raw.  But even listing the
# contents of a table may cause certain kernel modules to be loaded if they
# were not already (see bug 12445).  We don't want to trigger the loading 
# of any new kernel modules, so we check to see which ones are already 
# loaded, and only list those tables.
#
# We could have done this all in a single loop over ${MODULES}, but then
# the tables would be printed in the order the modules show up in lsmod.
# We want to dictate the order of the tables, hence this variant.
# 
if [ -e /sbin/iptables ]; then
    # XXX #dep/parse: lsmod
    MODULES=`lsmod | awk '{print $1}'`

    for MODULE in ${MODULES}; do
        if [ "${MODULE}" = "iptable_filter" ]; then
            do_command_output '/sbin/iptables -L -n -v'
        fi
    done

    for MODULE in ${MODULES}; do
        if [ "${MODULE}" = "iptable_nat" ]; then
            do_command_output '/sbin/iptables -L -n -v -t nat'
        fi
    done

    for MODULE in ${MODULES}; do
        if [ "${MODULE}" = "iptable_mangle" ]; then
            do_command_output '/sbin/iptables -L -n -v -t mangle'
        fi
    done

    for MODULE in ${MODULES}; do
        if [ "${MODULE}" = "iptable_raw" ]; then
            do_command_output '/sbin/iptables -L -n -v -t raw'
        fi
    done
fi

# .............................................................................
# Ip6tables (for IPv6)
#
# If we have ip6tables, save dumps of its tables.  There are potentially
# three tables to list: filter, mangle, and raw.  But even listing the
# contents of a table may cause certain kernel modules to be loaded if they
# were not already (see bug 12445).  We don't want to trigger the loading 
# of any new kernel modules, so we check to see which ones are already 
# loaded, and only list those tables.
#
# We could have done this all in a single loop over ${MODULES}, but then
# the tables would be printed in the order the modules show up in lsmod.
# We want to dictate the order of the tables, hence this variant.
# 
if [ "${HAS_IPV6}" = "1" -a -x /sbin/ip6tables ]; then
    # XXX #dep/parse: lsmod
    MODULES=`lsmod | awk '{print $1}'`

    for MODULE in ${MODULES}; do
        if [ "${MODULE}" = "ip6table_filter" ]; then
            do_command_output '/sbin/ip6tables -L -n -v'
        fi
    done

    for MODULE in ${MODULES}; do
        if [ "${MODULE}" = "ip6table_mangle" ]; then
            do_command_output '/sbin/ip6tables -L -n -v -t mangle'
        fi
    done

    for MODULE in ${MODULES}; do
        if [ "${MODULE}" = "ip6table_raw" ]; then
            do_command_output '/sbin/ip6tables -L -n -v -t raw'
        fi
    done
fi

if [ "${BUILD_PROD_FEATURE_CRYPTO}" = "1" ]; then
    # Pluto version information:
    if [ -x "/usr/sbin/ipsec" ]; then
        do_command_output '/usr/sbin/ipsec --version'
    fi
    # racoon doesn't have version information, and it's end of life anyway.
fi

if [ -f /proc/net/vlan/config ]; then
    do_command_output 'cat /proc/net/vlan/config'
fi
if [ -f /proc/net/bonding ]; then
    do_command_output 'ls /proc/net/bonding'
fi
do_command_output 'arp -na'
if [ "${HAS_IPV6}" = "1" ]; then
    do_command_output 'ip -f inet6 neigh show'
fi
do_command_output 'ps -AwwL -o user,pid,lwp,ppid,nlwp,pcpu,pri,nice,vsize,rss,tty,stat,wchan:12,start,bsdtime,command'

# This may help gauge the level of I/O activity in the system.
# The '2' is the number of seconds we'll run for, which is a 
# tradeoff between gathering more data and how long we delay sysdump
# generation.
do_command_output 'vmstat 1 2'

do_command_output 'who -a'

# List failure snapshots and sysdumps
do_command_output 'find /var/opt/tms/snapshots/ /var/opt/tms/sysdumps/ ( -name .running -prune ) -o -type f -ls'
# List log files
do_command_output 'find /var/log/ -type f -ls'
# List config files
do_command_output 'find /config/ -type f -ls'

#
# MESSAGES_LOG_LINES tells how many lines of /var/log/messages (starting
# from the most recent) we append to sysinfo.txt.
#
# MAX_LOG_KBYTES tells the maximum number of kilobytes of log files 
# (/var/log/messages*) we copy into the sysdump as standalone files.
#
# MAX_WEBLOG_KBYTES tells the maximum number of kilobytes of web access
# log files (/var/log/web_access_log*) we copy into the sysdump as 
# standalone files.  We don't save web_error_log, as it is probably
# redundant with /var/log/messages (and it's hard to sort it and its
# archives correctly together with web_access_log anyway).
#
# MAX_VIRTLOG_KBYTES tells the maximum number of kilobytes of virtualization
# (libvirt) log files (/var/log/libvirt/qemu/*) we copy into the sysdump
# as standalone files.
#
# XXX/EMT: due to bug 14754, each file is considered to have a minimum 
# impact of 10kB, so in cases where you have a large number of small files,
# this may end up limiting solely based on the number of files, rather than
# their actual size contribution.
#
# Any of these values can be overridden from sysdump_graft_3.
#
MESSAGES_LOG_LINES=50
MAX_LOG_KBYTES=300
MAX_WEBLOG_KBYTES=30
MAX_VIRTLOG_KBYTES=30

if [ "$HAVE_SYSDUMP_GRAFT_3" = "y" ]; then
    sysdump_graft_3
fi

do_command_output 'tail -'${MESSAGES_LOG_LINES}' /var/log/messages'

# XXX possible additions
# include dump of mgmtd monitoring tree

echo "Done." >> ${SYSINFO_FILENAME}

# Copy all the files we want into the stage: this is for the attachment
cp -a /config ${STAGE_DIR}
rm -rf ${STAGE_DIR}/config/lost+found
rm -f ${STAGE_DIR}/config/mfg/*.bak > /dev/null 2>&1

# Cleanse the config databases before dumping them as text.
# We even do some cleansing (albeit less) if PROD_FEATURE_SECURE_NODES
# is disabled.
if [ "${BUILD_PROD_FEATURE_SECURE_NODES}" = "1" ]; then
    CLEANSE_ARGS="-a"
else
    CLEANSE_ARGS="-h"
fi

find ${STAGE_DIR}/config/db/ -type f | xargs -r /sbin/cleanse_db.sh ${CLEANSE_ARGS}

cp -p ${STAGE_DIR}/config/db/`cat ${STAGE_DIR}/config/db/active` ${STAGE_DIR}/active.db
/opt/tms/bin/mddbreq -l ${STAGE_DIR}/active.db query iterate subtree / > ${STAGE_DIR}/active.txt
if [ -d ${STAGE_DIR}/config/mfg/ ]; then
    find ${STAGE_DIR}/config/mfg/ -type f | xargs -r /sbin/cleanse_db.sh -m /mfg/mfdb ${CLEANSE_ARGS}
fi
if [ ! -f ${STAGE_DIR}/config/mfg/mfdb ]; then
    cp -p /config/mfg/mfdb ${STAGE_DIR}/mfdb
    /sbin/cleanse_db.sh -m /mfg/mfdb ${CLEANSE_ARGS} ${STAGE_DIR}/mfdb
else
    cp -p ${STAGE_DIR}/config/mfg/mfdb ${STAGE_DIR}/mfdb
fi
/opt/tms/bin/mddbreq -l ${STAGE_DIR}/mfdb query iterate subtree / > ${STAGE_DIR}/mfdb.txt


lsof -b -n 2>&- > ${STAGE_DIR}/lsof.txt
# List all of /var/opt/tms
find /var/opt/tms/ -ls > ${STAGE_DIR}/list-var_opt_tms.txt

# Yes, these numbers are conservative, but we don't want to risk a giant mail
avg_bytes_per_line=100
avg_compression_ratio=10

#
# Save as many of a specified list of files as we can, up to a specified
# limit, in kB.
#
# Usage: save_files <file list> <max size> <stage dir>
#
save_files()
{
    local file_list="$1"
    local max_size="$2"
    local stage_dir="$3"
    total_size=0

    for vlm in ${file_list}; do

        # Note that we follow symlinks (as the 'cp' and 'tail' below do).
        # Also we "round up" the number of KBytes, and always give at
        # least 1, even in error cases.
        curr_size=`stat -L -c '%s' "$vlm" 2> /dev/null | awk 'BEGIN { f = 1 } {fs=$1; f= (fs / 1024) + 1} END { print int(f) }'`

        # See if it is compressed on disk already
        if [ `echo $vlm | sed s/.gz$//` != "${vlm}" ]; then
            compressed=1
        else
            compressed=0
        fi
        
        # Estimate compressed size
        if [ ${compressed} -eq 0 ]; then
            est_size=`expr ${curr_size} / ${avg_compression_ratio}`
        else
            est_size=${curr_size}
        fi
        # Always add at least 1k to cover any per-file overhead
        if [ ${est_size} -lt 1 ]; then
            est_size=1
        fi
        
        new_total=`expr ${total_size} + ${est_size}`
        if [ ${new_total} -gt ${max_size} ]; then
            
            # We never uncompress a file as part of this 
            if [ ${compressed} -eq 1 ]; then
                break
            fi
            
            # If it's an uncompressed file, take some trailing lines
            space_left=`expr ${max_size} - ${total_size}`
            lines_left=`expr ${space_left} \* 1024 \* ${avg_compression_ratio} / ${avg_bytes_per_line}`
            
            tail -${lines_left} ${vlm} > ${stage_dir}/`basename ${vlm}`
            
            break
        else
            cp -p ${vlm} ${stage_dir}
        fi
        
        total_size=${new_total}
    done
}

LOGS_LIST=`find /var/log/ -name messages\* -print | sort -n -k 1.19`
WEBLOGS_LIST=`find /var/log/ -name web_access_log\* -print | sort -n -k 1.25`

#
# We can't follow the same approach to sorting this list as the above two
# calls, since the paths are like "/var/log/libvirt/qemu/vtb1.log.1.gz".
# We want to sort primarily by archive number (with the current files,
# lacking an archive number, coming out first), and then secondarily by
# the VM name.
#
VIRTLOGS_LIST=`find /var/log/libvirt/qemu -type f | sort -n -t '.' -k 3 -k 1`

save_files "${LOGS_LIST}" "${MAX_LOG_KBYTES}" "${STAGE_DIR}"
save_files "${WEBLOGS_LIST}" "${MAX_WEBLOG_KBYTES}" "${STAGE_DIR}"
save_files "${VIRTLOGS_LIST}" "${MAX_VIRTLOG_KBYTES}" "${STAGE_DIR}"

lspci -vvv > ${STAGE_DIR}/lspci-vvv.log
lspci -vvvn > ${STAGE_DIR}/lspci-vvvn.log
cat /proc/version > ${STAGE_DIR}/version.log
if [ -e /proc/config.gz ]; then
    cat /proc/config.gz > ${STAGE_DIR}/kern_config.gz
    gunzip -c ${STAGE_DIR}/kern_config.gz > ${STAGE_DIR}/kern_config && rm -f ${STAGE_DIR}/kern_config.gz
fi
if [ -f /boot/config ]; then
    cp -p /boot/config ${STAGE_DIR}/kern_boot_config
fi
cat /proc/ioports > ${STAGE_DIR}/ioports.log
cat /proc/cmdline > ${STAGE_DIR}/cmdline.log
if [ "${BUILD_TARGET_ARCH_FAMILY}" = "X86" ]; then
    cat /proc/iomem > ${STAGE_DIR}/iomem.log
fi
if [ -e /proc/scsi/scsi ]; then
    cat /proc/scsi/scsi > ${STAGE_DIR}/scsi.log
fi
cat /proc/cpuinfo > ${STAGE_DIR}/cpuinfo.log
cat /proc/mounts > ${STAGE_DIR}/mounts.log
cat /proc/partitions > ${STAGE_DIR}/partitions.log
if [ -f /proc/mtd ]; then
    cat /proc/mtd > ${STAGE_DIR}/mtd.log
fi

if [ -f /etc/modprobe.conf ]; then
    cp -p /etc/modprobe.conf ${STAGE_DIR}
fi
if [ -d /etc/modprobe.d ]; then
    cp -a /etc/modprobe.d ${STAGE_DIR}
fi
dmesg > ${STAGE_DIR}/dmesg
# Include what dmesg had right after boot
cp -p /var/log/dmesg ${STAGE_DIR}/dmesg.boot

if [ -f /var/lib/logrotate.status ]; then
    cp -p /var/lib/logrotate.status ${STAGE_DIR}
fi

#
# Save dhclient data files.  This is more complicated because there may
# not be any such files, and we don't want to display cp's whining in 
# that case; but we also don't want to suppress all output, in case there
# is some other problem.  Of course, there's a race condition here if the
# files disappear right after we check, but life is too short for that.
#
DHCLIENT_FILE_LIST=`find /var/run -type f -name dhclient\*`
if [ ! -z "$DHCLIENT_FILE_LIST" ]; then
    cp -p /var/run/dhclient* ${STAGE_DIR}
fi

# Save all sysctl settings.  This is helpful for many things, but is the
# only place we're recording a number of IPv6 settings.
sysctl -a > ${STAGE_DIR}/sysctl.log

# Save off the interface addresses, routes and neighbors.  Most, but not
# all, of this information is in the email text.  For instance we
# otherwise miss IPv6 address status and lifetime information.
ip addr show > ${STAGE_DIR}/addrs.log
ip -f inet route list > ${STAGE_DIR}/routes-ipv4.log
if [ "${HAS_IPV6}" = "1" ]; then
    ip -f inet6 route list > ${STAGE_DIR}/routes-ipv6.log
fi
ip neigh show > ${STAGE_DIR}/neigh.log

cp -p /etc/fstab ${STAGE_DIR}
cp -p /etc/image_layout.sh ${STAGE_DIR}

if [ "${BUILD_TARGET_ARCH_FAMILY}" = "PPC" ]; then
    EETOOL=/opt/tms/bin/eetool
    if [ -x ${EETOOL} ]; then
        ${EETOOL} > ${STAGE_DIR}/eetool.log
    fi

    NVSTOOL=/opt/tms/bin/nvstool
    if [ -x ${NVSTOOL} ]; then
        ${NVSTOOL} -m > ${STAGE_DIR}/nvstool.log
    fi

else

    # x86 case
    if [ -f /bootmgr/boot/grub/grub.conf ]; then
        cp -p /bootmgr/boot/grub/grub.conf ${STAGE_DIR}/grub.conf
    fi

fi

cp -p ${SYSINFO_FILENAME} ${STAGE_DIR}/sysinfo.txt
cp -a /var/opt/tms/output ${STAGE_DIR}
cp -p /etc/build_version.sh ${STAGE_DIR}
cp -p /etc/build_version.txt ${STAGE_DIR}
cp -p /var/log/systemlog ${STAGE_DIR}

#
# If PROD_FEATURE_SECURE_NODES is enabled, the output directory may 
# contain sensitive info in the clear.  The UI obfuscates this info, and
# the config file encrypts it, but it has the be cleartext here for its
# legitimate consumers.  Per bug 12082, we should remove this information.
#
if [ "${BUILD_PROD_FEATURE_SECURE_NODES}" = "1" ]; then
    ODIR=${STAGE_DIR}/output

    SSCONF=${ODIR}/ssmtp.conf
    if [ -f ${SSCONF} ]; then
        cat ${SSCONF} | sed s/AuthPass=.*/'AuthPass=********'/ > ${SSCONF}.new
        mv -f ${SSCONF}.new ${SSCONF}
    fi

    SSACONF=${ODIR}/ssmtp-auto.conf
    if [ -f ${SSACONF} ]; then
        cat ${SSACONF} | sed -e s/^AuthPass=.*/'AuthPass=********'/ -e s/^BackupAuthPass=.*/'BackupAuthPass=********'/ > ${SSACONF}.new
        mv -f ${SSACONF}.new ${SSACONF}
    fi

    # Note: web and other application certificate keys are symlinks to these files
    CERT_KEY_FILES=`echo ${ODIR}/certs/*.key`
    for CERT_KEY_FILE in $CERT_KEY_FILES
    do
        if [ -f ${CERT_KEY_FILE} ]; then
            echo "(File contents removed from sysdump for security.)" >${CERT_KEY_FILE}
        fi
    done

    SETKEY=${ODIR}/setkey.conf
    if [ -f ${SETKEY} ]; then
        cat ${SETKEY} | sed s/"^\(^#*[ \t]*add .*-[EA][ \t].*\"\)\(.*\"\)"/"\1********\""/ > ${SETKEY}.new
        mv -f ${SETKEY}.new ${SETKEY}
    fi
    if [ -f ${SETKEY}.bak ]; then
        cat ${SETKEY}.bak | sed s/"^\(^#*[ \t]*add .*-[EA][ \t].*\"\)\(.*\"\)"/"\1********\""/ > ${SETKEY}.bak.new
        mv -f ${SETKEY}.bak.new ${SETKEY}.bak
    fi

    SETKEY_CH=${ODIR}/setkey.changes.conf
    if [ -f ${SETKEY_CH} ]; then
        cat ${SETKEY_CH} | sed s/"^\(^#*[ \t]*add .*-[EA][ \t].*\"\)\(.*\"\)"/"\1********\""/ > ${SETKEY_CH}.new
        mv -f ${SETKEY_CH}.new ${SETKEY_CH}
    fi
    if [ -f ${SETKEY_CH}.bak ]; then
        cat ${SETKEY_CH}.bak | sed s/"^\(^#*[ \t]*add .*-[EA][ \t].*\"\)\(.*\"\)"/"\1********\""/ > ${SETKEY_CH}.bak.new
        mv -f ${SETKEY_CH}.bak.new ${SETKEY_CH}.bak
    fi

    RKEYS=${ODIR}/racoon/psk.txt
    if [ -f ${RKEYS} ]; then
        cat ${RKEYS} | sed s/"^\([^ \t]*\)\t\([^ \t]*\)"/"\1\t********"/ > ${RKEYS}.new
        mv -f ${RKEYS}.new ${RKEYS}
    fi
    if [ -f ${RKEYS}.bak ]; then
        cat ${RKEYS}.bak | sed s/"^\([^ \t]*\)\t\([^ \t]*\)"/"\1\t********"/ > ${RKEYS}.bak.new
        mv -f ${RKEYS}.bak.new ${RKEYS}.bak
    fi
    
    RADCONF=${ODIR}/pam_radius_auth.conf
    if [ -f ${RADCONF} ]; then
        cat ${RADCONF} | sed s/"^\([^ \t]*\)\t\([^ \t]*\)"/"\1\t********"/ > ${RADCONF}.new
        mv -f ${RADCONF}.new ${RADCONF}
    fi
    if [ -f ${RADCONF}.bak ]; then
        cat ${RADCONF}.bak | sed s/"^\([^ \t]*\)\t\([^ \t]*\)"/"\1\t********"/ > ${RADCONF}.bak.new
        mv -f ${RADCONF}.bak.new ${RADCONF}.bak
    fi
    
    TACCONF=${ODIR}/pam_tacplus_server.conf
    if [ -f ${TACCONF} ]; then
        cat ${TACCONF} | sed s/"^\([^ \t]*\)\t\([^ \t]*\)\t\([^ \t]*\)\t\([^ \t]*\)"/"\1\t\2\t\3\t********"/ > ${TACCONF}.new
        mv -f ${TACCONF}.new ${TACCONF}
    fi
    if [ -f ${TACCONF}.bak ]; then
        cat ${TACCONF}.bak | sed s/"^\([^ \t]*\)\t\([^ \t]*\)\t\([^ \t]*\)\t\([^ \t]*\)"/"\1\t\2\t\3\t********"/ > ${TACCONF}.bak.new
        mv -f ${TACCONF}.bak.new ${TACCONF}.bak
    fi

    LDAPCONF=${ODIR}/pam_ldap.conf
    if [ -f ${LDAPCONF} ]; then
        cat ${LDAPCONF} | sed -e s/^bindpw\ .*/'bindpw ********'/ > ${LDAPCONF}.new
        mv -f ${LDAPCONF}.new ${LDAPCONF}
    fi

    BMPASS=${ODIR}/bootmgr_pass
    if [ -f ${BMPASS} ]; then
        echo "(File contents removed from sysdump for security.)" > ${BMPASS}
    fi

    BMPASS=${ODIR}/bootmgr_pass.bak
    if [ -f ${BMPASS} ]; then
        echo "(File contents removed from sysdump for security.)" > ${BMPASS}
    fi

    SHADOW=${ODIR}/shadow
    if [ -f ${SHADOW} ]; then
        echo "(File contents removed from sysdump for security.)" > ${SHADOW}
    fi

    SHADOW=${ODIR}/shadow.bak
    if [ -f ${SHADOW} ]; then
        echo "(File contents removed from sysdump for security.)" > ${SHADOW}
    fi

    GRUBCONF=${STAGE_DIR}/grub.conf
    if [ -f ${GRUBCONF} ]; then
        cat ${GRUBCONF} | sed -e 's/^password .*$/#password ********/' > ${GRUBCONF}.new
        mv -f ${GRUBCONF}.new ${GRUBCONF}
    fi

    EETOOLLOG=${STAGE_DIR}/eetool.log
    if [ -f ${EETOOLLOG} ]; then
        cat ${EETOOLLOG} | sed -e 's/^UBPASSWD=.*$/UBPASSWD="********"/' > ${EETOOLLOG}.new
        mv -f ${EETOOLLOG}.new ${EETOOLLOG}
    fi

    NVSTOOLLOG=${STAGE_DIR}/nvstool.log
    if [ -f ${NVSTOOLLOG} ]; then
        cat ${NVSTOOLLOG} | sed -e 's/^BOOTMGR_PASSWORD=.*$/BOOTMGR_PASSWORD='\''********'\''/' > ${NVSTOOLLOG}.new
        mv -f ${NVSTOOLLOG}.new ${NVSTOOLLOG}
    fi

fi

CLI_TIMEOUT_SEC=20
WANT_CLI_RUNNING_CONFIG=1

#
# If there are customer-defined secure nodes whose contents are reflected
# in files in /var/opt/tms/output, they could take steps in here to 
# obfuscate them, e.g. using 'sed' as we have done above for the base
# platform nodes.
#
# Anyone who needs to override the default timeout on calling the CLI to
# do a "show running-config" can set CLI_TIMEOUT_SEC here to the timeout 
# duration, in seconds.  The default is 20 seconds.
#
if [ "$HAVE_SYSDUMP_GRAFT_4" = "y" ]; then
    sysdump_graft_4
fi

# Find and include cli history files for all users
ALL_CLI_HISTORY_FILES=`(cd /var/home; find . -maxdepth 2 -name .cli_history)`
for hf in ${ALL_CLI_HISTORY_FILES}; do
    reordered=`dirname $hf``basename $hf`
    new_hf_name=`echo $reordered | sed 's/^\.\///' | sed 's/[^a-zA-Z0-9_-]/_/g'`
    cp -p /var/home/$hf ${STAGE_DIR}/${new_hf_name}
done

if [ "${WANT_CLI_RUNNING_CONFIG}" = "1" ]; then
    if [ "${BUILD_PROD_FEATURE_SECURE_NODES}" = "1" ]; then
        # Use the "limited" parameter to suppress licenses and hashed passwords.
        # Anything in cleartext is already obfuscated (except for the private
        # host keys, which are never shown at all anyway).
        SHOW_CMD="show running-config limited"
    else
        SHOW_CMD="show running-config"
    fi

    # Save the running config as CLI commands.
    # NOTE: this depends on mgmtd, and as such, may fail.

    /bin/timeout.sh -t ${CLI_TIMEOUT_SEC} -- /opt/tms/bin/cli -t enable \"${SHOW_CMD}\" > $STAGE_DIR/running-config.txt 2>&1
fi

# Copy any specified extra files
for ef in ${EXTRA_FILES}; do
    cp -p $ef ${STAGE_DIR}
done

#
# After staging area set up, but before it's made into a tarball.
# Code may operate on staging area.  Available variables:
#   - ${STAGE_DIR}: path to the staging directory
#
if [ "$HAVE_SYSDUMP_GRAFT_5" = "y" ]; then
    sysdump_graft_5
fi

# Tar and gzip up the stage, then remove it

tar -C ${WORK_DIR} -czf ${DUMP_FILENAME} ${STAGE_DIR_REL}

#
# After tarball is created.  Available variables:
#   - ${DUMP_FILENAME}: path to the newly created tarball
#
if [ "$HAVE_SYSDUMP_GRAFT_6" = "y" ]; then
    sysdump_graft_6
fi

chown ${OWNER}:${GROUP} ${SYSINFO_FILENAME} ${DUMP_FILENAME}
chmod ${MODE} ${SYSINFO_FILENAME} ${DUMP_FILENAME}

mv ${SYSINFO_FILENAME} ${FINAL_DIR}
mv ${DUMP_FILENAME} ${FINAL_DIR}

rm -rf ${WORK_DIR} 

# Print the names of the files we generated
echo "SYSINFO=`echo ${SYSINFO_FILENAME} | sed s,${WORK_DIR},${FINAL_DIR},`"
echo "SYSDUMP=`echo ${DUMP_FILENAME} | sed s,${WORK_DIR},${FINAL_DIR},`"

exit 0
