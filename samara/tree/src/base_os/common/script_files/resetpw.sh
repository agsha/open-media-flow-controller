#! /bin/sh

#
# (C) Copyright 2013-2015 Juniper Networks, Inc. All Rights reserved.
# (C) Copyright 2002-2010 Tall Maple Systems, Inc.
# All rights reserved.
# The contents of this file are subject to the terms of the MIT License.
# You may not use this file except in compliance with the License.
# See the file License.txt at the top of this source tree.
# You can obtain a copy of the license at https://opensource.org/licenses/MIT
#

PATH=/usr/bin:/bin:/usr/sbin:/sbin
export PATH

umask 0022


PW_BINDING=/auth/passwd/user/admin/password
AUTH_METHOD_BINDING=/aaa/auth_method/1/name
CONFIG_DIR=/config/db
ACTIVE_DB=${CONFIG_DIR}/active
INITIAL_DB=${CONFIG_DIR}/initial
IEXT="restore"
RESET_PW_DOTFILE=/var/opt/tms/.admin_pw_reset
PERMLOG=/var/log/systemlog

# This should only be run in single user mode, or at least with no pm or mgmtd
grep -q single /proc/cmdline
if [ $? != 0 ] ; then
    pm_pid="$(pidof pm)"
    mgmtd_pid="$(pidof mgmtd)"
    if [ ! -z "${pm_pid}" -o ! -z "${mgmtd_pid}" ]; then 
        echo "$0 may not be used on a running system"
        exit 1
    fi
fi

# If there is no 'active' db file, then make sure if there is an 'initial'
# db, we move it out of the way and force the system to create a clean db.
if [ ! -f ${ACTIVE_DB} ]; then
    if [ -f ${INITIAL_DB} ]; then
        mv -f ${INITIAL_DB} ${INITIAL_DB}.${IEXT}
        echo "No active db found, moving db '${INITIAL_DB}' to"
        echo "'${INITIAL_DB}.${IEXT}'. System will restore configuration"
        echo "state to initial values on restart."
    else
        echo "No active or initial db found.  System will restore"
        echo "configuration state to initial values on restart."
    fi
    exit 1
fi

ACTIVE_FN=${CONFIG_DIR}/`cat ${ACTIVE_DB}`

# Now check to make sure the db that active points to exists. If it
# doesn't, remove 'active'.
if [ ! -f "${ACTIVE_FN}" ]; then
    echo "Active db reference '${ACTIVE_FN}' does not exist."
    echo "The system will remove this reference and will restore"
    echo "configuration state to initial values on restart."
    rm -f ${ACTIVE_DB}

    if [ -f ${INITIAL_DB} ]; then
        mv -f ${INITIAL_DB} ${INITIAL_DB}.${IEXT}
        echo "Saving '${INITIAL_DB}' to '${INITIAL_DB}.${IEXT}'"
    fi
    exit 1
fi

# Now know that active exists and references a valid file, try to
# reset the password for admin. If this fails, save the active db
# and 'initial' if it exists out of the way

/opt/tms/bin/mddbreq "${ACTIVE_FN}" set modify "" ${PW_BINDING} string ""

if [ $? != 0 ] ; then
    echo "Password reset failed. Saving any old configuration dbs"

    rm -f ${ACTIVE_DB}
    mv -f "${ACTIVE_FN}" "${ACTIVE_FN}".${IEXT}
    echo "Saving '${ACTIVE_FN}' to '${ACTIVE_FN}.${IEXT}'"

    if [ -f ${INITIAL_DB} ]; then
        mv -f ${INITIAL_DB} ${INITIAL_DB}.${IEXT}
        echo "Saving '${INITIAL_DB}' to '${INITIAL_DB}.${IEXT}'"
    fi
    exit 1
fi

echo "Admin password reset successful."

# While the authentication method list may have been okay, explicitly
# make the first method be local in case this was screwed up.

/opt/tms/bin/mddbreq "${ACTIVE_FN}" set modify "" ${AUTH_METHOD_BINDING} string "local"

if [ $? != 0 ] ; then
    echo "Restoring first authentication method to local failed. Ignoring"
else
    echo "Modified first authentication method to 'local'."
fi

#
# Make sure this gets noticed.  We don't want to log to syslog now (and
# cannot do so via 'logger'), so we'll touch a file for md_passwd to 
# notice on next init.  But we will log directly to the systemlog, since
# other shell scripts do it (not ones that can run concurrently with us),
# and we want to make sure it gets in there.
#
touch ${RESET_PW_DOTFILE}
DATE=$(date "+%Y/%m/%d %H:%M:%S")
HOSTNAME=$(hostname | sed 's/\./ /' | awk '{print $1}')
printf "%s %s resetpw.sh: resetting admin password from single-user mode\n" "$DATE" "$HOSTNAME" >> $PERMLOG

exit 0
