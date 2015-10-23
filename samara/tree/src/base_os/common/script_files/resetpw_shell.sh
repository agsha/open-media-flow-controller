#!/bin/sh

#
# (C) Copyright 2013-2015 Juniper Networks, Inc. All Rights reserved.
# (C) Copyright 2002-2010 Tall Maple Systems, Inc.
# All rights reserved.
# The contents of this file are subject to the terms of the MIT License.
# You may not use this file except in compliance with the License.
# See the file License.txt at the top of this source tree.
# You can obtain a copy of the license at https://opensource.org/licenses/MIT
#

# ============================================================================
# This script is intended to rescue a user who has forgotten the admin
# password and/or set a non-local authentication method that is not 
# working and is interfering with login.
#
# The script first verifies that it is running on the console, to limit
# exposure to outside users.  If it is, it resets the admin account
# password to the empty string, and makes sure that 'local' is the
# primary authentication method.  It is generally intended to be used as
# the login shell for a rescue account.
#
# (XXX/EMT: resetting the password to the factory default may be
# preferable, but this is not as easy as it sounds since the 'reset'
# request resets a node to its registered initial value, which in the
# case of /auth/passwd/user/*/password is '*' (password login disabled).
# The fact that the initial db may have a different value in it does not
# come into play.
# ============================================================================

PATH=/usr/bin:/bin:/usr/sbin:/sbin
export PATH

umask 0022

. /etc/build_version.sh

MDREQ=/opt/tms/bin/mdreq

PW_USER=admin
PW_NEW_PASS=
# Set this to 1 to ignore PW_NEW_PASS and instead reset to the default
PW_RESET_TO_DEFAULT=0
PW_BINDING_PRE_USER=/auth/passwd/user/
PW_BINDING_POST_USER=/password
AUTH_METHOD_BINDING=/aaa/auth_method/1/name
AUTH_METHOD_VALUE=local

# Choose if we use 'read -e' (readline)
if [ ! -z "${BASH}" ]; then
    READ_CMD="read -e"
else
    READ_CMD="read"
fi

#
# Set this to 0 to skip the step of resetting the primary auth method
# to 'local'.  We'll still reset the password in either case.
#
RESET_AUTH_METHOD=1

VERBOSE=0

LOG_COMPONENT="resetpw_shell"
LOG_TAG="${LOG_COMPONENT}[$$]"
# LOG_LEVEL is uppercase, facility lower
LOG_FACILITY=auth
LOG_LEVEL=NOTICE

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
    fi
}

# ==================================================
# Make sure we have a license
# ==================================================
check_license()
{
    FAILURE=0
    BNAME="/license/feature/${LICENSE_FEATURE}/status/active"
    HAVE_LIC=`${MDREQ} -v query get - ${BNAME} 2>&1` || FAILURE=1
    if [ ${FAILURE} -eq 0 -a "x${HAVE_LIC}" = "xtrue" ]; then
        return 0
    fi

    echo "Must have active license for ${LICENSE_FEATURE} feature."
    echo -n "Enter license key: "
    ${READ_CMD} LICENSE_KEY
    LIC_ACTIVE=`${MDREQ} -l action /license/actions/decode license_key string "${LICENSE_KEY}" 2>&1 | grep "Name: active" | awk '{print $8}'` || FAILURE=1
    LIC_FEATURE=`${MDREQ} -l action /license/actions/decode license_key string "${LICENSE_KEY}" 2>&1 | grep "Name: feature" | awk '{print $8}'` || FAILURE=1

    if [ ${FAILURE} -eq 0 -a "x${LIC_ACTIVE}" = "xfalse" ]; then
        vlechoerr -1 "Invalid or inactive license key.  Cannot reset password."
        sleep 5
        exit 1
    fi

    if [ "x${LIC_FEATURE}" != "x${LICENSE_FEATURE}" ]; then
        vlechoerr -1 "License key is for the wrong feature.  Cannot reset password."
        sleep 5
        exit 1
    fi

    return 0
}

# Define graft functions
if [ -f /etc/customer.sh ]; then
    . /etc/customer.sh
fi

# ==================================================
# This is only allowed to be run from the serial console or physical console
# ==================================================
FAILURE=0
this_tty=`tty` || FAILURE=1
if [ ${FAILURE} -eq 1 -o -z "${this_tty}" ]; then
    vlechoerr -1 "$0: could not find tty"
    sleep 5
    exit 1
fi

# We hard code this instead of looking at /etc/securetty as that file usually
# has other things in it we are not willing to trust for this purpose.
is_console=0
if [ "${this_tty}" = "/dev/ttyS0" -o "${this_tty}" = "/dev/ttyS1" \
    -o "${this_tty}" = "/dev/console" \
    -o "${this_tty}" = "/dev/tty1" -o "${this_tty}" = "/dev/tty2" \
    -o "${this_tty}" = "/dev/tty3" -o "${this_tty}" = "/dev/tty4" \
    -o "${this_tty}" = "/dev/tty5" -o "${this_tty}" = "/dev/tty6" ]; then

    is_console=1

fi

if [ ${is_console} -ne 1 ]; then
    vlechoerr -1 "$0: not allowed from tty ${this_tty}"
    sleep 5
    exit 1
fi

CHECK_LICENSE=0
LICENSE_FEATURE=RESET_PASSWORD

# =============================================================================
# Graft point 1: allow customer to change:
#
#   - Password settings (set PW_USER, PW_NEW_PASS, etc.)  Note that 
#     if PW_NEW_PASS is not empty, it must be a hashed version of the 
#     password, not plaintext.  To get the hashed version, set the password
#     you want on a test system, and then do from the CLI (substituting the
#     name of the user you chose for "USER"):
#
#        internal q g - /auth/passwd/user/USER/password
#
#   - Whether or not we check for a license, e.g. make it conditional
#     on BUILD_PROD_FEATURE_RESTRICT_CMDS.  (Set CHECK_LICENSE to 1 
#     to do the check.)
#
#   - The name of the feature to require a license for, if we are 
#     checking for a license.  (Set LICENSE_FEATURE to the name of the
#     feature to require.  Note that a mgmtd module must register this
#     licensed feature with mdm_license_add_licensed_feature())
#
#   - etc.
# =============================================================================
if [ "$HAVE_RESETPW_SHELL_GRAFT_1" = "y" ]; then
    resetpw_shell_graft_1
fi

if [ "x$CHECK_LICENSE" = "x1" ]; then
    check_license
fi

PW_BINDING=${PW_BINDING_PRE_USER}${PW_USER}${PW_BINDING_POST_USER}

# =============================================================================
# Step 1: reset the password.
# =============================================================================

if [ ${PW_RESET_TO_DEFAULT} -eq 0 ]; then
    if [ ! -z "${PW_NEW_PASS}" ]; then
        vlecho -1 "Attempting to reset password for '${PW_USER}'"
    else
        vlecho -1 "Attempting to reset password for '${PW_USER}' to empty string"
    fi
else
    vlecho -1 "Attempting to reset password for '${PW_USER}' to default"
fi

sync
sleep 1

FAILURE=0
OPS=""
if [ ${PW_RESET_TO_DEFAULT} -eq 0 ]; then
    OPS=`${MDREQ} set modify "" ${PW_BINDING} string "${PW_NEW_PASS}" 2>&1` || FAILURE=1
else
    OPS=`${MDREQ} set reset "" ${PW_BINDING} 2>&1` || FAILURE=1
fi

if [ ${FAILURE} -eq 1 -o ! -z "${OPS}" ]; then
    vlechoerr -1 "${OPS}"
    vlechoerr -1 "$0: could not reset password"
    sleep 5
    exit 1
fi

vlecho -1 "Reset password for '${PW_USER}'"


# =============================================================================
# Step 2: reset the primary auth method, if RESET_AUTH_METHOD is set.
# =============================================================================

if [ ${RESET_AUTH_METHOD} -eq 1 ]; then
    vlecho -1 "Attempting to reset primary auth method to ${AUTH_METHOD_VALUE}"
    sync
    sleep 1

    FAILURE=0
    OPS=""
    OPS=`${MDREQ} set modify "" ${AUTH_METHOD_BINDING} string "${AUTH_METHOD_VALUE}" 2>&1` || FAILURE=1

    if [ ${FAILURE} -eq 1 -o ! -z "${OPS}" ]; then
        vlechoerr -1 "${OPS}"
        vlechoerr -1 "$0: could not reset auth method"
        # Don't exit here, we still did the password at least.
    else
        vlecho -1 "Reset primary auth method to ${AUTH_METHOD_VALUE}"
    fi
fi


# =============================================================================
# Step 3: save the config and exit.
# =============================================================================

vlecho -1 "Saving configuration"

FAILURE=0
OPS=""
OPS=`/opt/tms/bin/mdreq action /mgmtd/db/save 2>&1` || FAILURE=1

if [ ${FAILURE} -eq 1 -o ! -z "${OPS}" ]; then
    vlechoerr -1 "${OPS}"
    vlechoerr -1 "$0: could not save config"
    sleep 5
    exit 1
fi

vlecho -1 "Password reset for '${PW_USER}' complete"

# =============================================================================
# Step 4: log to systemlog, reporting the password reset
#
# XXX/EMT: maybe we should report some of the failure cases above?
# =============================================================================

FAILURE=0
OPS=`${MDREQ} action /logging/actions/systemlog/log message string "resetpw_shell.sh: reset password for user: ${PW_USER}"` || FAILURE=1

if [ ${FAILURE} -eq 1 -o ! -z "${OPS}" ]; then
    vlechoerr -1 "${OPS}"
    vlechoerr -1 "$0: could not report password reset for user ${PW_USER} to systemlog"
    sleep 5
    exit 1
fi

sync

exit 0
