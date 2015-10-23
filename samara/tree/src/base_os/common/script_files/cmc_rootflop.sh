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
# This file contains definitions and graft functions specific to the
# CMC (Central Management Console).  It is included, and its functions
# called, by opt_feature_rootflop.sh.
#
# It will be present on any system which has either the CMC client or
# the CMC server feature enabled (or both).  Thus we have to be able to
# check which of the client and the server are actually available.

. /etc/build_version.sh

# -----------------------------------------------------------------------------
HAVE_CMC_MANUFACTURE_GRAFT_1=y
cmc_manufacture_graft_1()
{
    OPT_CMC_SERVER_DEF="yes"
    OPT_CMC_SERVER_LICENSE_DEF=""
    OPT_CMC_CLIENT_DEF="yes"
    OPT_CMC_CLIENT_LICENSE_DEF=""
    OPT_CMC_CLIENT_AUTO_RENDV_DEF="no"
    OPT_CMC_CLIENT_SERVER_ADDR_DEF="cmc"

    # For client to authenticate to server for rendezvous
    # (And for server to initialize cmcrendv account)
    OPT_CMC_SERVER_AUTHTYPE_DEF="none"
    OPT_CMC_SERVER_PASSWORD_DEF=""

    # For server to authenticate to client for login
    # (And for client to initialize admin account)
    OPT_CMC_CLIENT_AUTHTYPE_DEF="password"
    OPT_CMC_CLIENT_SET_PASSWORD_DEF="yes"
    OPT_CMC_CLIENT_PASSWORD_DEF=""

    OPT_CMC_SERVICE_NAME_DEF="cmc"
}


# -----------------------------------------------------------------------------
HAVE_CMC_MANUFACTURE_GRAFT_2=y
cmc_manufacture_graft_2()
{
    # .........................................................................
    # CMC server enable
    #
    # Set both the available and enable flag from each choice.
    # If the CMC is not licensed, this is the correct thing to do.
    # If it is licensed, the "available" setting will be overridden
    # by the license module, but our setting it does no harm.
    if [ "x$BUILD_PROD_FEATURE_CMC_SERVER" = "x1" ]; then
        ${MDDBREQ} -c ${MFG_DB_PATH} set modify - /mfg/mfdb/cmc/config/available bool ${CFG_CMC_SERVER}
        ${MDDBREQ} -c ${MFG_INC_DB_PATH} set modify - /cmc/config/enable bool ${CFG_CMC_SERVER}
    fi

    # .........................................................................
    # CMC server license
    #
    # We don't know what other licenses may already have been added,
    # so figure out the first unused slot and use that.
    if [ "x$CFG_CMC_SERVER_LICENSE" != "x" ]; then
        GFU_DB_PATH="${MFG_INC_DB_PATH}"
        GFU_PARENT_NODE="/license/key"
        GFU_START_INDEX="1"
        get_first_unused_idx
        ${MDDBREQ} -c ${MFG_INC_DB_PATH} set modify - /license/key/${GFU_RESULT} uint32 ${GFU_RESULT}
        ${MDDBREQ} -c ${MFG_INC_DB_PATH} set modify - /license/key/${GFU_RESULT}/license_key string ${CFG_CMC_SERVER_LICENSE}
    fi

    # .........................................................................
    # CMC client enable
    #
    if [ "x$BUILD_PROD_FEATURE_CMC_CLIENT" = "x1" ]; then
        ${MDDBREQ} -c ${MFG_DB_PATH} set modify - /mfg/mfdb/cmc/client/config/available bool ${CFG_CMC_CLIENT}
        ${MDDBREQ} -c ${MFG_INC_DB_PATH} set modify - /cmc/client/config/enable bool ${CFG_CMC_CLIENT}
    fi

    # .........................................................................
    # CMC client license
    #
    if [ "x$CFG_CMC_CLIENT_LICENSE" != "x" ]; then
        GFU_DB_PATH="${MFG_INC_DB_PATH}"
        GFU_PARENT_NODE="/license/key"
        GFU_START_INDEX="1"
        get_first_unused_idx
        ${MDDBREQ} -c ${MFG_INC_DB_PATH} set modify - /license/key/${GFU_RESULT} uint32 ${GFU_RESULT}
        ${MDDBREQ} -c ${MFG_INC_DB_PATH} set modify - /license/key/${GFU_RESULT}/license_key string ${CFG_CMC_CLIENT_LICENSE}
    fi

    # .........................................................................
    # CMC client auto-rendezvous and server address
    #
    if [ "x$BUILD_PROD_FEATURE_CMC_CLIENT" = "x1" ]; then
        ${MDDBREQ} -c ${MFG_INC_DB_PATH} set modify - /cmc/client/config/rendezvous/auto/enable bool ${CFG_CMC_CLIENT_AUTO_RENDV}
        ${MDDBREQ} -c ${MFG_INC_DB_PATH} set modify - /cmc/client/config/rendezvous/server_addr hostname "${CFG_CMC_CLIENT_SERVER_ADDR}"
    fi

    # .........................................................................
    # Credentials for client to log into server for rendezvous: none
    #
    if [ "x$CFG_CMC_SERVER_AUTHTYPE" = "xnone" ]; then
        if [ "x$CFG_CMC_SERVER" = "xtrue" ]; then
            CRYPTED_PASSWD="*"
            ${MDDBREQ} -c ${MFG_INC_DB_PATH} set modify - /auth/passwd/user/cmcrendv/password string "${CRYPTED_PASSWD}"
        fi
    fi

    # .........................................................................
    # Credentials for client to log into server for rendezvous: password
    #
    if [ "x$CFG_CMC_SERVER_AUTHTYPE" = "xpassword" ]; then
        if [ "x$CFG_CMC_CLIENT" = "xtrue" ]; then
            ${MDDBREQ} -c ${MFG_INC_DB_PATH} set modify - /cmc/client/config/rendezvous/server_auth/password/password string "${CFG_CMC_SERVER_PASSWORD}"
        fi
        if [ "x$CFG_CMC_SERVER" = "xtrue" ]; then
            if [ "x$CFG_CMC_SERVER_PASSWORD" != "x" ]; then
                CRYPTED_PASSWD=`${CRYPT} -- ${CFG_CMC_SERVER_PASSWORD}`
            else
                CRYPTED_PASSWD=""
            fi
            ${MDDBREQ} -c ${MFG_INC_DB_PATH} set modify - /auth/passwd/user/cmcrendv/password string "${CRYPTED_PASSWD}"
        fi
    fi

    # .........................................................................
    # Credentials for client to log into server for rendezvous: DSA2
    #
    # XXX/EMT: in other areas of our system, the public and private keys
    # both have newlines at the end.  We do not add them here.  This does
    # not affect their functionality but may make "show" commands look
    # weird or inconsistent.
    #

    if [ "x$CFG_CMC_SERVER_AUTHTYPE" = "xssh-dsa2" ]; then
        if [ "x$CFG_CMC_CLIENT" = "xtrue" ]; then
            ${MDDBREQ} -c ${MFG_INC_DB_PATH} set modify - /cmc/client/config/rendezvous/server_auth/type string "ssh-dsa2"
            ${MDDBREQ} -c ${MFG_INC_DB_PATH} set modify - /ssh/client/username/cmcrendv string "cmcrendv"
            ${MDDBREQ} -c ${MFG_INC_DB_PATH} set modify - /ssh/client/username/cmcrendv/keytype/dsa2/public string "${CFG_CMC_CLIENT_PUBKEY_DSA}"
            ${MDDBREQ} -c ${MFG_INC_DB_PATH} set modify - /ssh/client/username/cmcrendv/keytype/dsa2/private string "${CFG_CMC_CLIENT_PRIVKEY_DSA}"
        fi
        if [ "x$CFG_CMC_SERVER" = "xtrue" ]; then
            GFU_DB_PATH="${MFG_INC_DB_PATH}"
            GFU_PARENT_NODE="/ssh/server/username/cmcrendv/auth-key/sshv2"
            GFU_START_INDEX="1"
            get_first_unused_idx
            ${MDDBREQ} -c ${MFG_INC_DB_PATH} set modify - /ssh/server/username/cmcrendv string "cmcrendv"
            ${MDDBREQ} -c ${MFG_INC_DB_PATH} set modify - /ssh/server/username/cmcrendv/auth-key/sshv2/${GFU_RESULT} uint32 ${GFU_RESULT}
            ${MDDBREQ} -c ${MFG_INC_DB_PATH} set modify - /ssh/server/username/cmcrendv/auth-key/sshv2/${GFU_RESULT}/public string "${CFG_CMC_CLIENT_PUBKEY_DSA}"
        fi
    fi

    # ........................................................................
    # Disable the cmcadmin account on a system that is a server but
    # not a client.
    #
    # XXX/EMT: this account is disabled everywhere for the time being
    #
    #if [ "x$CFG_CMC_SERVER" = "xtrue" ] && [ "x$CFG_CMC_CLIENT " != "xtrue" ] ; then
    #    ${MDDBREQ} -c ${MFG_INC_DB_PATH} set modify - /auth/passwd/user/cmcadmin/enable bool false
    #fi

    # .........................................................................
    # Credentials for server to log into new client: password
    #
    if [ "x$CFG_CMC_CLIENT_AUTHTYPE" = "xpassword" ]; then
        if [ "x$CFG_CMC_SERVER" = "xtrue" ]; then
            ${MDDBREQ} -c ${MFG_INC_DB_PATH} set modify - /cmc/config/rendezvous/auth/default/password/password string "${CFG_CMC_CLIENT_PASSWORD}"
        fi

        if [ "x$CFG_CMC_CLIENT" = "xtrue" ] && [ "x$CFG_CMC_CLIENT_SET_PASSWORD" = "xtrue" ] ; then
            if [ "x$CFG_CMC_CLIENT_PASSWORD" != "x" ]; then
                CRYPTED_PASSWD=`${CRYPT} -- ${CFG_CMC_CLIENT_PASSWORD}`
            else
                CRYPTED_PASSWD=""
            fi
            ${MDDBREQ} -c ${MFG_INC_DB_PATH} set modify - /auth/passwd/user/admin/password string "${CRYPTED_PASSWD}"
        fi
    fi

    # .........................................................................
    # Credentials for server to log into new client: DSA2
    #
    if [ "x$CFG_CMC_CLIENT_AUTHTYPE" = "xssh-dsa2" ]; then
        if [ "x$CFG_CMC_SERVER" = "xtrue" ]; then
            ${MDDBREQ} -c ${MFG_INC_DB_PATH} set modify - /cmc/config/rendezvous/auth/default/type string "ssh-dsa2"
            ${MDDBREQ} -c ${MFG_INC_DB_PATH} set modify - /cmc/config/rendezvous/auth/default/ssh-dsa2/identity string "client-default"
            ${MDDBREQ} -c ${MFG_INC_DB_PATH} set modify - /cmc/common/config/auth/ssh-dsa2/identity/client-default string "client-default"
            ${MDDBREQ} -c ${MFG_INC_DB_PATH} set modify - /cmc/common/config/auth/ssh-dsa2/identity/client-default/public string "${CFG_CMC_SERVER_PUBKEY_DSA}"
            ${MDDBREQ} -c ${MFG_INC_DB_PATH} set modify - /cmc/common/config/auth/ssh-dsa2/identity/client-default/private string "${CFG_CMC_SERVER_PRIVKEY_DSA}"
        fi
        if [ "x$CFG_CMC_CLIENT" = "xtrue" ]; then
            GFU_DB_PATH="${MFG_INC_DB_PATH}"
            GFU_PARENT_NODE="/ssh/server/username/admin/auth-key/sshv2"
            GFU_START_INDEX="1"
            get_first_unused_idx
            ${MDDBREQ} -c ${MFG_INC_DB_PATH} set modify - /ssh/server/username/admin string "admin"
            ${MDDBREQ} -c ${MFG_INC_DB_PATH} set modify - /ssh/server/username/admin/auth-key/sshv2/${GFU_RESULT} uint32 ${GFU_RESULT}
            ${MDDBREQ} -c ${MFG_INC_DB_PATH} set modify - /ssh/server/username/admin/auth-key/sshv2/${GFU_RESULT}/public string "${CFG_CMC_SERVER_PUBKEY_DSA}"
        fi
    fi

    # .........................................................................
    # CMC service name
    #
    if [ "x$CFG_CMC_SERVICE_NAME" != "x" ]; then
        ${MDDBREQ} -c ${MFG_INC_DB_PATH} set modify - /cmc/common/config/service_name hostname ${CFG_CMC_SERVICE_NAME}
    fi
}

# -----------------------------------------------------------------------------
HAVE_CMC_MANUFACTURE_GRAFT_3=y
cmc_manufacture_graft_3()
{
    if [ "x$BUILD_PROD_FEATURE_CMC_SERVER" = "x1" ]; then
        if [ "x$BUILD_PROD_FEATURE_CMC_SERVER_LICENSED" = "x1" ]; then
            echo "       [--cs {yes|no}] [--csl <license key>]"
        else
            echo "       [--cs {yes|no}]"
        fi
    fi
    if [ "x$BUILD_PROD_FEATURE_CMC_CLIENT" = "x1" ]; then
        if [ "x$BUILD_PROD_FEATURE_CMC_CLIENT_LICENSED" = "x1" ]; then
            echo "       [--cc {yes|no}] [--ccl <license key>] "
        else
            echo "       [--cc {yes|no}]"
        fi
        echo "       [--cca {yes|no}] [--ccd <server addr>]"
    fi
    echo "       [--cst {password|ssh-dsa2}] [--csp <password>] "
    echo "       [--cct {password|ssh-dsa2}] [--ccsp {yes|no}] [--ccp <password>]"
    echo "       [--csn <CMC service name>]"
}

# -----------------------------------------------------------------------------
HAVE_CMC_MANUFACTURE_GRAFT_3b=y
cmc_manufacture_graft_3b()
{
    if [ "x$BUILD_PROD_FEATURE_CMC_SERVER" = "x1" ]; then
        echo ""
        echo "--cs {yes|no}: enable CMC server functionality"
        if [ "x$BUILD_PROD_FEATURE_CMC_SERVER_LICENSED" = "x1" ]; then
            echo "--csl <license key>: enter license key to enable CMC server"
        fi
    fi
    if [ "x$BUILD_PROD_FEATURE_CMC_CLIENT" = "x1" ]; then
        echo ""
        echo "--cc {yes|no}: enable CMC client functionality"
        if [ "x$BUILD_PROD_FEATURE_CMC_CLIENT_LICENSED" = "x1" ]; then
            echo "--ccl <license key>: enter license key to enable CMC client"
        fi
        echo "--cca {yes|no}: enable automatic rendezvous to CMC server"
        echo "--ccd <server addr>: set address of CMC server for rendezvous"
    fi
    echo ""
    echo "--cst {password|ssh-dsa2}: CMC server authentication type for rendezvous"
    echo "                           (for client to announce itself to server)"
    echo "--csp <password>: CMC server password for rendezvous account"
    echo "                  (if password authtype selected)"
    echo "--cct {password|ssh-dsa2}: CMC client authentication type for admin account"
    echo "                           (for server to log into new client)"
    echo "--ccsp {yes|no}: Should we set the admin password on this system if it is"
    echo "                 a CMC client?  If yes, whatever is specified here will"
    echo "                 override system default admin password."
    echo "--ccp <password>: CMC client password for admin account"
    echo "                  (if password authtype selected)"
    echo "--csn <CMC service name>: string that must match on CMC client and server"
}

# -----------------------------------------------------------------------------
HAVE_CMC_MANUFACTURE_GRAFT_4=y
cmc_manufacture_graft_4()
{
    if [ "x$BUILD_PROD_FEATURE_CMC_SERVER" = "x1" ]; then
        if [ "x$GETOPT_LONGOPTS" != "x" ]; then
            GETOPT_LONGOPTS="${GETOPT_LONGOPTS},"
        fi
        GETOPT_LONGOPTS="${GETOPT_LONGOPTS}cs:"
        if [ "x$BUILD_PROD_FEATURE_CMC_SERVER_LICENSED" = "x1" ]; then
            GETOPT_LONGOPTS="${GETOPT_LONGOPTS},csl:"
        fi
        HAVE_OPT_CMC_SERVER=0
        OPT_CMC_SERVER=
        HAVE_OPT_CMC_SERVER_LICENSE=0
        OPT_CMC_SERVER_LICENSE=
    fi
    if [ "x$BUILD_PROD_FEATURE_CMC_CLIENT" = "x1" ]; then
        if [ "x$GETOPT_LONGOPTS" != "x" ]; then
            GETOPT_LONGOPTS="${GETOPT_LONGOPTS},"
        fi
        GETOPT_LONGOPTS="${GETOPT_LONGOPTS}cc:,cca:,ccd:"
        if [ "x$BUILD_PROD_FEATURE_CMC_CLIENT_LICENSED" = "x1" ]; then
            GETOPT_LONGOPTS="${GETOPT_LONGOPTS},ccl:"
        fi
        HAVE_OPT_CMC_CLIENT=0
        OPT_CMC_CLIENT=
        HAVE_OPT_CMC_CLIENT_LICENSE=0
        OPT_CMC_CLIENT_LICENSE=
        HAVE_OPT_CMC_CLIENT_AUTO_RENDV=0
        OPT_CMC_CLIENT_AUTO_RENDV=
        HAVE_OPT_CMC_CLIENT_SERVER_ADDR=0
        OPT_CMC_CLIENT_SERVER_ADDR=
    fi
    GETOPT_LONGOPTS="${GETOPT_LONGOPTS},cst:,csp:,cct:,ccsp:,ccp:,csn:"
    HAVE_OPT_CMC_SERVER_AUTHTYPE=0
    OPT_CMC_SERVER_AUTHTYPE=
    HAVE_OPT_CMC_SERVER_PASSWORD=0
    OPT_CMC_SERVER_PASSWORD=
    HAVE_OPT_CMC_CLIENT_AUTHTYPE=0
    OPT_CMC_CLIENT_AUTHTYPE=
    HAVE_OPT_CMC_CLIENT_SET_PASSWORD=0
    OPT_CMC_CLIENT_SET_PASSWORD=
    HAVE_OPT_CMC_CLIENT_PASSWORD=0
    OPT_CMC_CLIENT_PASSWORD=
    HAVE_OPT_CMC_SERVICE_NAME=0
    OPT_CMC_SERVICE_NAME=
}

# -----------------------------------------------------------------------------
HAVE_CMC_MANUFACTURE_GRAFT_4b=n
#cmc_manufacture_graft_4b()
#{
# XXXX should support at least some of the options from graft point 5 as xopts
#}

# -----------------------------------------------------------------------------
HAVE_CMC_MANUFACTURE_GRAFT_5=y
cmc_manufacture_graft_5()
{
    eval set -- "$PARSE"
    while true ; do
        case "$1" in
            --cs)
                if test "$2" != yes && test "$2" != no; then
                    echo "Please specify 'yes' or 'no' for --cs option"
                    usage
                fi
                HAVE_OPT_CMC_SERVER=1
                OPT_CMC_SERVER=$2
                shift 2 ;;
            --csl)
                HAVE_OPT_CMC_SERVER_LICENSE=1
                OPT_CMC_SERVER_LICENSE=$2
                shift 2 ;;
            --cst)
                if test "$2" != "password" && test "$2" != "ssh-dsa2" && test "$2" != "none"; then
                    echo "Please specify 'none', 'password', or 'ssh-dsa2' for --cst option"
                    usage
                fi
                HAVE_OPT_CMC_SERVER_AUTHTYPE=1
                OPT_CMC_SERVER_AUTHTYPE=$2
                shift 2 ;;
            --csp)
                HAVE_OPT_CMC_SERVER_PASSWORD=1
                OPT_CMC_SERVER_PASSWORD=$2
                shift 2 ;;
            --cc)
                if test "$2" != yes && test "$2" != no; then
                    echo "Please specify 'yes' or 'no' for --cc option"
                    usage
                fi
                HAVE_OPT_CMC_CLIENT=1
                OPT_CMC_CLIENT=$2
                shift 2 ;;
            --ccl)
                HAVE_OPT_CMC_CLIENT_LICENSE=1
                OPT_CMC_CLIENT_LICENSE=$2
                shift 2 ;;
            --cca)
                if test "$2" != yes && test "$2" != no; then
                    echo "Please specify 'yes' or 'no' for --cca option"
                    usage
                fi
                HAVE_OPT_CMC_CLIENT_AUTO_RENDV=1
                OPT_CMC_CLIENT_AUTO_RENDV=$2
                shift 2 ;;
            --ccd)
                HAVE_OPT_CMC_CLIENT_SERVER_ADDR=1
                OPT_CMC_CLIENT_SERVER_ADDR=$2
                shift 2 ;;
            --cct)
                if test "$2" != "password" && test "$2" != "ssh-dsa2"; then
                    echo "Please specify 'password' or 'ssh-dsa2' for --cct option"
                    usage
                fi
                HAVE_OPT_CMC_CLIENT_AUTHTYPE=1
                OPT_CMC_CLIENT_AUTHTYPE=$2
                shift 2 ;;
            --ccsp)
                if test "$2" != yes && test "$2" != no; then
                    echo "Please specify 'yes' or 'no' for --ccsp option"
                    usage
                fi
                HAVE_OPT_CMC_CLIENT_SET_PASSWORD=1
                OPT_CMC_CLIENT_SET_PASSWORD=$2
                shift 2 ;;
            --ccp)
                HAVE_OPT_CMC_CLIENT_PASSWORD=1
                OPT_CMC_CLIENT_PASSWORD=$2
                shift 2 ;;
            --csn)
                HAVE_OPT_CMC_SERVICE_NAME=1
                OPT_CMC_SERVICE_NAME=$2
                shift 2 ;;
            --) shift ; break ;;
            *) shift ;;
        esac
    done
}


# -----------------------------------------------------------------------------
HAVE_CMC_MANUFACTURE_GRAFT_6=y
cmc_manufacture_graft_6()
{
    # .........................................................................
    # CMC server enable
    #
    if [ "x$BUILD_PROD_FEATURE_CMC_SERVER" = "x1" ]; then
        PROMPT_ANSWERED_ALREADY="${HAVE_OPT_CMC_SERVER}"
        PROMPT_PRE_ANSWER="${OPT_CMC_SERVER}"
        PROMPT_HEADING="CMC server settings"
        PROMPT_DEFAULT=${OPT_CMC_SERVER_DEF}
        PROMPT_MODEL_DEFAULT="CMC_SERVER"
        PROMPT_STRING="Enable CMC server (yes no)"
        PROMPT_VALUE_DESCR="CMC server enabled"
        PROMPT_YES_NO=1
        prompt_get_answer
        CFG_CMC_SERVER="${PROMPT_RESPONSE}"

        # If they don't want the CMC server, don't ask them
        # the rest of the questions (but still take any answers
        # they provided in the parameters to manufacture.sh)
        if [ "$CFG_CMC_SERVER" = "false" ]; then
            HAVE_OPT_CMC_SERVER_LICENSE=1
        fi

        PROMPT_HEADING=""

        # .....................................................................
        # CMC server license
        #
        if [ "x$BUILD_PROD_FEATURE_CMC_SERVER_LICENSED" = "x1" ]; then
            PROMPT_ANSWERED_ALREADY="${HAVE_OPT_CMC_SERVER_LICENSE}"
            PROMPT_PRE_ANSWER="${OPT_CMC_SERVER_LICENSE}"
            PROMPT_DEFAULT="${OPT_CMC_SERVER_LICENSE_DEF}"
            PROMPT_MODEL_DEFAULT="CMC_SERVER_LICENSE"
            PROMPT_STRING="CMC server license key"
            PROMPT_VALUE_DESCR=""
            PROMPT_YES_NO=0
            prompt_get_answer
            CFG_CMC_SERVER_LICENSE="${PROMPT_RESPONSE}"
        fi
    fi

    # .........................................................................
    # CMC client enable
    #
    if [ "x$BUILD_PROD_FEATURE_CMC_CLIENT" = "x1" ]; then
        PROMPT_ANSWERED_ALREADY="${HAVE_OPT_CMC_CLIENT}"
        PROMPT_PRE_ANSWER="${OPT_CMC_CLIENT}"
        PROMPT_HEADING="CMC client settings"
        PROMPT_DEFAULT="${OPT_CMC_CLIENT_DEF}"
        PROMPT_MODEL_DEFAULT="CMC_CLIENT"
        PROMPT_STRING="Enable CMC client (yes no)"
        PROMPT_VALUE_DESCR="CMC client enabled"
        PROMPT_YES_NO=1
        prompt_get_answer
        CFG_CMC_CLIENT="${PROMPT_RESPONSE}"

        # If they don't want the CMC client, don't ask them
        # the rest of the questions (but still take any answers
        # they provided in the parameters to manufacture.sh)
        if [ "$CFG_CMC_CLIENT" = "false" ]; then
            HAVE_OPT_CMC_CLIENT_LICENSE=1
            if [ "x$HAVE_OPT_CMC_CLIENT_AUTO_RENDV" != "x1" ]; then
                OPT_CMC_CLIENT_AUTO_RENDV="no"
            fi
            HAVE_OPT_CMC_CLIENT_AUTO_RENDV=1
            HAVE_OPT_CMC_CLIENT_SERVER_ADDR=1
        fi

        PROMPT_HEADING=""

        # .....................................................................
        # CMC client license
        #
        if [ "x$BUILD_PROD_FEATURE_CMC_CLIENT_LICENSED" = "x1" ]; then
            PROMPT_ANSWERED_ALREADY="${HAVE_OPT_CMC_CLIENT_LICENSE}"
            PROMPT_PRE_ANSWER="${OPT_CMC_CLIENT_LICENSE}"
            PROMPT_DEFAULT="${OPT_CMC_CLIENT_LICENSE_DEF}"
            PROMPT_MODEL_DEFAULT="CMC_CLIENT_LICENSE"
            PROMPT_STRING="CMC client license key"
            PROMPT_VALUE_DESCR=""
            PROMPT_YES_NO=0
            prompt_get_answer
            CFG_CMC_CLIENT_LICENSE="${PROMPT_RESPONSE}"
        fi

        # .....................................................................
        # CMC client auto-rendezvous
        #
        PROMPT_ANSWERED_ALREADY="${HAVE_OPT_CMC_CLIENT_AUTO_RENDV}"
        PROMPT_PRE_ANSWER="${OPT_CMC_CLIENT_AUTO_RENDV}"
        PROMPT_DEFAULT="${OPT_CMC_CLIENT_AUTO_RENDV_DEF}"
        PROMPT_MODEL_DEFAULT="CMC_CLIENT_AUTO_RENDV"
        PROMPT_STRING="CMC client automatic rendezvous (yes no)"
        PROMPT_VALUE_DESCR="CMC client auto-rendezvous enabled"
        PROMPT_YES_NO=1
        prompt_get_answer
        CFG_CMC_CLIENT_AUTO_RENDV="${PROMPT_RESPONSE}"

        # .....................................................................
        # CMC client server address
        #
        PROMPT_ANSWERED_ALREADY="${HAVE_OPT_CMC_CLIENT_SERVER_ADDR}"
        PROMPT_PRE_ANSWER="${OPT_CMC_CLIENT_SERVER_ADDR}"
        PROMPT_DEFAULT="${OPT_CMC_CLIENT_SERVER_ADDR_DEF}"
        PROMPT_MODEL_DEFAULT="CMC_CLIENT_SERVER_ADDR"
        PROMPT_STRING="CMC server address for rendezvous"
        PROMPT_VALUE_DESCR=""
        PROMPT_YES_NO=0
        prompt_get_answer
        CFG_CMC_CLIENT_SERVER_ADDR="${PROMPT_RESPONSE}"
    fi

    # -------------------------------------------------------------------------
    # Shared settings.  Ask these questions if either CMC client or server
    # is enabled.
    #
    if test "x$CFG_CMC_SERVER" = "xtrue" || test "x$CFG_CMC_CLIENT" = "xtrue"; then

    # .........................................................................
    # CMC server rendezvous auth type
    #
    PROMPT_HEADING="CMC shared settings"
    while true ; do
        PROMPT_ANSWERED_ALREADY="${HAVE_OPT_CMC_SERVER_AUTHTYPE}"
        PROMPT_PRE_ANSWER="${OPT_CMC_SERVER_AUTHTYPE}"
        PROMPT_DEFAULT="${OPT_CMC_SERVER_AUTHTYPE_DEF}"
        PROMPT_MODEL_DEFAULT="CMC_SERVER_AUTHTYPE"
        PROMPT_STRING="CMC server rendezvous auth type (none password ssh-dsa2)"
        PROMPT_VALUE_DESCR=""
        PROMPT_YES_NO=0
        prompt_get_answer
        PROMPT_HEADING=""
        CFG_CMC_SERVER_AUTHTYPE="${PROMPT_RESPONSE}"
        if test "$CFG_CMC_SERVER_AUTHTYPE" = "password" || test "$CFG_CMC_SERVER_AUTHTYPE" = "ssh-dsa2" || test "$CFG_CMC_SERVER_AUTHTYPE" = "none"; then
            break
        else
            echo "Please specify 'none', 'password', or 'ssh-dsa2' for auth type"
        fi
    done

    # .........................................................................
    # CMC server rendezvous authtype: password
    #
    if [ "$CFG_CMC_SERVER_AUTHTYPE" = "password" ]; then
        PROMPT_ANSWERED_ALREADY="${HAVE_OPT_CMC_SERVER_PASSWORD}"
        PROMPT_PRE_ANSWER="${OPT_CMC_SERVER_PASSWORD}"
        PROMPT_DEFAULT="${OPT_CMC_SERVER_PASSWORD_DEF}"
        PROMPT_MODEL_DEFAULT="CMC_SERVER_PASSWORD"
        PROMPT_STRING="CMC server password for rendezvous account"
        PROMPT_VALUE_DESCR=""
        PROMPT_YES_NO=0
        prompt_get_answer
        CFG_CMC_SERVER_PASSWORD="${PROMPT_RESPONSE}"

    # .........................................................................
    # CMC server rendezvous authtype: none
    #
    elif [ "$CFG_CMC_SERVER_AUTHTYPE" = "none" ]; then
        if [ "x${HAVE_OPT_CMC_SERVER_PASSWORD}" = "x1" ]; then
            echo "Cannot specify server rendezvous password if authtype is not 'password'"
            exit 1
        fi

    # .........................................................................
    # CMC server rendezvous authtype: DSA keys
    #
    else
        # If we get here, authtype must be ssh-dsa2.
        # Make sure they didn't specify a password on the command line,
        # since we would ignore it if they did and we didn't catch it here.
        if [ "x${HAVE_OPT_CMC_SERVER_PASSWORD}" = "x1" ]; then
            echo "Cannot specify server rendezvous password if authtype is not 'password'"
            exit 1
        fi

        if [ "x${CFG_CMC_CLIENT}" = "xtrue" ]; then
            PROMPT_STRING="DSA key for client to log into server for rendezvous, PRIVATE key"
            PROMPT_TERM_STRING=""
            prompt_get_multiline_answer
            CFG_CMC_CLIENT_PRIVKEY_DSA="${PROMPT_RESPONSE}"
        fi

        # Both the client and server need to know the public key
        echo ""
        echo "DSA key for client to log into server for rendezvous, PUBLIC key:"
        ${READ_CMD} PROMPT_RESPONSE
        CFG_CMC_CLIENT_PUBKEY_DSA="${PROMPT_RESPONSE}"
    fi

    # .........................................................................
    # CMC client admin auth type
    #
    while true ; do
        PROMPT_ANSWERED_ALREADY="${HAVE_OPT_CMC_CLIENT_AUTHTYPE}"
        PROMPT_PRE_ANSWER="${OPT_CMC_CLIENT_AUTHTYPE}"
        PROMPT_DEFAULT="${OPT_CMC_CLIENT_AUTHTYPE_DEF}"
        PROMPT_MODEL_DEFAULT="CMC_CLIENT_AUTHTYPE"
        PROMPT_STRING="CMC client admin auth type (password ssh-dsa2)"
        PROMPT_VALUE_DESCR=""
        PROMPT_YES_NO=0
        prompt_get_answer
        CFG_CMC_CLIENT_AUTHTYPE="${PROMPT_RESPONSE}"
        if test "$CFG_CMC_CLIENT_AUTHTYPE" = "password" || test "$CFG_CMC_CLIENT_AUTHTYPE" = "ssh-dsa2"; then
            break
        else
            echo "Please specify 'password' or 'ssh-dsa2' for auth type"
        fi
    done

    # .........................................................................
    # CMC client admin password
    #
    if [ "$CFG_CMC_CLIENT_AUTHTYPE" = "password" ]; then

        if [ "x$CFG_CMC_CLIENT" = "xtrue" ] ; then
            PROMPT_ANSWERED_ALREADY="${HAVE_OPT_CMC_CLIENT_SET_PASSWORD}"
            PROMPT_PRE_ANSWER="${OPT_CMC_CLIENT_SET_PASSWORD}"
            PROMPT_DEFAULT="${OPT_CMC_CLIENT_SET_PASSWORD_DEF}"
            PROMPT_MODEL_DEFAULT="CMC_CLIENT_SET_PASSWORD"
            PROMPT_STRING="Set admin password on CMC client (yes no)"
            PROMPT_VALUE_DESCR="Set admin password on CMC client"
            PROMPT_YES_NO=1
            prompt_get_answer
            CFG_CMC_CLIENT_SET_PASSWORD="${PROMPT_RESPONSE}"
        fi

        # If they told us not to set the client password, we only want to
        # ask for the password if it's at least a server (whether a client
        # or not).
        if [ "x$CFG_CMC_CLIENT_SET_PASSWORD" = "xtrue" ] || [ "x$CFG_CMC_SERVER" = "xtrue" ] ; then
            PROMPT_ANSWERED_ALREADY="${HAVE_OPT_CMC_CLIENT_PASSWORD}"
            PROMPT_PRE_ANSWER="${OPT_CMC_CLIENT_PASSWORD}"
            PROMPT_DEFAULT="${OPT_CMC_CLIENT_PASSWORD_DEF}"
            PROMPT_MODEL_DEFAULT="CMC_CLIENT_PASSWORD"

            # XXX/EMT: since this is such a confusing area, we should 
            # concoct a more specific prompt that explains exactly what
            # we're going to do with this password.
            PROMPT_STRING="CMC client admin password (also used by server for new clients)"

            PROMPT_VALUE_DESCR=""
            PROMPT_YES_NO=0
            prompt_get_answer
            CFG_CMC_CLIENT_PASSWORD="${PROMPT_RESPONSE}"
        else
            # Even if we didn't want to print this message, we might still
            # need something here to disambiguate which 'if' the 'else'
            # below goes with.
            echo
            echo "(Not prompting for client password, since we would not use it.)"
        fi

    # .........................................................................
    # CMC client admin DSA keys
    #
    else
        # If we get here, authtype must be ssh-dsa2.
        # Make sure they didn't specify a password on the command line,
        # since we would ignore it if they did and we didn't catch it here.
        if [ "x${HAVE_OPT_CMC_CLIENT_PASSWORD}" = "x1" ]; then
            echo "Cannot specify client admin password if authtype is not 'password'"
            exit 1
        fi

        if [ "x${CFG_CMC_SERVER}" = "xtrue" ]; then
            PROMPT_STRING="DSA key for server to log into new clients, PRIVATE key"
            PROMPT_TERM_STRING=""
            prompt_get_multiline_answer
            CFG_CMC_SERVER_PRIVKEY_DSA="${PROMPT_RESPONSE}"
        fi

        # Both the client and server need to know the public key
        echo ""
        echo "DSA key for server to log into new clients, PUBLIC key:"
        ${READ_CMD} PROMPT_RESPONSE
        CFG_CMC_SERVER_PUBKEY_DSA="${PROMPT_RESPONSE}"
    fi

    # .........................................................................
    # CMC service name
    #
    PROMPT_ANSWERED_ALREADY="${HAVE_OPT_CMC_SERVICE_NAME}"
    PROMPT_PRE_ANSWER="${OPT_CMC_SERVICE_NAME}"
    PROMPT_DEFAULT="${OPT_CMC_SERVICE_NAME_DEF}"
    PROMPT_MODEL_DEFAULT="CMC_SERVICE_NAME"
    PROMPT_STRING="CMC service name"
    PROMPT_VALUE_DESCR=""
    PROMPT_YES_NO=0
    prompt_get_answer
    CFG_CMC_SERVICE_NAME="${PROMPT_RESPONSE}"

    # End test for prompting for shared settings.
    fi
}
