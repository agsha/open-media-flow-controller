#!/bin/sh

#
# (C) Copyright 2013-2015 Juniper Networks, Inc. All Rights reserved.
# (C) Copyright 2002-2009 Tall Maple Systems, Inc.
# All rights reserved.
# The contents of this file are subject to the terms of the MIT License.
# You may not use this file except in compliance with the License.
# See the file License.txt at the top of this source tree.
# You can obtain a copy of the license at https://opensource.org/licenses/MIT
#

PATH=/usr/bin:/bin:/usr/sbin:/sbin:/opt/tms/bin
export PATH

umask 0022

usage()
{
    echo "Usage: $0 [-v] -u URL -o OUTPUT_FILE"
    echo ""
    echo "-u: URL to download.  Should start with 'http://localhost/'"
    echo ""
    echo "-o: output file location.  The file is removed before fetch"
    echo ""
    echo "Example usage:"
    echo "  $0 -o /tmp/myoutput.html -u 'http://localhost/admin/launch?script=rh&template=mon-summary'"
    echo ""
    exit 1
}

PARSE=`/usr/bin/getopt -s sh 'vu:o:' "$@"`

if [ $? != 0 ] ; then
    usage
fi

eval set -- "$PARSE"

VERBOSE=0
URL=
OUTPUT_FILE=

WUSER=$(id -u -n)

#
# If LOGIN_USER is set, that means that we were remote-authenticated,
# and it holds our remote username.  When the GCL is deciding what
# username it should call us when we open the session with wsmd, 
# this is the first env var it looks at, since the session normally
# wants to go by its original (remote) username.  But the username
# we tell wsmd is the local mapped user (gleaned from 'id -u -n'),
# as it must be, because wsmd looks it up with getpwnam().  And if
# those two usernames don't match, wsmd will not authenticate us.
#
# By setting this environment variable, we are "tricking" the GCL
# into announcing us as the local mapped user, rather than the 
# remote user.  This was not the GCL's real security mechanism 
# (a good thing, since it is so easy to forge!) -- in reality, it
# depends on the gid (and uid).  It would be somewhat more elegant
# to have a way to tell the GCL what username you want to appear as,
# and thread it all the way through to mdreq, rather than hijacking
# this environment variable.  But this is a simpler and more 
# localized way to achieve the same effect.
#
# Because we are running in a new shell process, the polluted env var
# will not live beyond this script.
#
# Do NOT do this in other places without careful consideration!
#
if [ ! -z "${LOGIN_USER}" ]; then
    LOGIN_USER="${WUSER}"
fi

while true ; do
    case "$1" in
        -v) VERBOSE=$((${VERBOSE}+1)); shift ;;
        -u) URL=$2; shift 2 ;;
        -o) OUTPUT_FILE=$2; shift 2 ;;
        --) shift ; break ;;
        *) echo "$0: parse failure" >&2 ; usage ;;
    esac
done

if [ -z "${URL}" -o -z "${OUTPUT_FILE}" ]; then
    usage
fi

FULL_COOKIE=
ECOOKIE=
UCOOKIE=

##################################################
# Awk function to unescape url escaped cookie string
UN_URL_ESCAPE='
function unescape(s) {
 rs="";
 for (i=1; i<=length(s); i++) {
    c=substr(s,i,1);
    if (c=="%") {
      hexstr="0x" substr(s,i+1,2);
      rs= sprintf("%s%c", rs, strtonum(hexstr) + 0);
      i= i + 2;
    }
    else if (c=="+") {
      rs= rs " ";
    } else {
      rs= rs c;
    }
  }
 return rs;
}
{
  printf "%s\n", unescape($0);
}
'
##################################################

# Echo or not based on VERBOSE setting
vecho()
{
    level=$1
    shift

    if [ ${VERBOSE} -gt ${level} ]; then
        echo "$*"
    fi
}


cleanup()
{
    if [ ! -z "${UCOOKIE}" ]; then
        do_logout
    fi
}

cleanup_exit()
{
    cleanup
    vecho 0 "- ERROR: exiting"
    exit 1
}


do_login()
{
    # Do login
    vecho 0 "- Logging in to local web server as user '${WUSER}'"
    FAILURE=0
    RAW_COOKIE_RETURN=$(mdreq -v -c wsmd action /wsm/auth/login/basic user_id string "${WUSER}" remote_addr string "localhost") || FAILURE=1

    # Parse login return
    FULL_COOKIE=$(echo "${RAW_COOKIE_RETURN}" | grep '^Set-Cookie: ' | sed 's/^Set-Cookie: \([^; ]*\).*/\1/')
    ECOOKIE=$(echo "${FULL_COOKIE}"  | sed 's/.*session=\([^;]*\)/\1/')
    UCOOKIE=$(echo "${ECOOKIE}" | awk -- "${UN_URL_ESCAPE}" -)

    if [ ${FAILURE} -ne 0 -o -z "${ECOOKIE}" -o -z "${FULL_COOKIE}" -o -z "${UCOOKIE}" ]; then
        return 0
    else
        return 1
    fi
}

do_logout() {
    vecho 0 "- Logging out of local web server"
    FAILURE=0
    LOGOUT_RETURN=$(mdreq -v -c wsmd action /wsm/auth/logout/basic cookie string "${UCOOKIE}") || FAILURE=1

    if [ ${FAILURE} -ne 0 ]; then
        return 0
    else 
        return 1
    fi
    
}

# ==================================================

trap "cleanup_exit" HUP INT QUIT PIPE TERM

rm -f ${OUTPUT_FILE}

# Login, get a cookie
if do_login ; then
    exit 1
fi

# Get web page
vecho 0 "- Retrieving web page: '${URL}'"
FAILURE=0
curl -f -s --output ${OUTPUT_FILE} --cookie "${FULL_COOKIE}" "${URL}" || FAILURE=1
if [ ${FAILURE} -ne 0 ]; then
    vecho 0 "- ERROR: could not retrieve web page"
    cleanup_exit
fi

# Logout, invalidate the cookie
if do_logout ; then
    exit 1
fi

vecho 0 "- Done"

exit 0
# ==================================================
