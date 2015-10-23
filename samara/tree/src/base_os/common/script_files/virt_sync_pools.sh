#!/bin/sh

#
# (C) Copyright 2013-2015 Juniper Networks, Inc. All Rights reserved.
# (C) Copyright 2002-2011 Tall Maple Systems, Inc.
# All rights reserved.
# The contents of this file are subject to the terms of the MIT License.
# You may not use this file except in compliance with the License.
# See the file License.txt at the top of this source tree.
# You can obtain a copy of the license at https://opensource.org/licenses/MIT
#

#
# This script ensures that the set of directories and symlinks in
# /var/opt/tms/virt/pools matches the wishes of the mfdb to the 
# extent possible, and complains if it cannot fix certain things.
#
# Note that we don't create the .temp directories within each storage
# pool.  Tvirtd takes care of that.
#

PATH=/usr/bin:/bin:/usr/sbin:/sbin
export PATH

. /etc/init.d/functions
. /etc/build_version.sh

MFG_DB_DIR=/config/mfg
MFG_DB_PATH=${MFG_DB_DIR}/mfdb
MDDBREQ=/opt/tms/bin/mddbreq
CHOWN=/bin/chown
CHMOD=/bin/chmod
MKDIR=/bin/mkdir
RMDIR=/bin/rmdir
FIND=/usr/bin/find
RM=/bin/rm
LN=/bin/ln
READLINK=/usr/bin/readlink
LOGGER=/usr/bin/logger
EGREP=/bin/egrep
WC=/usr/bin/wc

POOL_ROOT_NODE=/mfg/mfdb/virt/pools/pool
POOL_ROOT_DIR=/var/opt/tms/virt/pools
POOL_OWNER=0
# mgt_virt = 3002
POOL_GROUP=3002
POOL_MODE=0770
POOL_FILE_MODE=0660

#
# The storage pool directories themselves must have mode 0770 (note this
# includes full group access, for the 'virt' group).
# But if we have to create any parents along the way (with '${MKDIR} -p'),
# we want those to be 0755, lest we get in the way of some other intended
# usage for those directories.  So we'll set this umask, but then chmod
# any leaf directories we create.
#
# On principle, we set this below /etc/init.d/functions, which sets its
# own umask... though it happens to be the same as this one.
#
umask 0022

echo -n "Setting up virtualization storage pools: "

#
# NOTE: we can try to log stuff below, but it probably won't come out
# during boot, since syslog is not running yet: see bug 10961.
# We'll do it anyway, in case this bug is ever fixed, and to help
# troubleshooting when the script is run manually from a running image.
#
# XXXX/EMT: we don't detect failures of operations that are unlikely to fail,
# such as ${MKDIR} when we think we know there's nothing already there.
#

POOLS=`${MDDBREQ} -v ${MFG_DB_PATH} query iterate - ${POOL_ROOT_NODE}`
RETURN=$?
if [ ${RETURN} -ne 0 ]; then
    #
    # With other failures below, we try to carry on, but with one this basic,
    # we just bail out now.
    #
    echo "virt_sync_pools.sh: unable to query mfdb at ${MFG_DB_PATH}"
    failure $"Virt pool sync"
    echo ""
    exit 1
fi

GLOBAL_FAILURE=0
FAILURE=0

# =============================================================================
# ensure_dir <pool name> <directory path>
#
# Function used to ensure that a specified path is a directory, if possible.
# If a pool is supposed to be a native directory, we use it on that.
# If a pool is supposed to be a symlink, we use it on the target.
#
# We test the path, and handle four cases:
#    (a) It exists, and is a symlink (good, we can fix it)
#    (b) It exists, and is a directory (good)
#    (c) It exists, and is something else (bad, leave alone)
#    (d) It does not exist (good, we can create it)
#
# We're trying to have a unique code to include in our log message  with 
# each case, and since our caller prints stuff with the number 1, we'll 
# use 2.
#
ensure_dir() {
    POOL="${1}"
    PATH="${2}"
    if [ -h "${PATH}" ]; then
        # .........................................................
        # 2a. Bad-->Good!  It's a symlink, which is wrong, but we
        #     don't mind fixing it.
        #
        # Note that we had to test for symlink before testing for existence,
        # because otherwise if it's a symlink and you end up testing for
        # its existence, it tells you whether the target exists.
        #
        ${RM} -f ${PATH}
        ${MKDIR} -p ${PATH}
        ${CHOWN} ${POOL_OWNER}:${POOL_GROUP} ${PATH}
        ${CHMOD} ${POOL_MODE} ${PATH}
        ${LOGGER} "Virt pool '${POOL}' (case 2a): ${PATH}: was a symlink, but removed and created a directory."
        
    elif [ -e "${PATH}" ]; then
        if [ -d "${PATH}" ]; then
            # .........................................................
            # 2b. Good!  It's already a directory.  Make sure its owner
            #     and mode are correct.  This is mostly only important on
            #     upgrade (the 45-to-46 /var upgrade in particular), but
            #     can't hurt to ensure each time.
            #
            ${LOGGER} "Virt pool '${POOL}' (case 2b): ${PATH}: already a directory."
            ${CHOWN} ${POOL_OWNER}:${POOL_GROUP} ${PATH}
            ${CHMOD} ${POOL_MODE} ${PATH}

        else
            # .........................................................
            # 2c. Bad!  It exists, but it's neither a directory nor
            #     a symlink.  We won't meddle, just leave it as is.
            #
            # We have to print something, since this was detected by a test.
            #
            FAILURE=1
            ${LOGGER} "Virt pool '${POOL}' (case 2c): ${PATH}: ERROR: was present, but not a directory or a symlink!  Skipping pool."
            echo "virt_sync_pools.sh: ${PATH}: not a directory or a symlink"
        fi

    else
        # .............................................................
        # 2d. Good!  It doesn't exist yet, so we can create it.
        #
        ${MKDIR} -p ${PATH}
        ${CHOWN} ${POOL_OWNER}:${POOL_GROUP} ${PATH}
        ${CHMOD} ${POOL_MODE} ${PATH}
        ${LOGGER} "Virt pool '${POOL}' (case 2d): ${PATH}: created directory."
    fi
}


# =============================================================================
# Main loop
#
# POOL:      name of storage pool
# POOL_PATH: full path to item under POOL_ROOT_DIR (dir or link)
# LINK:      if symlink pool, this is where it should link to
# CURR_LINK: where the pool currently links to
#

for POOL in ${POOLS}; do
    FAILURE=0

    BADTEMP=`echo ${POOL} | ${EGREP} -- '-TEMP$' | ${WC} -l`
    if [ "${BADTEMP}" = "1" ]; then
        ${LOGGER} "Virt pool '${POOL}': name may not end in "-TEMP", ignoring"
        continue
    fi

    BADCHAR=`echo ${POOL} | ${EGREP} -- '[^A-Za-z0-9_.-]' | ${WC} -l`
    if [ "${BADCHAR}" = "1" ]; then
        ${LOGGER} "Virt pool '${POOL}': invalid characters in name, ignoring"
        continue
    fi

    POOL_PATH="${POOL_ROOT_DIR}/${POOL}"
    LINK=`${MDDBREQ} -v ${MFG_DB_PATH} query get - ${POOL_ROOT_NODE}/${POOL}/link_to`
    # ---------------------------------------------------------------------
    # (1a) If it's supposed to be a directory, all we need is ensure_dir.
    #
    if [ -z "${LINK}" ]; then
        ${LOGGER} "Virt pool '${POOL}' (case 1a): ${POOL_PATH}: native directory."
        ensure_dir "${POOL}" "${POOL_PATH}"
        
    # ---------------------------------------------------------------------
    # If it's supposed to be a symlink, there are five cases:
    #    (1b) It exists, and is a symlink to the right location (good)
    #    (1c) It exists, and is a symlink to the wrong location (bad,
    #         but we can fix it)
    #    (1d) It exists, and is an empty directory (bad, but we can fix it)
    #    (1e) It exists, but is a non-empty directory (bad, leave alone)
    #    (1f) If exists, but is neither a symlink nor a directory
    #         (bad, leave alone)
    #    (1g) It does not exist (good, we can create it)
    # 
    # Then if we haven't already failed. we call ensure_dir afterwards for 
    # the target.
    #
    # (Again, we must test for symlink first, lest we be fooled by testing
    # the symlink's target.)
    #
    else
        if [ -h "${POOL_PATH}" ]; then
            CURR_LINK=`${READLINK} ${POOL_PATH}`
            if [ "${CURR_LINK}" = "${LINK}" ]; then
                # .............................................................
                # 1b. Good!  This is already a symlink to the right place.
                #
                ${LOGGER} "Virt pool '${POOL}' (case 1b): ${POOL_PATH}: symlink to ${LINK}: already present and correct."

            else
                # .........................................................
                # 1c. Bad-->Good!  This is a symlink, but to the wrong
                #     location.  We can fix this.
                #
                ${RM} -f "${POOL_PATH}"
                ${LN} -s "${LINK}" "${POOL_PATH}"
                ${LOGGER} "Virt pool '${POOL}' (case 1c): ${POOL_PATH}: symlink to ${LINK}: was present and wrong, but now fixed."
            fi

        elif [ -e "${POOL_PATH}" ]; then
            if [ -d "${POOL_PATH}" ]; then
                # Not using -r because we don't want to remove unless empty.
                ${RMDIR} "${POOL_PATH}"
                RETURN=$?
                
                # .............................................................
                # 1d. Bad--> Good!  An empty directory, we can fix it.
                #
                if [ ${RETURN} -eq 0 ]; then
                    ${LN} -s "${LINK}" "${POOL_PATH}"
                    ${LOGGER} "Virt pool '${POOL}' (case 1d): ${POOL_PATH}: symlink to ${LINK}: was an empty directory, now removed and fixed."

                # .............................................................
                # 1e. Bad!  A non-empty directory.  Leave it alone.
                #
                # No need to print anything: rmdir already did.
                #
                else
                    FAILURE=1
                    ${LOGGER} "Virt pool '${POOL}' (case 1e): ${POOL_PATH}: symlink to ${LINK}: ERROR: was a non-empty directory!  Skipping pool."
                fi

            # .................................................................
            # 1f. Bad!  Neither a symlink nor a directory.  Leave it alone.
            #
            # We have to print something, since this was detected by a test.
            #
            else
                FAILURE=1
                ${LOGGER} "Virt pool '${POOL}' (case 1f): ${POOL_PATH}: symlink to ${LINK}: ERROR: was neither a symlink nor a directory!  Skipping pool."
                echo "virt_sync_pools.sh: ${POOL_PATH}: not a directory or a symlink"
            fi
            
        # .....................................................................
        # 1g. Good!  It does not exist, we can create it.
        #
        else
            ${LN} -s "${LINK}" "${POOL_PATH}"
            ${LOGGER} "Virt pool '${POOL}' (case 1g): ${POOL_PATH}: symlink to ${LINK}: did not exist, so now created."
        fi

        if [ ${FAILURE} -eq 0 ]; then
            ensure_dir "${POOL}" "${LINK}"
        fi
    fi

    if [ ${FAILURE} -eq 0 ]; then
        # Make sure all files in the pool have the right owner/group.mode.
        # We need -L to make it follow the symlink if there is one.
        ${FIND} -L ${POOL_PATH} \
            -type f -a \( ! -uid ${POOL_OWNER} -o ! -gid ${POOL_GROUP} -o ! -perm ${POOL_FILE_MODE} \) \
            -exec ${LOGGER} "Virt pool '${POOL}': fixing owner/group/mode of volume '{}'" ';' \
            -exec ${CHOWN} ${POOL_OWNER}.${POOL_GROUP} '{}' ';' \
            -exec ${CHMOD} ${POOL_FILE_MODE} '{}' ';'
        ${LOGGER} "Virt pool '${POOL}': success."
    else
        ${LOGGER} "Virt pool '${POOL}': failure."
        echo "Virtualization storage pool '${POOL}' failed setup."
        GLOBAL_FAILURE=1
    fi
done
# =============================================================================

if [ ${GLOBAL_FAILURE} -eq 0 ]; then
    success $"Virt pool sync"
else
    failure $"Virt pool sync"
fi

echo ""

exit ${GLOBAL_FAILURE}
