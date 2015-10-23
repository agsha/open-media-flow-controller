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

PATH=/usr/bin:/bin:/usr/sbin:/sbin:/opt/tms/bin
export PATH

umask 0077

usage()
{
    echo "usage: $0 [-h] [-a] [-m mfg_prefix] <config db file path 1> [...]"
    echo "    -h: cleanse host keys"
    echo "    -a: cleanse everything that might need to be removed"
    echo "    -m MFG_PREFIX : db is a mfg db, and the given prefix should be"
    echo "        added before each node.  Typically used as '-m /mfg/mfdb' "
    echo "        Note that the unprefixed form is also cleansed."
    exit 1
}

MDDBREQ="/opt/tms/bin/mddbreq"
RM="/bin/rm"
EXPR="/usr/bin/expr"
LOGGER="/usr/bin/logger"

CLEANSE_ALL=0
CLEANSE_HOST_KEYS=0
CLEANSE_MFG=0
CLEANSE_MFG_PREFIX=

PARSE=`/usr/bin/getopt 'ham:' "$@"`

if [ $? != 0 ] ; then
    usage
fi

eval set -- "$PARSE"

while true ; do
    case "$1" in
        --)                           shift ; break ;;
        -a)  CLEANSE_ALL=1 ;          shift ;;
        -h)  CLEANSE_HOST_KEYS=1;     shift ;;
        -m)  CLEANSE_MFG=1; CLEANSE_MFG_PREFIX=$2;     shift 2 ;;
        *)   echo "cleanse_db.sh: parse failure" >&2 ; usage ;;
    esac
done

# Define any graft points
if [ -f /etc/customer.sh ]; then
    . /etc/customer.sh
fi

cleanse()
{
    DBPATH="$1"
    IDX="$2"

    NODES_FILE=/tmp/cleanse_db_tmp.$$.${IDX}
    NODES_FILE_PREFIX=/tmp/cleanse_db_tmp.$$.pfx.${IDX}

    ${RM} -f ${NODES_FILE} ${NODES_FILE_PREFIX}
    touch ${NODES_FILE} ${NODES_FILE_PREFIX}

    #
    # NOTE: we pass "-q" to mddbreq so it won't complain about non-db files.
    # We know we'll at least have that case with /config/db/active, but
    # we might also with others.  We aren't here to police what goes into
    # /config/db, we only want to operate on database files and ignore
    # the others.
    #
    # XXX/EMT: it would be better to skip calling it for "active", rather
    # than letting mddbreq fail out.
    #

    #
    # Cleanse host keys, if requested.
    #
    if [ ${CLEANSE_ALL} -eq 1 -o ${CLEANSE_HOST_KEYS} -eq 1 ]; then
        echo "/ssh/server/hostkey/private/rsa1" >> ${NODES_FILE}
        echo "/ssh/server/hostkey/private/rsa2" >> ${NODES_FILE}
        echo "/ssh/server/hostkey/private/dsa2" >> ${NODES_FILE}
    fi

    #
    # Cleanse certificate private keys, if requested (follow the -h option
    # since these are really another kind of host key).
    #
    if [ ${CLEANSE_ALL} -eq 1 -o ${CLEANSE_HOST_KEYS} -eq 1 ]; then
        CERTS=`${MDDBREQ} -v -q ${DBPATH} query iterate - /certs/config/certs`
        for CERT in ${CERTS}; do
            echo "/certs/config/certs/${CERT}/priv_key" >> ${NODES_FILE}
        done
    fi

    #
    # Cleanse all other stuff, if requested.
    #
    if [ ${CLEANSE_ALL} -eq 1 ]; then
        echo "/system/bootmgr/password" >> ${NODES_FILE}

        #
        # XXX/EMT: is there a limit to the length of the contents of a 
        # bash variable?  What if there are a lot of users?  I have tried
        # this with ~2200 results, full binding names instead of just 
        # usernames, and it worked fine.  That probably equates to around
        # 10000 usernames.
        #
        USERS=`${MDDBREQ} -v -q ${DBPATH} query iterate - /auth/passwd/user`
        for USER in ${USERS}; do
            echo "/auth/passwd/user/${USER}/password" >> ${NODES_FILE}
        done

        KEYS=`${MDDBREQ} -v -q ${DBPATH} query iterate - /license/key`
        for KEY in ${KEYS}; do
            echo "/license/key/${KEY}/license_key" >> ${NODES_FILE}
        done

        #
        # Graft point #1: other node deletions.  To delete other nodes,
        # append names to the file named by ${NODES_FILE} as we do above.
        # To query the database (to find out what nodes to delete), 
        # invoke ${MDDBREQ} as we do above.
        #
        if [ "$HAVE_CLEANSE_DB_GRAFT_1" = "y" ]; then
            cleanse_db_graft_1
        fi
    fi

    if [ "${CLEANSE_MFG}" -eq 1 -a ! -z "${CLEANSE_MFG_PREFIX}" ]; then
        cat ${NODES_FILE} | awk '{print "'"${CLEANSE_MFG_PREFIX}"'" $_}' >> ${NODES_FILE_PREFIX}
        ${MDDBREQ} -F ${NODES_FILE_PREFIX} -q -b ${DBPATH} set delete -

    fi

    ${MDDBREQ} -F ${NODES_FILE} -q -b ${DBPATH} set delete -

    ${RM} -f ${NODES_FILE} ${NODES_FILE_PREFIX}
}

IDX=0
while true ; do
    if [ -z "$1" ]; then
        break
    fi

    IDX=`${EXPR} ${IDX} + 1`
    cleanse $1 ${IDX}
    shift
done
