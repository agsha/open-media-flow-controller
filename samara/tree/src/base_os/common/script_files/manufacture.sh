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

PATH=/usr/bin:/bin:/usr/sbin:/sbin
export PATH

umask 0022

ARGV_0=$0
ARGV_STR="$*"

if [ -f /etc/build_version.sh ]; then
    . /etc/build_version.sh
fi

usage()
{
    echo "Usage: $0 [-a] [-m MODEL] [-u URL] [-f FILE] "
    echo "       [-L LAYOUT_TYPE] [-d /DEV/N1] [-p PARTNAME -s SIZE]"
    echo "       [-O LOCNAME] [-B] [-t] [-T LOCNAME] "
    echo "       [-k KERNEL_TYPE] [-w HWNAME] [-W] [-S] [-l LCD_TYPE] [-h HOSTID]"
    echo "       [-P] [-R] [-i] [-e] [-E ARG_STRING] [-I] [-r] [-z] [-y]"
    echo "       [-F LOGFILE] [-v]"

    # Graft point to print additional options
    if [ "$HAVE_MANUFACTURE_GRAFT_3" = "y" ]; then
        manufacture_graft_3
    fi
    if [ "$HAVE_OPT_MANUFACTURE_GRAFT_3" = "y" ]; then
        opt_manufacture_graft_3
    fi

    echo ""
    echo "-a (auto): do not prompt for configuration and layout choices; "
    echo "           take defaults from model definition"
    echo ""
    echo "-m MODEL: system to manufacture (i.e. echo100)"
    echo ""
    echo "-u: specify the URL from which to download the image to install"
    echo ""
    echo "-f: specify a path in the local filesystem to the image to install"
    echo "    (alternative to -u)"
    echo ""
    echo "-L LAYOUT_TYPE: STD or other defined layout"
    echo ""
    echo "-d: device(s) to manufacture image on (i.e. /dev/hda)"
    echo ""
    echo "-p PARTNAME -s SIZE: partition name and size to use"
    echo "   if desired to be different from ones defined for layout"
    echo ""
    echo "-O LOCNAME: exclude location (like BOOT_2, or --all- for all)"
    echo "-O +LOCNAME: include (normally excluded) location (like +VAR_1)"
    echo ""
    echo "-B: force install of bootmgr"
    echo ""
    echo "-t: don't use tmpfs for a working area during manufacture"
    echo "    will attempt to use space in /var"
    echo ""
    echo "-T LOCNAME: working area for mfg is on LOCNAME instead of VAR_1 ."
    echo "   This option is not supported with VPART-based layouts"
    echo ""
    echo "-k KERNEL_TYPE: uni or smp"
    echo ""
    echo "-w HWNAME: customer-specific hardware identification string"
    echo ""
    echo "-W: override hardware compatiability check"
    echo ""
    echo "-S: will disable Smartd from the system, overriding"
    echo "   any model configuration parameters"
    echo ""
    echo "-l LCD_TYPE: if LCD is present which to use, or none"
    echo ""
    echo "-h HOSTID: set normally random hostid to specified value"
    echo ""
    echo "-P: only do post-imaging steps: do not call writeimage"
    echo ""
    echo "-R: re-answer question automatically (if possible) from previous"
    echo "    manufacture.  Currently only uses model, layout, and hostid"
    echo ""
    echo "-i: disable image verify"
    echo ""
    echo "-e: take extra mfg options from kernel command line.  This is off"
    echo "    by default.  This can be specified in the mfg environment, or"
    echo "    in a running system."
    echo ""
    echo "-E ARG_STRING: take extra mfg options from the given argument"
    echo "    string.  This is in the same format as for kernel command"
    echo "    line."
    echo ""
    echo "-I: Ignore any image signature"
    echo ""
    echo "-r: Require image signature"
    echo ""
    echo "-F LOGFILE: also log some verbose output to the specified file"
    echo ""
    echo "-v: verbose mode.  Specify multiple times for more output"
    echo ""
    echo "-b NUM: switch next boot partition to specified number {1,2}.
    echo "        The default is 1.
    echo ""
    echo "-z : fail imaging if disk is not empty"
    echo ""
    echo "-y : zero start of target media on failure"
    echo ""
    echo "Exactly one of '-u' (url) or '-f' (file) must be specified"
    echo "for the image file to use for the manufacture"

    # Graft point to print explanations of additional options
    if [ "$HAVE_MANUFACTURE_GRAFT_3b" = "y" ]; then
        manufacture_graft_3b
    fi
    if [ "$HAVE_OPT_MANUFACTURE_GRAFT_3b" = "y" ]; then
        opt_manufacture_graft_3b
    fi

    exit 1
}

# ==================================================
# Cleanup when we are finished or interrupted
# ==================================================
cleanup()
{
    # Get out of any dirs we might remove
    cd /

    sync

}

# ==================================================
# Cleanup on fatal error
# ==================================================
cleanup_exit()
{
    cleanup

    END_TIME=$(date "+%Y%m%d-%H%M%S")
    vlecho 0 "====== Ending image manufacture with error at ${END_TIME}"

    exit 1
}

# ==================================================
# Cleanup when called from 'trap' for ^C or signal
# ==================================================
trap_cleanup_exit()
{
    vlechoerr -1 "*** Manufacture interrupted"
    cleanup

    END_TIME=$(date "+%Y%m%d-%H%M%S")
    vlecho 0 "====== Ending image manufacture by trap at ${END_TIME}"

    exit 1
}

# ==================================================
# Based on verboseness setting suppress command output
# ==================================================
do_verbose()
{
    if [ ${OPT_VERBOSE} -gt 0 ]; then
        $*
    else
        $* > /dev/null 2>&1
    fi
}

# ==================================================
# Echo or not based on OPT_VERBOSE setting
# ==================================================
vecho()
{
    level=$1
    shift

    if [ ${OPT_VERBOSE} -gt ${level} ]; then
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

    if [ ${OPT_VERBOSE:-0} -gt ${level} ]; then
        echo "$*"
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
# Get the first unused wildcard instance in a config db,
# starting from a specified number, assuming that instances
# are unsigned integers.  Assign parameters to these variables:
#
#   GFU_DB_PATH: full path to database to query with mddbreq
#   GFU_PARENT_NODE: name of node under which to iterate to get
#                    indices to search
#   GFU_START_INDEX: index from which to start looking.
#                    Usually either "0" or "1".
#
# The result will be placed in GFU_RESULT.
# ==================================================
get_first_unused_idx()
{
    mddbreq_invoc="${MDDBREQ} -v ${GFU_DB_PATH} query iterate - ${GFU_PARENT_NODE}"
    ilist=`${mddbreq_invoc}`
    idx=${GFU_START_INDEX}
    while true ; do
        found=0
        for idx_used in ${ilist} ; do
            if [ "${idx_used}" = "${idx}" ]; then
                found=1
                break
            fi
        done
        if [ "${found}" = "0" ]; then
            break
        fi
        idx=$((${idx}+1))
    done
    GFU_RESULT=${idx}
}


# ==================================================
# Print a prompt and get the response.
# Before calling this, set these variables:
#
#    PROMPT_ANSWERED_ALREADY: 0 if an answer to this question has not yet
#                             been provided in the command-line options,
#                             and 1 if it has.
#
#    PROMPT_PRE_ANSWER: the answer provided in the command-line options,
#                       if any
#
#    PROMPT_HEADING: the string to be printed, outlined in hyphens, 
#                    before this prompt.  If empty, no heading is
#                    printed, allowing several options to show up in
#                    one logical block.
#
#    PROMPT_DEFAULT: to either "yes" or "no" to specify what the answer
#                    is if the user just hits Enter.  (XXX/EMT: should
#                    allow this to be omitted, requiring an explicit answer)
#
#    PROMPT_MODEL_DEFAULT: a string that will be used to construct the
#                     variable name into which model definitions can put
#                     overrides of the default value.  The variable name
#                     will be MODEL_${CFG_MODEL}_${PROMPT_MODEL_DEFAULT}.
#                     If PROMPT_MODEL_DEFAULT is not set, or if the 
#                     variable name constructed from it is not set, then
#                     the value specified in PROMPT_DEFAULT will be used.
#
#    PROMPT_STRING: the string to user to prompt the user for this answer.
#                   This is reprinted after every invalid answer, whereas
#                   the heading is not.
#
#    PROMPT_YES_NO: a "1" here means to only permit "yes" or "no" answers,
#                   and to convert a "yes" answer to "true", and "no" to
#                   "false".  Any other value, or if this is not set, 
#                   means that no validation or remapping is done.
#
#                   XXX/EMT: should allow a string to specify arbitrary
#                   set of options for validation, e.g. for model names.
#
#    PROMPT_VALUE_DESCR: how we should describe the value being set
#                        when we print the answer we get.  If empty,
#                        PROMPT_STRING is used.
#
# When this returns, PROMPT_RESPONSE will contain the choice made by 
# the user.
#
# XXX/EMT: should convert other prompts in this file to use this.
# ==================================================
prompt_get_answer()
{
    # We want to print this even if they answered it already, so the
    # user doesn't get disoriented.  But we must skip it if we're in
    # "auto" mode, which is supposed to be less verbose.
    if [ ${OPT_AUTO} -eq 0 ]; then
        if [ "x${PROMPT_HEADING}" != "x" ]; then
            echo ""
            echo "---------------------"
            echo "${PROMPT_HEADING}"
            echo "---------------------"
        fi
        echo ""
    fi

    if [ ${PROMPT_ANSWERED_ALREADY} -eq 0 ]; then
        # Get the model's default for this setting, if any
        if [ "x${PROMPT_MODEL_DEFAULT}" != "x" ]; then
            eval 'MODEL_DEFAULT="${MODEL_'${CFG_MODEL}'_'${PROMPT_MODEL_DEFAULT}'}"'
            if [ "x${MODEL_DEFAULT}" != "x" ]; then
                PROMPT_DEFAULT=${MODEL_DEFAULT}
            fi
        fi

        PROMPT_RESPONSE=${PROMPT_DEFAULT}
        if [ ${OPT_AUTO} -eq 0 ]; then
            PROMPT_RESPONSE=""
            while true ; do
                echo -n "${PROMPT_STRING}"
                if [ "x${PROMPT_DEFAULT}" != "x" ]; then
                    echo -n " [${PROMPT_DEFAULT}]"
                fi
                echo -n ": "
                ${READ_CMD} PROMPT_RESPONSE
                if [ "x${PROMPT_RESPONSE}" = "x" ]; then
                    PROMPT_RESPONSE=${PROMPT_DEFAULT}
                    break
                fi
                if [ ${PROMPT_YES_NO} -eq 1 ]; then
                    if [ "${PROMPT_RESPONSE}" = "yes" ]; then
                        break
                    elif [ "${PROMPT_RESPONSE}" = "no" ]; then
                        break
                    else
                        echo "Please answer 'yes' or 'no', or Enter to accept default"
                    fi
                else
                    break
                fi
            done
            echo ""
        fi
    else
        PROMPT_RESPONSE=${PROMPT_PRE_ANSWER}
    fi

    if [ "x${PROMPT_VALUE_DESCR}" = "x" ]; then
        PROMPT_VALUE_DESCR=${PROMPT_STRING}
    fi
    if [ "x${PROMPT_RESPONSE}" = "x" ]; then
        PROMPT_RESPONSE_DISPLAY="(none)"
    else
        PROMPT_RESPONSE_DISPLAY=${PROMPT_RESPONSE}
    fi
    vecho 0 "== ${PROMPT_VALUE_DESCR}: ${PROMPT_RESPONSE_DISPLAY}"

    if [ ${PROMPT_YES_NO} -eq 1 ]; then
        if [ "${PROMPT_RESPONSE}" = "yes" ]; then
            PROMPT_RESPONSE=true
        else
            PROMPT_RESPONSE=false
        fi
    fi
}


# ==================================================
# Print a prompt and get a multi-line response.
#
# We will keep getting lines until a specified termination string
# is entered on a line by itself (either "", i.e. a completely blank
# line, or "." are recommended by convention).
#
# The prompt will be amended to include the information about
# how to terminate the answer.
#
# Before calling this, set these variables:
#
#    PROMPT_STRING: the string to user to prompt the user for this answer.
#                   This is reprinted after every invalid answer, whereas
#                   the heading is not.
#
#    PROMPT_TERM_STRING: the string the user must enter on a line by itself
#                        to terminate the reply.  This may be the empty
#                        string if desired.
#
# When this returns, PROMPT_RESPONSE will contain the string entered by
# the user.  It will be the concatenation of all lines entered, with
# each pair of lines separated by a newline character.
# ==================================================
prompt_get_multiline_answer()
{
    PROMPT_RESPONSE=""
    echo ""
    echo "${PROMPT_STRING}"
    if [ "x${PROMPT_TERM_STRING}" = "x" ]; then
        echo "Enter a blank line to end your reply."
    else
        echo "Enter '${PROMPT_TERM_STRING}' on a line by itself to end your reply."
    fi

    while true ; do
        # We'd like to include a courtesy "> " at the beginning of
        # each line, but are not doing so because when pasting several
        # lines into the serial console, these prompts tend to get
        # mixed up with the lines being pasted.
        # echo -n "> "
        ${READ_CMD} PROMPT_RESPONSE_LINE
        if [ "x${PROMPT_RESPONSE_LINE}" = "x${PROMPT_TERM_STRING}" ]; then
            break
        fi
        if [ "x${PROMPT_RESPONSE}" = "x" ]; then
            PROMPT_RESPONSE="${PROMPT_RESPONSE_LINE}"
        else
            PROMPT_RESPONSE="`printf "%s\n%s" "${PROMPT_RESPONSE}" "${PROMPT_RESPONSE_LINE}"`"
        fi
    done
}


# ==================================================


do_mount_vpart()
{
    vpart=$1
    tmount_dir=$2

    eval 'bkpart_name="${IL_LO_'${IL_LAYOUT}'_VPART_'${vpart}'_PART_NAME}"'
    eval 'vp_file_name="${IL_LO_'${IL_LAYOUT}'_VPART_'${vpart}'_FILE_NAME}"'
    eval 'fstype="${IL_LO_'${IL_LAYOUT}'_VPART_'${vpart}'_FSTYPE}"'

    if [ -z "${bkpart_name}" -o -z "${vp_file_name}" ]; then
        vlechoerr -1 "*** Configuration error for virtual ${vpart}"
        continue
    fi

    vecho 2 "vpart=${vpart} backing part=${bkpart_name} vp_file_name=${vp_file_name}"

    eval 'part_mount="${IL_LO_'${IL_LAYOUT}'_PART_'${bkpart_name}'_MOUNT}"'

    phys_part_mount=${MFG_VMOUNT}/${part_mount}

    # Assume the backing partition has already been mounted

    vp_path=${phys_part_mount}/${vp_file_name}

    vecho 1 "-- Mounting virtual fs image in ${vp_path} for ${vpart}"

    mkdir -p ${tmount_dir}

    FAILURE=0
    mount -o loop,noatime -t ${fstype} ${vp_path} ${tmount_dir} || FAILURE=1
    if [ ${FAILURE} -eq 1 ]; then
        vlechoerr -1 "*** Could not mount partition ${vp_path} on ${tmount_dir}"
        cleanup_exit
    fi
    UNMOUNT_LIST="${tmount_dir} ${UNMOUNT_LIST}"
}

# returns OK if $1 contains $2
strstr() {
    case "${1}" in 
        *${2}*) return 0;;
    esac
    return 1
}

# ==================================================
# Parse command line arguments, setup
# ==================================================

LOG_COMPONENT="manufacture"
LOG_TAG="${LOG_COMPONENT}[$$]"
# LOG_LEVEL is uppercase, facility lower
LOG_FACILITY=user
LOG_LEVEL=NOTICE
LOG_OUTPUT=0
LOG_FILE=

# See if we're in a running image or a bootfloppy
kern_cmdline="`cat /proc/cmdline`"

if strstr "$kern_cmdline" img_id= ; then
    IS_BOOTFLOP=0
else
    IS_BOOTFLOP=1
fi

# NOTE: This REMANUFACTURE environment variable must be set by the caller,
# normally remanufacture.sh .

if [ "x${REMANUFACTURE}" = "x1" ]; then
    IS_REMANUFACTURE=1
else
    IS_REMANUFACTURE=0
fi

# Choose if we use 'read -e' (readline)
if [ ! -z "${BASH}" ]; then
    READ_CMD="read -e"
else
    READ_CMD="read"
fi

# NOTE: if you change MFG_MOUNT or others below be sure to check other
# scripts for dependencies.

DO_MOUNT=1
if [ ${IS_BOOTFLOP} -eq 1 -o ${IS_REMANUFACTURE} -eq 1 ]; then
    MFG_MOUNT=/tmp/mfg_mount
    MFG_VMOUNT=/tmp/mfg_vmount
else
    DO_MOUNT=0
    MFG_MOUNT=
    MFG_VMOUNT=
fi
MFG_DB_DIR=${MFG_MOUNT}/config/mfg
MFG_DB_PATH=${MFG_DB_DIR}/mfdb
MFG_INC_DB_PATH=${MFG_DB_DIR}/mfincdb

WRITEIMAGE=/sbin/writeimage.sh
MDDBREQ=/opt/tms/bin/mddbreq
IMGVERIFY=/sbin/imgverify.sh
CRYPT=/opt/tms/bin/crypt

MFG_POST_COPY_DEPRECATED="\
"

IL_PATH=/etc/layout_settings.sh


# XXXXXXXX  testing
# MDDBREQ=./mddbreq
# MFG_DB_DIR=./mfg
# MFG_DB_PATH=${MFG_DB_DIR}/mfdb
# MFG_INC_DB_PATH=${MFG_DB_DIR}/mfincdb
# IL_PATH=./layout_settings.sh

HAVE_OPT_AUTO=0
OPT_AUTO=0
HAVE_OPT_MODEL=0
OPT_MODEL=
HAVE_OPT_KERNEL_TYPE=0
OPT_KERNEL_TYPE=
HAVE_OPT_HWNAME=0
OPT_HWNAME=
HAVE_OPT_HWNAME_COMPAT_IGNORE=0
OPT_HWNAME_COMPAT_IGNORE=0
HAVE_OPT_LCD_TYPE=0
OPT_LCD_TYPE=
HAVE_OPT_LAYOUT=0
OPT_LAYOUT=
HAVE_OPT_PART_NAME_SIZE_LIST=0
OPT_PART_NAME_SIZE_LIST=
HAVE_OPT_VPART_NAME_SIZE_LIST=0
OPT_VPART_NAME_SIZE_LIST=
HAVE_OPT_DEV_LIST=0
OPT_DEV_LIST=
HAVE_OPT_IF_LIST=0
OPT_IF_LIST=
HAVE_OPT_IF_NAMING=0
OPT_IF_NAMING=
HAVE_OPT_VERBOSE=0
OPT_VERBOSE=1
HAVE_OPT_SMARTD=0
OPT_SMARTD=1
OPT_IMAGE_VERIFY=1
HAVE_OPT_HOSTID=0
OPT_HOSTID=
HAVE_OPT_POSTEXEC=0
OPT_POSTEXEC=
OPT_POSTIMAGE_ONLY=0
OPT_REANSWER_AUTO=0
OPT_EXTRA_OPTS=0
HAVE_OPT_EXTRA_OPTS_STR=0
OPT_EXTRA_OPTS_STR=
OPT_SIG_IGNORE=0
OPT_SIG_REQUIRE=0
OPT_CHANGE_NEXTBOOT_LOC=0

WRITEIMAGE_ARGS=
VERBOSE_ARGS=

# Define customer-specific graft functions
if [ -f /etc/customer_rootflop.sh ]; then
    . /etc/customer_rootflop.sh
fi

# Define optional feature-specific graft functions
if [ -f /sbin/opt_feature_rootflop.sh ]; then
    . /sbin/opt_feature_rootflop.sh
fi

GETOPT_OPTS='am:u:f:k:d:p:s:l:O:BL:tT:ih:x:vw:WF:qSPReE:Irb:zy'
GETOPT_LONGOPTS=''

# Graft point to add extra options for getopt.
# Also, initialize your options and their respective "HAVE_..." variables.
if [ "$HAVE_MANUFACTURE_GRAFT_4" = "y" ]; then
    manufacture_graft_4
fi
if [ "$HAVE_OPT_MANUFACTURE_GRAFT_4" = "y" ]; then
    opt_manufacture_graft_4
fi

if [ "x$GETOPT_LONGOPTS" = "x" ]; then
    PARSE=`/usr/bin/getopt -s sh $GETOPT_OPTS "$@"`
else
    PARSE=`/usr/bin/getopt -s sh -l ${GETOPT_LONGOPTS} $GETOPT_OPTS "$@"`
fi

if [ $? != 0 ] ; then
    vlechoerr -1 "Error from getopt"
    usage
fi

eval set -- "$PARSE"

# Defaults
SYSIMAGE_USE_URL=-1
SYSIMAGE_FILE=
SYSIMAGE_URL=
USE_TMPFS=1
IF_CHECK_PHYSICAL=1
REQUIRE_EMPTY_TARGETS=0
MARK_TARGETS_EMPTY_ON_FAILURE=0

while true ; do
    case "$1" in
        -a) HAVE_OPT_AUTO=1; OPT_AUTO=1; shift ;; 
        -k) HAVE_OPT_KERNEL_TYPE=1; OPT_KERNEL_TYPE=$2; shift 2 ;;
        -w) HAVE_OPT_HWNAME=1; OPT_HWNAME=$2; shift 2 ;;
        -W) HAVE_OPT_HWNAME_COMPAT_IGNORE=1; OPT_HWNAME_COMPAT_IGNORE=1; shift ;;
        -l) HAVE_OPT_LCD_TYPE=1; OPT_LCD_TYPE=$2; shift 2 ;;
        -v) HAVE_OPT_VERBOSE=1; OPT_VERBOSE=$((${OPT_VERBOSE}+1));
            WRITEIMAGE_ARGS="${WRITEIMAGE_ARGS} $1";  
            VERBOSE_ARGS="${VERBOSE_ARGS} -v"; shift ;;
        -F) LOG_FILE=$2
            LOG_OUTPUT=1
            WRITEIMAGE_ARGS="${WRITEIMAGE_ARGS} $1 $2"
            shift 2 ;;
        -q) HAVE_OPT_VERBOSE=1; OPT_VERBOSE=-1; shift ;;
        -L) HAVE_OPT_LAYOUT=1; OPT_LAYOUT=$2; shift 2 ;;
        -m) HAVE_OPT_MODEL=1; OPT_MODEL=$2; shift 2 ;;
        -d) HAVE_OPT_DEV_LIST=1
            new_disk=$2; shift 2
            echo $new_disk | grep -q "^/dev"
            if [ $? -eq 1 ]; then
                vlechoerr -1 "Problem with -d option"
                usage
            fi
            OPT_DEV_LIST="${OPT_DEV_LIST} ${new_disk}"
            ;;
        -p) LAST_PART=$2; shift 2 ;;
        -s) LAST_PART_SIZE=$2; shift 2 
            if [ -z "${LAST_PART}" ]; then
                vlechoerr -1 "-s option specified without -p"
                usage
            fi
            HAVE_OPT_PART_NAME_SIZE_LIST=1
            OPT_PART_NAME_SIZE_LIST="${OPT_PART_NAME_SIZE_LIST} ${LAST_PART} ${LAST_PART_SIZE}"
            ;;
        -u) SYSIMAGE_USE_URL=1
            SYSIMAGE_URL=$2
            WRITEIMAGE_ARGS="${WRITEIMAGE_ARGS} $1 $2"; shift 2 ;;
        -f) SYSIMAGE_USE_URL=0
            SYSIMAGE_FILE=$2
            WRITEIMAGE_ARGS="${WRITEIMAGE_ARGS} $1 $2"; shift 2 ;;
        -t) USE_TMPFS=0
            WRITEIMAGE_ARGS="${WRITEIMAGE_ARGS} $1"; shift ;;
        -T) WRITEIMAGE_ARGS="${WRITEIMAGE_ARGS} $1 $2"; shift 2 ;;
        -S) HAVE_OPT_SMARTD=1; OPT_SMARTD=0; shift ;;
        -i) OPT_IMAGE_VERIFY=0; shift ;;
        -h) HAVE_OPT_HOSTID=1; OPT_HOSTID="$2"; shift 2 ;;
        -P) OPT_POSTIMAGE_ONLY=1; shift ;;
        -R) OPT_REANSWER_AUTO=1; shift ;; 
        -x) HAVE_OPT_POSTEXEC=1; OPT_POSTEXEC=$2; shift 2 ;;
        -e) OPT_EXTRA_OPTS=1; shift ;;
        -E) OPT_EXTRA_OPTS=1; 
            HAVE_OPT_EXTRA_OPTS_STR=1; OPT_EXTRA_OPTS_STR=$2; shift 2 ;;
        -O) WRITEIMAGE_ARGS="${WRITEIMAGE_ARGS} $1 $2"; shift 2 ;;
        -B) WRITEIMAGE_ARGS="${WRITEIMAGE_ARGS} $1"; shift ;;
        -I) OPT_SIG_IGNORE=1
            WRITEIMAGE_ARGS="${WRITEIMAGE_ARGS} $1"; shift ;;
        -r) OPT_SIG_REQUIRE=1
            WRITEIMAGE_ARGS="${WRITEIMAGE_ARGS} $1"; shift ;;
        -b) OPT_CHANGE_NEXTBOOT_LOC=1
            NEXTBOOT_LOC=$2
            WRITEIMAGE_ARGS="${WRITEIMAGE_ARGS} $1 $2"
            shift 2 ;;
        -z) REQUIRE_EMPTY_TARGETS=1
            WRITEIMAGE_ARGS="${WRITEIMAGE_ARGS} $1"; shift ;;
        -y) MARK_TARGETS_EMPTY_ON_FAILURE=1
            WRITEIMAGE_ARGS="${WRITEIMAGE_ARGS} $1"; shift ;;
        --) shift ; break ;;

        # Don't complain; there could be customer-specific options
        # in the list.  Getopt should catch it if any truly invalid
        # options were specified.
        *) shift ;;
    esac
done

# We don't do our previous test here:
#      if [ ! -z "$*" ] ; then
#          usage
#      fi
# because there could be customer- or product-specific options in
# the list, which will be caught below.  Getopt should catch any
# invalid options.


#
# Use extra kernel params, if desired.  Allow specified options to override
# the extra params.  This allows for more automated installs via PXE labels.
#
# In general, the kernel command line params are:
#    xopt_mfg_MFG-FLAG[=MFG.VALUE]
#
# Not all mfg flags are supported, just those that would likely to be
# used for an automated manufacture.  See S34automfg for one way these
# options might be specified in an automated manufacture environment.
#
# An example:
#
# xopt_mfg_a
# xopt_mfg_t
# xopt_mfg_m=echo101
# xopt_mfg_u=http://server/latest/image.img
# xopt_mfg_z
#
#
# The full list of supported 'xopt_mfg_...' settings is:
#
# xopt_mfg_a         : enable auto manufacture (like "-a")
#
# xopt_mfg_v         : verbose mode ("-v")
# xopt_mfg_q         : quiet mode ("-q")
#
# xopt_mfg_m=MODEL   : model to manufacture (like "-m MODEL")
#
# xopt_mfg_u=URL     : URL to download image from (like "-u URL")
# xopt_mfg_f=FILE    : FILE to use for image (like "-f FILE")
#
# xopt_mfg_t         : do not use tmpfs for working area, use /var ("-t")
#
# xopt_mfg_z         : fail if disk not empty ("-z")
#
# xopt_mfg_y         : zero start of target media on failure ("-y")
#
# xopt_mfg_i         : disable image verify ("-i")
#
# xopt_mfg_h=HOSTID  : set normally random hostid (like "-h HOSTID")
#
# xopt_mfg_B         : force install of bootmgr ("-B")


# Note: if you want to put a space in, you'll have to use \040 or \0040 .
#
XOPTS_OPT_COUNT=0
if [ ${OPT_EXTRA_OPTS} -eq 1 ]; then

    opts_str=
    if [ ${HAVE_OPT_EXTRA_OPTS_STR} -eq 0 ]; then
        opts_str="${kern_cmdline}"
    else
        opts_str="${OPT_EXTRA_OPTS_STR}"
    fi

    #
    # First, we'll get all the things from the kernel or specified command
    # line into our XOPTS_OPTNUM_###_NAME and XOPTS_OPTNUM_###_VALUE 'arrays'.
    # We'll process them only after we have everything stored away, in case
    # options depend on others, and to make customer processing work like
    # base processing.
    #

    xopts_opt_count=0

    for kcw in ${opts_str}; do
        echo $kcw | grep -q "^xopt_mfg_."
        if [ $? -eq 1 ]; then
            continue
        fi

        opt_name=$(echo -n "$kcw" | sed 's/^xopt_mfg_\([^=]*\).*$/\1/')
        # printf %b is broken on our version of busybox, so just fixup spaces
        opt_value=$(echo -n "$kcw" | sed 's/\\0040/ /g' | sed 's/\\040/ /g' | sed 's/^xopt_mfg_[^=]*\(.*\)$/\1/' | sed 's/^=\(.*\)$/\1/')

        if [ -z "${opt_name}" ]; then
            continue
        fi

        xopts_opt_count=$((${xopts_opt_count} + 1))

        eval 'XOPTS_OPTNUM_'${xopts_opt_count}'_NAME="'${opt_name}'"'
        eval 'XOPTS_OPTNUM_'${xopts_opt_count}'_VALUE="'${opt_value}'"'
        eval 'XOPTS_OPTNAME_'${opt_name}'_VALUE="'${opt_value}'"'
    done

    XOPTS_OPT_COUNT=${xopts_opt_count}

    #
    # Now we will walk through the base extra opts 'array' and handle ones we
    # support.  Customers can add their own such things in graft point 5.  
    #

    for eon in `seq 1 ${XOPTS_OPT_COUNT}`; do
        opt_name=
        eval 'opt_name="${XOPTS_OPTNUM_'${eon}'_NAME}"'
        opt_value=
        eval 'opt_value="${XOPTS_OPTNUM_'${eon}'_VALUE}"'

        case "${opt_name}" in
            a) if [ ${HAVE_OPT_AUTO:-0} -eq 0 ]; then
                   HAVE_OPT_AUTO=1; OPT_AUTO=1
               fi
               ;;
            v) if [ ${HAVE_OPT_VERBOSE:-0} -eq 0 ]; then
                   HAVE_OPT_VERBOSE=1; OPT_VERBOSE=$((${OPT_VERBOSE}+1))
                   WRITEIMAGE_ARGS="${WRITEIMAGE_ARGS} -v"
                   VERBOSE_ARGS="${VERBOSE_ARGS} -v"
               fi
               ;;
            q) if [ ${HAVE_OPT_VERBOSE:-0} -eq 0 ]; then
                   HAVE_OPT_VERBOSE=1; OPT_VERBOSE=-1
                   VERBOSE_ARGS=""
               fi
               ;;
            m) if [ ${HAVE_OPT_MODEL:-0} -eq 0 ]; then
                   HAVE_OPT_MODEL=1; OPT_MODEL=${opt_value}
               fi
               ;;
            u) if [ ${SYSIMAGE_USE_URL:--1} -eq -1 ]; then
                   SYSIMAGE_USE_URL=1
                   SYSIMAGE_URL=${opt_value}
                   WRITEIMAGE_ARGS="${WRITEIMAGE_ARGS} -u ${opt_value}"
               fi
               ;;
            f) if [ ${SYSIMAGE_USE_URL:--1} -eq -1 ]; then
                   SYSIMAGE_USE_URL=0
                   SYSIMAGE_FILE=${opt_value}
                   WRITEIMAGE_ARGS="${WRITEIMAGE_ARGS} -f ${opt_value}"
               fi
               ;;
            t) USE_TMPFS=0
               WRITEIMAGE_ARGS="${WRITEIMAGE_ARGS} -t"
               ;;
            z) REQUIRE_EMPTY_TARGETS=1
               WRITEIMAGE_ARGS="${WRITEIMAGE_ARGS} -z"
               ;;
            y) MARK_TARGETS_EMPTY_ON_FAILURE=1
               WRITEIMAGE_ARGS="${WRITEIMAGE_ARGS} -y"
               ;;
            i) OPT_IMAGE_VERIFY=0
               ;;
            h) if [ ${HAVE_OPT_HOSTID:-0} -eq 0 ]; then
                   HAVE_OPT_HOSTID=1; OPT_HOSTID="${opt_value}"
               fi
               ;;
            B) WRITEIMAGE_ARGS="${WRITEIMAGE_ARGS} -B"
               ;;
            *) ;;
        esac
        
    done
fi

# Let the customer graft point do any XOPT processing
if [ "$HAVE_MANUFACTURE_GRAFT_4b" = "y" ]; then
    manufacture_graft_4b
fi
if [ "$HAVE_OPT_MANUFACTURE_GRAFT_4b" = "y" ]; then
    opt_manufacture_graft_4b
fi


# Try to retrieve previous run answers, if possible.  Allow specified options
# to override the previous run answer.
if [ ${IS_BOOTFLOP} -eq 0 -a ${OPT_REANSWER_AUTO} -eq 1 ]; then

    # model
    if [ ${HAVE_OPT_MODEL} -eq 0 ]; then
        FAILURE=0
        ns=`${MDDBREQ} -v ${MFG_DB_PATH} query get - /mfg/mfdb/system/model` || FAILURE=1
        if [ ${FAILURE} -eq 0 -a ! -z "${ns}" ]; then
            HAVE_OPT_MODEL=1
            OPT_MODEL=${ns}
        fi
    fi

    # hwname
    if [ ${HAVE_OPT_HWNAME} -eq 0 ]; then
        FAILURE=0
        ns=`${MDDBREQ} -v ${MFG_DB_PATH} query get - /mfg/mfdb/system/hwname` || FAILURE=1
        if [ ${FAILURE} -eq 0 -a ! -z "${ns}" ]; then
            HAVE_OPT_HWNAME=1
            OPT_HWNAME=${ns}
        fi
    fi
    
    # layout
    if [ ${HAVE_OPT_LAYOUT} -eq 0 ]; then
        FAILURE=0
        ns=`${MDDBREQ} -v ${MFG_DB_PATH} query get - /mfg/mfdb/system/layout` || FAILURE=1
        if [ ${FAILURE} -eq 0 -a ! -z "${ns}" ]; then
            HAVE_OPT_LAYOUT=1
            OPT_LAYOUT=${ns}
        fi
    fi

    # hostid
    if [ ${HAVE_OPT_HOSTID} -eq 0 ]; then
        FAILURE=0
        ns=`${MDDBREQ} -v ${MFG_DB_PATH} query get - /mfg/mfdb/system/hostid` || FAILURE=1
        if [ ${FAILURE} -eq 0 -a ! -z "${ns}" ]; then
            HAVE_OPT_HOSTID=1
            OPT_HOSTID="${ns}"
        fi
    fi

fi


# Now let the customer graft point test its own options
if [ "$HAVE_MANUFACTURE_GRAFT_5" = "y" ]; then
    manufacture_graft_5
fi
if [ "$HAVE_OPT_MANUFACTURE_GRAFT_5" = "y" ]; then
    opt_manufacture_graft_5
fi

if [ ${IS_BOOTFLOP} -eq 0 -a ${IS_REMANUFACTURE} -eq 0 -a ${OPT_POSTIMAGE_ONLY} -ne 1 ]; then
    vlechoerr -1 "*** Must specify -P if not run from the manufacturing environment"
    usage
fi

if [ ${SYSIMAGE_USE_URL} -eq -1 -a ${OPT_POSTIMAGE_ONLY} -ne 1 ]; then
    usage
fi


if [ ! -z "${MFG_POST_COPY}" ]; then
    vlechoerr -1 "*** Must not set MFG_POST_COPY .  Instead install needed components in the rootflop image"
    usage
fi

if [ ${LOG_OUTPUT} -ne 0 -a ! -z "${LOG_FILE}" -a ! -f "${LOG_FILE}" ]; then
    touch "${LOG_FILE}"
fi

trap "trap_cleanup_exit" HUP INT QUIT PIPE TERM

START_TIME=$(date "+%Y%m%d-%H%M%S")
vlecho 0 "====== Starting manufacture at ${START_TIME}"
vlecho 0 "====== Called as: ${ARGV_0} ${ARGV_STR}"

vecho 0 ""
vecho 0 "=================================================="
vecho 0 " Manufacture script starting"
vecho 0 "=================================================="
vecho 0 ""

MODEL_sd100_ENABLE=1
MODEL_sd100_DESCR="Generic appliance, Single IDE disk, STD disk layout"
MODEL_sd100_KERNEL_TYPE="uni"
MODEL_sd100_LCD_TYPE="none"
MODEL_sd100_DEV_LIST="/dev/hda"
MODEL_sd100_LAYOUT="STD"
MODEL_sd100_PART_NAME_SIZE_LIST=""
MODEL_sd100_IF_LIST="eth0 eth1"
MODEL_sd100_IF_NAMING="none"

MODEL_sd101_ENABLE=1
MODEL_sd101_DESCR="Generic appliance, Single SCSI disk, STD disk layout"
MODEL_sd101_KERNEL_TYPE="uni"
MODEL_sd101_LCD_TYPE="none"
MODEL_sd101_DEV_LIST="/dev/sda"
MODEL_sd101_LAYOUT="STD"
MODEL_sd101_PART_NAME_SIZE_LIST=""
MODEL_sd101_IF_LIST="eth0 eth1 eth2 eth3"
MODEL_sd101_IF_NAMING="none"

# Note that on x86 , the uni and smp kernels are the same, so this is
# just an example.
MODEL_sd200_ENABLE=1
MODEL_sd200_DESCR="Generic SMP appliance, Single IDE disk, STD disk layout (2G VAR 1G SWAP)"
MODEL_sd200_KERNEL_TYPE="smp"
MODEL_sd200_LCD_TYPE="none"
MODEL_sd200_DEV_LIST="/dev/hda"
MODEL_sd200_LAYOUT="STD"
MODEL_sd200_PART_NAME_SIZE_LIST="VAR 2048 SWAP 1024"
MODEL_sd200_IF_LIST="eth0 eth1 eth2 eth3"
MODEL_sd200_IF_NAMING="manual"

CFG_MODEL_DEF="sd100"
CFG_MODEL_CHOICES="sd100 sd101 sd200"
CFG_KERNEL_TYPE_DEF="uni"
CFG_KERNEL_TYPE_CHOICES="uni smp"
CFG_LCD_TYPE_DEF="none"
CFG_LCD_TYPE_CHOICES="cf631 cf633 cf635 advlcm100 none"
CFG_DEV_LIST_DEF="/dev/hda"
CFG_PART_NAME_SIZE_LIST_DEF=""
CFG_LAYOUT_DEF="STD"
CFG_LAYOUT_CHOICES="STD"
CFG_IF_LIST_CHOICES=""
CFG_IF_LIST_DEF="eth0 eth1 eth2 eth3 eth4 eth5 eth6 eth7"
CFG_IF_NAMING_CHOICES="dyn-mac-sorted dyn-ifindex-sorted mac-sorted ifindex-sorted unsorted manual none"
CFG_IF_NAMING_DEF="none"


# Graft point for models: append to/reset CFG_MODEL_CHOICES, CFG_MODEL_DEF
if [ "$HAVE_MANUFACTURE_GRAFT_1" = "y" ]; then
    manufacture_graft_1
fi
if [ "$HAVE_OPT_MANUFACTURE_GRAFT_1" = "y" ]; then
    opt_manufacture_graft_1
fi

# Remove any choices that are not enabled, so we don't tease the user
enabled_model_choices=
for model in ${CFG_MODEL_CHOICES}; do
    eval 'enabled="${MODEL_'${model}'_ENABLE}"'
    if [ "${enabled:-0}" -ne 1 ]; then
        continue
    fi
    enabled_model_choices="${enabled_model_choices} ${model}"
done
CFG_MODEL_CHOICES=${enabled_model_choices}

if [ ${HAVE_OPT_MODEL} -eq 0 ]; then
    default_response=${CFG_MODEL_DEF}
    CFG_MODEL="${default_response}"

    if [ ${OPT_AUTO} -eq 0 ]; then
        while true ; do
            echo ""
            echo "--------------------"
            echo "Model selection"
            echo "--------------------"
            echo ""
            
            response=""
            echo -n "Product model ('?' for info) (${CFG_MODEL_CHOICES}) [${default_response}]: "
            ${READ_CMD} response
            echo ""
            if [ "x${response}" = "x" ]; then
                response=${default_response}
            fi
            CFG_MODEL="${response}"
            if [ "${CFG_MODEL}" != '?' ]; then
                break
            else
                echo "Available model descriptions:"
                for model in ${CFG_MODEL_CHOICES}; do
                    eval 'descr="${MODEL_'${model}'_DESCR}"'
                    echo "    Model ${model} : ${descr:-${model}}"
                done
            fi

        done
    fi
else
    CFG_MODEL="${OPT_MODEL}"
fi
vecho 0 "== Using model: ${CFG_MODEL}"

FAILURE=0
eval 'enabled="${MODEL_'${CFG_MODEL}'_ENABLE}"' > /dev/null 2>&1 || FAILURE=1
if [ ${FAILURE} -eq 1 -o "${enabled:-0}" -ne 1 ]; then
    vlechoerr -1 "Error: unknown or disabled model: ${CFG_MODEL}"
    cleanup_exit
fi

# Now get any model-specific settings
# XXXXX impl

# XXX/EMT: would be nice if for items where there is a limited set of 
# choices (e.g. kernel type, LCD type, etc.) we caught when they type
# an invalid choice and present the question again.  Currently it looks
# like we just let it go but it'll cause failure later.

if [ ${HAVE_OPT_KERNEL_TYPE} -eq 0 ]; then
    eval 'model_kernel_type_def="${MODEL_'${CFG_MODEL}'_KERNEL_TYPE}"'
    if [ "x${model_kernel_type_def}" = "x" ]; then
        default_response="${CFG_KERNEL_TYPE_DEF}"
    else
        default_response="${model_kernel_type_def}"
    fi
    CFG_KERNEL_TYPE=${default_response}

    if [ ${OPT_AUTO} -eq 0 ]; then
        echo ""
        echo "--------------------"
        echo "Kernel type selection"
        echo "--------------------"
        echo ""
        
        response=""
        echo -n "Kernel type (${CFG_KERNEL_TYPE_CHOICES}) [${default_response}]: "
        ${READ_CMD} response
        echo ""
        if [ "x${response}" = "x" ]; then
            response=${default_response}
        fi
        CFG_KERNEL_TYPE="${response}"
    fi
else
    CFG_KERNEL_TYPE="${OPT_KERNEL_TYPE}"
fi
vecho 0 "== Using kernel type: ${CFG_KERNEL_TYPE}"


# XXXX-ppc right now, we're only prompting for the hwname on PowerPC
if [ "x$BUILD_TARGET_ARCH" = "xPPC" ]; then
    if [ ${HAVE_OPT_HWNAME} -eq 0 ]; then
        eval 'model_hwname_def="${MODEL_'${CFG_MODEL}'_HWNAME}"'
        if [ "x${model_hwname_def}" = "x" ]; then
            default_response="${CFG_HWNAME_DEF}"
        else
            default_response="${model_hwname_def}"
        fi
        CFG_HWNAME=${default_response}

        if [ ${OPT_AUTO} -eq 0 ]; then
            echo ""
            echo "--------------------"
            echo "Hardware name selection"
            echo "--------------------"
            echo ""
            
            response=""
            echo -n "Hardware name (${CFG_HWNAME_CHOICES}) [${default_response}]: "
            ${READ_CMD} response
            echo ""
            if [ "x${response}" = "x" ]; then
                response=${default_response}
            fi
            CFG_HWNAME="${response}"
        fi
    else
        CFG_HWNAME="${OPT_HWNAME}"
    fi
    if [ ! -z "${CFG_HWNAME}" ]; then
        vecho 0 "== Using hardware name: ${CFG_HWNAME}"
    fi
fi


if [ "x$BUILD_PROD_FEATURE_FRONT_PANEL" = "x1" ]; then
    if [ ${HAVE_OPT_LCD_TYPE} -eq 0 ]; then
        eval 'model_lcd_type_def="${MODEL_'${CFG_MODEL}'_LCD_TYPE}"'
        if [ "x${model_lcd_type_def}" = "x" ]; then
            default_response="${CFG_LCD_TYPE_DEF}"
        else
            default_response="${model_lcd_type_def}"
        fi
        CFG_LCD_TYPE=${default_response}

        if [ ${OPT_AUTO} -eq 0 ]; then
            echo ""
            echo "--------------------"
            echo "LCD type selection"
            echo "--------------------"
            echo ""
        
            response=""
            echo -n "LCD type (${CFG_LCD_TYPE_CHOICES}) [${default_response}]: "
            ${READ_CMD} response
            echo ""
            if [ "x${response}" = "x" ]; then
                response=${default_response}
            fi
            CFG_LCD_TYPE="${response}"
        fi
    else
        CFG_LCD_TYPE="${OPT_LCD_TYPE}"
    fi

    vecho 0 "== Using LCD type: ${CFG_LCD_TYPE}"
fi


if [ ${HAVE_OPT_LAYOUT} -eq 0 ]; then
    eval 'model_layout_def="${MODEL_'${CFG_MODEL}'_LAYOUT}"'
    if [ "x${model_layout_def}" = "x" ]; then
        default_response="${CFG_LAYOUT_DEF}"
    else
        default_response="${model_layout_def}"
    fi
    CFG_LAYOUT=${default_response}

    if [ ${OPT_AUTO} -eq 0 ]; then
        echo ""
        echo "--------------------"
        echo "Layout selection"
        echo "--------------------"
        echo ""
        
        response=""
        echo -n "Layout (${CFG_LAYOUT_CHOICES}) [${default_response}]: "
        ${READ_CMD} response
        echo ""
        if [ "x${response}" = "x" ]; then
            response=${default_response}
        fi
        CFG_LAYOUT="${response}"
    fi
else
    CFG_LAYOUT="${OPT_LAYOUT}"
fi
vecho 0 "== Using layout: ${CFG_LAYOUT}"


# XXX We could reasonably want to merge (overlay) the command line options
# on top of the model defaults, but do not do this currently.

if [ ${HAVE_OPT_PART_NAME_SIZE_LIST} -eq 0 ]; then
    eval 'model_part_name_size_list_def="${MODEL_'${CFG_MODEL}'_PART_NAME_SIZE_LIST}"'
    if [ "x${model_part_name_size_list_def}" = "x" ]; then
        default_response="${CFG_PART_NAME_SIZE_LIST_DEF}"
    else
        default_response="${model_part_name_size_list_def}"
    fi
    CFG_PART_NAME_SIZE_LIST=${default_response}

    if [ ${OPT_AUTO} -eq 0 ]; then
        echo ""
        echo "--------------------"
        echo "Partition name-size list selection"
        echo "--------------------"
        echo ""
        
        response=""
        echo -n "Paritition name-size list [${default_response}]: "
        ${READ_CMD} response
        echo ""
        if [ "x${response}" = "x" ]; then
            response=${default_response}
        fi
        CFG_PART_NAME_SIZE_LIST="${response}"
    fi
else
    CFG_PART_NAME_SIZE_LIST="${OPT_PART_NAME_SIZE_LIST}"
fi
vecho 0 "== Using partition name-size list: ${CFG_PART_NAME_SIZE_LIST}"

CFG_PART_NAME_SIZE_LIST_WRITEIMAGE=
ps_is_part=1
for part_name_size in ${CFG_PART_NAME_SIZE_LIST}; do
    if [ ${ps_is_part} -eq 1 ]; then
        CFG_PART_NAME_SIZE_LIST_WRITEIMAGE="${CFG_PART_NAME_SIZE_LIST_WRITEIMAGE} -p ${part_name_size}"
        ps_is_part=0
    else
        CFG_PART_NAME_SIZE_LIST_WRITEIMAGE="${CFG_PART_NAME_SIZE_LIST_WRITEIMAGE} -s ${part_name_size}"
        ps_is_part=1
    fi
done


if [ ${HAVE_OPT_DEV_LIST} -eq 0 ]; then
    eval 'model_dev_list_def="${MODEL_'${CFG_MODEL}'_DEV_LIST}"'
    if [ "x${model_dev_list_def}" = "x" ]; then
        default_response="${CFG_DEV_LIST_DEF}"
    else
        default_response="${model_dev_list_def}"
    fi
    CFG_DEV_LIST=${default_response}

    if [ ${OPT_AUTO} -eq 0 ]; then
        echo ""
        echo "--------------------"
        echo "Device list selection"
        echo "--------------------"
        echo ""
        
        response=""
        echo -n "Device list [${default_response}]: "
        ${READ_CMD} response
        echo ""
        if [ "x${response}" = "x" ]; then
            response=${default_response}
        fi
        CFG_DEV_LIST="${response}"
    fi
else
    CFG_DEV_LIST="${OPT_DEV_LIST}"
fi
vecho 0 "== Using device list: ${CFG_DEV_LIST}"

CFG_DEV_LIST_WRITEIMAGE=
for dev in ${CFG_DEV_LIST}; do
    CFG_DEV_LIST_WRITEIMAGE="${CFG_DEV_LIST_WRITEIMAGE} -d ${dev}"
done


if [ ${HAVE_OPT_IF_LIST} -eq 0 ]; then
    eval 'model_if_list_def="${MODEL_'${CFG_MODEL}'_IF_LIST}"'
    if [ "x${model_if_list_def}" = "x" ]; then
        default_response="${CFG_IF_LIST_DEF}"
    else
        default_response="${model_if_list_def}"
    fi
    CFG_IF_LIST=${default_response}

    if [ ${OPT_AUTO} -eq 0 ]; then
        echo ""
        echo "--------------------"
        echo "Interface list selection"
        echo "--------------------"
        echo ""
        
        response=""
        echo -n "Interface list [${default_response}]: "
        ${READ_CMD} response
        echo ""
        if [ "x${response}" = "x" ]; then
            response=${default_response}
        fi
        CFG_IF_LIST="${response}"
    fi
else
    CFG_IF_LIST="${OPT_IF_LIST}"
fi
vecho 0 "== Using interface list: ${CFG_IF_LIST}"


if [ ${HAVE_OPT_IF_NAMING} -eq 0 ]; then
    eval 'model_if_naming_def="${MODEL_'${CFG_MODEL}'_IF_NAMING}"'
    if [ "x${model_if_naming_def}" = "x" ]; then
        default_response="${CFG_IF_NAMING_DEF}"
    else
        default_response="${model_if_naming_def}"
    fi
    CFG_IF_NAMING=${default_response}

    if [ ${OPT_AUTO} -eq 0 ]; then
        echo ""
        echo "--------------------"
        echo "Interface naming selection"
        echo "--------------------"
        echo ""
        
        response=""
        echo -n "Interface naming [${default_response}]: "
        ${READ_CMD} response
        echo ""
        if [ "x${response}" = "x" ]; then
            response=${default_response}
        fi
        CFG_IF_NAMING="${response}"
    fi
else
    CFG_IF_NAMING="${OPT_IF_NAMING}"
fi
if [ ${OPT_AUTO} -ne 0 ]; then
    if [ "${CFG_IF_NAMING}" = "manual" ]; then
        vlechoerr -1 "Error: manual interface naming not allowed with auto."
        cleanup_exit
    fi
fi
vecho 0 "== Using interface naming: ${CFG_IF_NAMING}"


if [ ${HAVE_OPT_SMARTD} -eq 0 ]; then
    eval 'model_smartd_enable_def="${MODEL_'${CFG_MODEL}'_SMARTD_OPT}"'
    if [ "x${model_smartd_enable_def}" != "x" ]; then
        OPT_SMARTD="${model_smartd_enable_def}"
    fi

fi

if [ ${OPT_SMARTD} -eq 0 ]; then
    vecho 0 "== Smartd disabled"
else
    vecho 0 "== Smartd enabled"
fi

# Process any custom options here.
if [ "$HAVE_MANUFACTURE_GRAFT_6" = "y" ]; then
    manufacture_graft_6
fi
if [ "$HAVE_OPT_MANUFACTURE_GRAFT_6" = "y" ]; then
    opt_manufacture_graft_6
fi

IF_RUNNING_LIST=

# Sets global IF_RUNNING_LIST
update_if_running_list()
{
    IF_RUNNING_LIST=
    # XXX #dep/parse: ifconfig
    IFNF_RAW=`ifconfig -a | sed -e '/./{H;$!d;}' -e 'x;/Ethernet/!d' | egrep 'Ethernet|MTU'`

    # The names of the Ethernet interfaces
    IFNF_NAME_LINES=`echo "${IFNF_RAW}" | sed -n '1,${p;n}' | awk '{print $1}'`
    # The MACs of the Ethernet interfaces
    IFNF_MAC_LINES=`echo "${IFNF_RAW}" | sed -n '1,${p;n}' | sed 's/^.*HWaddr \(.*\)/\1/'`
    # The flags of the Ethernet interfaces
    IFNF_FLAGS_LINES=`echo "${IFNF_RAW}" | sed -n '2,${p;n}' | sed  's/^[ \t]*\(.*\)[ \t]MTU:.*/\1/'`

    uplist=
    line_count=1
    for mac in ${IFNF_MAC_LINES}; do
        flags=`echo "${IFNF_FLAGS_LINES}" | sed -n "${line_count}p"`
        name=`echo "${IFNF_NAME_LINES}" | sed -n "${line_count}p"`

        vecho 2 "mac: ${mac}"
        vecho 2 "name: ${name}"
        vecho 2 "fl: ${flags}"

        running=0
        echo "${flags}" | grep -q "RUNNING" && running=1
        if [ ${running} -eq 1 ]; then
            uplist="${uplist} ${mac}"
        fi

        line_count=$((${line_count} + 1))
    done

    IF_RUNNING_LIST=${uplist}
}


# Get the naming of all the interfaces
# ==================================================

# We make sure that there is a device given in
# /sys/class/net/IFNAME/device , as otherwise this may
# not be a physical interface.
#
# Note that we take out any alias interfaces, like eth0:1 , as they are 
# likely not a 'real' interface.

IF_TARGET_NAMES=${CFG_IF_LIST}
IF_ACCEPTED_PATTERN='.*'
if [ "${IF_CHECK_PHYSICAL}" = "1" ]; then
    # XXX #dep/parse: ifconfig
    IF_CANDIDATES=`ifconfig -a | egrep '^[^ \t]' | grep 'Ethernet.*HWaddr' | awk '{print $1}' | tr '\n' ' '`

    # Now, walk the candidates, and make sure there's a device file for each
    IF_ACCEPTED=
    IF_ACCEPTED_PATTERN=
    for ifn in ${IF_CANDIDATES}; do
        DN="/sys/class/net/${ifn}/device"
        if [ -e "${DN}" ]; then
            IF_ACCEPTED="${IF_ACCEPTED} ${ifn}"
            if [ ! -z "${IF_ACCEPTED_PATTERN}" ]; then
                IF_ACCEPTED_PATTERN="${IF_ACCEPTED_PATTERN}|${ifn}"
            else
                IF_ACCEPTED_PATTERN="${ifn}"
            fi
        fi
    done
fi

# XXX #dep/parse: ifconfig
IF_NAME_MAC_RAW=`ifconfig -a | grep 'Ethernet.*HWaddr' | sed 's/^\([^ ]*\) .*HWaddr \(.*\)/\2 \1/' | tr -s ' ' | egrep -v '^.* [^:]*:.*$' | egrep ' ('${IF_ACCEPTED_PATTERN}')$'`
IF_KERNNAMES_LIST=`echo "${IF_NAME_MAC_RAW}" | awk '{print $2}' | tr '\n' ' '`
IF_MACS_LIST=`echo "${IF_NAME_MAC_RAW}" | awk '{print $1}' | tr '\n' ' '`

if_num=1
for mac in ${IF_MACS_LIST}; do
    temp_mac_nums=`echo ${mac} | tr -d ':' | sed 's/\(..\)/ 0x\1/g'`
    dec_mac=`printf "%03d%03d%03d%03d%03d%03d" ${temp_mac_nums}`

    kernname=`echo ${IF_KERNNAMES_LIST} | awk '{print $'${if_num}'}'`
    targetname=`echo ${IF_TARGET_NAMES} | awk '{print $'${if_num}'}'`
    ifindex=`cat /sys/class/net/${kernname}/ifindex`

    eval 'IF_MAC_'${dec_mac}'_MAC="'${mac}'"'
    eval 'IF_MAC_'${dec_mac}'_IFINDEX="'${ifindex}'"'
    eval 'IF_MAC_'${dec_mac}'_KERNNAME="'${kernname}'"'
    eval 'IF_MAC_'${dec_mac}'_TARGETNAME="'${targetname}'"'

    if_num=$((${if_num} + 1))
done


# 'mac-sorted' works by, at mfg time, sorting the MAC addresses of the interfaces
# See also 'dyn-mac-sorted' for one that does this at run-time, which
# may be preferred.
if [ "${CFG_IF_NAMING}" = "mac-sorted" ]; then
    vecho 0 "- Assigning specified interface names in MAC-sorted order"

    # Build ordered list of decimal mac addresses from the mac list
    # We do this because 'sort -n' does not work for hex numbers.
    for mac in ${IF_MACS_LIST}; do
        temp_mac_nums=`echo ${mac} | tr -d ':' | sed 's/\(..\)/ 0x\1/g'`
        dec_mac=`printf "%03d%03d%03d%03d%03d%03d" ${temp_mac_nums}`

        dec_macs="${dec_macs} ${dec_mac}"
    done

    ORDERED_DEC_MACS=`echo ${dec_macs} | tr ' ' '\n' | sort -n | tr '\n' ' '`

    # Update the IF_MAC_*_TARGETNAME settings
    if_num=1
    for dec_mac in ${ORDERED_DEC_MACS}; do
        eval 'kernname="${IF_MAC_'${dec_mac}'_KERNNAME}"'
        targetname=`echo ${IF_TARGET_NAMES} | awk '{print $'${if_num}'}'`
        eval 'hex_mac="${IF_MAC_'${dec_mac}'_MAC}"'

        
        vecho 0 "-- Mapping MAC: ${hex_mac} from: ${kernname} to: ${targetname}"
        eval 'IF_MAC_'${dec_mac}'_TARGETNAME="'${targetname}'"'

        if_num=$((${if_num} + 1))
    done

elif [ "${CFG_IF_NAMING}" = "ifindex-sorted" ]; then
    # Note: see also dyn-ifindex-sorted for a run-time variant

    vecho 0 "- Assigning specified interface names in ifindex-sorted order"

    if_num=1
    isml=""
    for mac in ${IF_MACS_LIST}; do
        temp_mac_nums=`echo ${mac} | tr -d ':' | sed 's/\(..\)/ 0x\1/g'`
        dec_mac=`printf "%03d%03d%03d%03d%03d%03d" ${temp_mac_nums}`

        eval 'ifindex="${IF_MAC_'${dec_mac}'_IFINDEX}"'
        eval 'hex_mac="${IF_MAC_'${dec_mac}'_MAC}"'

        nl=$(printf "${ifindex}-${dec_mac}")
        isml="${isml} ${nl}"
    done
    IFINDEX_ORDERED_DECMACS=`echo "${isml}" | tr ' ' '\n' | sort -n | awk -F- '{print $2}' | tr '\n' ' '`
    ##echo "IOM: ${IFINDEX_ORDERED_DECMACS}"

    for dec_mac in ${IFINDEX_ORDERED_DECMACS}; do

        eval 'kernname="${IF_MAC_'${dec_mac}'_KERNNAME}"'
        targetname=`echo ${IF_TARGET_NAMES} | awk '{print $'${if_num}'}'`
        eval 'hex_mac="${IF_MAC_'${dec_mac}'_MAC}"'

        vecho 0 "-- Mapping MAC: ${hex_mac} from: ${kernname} to: ${targetname}"
        eval 'IF_MAC_'${dec_mac}'_TARGETNAME="'${targetname}'"'

        if_num=$((${if_num} + 1))
    done

elif [ "${CFG_IF_NAMING}" = "unsorted" ]; then
    # This is deprecated, and should not be used.  Use 'ifindex-sorted'
    # instead.

    vecho 0 "- Assigning specified interface names in kernel 'ifconfig' order"

    # There is nothing to do, this is currently the default

    # Just print what we'll be doing
    for mac in ${IF_MACS_LIST}; do
        temp_mac_nums=`echo ${mac} | tr -d ':' | sed 's/\(..\)/ 0x\1/g'`
        dec_mac=`printf "%03d%03d%03d%03d%03d%03d" ${temp_mac_nums}`

        eval 'kernname="${IF_MAC_'${dec_mac}'_KERNNAME}"'
        eval 'targetname="${IF_MAC_'${dec_mac}'_TARGETNAME}"'
        eval 'hex_mac="${IF_MAC_'${dec_mac}'_MAC}"'

        vecho 0 "-- Mapping MAC: ${hex_mac} from: ${kernname} to: ${targetname}"
    done

elif [ "${CFG_IF_NAMING}" = "dyn-mac-sorted" ]; then
    vecho 0 "- Interface names will be assigned in dynamic mac-sorted order"

elif [ "${CFG_IF_NAMING}" = "dyn-ifindex-sorted" ]; then
    vecho 0 "- Interface names will be assigned in dynamic ifindex-sorted order"

elif [ "${CFG_IF_NAMING}" = "manual" ]; then

    # XXX ask them for each kernname, plug in MAC

    for intf in ${IF_TARGET_NAMES}; do
        update_if_running_list
        prev_uplist=${IF_RUNNING_LIST}
        vecho 2 "prev ul: ${uplist}"

        echo "=== Change the link state of the interface you want to be '${intf}', and hit enter:"
        ${READ_CMD} junk

        update_if_running_list
        new_uplist=${IF_RUNNING_LIST}
        vecho 2 "new ul: ${uplist}"

        diff_mac=`echo "${prev_uplist} ${new_uplist}" | tr ' ' '\n' | sort | uniq -u`

        if [ -z "${diff_mac}" ]; then
            echo "== Warning: no change in link state detected for ${intf}, ${intf} not assigned"
            continue
        fi

        temp_mac_nums=`echo ${diff_mac} | tr -d ':' | sed 's/\(..\)/ 0x\1/g'`
        dec_mac=`printf "%03d%03d%03d%03d%03d%03d" ${temp_mac_nums}`

        eval 'kernname="${IF_MAC_'${dec_mac}'_KERNNAME}"'
        eval 'IF_MAC_'${dec_mac}'_TARGETNAME="'${intf}'"'

        echo "-- Mapping MAC: ${diff_mac} from: ${kernname} to: ${intf}"
    done

elif [ "${CFG_IF_NAMING}" = "none" ]; then
    vecho 0 "- Disabling interface renaming."
else
    vlechoerr -1 "*** Error: bad CFG_IF_NAMING setting"
    cleanup_exit
fi

# Validate that HWNAME matches our hardware.  
#
# It is very important that the validation be accurate, as once the
# HWNAME is given to writeimage, it will be persisted and is not
# changeable.  It will be used on all future image installs to check
# compatibility between the system and the image. 
#

if [ ! -z "${CFG_HWNAME}" ]; then

    SET_HWNAME="${CFG_HWNAME}"
    # This next line only works on PowerPC.
    FOUND_HWNAME="$(cat /proc/cpuinfo | grep '^platform' | sed 's/^.*: *\(.*\)$/\1/' | tr -d ' ')"
    DONE_COMPARE=0

    # The customer can set "FOUND_HWNAME" based on techniques like looking
    # at an EEPROM, looking at things in /proc, etc .

    if [ "$HAVE_MANUFACTURE_GRAFT_7" = "y" ]; then
        manufacture_graft_7
    fi

    if [ ${DONE_COMPARE} -ne 1 ]; then
        if [ "${OPT_HWNAME_COMPAT_IGNORE}" != "1" ]; then
            if [ -z "${FOUND_HWNAME}" ]; then
                vlechoerr -1 "*** Error: wanted hardware ${SET_HWNAME} but found no HWNAME"
                cleanup_exit
            elif [ "${FOUND_HWNAME}" != "${SET_HWNAME}" ]; then
                vlechoerr -1 "*** Error: wanted hardware ${SET_HWNAME} but found ${FOUND_HWNAME}"
                cleanup_exit
            else
                vecho 0 "- Verified hardware is ${SET_HWNAME}"
            fi
        else
            if [ "${FOUND_HWNAME}" != "${SET_HWNAME}" ]; then
                if [ -z "${FOUND_HWNAME}" ]; then
                    vlecho 0 "-- Note: wanted hardware ${SET_HWNAME} but found no HWNAME, forced compatibility"
                else
                    vlecho 0 "-- Note: wanted hardware ${SET_HWNAME} but found ${FOUND_HWNAME}, forced compatibility"
                fi
            else
                vecho 0 "- Verified hardware is ${SET_HWNAME}"
            fi
        fi
    fi
fi

# ==================================================

if [ ${OPT_POSTIMAGE_ONLY} -ne 1 ]; then

    if [ ${SYSIMAGE_USE_URL} -ne 0 ]; then
        vlecho 0 "== Using image from URL: ${SYSIMAGE_URL}"
    else
        vlecho 0 "== Using image from file: ${SYSIMAGE_FILE}"
    fi

    WI_CMD="${WRITEIMAGE} -m ${WRITEIMAGE_ARGS} -k ${CFG_KERNEL_TYPE}  \
        -L ${CFG_LAYOUT} ${CFG_DEV_LIST_WRITEIMAGE} ${CFG_PART_NAME_SIZE_LIST_WRITEIMAGE}"

    if [ ! -z "${CFG_HWNAME}" ]; then
        WI_CMD="${WI_CMD} -w ${CFG_HWNAME}"
    fi

    vecho 0 ""
    vlecho 0 "== Calling writeimage to image system"
    vlecho 1 "--- Executing: ${WI_CMD}"

    FAILURE=0
    ${WI_CMD} || FAILURE=1
    if [ ${FAILURE} -eq 1 ]; then
        vlechoerr -1 "Error: writeimage failed, exiting."
        cleanup_exit
    fi
    
    vlecho 0 "== System successfully imaged"
else
    vlecho 0 "== Skipping system imaging"
fi

# ==================================================


# Do the post-install stuff plan : slurp in layout_settings.sh, mount all
# partitions, copy over any needed binaries (like mddbreq), write bindings
# into the mfg db (like interface naming bindings).



# --------------------------------------------------
# Get all our partition related settings in

IL_LAYOUT=${CFG_LAYOUT}
export IL_LAYOUT
IL_HWNAME=${CFG_HWNAME}
export IL_HWNAME

if [ -r ${IL_PATH} ]; then
    . ${IL_PATH}
else
    vlechoerr -1 "*** Invalid image layout settings."
    usage
fi

# Now set the targets (devices) that the user specified

# Set TARGET_NAMES, which are things like 'DISK1'
eval 'TARGET_NAMES="${IL_LO_'${IL_LAYOUT}'_TARGETS}"'
dev_num=1
dev_list="${CFG_DEV_LIST}"
for tn in ${TARGET_NAMES}; do
    nname=`echo ${dev_list} | awk '{print $'${dev_num}'}'`
    if [ ! -z "${nname}" ]; then
        eval 'IL_LO_'${IL_LAYOUT}'_TARGET_'${tn}'_DEV="${nname}"'
    else
        eval 'curr_dev="${IL_LO_'${IL_LAYOUT}'_TARGET_'${tn}'_DEV}"'
        if [ -z "${curr_dev}" ]; then
            vlechoerr -1 "*** Not enough device targets specified."
            cleanup_exit
        fi
    fi
    dev_num=$((${dev_num} + 1))
done

if [ -r ${IL_PATH} ]; then
    . ${IL_PATH}
else
    vlechoerr -1 "*** Invalid image layout settings."
    usage
fi


# --------------------------------------------------
# Mount everything, arbitrarily from image 1

HAVE_VPART=0
UNMOUNT_LIST=

if [ ${DO_MOUNT} -ne 0 ]; then
    mkdir -p ${MFG_MOUNT}/

    inum=1
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
                eval 'IMAGE_'${inum}'_PART_TYPE_'${add_part}'="p"'
            fi
        fi
        
        eval 'add_vpart="${IL_LO_'${IL_LAYOUT}'_LOC_'${loc}'_VPART}"'
        
        if [ ! -z "${add_vpart}" ]; then
            # Only add it on if it is unique
            eval 'curr_list="${IMAGE_'${inum}'_PART_LIST}"'
            
            vecho 2 "add_vpart = ${add_vpart}"
            
            present=0
            echo "${curr_list}" | grep -q " ${add_vpart} " - && present=1
            if [ ${present} -eq 0 ]; then
                eval 'IMAGE_'${inum}'_PART_LIST="${IMAGE_'${inum}'_PART_LIST} ${add_vpart} "'
                eval 'IMAGE_'${inum}'_PART_TYPE_'${add_vpart}'="v"'
                
                eval 'bk_part_name="${IL_LO_'${IL_LAYOUT}'_VPART_'${add_vpart}'_PART_NAME}"'
                
                vecho 2 "bk partition for virtual ${add_vpart} is ${bk_part_name}"
                
                if [ ! -z "${bk_part_name}" ]; then
                    # Only add it on if it is unique
                    eval 'curr_bkpart_list="${IMAGE_'${inum}'_VPART_BACK_LIST}"'
                
                    present=0
                    echo "${curr_bkpart_list}" | grep -q " ${bk_part_name} " - && present=1
                    if [ ${present} -eq 0 ]; then
                        eval 'IMAGE_'${inum}'_VPART_BACK_LIST="${IMAGE_'${inum}'_VPART_BACK_LIST} ${bk_part_name} "'
                    fi
                fi
                
            fi
        fi
        
    done

    # Mount all physical partitions prior to any virtual ones (as they
    # require the backing partitions to be mounted).

    inum=1
    eval 'bkpart_list="${IMAGE_'${inum}'_VPART_BACK_LIST}"'
    UNMOUNT_LIST=

    vecho 2 "bkpart_list = ${bkpart_list}"

    for bkpart in ${bkpart_list}; do
        # Must get the backing partition mounted
        eval 'part_dev="${IL_LO_'${IL_LAYOUT}'_PART_'${bkpart}'_DEV}"'
        eval 'part_mount="${IL_LO_'${IL_LAYOUT}'_PART_'${bkpart}'_MOUNT}"'
        eval 'part_fstype="${IL_LO_'${IL_LAYOUT}'_PART_'${bkpart}'_FSTYPE}"'
        
        if [ -z "${bkpart}" -o -z "${part_dev}" \
            -o -z "${part_mount}" -o -z "${part_fstype}" \
            -o "${part_fstype}" = "swap" ]; then
            continue
        fi

        mount_point="${MFG_VMOUNT}/${part_mount}"

        mkdir -p ${mount_point}
        umount ${mount_point} > /dev/null 2>&1
        FAILURE=0

        vecho 2 "mount ${part_dev} on ${mount_point}"
        mount_options="-o noatime -t ${part_fstype}"

        mount ${mount_options} ${part_dev} ${mount_point} || FAILURE=1
        if [ ${FAILURE} -eq 1 ]; then
            vlechoerr -1 "*** Could not mount partition ${part_dev} on ${mount_point}"
            cleanup_exit
        fi
        UNMOUNT_LIST="${mount_point} ${UNMOUNT_LIST}"
    done

    inum=1
    eval 'part_list="${IMAGE_'${inum}'_PART_LIST}"'

    vecho 2 "part_list = ${part_list}"

    # Indicate if there are any virtual partitions
    HAVE_VPART=0

    for part in ${part_list}; do
        # Either a virtual or physical partition
        eval 'part_vtype="${IMAGE_'${inum}'_PART_TYPE_'${part}'}"'

        vecho 2 "part type for ${part} is ${part_vtype}"
        if [ "${part_vtype}" = "v" ]; then
            eval 'part_mount="${IL_LO_'${IL_LAYOUT}'_VPART_'${part}'_MOUNT}"'
            mount_point="${MFG_MOUNT}/${part_mount}"
            
            mkdir -p ${mount_point}
            
            do_mount_vpart ${part} ${mount_point}
            HAVE_VPART=1
            continue
        fi

        eval 'part_dev="${IL_LO_'${IL_LAYOUT}'_PART_'${part}'_DEV}"'
        eval 'part_mount="${IL_LO_'${IL_LAYOUT}'_PART_'${part}'_MOUNT}"'
        eval 'part_fstype="${IL_LO_'${IL_LAYOUT}'_PART_'${part}'_FSTYPE}"'

        if [ -z "${part}" -o -z "${part_dev}" \
            -o -z "${part_mount}" -o -z "${part_fstype}" \
            -o "${part_fstype}" = "swap" ]; then
            continue
        fi

        mount_point="${MFG_MOUNT}/${part_mount}"
        mount_options="-o noatime -t ${part_fstype}"

        mkdir -p ${mount_point}
        umount ${mount_point} > /dev/null 2>&1
        FAILURE=0

        vecho 2 "mount ${part_dev} on ${mount_point}"

        mount ${mount_options} ${part_dev} ${mount_point} || FAILURE=1
        if [ ${FAILURE} -eq 1 ]; then
            vlechoerr -1 "*** Could not mount partition ${part_dev} on ${mount_point}"
            cleanup_exit
        fi
        UNMOUNT_LIST="${mount_point} ${UNMOUNT_LIST}"
    done
fi

# Copy over files from the image into the mfg environment.  This is
# deprecated, and should not be used.
if [ ${DO_MOUNT} -ne 0 ]; then
    for file in ${MFG_POST_COPY_DEPRECATED}; do
        mkdir -p `dirname $file`
        FAILURE=0
        cp -p ${MFG_MOUNT}/$file $file || FAILURE=1
        if [ ${FAILURE} -eq 1 ]; then
            vlechoerr -1 "*** Could not copy $file from image to manufacturing environment"
            cleanup_exit
        fi
    done
fi

if [ ! -x "${MDDBREQ}" ]; then
    vlechoerr -1 "*** Could not find ${MDDBREQ} binary"
    cleanup_exit
fi

# Generate a random host id which is 48 bits long
if [ ${HAVE_OPT_HOSTID} -eq 1 ]; then
    CFG_HOSTID="${OPT_HOSTID}"
else
    CFG_HOSTID=`dd if=/dev/urandom bs=1 count=6 2> /dev/null | od -x | head -1 | \
        awk '{print $2 $3 $4}'`
fi

# Hostid must contain only: alphanumeric, - and _ , and must not start
# with a - .  Leading and trailing spaces are silently trimmed.
# Offending characters are converted to _ .  Maximum length is 64
# characters.

RAW_HOSTID=`echo "${CFG_HOSTID}" | sed 's/^[ \t]*//' | sed 's/[ \t]*$//'`
CFG_HOSTID=`echo "${RAW_HOSTID}" | sed 's/[^a-zA-Z0-9_-]/_/g' | sed 's/^-/_/' | \
    sed 's/^\(................................................................\).*$/\1/'`
if [ "${CFG_HOSTID}" != "${RAW_HOSTID}" ]; then
    vlecho -1 "-- Warning: Host ID will be \"${CFG_HOSTID}\" instead of \"${RAW_HOSTID}\""
fi

vecho 0 "-- Writing Host ID: ${CFG_HOSTID}"

# XXXX prompt about other things like licenses, have customer graft point

mkdir -m 755 -p ${MFG_DB_DIR}
rm -f ${MFG_DB_PATH}

FAILURE=0
${MDDBREQ} -c ${MFG_DB_PATH} set modify "" /mfg/mfdb/system/layout string "${IL_LAYOUT}" || FAILURE=1
${MDDBREQ} -c ${MFG_DB_PATH} set modify "" /mfg/mfdb/system/model string "${CFG_MODEL}" || FAILURE=1
${MDDBREQ} -c ${MFG_DB_PATH} set modify "" /mfg/mfdb/system/hostid string "${CFG_HOSTID}" || FAILURE=1
if [ ! -z "${CFG_HWNAME}" ]; then
    ${MDDBREQ} -c ${MFG_DB_PATH} set modify "" /mfg/mfdb/system/hwname string "${CFG_HWNAME}" || FAILURE=1
else
    ${MDDBREQ} -c ${MFG_DB_PATH} set delete "" /mfg/mfdb/system/hwname || FAILURE=1
fi

if [ ${FAILURE} -eq 1 ]; then
    vlechoerr -1 "*** Could not set base manufacturing settings"
    cleanup_exit
fi

# XXXXX We should check the return code of each of the mddbreq's below

if [ ${OPT_SMARTD} -eq 1 ]; then
    ${MDDBREQ} -c ${MFG_DB_PATH} set modify "" /mfg/mfdb/smartd/enable bool true
else
    ${MDDBREQ} -c ${MFG_DB_PATH} set modify "" /mfg/mfdb/smartd/enable bool false
fi

#
# Set up the default virtualization storage pool.  The customer can override
# or add to this from manufacture_graft_2.
#
# Note that we intentionally do not conditionalize this on PROD_FEATURE_VIRT.
# This is because some users may use a single manufacturing environment to
# manufacture many different images, and we don't want trouble because 
# a manufacturing environment lacking this feature was used to install an
# image which had it.  And in particular, with bug 11753 and bug 11969,
# we only ever use the 32-bit manufacturing environment, and 
# PROD_FEATURE_VIRT is not supported for 32-bit.  So it is expected that
# in most cases, the manufacturing environment will lack this feature.
#
${MDDBREQ} -c ${MFG_DB_PATH} set modify "" /mfg/mfdb/virt/pools/pool/default string default

for dev in ${CFG_DEV_LIST}; do
    dev_name=`echo ${dev} | sed 's/\//\\\\\//g'`
    ${MDDBREQ} -c ${MFG_DB_PATH} set modify "" /mfg/mfdb/smartd/device/${dev_name} string ${dev}
done

if [ ${HAVE_VPART} -eq 1 ]; then
    ${MDDBREQ} -c ${MFG_DB_PATH} set modify "" /mfg/mfdb/vpart/enable bool true
else
    ${MDDBREQ} -c ${MFG_DB_PATH} set modify "" /mfg/mfdb/vpart/enable bool false
fi

# Write out the list of interface names for the model in /mfg/mfdb/interface/name/#/name
if [ "${CFG_IF_NAMING}" != "none" ]; then
    if_num=1
    for ifn in ${IF_TARGET_NAMES}; do
        ${MDDBREQ} -c ${MFG_DB_PATH} set modify "" /mfg/mfdb/interface/name/${if_num} uint32 ${if_num}
        ${MDDBREQ} -c ${MFG_DB_PATH} set modify "" /mfg/mfdb/interface/name/${if_num}/name string "${ifn}"
        if_num=$((${if_num} + 1))
    done
fi

#
# Write out the list of mac -> interface name mappings in
#  /mfg/mfdb/interface/map/macifname/1/macaddr and
#  /mfg/mfdb/interface/map/macifname/1/name
#
if [ "${CFG_IF_NAMING}" = "none" ]; then
    ${MDDBREQ} -c ${MFG_DB_PATH} set modify "" /mfg/mfdb/interface/map/enable bool false
    ${MDDBREQ} -c ${MFG_DB_PATH} set modify "" /mfg/mfdb/interface/rename_type string "none"
else
    is_mac_map=1
    if [ "${CFG_IF_NAMING}" = "dyn-mac-sorted" -o "${CFG_IF_NAMING}" = "dyn-ifindex-sorted" ]; then
        is_mac_map=0
    fi

    if [ "${is_mac_map}" = "1" ]; then
        ${MDDBREQ} -c ${MFG_DB_PATH} set modify "" /mfg/mfdb/interface/map/enable bool true
        ${MDDBREQ} -c ${MFG_DB_PATH} set modify "" /mfg/mfdb/interface/map/type string "${CFG_IF_NAMING}"
        ${MDDBREQ} -c ${MFG_DB_PATH} set modify "" /mfg/mfdb/interface/rename_type string "mac-map"

        map_num=1
        for mac in ${IF_MACS_LIST}; do
            temp_mac_nums=`echo ${mac} | tr -d ':' | sed 's/\(..\)/ 0x\1/g'`
            dec_mac=`printf "%03d%03d%03d%03d%03d%03d" ${temp_mac_nums}`

            eval 'kernname="${IF_MAC_'${dec_mac}'_KERNNAME}"'
            eval 'targetname="${IF_MAC_'${dec_mac}'_TARGETNAME}"'

            if [ -z "${targetname}" ]; then
                vlecho -1 "-- Ignoring unmapped mac ${mac} on ${kernname}"
                continue
            fi
            vecho 0 "-- Writing mapping for ${mac} from ${kernname} to ${targetname}" 

            ${MDDBREQ} -c ${MFG_DB_PATH} set modify "" /mfg/mfdb/interface/map/macifname/${map_num} uint32 ${map_num}
            ${MDDBREQ} -c ${MFG_DB_PATH} set modify "" /mfg/mfdb/interface/map/macifname/${map_num}/macaddr macaddr802 "${mac}"
            ${MDDBREQ} -c ${MFG_DB_PATH} set modify "" /mfg/mfdb/interface/map/macifname/${map_num}/name string "${targetname}"

            map_num=$((${map_num} + 1))

        done
    else
        vecho 0 "-- Using dynamic interface mapping: ${CFG_IF_NAMING} .  No static mappings."
        ${MDDBREQ} -c ${MFG_DB_PATH} set modify "" /mfg/mfdb/interface/map/enable bool false
        ${MDDBREQ} -c ${MFG_DB_PATH} set modify "" /mfg/mfdb/interface/rename_type string "${CFG_IF_NAMING}"

    fi
fi


# Write mfdb node to tell FPD which I/O module to load to support LCD
if [ "x$BUILD_PROD_FEATURE_FRONT_PANEL" = "x1" ]; then
    ${MDDBREQ} -c ${MFG_DB_PATH} set modify "" /mfg/mfdb/fpd/hw_model string ${CFG_LCD_TYPE}
    if [ "${CFG_LCD_TYPE}" != "none" ]; then
        if [ "${CFG_LCD_TYPE}" = "advlcm100" ]; then
            ${MDDBREQ} -c ${MFG_DB_PATH} set modify "" /mfg/mfdb/fpd/io_modules/io_adv_lcm100_ser string io_adv_lcm100_ser
            ${MDDBREQ} -c ${MFG_DB_PATH} set modify "" /mfg/mfdb/fpd/device_path string "/dev/ttyS1"
            ${MDDBREQ} -c ${MFG_DB_PATH} set modify "" /mfg/mfdb/fpd/device_baud uint32 2400
        fi
        if [ "${CFG_LCD_TYPE}" = "cf631" ]; then
            ${MDDBREQ} -c ${MFG_DB_PATH} set modify "" /mfg/mfdb/fpd/io_modules/io_cfontz_633_ser string io_cfontz_633_ser
            ${MDDBREQ} -c ${MFG_DB_PATH} set modify "" /mfg/mfdb/fpd/device_path string "/dev/ttyUSB0"
            ${MDDBREQ} -c ${MFG_DB_PATH} set modify "" /mfg/mfdb/fpd/device_baud uint32 115200
        fi
        if [ "${CFG_LCD_TYPE}" = "cf633" ]; then
            ${MDDBREQ} -c ${MFG_DB_PATH} set modify "" /mfg/mfdb/fpd/io_modules/io_cfontz_633_ser string io_cfontz_633_ser
            ${MDDBREQ} -c ${MFG_DB_PATH} set modify "" /mfg/mfdb/fpd/device_path string "/dev/ttyS1"
            ${MDDBREQ} -c ${MFG_DB_PATH} set modify "" /mfg/mfdb/fpd/device_baud uint32 19200
        fi
        if [ "${CFG_LCD_TYPE}" = "cf635" ]; then
            ${MDDBREQ} -c ${MFG_DB_PATH} set modify "" /mfg/mfdb/fpd/io_modules/io_cfontz_633_ser string io_cfontz_633_ser
            ${MDDBREQ} -c ${MFG_DB_PATH} set modify "" /mfg/mfdb/fpd/device_path string "/dev/ttyUSB0"
            ${MDDBREQ} -c ${MFG_DB_PATH} set modify "" /mfg/mfdb/fpd/device_baud uint32 115200
        fi
    fi
fi

# Graft point for adding things to mfdb or mfincdb, or acting on any
# custom options specified.
if [ "$HAVE_MANUFACTURE_GRAFT_2" = "y" ]; then
    manufacture_graft_2
fi
if [ "$HAVE_OPT_MANUFACTURE_GRAFT_2" = "y" ]; then
    opt_manufacture_graft_2
fi

if [ "${HAVE_OPT_POSTEXEC}" -ne 0 ]; then
    ${OPT_POSTEXEC}
fi

sync

# Unmount all the partitions
if [ ${DO_MOUNT} -ne 0 ]; then
    for ump in ${UNMOUNT_LIST}; do
        umount ${ump} > /dev/null 2>&1
    done
fi

if [ ${OPT_IMAGE_VERIFY} -ne 0 ]; then
    vlecho 0 "== Calling imgverify to verify manufactured system"
    HWNAME_IMGV=
    if [ ! -z "${CFG_HWNAME}" ]; then
        HWNAME_IMGV="-w ${CFG_HWNAME}"
    fi

    # See if the layout used VFAT, as we must tell imgverify to be
    # suitably flexible about timestamps.
    HAS_FAT=0
    eval 'parts="${IL_LO_'${IL_LAYOUT}'_PARTS}"'
    for part in ${parts}; do
        eval 'part_fstype="${IL_LO_'${IL_LAYOUT}'_PART_'${part}'_FSTYPE}"'
        if [ "${part_fstype}" = "vfat" ]; then
            HAS_FAT=1
        fi
    done

    FAT_ARG=
    if [ ${HAS_FAT} -eq 1 ]; then
        FAT_ARG="-F"
    fi

    ${IMGVERIFY} -m ${VERBOSE_ARGS} -L ${CFG_LAYOUT} ${HWNAME_IMGV} ${FAT_ARG} ${CFG_DEV_LIST_WRITEIMAGE} > /tmp/imgverify.out || FAILURE=1
    cat /tmp/imgverify.out
    if [ ${FAILURE} -eq 1 ]; then
        vlechoerr -1 "*** Could not verify manufactured system"
        cleanup_exit
    fi
fi

cleanup

END_TIME=$(date "+%Y%m%d-%H%M%S")
vlecho 0 "====== Ending manufacture at ${END_TIME}"
vecho 0 "-- Manufacture done."
