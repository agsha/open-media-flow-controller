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

# This script is used to write an image or images to a disk.  There are two
# modes for this program: install a single image (possibly also installing
# the boot loader), and manufacture mode, which fully partitions a disk or
# disks, makes relevant file systems, and installs the image.  A disk cannot
# be manufactured when any part of it is mounted, nor can an image be
# installed over the running image in the non-manufacturing case.

# There are a number of supported layouts, see the layout_settings.sh file
# for more information.  The currently supported layouts are: 1) single
# disk, two image (standard) 2) two disks, two images on first, replicated
# layout (no images) on second (replicate) 3) single disk, single image
# (compact) In all layouts, the entire disk is used, and (during
# manufacturing) any existing paritions are destroyed.


# XXXX need way to call in firstboot.sh to generate grub / fstab / do any
# other post-install fixups.  This is because the version of writeimage.sh
# that installed the image may be much older than the image (which does
# contain a new writeimage).


PATH=/usr/bin:/bin:/usr/sbin:/sbin
export PATH

umask 0022


usage()
{
    echo "usage: $0 -m [-u URL] [-f FILE] [-L LAYOUT_TYPE] -d /DEV/N1"
    echo "             [-p PARTNAME -s SIZE] [-O LOCNAME] [-t] [-T LOCNAME]"
    echo "             [-k KERNEL_TYPE] [-w HWNAME] [-I] [-r] [-z] [-y]"
    echo "             [-F LOGFILE] [-v]"
    echo "usage: $0 -i [-u URL] [-f FILE] [-d /DEV/N1] -l {1,2} [-t]"
    echo "             [-O LOCNAME] [-k KERNEL_TYPE] [-w HWNAME] [-I] [-r]"
    echo "             [-F LOGFILE] [-v]"
    echo "             [-P ID] [-b {1,2}]"
    echo ""
    echo ""
    echo "Exactly one of '-m' (manufacture) or '-i' (install) must be specified."
    echo ""
    echo "Exactly one of '-u' (url) or '-f' (file) must be specified."
    echo ""
    echo "-u: specify the URL from which to download the image to install"
    echo ""
    echo "-f: specify a path in the local filesystem to the image to install"
    echo "    (alternative to -u)"
    echo ""
    echo "-L LAYOUT_TYPE: STD or other defined layout"
    echo ""
    echo "-d: device(s) to put image on (i.e. /dev/hda)"
    echo ""
    echo "-p PARTNAME -s SIZE: (mfg only) partition name and size to use"
    echo "   if desired to be different from ones defined for layout"
    echo ""
    echo "-O LOCNAME: exclude location (like -BOOT_2, or --all- for all)"
    echo "-O +LOCNAME: include (normally excluded) location (like +VAR_1)"
    echo ""
    echo "-B: force install of bootmgr"
    echo ""
    echo "-l LOCATION_NUMBER: 1 or 2 (install only) specifies which of the two"
    echo "   image locations to which to install"
    echo ""
    echo "-t: don't use tmpfs for a working area, instead"
    echo "    will attempt to use space in /var"
    echo ""
    echo "-T LOCNAME: (mfg only) working area is on LOCNAME instead of VAR_1 ."
    echo "   This option is not supported with VPART-based layouts"
    echo ""
    echo "-k KERNEL_TYPE: uni or smp"
    echo ""
    echo "-w HWNAME: customer-specific hardware identification string"
    echo ""
    echo "-I: Ignore any image signature"
    echo ""
    echo "-r: Require image signature"
    echo ""
    echo "-F LOGFILE: also log some verbose output to the specified file"
    echo ""
    echo "-v: verbose mode.  Specify multiple times for more output"
    echo ""
    echo "-P OPER_ID: (install only) track progress using specified progress "
    echo "            operation ID (expected to be already begun)"
    echo ""
    echo "-b LOCATION_NUMBER: 1 or 2: set the next boot location to the"
    echo "                    specified one."
    echo ""
    echo "-z : fail imaging if disk is not empty.  Currently this fails if"
    echo "    there is a partition table (for msdos and gpt labels), or"
    echo "    for raw (unlabeled) targets, if the first 64k is not all zeros."
    echo "    Each target to be modified is tested in turn.  Only used during"
    echo "    manufacture, not install."
    echo ""
    echo "-y : if manufacturing fails, zero out the first 64k of all targets. "
    echo "    In the 64k that follow the first 64k, a failure message will be"
    echo "    written, which may be read by automated manufacturing tools."
    echo "    Each target to be modified is zeroed in turn.  Only used during"
    echo "    manufacture, not install."
    echo ""
    echo ""
    exit 1
}


# ==================================================
# Cleanup when we are finished or interrupted
# ==================================================
cleanup()
{
    # Get out of any dirs we might remove
    cd /

    vlecho 0 "== Cleanup"

    # XXX/EMT: in some cases, this may take a significant amount of time.
    # e.g. if we were given a large, bad image file, we have copied it,
    # and now have to sync it.  Meanwhile, the progress bar holds at 0%,
    # and the user wonders.  But it's hard to track progress in cleanup(),
    # since we don't know where we are.  Maybe we could start another step
    # of our own (without marking the prior one finished)...
    ${SYNC}

    # delete temp files and potentially unmount partitions
    if [ ! -z ${UNMOUNT_TMPFS} ]; then
        umount ${UNMOUNT_TMPFS}
    fi

    if [ ! -z ${WORKSPACE_DIR} ]; then
        rm -rf ${WORKSPACE_DIR}
    fi

    rm -f ${SYSIMAGE_FILE_TAR}

    if [ ! -z ${UNMOUNT_WORKINGFS} ]; then
        umount ${UNMOUNT_WORKINGFS}
    fi

    if [ ! -z ${UNMOUNT_EXTRACTFS} ]; then
        umount ${UNMOUNT_EXTRACTFS}
    fi

    if [ ! -z ${UNMOUNT_XMTD} ]; then
        umount ${UNMOUNT_XMTD}
    fi

    if [ ! -z ${UNMOUNT_VPART_PHYS} ]; then
        umount ${UNMOUNT_VPART_PHYS}
    fi

    rm -rf ${VPART_PHYS_PART_MOUNT}

    if [ ! -z ${UNMOUNT_VPART} ]; then
        umount ${UNMOUNT_VPART}
    fi

    rm -rf ${VPART_LOOP_MOUNT}

    if [ ! -z ${UNMOUNT_INITRD} ]; then
        umount ${UNMOUNT_INITRD}
    fi

    rm -rf ${UNMOUNT_INITRD}

    if [ ! -z "${BCP_BOOTMGR_UMOUNT}" ]; then
        umount ${BCP_BOOTMGR_UMOUNT}
    fi
    if [ "${BCP_REMOUNTED_RW}" = "1" ]; then
        mount -o remount,ro /bootmgr
    fi

    if [ ! -z "${UNMOUNTED_BOOTMGR_LIST}" ]; then
        vlecho 0 "-- Re-mounting as required after install of bootmgr"
        for curr_part in ${UNMOUNTED_BOOTMGR_LIST}; do
            eval 'part_mount="${IL_LO_'${IL_LAYOUT}'_PART_'${curr_part}'_MOUNT}"'
            eval 'curr_fstype="${IL_LO_'${IL_LAYOUT}'_PART_'${curr_part}'_FSTYPE}"'

            if [ -z "${part_mount}" ]; then
                vlechoerr -1 "*** Invalid unmounted partition ${curr_part}"
                continue
            fi

            # Use fstab to get back mounted like it was before
            mount ${part_mount} || FAILURE=1
            if [ ${FAILURE} -eq 1 ]; then
                vlechoerr -1 "*** Could not mount partition ${curr_dev} on ${part_mount}"
                continue
            fi
            if [ "${curr_fstype}" = "jffs2" ]; then
                ${SYNC}
            fi
        done
    fi
}

# ==================================================
# The time_ routines are "low level" functions to get and subtract
# times, with up to microsecond precision, depending on your 'date'
# command.  Some example usage:
#
# time_get start_time
# do_my_thing
# time_get end_time
# time_sub diff_time $end_time $start_time
# echo "It took ${diff_time} seconds to do my thing
#
#
# --------------------
#
# The timer_ routines are "high level" functions which keep track of a
# numbered set of timers.  The timers each have an overall timer, and a
# task timer.  Typical usage is:
#
# timer_task_start_print 1 "setup"
# setup
# timer_task_start_print 1 "thing 1"
# thing_1
# timer_task_start_print 1 "thing 2"
# thing_2
# timer_task_start_print 1 "thing 3"
# thing_3
# timer_task_start_print 1 "done"
#
# In this example, "1" is the timer id.  The output from this might be:
#
# ########## ==> TIMER 1: did '' starting 'setup' total: 0  task: 0
# ########## ==> TIMER 1: did 'setup' starting 'thing 1' total: .577060  task: .577060
# ########## ==> TIMER 1: did 'thing 1' starting 'thing 2' total: .604041  task: .026981
# ########## ==> TIMER 1: did 'thing 2' starting 'thing 3' total: 1.236273  task: .632232
# ########## ==> TIMER 1: did 'thing 3' starting 'done' total: 10.645142  task: 9.408869
#
# ==================================================


# ==================================================
# Get current time, in sec.usec 
# ==================================================
time_get() {
    # The %N sed is for busybox 'date', which does not do fractional
    # seconds
    eval "$1=$(date '+%s.%N' | sed 's/%N/0/' | sed 's/\.\(......\)...$/.\1/')"
}

# ==================================================
# Subtract two time values
# params: end_time start_time
# ==================================================
time_sub() {
    eval "$1=$(echo $2 $3 - p | dc)"
}

# ==================================================
# Divide a time into "n" parts
# params: ret_val time num_parts
# sets: $ret_val = $2 / $3
# ==================================================
time_per() {
    eval "$1=$(echo 6 k $2 $3 / p | dc)"
}

# ==================================================
# Starts timer "TIMER_ID", and names the new task "NEW_TASK_NAME"
# params: timer_id new_task_name
# sets: (under timer_id_${TIMER_ID}_) overall_start_time task_start_time overall_duration task_duration task_name
# ==================================================
timer_task_start() {
    time_get tts_now
    tts_id="${1}"
    tts_task_name="${2}"

    tss_c_os=
    eval 'tss_c_os="${timer_id_'${tts_id}'_overall_start_time}"'
    tss_c_ts=
    eval 'tss_c_ts="${timer_id_'${tts_id}'_task_start_time}"'
    tss_c_tn=
    eval 'tss_c_tn="${timer_id_'${tts_id}'_task_name}"'

    # Update overall start time and duration
    if [ ! -z "${tss_c_os}" ]; then
        time_sub tts_overall_duration ${tts_now} ${tss_c_os}
        eval 'timer_id_'${tts_id}'_overall_duration="${tts_overall_duration}"'
    else
        eval 'timer_id_'${tts_id}'_overall_start_time="${tts_now}"'
        eval 'timer_id_'${tts_id}'_overall_duration=0'
    fi

    # Update task start time and duration
    eval 'timer_id_'${tts_id}'_task_start_time="${tts_now}"'
    if [ ! -z "${tss_c_ts}" ]; then
        time_sub tts_task_duration ${tts_now} ${tss_c_ts}
        eval 'timer_id_'${tts_id}'_task_duration="${tts_task_duration}"'
    else
        eval 'timer_id_'${tts_id}'_task_duration="0"'
    fi

    # Update task name
    eval 'timer_id_'${tts_id}'_task_name="${tts_task_name}"'
    if [ ! -z "${tss_c_tn}" ]; then
        eval 'timer_id_'${tts_id}'_prev_task_name="${tss_c_tn}"'
    else
        eval 'timer_id_'${tts_id}'_prev_task_name=""'
    fi
}

# ==================================================
# Start timer "TIMER_ID", and name the new task "NEW_TASK_NAME".  Print
# the previous task's time info.
#
# params: timer_id new_task_name
# sets: (under timer_id_${TIMER_ID}_) overall_start_time task_start_time overall_duration task_duration task_name
# ==================================================
timer_task_start_print() {
    ttsp_id="${1}"
    ttsp_new_task_name="${2}"
    timer_task_start "${ttsp_id}" "${ttsp_new_task_name}"
    tssp_c_os=
    eval 'tssp_c_os="${timer_id_'${ttsp_id}'_overall_start_time}"'
    tssp_c_ts=
    eval 'tssp_c_ts="${timer_id_'${ttsp_id}'_task_start_time}"'
    tssp_c_od=
    eval 'tssp_c_od="${timer_id_'${ttsp_id}'_overall_duration}"'
    tssp_c_td=
    eval 'tssp_c_td="${timer_id_'${ttsp_id}'_task_duration}"'
    tssp_c_tn=
    eval 'tssp_c_tn="${timer_id_'${ttsp_id}'_task_name}"'
    p_tn=
    eval 'p_tn="${timer_id_'${ttsp_id}'_prev_task_name}"'

    if [ ! -z "${p_tn}" -o ! -z "${tssp_c_tn}" ]; then
        echo "########## ==> TIMER ${ttsp_id}: did '${p_tn}' starting '${tssp_c_tn}' total: ${tssp_c_od}  task: ${tssp_c_td}"
    else
        echo "########## ==> TIMER ${ttsp_id}: total: ${tssp_c_od}  task: ${tssp_c_td}"
    fi
}

# ==================================================
# Cleanup on fatal error
# ==================================================
cleanup_exit()
{
    cleanup

    if [ ${MARK_TARGETS_EMPTY_ON_FAILURE} -eq 1 -a ${READY_FOR_EMPTY_TARGETS} -eq 1 ]; then
        empty_targets "$*"
    fi

    END_TIME=$(date "+%Y%m%d-%H%M%S")
    vlecho 0 "====== Ending image install with error at ${END_TIME}"

    exit 1
}

# ==================================================
# Cleanup when called from 'trap' for ^C or signal
# ==================================================
trap_cleanup_exit()
{
    vlechoerr -1 "*** Imaging interrupted"
    cleanup

    if [ ${MARK_TARGETS_EMPTY_ON_FAILURE} -eq 1 -a ${READY_FOR_EMPTY_TARGETS} -eq 1 ]; then
        empty_targets "Interrupted"
    fi

    END_TIME=$(date "+%Y%m%d-%H%M%S")
    vlecho 0 "====== Ending image install by trap at ${END_TIME}"

    exit 1
}

# ==================================================
# Based on verboseness setting suppress command output
# ==================================================
do_verbose()
{
    if [ ${VERBOSE} -gt 0 ]; then
        $*
    else
        $* > /dev/null 2>&1
    fi
}

# ==================================================
# Echo or not based on VERBOSE setting
# ==================================================
vecho()
{
    level=$1
    shift

    if [ ${VERBOSE} -gt ${level} ]; then
        echo "$*"
    fi
}

# ==================================================
# Echo and log or not based on VERBOSE setting
# ==================================================
vlecho()
{
    level=$1
    shift
    local log_level=${LOG_LEVEL:-NOTICE}

    if [ ${VERBOSE:-0} -gt ${level} ]; then
        echo "$*"
        logger -p ${LOG_FACILITY:-user}.${log_level} -t "${LOG_TAG:-${0}[$$]}" "[${LOG_COMPONENT:-${0}}.${log_level}]: $*"
        if [ ${LOG_OUTPUT} -ne 0 -a ! -z "${LOG_FILE}" ]; then
            echo "$*" >> ${LOG_FILE}
        fi
    fi
}

# ==================================================
# Log or not at the stated level, based on VERBOSE setting
# ==================================================
vlog()
{
    level=$1
    shift
    local log_level=${1:-${LOG_LEVEL}}
    shift

    if [ ${VERBOSE:-0} -gt ${level} ]; then
        logger -p ${LOG_FACILITY:-user}.${log_level} -t "${LOG_TAG:-${0}[$$]}" "[${LOG_COMPONENT:-${0}}.${log_level}]: $*"
        if [ ${LOG_OUTPUT} -ne 0 -a ! -z "${LOG_FILE}" ]; then
            echo "$*" >> ${LOG_FILE}
        fi
    fi
}

# ==================================================
# Echo and log an Error or not based on VERBOSE setting
# ==================================================
vlechoerr()
{
    level=$1
    shift
    local log_level=ERR

    if [ ${VERBOSE:-0} -gt ${level} ]; then
        echo "$*"
        logger -p ${LOG_FACILITY:-user}.${log_level} -t "${LOG_TAG:-${0}[$$]}" "[${LOG_COMPONENT:-${0}}.${log_level}]: $*"
        if [ ${LOG_OUTPUT} -ne 0 -a ! -z "${LOG_FILE}" ]; then
            echo "$*" >> ${LOG_FILE}
        fi
    fi
}


# ==================================================
# Function to zero the start of a given target, and write a message
# ==================================================
empty_target()
{
    local target_dev=$1
    shift
    local target_message="$*"

    vlecho 0 "=== Emptying target ${target_dev}"

    if [ -z "${target_message}" ]; then
        target_message="Error during imaging"
    fi

    eval 'target_size="${IL_LO_'${IL_LAYOUT}'_TARGET_'${target}'_CURR_SIZE_MEGS}"'
    eval 'target_dev="${IL_LO_'${IL_LAYOUT}'_TARGET_'${target}'_DEV}"'
    FAILURE=0
    dd if=/dev/zero of=${target_dev} bs=512 count=256 > /dev/null 2>&1 || FAILURE=1
    if [ ${FAILURE} -eq 1 ]; then
        vlechoerr -1 "*** Could not zero out ${target_dev}"
        return 1
    fi

    # There could be an old GPT label at the end of the disk,
    # so zero it too.
    #
    # We use 'awk' as busybox expr does not work above 2^32
    # We conservatively decide to zero out the last 10 megs
    gpt_del_start_m=`echo ${target_size} | awk '{f= $1 - 10; if (f > 0) {print int(f)} else {print 0}}'`
    if [ ! -z "${gpt_del_start_m}" ]; then
        dd if=/dev/zero of=${target_dev} bs=1M seek=${gpt_del_start_m} > /dev/null 2>&1
    fi

    # The image file format is:
    #  [16-byte sig: IMG-MSG-1\r\n\0\0\0\0\0]
    #  [4-byte tag: 0x01=return msg] message bytes \0\r\n
    #  [4-byte tag: 0x00=done] \0

    FAILURE=0
    printf "IMG-MSG-1\r\n\0\0\0\0\0""\0\0\0\1${target_message}\0\r\n\0\0\0\0\0" | \
        dd of=${target_dev} bs=512 seek=128 > /dev/null 2>&1 || FAILURE=1
    if [ ${FAILURE} -eq 1 ]; then
        vlechoerr -1 "*** Could not write message to ${target_dev}"
        return 1
    fi

    return 0
}

# ==================================================
# Function to zero the start of each target, and write a message
# ==================================================
empty_targets()
{
    local target_message="$*"

    vlecho 0 "== Emptying targets"

    if [ ${MARK_TARGETS_EMPTY_ON_FAILURE} -eq 1 -a ${READY_FOR_EMPTY_TARGETS} -eq 1 -a \
        \( ${DO_MANUFACTURE} -eq 1  -a ! -z "${PART_CHANGE_LIST}" -a \
        ${NO_PARTITIONING} -eq 0 \) ]; then

        
        for target in ${TARGET_CHANGE_LIST}; do
            eval 'target_dev="${IL_LO_'${IL_LAYOUT}'_TARGET_'${target}'_DEV}"'
            if [ ! -z "${target_dev}" ]; then
                empty_target "${target}" "${target_message}"
            fi
        done
    fi
}

# ==================================================
# Function to compute and write partition tables
# ==================================================
do_partition_disks()
{
    # We assume caller makes sure any disk we care about is unmounted

    vlecho 0 "==== Disk partitioning"
    # Calculate a new partition table 


    # Loop over each target (things like DISK1)
    for target in ${TARGET_CHANGE_LIST}; do
        vecho 1 "target= ${target}"
        eval 'target_size="${IL_LO_'${IL_LAYOUT}'_TARGET_'${target}'_CURR_SIZE_MEGS}"'
        eval 'target_cyl_size="${IL_LO_'${IL_LAYOUT}'_TARGET_'${target}'_CURR_CYL_MEGS}"'
        eval 'target_dev="${IL_LO_'${IL_LAYOUT}'_TARGET_'${target}'_DEV}"'
        eval 'target_labeltype="${IL_LO_'${IL_LAYOUT}'_TARGET_'${target}'_LABELTYPE}"'
        if [ -z "${target_labeltype}" ]; then
            target_labeltype=msdos
        fi
        vlecho 0 "=== Calculating ${target_labeltype} partition table for ${target}"
        # We use sfdisk, unless the label is gpt, which only parted supports.
        # Note that grub 0.95 cannot boot from gpt.
        #
        # The current choices for labeltype are: msdos and gpt .

        if [ "${target_labeltype}" = "gpt" ]; then
            target_part_prog=parted
        elif [ "${target_labeltype}" = "msdos" ]; then
            target_part_prog=sfdisk
        elif [ "${target_labeltype}" = "raw" ]; then
            target_part_prog=
        else
            vlechoerr -1 "*** Target ${target} specifies bad label type ${target_labeltype}"
            cleanup_exit "Internal error: invalid image settings"
        fi

        #
        # By default, the layout specifies the size for each partition in
        # megabytes by setting SIZE.  If EXPLICIT_SECTORS is set to 1 for
        # a given target, that target will describe its partitions instead
        # with explicit start and end sector numbers through SECTOR_START 
        # and SECTOR_END.  This can ONLY be used if we are partitioning 
        # with Parted, which currently means that the disk must have a 
        # GPT label.
        #
        # NOTE: the first partition need not specify a start sector number.
        # If not specified, it will default to 34, which is the lowest
        # permissible start sector according to the GPT label type.
        #
        # If LAST_PART_FILL is set, the last partition does not need to
        # specify an end sector number for the last partition.  For anyone 
        # not using this option, and thus specifying an end sector number 
        # for the last partition, it should be noted that the last sector 
        # on the disk is referred to as -34.  This is seemingly a bug in 
        # Parted; it should be -1, but that evaluates to a sector number 
        # off the end of the disk:
        #
        #        # parted --script -- /dev/sda unit s mkpart primary ext2 5898240 -1 
        #        Warning: You requested a partition from 5898240s to 156249999s.
        #        The closest location we can manage is 5898240s to 156249966s.  Is this still acceptable to you?
        #
        # Sector number -34 is the closest number to -1 that is actually
        # accepted by Parted.
        #
        eval 'target_explicit_sectors="${IL_LO_'${IL_LAYOUT}'_TARGET_'${target}'_EXPLICIT_SECTORS}"'
        if [ -z "${target_explicit_sectors}" ]; then
            target_explicit_sectors=0
        fi
        if [ "$target_explicit_sectors" = "1" ]; then
            if [ "${target_part_prog}" != "parted" ]; then
                vlechoerr -1 "*** Target ${target} specifies EXPLICIT_SECTORS, but we're not using Parted (must set LABELTYPE=gpt)"
                cleanup_exit "Internal error: invalid image settings"
            fi
            target_parted_size_units=s
        else
            target_parted_size_units=MiB
        fi

        # Loop over each PART / partition (things like BOOTMGR)
        eval 'part_list="${TARGET_'${target}'_ALL_PARTS}"'
        eval 'part_num_list="${TARGET_'${target}'_ALL_PART_NUMS}"'
        eval 'target_last_part_fill="${IL_LO_'${IL_LAYOUT}'_TARGET_'${target}'_LAST_PART_FILL}"'

        # If we should let the last partition on a target grow to fill
        # the rest of the media
        eval 'def_last_part_fill="${IL_LO_'${IL_LAYOUT}'_OPT_LAST_PART_FILL}"'
        if [ -z "${def_last_part_fill}" ]; then
            def_last_part_fill=1
        fi
        if [ ! -z "${target_last_part_fill}" ]; then
            last_part_fill="${target_last_part_fill}"
        else
            last_part_fill="${def_last_part_fill}"
        fi
        if [ "${last_part_fill}" != "1" ]; then
            last_part_fill=0
        fi

        eval 'all_parts_fill="${IL_LO_'${IL_LAYOUT}'_OPT_ALL_PARTS_FILL}"'
        [ -z "${all_parts_fill}" ] && all_parts_fill=0
        if [ -z "${part_list}" ]; then
            # Even a virtual partition needs to have a physical
            # partition as a backing store.
            vlechoerr -1 "*** No partition list for target ${target}"
            cleanup_exit "Internal error: invalid image settings"
        fi

        if [ "${target_labeltype}" = "raw" ]; then
            vlecho 0 "== Not partitioning, as target ${target} is not labeled"
            continue
        fi
        unalloc_size=
        ############### START ALLOCATION LOOP #################################
        # Loop until all the space is allocated.
        OUTER_COUNTER=0
        while true
        do
            OUTER_COUNTER=`expr 1 + ${OUTER_COUNTER}`
            [ ${OUTER_COUNTER} -ge 2 ] && vlecho 0 Loop ${OUTER_COUNTER}
            if [ ${OUTER_COUNTER} -gt 20 ] ; then
                vlechoerr -1 Error, too many loops!
                exit
            fi
            # Pass one: calculate total request size, num partitions, aggregate
            # growth
            vlecho 0 Phase one: calculate sizes and growth aggregate
            total_size=0
            growth_cap_aggregate=0
            growth_weight_aggregate=0
            num_parts=0
            last_part=
            for part in ${part_list}; do
                vecho 1 "part= ${part}"

                # Get the size (in MB)
                eval 'num_meg="${IL_LO_'${IL_LAYOUT}'_PART_'${part}'_SIZE}"'
                if [ -z "$num_meg" ]; then
                    continue;
                fi
                if [ "$target_explicit_sectors" = "1" ]; then
                    vlechoerr -1 "*** Target ${target} specifies EXPLICIT_SECTORS, so SIZE for partition \"${part}\" is illegal"
                    cleanup_exit
                fi
                eval 'this_size="${IL_LO_'${IL_LAYOUT}'_PART_'${part}'_CURR_SIZE}"'
                [ -z ${this_size} ] && this_size=0
                if [ ${this_size} -eq 0 ] ; then
                    this_size=${num_meg}
                    eval 'IL_LO_'${IL_LAYOUT}'_PART_'${part}'_CURR_SIZE='"${num_meg}"
                fi
                total_size=`expr ${total_size} + ${this_size}`
                
                # Add up growth values as well
                eval 'growth_cap="${IL_LO_'${IL_LAYOUT}'_PART_'${part}'_GROWTHCAP}"'
                eval 'growth_weight="${IL_LO_'${IL_LAYOUT}'_PART_'${part}'_GROWTHWEIGHT}"'
                [ ${OUTER_COUNTER} -eq 1  ] && vlecho 0 $part this_size=${this_size} growth_cap=${growth_cap} growth_weight=${growth_weight}

                # Only accumulate the aggregates when the cap has not been met.
                if [ ! -z "$growth_cap" ]; then
                    if [ $this_size -lt $growth_cap ] ; then
                        growth_cap_aggregate=`expr ${growth_cap_aggregate} + ${growth_cap}`
                        if [ ! -z "$growth_weight" ]; then
                            growth_weight_aggregate=`expr ${growth_weight_aggregate} + ${growth_weight}`
                            vlecho 0 ${part} new growth_weight_aggregate=${growth_weight_aggregate}
                        fi
                        if [ -z ${growth_weight_aggregate} ] ; then
                            vlecho 0 new growth_cap_aggregate=${growth_cap_aggregate}
                        fi
                    fi
                else
                    # No cap.
                    if [ ! -z "$growth_weight" ]; then
                        growth_weight_aggregate=`expr ${growth_weight_aggregate} + ${growth_weight}`
                     vlecho 0 ${part} new growth_weight_aggregate=${growth_weight_aggregate}
                    fi
                fi
                num_parts=`expr ${num_parts} + 1`
                last_part=${part}
            done

            #
            # If we're doing explicit sector numbers, we missed out on 
            # figuring out the last partition above.  We could just move 
            # the last_part assignment higher, but do this instead to avoid
            # breaking anything esoteric.
            #
            if [ "$target_explicit_sectors" = "1" ]; then
                vlechoerr -1 "*** Target ${target} specifies EXPLICIT_SECTORS, so SIZE for partition \"${part}\" is illegal"
                cleanup_exit "Internal error: invalid image settings"
            fi
            if [ -z $unalloc_size ] ; then
                # Be conservative about rounding error: loose a cyl per partition
                unalloc_size=`expr ${target_size} - ${total_size} - \
                    ${num_parts} \* ${target_cyl_size}`
            fi

            vlecho 0 "== Device size:                ${target_size} M"
            vlecho 0 "== Allocated size:             ${total_size} M"
            vlecho 0 "== Unallocated size:           ${unalloc_size} M"

            #
            # Pass two: adjust partition sizes
            # 
            # The plan is to give each partition which is growable it's 'fair
            # share' of the unallocated space.  
            #
            # When no growth weighting is specified, grow all the partitons
            # that have caps at about the same rate.
            # This is dones by dividing the individual growth cap by the
            # aggregate growth caps and multiplying by the total unallocated
            # size. This is then clamped at the individual growth cap.
            #
            # When there is growth weighting, then use the percent of that
            # weighting. This is then clamped at the individual growth cap.
            #
            # When IL_LO_'${IL_LAYOUT}'_OPT_LAST_PART_FILL is set to '0'
            # in customer_rootflop.sh, then leftover space due to clamping is
            # used up by going around again and growing the partitions that
            # are not at their limit.
            # Otherwise, add the leftover into the last partition.
            #
            # None of this logic will happen if EXPLICIT_SECTORS was set, because
            # we already caught attempts to specify SIZE above.  And even if it
            # did, we'd still ignore the sizes below.
            #

            vlecho 0 Phase two: adjust partition sizes

            # Juniper added the use of prev_unalloc_value
            prev_unalloc_value=${unalloc_size}
            # Loop over each PART / partition (things like BOOTMGR)
            for part in ${part_list}; do
                [ ${unalloc_size} -le 0 ] && break
                vecho 1 "part= ${part}"

                eval 'growth_cap="${IL_LO_'${IL_LAYOUT}'_PART_'${part}'_GROWTHCAP}"'
                eval 'growth_weight="${IL_LO_'${IL_LAYOUT}'_PART_'${part}'_GROWTHWEIGHT}"'
                eval 'curr_size="${IL_LO_'${IL_LAYOUT}'_PART_'${part}'_CURR_SIZE}"'

                [ -z "${curr_size}" ] && continue

                # Do not add to this partition if it is already at the limit.
                if [ ! -z "${growth_cap}" ]; then
                    if [ ${curr_size} -ge ${growth_cap} ] ; then
                        continue
                    fi
                fi

                # Now calc this partition's share of the unallocated space
                if [ ! -z ${growth_weight} ] ; then
                    my_share=`expr ${unalloc_size} \* ${growth_weight} \
                        / ${growth_weight_aggregate}`
                    rate_string="${growth_weight}/${growth_weight_aggregate}"
                elif [ ! -z "${growth_cap}" ]; then
                    my_share=`expr \( ${growth_cap} \* 1000 \
                        / ${growth_cap_aggregate} \) \
                        \* ${unalloc_size} / 1000`
                    rate_string="${growth_cap}/${growth_cap_aggregate}"
                else
                    # When no growth weight or growth limit, then do not add
                    # to this partition.
                    continue
                fi
                if [ ${my_share} -eq 0 ] ; then
                    # When down to the last bits, give at least one, and recalc
                    # the unallocated size.
                    if [ ${unalloc_size} -gt 0 ] ; then
                        my_share=1
                        unalloc_size=`expr ${unalloc_size} - 1`
                    fi
                fi
                if [ ${my_share} -eq 0 ] ; then
                    continue
                fi

                # Clamp at our cap
                new_size=`expr ${curr_size} + ${my_share}`
                if [ ! -z ${growth_cap} ] ; then
                    if [ ${new_size} -gt ${growth_cap} ]; then
                        new_size=${growth_cap}
                    fi
                fi
                if [ "_${new_size}" = "_${growth_cap}" ]; then
                    vlecho 0 $part grow "${rate_string}" = ${my_share} new_size ${new_size} CLAMPED
                else
                    vlecho 0 $part grow "${rate_string}" = ${my_share} new_size ${new_size}
                fi

                eval 'IL_LO_'${IL_LAYOUT}'_PART_'${part}'_CURR_SIZE='"${new_size}"
                total_size=`expr ${total_size} + \( ${new_size} - ${curr_size} \)`
            done
            unalloc_size=`expr ${target_size} - ${total_size}`

            vlecho 0 "== Unallocated after growth:   ${unalloc_size} M"


            # When no more to allocate, stop the loop.
            [ $unalloc_size -le 0 ]  && break
            [ ${all_parts_fill} -eq 0 ] && break
            # Juniper added the use of prev_unalloc_value to break out of
            # the loop when all partitions have been assigned the maximum.
            if [ ${prev_unalloc_value} -eq ${unalloc_size} ] ; then
               vlecho 0 "== All partitions at max size. Unused: ${unalloc_size} M"
               break
            fi
        done
        ################# END ALLOCATION LOOP #################################

        # Pass three: emit partition lines for each partition

        if [ "$HAVE_WRITEIMAGE_GRAFT_J3" = "y" ] ; then
          # Juniper added writeimage_graft_J3
          writeimage_graft_J3
        fi

        temp_ptable=/tmp/ptable-${target_part_prog}-${target}.$$
        rm -f ${temp_ptable}

        if [ "${target_part_prog}" = "parted" ]; then
            printf "${PARTED} --script -- ${target_dev} mklabel ${target_labeltype}\n" >> ${temp_ptable}
        fi

        #
        # All the references to end_meg, start_meg, and num_meg below are only
        # applicable if EXPLICIT_SECTORS is not set.  But they're harmless if
        # it is, so we don't conditionalize them all, to save code clutter.
        #
        end_meg=0
        seen_extended=0
        part_num=0

        # Loop over each PART / partition (things like BOOTMGR) in number order
        # and put a line for each in our temp_ptable file.
        for pn in ${part_num_list}; do
            
            # Find this numbered partition
            found=0
            for part in ${part_list}; do

                eval 'curr_num="${IL_LO_'${IL_LAYOUT}'_PART_'${part}'_PART_NUM}"'
                if [ ${curr_num} -ne ${pn} ]; then
                    continue;
                fi

                vecho 1 "part= ${part}"

                part_num=`expr ${part_num} + 1`
                start_meg=${end_meg}
                num_meg=
                id=
                bootable=

                eval 'part_id="${IL_LO_'${IL_LAYOUT}'_PART_'${part}'_PART_ID}"'
                eval 'curr_size="${IL_LO_'${IL_LAYOUT}'_PART_'${part}'_CURR_SIZE}"'
                eval 'is_bootable="${IL_LO_'${IL_LAYOUT}'_PART_'${part}'_BOOTABLE}"'

                id=${part_id}
                num_meg=${curr_size}
                [ -z ${num_meg} ] && num_meg=0

                if [ "$target_explicit_sectors" = "1" ]; then
                    eval 'start_sector="${IL_LO_'${IL_LAYOUT}'_PART_'${part}'_SECTOR_START}"'
                    eval 'end_sector="${IL_LO_'${IL_LAYOUT}'_PART_'${part}'_SECTOR_END}"'
                    if [ -z "$start_sector" ]; then
                        if [ $part_num -eq 1 ]; then
                            start_sector=34
                        else
                            vlechoerr -1 "*** Partition ${part} for target ${target} does not specify START_SECTOR"
                            cleanup_exit "Internal error: invalid image settings"
                        fi
                    fi

                    start_loc=$start_sector

                    # See comments above re: why this is -34 instead of -1.
                    end_of_disk=-34s
                else
                    start_loc=$start_meg

                    # We had this at -1s for quite a while, and at least one
                    # customer used it successfully.  It also didn't cause
                    # problems in my testing (which is strange, because it
                    # DID cause problems when I was testing EXPLICIT_SECTORS).
                    # But per bug 12107, someone did have a problem with it.
                    # So use -34s here too, just to be safe.  The worse that
                    # can happen is that we waste 33 sectors; no big deal.
                    end_of_disk=-34s
                fi

                # Only used by sfdisk
                if [ ! -z ${is_bootable} ]; then
                    if [ "${target_part_prog}" = "sfdisk" ]; then
                        bootable='*'
                    fi
                fi

                # Only used by parted
                fstype_str=
                parttype_str=
                if [ "${target_part_prog}" = "parted" ]; then
                    case "${part_id}" in
                        # XXXX/EMT: are these new codes for vfat correct?
                        # Will it be usually/always wanted with or without LBA?
                        06) fstype_str="fat16" ;;
                        0b) fstype_str="fat32" ;;
                        0c) fstype_str="fat32" ;; # LBA
                        0e) fstype_str="fat16" ;; # LBA
                        83) fstype_str="ext2" ;;
                        82) fstype_str="linux-swap" ;;
                         *) fstype_str="invalid" ;;
                    esac

                    if [ ${seen_extended} -eq 0 ]; then
                        parttype_str=primary
                    else
                        parttype_str=logical
                    fi
                fi


                # Special case: the extended partitions partition
                if [ "$id" = "0f" ]; then
                    found=1
                    seen_extended=1

                    if [ "${target_part_prog}" = "sfdisk" ]; then
                        printf ",,${id},${bootable}\n" >> ${temp_ptable}
                    else
                        printf "${PARTED} --script -- ${target_dev} unit ${target_parted_size_units} mkpart extended ${start_loc} ${end_of_disk}\n" >> ${temp_ptable}
                    fi

                    break
                fi


                if [ "$target_explicit_sectors" = "1" ]; then
                    end_loc=$end_sector
                else
                    # For first partition, we budget some slack for the stuff
                    # at the start of the disk.
                    if [ "$pn" = "1" ]; then
                        num_meg=`expr ${num_meg} - 1`
                    fi

                    num_meg_str=${num_meg}
                    end_meg=`expr ${start_meg} + ${num_meg}`
                    end_loc=$end_meg
                fi

                # If we're the last partition, may take all the remaining space
                if [ ${last_part_fill} -eq 1 -a "${part}" = "${last_part}" ]; then
                    num_meg=`expr ${num_meg} + $unalloc_size `
                    vlecho 0 "${part} ${num_meg} MB FILL"
                    if [ "${target_part_prog}" = "sfdisk" ]; then
                        num_meg_str=
                    else
                        num_meg_str=
                        end_loc=${end_of_disk}
                    fi
                else
                    vlecho 0 "${part} ${num_meg_str} MB"
                fi

                # Write out the partition lines
                if [ "${target_part_prog}" = "sfdisk" ]; then
                    printf ",${num_meg_str},${id},${bootable}\n" >> ${temp_ptable}
                else
                    printf "${PARTED} --script -- ${target_dev} unit ${target_parted_size_units} mkpart ${parttype_str} ${fstype_str} ${start_loc} ${end_loc}\n" >> ${temp_ptable}
                    if [ ! -z ${is_bootable} ]; then
                        printf "${PARTED} --script -- ${target_dev} set ${part_num} boot on\n" >> ${temp_ptable}
                    fi
                fi

                found=1
                break
            done

            if [ ${found} -ne 1 ]; then
                vlechoerr -1 "*** Could not find partition number ${pn} for target ${target}"
                cleanup_exit "Internal error: invalid image settings"
            fi

        done

        vlecho 0 "=== Zero'ing existing partition table on ${target_dev}"

        # We're about to zero the disk, so maybe dump the partiton table first
        if [ ${VERBOSE} -gt 0 ]; then
            if [ "${target_part_prog}" = "sfdisk" ]; then
                # Note, no need for 'do_verbose' here
                # XXX #dep/parse: sfdisk
                sfdisk -l ${target_dev} 2>&1 | grep -v '^WARNING: GPT.*$'
            else
                do_verbose ${PARTED} --script -- ${target_dev} unit ${target_parted_size_units} print
            fi
        fi

        FAILURE=0
        dd if=/dev/zero of=${target_dev} bs=512 count=2 > /dev/null 2>&1 || FAILURE=1
        if [ ${FAILURE} -eq 1 ]; then
            vlechoerr -1 "*** Could not zero out ${target_dev}"
            cleanup_exit "Could not zero media"
        fi

        # There could be an old GPT label at the end of the disk,
        # so zero it too.
        #
        # We use 'awk' as busybox expr does not work above 2^32
        # We conservatively decide to zero out the last 10 megs
        gpt_del_start_m=`echo ${target_size} | awk '{f= $1 - 10; if (f > 0) {print int(f)} else {print 0}}'`
        if [ ! -z "${gpt_del_start_m}" ]; then
            dd if=/dev/zero of=${target_dev} bs=1M seek=${gpt_del_start_m} > /dev/null 2>&1
        fi

        vlecho 0 "=== Writing partition table to ${target}"
        FAILURE=0
        if [ "${target_part_prog}" = "sfdisk" ]; then
            cat ${temp_ptable} | do_verbose sfdisk -uM ${target_dev} || FAILURE=1
        else
            sh ${temp_ptable} || FAILURE=1
            if [ ${VERBOSE} -gt 0 ]; then
                ${PARTED} --script -- ${target_dev} unit ${target_parted_size_units} print || FAILURE=1
            fi
        fi
        if [ ${FAILURE} -eq 1 ]; then
            vlechoerr -1 "*** Could not make partition table on ${target_dev}"
            cleanup_exit "Could not partition media"
        fi

        if [ "$HAVE_WRITEIMAGE_GRAFT_J4" = "y" ] ; then
            # Juniper added writeimage_graft_J4
            writeimage_graft_J4
        fi

    done
}



# ==================================================
# Make new filesystems on disk or disks
# ==================================================
do_make_filesystems()
{
    # We assume caller makes sure any partition we care about is unmounted

    vlecho 0 "==== Making filesystems"

    # First, count how many filesystems there are to make, for progress
    # tracking.
    numfs=0
    for target in ${TARGET_CHANGE_LIST}; do
        eval 'change_parts="${TARGET_'${target}'_CHANGE_PARTS}"'
        for part in ${change_parts}; do
            numfs=$((${numfs} + 1))
        done
        eval 'change_vparts="${TARGET_'${target}'_CHANGE_VPARTS}"'
        for vpart in ${change_vparts}; do
            numfs=$((${numfs} + 1))
        done
    done
    
    if [ ${numfs} -gt 0 ]; then
        numsteps=${numfs}
        pctperstep=$((100 / ${numsteps}))
        currpct=$((0 - ${pctperstep}))
    else
        numsteps=0
        pctperstep=100
        currpct=0
    fi

    # ==================================================
    # Create filesystems / swap
    # ==================================================
    for target in ${TARGET_CHANGE_LIST}; do
        vecho 1 "target= ${target}"

        eval 'change_parts="${TARGET_'${target}'_CHANGE_PARTS}"'

        for part in ${change_parts}; do

            if [ ${TRACK_PROGRESS} -eq 1 ]; then
                currpct=$((${currpct} + ${pctperstep}))
                ${MDREQ} action /progress/actions/update_step \
                    oper_id string ${PROGRESS_OPER_ID} \
                    percent_done float32 ${currpct} \
                    step_status string "Making filesystem ${part} on ${target}"
            fi

            vecho 1 "part= ${part}"

            eval 'target_type="${IL_LO_'${IL_LAYOUT}'_TARGET_'${target}'_TYPE}"'
            if [ -z "${target_type}" ]; then
                target_type=block
            fi
            eval 'part_id="${IL_LO_'${IL_LAYOUT}'_PART_'${part}'_PART_ID}"'
            eval 'dev="${IL_LO_'${IL_LAYOUT}'_PART_'${part}'_DEV}"'
            eval 'cdev="${IL_LO_'${IL_LAYOUT}'_PART_'${part}'_DEV_CHAR}"'
            eval 'label="${IL_LO_'${IL_LAYOUT}'_PART_'${part}'_LABEL}"'
            eval 'fstype="${IL_LO_'${IL_LAYOUT}'_PART_'${part}'_FSTYPE}"'
            eval 'part_mount="${IL_LO_'${IL_LAYOUT}'_PART_'${part}'_MOUNT}"'

            if [ -z "${part_id}" -o -z "${dev}" ]; then
                continue;
            fi

            if [ "$fstype" = "ext2" -o "$fstype" = "ext3" -o "$fstype" = "ext4" ]; then
                fs_args=
                if [ "$fstype" = "ext3" -o "$fstype" = "ext4" ]; then
                    fs_args=-j
                fi
                if [ "${MKE2FS_HAS_FSTYPE}" = "1" ]; then
                    fs_args="${fs_args} -t ${fstype}"
                fi

                label_args=
                if [ ! -z "${label}" ]; then
                    label_args="-L ${label}"
                fi

                vlecho 0 "== Creating ${fstype} filesystem on ${dev} for ${part}"

                mkfs=${MKE2FS}

                FAILURE=0

                if [ ${TRACK_PROGRESS} -eq 1 ]; then
                    currrangehi=$((${currpct} + ${pctperstep} - 1))
                    ${PROGRESS} -i ${PROGRESS_OPER_ID} -l ${currpct} -h ${currrangehi} -- ${mkfs} ${MKE2FS_BASE_OPTIONS} -q ${label_args} ${fs_args} ${dev} || FAILURE=1
                else
                    ${mkfs} ${MKE2FS_BASE_OPTIONS} -q ${label_args} ${fs_args} ${dev} || FAILURE=1
                fi
                if [ ${FAILURE} -eq 1 ]; then
                    vlechoerr -1 "*** Could not make filesystem on ${dev}"
                    cleanup_exit "Could not make filesystem"
                fi
            fi

            if [ "$fstype" = "vfat" ]; then

                # Make sure the label doesn't exceed 11 chars
                label_args=
                if [ ! -z "${label}" ]; then
                    label="$(echo "${label}" | sed 's/^\(...........\).*$/\1/')"
                    label_args="-n ${label}"
                fi

                vlecho 0 "== Creating ${fstype} filesystem on ${dev} for ${part}"

                mkfs=${MKFS_VFAT}
                vfat_args="-F 16"
                if [ "${part_id}" = "06" -o "${part_id}" = "0e" ]; then
                    vfat_args="-F 16"
                elif [ "${part_id}" = "0b" -o "${part_id}" = "0c" ]; then
                    vfat_args="-F 32"
                fi

                FAILURE=0

                if [ ${TRACK_PROGRESS} -eq 1 ]; then
                    currrangehi=$((${currpct} + ${pctperstep} - 1))
                    do_verbose ${PROGRESS} -i ${PROGRESS_OPER_ID} -l ${currpct} -h ${currrangehi} -- ${mkfs} ${vfat_args} ${label_args} ${dev} || FAILURE=1
                else
                    do_verbose ${mkfs} ${vfat_args} ${label_args} ${dev} || FAILURE=1
                fi
                if [ ${FAILURE} -eq 1 ]; then
                    vlechoerr -1 "*** Could not make filesystem on ${dev}"
                    cleanup_exit "Could not make filesystem"
                fi
            fi

            if [ "$fstype" = "swap" ]; then
                label_args=
                if [ ! -z "${label}" ]; then
                    label_args="-L ${label}"
                fi

                vlecho 0 "== Making swap on ${dev} for ${part}"
                FAILURE=0
                do_verbose mkswap ${label_args} ${dev} || FAILURE=1
                if [ ${FAILURE} -eq 1 ]; then
                    vlechoerr -1 "*** Could not make swap on ${dev}"
                    cleanup_exit "Could not make swap filesystem"
                fi
            fi

            if [ "$fstype" = "jffs2" ]; then
                # XXX-mtd? : What if this isn't an MTD?  Should check if raw?
                
                vlecho 0 "== Erasing MTD JFFS2 partition on ${cdev} for ${part}"

                # XXXX-mtd: progress!

                FAILURE=0
                do_verbose ${FLASH_ERASEALL} ${cdev} || FAILURE=1
                if [ ${FAILURE} -eq 1 ]; then
                    vlechoerr -1 "*** Could not erase MTD JFFS2 partition ${cdev}"
                    cleanup_exit "Could not erase MTD"
                fi
            fi

            if [ -z "$fstype" ]; then
                if [ "${part_id}" = "da" ]; then
                    vlecho 0 "== Zero'ing first 4 meg of ${dev} for ${part}"
                    FAILURE=0
                    dd if=/dev/zero of=${dev} bs=512 count=8192 > /dev/null 2>&1 || FAILURE=1
                    if [ ${FAILURE} -eq 1 ]; then
                        vlechoerr -1 "*** Could not zero ${mp}"
                        cleanup_exit "Could not zero media"
                    fi
                elif [ "${part_id}" = "d9" ]; then
                    vlecho 0 "== Erasing non-fs MTD partition on ${cdev} for ${part}"

                    # XXXX-mtd: progress!

                    FAILURE=0
                    do_verbose ${FLASH_ERASEALL} ${cdev} || FAILURE=1
                    if [ ${FAILURE} -eq 1 ]; then
                        vlechoerr -1 "*** Could not zero non-fs MTD partition ${cdev}"
                        cleanup_exit "Could not zero media"
                    fi
                elif [ "${part_mount}" = "" ]; then
                    vlecho 0 "== Nothing to do on ${dev} for ${part}"
                    continue
                else
                    vlechoerr -1 "*** No filesystem made for unknown partition id ${part_id} on ${dev}"
                fi
            fi
        done

        # MUST be run after physical partitions are made so the backing
        # partitions can be mounted

        # XXX-mtd: vpart on MTD not supported

        eval 'change_vparts="${TARGET_'${target}'_CHANGE_VPARTS}"'
        for vpart in ${change_vparts}; do

            if [ ${TRACK_PROGRESS} -eq 1 ]; then
                currpct=$((${currpct} + ${pctperstep}))
                ${MDREQ} action /progress/actions/update_step \
                    oper_id string ${PROGRESS_OPER_ID} \
                    percent_done float32 ${currpct} \
                    step_status string "Making virtual filesystem ${part} on ${target}"
            fi

            eval 'part_name="${IL_LO_'${IL_LAYOUT}'_VPART_'${vpart}'_PART_NAME}"'
            vecho 1 "vpart= ${vpart} backing part=${part_name}"

            eval 'vp_file_name="${IL_LO_'${IL_LAYOUT}'_VPART_'${vpart}'_FILE_NAME}"'
            eval 'c_size="${IL_LO_'${IL_LAYOUT}'_VPART_'${vpart}'_CONTENT_SIZE}"'
            eval 'v_fstype="${IL_LO_'${IL_LAYOUT}'_VPART_'${vpart}'_FSTYPE}"'
            eval 'v_label="${IL_LO_'${IL_LAYOUT}'_VPART_'${vpart}'_LABEL}"'
            eval 'inode_count="${IL_LO_'${IL_LAYOUT}'_VPART_'${vpart}'_INODE_COUNT}"'

            eval 'part_id="${IL_LO_'${IL_LAYOUT}'_PART_'${part_name}'_PART_ID}"'

            eval 'dev="${IL_LO_'${IL_LAYOUT}'_PART_'${part_name}'_DEV}"'
            eval 'cdev="${IL_LO_'${IL_LAYOUT}'_PART_'${part_name}'_DEV_CHAR}"'
            eval 'part_mount="${IL_LO_'${IL_LAYOUT}'_PART_'${part_name}'_MOUNT}"'

            if [ ${DO_MANUFACTURE} -eq 1 ]; then
                phys_part_mount=${VPART_PHYS_PART_MOUNT}
            else
                # Should be already mounted on a running system
                phys_part_mount=${part_mount}
            fi

            if [ -z "${part_name}" -o -z "${vp_file_name}" ]; then
                vlechoerr -1 "*** Configuration error for virtual ${vpart} must specify vpart name"
                cleanup_exit "Internal error: invalid image settings"
            fi

            if [ -z "${inode_count}" ]; then
                vlechoerr -1 "*** Need to specify inode count for virtual ${vpar}"
                cleanup_exit "Internal error: invalid image settings"
            fi

            if [ -z "${part_id}" -o -z "${dev}" -o -z "${phys_part_mount}" ]; then
                vlechoerr -1 "*** No physical partition for virtual ${vpart}"
                cleanup_exit "Internal error: invalid image settings"
            fi

            # Mount the backing partition
            if [ ${DO_MANUFACTURE} -eq 1 ]; then
                mkdir -p ${phys_part_mount}
                FAILURE=0
                mount ${dev} ${phys_part_mount} || FAILURE=1
                if [ ${FAILURE} -eq 1 ]; then
                    vlechoerr -1 "*** Could not mount partition ${dev} on ${phys_part_mount}"
                    cleanup_exit
                fi
            else
                mount -o remount,rw ${phys_part_mount} || FAILURE=1
                if [ ${FAILURE} -eq 1 ]; then
                    vlechoerr -1 "*** Could not remount partition ${dev} on ${phys_part_mount}"
                    cleanup_exit
                fi
            fi

            UNMOUNT_VPART_PHYS=${phys_part_mount}
            vecho 1 "mount ${dev} on ${phys_part_mount}"

            vp_path=${phys_part_mount}/${vp_file_name}
            rm -f ${vp_path}

            tvp_path=`dirname ${vp_path}`
            mkdir -p ${tvp_path}

            label_args=
            if [ ! -z "${v_label}" ]; then
                label_args="-L ${v_label}"
            fi

            vlecho 0 "== Creating virtual partition image for ${vpart}"

            FAILURE=0
            dd if=/dev/zero of=${vp_path} bs=1M count=${c_size} > /dev/null 2>&1 || FAILURE=1
            if [ ${FAILURE} -eq 1 ]; then
                vlechoerr -1 "*** Could not zero ${vp_path}"
                cleanup_exit "Could not zero media"
            fi

            FAILURE=0

            if [ ${TRACK_PROGRESS} -eq 1 ]; then
                currrangehi=$((${currpct} + ${pctperstep} - 1))
                ${PROGRESS} -i ${PROGRESS_OPER_ID} -l ${currpct} -h ${currrangehi} -- ${MKE2FS} ${MKE2FS_BASE_OPTIONS} -q ${label_args} -m 0 -N ${inode_count} -F ${vp_path} || FAILURE=1
            else
                ${MKE2FS} ${MKE2FS_BASE_OPTIONS} -q ${label_args} -m 0 -N ${inode_count} -F ${vp_path} || FAILURE=1
            fi

            if [ ${FAILURE} -eq 1 ]; then
                vlechoerr -1 "*** Could not make filesystem on ${dev}"
                cleanup_exit "Could not make filesystem"
            fi

            tmount_dir=${VPART_LOOP_MOUNT}
            mkdir -p ${tmount_dir}
            vlecho 1 "== Removing lost+found from ${tmount_dir}"
            FAILURE=0
            mount -o loop,noatime -t ${v_fstype} ${vp_path} ${tmount_dir} || FAILURE=1
            if [ ${FAILURE} -eq 1 ]; then
                vlechoerr -1 "*** Could not mount ${vp_path} on ${tmount_dir}"
                cleanup_exit
            fi

            UNMOUNT_VPART=${tmount_dir}

            rmdir ${tmount_dir}/lost+found

            umount ${tmount_dir}
            UNMOUNT_VPART=

            # Unmount the backing partition
            if [ ${DO_MANUFACTURE} -eq 1 ]; then
                umount ${phys_part_mount}
            else
                mount -o remount,ro ${phys_part_mount}
            fi
            UNMOUNT_VPART_PHYS=
        done

    done
}

# Size chosen to be the largest floppy size
# XXX Is there a max size the initrd can be?
INITRD_SIZE=2880

# ======================================================
# Make an initrd for helping in loading a virtual rootfs
# ======================================================
do_make_initrd()
{
    initrd_name=$1
    do_gzip=$2
    rootfs_dev=$3
    root_mount_type=$4
    # The next 3 args pertain to when root fs is to be mounted
    # on the loopback file system
    rootfs_path=$5
    rootfs_size=$6
    rootfs_fstype=$7

    # 2.4 kernel doesn't have sysfs
    uname -r | grep -q ^2.4
    if [ $? -eq 0 ]; then
        no_sysfs=1
        # 2.4 seems to have a problem mount a loop back file
        # in a tmpfs. XXX could be sizing issue...
        initrd_file=/tmp/initrd_${initrd_name}
    else
        no_sysfs=0
        initrd_file=${WORKSPACE_DIR}/initrd_${initrd_name}
    fi

    initrd_mnt=/tmp/mnt_initrd.$$

    rm -f ${initrd_file}

    dd if=/dev/zero of=${initrd_file} bs=1k count=${INITRD_SIZE} > /dev/null 2>&1
    ${MKE2FS} ${MKE2FS_BASE_OPTIONS} -q -m 0 -N 400 -F ${initrd_file}
    mkdir -p ${initrd_mnt}

    FAILURE=0
    mount -t ext2 -o loop ${initrd_file} ${initrd_mnt} || FAILURE=1
    if [ ${FAILURE} -eq 1 ]; then
        vlechoerr -1 "*** Initrd: Could not mount ${initrd_file} on ${initrd_mnt}"
        cleanup_exit
    fi
    UNMOUNT_INITRD=${initrd_mnt}
    rmdir ${initrd_mnt}/lost+found

    # Create the small rootfs
    cd ${initrd_mnt}/

    # Make all directories
    mkdir bin
    mkdir sbin
    mkdir -p usr/sbin
    mkdir -p usr/bin
    mkdir dev
    mkdir etc
    mkdir lib
    mkdir proc
    mkdir sys
    mkdir sysroot
    mkdir mnt
    mkdir tmp
    if [ "${root_mount_type}" = "tmpfs" ]; then
        mkdir loopfs
        mkdir rootfs
    elif [ "${root_mount_type}" = "loop" ]; then
        mkdir rootfs
    fi

    cat > ./linuxrc <<EOF
#!/bin/sh
# Automatically generated file: DO NOT EDIT!
#

PATH=/usr/bin:/bin:/usr/sbin:/sbin
export PATH

echo Running initrd linuxrc

mount -t proc /proc /proc
EOF

if [ ${no_sysfs} -ne 1 ]; then
    cat >> ./linuxrc <<EOF
mount -t sysfs none /sys
EOF
fi

    if [ "${root_mount_type}" = "tmpfs" ]; then
    cat >> ./linuxrc <<EOF
# This will be our root fs
mount -t tmpfs -o size=${rootfs_size}M tmpfs /rootfs
echo Created tmpfs for root fs

mount ${rootfs_dev} /mnt
echo Mounted partition ${rootfs_dev} that contains root fs image

## XXX cp/gunzip /mnt/${rootfs_path} /tmp/XXX
## XXX Need RAM space to uncompress image and have room for fs
mount -o loop -t ${rootfs_fstype} /mnt/${rootfs_path} /loopfs

cd /loopfs
find . -print | cpio -p -a -B -d -m -u /rootfs
cd /rootfs
echo Transferred root fs image to /rootfs

# Done with root fs loop image and backing partition
umount /loopfs
umount /mnt

echo Switching to new root
cd /rootfs
EOF

if [ "$HAVE_WRITEIMAGE_GRAFT_6" = "y" ]; then
    writeimage_graft_6
fi
    fi

if [ "$HAVE_WRITEIMAGE_GRAFT_7" = "y" ]; then
    writeimage_graft_7
fi

    cat >> ./linuxrc <<EOF
mkdir -p old-root
pivot_root . old-root
EOF

if [ ${no_sysfs} -ne 1 ]; then
    cat >> ./linuxrc <<EOF
exec chroot . sh -c 'umount /old-root/proc; umount /old-root/sys; umount /old-root; exec /sbin/init' <dev/console >dev/console 2>&1
EOF
else
    cat >> ./linuxrc <<EOF
exec chroot . sh -c 'umount /old-root/proc; umount /old-root; exec /sbin/init' <dev/console >dev/console 2>&1
EOF
fi

    chmod 755 linuxrc

    cd ${initrd_mnt}/bin
    # Steal the busybox from the manufacturing environment
    cp -p /bin/busybox .
    cp -p /bin/cpio .
    ln -s busybox mount
    ln -s busybox umount
    ln -s busybox echo
    ln -s busybox sh
    ln -s busybox mkdir
    ln -s busybox gzip
    ln -s busybox gunzip
    ln -s busybox cp

    cd ${initrd_mnt}/etc
    ln -s /proc/mounts mtab

    cd ${initrd_mnt}/lib
    cp -p /lib/libc-2.3.*.so .
    cp -p /lib/libcrypt-2.3.*.so .
    cp -p /lib/ld-2.3.*.so .
    ln -s libc-2.3.*.so libc.so.6
    ln -s libcrypt-2.3.*.so libcrypt.so.1
    ln -s ld-2.3.*.so ld-linux.so.2

    cd ${initrd_mnt}/sbin
    ln -s ../bin/busybox pivot_root

    cd ${initrd_mnt}/usr/sbin
    ln -s ../../bin/busybox chroot

    cd ${initrd_mnt}/usr/bin
    ln -s ../../bin/busybox find

    # XXX device creation part taken from mkbootfl.sh (really should share!)
    cd ${initrd_mnt}/dev
    mknod console c 5 1
    mknod full c 1 7
    mknod kmem c 1 2
    mknod mem c 1 1
    mknod null c 1 3
    mknod port c 1 4
    mknod random c 1 8
    mknod urandom c 1 9
    mknod zero c 1 5
    ln -s /proc/kcore core

    # loop devs
    for i in `seq 0 7`; do
        mknod loop$i b 7 $i
    done

    # ram devs
    for i in `seq 0 9`; do
        mknod ram$i b 1 $i
    done
    ln -s ram1 ram

    # ttys
    mknod tty c 5 0
    for i in `seq 0 9`; do
        mknod tty$i c 4 $i
    done

    # ttySs
    for i in `seq 0 2`; do
        mknod ttyS$i c 4 `expr 64 + $i`
    done

    # virtual console screen devs
    for i in `seq 0 9`; do
        mknod vcs$i b 7 $i
    done
    ln -s vcs0 vcs

    # virtual console screen w/ attributes devs
    for i in `seq 0 9`; do
        mknod vcsa$i b 7 $i
    done
    ln -s vcsa0 vcsa

    # ide hda
    for i in `seq 1 15`; do
        mknod hda$i b 3 $i
    done
    mknod hda b 3 0

    # ide hdb
    for i in `seq 1 15`; do
        mknod hdb$i b 3 `expr 64 + $i`
    done
    mknod hdb b 3 64

    # ide hdc
    for i in `seq 1 15`; do
        mknod hdc$i b 22 $i
    done
    mknod hdc b 22 0

    # ide hdd
    for i in `seq 1 15`; do
        mknod hdd$i b 22 `expr 64 + $i`
    done
    mknod hdd b 22 64

    # scsi hda
    for i in `seq 0 15`; do
        mknod sda$i b 8 `expr 16 \* 0 + $i`
    done
    mknod sda b 8 `expr 16 \* 0 + 0`

    # scsi hdb
    for i in `seq 0 15`; do
        mknod sdb$i b 8 `expr 16 \* 1 + $i`
    done
    mknod sdb b 8 `expr 16 \* 1 + 0`

    # pty
    for i in `seq 0 31`; do
        mknod pty$i c 2 $i
    done

    # fd
    for i in `seq 0 1`; do
        mknod fd$i b 2 $i
        mknod fd${i}H1440 b 2 `expr 28 + $i`
    done

    # Cleanup
    cd /tmp
    umount ${initrd_mnt}
    rmdir ${initrd_mnt}
    UNMOUNT_INITRD=

    if [ ${do_gzip} -eq 1 ]; then
        vecho 1 "gzip initrd ${initrd_file}"
        FAILURE=0
        gzip -9 ${initrd_file} || FAILURE=1
        if [ ${FAILURE} -eq 1 ]; then
            vlechoerr -1 "*** gzip of initrd ${initrd_file} failed"
            cleanup_exit
        fi
        mv -f ${initrd_file}.gz ${initrd_file}
    fi
}

# ==================================================
# Parse command line arguments, setup
# ==================================================

LOG_COMPONENT="writeimage"
LOG_TAG="${LOG_COMPONENT}[$$]"
# LOG_LEVEL is uppercase, facility lower
LOG_FACILITY=user
LOG_LEVEL=NOTICE
LOG_OUTPUT=0
LOG_FILE=

AISET_PATH=/sbin/aiset.sh
TPKG_QUERY=/sbin/tpkg_query.sh
TPKG_EXTRACT=/sbin/tpkg_extract.sh
MDREQ=/opt/tms/bin/mdreq
PROGRESS=/opt/tms/bin/progress
BZCAT=/usr/bin/bzcat
ZCAT=/bin/zcat
SYNC=/bin/sync
TAR=/bin/tar
MKE2FS=/sbin/mke2fs
MKFS_VFAT=/sbin/mkfs.vfat
CP=/bin/cp
FLASH_ERASEALL=/usr/sbin/flash_eraseall

BV_PATH=/etc/build_version.sh
if [ -r ${BV_PATH} ]; then
    . ${BV_PATH}
else
    vlechoerr -1 "*** Invalid build version"
    BUILD_PROD_CUSTOMER=invalid
fi

PARSE=`/usr/bin/getopt -s sh 'miu:f:k:d:l:O:Bp:s:L:tT:vw:F:IrP:b:zy' "$@"`

if [ $? != 0 ] ; then
    usage
fi

eval set -- "$PARSE"

# Defaults
DO_MANUFACTURE=-1
SYSIMAGE_USE_URL=-1
USE_TMPFS=1
TMPFS_SIZE_MB=512
# Juniper specific change, make TMPFS_SIZE_MB larger.
TMPFS_SIZE_MB=1024
SYSIMAGE_FILE=
SYSIMAGE_URL=
INSTALL_IMAGE_ID=-1
DO_REPLICATE_DISK=0
IMAGE_LAYOUT=STD
USER_TARGET_DEVS=
USER_TARGET_DEV_COUNT=0
INSTALL_BOOTMGR=0
REQUIRE_EMPTY_TARGETS=0
MARK_TARGETS_EMPTY_ON_FAILURE=0
READY_FOR_EMPTY_TARGETS=0

# Hard-coded minimum free space required for ${TMP_LOCAL_WORKSPACE}
# (usually in /var) before attempting to upgrade the image.
# (XXX would really like to have a manifest in the image to know how
# big the image really is and use this). Default is 300MB.
WORKSPACE_MIN_AVAIL_SPACE=300000
WORKSPACE_LOC=VAR_1

VPART_PHYS_PART_MOUNT=/tmp/phys_part_mnt.$$
VPART_LOOP_MOUNT=/tmp/vpart_loop_mnt.$$
PARTED=parted
SIG_IGNORE=0
SIG_REQUIRE=0
HWNAME_PASSED=0

USER_LOC_OVERRIDE_ADD=
USER_LOC_OVERRIDE_REMOVE=
USER_LOC_OVERRIDE_REMOVE_ALL=0
VERBOSE=0
VERBOSE_ARGS=
TPKG_ARGS=
LAST_PART=

TRACK_PROGRESS=0
PROGRESS_OPER_ID=
SWITCH_BOOT_LOC=0
NEXT_BOOT_LOC=1

while true ; do
    case "$1" in
        -m) DO_MANUFACTURE=1; shift ;;
        -i) DO_MANUFACTURE=0; shift ;;
        -u) SYSIMAGE_USE_URL=1; SYSIMAGE_URL=$2; shift 2 ;;
        -f) SYSIMAGE_USE_URL=0; SYSIMAGE_FILE=$2; shift 2 ;;
        -k) IL_KERNEL_TYPE=$2; shift 2 ;;
        -w) IL_HWNAME=$2; HWNAME_PASSED=1 ; shift 2 ;;
        -d) 
            new_disk=$2; shift 2
            echo $new_disk | grep -q "^/dev"
            if [ $? -eq 1 ]; then
                usage
            fi
            USER_TARGET_DEV_COUNT=$((${USER_TARGET_DEV_COUNT} + 1))
            USER_TARGET_DEVS="${USER_TARGET_DEVS} ${new_disk}"
            eval 'USER_TARGET_DEV_'${USER_TARGET_DEV_COUNT}'="${new_disk}"'
            ;;
        -l) INSTALL_IMAGE_ID=$2; shift 2 ;;
        -O)
            new_loc_override=$2; shift 2
            if [ -z "${new_loc_override}" ]; then
                usage
            fi

            is_remove=1
            case "${new_loc_override}" in
                -*) is_remove=1; new_loc_override=`echo ${new_loc_override} | sed 's/^.\(.*\)$/\1/'` ;;
                +*) is_remove=0; new_loc_override=`echo ${new_loc_override} | sed 's/^.\(.*\)$/\1/'` ;;
            esac

            if [ ${is_remove} -eq 1 ]; then
                if [ "${new_loc_override}" != "-all-" ]; then
                    USER_LOC_OVERRIDE_REMOVE="${USER_LOC_OVERRIDE_REMOVE} ${new_loc_override}"
                    eval 'USER_LOC_OVERRIDE_REMOVE_LOC_'${new_loc_override}'="1"'
                else
                    USER_LOC_OVERRIDE_REMOVE_ALL=1
                fi
            else
                USER_LOC_OVERRIDE_ADD="${USER_LOC_OVERRIDE_ADD} ${new_loc_override}"
                eval 'USER_LOC_OVERRIDE_ADD_LOC_'${new_loc_override}'="1"'
            fi
            ;;
        -B) INSTALL_BOOTMGR=1; shift ;;
        -p) LAST_PART=$2; shift 2 ;;
        -s) 
            LAST_PART_SIZE=$2; shift 2 
            if [ -z "${LAST_PART}" ]; then
                usage
            fi
            eval 'USER_PART_'${LAST_PART}'_SIZE="${LAST_PART_SIZE}"'
            ;;
        -L) IMAGE_LAYOUT=$2; shift 2 ;;
        -t) USE_TMPFS=0; shift ;;
        -S) TMPFS_SIZE_MB=$2; shift 2 ;;
        -T) WORKSPACE_LOC=$2; shift 2 ;;
        -v) VERBOSE=$((${VERBOSE}+1))
            VERBOSE_ARGS="${VERBOSE_ARGS} -v"
            shift ;;
        -F) LOG_FILE=$2; LOG_OUTPUT=1; shift 2 ;;
        -I) SIG_IGNORE=1
            TPKG_ARGS="${TPKG_ARGS} -S"; shift ;;
        -r) SIG_REQUIRE=1
            TPKG_ARGS="${TPKG_ARGS} -s"; shift ;;
        -P) TRACK_PROGRESS=1; PROGRESS_OPER_ID=$2; shift 2 ;;
        -b) SWITCH_BOOT_LOC=1; NEXT_BOOT_LOC=$2; shift 2 ;;
        -z) REQUIRE_EMPTY_TARGETS=1; shift ;;
        -y) MARK_TARGETS_EMPTY_ON_FAILURE=1; shift ;;
        --) shift ; break ;;
        *) echo "writeimage.sh: parse failure" >&2 ; usage ;;
    esac
done

if [ ! -z "$*" ] ; then
    usage
fi

# Test all the options that got set for correctness

if [ ${TRACK_PROGRESS} -eq 1 -a ! -e ${MDREQ} ]; then
    vlechoerr -1 "*** Cannot report progress without mdreq."
    usage
fi

if [ ${TRACK_PROGRESS} -eq 1 -a ${DO_MANUFACTURE} -eq 1 ]; then
    vlechoerr -1 "*** Cannot track progress during manufacturing."
    usage
fi

if [ ${DO_MANUFACTURE} -eq -1 ]; then
    usage
fi

if [ ${SYSIMAGE_USE_URL} -eq -1 ]; then
    usage
fi

if [ ${DO_MANUFACTURE} -eq 0 ]; then
    if [ ${INSTALL_IMAGE_ID} -eq -1 ]; then
        usage
    fi
    if [ ${INSTALL_IMAGE_ID} -ne 1 -a ${INSTALL_IMAGE_ID} -ne 2  ]; then
        vlechoerr -1 "*** Invalid image location ${INSTALL_IMAGE_ID}, must be 1 or 2."
        usage
    fi
    INSTALL_IMAGE_ID_LIST=${INSTALL_IMAGE_ID}
else
    if [ ${INSTALL_IMAGE_ID} -ne -1 ]; then
        usage
    fi
    INSTALL_IMAGE_ID_LIST="1 2"
fi

if [ ${SIG_IGNORE} -eq 1 -a ${SIG_REQUIRE} -eq 1 ]; then
    usage
fi

if [ ${SWITCH_BOOT_LOC} -eq 1 ]; then
    if [ "${NEXT_BOOT_LOC}" != "1" -a "${NEXT_BOOT_LOC}" != "2" ]; then
        vlechoerr -1 "*** Invalid next boot location ${NEXT_BOOT_LOC}, must be 1 or 2."
        usage
    fi
fi

# Define graft functions
if [ -f /etc/customer_rootflop.sh ]; then
    . /etc/customer_rootflop.sh
fi

if [ "$HAVE_WRITEIMAGE_GRAFT_1" = "y" ]; then
    writeimage_graft_1
fi

if [ ${LOG_OUTPUT} -ne 0 -a ! -z "${LOG_FILE}" -a ! -f "${LOG_FILE}" ]; then
    touch "${LOG_FILE}"
fi

# If no kernel type was specified, the default is uniprocessor.
if [ -z "${IL_KERNEL_TYPE}" ]; then
    IL_KERNEL_TYPE="uni"
fi

# Do we want tar to print every file
if [ ${VERBOSE} -gt 1 ]; then
    TAR_VERBOSE=v
else
    TAR_VERBOSE=
fi

#
# Start off progress bar, if requested.
#
# XXXX/EMT: need a way for graft points to add their own steps to be
# tracked by progress.
#
# As a convention, for step weights, we use the time in seconds that it
# took to run each step on one of our test systems, times ten.  These
# don't have to be precise, they just help estimate what to show for the
# overall operation progress.
#
if [ ${TRACK_PROGRESS} -eq 1 ]; then
    # XXX/EMT: would be better to factor out commonality.  Tried to
    # have a var to have either empty string or 5th step, but could
    # not get escaping right for spaces in one of the parameters...
    if [ ${SWITCH_BOOT_LOC} -eq 1 ]; then
        ${MDREQ} action /progress/actions/update_oper \
            oper_id string ${PROGRESS_OPER_ID} \
            oper_type string "image_install" \
            descr string "Installing System Image" \
            num_steps uint32 5 \
            step/1/descr string "Verify Image" \
            step/1/weight uint32 150 \
            step/2/descr string "Uncompress Image" \
            step/2/weight uint32 250 \
            step/3/descr string "Create Filesystems" \
            step/3/weight uint32 50 \
            step/4/descr string "Extract Image" \
            step/4/weight uint32 180 \
            step/5/descr string "Switch Next Boot Location" \
            step/5/weight uint32 10
    else
        ${MDREQ} action /progress/actions/update_oper \
            oper_id string ${PROGRESS_OPER_ID} \
            oper_type string "image_install" \
            descr string "Installing System Image" \
            num_steps uint32 4 \
            step/1/descr string "Verify Image" \
            step/1/weight uint32 150 \
            step/2/descr string "Uncompress Image" \
            step/2/weight uint32 250 \
            step/3/descr string "Create Filesystems" \
            step/3/weight uint32 50 \
            step/4/descr string "Extract Image" \
            step/4/weight uint32 180
    fi

    #
    # Step #1: Verify Image
    #
    # This step contains several substeps, weighted roughly as follows:
    #   (a) General initialization (5%)  (Some of this has nothing to do
    #       with verifying the image, but since "initialization" is a
    #       jargony concept, and it's pretty quick, we just lump it in
    #       with verification for simplicity.
    #   (b) Making a copy of the image file (10%)
    #   (c) Synching the disk after making the copy (10%)
    #   (d) Tpkg_query (5%)
    #   (e) Tpkg_extract (65%)
    #   (f) Cleanup (5%)
    #
    # XXXX/EMT: use progress wrapper to update progress on substeps
    # (b) and (c).
    #
    ${MDREQ} -q action /progress/actions/begin_step \
        oper_id string ${PROGRESS_OPER_ID} \
        quant bool true \
        step_number uint32 1

    # Step 1a
    ${MDREQ} action /progress/actions/update_step \
        oper_id string ${PROGRESS_OPER_ID} \
        percent_done float32 0.0 \
        step_status string "Init"
fi

# Read in layout stuff.  This is done twice on install, once on
# manufacture.  The first is before we set the user's device and HWNAME
# settings (from /etc/image_layout.sh), and the second is after, so
# their settings can affect the final layout settings.

IL_LAYOUT="${IMAGE_LAYOUT}"

IL_PATH=/etc/layout_settings.sh
if [ -r ${IL_PATH} ]; then
    . ${IL_PATH}
else
    vlechoerr -1 "*** Invalid image layout settings."
    usage
fi

# If no HWNAME is given, see if the build env can help us out.  This
# should only ever come into play during manufacturing, before a model
# definiton is complete.
if [ ${DO_MANUFACTURE} -eq 1 -a -z "${IL_HWNAME}" -a "${HWNAME_PASSED}" != "1" ]; then
    IL_HWNAME="${BUILD_TARGET_HWNAME}"
fi

# Re-read the layout stuff, so the user's settings will take effect.
if [ ${DO_MANUFACTURE} -eq 0 ]; then
    . /etc/image_layout.sh
    . ${IL_PATH}
fi

START_TIME=$(date "+%Y%m%d-%H%M%S")
vlecho 0 "====== Starting image install at ${START_TIME}"

if [ ${DO_MANUFACTURE} -eq 0 ]; then
    vlecho 0 "==== Installing image into location: ${INSTALL_IMAGE_ID}"
else
    vlecho 0 "==== Manufacturing image"
fi

eval 'targets="${IL_LO_'${IL_LAYOUT}'_TARGETS}"'
eval 'parts="${IL_LO_'${IL_LAYOUT}'_PARTS}"'
if [ -z "${targets}" -o -z "${parts}" ]; then
    vlechoerr -1 "*** Invalid layout ${IL_LAYOUT} specified."
    cleanup_exit "Internal error: invalid image settings"
fi
vlecho 0 "==== Using layout: ${IL_LAYOUT}"

eval 'bootmgr_type="${IL_LO_'${IL_LAYOUT}'_BOOTMGR}"'
if [ -z "${bootmgr_type}" ]; then
    bootmgr_type=grub
fi
if [ "${bootmgr_type}" != "grub" -a "${bootmgr_type}" != "uboot" ]; then
    vlechoerr -1 "*** Invalid bootmgr type specified: ${bootmgr_type}"
    cleanup_exit "Internal error: invalid image settings"
fi

# Force installation of the bootmgr, if requested.  Note that for our
# layouts at least, at mfg time the GRUB gets installed by default, but
# u-boot does not.  At install (upgrade) time, neither is installed by
# default.
#
# XXXX Note: in the install (upgrade) case, for u-boot we'll install the
# u-boot from the new image, whereas for GRUB, we'll take what's in the
# running image.  This needs to be made consistent.

#
if [ "${INSTALL_BOOTMGR}" = "1" ]; then
    # We'll prefix the USER_LOC_OVERRIDE_ADD with the relevant
    # location(s) for the bootmgr for this layout.

    INSTALL_BOOTMGR_LOC_LIST=
    if [ "${bootmgr_type}" = "grub" ]; then
        new_loc_override=BOOTMGR_1
        USER_LOC_OVERRIDE_ADD="${new_loc_override} ${USER_LOC_OVERRIDE_ADD}"
        eval 'USER_LOC_OVERRIDE_ADD_LOC_'${new_loc_override}'="1"'
        INSTALL_BOOTMGR_LOC_LIST="BOOTMGR_1"
    elif [ "${bootmgr_type}" = "uboot" ]; then
        new_loc_override=UBOOT_1
        USER_LOC_OVERRIDE_ADD="${new_loc_override} ${USER_LOC_OVERRIDE_ADD}"
        eval 'USER_LOC_OVERRIDE_ADD_LOC_'${new_loc_override}'="1"'

        new_loc_override=UBOOT_ENV_1
        USER_LOC_OVERRIDE_ADD="${new_loc_override} ${USER_LOC_OVERRIDE_ADD}"
        eval 'USER_LOC_OVERRIDE_ADD_LOC_'${new_loc_override}'="1"'
        INSTALL_BOOTMGR_LOC_LIST="UBOOT_1 UBOOT_ENV_1"
    fi

    INSTALL_BOOTMGR_PART_MOUNTABLE_LIST=
    for loc in ${INSTALL_BOOTMGR_LOC_LIST}; do
        eval 'curr_part="${IL_LO_'${IL_LAYOUT}'_LOC_'${loc}'_PART}"'
        if [ -z "${curr_part}" ]; then
            continue
        fi

        eval 'part_mount="${IL_LO_'${IL_LAYOUT}'_PART_'${curr_part}'_MOUNT}"'
        if [ -z "${part_mount}" ]; then
            continue
        fi

        # Add it in, if unique
        present=0
        echo "${INSTALL_BOOTMGR_PART_MOUNTABLE_LIST}" | grep -q " ${curr_part} " - && present=1
        if [ ${present} -eq 0 ]; then
            INSTALL_BOOTMGR_PART_MOUNTABLE_LIST="${INSTALL_BOOTMGR_PART_MOUNTABLE_LIST} ${curr_part}"
        fi
    done

    vlecho 0 "==== Install forced for bootmgr"
fi

# Now associate the targets (devices) from the layout file with the devices
# the user specified on the command line

# Set TARGET_NAMES, which are things like 'DISK1'
TARGET_DEV_ARGS=
eval 'TARGET_NAMES="${IL_LO_'${IL_LAYOUT}'_TARGETS}"'
dev_num=1
for tn in ${TARGET_NAMES}; do
    if [ "${dev_num}" -le "${USER_TARGET_DEV_COUNT}" ]; then
        eval 'curr_dev="${USER_TARGET_DEV_'${dev_num}'}"'
        eval 'IL_LO_'${IL_LAYOUT}'_TARGET_'${tn}'_DEV="${curr_dev}"'
    else
        eval 'curr_dev="${IL_LO_'${IL_LAYOUT}'_TARGET_'${tn}'_DEV}"'
        if [ -z "${curr_dev}" ]; then
            vlechoerr -1 "*** Not enough device targets specified."
            cleanup_exit "Not enough media specified"
        fi
    fi

    # Possible MTD char/block fixups
    #
    # This is all because there are both block and char devices for
    # MTDs, and want to know the names of both, and get the partition
    # names to know both.

    eval 'target_type="${IL_LO_'${IL_LAYOUT}'_TARGET_'${tn}'_TYPE}"'
    if [ -z "${target_type}" ]; then
        target_type=block
    fi
    if [ "${target_type}" = "mtd" ]; then
        eval 'curr_char_dev="${IL_LO_'${IL_LAYOUT}'_TARGET_'${tn}'_DEV_CHAR}"'
        if [ -z "${curr_char_dev}" ]; then
            eval 'IL_LO_'${IL_LAYOUT}'_TARGET_'${tn}'_DEV_CHAR="${curr_dev}"'
        fi
        eval 'curr_block_dev="${IL_LO_'${IL_LAYOUT}'_TARGET_'${tn}'_DEV_BLOCK}"'
        if [ -z "${curr_block_dev}" ]; then
            eval 'IL_LO_'${IL_LAYOUT}'_TARGET_'${tn}'_DEV_BLOCK="${curr_dev}block"'
        fi
    elif [ "${target_type}" != "block" ]; then
        vlechoerr -1 "*** Invalid target type ${target_type} specified."
        cleanup_exit "Internal error: invalid image settings"
    fi
    

    TARGET_DEV_ARGS="${TARGET_DEV_ARGS} -d ${curr_dev}"
    dev_num=$((${dev_num} + 1))
done
vlecho 0 "==== Layout targets: ${TARGET_NAMES}"

# Reread layout settings file, as we can only now reference target device
# names to get partition device name (now that we have the devices from
# the user).
if [ -r ${IL_PATH} ]; then
    . ${IL_PATH}
else
    vlechoerr -1 "*** Invalid image layout settings."
    usage
fi


# User overrides for partitions sizes
eval 'PART_NAMES="${IL_LO_'${IL_LAYOUT}'_PARTS}"'
for part in ${PART_NAMES}; do
    eval 'user_part_size="${USER_PART_'${part}'_SIZE}"'
    if [ ! -z "${user_part_size}" ]; then
        # First verify there is a part with this name
        eval 'spn="${IL_LO_'${IL_LAYOUT}'_PART_'${part}'_DEV}"'
        if [ ! -z "${spn}" ]; then
            eval 'IL_LO_'${IL_LAYOUT}'_PART_'${part}'_SIZE="${user_part_size}"'
        fi
        eval 'spn="${IL_LO_'${IL_LAYOUT}'_PART_REPL_'${part}'_DEV}"'
        if [ ! -z "${spn}" ]; then
            eval 'IL_LO_'${IL_LAYOUT}'_PART_REPL_'${part}'_SIZE="${user_part_size}"'
        fi
    fi
done

# User overrides for virtual partitions sizes
eval 'VPART_NAMES="${IL_LO_'${IL_LAYOUT}'_VPARTS}"'
for vpart in ${VPART_NAMES}; do
    eval 'user_vpart_size="${USER_PART_'${vpart}'_SIZE}"'
    if [ ! -z "${user_vpart_size}" ]; then
        # First verify there is a vpart with this name
        eval 'spn="${IL_LO_'${IL_LAYOUT}'_VPART_'${vpart}'_PART_NAME}"'
        if [ ! -z "${spn}" ]; then
            eval 'IL_LO_'${IL_LAYOUT}'_VPART_'${vpart}'_FSSIZE="${user_vpart_size}"'
        fi
        eval 'spn="${IL_LO_'${IL_LAYOUT}'_VPART_REPL_'${vpart}'_PART_NAME}"'
        if [ ! -z "${spn}" ]; then
            eval 'IL_LO_'${IL_LAYOUT}'_VPART_REPL_'${vpart}'_FSSIZE="${user_vpart_size}"'
        fi
    fi
done

# Verify for any virtual partitions that a valid associated 
# partition exists.
eval 'VPART_NAMES="${IL_LO_'${IL_LAYOUT}'_VPARTS}"'
for vpart in ${VPART_NAMES}; do
    eval 'part_name="${IL_LO_'${IL_LAYOUT}'_VPART_'${vpart}'_PART_NAME}"'
    if [ -z "${part_name}" ]; then
        vlechoerr -1 "*** No partition associated with virtual partition ${vpart}"
        cleanup_exit "Internal error: invalid image settings"
    fi

    found=0
    for part in ${PART_NAMES}; do
        if [ ! -z "${part}" -a "${part_name}" = "${part}" ]; then
            found=1
            break
        fi
    done

    if [ ${found} -ne 1 ]; then
        vlechoerr -1 "*** Could not find partition ${part} for virtual partition ${vpart}"
        cleanup_exit "Internal error: invalid image settings"
    fi
done

# Set TARGET_DEVS, which are things like /dev/hda
# If ever let user change TARGET devices would need to check if it's
# consistent with the layout settings. XXX
TARGET_DEVS=""
for tn in ${TARGET_NAMES}; do
    new_dev=""
    eval 'new_dev="${IL_LO_'${IL_LAYOUT}'_TARGET_'${tn}'_DEV}"'

    if [ -z "${new_dev}" ]; then
        vlechoerr -1 "*** Target ${tn}: could not determine device"
        cleanup_exit "Internal error: invalid image settings"
    fi

    TARGET_DEVS="${TARGET_DEVS} ${new_dev}"
done
vlecho 0 "==== Using devices: ${TARGET_DEVS}"

# Verify that every PART is referenced by at least one LOC
eval 'PART_NAMES="${IL_LO_'${IL_LAYOUT}'_PARTS}"'
eval 'LOC_NAMES="${IL_LO_'${IL_LAYOUT}'_LOCS}"'
for part in ${PART_NAMES}; do
    found=0
    for loc in ${LOC_NAMES}; do
        this_part=
        eval 'this_part="${IL_LO_'${IL_LAYOUT}'_LOC_'${loc}'_PART}"'
        if [ "${this_part}" = "${part}" ]; then
            found=1
            break
        fi
    done

    part_id=
    eval 'part_id="${IL_LO_'${IL_LAYOUT}'_PART_'${part}'_PART_ID}"'

    if [ ${found} -eq 0 ]; then
        # The 'EXT' part isn't a 'real' partition, so ignore it
        if [ "${part_id}" = "0f" ]; then
            continue
        fi

        vlechoerr -1 "*** Could not find location for partition ${part}"
        cleanup_exit "Internal error: invalid image settings"
    fi
done

vlecho 0 "==== Using bootmgr: ${bootmgr_type}"
vlecho 0 "==== Hardware name:   ${IL_HWNAME}"

# ==================================================
# Initial settings and defaults, override for debug/testing
# ==================================================

#DO_MANUFACTURE=0
# Must be 1 or 2, only used if DO_MANUFACTURE=0
#INSTALL_IMAGE_ID=1
#SYSIMAGE_URL=""
#SYSIMAGE_FILE=

NO_PARTITIONING=0
NO_REWRITE_FILESYSTEMS=0

# See if we're in a running image or a bootfloppy
uname -r | grep -q bfuni
if [ $? -eq 1 ]; then
    IS_BOOTFLOP=0
else
    IS_BOOTFLOP=1
fi

UNMOUNT_TMPFS=
UNMOUNT_WORKINGFS=
UNMOUNT_EXTRACTFS=
UNMOUNT_XMTD=
UNMOUNT_VPART_PHYS=
UNMOUNT_VPART=
UNMOUNT_INITRD=
SYSIMAGE_FILE_TAR=
WORKSPACE_DIR=
UNMOUNTED_BOOTMGR_LIST=
BCP_BOOTMGR_UMOUNT=
BCP_REMOUNTED_RW=0

trap "trap_cleanup_exit" HUP INT QUIT PIPE TERM

#
# This whole MKE2FS_BASE_OPTIONS thing is because mke2fs now needs to be
# told not to reserve space to resize the partition, or older version of
# linux cannot mount the created filesystem!

MKE2FS_BASE_OPTIONS='-O ^resize_inode'
tmp_me2fs_file=/tmp/tmf.$$
rm -f ${tmp_me2fs_file}
dd if=/dev/zero of=${tmp_me2fs_file} bs=1k count=100 2> /dev/null

FAILURE=0
${MKE2FS} -n ${MKE2FS_BASE_OPTIONS} -q -F ${tmp_me2fs_file} > /dev/null 2>&1  || FAILURE=1
if [ ${FAILURE} -eq 1 ]; then
    MKE2FS_BASE_OPTIONS=
fi

# See if we have '-t FSTYPE' support in mke2fs.  Newer mke2fs, like in
# EL6 (e2fsprogs >= v1.41), has support for either ext3 or ext4 by
# specifying this option.
MKE2FS_HAS_FSTYPE=0
FAILURE=0
${MKE2FS} -n -t ext3 -q -F ${tmp_me2fs_file} > /dev/null 2>&1  || FAILURE=1
if [ ${FAILURE} -eq 0 ]; then
    MKE2FS_HAS_FSTYPE=1
fi

rm -f ${tmp_me2fs_file}

# ==================================================
# Partition settings
# ==================================================

# Set TARGET_NAMES, which are things like 'DISK1'
eval 'TARGET_NAMES="${IL_LO_'${IL_LAYOUT}'_TARGETS}"'

# Set TARGET_DEVS, which are things like /dev/hda
TARGET_DEVS=""
for tn in ${TARGET_NAMES}; do
    new_dev=""
    eval 'new_dev="${IL_LO_'${IL_LAYOUT}'_TARGET_'${tn}'_DEV}"'

    if [ -z "${new_dev}" ]; then
        vlechoerr -1 "*** Target ${tn}: could not determine device"
        cleanup_exit "Internal error: invalid image settings"
    fi

    TARGET_DEVS="${TARGET_DEVS} ${new_dev}"
done

# Make sure the workspace location exists in the layout.
if [ ${DO_MANUFACTURE} -eq 1 -a ${USE_TMPFS} -eq 0 ]; then
    curr_part=
    eval 'curr_part="${IL_LO_'${IL_LAYOUT}'_LOC_'${WORKSPACE_LOC}'_PART}"'
    if [ -z "${curr_part}" ]; then
        vlechoerr -1 "*** No partition found for workspace location ${WORKSPACE_LOC}"
        cleanup_exit "Internal error: invalid image settings"
    fi
fi

# 
# This sizing information is to make sure the target disk is big enough, and
# to remember the total disk size, and the number of megs in a cylinder (for
# partition padding).

BYTES_PER_SECTOR=512
# Number of sectors in a meg
SEC_PER_MEG=`expr 1048576 / ${BYTES_PER_SECTOR}`


# XXX We may want to trim out some things in TARGET_NAMES, as we may be
# looking at a target that has been fully excluded.

for tn in ${TARGET_NAMES}; do
    eval 'imdev="${IL_LO_'${IL_LAYOUT}'_TARGET_'${tn}'_DEV}"'
    eval 'labeltype="${IL_LO_'${IL_LAYOUT}'_TARGET_'${tn}'_LABELTYPE}"'
    if [ -z "${labeltype}" ]; then
        labeltype=msdos
    fi
    if [ "$HAVE_WRITEIMAGE_GRAFT_J1" = "y" ] ; then
       # Juniper added writeimage_graft_J1
       writeimage_graft_J1
    fi

    if [ "${labeltype}" = "msdos" ]; then
        FAILURE=0
        imdev_size_k=`sfdisk -q -s ${imdev} 2> /dev/null` || FAILURE=1
        if [ ${FAILURE} -eq 1 -o -z "${imdev_size_k}"  ]; then
            vlechoerr -1 "*** Target ${imdev}: could not determine size"
            cleanup_exit
        fi
        # We use 'awk' as busybox expr does not work above 2^32
        imdev_size_m=`echo ${imdev_size_k} | awk '{f= $1 / 1024; print int(f)}'`
        
        FAILURE=0
        imdev_geom=`sfdisk -g ${imdev} 2> /dev/null` || FAILURE=1
        if [ ${FAILURE} -eq 1 -o -z "${imdev_geom}" ]; then
            vlechoerr -1 "*** Target ${imdev}: could not determine geometry"
            cleanup_exit
        fi
        imdev_heads=`echo ${imdev_geom} | awk '{print $4}'`
        imdev_sectrack=`echo ${imdev_geom} | awk '{print $6}'`
        imdev_sec_multiple=`expr ${imdev_heads} \* ${imdev_sectrack}`
        
        # This number just has to come out to be more than the actual so we
        # don't overallocate
        
        imdev_cyl_m=`expr ${imdev_sec_multiple} \* ${BYTES_PER_SECTOR} / \
            1048576 + 1`
        
        # Set CURR_SIZE for our target
        eval 'IL_LO_'${IL_LAYOUT}'_TARGET_'${tn}'_CURR_SIZE_MEGS="${imdev_size_m}"'
        
        # Set CURR_CYL_MEGS for our target
        eval 'IL_LO_'${IL_LAYOUT}'_TARGET_'${tn}'_CURR_CYL_MEGS="${imdev_cyl_m}"'
    elif [ "${labeltype}" = "gpt" ]; then
        FAILURE=0
        imdev_size_k=`sfdisk -q -s ${imdev} 2> /dev/null` || FAILURE=1
        if [ ${FAILURE} -eq 1 -o -z "${imdev_size_k}"  ]; then
            vlechoerr -1 "*** Target ${imdev}: could not determine size"
            cleanup_exit
        fi
        # We use 'awk' as busybox expr does not work above 2^32
        imdev_size_m=`echo ${imdev_size_k} | awk '{f= $1 / 1024; print int(f)}'`
        
        # Set CURR_SIZE for our target
        eval 'IL_LO_'${IL_LAYOUT}'_TARGET_'${tn}'_CURR_SIZE_MEGS="${imdev_size_m}"'
        
        # Set CURR_CYL_MEGS for our target.  
        # Note: we assume the worst: 8 meg cylinders, as space is likely not
        # at a premium, and it's a pretty good guess anyway.
        eval 'IL_LO_'${IL_LAYOUT}'_TARGET_'${tn}'_CURR_CYL_MEGS="8"'
    elif [ "${labeltype}" = "raw" ]; then
        # XXX-mtd: we need to make sure that raw vs. mtd does not trip us up!

        # For MTDs the whole size does not matter, just that the individual 
        # "partition" sizes in the layout match what the kernel has

        MTD_SYSTEM_SIZES="$(printf "%s %d\n" $(cat /proc/mtd | grep '^mtd.*:' | sed 's/^\([^ :]*\):\(.*\)$/\1\2/' | awk '{print $1 " " "0x" $2}' ))"
        if [ -z "${MTD_SYSTEM_SIZES}" ]; then
            vlechoerr -1 "*** Target ${imdev}: could not find any MTD size info"
            cleanup_exit
        fi
        MTD_LABELS="$(cat /proc/mtd | grep '^mtd.*:' | sed 's/^.*"\([^"]*\)"$/\1/' | sed 's/ /_/g' )"
        mdev=
        msize=
        mlabel=
        ps=0
        mss=1
        for msys in ${MTD_SYSTEM_SIZES} ; do
            if [ ${ps} -eq 0 ]; then
                mdev=${msys}
                ps=1
                continue
            fi
            if [ ${ps} -ne 0 ]; then
                msize=${msys}
                ps=0
            fi

            mlc=1
            mlabel=
            for cl in ${MTD_LABELS}; do
                if [ "${mlc}" = "${mss}" ]; then
                   mlabel="${cl}"
                   break
                fi
                mlc=$((${mlc} + 1))
            done

            eval 'MTD_SYS_PART_'${mdev}'_SIZE_BYTES="${msize}"'
            eval 'MTD_SYS_PART_'${mdev}'_LABEL="${mlabel}"'
            mdev=
            msize=
            mss=$((${mss} + 1))
        done

        # Below, we will check MTD_SYS_PART_*_SIZE_BYTES against the layout

    elif [ "${labeltype}" = "none" ]; then
        vlecho 2 "No label in use for ${imdev}";
    else
        vlechoerr -1 "*** Target ${imdev}: unknown label type ${labeltype}"
        cleanup_exit
    fi
    if [ "$HAVE_WRITEIMAGE_GRAFT_J2" = "y" ] ; then
       # Juniper added writeimage_graft_J2
       writeimage_graft_J2
    fi
done

# Make a per-target list of the names, devices, and partition numbers of all
# the partitions

eval 'part_list="${IL_LO_'${IL_LAYOUT}'_PARTS}"'
for part in ${part_list}; do
    eval 'target="${IL_LO_'${IL_LAYOUT}'_PART_'${part}'_TARGET}"'
    eval 'add_part_num="${IL_LO_'${IL_LAYOUT}'_PART_'${part}'_PART_NUM}"'
    eval 'add_part_dev="${IL_LO_'${IL_LAYOUT}'_PART_'${part}'_DEV}"'

    eval 'TARGET_'${target}'_ALL_PARTS="${TARGET_'${target}'_ALL_PARTS} ${part}"'
    eval 'TARGET_'${target}'_ALL_PART_NUMS="${TARGET_'${target}'_ALL_PART_NUMS} ${add_part_num}"'
    eval 'TARGET_'${target}'_ALL_PART_DEVS="${TARGET_'${target}'_ALL_PART_DEVS} ${add_part_dev}"'
done


# Build up a list of the partitions needed for images 1 and 2 (if any).

for inum in 1 2; do
    loc_list=""
    eval 'loc_list="${IL_LO_'${IL_LAYOUT}'_IMAGE_'${inum}'_LOCS}"'

    for loc in ${loc_list}; do
        add_part=""
        eval 'add_part="${IL_LO_'${IL_LAYOUT}'_LOC_'${loc}'_PART}"'

        if [ ! -z "${add_part}" ]; then
            # Only add it on if it is unique
            eval 'curr_list="${IMAGE_'${inum}'_PART_LIST}"'

            present=0
            echo "${curr_list}" | grep -q " ${add_part} " - && present=1
            if [ ${present} -eq 0 ]; then
                eval 'IMAGE_'${inum}'_PART_LIST="${IMAGE_'${inum}'_PART_LIST} ${add_part} "'
            fi
        fi

        add_vpart=""
        eval 'add_vpart="${IL_LO_'${IL_LAYOUT}'_LOC_'${loc}'_VPART}"'

        if [ ! -z "${add_vpart}" ]; then
            # Only add it on if it is unique
            eval 'curr_list="${IMAGE_'${inum}'_VPART_LIST}"'

            present=0
            echo "${curr_list}" | grep -q " ${add_vpart} " - && present=1
            if [ ${present} -eq 0 ]; then
                eval 'IMAGE_'${inum}'_VPART_LIST="${IMAGE_'${inum}'_VPART_LIST} ${add_vpart} "'
            fi
        fi


    done

    eval 'curr_parts="${IMAGE_'${inum}'_PART_LIST}"'
    vecho 1 "IP=${curr_parts}"

done



# Any layout specific checks go here

if [ ${IL_LAYOUT} = "REPL" ]; then

    last=0
    for tn in ${TARGET_NAMES}; do
        tds=-1
        eval 'tds="${IL_LO_'${IL_LAYOUT}'_TARGET_'${tn}'_CURR_SIZE_MEGS}"'
        if [ -z "${tds}" ]; then
            vlechoerr -1 "*** No size found for target ${tn}"
            cleanup_exit
        fi
        if [ ${last} -ne 0 ]; then
            if [ ${tds} -ne ${last} ]; then
                vlechoerr -1 "*** Targets are not the same size: ${last} and ${tds}."
                cleanup_exit
            fi
        fi
        last=${tds}
    done
fi


# ==================================================
# Calculate partitions and targets to operate on
# ==================================================

# See which locations need changing, and note the partitions associated
# with them.  This is where any overrides (exclusions) are taken into
# account.

# Make candidate_loc_list be the list of locations we want to change
if [ ${DO_MANUFACTURE} -eq 1 ]; then
    eval 'candidate_loc_list="${IL_LO_'${IL_LAYOUT}'_LOCS}"'
else
    eval 'candidate_loc_list="${IL_LO_'${IL_LAYOUT}'_IMAGE_'${INSTALL_IMAGE_ID}'_INSTALL_LOCS}"'
fi
if [ ! -z "${USER_LOC_OVERRIDE_REMOVE}" -o "${USER_LOC_OVERRIDE_REMOVE_ALL}" = "1" ]; then
    if [ "${USER_LOC_OVERRIDE_REMOVE_ALL}" != "1" ]; then
        cll="${candidate_loc_list}"
    else
        cll=
    fi
    candidate_loc_list=
    for cl in ${cll}; do 
        removed=0
        eval 'removed="${USER_LOC_OVERRIDE_REMOVE_LOC_'${cl}'}"'
        if [ -z "${removed}" -o "${removed}" = "0" ]; then
            candidate_loc_list="${candidate_loc_list} ${cl}"
        fi
    done
fi
if [ ! -z "${USER_LOC_OVERRIDE_ADD}" ]; then
    for oa in ${USER_LOC_OVERRIDE_ADD}; do 

        present=0
        echo " ${candidate_loc_list} " | grep -q " ${oa} " - && present=1
        if [ ${present} -eq 0 ]; then
            candidate_loc_list="${candidate_loc_list} ${oa}"
        fi
    done
fi


# Make ACTIVE_LOC_LIST be all the candiate locations that have a valid
# partition with a valid target.
#
# Make ACTIVE_PART_XMTDS_LIST be all the partitions the candidate
# locations need to have access to to copy files to an MTD.

ACTIVE_LOC_LIST=""
ACTIVE_PART_XMTDS_LIST=""
for cl in ${candidate_loc_list}; do 
    eval 'part="${IL_LO_'${IL_LAYOUT}'_LOC_'${cl}'_PART}"'
    eval 'vpart="${IL_LO_'${IL_LAYOUT}'_LOC_'${cl}'_VPART}"'
    eval 'active="${IL_LO_'${IL_LAYOUT}'_LOC_'${cl}'_ACTIVE}"'
    if [ -z "${active}" ]; then
        active=1
    fi
    eval 'oa="${USER_LOC_OVERRIDE_ADD_LOC_'${cl}'}"'

    if [ "${active}" = "0" -a "${oa}" != "1" ]; then
        vlecho 0 "---- Ignoring non-active location ${cl}"
        continue
    fi

    if [ ! -z "${part}" -a ! -z "${vpart}" ]; then
        vlechoerr -1 "*** Partition and virtual partition both specified for location ${cl}"
        cleanup_exit "Internal error: invalid image settings"
    fi

    if [ -z "${part}" -a -z "${vpart}" ]; then
        vlecho -1 "Warning: No partition for location ${cl}"
        continue
    fi

    if [ ! -z "${part}" ]; then
        eval 'target="${IL_LO_'${IL_LAYOUT}'_PART_'${part}'_TARGET}"'

        if [ -z "${target}" ]; then
            vlecho -1 "Warning: No target for location ${cl} partition ${part}"
            continue
        fi

        eval 'target_dev="${IL_LO_'${IL_LAYOUT}'_TARGET_'${target}'_DEV}"'

        if [ -z "${target_dev}" ]; then
            vlecho -1 "Warning: No device for location ${cl} partition ${part} target ${target}"
            continue
        fi
    fi

    # MTD extraction validation, etc.
    eval 'xmtd="${IL_LO_'${IL_LAYOUT}'_LOC_'${cl}'_IMAGE_EXTRACT_MTD}"'
    if [ ! -z "${xmtd}" ]; then
        # XXX-mtd: vpart on MTD not supported

        eval 'xmtd_src_part="${IL_LO_'${IL_LAYOUT}'_LOC_'${cl}'_SRC_PART}"'
        if [ -z "${xmtd_src_part}" ]; then
            vlecho -1 "Warning: No source partition for MTD extract location ${cl}"
            continue
        fi

        ACTIVE_PART_XMTDS_LIST="${ACTIVE_PART_XMTDS_LIST} ${xmtd_src_part}"
    fi

    ACTIVE_LOC_LIST="${ACTIVE_LOC_LIST} ${cl}"
done

vecho 1 "ALL=${ACTIVE_LOC_LIST}"
vecho 1 "APXL=${ACTIVE_PART_XMTDS_LIST}"

PART_CHANGE_LIST=""
PART_MTD_CHANGE_LIST=""
VPART_CHANGE_LIST=""
TARGET_CHANGE_LIST=""
TARGET_DEV_CHANGE_LIST=""
TARGET_MTD_CHANGE_LIST=""

for cl in ${ACTIVE_LOC_LIST}; do 
    add_part=""
    add_vpart=""
    add_part_dev=""
    add_part_num=""
    add_target=""
    add_target_dev=""
    vpart_part=""
    add_mtd_part=""

    eval 'add_part="${IL_LO_'${IL_LAYOUT}'_LOC_'${cl}'_PART}"'
    eval 'add_vpart="${IL_LO_'${IL_LAYOUT}'_LOC_'${cl}'_VPART}"'

    # Have already ensured that only one of add_part or add_vpart
    # will be set

    if [ ! -z "${add_part}" ]; then
        eval 'add_part_dev="${IL_LO_'${IL_LAYOUT}'_PART_'${add_part}'_DEV}"'
        eval 'add_part_num="${IL_LO_'${IL_LAYOUT}'_PART_'${add_part}'_PART_NUM}"'
        eval 'add_target="${IL_LO_'${IL_LAYOUT}'_PART_'${add_part}'_TARGET}"'
        eval 'add_target_dev="${IL_LO_'${IL_LAYOUT}'_TARGET_'${add_target}'_DEV}"'
        eval 'add_target_type="${IL_LO_'${IL_LAYOUT}'_TARGET_'${add_target}'_TYPE}"'
        if [ -z "${add_target_type}" ]; then
            add_target_type=block
        fi
        if [ "${add_target_type}" = "mtd" ]; then
            add_target_dev=
            add_mtd_part="${add_part}"
        fi
    fi

    # For a virtual partition, must find the associated target via the
    # associated partition. (This is to add the target to the appropriate list)
    if [ ! -z "${add_vpart}" ]; then
        eval 'vpart_part="${IL_LO_'${IL_LAYOUT}'_VPART_'${add_vpart}'_PART_NAME}"'
        eval 'add_target="${IL_LO_'${IL_LAYOUT}'_PART_'${vpart_part}'_TARGET}"'
    fi

    if [ ! -z "${add_part}" ]; then
        PART_CHANGE_LIST="${PART_CHANGE_LIST} ${add_part}"
        if [ ! -z "${add_mtd_part}" ]; then
            PART_MTD_CHANGE_LIST="${PART_MTD_CHANGE_LIST} ${add_mtd_part}"
        fi
        TARGET_CHANGE_LIST="${TARGET_CHANGE_LIST} ${add_target}"
        TARGET_DEV_CHANGE_LIST="${TARGET_DEV_CHANGE_LIST} ${add_target_dev}"
        eval 'TARGET_'${add_target}'_CHANGE_LOCS="${TARGET_'${add_target}'_CHANGE_LOCS} ${cl}"'
        eval 'TARGET_'${add_target}'_CHANGE_PARTS="${TARGET_'${add_target}'_CHANGE_PARTS} ${add_part}"'
        eval 'TARGET_'${add_target}'_CHANGE_PART_NUMS="${TARGET_'${add_target}'_CHANGE_PART_NUMS} ${add_part_num}"'

    fi

    if [ ! -z "${add_vpart}" ]; then
        VPART_CHANGE_LIST="${VPART_CHANGE_LIST} ${add_vpart}"
        TARGET_CHANGE_LIST="${TARGET_CHANGE_LIST} ${add_target}"
        eval 'TARGET_'${add_target}'_CHANGE_LOCS="${TARGET_'${add_target}'_CHANGE_LOCS} ${cl}"'
        eval 'TARGET_'${add_target}'_CHANGE_VPARTS="${TARGET_'${add_target}'_CHANGE_VPARTS} ${add_vpart}"'
    fi
done

# Uniquify the lists we just made
PART_CHANGE_LIST=`echo "${PART_CHANGE_LIST}" | \
    tr ' ' '\n' | sort | uniq | tr '\n' ' '`
PART_MTD_CHANGE_LIST=`echo "${PART_MTD_CHANGE_LIST}" | \
    tr ' ' '\n' | sort | uniq | tr '\n' ' '`
VPART_CHANGE_LIST=`echo "${VPART_CHANGE_LIST}" | \
    tr ' ' '\n' | sort | uniq | tr '\n' ' '`
TARGET_CHANGE_LIST=`echo "${TARGET_CHANGE_LIST}" | \
    tr ' ' '\n' | sort | uniq | tr '\n' ' '`
TARGET_DEV_CHANGE_LIST=`echo "${TARGET_DEV_CHANGE_LIST}" | \
    tr ' ' '\n' | sort | uniq | tr '\n' ' '`

for target in ${TARGET_CHANGE_LIST}; do
    eval 'curr_part_nums="${TARGET_'${target}'_CHANGE_PART_NUMS}"'
    new_part_nums=`echo "${curr_part_nums}" | \
        tr ' ' '\n' | sort -n | uniq | tr '\n' ' '`
    eval 'TARGET_'${target}'_CHANGE_PART_NUMS="${new_part_nums}"'

    vecho 1 "NPNL: ${target} is: ${new_part_nums}"

    eval 'curr_parts="${TARGET_'${target}'_CHANGE_PARTS}"'
    new_parts=`echo "${curr_parts}" | \
        tr ' ' '\n' | sort -n | uniq | tr '\n' ' '`
    eval 'TARGET_'${target}'_CHANGE_PARTS="${new_parts}"'

    vecho 1 "NPL: ${target} is: ${new_parts}"

done

vecho 1 "PCL: ${PART_CHANGE_LIST}"
vecho 1 "PMCL: ${PART_MTD_CHANGE_LIST}"
vecho 1 "VPCL: ${VPART_CHANGE_LIST}"
vecho 1 "TCL: ${TARGET_CHANGE_LIST}"
vecho 1 "TDCL: ${TARGET_DEV_CHANGE_LIST}"


# Unmount any mounted partitions we think we'll need to operate on, if
# they would usually be and are already mounted

UNMOUNTED_BOOTMGR_LIST=
if [ "${DO_MANUFACTURE}" -eq 0 -a "${INSTALL_BOOTMGR}" = "1" ]; then
    vlecho 0 "==== Unmounting as required for install of bootmgr"

    for part in ${INSTALL_BOOTMGR_PART_MOUNTABLE_LIST}; do

        eval 'curr_dev="${IL_LO_'${IL_LAYOUT}'_PART_'${part}'_DEV}"'
        if [ -z "${curr_dev}" ]; then
            continue
        fi

        # XXX #dep/parse: mount
        mp=`/bin/mount | grep "^${curr_dev} on " | awk '{print $1}'`
        if [ -z "${mp}" ]; then
            continue
        fi

        ${SYNC}
        UNMOUNTED_BOOTMGR_LIST="${UNMOUNTED_BOOTMGR_LIST} ${part}"
        umount ${curr_dev}
    done

    vlecho 1 "UBL: ${UNMOUNTED_BOOTMGR_LIST}"
fi

# ==================================================
# Verify no needed partition is mounted
# ==================================================

if [ ! -z "${PART_CHANGE_LIST}" -a ${NO_REWRITE_FILESYSTEMS} -eq 0 ]; then

    vlecho 0 "==== Verifying no needed partitions currently mounted"
    
    # XXX #dep/parse: mount
    MPL=`(mount -t ext2; mount -t ext3; mount -t ext4; mount -t jffs2; mount -t vfat) | grep /dev | grep -v "^none" | awk '{print $1}'`
    if [ -e /proc/swaps ]; then
        SPL=`cat /proc/swaps | grep /dev | awk '{print $1}'`
    else
        SPL=
    fi

    for testp in ${PART_CHANGE_LIST}; do
        curr_dev=""
        eval 'curr_dev="${IL_LO_'${IL_LAYOUT}'_PART_'${testp}'_DEV}"'

        for mpl in $MPL; do
            if [ "${mpl}" = "${curr_dev}" ]; then
                FAILURE=1
                if [ ${FAILURE} -eq 1 ]; then
                    vlechoerr -1 "*** Partition $mpl was mounted and should not be"
                    cleanup_exit
                fi
            fi
        done

        # Check for swap too
        for spl in $SPL; do
            if [ "${spl}" = "${curr_dev}" ]; then
                FAILURE=1
                if [ ${FAILURE} -eq 1 ]; then
                    vlechoerr -1 "*** Partition $spl was swap'd on and should not be"
                    cleanup_exit
                fi
            fi
        done

    done
fi


# ==================================================
# Verify disk can be repartitioned, if requested
# ==================================================

if [ ${DO_MANUFACTURE} -eq 1  -a ! -z "${PART_CHANGE_LIST}" -a \
    ${NO_PARTITIONING} -eq 0 ]; then

    vlecho 0 "==== Verifying targets can be repartitioned"
    # Verify that the kernel thinks nothing is using the disk(s)
    for td in ${TARGET_DEV_CHANGE_LIST}; do
        sfdo=""
        FAILURE=0
        # XXX #dep/parse: sfdisk
        sfdo=`sfdisk --force -R ${td} 2>&1` || FAILURE=1
        sfdo_is_gpt=`echo "${sfdo}" | grep '^WARNING: GPT '`
        sfdo=`echo "${sfdo}" | grep -v '^WARNING: GPT ' | grep -v '^$'`
        if [ ${FAILURE} -eq 1 -o "${sfdo}" != "" ]; then
            echo "${sfdo}" | grep -q " busy"
            if [ $? -ne 0 ]; then
                vlecho -1 "--- Target ${td}: warning: error verifying partition table"
                vlecho -1 "--- Zero'ing existing partition table"
                dd if=/dev/zero of=${td} bs=512 count=2 > /dev/null 2>&1

                # We may also need to clear the end of the disk, in case
                # there was a GPT label there.

                if [ ! -z "${sfdo_is_gpt}" ]; then
                    FAILURE=0
                    imdev_size_k=`sfdisk -q -s ${td} 2> /dev/null` || FAILURE=1
                    if [ ${FAILURE} -eq 1 -o -z "${imdev_size_k}"  ]; then
                        vlechoerr -1 "*** Target ${imdev}: could not determine size"
                        cleanup_exit
                    fi
                    # We use 'awk' as busybox expr does not work above 2^32
                    # We conservatively decide to zero out the last 10 megs
                    gpt_del_start_m=`echo ${imdev_size_k} | awk '{f= $1 / 1024 - 10; if (f > 0) {print int(f)} else {print 0}}'`
                    if [ ! -z "${gpt_del_start_m}" ]; then
                        dd if=/dev/zero of=${td} bs=1M seek=${gpt_del_start_m} > /dev/null 2>&1
                    fi
                fi
            else
                vlechoerr -1 "*** Target ${td}: error verifying could write partition table: disk busy"
                cleanup_exit
            fi
        fi
    done
fi


# Note that from here on in, if '-y' is specified, all the targets will
# be destroyed.
READY_FOR_EMPTY_TARGETS=1

# Check layout sizes against system sizes for MTD targets
if [ ! -z "${PART_MTD_CHANGE_LIST}" -a ${NO_REWRITE_FILESYSTEMS} -eq 0 ]; then
    vlecho 0 "==== Verifying MTD partitions are the correct size"

    for curr_part in ${PART_MTD_CHANGE_LIST}; do

        eval 'curr_dev_char="${IL_LO_'${IL_LAYOUT}'_PART_'${curr_part}'_DEV_CHAR}"'
        cdev_name=$(echo "${curr_dev_char}" | sed 's,^/dev/,,')
        eval 'curr_size_kb="${IL_LO_'${IL_LAYOUT}'_PART_'${curr_part}'_MTD_SIZE_KB}"'
        if [ -z "${curr_size_kb}" ]; then
            vlechoerr -1 "*** Partition for MTD ${curr_part} does not have a valid size"
            cleanup_exit
        fi
        eval 'curr_label="${IL_LO_'${IL_LAYOUT}'_PART_'${curr_part}'_MTD_LABEL}"'
        curr_size_bytes=$(echo "${curr_size_kb}" | awk '{s= $1 * 1024 ; print s}')
        eval 'sys_size_bytes="${MTD_SYS_PART_'${cdev_name}'_SIZE_BYTES}"'
        if [ -z "${sys_size_bytes}" ]; then
            vlechoerr -1 "*** System does not have MTD device ${curr_dev_char}"
            cleanup_exit
        fi
        eval 'sys_label="${MTD_SYS_PART_'${cdev_name}'_LABEL}"'

        if [ "${curr_size_bytes}" != "${sys_size_bytes}" ]; then
            vlechoerr -1 "*** Partition for MTD ${curr_part} size incorrect: ${curr_size_bytes} vs ${sys_size_bytes} actual"
            cleanup_exit
        fi

        if [ "${curr_label}" != "${sys_label}" ]; then
            vecho -1 "*** WARNING: MTD partition label for ${curr_part} incorrect: ${curr_label} vs ${sys_label} actual"
        fi

    done

fi


# ==================================================
# Verify media sizes are at least the minimum the layout says
# ==================================================

if [ ${DO_MANUFACTURE} -eq 1  -a ! -z "${PART_CHANGE_LIST}" -a \
    ${NO_PARTITIONING} -eq 0 ]; then

    vlecho 0 "==== Verifying targets are big enough"

    # Verify that the kernel thinks nothing is using the disk(s)
    for target in ${TARGET_CHANGE_LIST}; do
        eval 'target_size="${IL_LO_'${IL_LAYOUT}'_TARGET_'${target}'_CURR_SIZE_MEGS}"'
        eval 'min_size="${IL_LO_'${IL_LAYOUT}'_TARGET_'${target}'_MINSIZE}"'
        
        if [ -z "${min_size}" ]; then
            continue
        fi
        if [ -z "${target_size}" ]; then
            vlechoerr -1 "*** Target ${target} current size unavailable"
            cleanup_exit
        fi

        if [ "${target_size}" -lt "${min_size}" ]; then
            vlechoerr -1 "*** Target ${target} not big enough.  Required: ${min_size} Current: ${target_size}"
            cleanup_exit "Media size too small.  Required: ${min_size} .  Current: ${target_size} ."
        fi
    done
fi

# ==================================================
# If the media is supposed to be empty, make sure it is
# ==================================================

if [ ${REQUIRE_EMPTY_TARGETS} -eq 1 -a \
        \( ${DO_MANUFACTURE} -eq 1  -a ! -z "${PART_CHANGE_LIST}" -a \
            ${NO_PARTITIONING} -eq 0 \) ]; then

    for target in ${TARGET_CHANGE_LIST}; do
        eval 'target_dev="${IL_LO_'${IL_LAYOUT}'_TARGET_'${target}'_DEV}"'
        eval 'target_labeltype="${IL_LO_'${IL_LAYOUT}'_TARGET_'${target}'_LABELTYPE}"'
        if [ -z "${target_labeltype}" ]; then
            target_labeltype=msdos
        fi

        if [ "${target_labeltype}" = "gpt" ]; then
            target_part_prog=parted
        elif [ "${target_labeltype}" = "msdos" ]; then
            target_part_prog=sfdisk
        elif [ "${target_labeltype}" = "raw" ]; then
            target_part_prog=
        else
            vlechoerr -1 "*** Target ${target} specifies bad label type ${target_labeltype}"
            cleanup_exit "Internal error: invalid image settings"
        fi

        FAILURE=0
        PLIST=
        NOT_EMPTY=0
        if [ "${target_part_prog}" = "sfdisk" ]; then
            # XXX #dep/parse: sfdisk
            PLIST=$(sfdisk -uS -d -l ${target_dev} 2> /dev/null | grep 'start=.*size=.*Id=')
            if [ ! -z "${PLIST}" ]; then
                NOT_EMPTY=1
            fi
        elif [ "${target_part_prog}" = "parted" ]; then
            # XXX #dep/parse: parted
            PLIST=$(${PARTED} --script -- ${target_dev} unit s print 2> /dev/null | egrep 'Partition Table|primary')
            if [ ! -z "${PLIST}" ]; then
                NOT_EMPTY=1
            fi
        else
            # Look at the first 64k of 'raw', and see if it is 0's
            # XXX #dep/parse: dd
            DDOS=$(dd if=${target_dev} bs=1024 count=64 2>/dev/null | od -b -v | grep -v '^[0-9]* [0 ]*$' | tail -2)
            if [ "${DDOS}" != "0200000" ]; then
                NOT_EMPTY=1
            fi
        fi
        if [ ${NOT_EMPTY} -ne 0 ]; then
            vlechoerr -1 "*** Data present on target storage device, stopping without imaging."
            vlechoerr 0 "*** Data on storage device ${target_dev} target ${target}"
            cleanup_exit "Data present on media"
        fi
    done
fi

if [ ! -z "${USER_LOC_OVERRIDE_REMOVE}" ]; then
    vlecho 0 "==== Override location remove: ${USER_LOC_OVERRIDE_REMOVE}"
fi
if [ ! -z "${USER_LOC_OVERRIDE_ADD}" ]; then
    vlecho 0 "==== Override location add: ${USER_LOC_OVERRIDE_ADD}"
fi
if [ ! -z "${TARGET_CHANGE_LIST}" ]; then
    vlecho 0 "==== Target change list: ${TARGET_CHANGE_LIST}"
fi

# ==================================================
# Setup our work area, making TMPFS for scratch space, if requested
# ==================================================

TMP_MNT_IMAGE=/tmp/mnt_image_wi
mkdir -m 700 -p ${TMP_MNT_IMAGE}
chmod 700 ${TMP_MNT_IMAGE}
chown 0:0 ${TMP_MNT_IMAGE}

# If /var is a tmpfs, then force the TMPFS option
mount -t tmpfs | grep -q ' /var '
if [ $? -eq 0 ]; then
    vlecho 0 "---- Forcing tmpfs option as /var is a tmpfs"
    USE_TMPFS=1
fi

if [ ${USE_TMPFS} -eq 1 ]; then
    vlecho 0 "==== Mounting tmpfs for working area"
    target_dir=${TMP_MNT_IMAGE}/tmpfs
    mkdir -m 700 -p ${target_dir}
    FAILURE=0
    mount -t tmpfs -o size=${TMPFS_SIZE_MB}M,mode=700 none ${target_dir} || FAILURE=1
    if [ ${FAILURE} -eq 1 ]; then
        if [ ${DO_MANUFACTURE} -eq 1 ]; then
            vlechoerr -1 "*** Could not mount tmpfs for retrieving image"
            cleanup_exit
        fi
        # XXX could fall back to handling below and use var
        vlechoerr -1 "*** Could not mount tmpfs for retrieving image"
        cleanup_exit
    fi
    WORKSPACE_DIR=${target_dir}
    UNMOUNT_TMPFS=${target_dir}
else
    vlecho 0 "==== Setting up working area"

    #
    # If we are manufacturing, we make the partitions a little earlier.
    # Of course the wget/curl could fail, but that's a risk we're taking.
    #
    if [ ${DO_MANUFACTURE} -eq 1 ]; then

        # We're going to use somewhere in VAR as our workspace
        location=${WORKSPACE_LOC}

        # REMXXX if not backed by a part... fail
        # Or could have tmp space specified in layout (should do this)
        curr_vpart=
        eval 'curr_vpart="${IL_LO_'${IL_LAYOUT}'_LOC_'${location}'_VPART}"'
        if [ ! -z "${curr_vpart}" ]; then
            vlechoerr -1 "*** location ${location} cannot be backed by a vpart"
            cleanup_exit
        fi

        curr_part=
        eval 'curr_part="${IL_LO_'${IL_LAYOUT}'_LOC_'${location}'_PART}"'
        if [ -z "${curr_part}" ]; then
            vlechoerr -1 "*** No partition found for workspace location ${location}"
            cleanup_exit
        fi
        eval 'curr_dev="${IL_LO_'${IL_LAYOUT}'_PART_'${curr_part}'_DEV}"'
        eval 'curr_target="${IL_LO_'${IL_LAYOUT}'_PART_'${curr_part}'_TARGET}"'
        eval 'curr_fstype="${IL_LO_'${IL_LAYOUT}'_PART_'${curr_part}'_FSTYPE}"'
        eval 'curr_extract_prefix="${IL_LO_'${IL_LAYOUT}'_LOC_'${location}'_IMAGE_EXTRACT_PREFIX}"'
        mount_point="${TMP_MNT_IMAGE}/${curr_target}/${curr_part}/${curr_extract_prefix}"


        # Write the new partition table
        if [ ${NO_PARTITIONING} -eq 0 ]; then
            do_partition_disks
        fi


        # Make new filesystems on disk or disks
        if [ ${NO_REWRITE_FILESYSTEMS} -eq 0 ]; then
            do_make_filesystems
        fi


        # Setup our workspace
        mkdir -m 700 -p ${mount_point}
        FAILURE=0
        vlecho 0 "== Mounting ${curr_dev} on ${mount_point}"
        mount_options="-o noatime -t ${curr_fstype}"
        mount ${mount_options} ${curr_dev} ${mount_point} || FAILURE=1
        if [ ${FAILURE} -eq 1 ]; then
            vlechoerr -1 "*** Could not mount partition ${curr_dev} on ${mount_point}"
            cleanup_exit
        fi
        if [ "${curr_fstype}" = "jffs2" ]; then
            ${SYNC}
        fi

        UNMOUNT_WORKINGFS=${mount_point}
        eval 'PART_PRE_MOUNTED_'${curr_part}'=1'
        eval 'PART_PRE_MOUNTED_MOUNTPOINT_'${curr_part}'="'${mount_point}'"'
        TMP_LOCAL_WORKSPACE_ROOT=${mount_point}
        TMP_LOCAL_WORKSPACE=${TMP_LOCAL_WORKSPACE_ROOT}/wiw-scratch-$$
    else
        TMP_LOCAL_WORKSPACE_ROOT=/var/tmp/wiw
        TMP_LOCAL_WORKSPACE=${TMP_LOCAL_WORKSPACE_ROOT}/scratch-$$
    fi


    # XXX if we allowed var to get overwritten, this is an issue for
    # installs (not manufactures)!

    #
    # For both install and manufacture, we now make a tmp dir under /var
    #

    rm -f ${TMP_LOCAL_WORKSPACE}
    FAILURE=0
    mkdir -m 700 -p ${TMP_LOCAL_WORKSPACE} || FAILURE=1
    if [ ${FAILURE} -eq 1 ]; then
        vlechoerr -1 "*** Could not make work directory ${TMP_LOCAL_WORKSPACE}"
    fi
    WORKSPACE_DIR=${TMP_LOCAL_WORKSPACE}

    # Check if there is enough space in the workspace area to do the install
    # XXX #dep/parse: df
    avail_space=`df -k ${TMP_LOCAL_WORKSPACE_ROOT} | tail -1 | awk '{print $4}'`
    if [ ${WORKSPACE_MIN_AVAIL_SPACE} -gt ${avail_space} ]; then
        vlechoerr -1 "Not enough available workspace to install image."
        vecho -1 "Delete any unused images or unneeded logs to free up space."
        vlog -1 INFO "Image install failed: ${TMP_LOCAL_WORKSPACE_ROOT} has ${avail_space} Kbytes available, needs at least ${WORKSPACE_MIN_AVAIL_SPACE} Kbytes available"
        cleanup_exit "Insufficent workspace for operation"
    fi

fi


# ==================================================
# Retrieve system image: we use wget or curl to retrieve the image to our
# workspace in the url case, otherwise we copy the file to our workspace.
#
# (We try wget first, since that's what we were originally written to do,
# and we're probably in the manufacturing environment where wget is all
# we have.  But if we are in the main image, and if PROD_FEATURE_WGET is
# disabled, then we won't have wget, but should still have curl.)
# ==================================================

if [ $SYSIMAGE_USE_URL -eq 1 ]; then
    local_filename=`echo $SYSIMAGE_URL | sed 's/^.*\/\([^\/]*\)$/\1/'`
    vlecho 0 "==== Retrieving image file from ${SYSIMAGE_URL}"
    vecho 1 "lfn: ${local_filename}"

    target_dir=${WORKSPACE_DIR}/image
    mkdir -p ${target_dir}
    SYSIMAGE_FILE=${target_dir}/${local_filename}
    rm -f ${SYSIMAGE_FILE}

    if [ -f /usr/bin/wget ]; then
        XFERPROG=wget
        vecho 1 "wget: -O ${SYSIMAGE_FILE} ${SYSIMAGE_URL}"
        FAILURE=0
        do_verbose /usr/bin/wget -O ${SYSIMAGE_FILE} ${SYSIMAGE_URL} || FAILURE=1
    else
        XFERPROG=curl
        if [ -f /var/opt/tms/output/curlrc ]; then
            CURLCONF="-K /var/opt/tms/output/curlrc"
        else
            CURLCONF=""
        fi

        # Curl options:
        #   -v, -sS, -f  Verbosity options (also needed to get return code)
        #   -q           Don't look in user's home directory for a curlrc
        #   -g           Disable the "URL globbing parser"
        #   -o           Specify output file
        #   -K           Specify location of config file (see CURLCONF)
        vlecho 1 "curl: -v -sS -f -q -g -o ${SYSIMAGE_FILE} ${CURLCONF} ${SYSIMAGE_URL}"
        FAILURE=0
        do_verbose /usr/bin/curl -v -sS -f -q -g -o ${SYSIMAGE_FILE} ${CURLCONF} ${SYSIMAGE_URL} || FAILURE=1
    fi

    if [ ${FAILURE} -eq 1 ]; then
        vlechoerr -1 "*** Could not retrieve image from ${SYSIMAGE_URL} (using ${XFERPROG})"
        cleanup_exit "Could not retrieve specified image"
    fi
    SYSIMAGE_FRIENDLY=${local_filename}
else
    if [ -z "${SYSIMAGE_FILE}" -o ! -f "${SYSIMAGE_FILE}" ]; then
        vlechoerr -1 "*** No system image file:  ${SYSIMAGE_FILE}"
        cleanup_exit "Could not find specified image file"
    fi
    local_filename=`echo $SYSIMAGE_FILE | sed 's/^.*\/\([^\/]*\)$/\1/'`
    vlecho 0 "==== Copying image file from ${SYSIMAGE_FILE}"

    target_dir=${WORKSPACE_DIR}/image
    mkdir -p ${target_dir}
    NEW_SYSIMAGE_FILE=${target_dir}/${local_filename}
    if [ "${SYSIMAGE_FILE}" != "${NEW_SYSIMAGE_FILE}" ]; then
        rm -f ${NEW_SYSIMAGE_FILE}

        if [ ${TRACK_PROGRESS} -eq 1 ]; then
            # Step 1b
            ${MDREQ} action /progress/actions/update_step \
                oper_id string ${PROGRESS_OPER_ID} \
                percent_done float32 5.0 \
                step_status string "Copy image file"
            ${PROGRESS} -i ${PROGRESS_OPER_ID} -l 5.0 -h 15.0 --src-file ${SYSIMAGE_FILE} --dest-file ${NEW_SYSIMAGE_FILE} -- ${CP} -p ${SYSIMAGE_FILE} ${NEW_SYSIMAGE_FILE}

            # Step 1c
            ${MDREQ} action /progress/actions/update_step \
                oper_id string ${PROGRESS_OPER_ID} \
                percent_done float32 15.0 \
                step_status string "Sync disks"
            ${PROGRESS} -i ${PROGRESS_OPER_ID} -l 15.0 -h 25.0 -- ${SYNC}
        else
            ${CP} -p ${SYSIMAGE_FILE} ${NEW_SYSIMAGE_FILE}
            ${SYNC}
        fi
    fi
    ORIG_SYSIMAGE_FILE=${SYSIMAGE_FILE}
    SYSIMAGE_FILE=${NEW_SYSIMAGE_FILE}
    SYSIMAGE_FRIENDLY=${local_filename}
fi



# ========================================
# Extract / uncompress the image we wish to write.  This goes to our
# workspace, which will normally be a tmpfs.  There are two supported file
# formats, a 'zip' file and a 'bzip2' file.  The zip file has a bzip2'd tar
# inside it, as well as some version and hash info to describe and validate
# the bzip'd tar.  The bzip2 file has just a tar inside it.  We auto
# identify each based on the first 3-4 bytes, so we can support both formats
# transparently.
#
# .img (zip):   4 bytes: 0120 0113  003  004  ('PK\003\004')
# .tbz (bzip2): 3 bytes: 0102 0132  150       ('BZh')
# ========================================

IMG_FIRST_4_BYTES="120 113 003 004"
TBZ_FIRST_3_BYTES="102 132 150"

FILE_TYPE=none

# XXX #dep/parse: dd
first_four_bytes=`dd if=${SYSIMAGE_FILE} bs=1 count=4 2> /dev/null | od -b | awk '{print $2" "$3" "$4" "$5}' | sed 's, *$,,' | tr -d '\n'`
first_three_bytes=`echo ${first_four_bytes} | awk '{print $1" "$2" "$3}'`

if [ "${first_four_bytes}" = "${IMG_FIRST_4_BYTES}" ]; then
    FILE_TYPE=img
fi

if [ "${first_three_bytes}" = "${TBZ_FIRST_3_BYTES}" ]; then
    FILE_TYPE=tbz
fi

if [ "${FILE_TYPE}" = "none" ]; then
    vlechoerr -1 "*** Could not identify image type of ${SYSIMAGE_FRIENDLY}"
    cleanup_exit "Unknown image type"
fi

# This is the name of the file in the image that has the MD5s
MD5SUMS_NAME=md5sums

UNZIP_DIR=${WORKSPACE_DIR}/unzip
rm -f ${UNZIP_DIR}
mkdir -p ${UNZIP_DIR}
UNENCRYPT_DIR=${WORKSPACE_DIR}/unencrypt
rm -f ${UNENCRYPT_DIR}

IMAGE_BALL_DIR=${UNZIP_DIR}

if [ "${FILE_TYPE}" != "img" -a \( "${BUILD_PROD_FEATURE_IMAGE_SECURITY}" = "1" -o "${SIG_REQUIRE}" = "1" \) ]; then
    vlechoerr -1 "*** Invalid image format in image ${SYSIMAGE_FRIENDLY}"
    cleanup_exit "Invalid image"
fi

# For new style images, we first verify the image integrity
if [ "${FILE_TYPE}" = "img" ]; then
    vlecho 0 "==== Verifying image integrity for ${SYSIMAGE_FRIENDLY}"
    FAILURE=0
    do_verbose /usr/bin/unzip -q -d ${UNZIP_DIR} ${SYSIMAGE_FILE} || FAILURE=1
    if [ ${FAILURE} -eq 1 ]; then
        vlechoerr -1 "*** Could not verify image ${SYSIMAGE_FRIENDLY}"
        cleanup_exit "Could not verify image file"
    fi

    # Get rid of the system image file now, to save peak space
    rm -f ${SYSIMAGE_FILE}

    if [ ${TRACK_PROGRESS} -eq 1 ]; then
        # Step 1d
        ${MDREQ} action /progress/actions/update_step \
            oper_id string ${PROGRESS_OPER_ID} \
            percent_done float32 25.0 \
            step_status string "Check for image corruption, or lack of sig"
    fi

    # First, do a check for image corruption, or lack of sig
    FAILURE=0
    ${TPKG_QUERY} ${VERBOSE_ARGS} -t -w -f ${UNZIP_DIR} ${TPKG_ARGS} || FAILURE=1
    if [ ${FAILURE} -eq 1 ]; then
        vlechoerr -1 "*** Invalid image: could not verify ${SYSIMAGE_FRIENDLY}"
        cleanup_exit "Could not verify image file"
    fi

    # Slurp in the image's build version information.  The current (running
    # system) version information is in BUILD_xxx, so we rewrite the version
    # information of the image to be installed to be IMAGE_BUILD_xxx.
    #
    # We want this ASAP, so we can update the operation description to
    # include the BUILD_PROD_VERSION of the image being installed.
    # But this cannot be moved any earlier than it is here.

    # XXXX this should use tpkg_query to get this out, just like we do above
    eval `cat ${UNZIP_DIR}/build_version.sh | sed -e 's,^BUILD_\([A-Za-z_]*\)=,IMAGE_BUILD_\1=,' -e 's,^export BUILD_\([A-Za-z_]*\)$,export IMAGE_BUILD_\1,' | grep 'IMAGE_BUILD_'`

    if [ ${TRACK_PROGRESS} -eq 1 ]; then
        ${MDREQ} action /progress/actions/update_oper \
            oper_id string ${PROGRESS_OPER_ID} \
            descr string "Installing System Image: ${IMAGE_BUILD_PROD_VERSION}"

        # Step 1e
        ${MDREQ} action /progress/actions/update_step \
            oper_id string ${PROGRESS_OPER_ID} \
            percent_done float32 30.0 \
            step_status string "Verify, and maybe decrypt"
    fi

    # XXXX we should move the raw use of build_version.sh below to here,
    # and use tpkg_query for it too.
    eval `${TPKG_QUERY} ${VERBOSE_ARGS} -i -w -x -f ${UNZIP_DIR} | egrep '^IMAGE_|^export IMAGE_' | sed -e 's,^IMAGE_\([A-Za-z0-9_]*\)=,IMAGE_IMAGE_\1=,' -e 's,^export IMAGE_\([A-Za-z0-9_]*\)$,export IMAGE_IMAGE_\1,'`

    # Call tpkg_extract to verify the image, and to get the
    # unencrypted, but still maybe compressed, tarball out of the
    # unzip directory.  Since -X is specified, it does not write the tar
    # ball (but returns success) if there is no encryption, to save peak
    # space; the caller can just use the unzip dir tarball in this case.
    FAILURE=0
    if [ ${TRACK_PROGRESS} -eq 1 ]; then
        if [ "x${IMAGE_IMAGE_PAYLOAD_SIZE}" != "x" ]; then
            SIZE_ARGS="--file-size ${IMAGE_IMAGE_PAYLOAD_SIZE}"
        else
            SIZE_ARGS=
        fi
        do_verbose ${PROGRESS} -i ${PROGRESS_OPER_ID} --dest-dir ${UNENCRYPT_DIR} ${SIZE_ARGS} -l 30.0 -h 95.0 -- ${TPKG_EXTRACT} ${TPKG_ARGS} ${VERBOSE_ARGS} -f ${UNZIP_DIR} -i -x ${UNENCRYPT_DIR} -X || FAILURE=1
    else
        do_verbose ${TPKG_EXTRACT} ${TPKG_ARGS} ${VERBOSE_ARGS} -f ${UNZIP_DIR} -i -x ${UNENCRYPT_DIR} -X || FAILURE=1
    fi
    if [ ${FAILURE} -eq 1 ]; then
        vlechoerr -1 "*** Invalid image: could not extract ${SYSIMAGE_FRIENDLY}"
        cleanup_exit "Invalid image"
    fi

    if [ ${TRACK_PROGRESS} -eq 1 ]; then
        # Step 1f
        ${MDREQ} action /progress/actions/update_step \
            oper_id string ${PROGRESS_OPER_ID} \
            percent_done float32 95.0 \
            step_status string "Wrap-up"
    fi

    # See if we decrypted anything, and if so note it
    IMAGE_BALL_DIR=${UNZIP_DIR}
    SYSIMAGE_FILE_ENCRYPTED_IMAGE=
    if [ -d ${UNENCRYPT_DIR} ]; then
        sfi=`ls ${UNENCRYPT_DIR}/image-*.*`
        if [ ! -z "${sfi}" ]; then
            IMAGE_BALL_DIR=${UNENCRYPT_DIR}
            SYSIMAGE_FILE_ENCRYPTED_IMAGE=`ls ${UNZIP_DIR}/image-*.*`
        fi
    fi

    # XXXX everywhere in tpkg lets there be multiple image-*.* files
    SYSIMAGE_FILE_IMAGE=`ls ${IMAGE_BALL_DIR}/image-*.* | head -1`
    SYSIMAGE_FILE_TAR=`echo ${UNZIP_DIR}/${local_filename} | sed -e 's/.img$/.tar/'`
    if [ "${SYSIMAGE_FILE_TAR}" = "${UNZIP_DIR}/${local_filename}" ]; then
        SYSIMAGE_FILE_TAR=${SYSIMAGE_FILE_TAR}.tar
    fi

    # Print the version strings
    vlecho 0 "== Running version: ${BUILD_PROD_VERSION}"
    vlecho 0 "== Image version:   ${IMAGE_BUILD_PROD_VERSION}"
    if [ ! -z "${IMAGE_IMAGE_PAYLOAD_SIZE}" ]; then
        # Use awk to avoid busybox expr/dc limitations
        IPS_MB=$(echo ${IMAGE_IMAGE_PAYLOAD_SIZE} | awk '{f= $1 / 1024 / 1024; print int(f)}')
        IPUS_MB=$(echo ${IMAGE_IMAGE_PAYLOAD_UNCOMPRESSED_SIZE} | awk '{f= $1 / 1024 / 1024; print int(f)}')
        vlecho 0 "== Image size: ${IPS_MB} MB / ${IPUS_MB} MB uncompressed"
    fi
 
    # See if this is going to be okay
    DO_ARCH_CHECKS=1
    arch_ok=1
    DO_HWNAME_CHECKS=1
    hwname_ok=1

    # Used for comparision about our hwname
    hwname="${IL_HWNAME}"

    eval 'HWNAME_REQUIRED="${IL_LO_'${IL_LAYOUT}'_HWNAME_REQUIRED}"'
    if [ -z "${HWNAME_REQUIRED}" -o "${HWNAME_REQUIRED}" != "1" ]; then
        HWNAME_REQUIRED=0
    else
        HWNAME_REQUIRED=1
    fi

    # If there's no HWNAME specified, and the HWNAME is not strictly
    # required, and the image's HWNAME settings are empty, all of which
    # are typical for x86, do not do HWNAME checks.  For all other
    # cases, we'll check below.
    if [ -z "${hwname}" -a "${HWNAME_REQUIRED}" = 0 -a -z "${IMAGE_BUILD_TARGET_HWNAME}" -a -z "${IMAGE_BUILD_TARGET_HWNAMES_COMPAT}" ]; then
        DO_HWNAME_CHECKS=0
    fi

    if [ "$HAVE_WRITEIMAGE_GRAFT_4" = "y" ]; then
        writeimage_graft_4
    else
        # By default, we make sure the product matches
        if [ ${BUILD_PROD_PRODUCT} != ${IMAGE_BUILD_PROD_PRODUCT} ]; then
            vlechoerr -1 "*** Bad product: image ${IMAGE_BUILD_PROD_PRODUCT} on system ${BUILD_PROD_PRODUCT}"
            cleanup_exit "Mismatched image product"
        fi
    fi

    if [ "${DO_ARCH_CHECKS}" = "1" ]; then
        if [ "${BUILD_TARGET_ARCH}" = "${IMAGE_BUILD_TARGET_ARCH}" ]; then
            arch_ok=1
        else
            arch_ok=0
        fi
        if [ "${BUILD_TARGET_ARCH}" = "X86_64" -a "${IMAGE_BUILD_TARGET_ARCH}" = "I386" ]; then
            arch_ok=1
        fi
        if [ "${BUILD_TARGET_ARCH}" = "I386" -a "${IMAGE_BUILD_TARGET_ARCH}" = "X86_64" ]; then
            # XXX? this may not be safe, if the system doesn't support it
            cat /proc/cpuinfo | grep '^flags' | tr ': ' '\n\n' | grep -q '^lm$'
            if [ $? -eq 0 ]; then
                arch_ok=1
            fi
        fi
    fi

    if [ "${arch_ok}" != "1" ]; then
        vlechoerr -1 "*** Bad architecture: image ${IMAGE_BUILD_TARGET_ARCH} on system ${BUILD_TARGET_ARCH}"
        cleanup_exit "Incorrect image architecture"
    fi

    if [ "${DO_HWNAME_CHECKS}" = "1" ]; then
        if [ "${hwname}" = "${IMAGE_BUILD_TARGET_HWNAME}" ]; then
            hwname_ok=1
        else
            hwname_ok=0
            
            for hwn in ${IMAGE_BUILD_TARGET_HWNAMES_COMPAT} ; do
                if [ "${hwn}" = "${hwname}" ]; then
                    hwname_ok=1
                fi
            done
        fi

    fi

    if [ "${hwname_ok}" != "1" ]; then
        vlechoerr -1 "*** Bad hardware: our hardware is '${hwname}' but image is compatible with: '${IMAGE_BUILD_TARGET_HWNAMES_COMPAT}'"
        cleanup_exit "Hardware is not compatible with image hardware"
    fi


    # If we had encryption, we no longer need the encrypted form
    if [ ! -z "${SYSIMAGE_FILE_ENCRYPTED_IMAGE}" ]; then
        rm -f ${SYSIMAGE_FILE_ENCRYPTED_IMAGE}
    fi
else
    vlecho 0 "==== Old-style image, not verifying image integrity for ${SYSIMAGE_FRIENDLY}"
    SYSIMAGE_FILE_IMAGE=${SYSIMAGE_FILE}
    SYSIMAGE_FILE_TAR=`echo ${UNZIP_DIR}/${local_filename} | sed -e 's/.tbz$/.tar/' -e 's/.tgz$/.tar/' -e 's/.img$/.tar/'`
fi

sysimage_file_extension=`echo ${SYSIMAGE_FILE_IMAGE} | sed 's/^\(.*\)\.\([^\.]*\)$/\2/'`

if [ ${SYSIMAGE_FILE_IMAGE} != ${SYSIMAGE_FILE_TAR} ]; then
    rm -f ${SYSIMAGE_FILE_TAR}
fi

# Step #2: Uncompress Image
if [ ${TRACK_PROGRESS} -eq 1 ]; then
    ${MDREQ} -q action /progress/actions/begin_step \
        oper_id string ${PROGRESS_OPER_ID} \
        quant bool true \
        step_number uint32 2
fi

# Uncompress image file first, for speed
vlecho 0 "==== Uncompressing source image file: ${SYSIMAGE_FILE_IMAGE} to ${SYSIMAGE_FILE_TAR}"
FAILURE=0

# This is the common case
if [ "${sysimage_file_extension}" = "tbz" ]; then
    if [ ${TRACK_PROGRESS} -eq 1 ]; then
        if [ "x${IMAGE_IMAGE_PAYLOAD_UNCOMPRESSED_SIZE}" != "x" ]; then
            SIZE_ARGS="--file-size ${IMAGE_IMAGE_PAYLOAD_UNCOMPRESSED_SIZE}"
        else
            SIZE_ARGS=
        fi
        ${PROGRESS} -i ${PROGRESS_OPER_ID} --dest-file ${SYSIMAGE_FILE_TAR} ${SIZE_ARGS} -- ${BZCAT} ${SYSIMAGE_FILE_IMAGE} > ${SYSIMAGE_FILE_TAR} || FAILURE=1
    else
        # Juniper Change - Debug output
        df >> ${LOG_FILE}
        echo ls -l ${SYSIMAGE_FILE_IMAGE} >> ${LOG_FILE}
        ls -l ${SYSIMAGE_FILE_IMAGE} >> ${LOG_FILE}
        echo "${BZCAT} ${SYSIMAGE_FILE_IMAGE} > ${SYSIMAGE_FILE_TAR}" >> ${LOG_FILE}
        if [ ${VERBOSE} -gt 0 ]; then
            df
            echo ls -l ${SYSIMAGE_FILE_IMAGE}
            ls -l ${SYSIMAGE_FILE_IMAGE}
            echo "${BZCAT} ${SYSIMAGE_FILE_IMAGE} > ${SYSIMAGE_FILE_TAR}"
        fi
        # Juniper Change end
        ${BZCAT} ${SYSIMAGE_FILE_IMAGE} > ${SYSIMAGE_FILE_TAR} || FAILURE=1
        # Juniper Change - Debug output
        echo RetVal=${FAILURE} >> ${LOG_FILE}
        echo ls -l ${SYSIMAGE_FILE_TAR} >> ${LOG_FILE}
        ls -l ${SYSIMAGE_FILE_TAR} >> ${LOG_FILE}
        df >> ${LOG_FILE}
        if [ ${VERBOSE} -gt 0 ]; then
            echo RetVal=${FAILURE}
            echo ls -l ${SYSIMAGE_FILE_TAR}
            ls -l ${SYSIMAGE_FILE_TAR}
            df
        fi
        # Juniper Change end
    fi
    if [ ${FAILURE} -eq 1 ]; then
        vlechoerr -1 "*** Could not uncompress image ${SYSIMAGE_FILE_IMAGE}"
        cleanup_exit "Could not uncompress image"
    fi
elif [ "${sysimage_file_extension}" = "tgz" ]; then

    # XXX/EMT: we originally fed the gzipped image to zcat on stdin, 
    # like "cat ${SYSIMAGE_FILE_IMAGE} | ${PROGRESS} ... -- ${ZCAT} -".
    # This is because long ago, Greg decided it was safer this way,
    # possibly because we don't trust Busybox's gzip to take it as a 
    # filename.  But this yields terrible performance when using the
    # progress wrapper, as it is inefficient in how it forwards stdin
    # to the program it forks.  So in the progress case (which is 
    # currently only in the full image), we just give it the filename.
    # In the non-progress case (the only case we get with Busybox),
    # we still invoke it the old-fashioned way.
    if [ ${TRACK_PROGRESS} -eq 1 ]; then
        if [ "x${IMAGE_IMAGE_PAYLOAD_UNCOMPRESSED_SIZE}" != "x" ]; then
            SIZE_ARGS="--file-size ${IMAGE_IMAGE_PAYLOAD_UNCOMPRESSED_SIZE}"
        else
            SIZE_ARGS=
        fi
        ${PROGRESS} -i ${PROGRESS_OPER_ID} --dest-file ${SYSIMAGE_FILE_TAR} ${SIZE_ARGS} -- ${ZCAT} ${SYSIMAGE_FILE_IMAGE} > ${SYSIMAGE_FILE_TAR} || FAILURE=1
    else 
        cat ${SYSIMAGE_FILE_IMAGE} | ${ZCAT} - > ${SYSIMAGE_FILE_TAR} || FAILURE=1
    fi

    if [ ${FAILURE} -eq 1 ]; then
        vlechoerr -1 "*** Could not uncompress image ${SYSIMAGE_FILE_IMAGE}"
        cleanup_exit "Could not uncompress image"
    fi
elif [ "${sysimage_file_extension}" = "tar" ]; then
    if [ ${SYSIMAGE_FILE_IMAGE} != ${SYSIMAGE_FILE_TAR} ]; then
        mv ${SYSIMAGE_FILE_IMAGE} ${SYSIMAGE_FILE_TAR} || FAILURE=1
        if [ ${FAILURE} -eq 1 ]; then
            vlechoerr -1 "*** Could not uncompress image ${SYSIMAGE_FILE_IMAGE}"
            cleanup_exit "Could not uncompress image"
        fi
    fi
else
    vlechoerr -1 "*** Invalid extension on image inner ball: ${sysimage_file_extension}"
    cleanup_exit "Invalid image file"
fi



# We no longer need the compressed tarball in our workspace
if [ ${SYSIMAGE_FILE_IMAGE} != ${SYSIMAGE_FILE_TAR} ]; then
    rm -f ${SYSIMAGE_FILE_IMAGE}
fi

# For the non-manufacture case, mark the image location we are
# installing to as invalid
if [ ${DO_MANUFACTURE} -eq 0 ]; then
    vlecho 0 "-- Disabling image we are installing over"

    # Get our location's current state into PREV_LOCATION_STATE
    eval `/sbin/aiget.sh | grep '^AIG_LOC_ID_[^=]*_STATE=' | sed -e 's,^\([^=]*\)=,PREV_\1=,'`
    PREV_LOCATION_STATE=
    OTHER_LOCATION_STATE=
    if [ "${INSTALL_IMAGE_ID}" = "1" ]; then
        PREV_LOCATION_STATE="${PREV_AIG_LOC_ID_1_STATE}"
        PREV_OTHER_LOCATION_STATE="${PREV_AIG_LOC_ID_2_STATE}"
    elif [ "${INSTALL_IMAGE_ID}" = "2" ]; then
        PREV_LOCATION_STATE="${PREV_AIG_LOC_ID_2_STATE}"
        PREV_OTHER_LOCATION_STATE="${PREV_AIG_LOC_ID_1_STATE}"
    fi

    ${AISET_PATH} -i -I "${INSTALL_IMAGE_ID}" -s 0
fi

# Step #3: Make Filesystems
if [ ${TRACK_PROGRESS} -eq 1 ]; then
    ${MDREQ} -q action /progress/actions/begin_step \
        oper_id string ${PROGRESS_OPER_ID} \
        quant bool true \
        step_number uint32 3
fi

if [ ${USE_TMPFS} -eq 1 ]; then

    # If we get here, any disk we care about is thought to be unmounted

    # Write the new partition table
    if [ ${DO_MANUFACTURE} -eq 1 -a ${NO_PARTITIONING} -eq 0 ]; then
        do_partition_disks
    fi

    # Make new filesystems on disk or disks
    if [ ${DO_MANUFACTURE} -eq 1 -o ${NO_REWRITE_FILESYSTEMS} -eq 0 ]; then
        do_make_filesystems
    fi
else
    # In the manufacturing case, we already made the filesystems above.

    if [ ${DO_MANUFACTURE} -eq 0 -a ${NO_REWRITE_FILESYSTEMS} -eq 0 ]; then
        do_make_filesystems
    fi
fi

# Step #4: Extract Image
if [ ${TRACK_PROGRESS} -eq 1 ]; then
    ${MDREQ} -q action /progress/actions/begin_step \
        oper_id string ${PROGRESS_OPER_ID} \
        quant bool true \
        step_number uint32 4
fi

# At this point we've either created all the paritions, or they already existed
# and this is a running system.

vlecho 0 "==== Extracting image contents from: ${SYSIMAGE_FILE_TAR}"

# Keep a list of any root locations that require
# a two stage boot involving the need for an initrd
NEED_INITRD=

for target in ${TARGET_CHANGE_LIST}; do

    vlecho 0 "=== Extract to target ${target}"

    eval 'loc_list="${TARGET_'${target}'_CHANGE_LOCS}"'

    #
    # We want to track our progress within the "Extract Image" step.
    # There are 2n substeps, where n is the number of locations:
    # 1 to do any untarring, etc.; and 1 to sync and unmount the location.
    # Here we figure out what n is, and calculate what percentage of the
    # step we should pass for each of these substeps.
    #
    # We start currpct negative, to simplify the convention for updating
    # it in the loop below.
    #
    numlocs=0
    if [ ! -z "${loc_list}" ]; then
        for dummy in ${loc_list}; do 
            numlocs=$((${numlocs} + 1))
        done
        numsteps=$((${numlocs} * 2))
        pctperstep=$((100 / ${numsteps}))
        currpct=$((0 - ${pctperstep}))

    else
        numsteps=0
        pctperstep=100
        currpct=0
    fi
    
    vlecho 2 "Items in loc_list: ${numlocs}"
    vlecho 2 "Percent for each location: ${pctperstep}"

    for location in ${loc_list}; do

        if [ ${TRACK_PROGRESS} -eq 1 ]; then
            currpct=$((${currpct} + ${pctperstep}))
            ${MDREQ} action /progress/actions/update_step \
                oper_id string ${PROGRESS_OPER_ID} \
                percent_done float32 ${currpct} \
                step_status string "Extracting to ${location}"
        fi
        
        eval 'curr_part="${IL_LO_'${IL_LAYOUT}'_LOC_'${location}'_PART}"'
        eval 'curr_vpart="${IL_LO_'${IL_LAYOUT}'_LOC_'${location}'_VPART}"'

        eval 'curr_dir="${IL_LO_'${IL_LAYOUT}'_LOC_'${location}'_DIR}"'
        eval 'curr_extract="${IL_LO_'${IL_LAYOUT}'_LOC_'${location}'_IMAGE_EXTRACT}"'
        eval 'curr_extract_exclude="${IL_LO_'${IL_LAYOUT}'_LOC_'${location}'_IMAGE_EXTRACT_EXCLUDE}"'
        eval 'curr_extract_prefix="${IL_LO_'${IL_LAYOUT}'_LOC_'${location}'_IMAGE_EXTRACT_PREFIX}"'
        eval 'curr_extract_dest="${IL_LO_'${IL_LAYOUT}'_LOC_'${location}'_IMAGE_EXTRACT_DEST}"'
        is_xmtd=0

        if [ ! -z "${curr_part}" ]; then
            # Things to do only if a physical partition
            eval 'curr_fstype="${IL_LO_'${IL_LAYOUT}'_PART_'${curr_part}'_FSTYPE}"'
            eval 'curr_part_id="${IL_LO_'${IL_LAYOUT}'_PART_'${curr_part}'_PART_ID}"'
            if [ "${curr_part_id}" = "d9" ]; then
                is_xmtd=1
            elif [ -z "${curr_fstype}" -o "${curr_fstype}" = "swap" ]; then
                vlecho 0 "-- Nothing to do for location ${location}."
                continue
            elif [ -z "${curr_extract}" ]; then
                vlecho 0 "-- Nothing to extract for ${curr_part}."
                continue
            fi
        fi

        if [ ! -z "${curr_vpart}" ]; then
            # Get physical partition by indirection
            eval 'curr_part="${IL_LO_'${IL_LAYOUT}'_VPART_'${curr_vpart}'_PART_NAME}"'
            eval 'curr_vp_file_name="${IL_LO_'${IL_LAYOUT}'_VPART_'${curr_vpart}'_FILE_NAME}"'
            eval 'curr_v_fstype="${IL_LO_'${IL_LAYOUT}'_VPART_'${curr_vpart}'_FSTYPE}"'
            eval 'dev="${IL_LO_'${IL_LAYOUT}'_PART_'${curr_part}'_DEV}"'
            eval 'cdev="${IL_LO_'${IL_LAYOUT}'_PART_'${curr_part}'_DEV_CHAR}"'
            eval 'part_mount="${IL_LO_'${IL_LAYOUT}'_PART_'${curr_part}'_MOUNT}"'

            if [ ${DO_MANUFACTURE} -eq 1 ]; then
                phys_part_mount=${VPART_PHYS_PART_MOUNT}
                # Mount the backing partition
                mount ${dev} ${phys_part_mount} || FAILURE=1
                if [ ${FAILURE} -eq 1 ]; then
                    vlechoerr -1 "*** Could not mount partition ${dev} on ${phys_part_mount}"
                    cleanup_exit
                fi
            else
                # Should be already mounted on a running system
                phys_part_mount=${part_mount}
                mount -o remount,rw ${phys_part_mount} || FAILURE=1
                if [ ${FAILURE} -eq 1 ]; then
                    vlechoerr -1 "*** Could not remount partition ${dev} on ${phys_part_mount}"
                    cleanup_exit
                fi
            fi

            UNMOUNT_VPART_PHYS=${phys_part_mount}

            vlecho 0 "-- Mount backing partition ${dev} on ${phys_part_mount}"

            vp_path=${phys_part_mount}/${curr_vp_file_name}
        fi

        eval 'curr_dev="${IL_LO_'${IL_LAYOUT}'_PART_'${curr_part}'_DEV}"'
        eval 'curr_cdev="${IL_LO_'${IL_LAYOUT}'_PART_'${curr_part}'_DEV_CHAR}"'
        eval 'curr_target="${IL_LO_'${IL_LAYOUT}'_PART_'${curr_part}'_TARGET}"'
        eval 'curr_target_dev="${IL_LO_'${IL_LAYOUT}'_TARGET_'${curr_target}'_DEV}"'

        if [ "${is_xmtd}" = "1" ]; then
            eval 'curr_extract_mtd="${IL_LO_'${IL_LAYOUT}'_LOC_'${location}'_IMAGE_EXTRACT_MTD}"'
            if [ -z "${curr_extract_mtd}" ]; then
                vlecho 0 "== Nothing to extract for MTD location ${location} on ${curr_cdev}"
                continue
            fi

            vlecho 0 "== Extracting by copying to MTD for location ${location} on ${curr_cdev}"

            # XXX-mtd: vpart on MTD not supported

            # Mount the source location
            eval 'xmtd_part="${IL_LO_'${IL_LAYOUT}'_LOC_'${location}'_SRC_PART}"'
            eval 'xmtd_dev="${IL_LO_'${IL_LAYOUT}'_PART_'${xmtd_part}'_DEV}"'
            eval 'xmtd_fstype="${IL_LO_'${IL_LAYOUT}'_PART_'${xmtd_part}'_FSTYPE}"'

            xmtd_mount_options="-o noatime,ro -t ${xmtd_fstype}"
            xmtd_mount_point="${TMP_MNT_IMAGE}/${curr_target}/${xmtd_part}"

            # XXXX-mtd!: this is fragile, and it's likely problematic if
            # the layout picks partitions that are already mounted, but
            # that writeimage hasn't mounted.
            eval 'amp="${PART_PRE_MOUNTED_MOUNTPOINT_'${xmtd_part}'}"'
            if [ -z "${amp}" ]; then
                mkdir -p ${xmtd_mount_point}
                FAILURE=0
                vlecho 0 "-- Mounting ${xmtd_dev} on ${xmtd_mount_point}"
                mount ${xmtd_mount_options} ${xmtd_dev} ${xmtd_mount_point} || FAILURE=1
                if [ ${FAILURE} -eq 1 ]; then
                    vlechoerr -1 "*** Could not mount partition ${xmtd_dev} on ${xmtd_mount_point}"
                    cleanup_exit
                fi
                UNMOUNT_XMTD=${xmtd_mount_point}

                xmtd_from_path=${xmtd_mount_point}/${curr_extract_mtd}
            else
                xmtd_from_path=${amp}/${curr_extract_mtd}
            fi

            if [ ! -f "${xmtd_from_path}" ]; then
                vlechoerr -1 "*** Could not find file to copy for ${location} from ${xmtd_from_path}"
                cleanup_exit
            fi

            FAILURE=0
            ${CP} ${xmtd_from_path} ${curr_cdev} || FAILURE=1
            if [ ${FAILURE} -eq 1 ]; then
                vlechoerr -1 "*** Could not copy for ${location} to ${curr_cdev} from ${xmtd_from_path}"
                cleanup_exit
            fi


            # XXXX-mtd!: we currently verify that the copy worked correctly, as
            # imgverify does not do this right now for raw MTD files.

            # The MTD may be bigger than the source file, so deal with this
            VMC_ORIG_SIZE=$(ls -lLn ${xmtd_from_path} | awk '{print $5}')
            VMC_BLOCKS=$(echo ${VMC_ORIG_SIZE} | awk '{s=4096; b= int($1 / s); x=$1 % s; k=b * s; if (k + x == $1) {print b}}')
            VMC_EXTRA=$(echo  ${VMC_ORIG_SIZE} | awk '{s=4096; b= int($1 / s); x=$1 % s; k=b * s; if (k + x == $1) {print x}}')
            VMC_SKIP=$(echo   ${VMC_ORIG_SIZE} | awk '{s=4096; b= int($1 / s); x=$1 % s; k=b * s; if (k + x == $1) {print k}}')
            if [ -z "${VMC_BLOCKS}" -o -z "${VMC_EXTRA}" -o -z "${VMC_SKIP}" ]; then
                vlechoerr -1 "*** Could not verify copy for ${location} to ${curr_cdev} from ${xmtd_from_path}"
                cleanup_exit "Could not verify copied file"
            fi

            # XXX #dep/parse: dd
            devsum=$( (dd if=${curr_cdev} bs=4096 count=${VMC_BLOCKS} conv=notrunc; dd if=${curr_cdev} bs=1 count=${VMC_EXTRA} skip=${VMC_SKIP} ) 2>/dev/null | md5sum | awk '{print $1}')
            filesum=$(md5sum ${xmtd_from_path} | awk '{print $1}')

            if [ "${devsum}" != "${filesum}" -o -z "${filesum}" ]; then
                vlechoerr -1 "*** Could not verify copy for ${location} to ${curr_cdev} from ${xmtd_from_path}"
                vlechoerr -1 "*** Failed sums: ${devsum} and ${filesum}"
                cleanup_exit "Could not verify copied file"
            fi
            vlecho 0 "-- Verified copy to MTD matches source file"

            ${SYNC}
            umount ${xmtd_mount_point}
            UNMOUNT_XMTD=

            continue
        fi

        vlecho 0 "== Extracting for location ${location} onto ${curr_dev}"

        mount_options=""

        mount_point="${TMP_MNT_IMAGE}/${curr_target}/${curr_part}/${curr_extract_prefix}"
        chdir_to="${TMP_MNT_IMAGE}/${curr_target}/${curr_part}"

        exclude_arg=""
        if [ ! -z "${curr_extract_exclude}" ]; then
            for xa in ${curr_extract_exclude}; do
                new_ex="--exclude ${xa}"
                exclude_arg="${exclude_arg} ${new_ex}"
            done
        fi

        vecho 1 "exclude_arg= ${exclude_arg}"

        need_sync=0
        if [ ! -z "${curr_vpart}" ]; then
            mount_options="-o loop,noatime -t ${curr_v_fstype}"

            curr_dev=${vp_path}

            # Note that this case is not supported
            if [ "${curr_v_fstype}" = "jffs2" ]; then
                need_sync=1
            fi
        else
            mount_options="-o noatime -t ${curr_fstype}"
            if [ "${curr_fstype}" = "jffs2" ]; then
                need_sync=1
            fi
        fi

        # Make sure it isn't already mounted, like in the -t case
        already_mounted=
        eval 'already_mounted="${PART_PRE_MOUNTED_'${curr_part}'}"'

        if [ -z "${already_mounted}" ]; then
            mkdir -p ${mount_point}
            FAILURE=0
            vlecho 0 "-- Mounting ${curr_dev} on ${mount_point}"
            mount ${mount_options} ${curr_dev} ${mount_point} || FAILURE=1
            if [ ${FAILURE} -eq 1 ]; then
                vlechoerr -1 "*** Could not mount partition ${curr_dev} on ${mount_point}"
                cleanup_exit
            fi
            if [ "${need_sync}" = "1" ]; then
                ${SYNC}
            fi
            UNMOUNT_EXTRACTFS=${mount_point}
        fi

        if [ ! -z "${curr_extract_dest}" ]; then
            chdir_to="${chdir_to}/${curr_extract_dest}"
            mkdir -p ${chdir_to}
        fi

        # Pre-extract special processing for standard locations

        case "${location}" in
            BOOTMGR_1|REPL_BOOTMGR_1) 
                if [ "${bootmgr_type}" = "grub" ]; then

                    vlecho 0 "-- Putting grub on ${curr_target_dev}"

                    rm -f ${mount_point}/boot/grub/device.map
                    mkdir -p ${mount_point}/boot/grub

                    # Allow customer to emit things into:
                    #      ${mount_point}/boot/grub/device.map
                    # This might be needed if the grub auto probing does not work.
                    #
                    #
                    # Customer can set DID_GRUB_INSTALL=1 if they're install
                    # the grub themselves in this graft point.
                    #
                    # NOTE: this graftpoint is similar to aiset graft point 1,
                    # which is used in aiset.sh if it is asked to re-install
                    # the grub on a running system.

                    DID_GRUB_INSTALL=0
                    if [ "$HAVE_WRITEIMAGE_GRAFT_5" = "y" ]; then
                        writeimage_graft_5
                    fi

                    if [ ${DID_GRUB_INSTALL} -ne 1 ]; then
                        FAILURE=0
                        do_verbose grub-install --no-floppy --root-directory=${mount_point} ${curr_target_dev} || FAILURE=1
                        if [ ${FAILURE} -eq 1 ]; then
                            vlechoerr -1 "*** Could not install grub on ${curr_target_dev}"
                            cleanup_exit "Could not install boot manager"
                        fi
                    fi

                fi
                
                # XXX-mtd: Note: we instead install u-boot in its own loc

                ;;
        esac


        vlecho 0 "-- Extracting files for ${location}"
        vecho 1 "   chdir_to= ${chdir_to} curr_extract= ${curr_extract}"

        failure_file=${WORKSPACE_DIR}/ff.$$
        rm -f ${failure_file}
        if [ ${TRACK_PROGRESS} -eq 1 ]; then
            currrangehi=$((${currpct} + ${pctperstep} - 1))
            # XXX #dep/parse: tar
            (${PROGRESS} -i ${PROGRESS_OPER_ID} -l ${currpct} -h ${currrangehi} --src-file ${SYSIMAGE_FILE_TAR} -- ${TAR} -xp${TAR_VERBOSE} -f ${SYSIMAGE_FILE_TAR} --record-size=262144 --checkpoint -C ${chdir_to} ${exclude_arg} ${curr_extract} 2>&1 || echo $? > ${failure_file}) 2>&1 | grep -v 'tar:.*: time stamp.*in the future$'
        else 
            # XXX #dep/parse: tar
            (${TAR} -xp${TAR_VERBOSE} -f ${SYSIMAGE_FILE_TAR} -C ${chdir_to} ${exclude_arg} ${curr_extract} 2>&1 || echo $? > ${failure_file}) 2>&1 | grep -v 'tar:.*: time stamp.*in the future$'
        fi

        if [ -f ${failure_file} -a ! -z ${failure_file} ]; then
            vlechoerr -1 "*** Could not extract files from ${SYSIMAGE_FILE_TAR} : $(cat ${failure_file})"
            cleanup_exit "Error extracting image.  This may be caused by insufficient disk space."
        fi
        rm -f ${failure_file}

        # Post-extract special processing for standard locations
        case "${location}" in
            VAR_1)
                vlecho 0 "-- Post-extraction work for: ${location}"
                cd ${chdir_to}
 
                # Mark that this is a newly manufactured system.
                FIRSTMFG_FILE=/var/opt/tms/.firstmfg
                if [ ${DO_MANUFACTURE} -eq 1 ]; then
                    rm -f ./${FIRSTMFG_FILE}
                    touch ./${FIRSTMFG_FILE}
                    chown 0:0 ./${FIRSTMFG_FILE}
                    chmod 644 ./${FIRSTMFG_FILE}
                fi

                # Save random seed, so it is available on initial boot
                RS_SOURCE=/dev/urandom
                RS_FILE=/var/lib/random-seed
                if [ ${DO_MANUFACTURE} -eq 1 -a -e "${RS_SOURCE}" ]; then
                    rm -f ./${RS_FILE}
                    touch ./${RS_FILE}
                    chown 0:0 ./${RS_FILE}
                    chmod 600 ./${RS_FILE}
                    dd if=${RS_SOURCE} of=./${RS_FILE} count=1 bs=512 > /dev/null 2>&1
                fi

                ;;
            BOOT_1|BOOT_2)
                vlecho 0 "-- Post-extraction work for: ${location}"

                cd ${mount_point}/${curr_dir}

                # FAT does not support symlinks, so just copy
                if [ "${curr_fstype}" = "vfat" ]; then
                    lnk_cmd="cp -p"
                else
                    lnk_cmd="ln -sf"
                fi
                # We want to potentially make or correct the kernel symlink
                if [ -e vmlinuz-${IL_KERNEL_TYPE} -a \( ! -e vmlinuz -o -L vmlinuz \) ]; then
                    rm -f vmlinuz
                    ${lnk_cmd} vmlinuz-${IL_KERNEL_TYPE} vmlinuz
                fi
                if [ -e System.map-${IL_KERNEL_TYPE} -a \( ! -e System.map -o -L System.map \) ]; then
                    rm -f System.map
                    ${lnk_cmd} System.map-${IL_KERNEL_TYPE} System.map
                fi
                if [ -e config-${IL_KERNEL_TYPE} -a \( ! -e config -o -L config \) ]; then
                    rm -f config
                    ${lnk_cmd} config-${IL_KERNEL_TYPE} config
                fi
                if [ -e fdt-${IL_KERNEL_TYPE} -a \( ! -e fdt -o -L fdt \) ]; then
                    rm -f fdt
                    ${lnk_cmd} fdt-${IL_KERNEL_TYPE} fdt
                fi

                # This next set of step is in case IL_KERNEL_TYPE does
                # not match what the image has.  Part of why we do this
                # is that the build used to produce these uni symlinks
                # automatically.  We want to do our best for there to be
                # some kernel present.

                fallback_type_list="uni smp"
                for fallback_type in ${fallback_type_list}; do
                    if [ -e vmlinuz-${fallback_type} -a ! -e vmlinuz ]; then
                        rm -f vmlinuz
                        ${lnk_cmd} vmlinuz-${fallback_type} vmlinuz
                    fi
                    if [ -e System.map-${fallback_type} -a ! -e System.map ]; then
                        rm -f System.map
                        ${lnk_cmd} System.map-${fallback_type} System.map
                    fi
                    if [ -e config-${fallback_type} -a ! -e config ]; then
                        rm -f config
                        ${lnk_cmd} config-${fallback_type} config
                    fi
                    if [ -e fdt-${fallback_type} -a ! -e fdt ]; then
                        rm -f fdt
                        ${lnk_cmd} fdt-${fallback_type} fdt
                    fi
                done

                eval 'boot_bootmgr_copy="${IL_LO_'${IL_LAYOUT}'_BOOT_BOOTMGR_COPY}"'
                if [ "${boot_bootmgr_copy}" != "1" ]; then
                    boot_bootmgr_copy=0
                fi

                if [ "${bootmgr_type}" = "grub" -a "${boot_bootmgr_copy}" = "1" ]; then
                    vlecho 0 "---- Doing boot bootmgr copy for ${location}"

                    # See if it is already mounted ro, mounted rw, or
                    # not mounted at all
                    bcp_mounted=0
                    bcp_was_rw=0
                    BCP_REMOUNTED_RW=0

                    # NOTE: this is another place the BOOTMGR mounted on
                    # /bootmgr assumption is burned in, just like in
                    # aiset.sh .

                    # XXXXX PART_PRE_MOUNTED_ check?  Use /bootmgr for
                    # scratch space will not work without such a check.

                    if [ ${DO_MANUFACTURE} -eq 0 ]; then
                        # XXX #dep/parse: mount
                        bcp_mount_line=`/bin/mount | grep '^.* on /bootmgr type .*$'`
                        bcp_dev=`echo $bcp_mount_line | awk '{print $1}'`
                        bcp_opts=`echo $bcp_mount_line | awk '{print $6}' | sed 's/^(\(.*\))/\1/' | tr ',' '\n'`
                        if [ -z "${bcp_mount_line}" -o -z "${bcp_dev}" ]; then
                            bcp_mounted=0
                        else
                            bcp_mounted=1
                            echo "${bcp_opts}" | grep -q '^rw$'
                            if [ $? -eq 0 ]; then
                                bcp_was_rw=1
                            fi
                        fi
                    fi

                    eval 'bcp_name_part="${IL_LO_'${IL_LAYOUT}'_LOC_BOOTMGR_1_PART}"'
                    eval 'bcp_dir="${IL_LO_'${IL_LAYOUT}'_LOC_BOOTMGR_1_DIR}"'
                    eval 'bcp_dev="${IL_LO_'${IL_LAYOUT}'_PART_'${bcp_name_part}'_DEV}"'
                    eval 'bcp_fstype="${IL_LO_'${IL_LAYOUT}'_PART_'${bcp_name_part}'_FSTYPE}"'

                    bcp_root_dir=${TMP_MNT_IMAGE}/bcp/BOOTMGR
                    bcp_mount=${TMP_MNT_IMAGE}/bcp/BOOTMGR/bootmgr
                    bcp_mount_options="-o noatime -t ${bcp_fstype}"

                    bcp_copy_root=

                    if [ "${bcp_mounted}" = "0" ]; then
                        mkdir -p ${bcp_mount}
                        BCP_BOOTMGR_UMOUNT=${bcp_mount}
                        bcp_copy_root=${bcp_root_dir}

                        FAILURE=0
                        mount ${bcp_mount_options} ${bcp_dev} ${bcp_mount} || FAILURE=1
                        if [ ${FAILURE} -eq 1 ]; then
                            BCP_BOOTMGR_UMOUNT=
                            vlechoerr -1 "*** Could not mount partition ${bcp_dev} on ${bcp_mount}"
                            cleanup_exit
                        fi
                    else
                        bcp_copy_root=/
                        if [ "${bcp_was_rw}" = "0" ]; then
                            BCP_REMOUNTED_RW=1
                            FAILURE=0
                            mount -o remount,rw /bootmgr || FAILURE=1
                            if [ ${FAILURE} -eq 1 ]; then
                                BCP_REMOUNTED_RW=0
                                vlechoerr -1 "*** Could not remount rw /bootmgr"
                                cleanup_exit
                            fi
                        fi
                    fi

                    # /bootmgr is now mounted and read-write

                    bcp_source_dir=./
                    bcp_target_dir=${bcp_copy_root}/bootmgr/boot/`echo "${location}" | tr '[A-Z]' '[a-z]' | sed 's/[^a-z0-9_-]/_/g'`
                    mkdir -p ${bcp_target_dir}

                    FAILURE=0
                    if [ "$(echo ${bcp_source_dir}/vmlinuz*)" != "${bcp_source_dir}/vmlinuz*" ]; then
                        /bin/cp -p ${bcp_source_dir}/vmlinuz* ${bcp_target_dir} || FAILURE=1
                    fi
                    if [ "$(echo ${bcp_source_dir}/System.map*)" != "${bcp_source_dir}/System.map*" ]; then
                        /bin/cp -p ${bcp_source_dir}/System.map* ${bcp_target_dir} || FAILURE=1
                    fi
                    if [ "$(echo ${bcp_source_dir}/config*)" != "${bcp_source_dir}/config*" ]; then
                        /bin/cp -p ${bcp_source_dir}/config* ${bcp_target_dir} || FAILURE=1
                    fi
                    if [ "$(echo ${bcp_source_dir}/fdt*)" != "${bcp_source_dir}/fdt*" ]; then
                        /bin/cp -p ${bcp_source_dir}/fdt* ${bcp_target_dir} || FAILURE=1
                    fi

                    if [ ${FAILURE} -eq 1 ]; then
                        vlechoerr -1 "*** Could not copy boot files to bootmgr"
                        cleanup_exit
                    fi

                    # Potentially unmount or remount ro bootmgr
                    ${SYNC}
                    if [ ! -z "${BCP_BOOTMGR_UMOUNT}" ]; then
                        umount ${BCP_BOOTMGR_UMOUNT}
                        BCP_BOOTMGR_UMOUNT=
                    else
                        if [ "${BCP_REMOUNTED_RW}" = "1" ]; then
                            mount -o remount,ro /bootmgr || FAILURE=1
                            if [ ${FAILURE} -eq 1 ]; then
                                vlechoerr -1 "*** Could not remount ro /bootmgr"
                                cleanup_exit
                            fi
                            BCP_REMOUNTED_RW=0
                        fi
                    fi
                fi

                ;;

            ROOT_1|ROOT_2)
                vlecho 0 "-- Post-extraction work for: ${location}"

                if [ ! -z "${curr_vpart}" ]; then
                    eval 'isrootfs="${IL_LO_'${IL_LAYOUT}'_VPART_'${curr_vpart}'_ISROOTFS}"'
                    if [ ${isrootfs} -eq 1 ]; then
                        NEED_INITRD="${location} ${NEED_INITRD}"
                    fi
                fi

                cd ${mount_point}/${curr_dir}

                mkdir -p -m 755 ./boot
                mkdir -p -m 755 ./bootmgr
                mkdir -p -m 755 ./var
                mkdir -p -m 755 ./config
                mkdir -p -m 755 ./data
                mkdir -p -m 755 ./sys
                mkdir -p -m 755 ./proc

                eval 'extra_mount_points="${IL_LO_'${IL_LAYOUT}'_LOC_'${location}'_IMAGE_EXTRA_MOUNTS}"'
                if [ ! -z "${extra_mount_points}" ]; then
                    for emp in ${extra_mount_points}; do
                        mkdir -p -m 755 ${emp}
                    done
                fi

                if [ "$HAVE_WRITEIMAGE_GRAFT_2" = "y" ]; then
                    writeimage_graft_2
                fi

                # The graft point script may have done a cd, so cd again
                cd ${mount_point}/${curr_dir}

                LAYOUT_FILE=etc/image_layout.sh
                rm -f ./${LAYOUT_FILE}
                cat > ./${LAYOUT_FILE} <<EOF
# Automatically generated file: DO NOT EDIT!
#
IL_LAYOUT=${IL_LAYOUT}
export IL_LAYOUT
IL_KERNEL_TYPE=${IL_KERNEL_TYPE}
export IL_KERNEL_TYPE
EOF

                if [ ! -z "${IL_HWNAME}" ]; then
                    echo "IL_HWNAME=${IL_HWNAME}" >> ./${LAYOUT_FILE}
                    echo "export IL_HWNAME" >> ./${LAYOUT_FILE}
                fi

                for rtn in ${TARGET_NAMES}; do
                    eval 'tnd="${IL_LO_'${IL_LAYOUT}'_TARGET_'${rtn}'_DEV}"'
                    echo "IL_LO_${IL_LAYOUT}_TARGET_${rtn}_DEV=${tnd}" >> ./${LAYOUT_FILE}
                    echo "export IL_LO_${IL_LAYOUT}_TARGET_${rtn}_DEV" >> ./${LAYOUT_FILE}
                done

                chmod 0644 ./${LAYOUT_FILE}

                # Generate the fstab for this target

                # XXX ordering of these lines?

                FSTAB_FILE=etc/fstab
                FSTAB_LINES_TOP_FILE=etc/fstab.lines.top
                FSTAB_LINES_BOTTOM_FILE=etc/fstab.lines.bottom
                rm -f ./${FSTAB_FILE}
                touch ./${FSTAB_FILE}


                if [ -f ./${FSTAB_LINES_TOP_FILE} ]; then
                    cat ./${FSTAB_LINES_TOP_FILE} >> ./${FSTAB_FILE}
                fi
                if [ "${location}" = "ROOT_1" ]; then
                    part_list="${IMAGE_1_PART_LIST}"
                    vpart_list="${IMAGE_1_VPART_LIST}"
                else
                    part_list="${IMAGE_2_PART_LIST}"
                    vpart_list="${IMAGE_2_VPART_LIST}"
                fi

                for mpart in ${part_list}; do
                    vecho 1 "part= ${mpart}"

                    eval 'part_dev="${IL_LO_'${IL_LAYOUT}'_PART_'${mpart}'_DEV}"'
                    eval 'part_label="${IL_LO_'${IL_LAYOUT}'_PART_'${mpart}'_LABEL}"'
                    eval 'part_mount="${IL_LO_'${IL_LAYOUT}'_PART_'${mpart}'_MOUNT}"'
                    eval 'part_fstype="${IL_LO_'${IL_LAYOUT}'_PART_'${mpart}'_FSTYPE}"'
                    eval 'part_options="${IL_LO_'${IL_LAYOUT}'_PART_'${mpart}'_OPTIONS}"'
                    if [ -z "${part_options}" ]; then
                        part_options="defaults,noatime"
                    fi
                    part_dumpfreq=1
                    part_fsckpass=2

                    if [ "${part_fstype}" = "swap" ]; then
                        part_dumpfreq=0
                        part_fsckpass=0
                    fi

                    # XXX Assumption about where things are mounted
                    if [ "${part_mount}" = "/" ]; then
                        part_fsckpass=1
                    fi

                    # XXX! One of the issues with all this is that we are
                    # assuming the place where we are mounting the device
                    # now is where we want to mount the device later.  This
                    # is not correct if we have some manufacturing box!

                    
                    if [ ! -z "${part_fstype}" -a ! -z "${part_mount}" ]; then
                        if [ ! -z "${part_label}" ]; then
                            echo "LABEL=${part_label}	${part_mount}	${part_fstype}	${part_options}	${part_dumpfreq} ${part_fsckpass}" >> ./${FSTAB_FILE}
                        else
                            echo "${part_dev}	${part_mount}	${part_fstype}	${part_options}	${part_dumpfreq} ${part_fsckpass}" >> ./${FSTAB_FILE}
                        fi
                    else
                        vecho -1 "-- No fstab entry for ${mpart}"
                    fi
                        
                done

                for vpart in ${vpart_list}; do
                    vecho 1 "vpart= ${vpart}"

                    eval 'vpart_mount="${IL_LO_'${IL_LAYOUT}'_VPART_'${vpart}'_MOUNT}"'
                    eval 'vpart_size="${IL_LO_'${IL_LAYOUT}'_VPART_'${vpart}'_FSSIZE}"'
                    eval 'vpart_rdonly="${IL_LO_'${IL_LAYOUT}'_VPART_'${vpart}'_RDONLY}"'

                    vpart_fstype=tmpfs
                    if [ "${vpart_rdonly}" = "1" ]; then
                        vpart_options="ro,size=${vpart_size}m,mode=0755"
                    else
                        vpart_options="size=${vpart_size}m,mode=0755"
                    fi
                    vpart_dumpfreq=0
                    vpart_fsckpass=0

                    if [ ! -z "${vpart_mount}" ]; then
                        echo "tmpfs	       ${vpart_mount}	${vpart_fstype}	${vpart_options}	${vpart_dumpfreq} ${vpart_fsckpass}" >> ./${FSTAB_FILE}
                    else
                        vecho -1 "-- No fstab entry for ${vpart}"
                    fi
                done

                # NOTE: if you change any lines that go into fstab think
                # about if, in firstboot.sh, you need to make any
                # changes if your new image is installed by older
                # system's writeimage (without your change).

                if [ -f ./${FSTAB_LINES_BOTTOM_FILE} ]; then
                    cat ./${FSTAB_LINES_BOTTOM_FILE} >> ./${FSTAB_FILE}
                else
                    cat >> ./${FSTAB_FILE} <<EOF
tmpfs           /dev/shm        tmpfs   defaults        0 0
devpts          /dev/pts        devpts  gid=5,mode=620  0 0
sysfs           /sys            sysfs   defaults        0 0
proc            /proc           proc    defaults        0 0
/dev/cdrom      /mnt/cdrom      iso9660 noauto,ro       0 0
/dev/fd0        /mnt/floppy     auto    noauto          0 0
EOF
                fi

                if [ "$HAVE_WRITEIMAGE_GRAFT_3" = "y" ]; then
                    writeimage_graft_3
                fi


                chmod 0644 ./${FSTAB_FILE}
                ;;

        esac

        cd /

        if [ ${TRACK_PROGRESS} -eq 1 ]; then
            currpct=$((${currpct} + ${pctperstep}))
            ${MDREQ} action /progress/actions/update_step \
                oper_id string ${PROGRESS_OPER_ID} \
                percent_done float32 ${currpct} \
                step_status string "Synching ${location} filesystem"
        fi

        if [ -z "${already_mounted}" ]; then
            if [ ${TRACK_PROGRESS} -eq 1 ]; then
                currrangehi=$((${currpct} + ${pctperstep} - 1))
                ${PROGRESS} -i ${PROGRESS_OPER_ID} \
                    -l ${currpct} -h ${currrangehi} -- ${SYNC}
            else
                ${SYNC}
            fi
            umount ${mount_point}
            UNMOUNT_EXTRACTFS=
        fi

        # Must unmount the backing partition for the virtual partition
        if [ ! -z "${curr_vpart}" ]; then
            if [ ${DO_MANUFACTURE} -eq 1 ]; then
                umount ${phys_part_mount}
            else
                mount -o remount,ro ${phys_part_mount}
            fi
            UNMOUNT_VPART_PHYS=
        fi

    done

done

# Handle the creation and storing of the initrd
# initrds are stored in the associated boot partitions


# XXX-mtd: vpart on MTD not supported.  initrd on MTD not supported

for root_loc in ${NEED_INITRD}; do
    eval 'root_vpart="${IL_LO_'${IL_LAYOUT}'_LOC_'${root_loc}'_VPART}"'

    eval 'root_part_name="${IL_LO_'${IL_LAYOUT}'_VPART_'${root_vpart}'_PART_NAME}"'
    eval 'root_vp_file_name="${IL_LO_'${IL_LAYOUT}'_VPART_'${root_vpart}'_FILE_NAME}"'
    eval 'root_fssize="${IL_LO_'${IL_LAYOUT}'_VPART_'${root_vpart}'_FSSIZE}"'
    eval 'root_fstype="${IL_LO_'${IL_LAYOUT}'_VPART_'${root_vpart}'_FSTYPE}"'
    eval 'root_type="${IL_LO_'${IL_LAYOUT}'_VPART_'${root_vpart}'_ROOT_TYPE}"'
    eval 'root_dev="${IL_LO_'${IL_LAYOUT}'_PART_'${root_part_name}'_DEV}"'

    vlecho 0 "== Creating initrd for ${root_vpart} on device ${root_dev} with ${root_vp_file_name}"

    if [ -z "${root_type}" ]; then
        root_type="loop"
    fi

    do_make_initrd ${root_vpart} 1 ${root_dev} ${root_type} ${root_vp_file_name} ${root_fssize} ${root_fstype}

    if [ "${root_loc}" = "ROOT_1" ]; then
        eval 'NAME_BOOT_1="${IL_LO_'${IL_LAYOUT}'_LOC_BOOT_1_PART}"'
        eval 'PART_BOOT_1="${IL_LO_'${IL_LAYOUT}'_PART_'${NAME_BOOT_1}'_DEV}"'
        eval 'DIR_BOOT_1="${IL_LO_'${IL_LAYOUT}'_LOC_BOOT_1_DIR}"'

        if [ -z "${PART_BOOT_1}" ]; then
            vlechoerr -1 "*** Error: could not locate BOOT_1 partition!"
            cleanup_exit "Internal error: invalid image settings"
        fi
        boot_1_mount=${TMP_MNT_IMAGE}/BOOT_1
        boot_1_dir=${boot_1_mount}/${DIR_BOOT_1}
        mkdir -p ${boot_1_mount}
        mount ${PART_BOOT_1} ${boot_1_mount}

        cp -p ${WORKSPACE_DIR}/initrd_${root_vpart} ${boot_1_mount}/${DIR_BOOT_1}initrd
        ${SYNC}
        umount ${boot_1_mount}
    fi

    if [ "${root_loc}" = "ROOT_2" ]; then
        eval 'NAME_BOOT_2="${IL_LO_'${IL_LAYOUT}'_LOC_BOOT_2_PART}"'
        eval 'PART_BOOT_2="${IL_LO_'${IL_LAYOUT}'_PART_'${NAME_BOOT_2}'_DEV}"'
        eval 'DIR_BOOT_2="${IL_LO_'${IL_LAYOUT}'_LOC_BOOT_2_DIR}"'

        if [ -z "${PART_BOOT_2}" ]; then
            vlechoerr -1 "*** Error: could not locate BOOT_2 partition!"
            cleanup_exit "Internal error: invalid image settings"
        fi
        boot_2_mount=${TMP_MNT_IMAGE}/BOOT_2
        boot_2_dir=${boot_2_mount}/${DIR_BOOT_2}
        mkdir -p ${boot_2_mount}
        mount ${PART_BOOT_2} ${boot_2_mount}

        cp -p ${WORKSPACE_DIR}/initrd_${root_vpart} ${boot_2_mount}/${DIR_BOOT_2}initrd
        ${SYNC}
        umount ${boot_2_mount}
    fi

done

# Fixup the bootmgr configuration, for example grub.conf for the grub
vlecho 0 "== Updating bootmgr settings"
if [ ${DO_MANUFACTURE} -eq 1 ]; then
    HWNAME_AISET=
    if [ ! -z "${IL_HWNAME}" ]; then
        HWNAME_AISET="-w ${IL_HWNAME}"
    fi
    ${AISET_PATH} -m ${TARGET_DEV_ARGS} -L ${IL_LAYOUT} ${HWNAME_AISET} -l ${NEXT_BOOT_LOC:-1}
else
    if [ ! -z "${PREV_LOCATION_STATE}" ]; then
        if [ "${PREV_LOCATION_STATE}" != "0" ]; then
            ${AISET_PATH} -i -I "${INSTALL_IMAGE_ID}" -s "${PREV_LOCATION_STATE}"
        else
            # The location we just installed into was marked failed before
            if [ -z "${PREV_OTHER_LOCATION_STATE}" -o "${PREV_OTHER_LOCATION_STATE}" = "1" ]; then
                ${AISET_PATH} -i -I "${INSTALL_IMAGE_ID}" -s 2
            else
                ${AISET_PATH} -i -I "${INSTALL_IMAGE_ID}" -s 1
            fi
        fi
    else
        ${AISET_PATH} -i
    fi
fi


# ==================================================
# Cleanup: delete temp files and potentially unmount
# ==================================================

cleanup


# ==================================================
# Switch boot location (if requested)
# ==================================================

if [ ${DO_MANUFACTURE} -eq 0 -a ${SWITCH_BOOT_LOC} -eq 1 ]; then
    if [ ${TRACK_PROGRESS} -eq 1 ]; then
        ${MDREQ} -q action /progress/actions/begin_step \
            oper_id string ${PROGRESS_OPER_ID} \
            quant bool true \
            step_number uint32 5
    fi
    ${AISET_PATH} -i -l ${NEXT_BOOT_LOC}
fi


END_TIME=$(date "+%Y%m%d-%H%M%S")
vlecho 0 "====== Ending image install at ${END_TIME}"

exit 0
