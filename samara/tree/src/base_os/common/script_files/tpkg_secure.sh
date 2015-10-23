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

PATH=/usr/bin:/bin:/usr/sbin:/sbin
export PATH

umask 0022

usage()
{
    echo "Usage: $0 [-v] -f FILENAME -i -w"
    echo "    -k SIGN_PUBRING -p SIGN_SECRING -u SIGN_USERID "
    echo "    [-e [-K ENCRYPT_PUBRING] [-P ENCRYPT_SECRING] [-U ENCRYPT_USERID]] "
    echo "    [-o OUTPUT_FILENAME]"
    echo ""
    echo "Note: You MUST NOT use the same key for signing as encryption;"
    echo "       doing so would result in absolutely no security."
    echo ""
    echo "-v: verbose output"

    cat <<EOF

-f FILENAME: file to process
-i: in place operation: file specifies a stage dir.  no zip'ing or unzip'ing is done
-w: wrap specified file in another zip file.  Useful for turning files into tpkg.
-k SIGN_PUBRING: keyring file to use for signing
-p SIGN_SECRING: private keyring file to use for signing
-u SIGN_USERID: userid or name used for signing
-e: encrypt files listed in the ./tpkg_encrypt file
-K ENCRYPT_PUBRING: public keyring used for encryption
-P ENCRYPT_SECRING: private keyring used for encryption
-U ENCRYPT_USERID: userid or name used to encrypt
-o OUTPUTFILE: output file (not valid if -i specified)
-l HOST_BIN_LFI_PATH: location of the lfi binary built for the build system.  This
    is required if it cannot be automatically found
-c TPKG_CREATE_PATH: location of tpkg_create.sh .  Only required if not in source tree,
    or cannot be automatically found

Exactly one of -i and -w must be specified.

EOF

    exit 1
}

GPG=/usr/bin/gpg
HOST_BIN_LFI=
TPKG_CREATE=/sbin/tpkg_create.sh
if [ ! -z "${PROD_TREE_ROOT}" -a -x "${PROD_TREE_ROOT}/src/base_os/common/script_files/tpkg_create.sh" ]; then
    TPKG_CREATE=${PROD_TREE_ROOT}/src/base_os/common/script_files/tpkg_create.sh
fi
TPKG_EXTRACT=/sbin/tpkg_extract.sh
if [ ! -z "${PROD_TREE_ROOT}" -a -x "${PROD_TREE_ROOT}/src/base_os/common/script_files/tpkg_extract.sh" ]; then
    TPKG_EXTRACT=${PROD_TREE_ROOT}/src/base_os/common/script_files/tpkg_extract.sh
fi

# This could also be AES256, but there's not too much benefit in AES128
# vs. AES256 in common use cases (especially in images where the keys
# are all on the appliance anyway) and AES (128) is a little faster.
CIPHER_ALGO=AES
DIGEST_ALGO=SHA256

# GPG when encrypting does compression by default.  Specify a low-CPU
# and memory overhead method by default.
COMPRESS_ARGS="--compress-algo zlib -z 1"
# To turn off compression, instead do:
## COMPRESS_ARGS="--compress-algo none"

PARSE=`/usr/bin/getopt 'vf:l:c:iwk:p:u:eK:P:U:o:' "$@"`

if [ $? != 0 ] ; then
    usage
fi

eval set -- "$PARSE"

VERBOSE=0
FILENAME=
ENCRYPT_PASSPHRASE=
DO_IN_PLACE=0
DO_ENCRYPT=0
DO_WRAP=0
SIGN_PUBRING=
SIGN_SECRING=
SIGN_USERID=
ENCRYPT_PUBRING=
ENCRYPT_SECRING=
ENCRYPT_USERID=
OUTPUT_FILENAME=
EXTRACT_ARGS=

while true ; do
    case "$1" in
        -v) VERBOSE=$((${VERBOSE}+1)); shift ;;
        -f) FILENAME=$2; shift 2 ;;
        -l) HOST_BIN_LFI=$2; EXTRACT_ARGS="${EXTRACT_ARGS} $1 $2"; shift 2 ;;
        -c) TPKG_CREATE=$2; shift 2 ;;
        -i) DO_IN_PLACE=1; shift ;;
        -w) DO_WRAP=1; shift ;;
        -k) SIGN_PUBRING=$2; EXTRACT_ARGS="${EXTRACT_ARGS} $1 $2"; shift 2 ;;
        -p) SIGN_SECRING=$2; shift 2 ;;
        -u) SIGN_USERID=$2; shift 2 ;;
        -e) DO_ENCRYPT=1; shift ;;
        -K) ENCRYPT_PUBRING=$2; EXTRACT_ARGS="${EXTRACT_ARGS} $1 $2"; shift 2 ;;
        -P) ENCRYPT_SECRING=$2; EXTRACT_ARGS="${EXTRACT_ARGS} $1 $2"; shift 2 ;;
        -U) ENCRYPT_USERID=$2; shift 2 ;;
        -o) OUTPUT_FILENAME=$2; shift 2 ;;
        --) shift ; break ;;
        *) echo "$0: parse failure" >&2 ; usage ;;
    esac
done

# ==================================================
# Cleanup when we are finished or interrupted
# ==================================================
cleanup()
{
    # Get out of any dirs we might remove
    cd /

    if [ ! -z "${GPG_WORKDIR}" -a -e ${GPG_WORKDIR} ]; then
        rm -rf ${GPG_WORKDIR}
    fi

    if [ ! -z "${TMP_UNZIPDIR}" ]; then
        rm -rf ${TMP_UNZIPDIR}
    fi

}

# ==================================================
# Cleanup when called from 'trap' for ^C, signal, or fatal error
# ==================================================
cleanup_exit()
{
    cleanup
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
# Generate / re-generate the manifest (hash) files
# ==================================================
generate_manifest()
{
    FAILURE=0
    if [ -z "${TPKG_CREATE}" ]; then
        echo "ERROR: could not find tpkg_create.sh"
        cleanup_exit
    fi
    if [ ! -z "${HOST_BIN_LFI}" ]; then
        ${TPKG_CREATE} -f ${STAGE_DIR} -i -l ${HOST_BIN_LFI} || FAILURE=1
    else
        ${TPKG_CREATE} -f ${STAGE_DIR} -i || FAILURE=1
    fi
    if [ ${FAILURE} -ne 0 ]; then
        echo "ERROR: could not generate manifest"
        cleanup_exit
    fi
}

if [ -z "${FILENAME}" ]; then
    usage
fi

if [ -z "${OUTPUT_FILENAME}" -a ${DO_IN_PLACE} -eq 0 ]; then
    usage
fi
if [ ! -z "${OUTPUT_FILENAME}" -a ${DO_IN_PLACE} -ne 0 ]; then
    usage
fi

if [ -z "${SIGN_PUBRING}" -o -z "${SIGN_SECRING}" -o -z "${SIGN_USERID}" ]; then
    usage
fi

if [ ${DO_ENCRYPT} -ne 0 ]; then
    if [ -z "${ENCRYPT_PUBRING}" -o -z "${ENCRYPT_USERID}" ]; then
        usage
    fi
fi

ENCRYPTED_LIST_FILE=./tpkg-encrypt
GPG_WORKDIR=/var/tmp/tsecure-wd.$$
TMP_UNZIPDIR=

trap "cleanup_exit" HUP INT QUIT PIPE TERM

rm -f ${GPG_WORKDIR}
mkdir -p -m 700 ${GPG_WORKDIR}

# In this case, we already have a ZIP file.  We need to unzip it, do our
# processing, and re-ZIP at the end.
if [ ${DO_IN_PLACE} -ne 1 ]; then
    TMP_UNZIPDIR=/var/tmp/tsecure-wd-uz.$$
    rm -rf ${TMP_UNZIPDIR}

    FAILURE=0
    ${TPKG_EXTRACT} -f ${FILENAME} -x ${TMP_UNZIPDIR} -c - ${EXTRACT_ARGS} || FAILURE=1
    if [ ${FAILURE} -ne 0 ]; then
        echo "ERROR: could not extract contents: ${FILENAME}"
        cleanup_exit
    fi

    STAGE_DIR=${TMP_UNZIPDIR}
else
    STAGE_DIR=${FILENAME}
fi


if [ -z "${STAGE_DIR}" -o ! -d ${STAGE_DIR} ]; then
    echo "ERROR: could not find tpkg directory: ${STAGE_DIR}"
    cleanup_exit
fi

cd ${STAGE_DIR}

# First, make sure they've previously done a 'tpkg_create.sh'
# This is important, as we'll be saving aside the current version
# of 'tpkg-manifest' as 'tpkg-manifest-original' .
if [ ! -f "./tpkg-manifest" -o ! -f "./md5sums" ]; then
    echo "ERROR: must run tpkg_create.sh before signing."
    cleanup_exit
fi

# Set our cipher and hash work with our version of gpg
# XXX #dep/parse: gpg
GPG_VERSION=$(${GPG} --no-options --version | head -1 | sed 's/^.* \([^ ]*\)$/\1/')
GPG_VERSION_1=$(echo "${GPG_VERSION}" | awk -F. '{print $1}')
GPG_VERSION_2=$(echo "${GPG_VERSION}" | awk -F. '{print $2}')
GPG_VERSION_3=$(echo "${GPG_VERSION}" | awk -F. '{print $3}')
# For versions that are 1.2.*, use SHA1 instead of SHA256
if [ "${GPG_VERSION_1}" = "1" -a "${GPG_VERSION_2}" = "2" ]; then
    echo "WARNING: no GPG sha256 digest algorithm, using sha1"
    DIGEST_ALGO=SHA1
fi

DID_ENCRYPT=0
if [ ${DO_ENCRYPT} -ne 0 ]; then
    if [ ! -z "${ENCRYPTED_LIST_FILE}" -a -f "${ENCRYPTED_LIST_FILE}" ]; then
        FL=`cat ${ENCRYPTED_LIST_FILE}`
    else
        if [ -f "./tpkg-vars" ]; then
            find . -type f \! \( -name tpkg-\* -o -name md5sums -o -name '*.gpg' -o -name '*.sig' \) -print | grep -v -x -f ./tpkg-vars | sort > ${ENCRYPTED_LIST_FILE}
        else
            find . -type f \! \( -name tpkg-\* -o -name md5sums -o -name '*.gpg' -o -name '*.sig' \) -print | sort > ${ENCRYPTED_LIST_FILE}
        fi
        FL=`cat ${ENCRYPTED_LIST_FILE}`
    fi

    # Verify that if we filter the encrypted file list, it is the same as the original list.
    if [ -f "./tpkg-vars" ]; then
        FL_N=`cat ${ENCRYPTED_LIST_FILE} | grep -v -x -f ./tpkg-vars | grep -v '^./tpkg-' | grep -v '^md5sums$'`
    else
        FL_N=`cat ${ENCRYPTED_LIST_FILE} | grep -v '^./tpkg-' | grep -v '^md5sums$'`
    fi
    if [ "${FL}" != "${FL_N}" ]; then
        echo "ERROR: bad filename given for encrypted file"
        cleanup_exit
    fi

    # Generate / re-generate the hashes
    generate_manifest

    for f in ${FL}; do
        # Validate the filename, so we don't escape from our subtree or encrypt our metadata
        nobad_f=`echo ${f} | grep -v '\.\.' | grep '^\./' | grep -v '^\./tpkg-' | grep -v '^./md5sums' | sed 's/[^A-Za-z0-9_\./-]//g'`
        if [ -z "${nobad_f}" -o "${nobad_f}" != "${f}" ]; then
            echo "ERROR: bad filename given for encrypted file"
            cleanup_exit
        fi

        f_gpg=${f}.gpg
        # See if maybe this has already been encrypted
        if [ ! -f ${f} -a -f ${f_gpg} ]; then
            echo "ERROR: $f is already encrypted to ${f_gpg} . Re-run without -e"
            cleanup_exit
        fi

        if [ ! -f ${f} ]; then
            echo "ERROR: could not find file: ${f}"
            cleanup_exit
        fi
        
        if [ -z "${ENCRYPT_PUBRING}" -o ! -f "${ENCRYPT_PUBRING}" ]; then
            echo "ERROR: could not open public encryption keyring: ${ENCRYPT_PUBRING}"
            cleanup_exit
        fi
        if [ ! -z "${ENCRYPT_SECRING}" -a ! -f "${ENCRYPT_SECRING}" ]; then
            echo "ERROR: could not open optional private encryption keyring: ${ENCRYPT_SECRING}"
            cleanup_exit
        fi

        # Both sign and encrypt the file we're encrypting if given the
        # private part, and otherwise just encrypt it.
        ENCRYPT_SIGN_ARGS=
        if [ ! -z "${ENCRYPT_SECRING}" ]; then
            ENCRYPT_SIGN_ARGS="--secret-keyring ${ENCRYPT_SECRING} --sign"
        fi

        vecho 0 "Encrypting: $f"        
        rm -f ${f_gpg}
        FAILURE=0
        do_verbose ${GPG} --openpgp --no-emit-version --no-auto-check-trustdb \
            --homedir ${GPG_WORKDIR} --no-random-seed-file \
            --no-options --no-default-keyring --keyring ${ENCRYPT_PUBRING} \
            --cipher-algo ${CIPHER_ALGO} --digest-algo ${DIGEST_ALGO} \
            ${COMPRESS_ARGS} \
            --no-verbose --batch \
            --trust-model always --encrypt ${ENCRYPT_SIGN_ARGS} --recipient ${ENCRYPT_USERID} \
            --output ${f_gpg} ${f} || FAILURE=1

        if [ ${FAILURE} -ne 0 ]; then
            echo "ERROR: could not encrypt file: ${f}"
            cleanup_exit
        fi

        DID_ENCRYPT=1
        rm -f ${f}
    done
else

    # Make sure if we're not asked for encryption that we do not have a
    # list of files to encrypt.

    if [ ! -z "${ENCRYPTED_LIST_FILE}" -a -f "${ENCRYPTED_LIST_FILE}" ]; then
        if [ ${DO_IN_PLACE} -ne 1 ]; then
            vecho 1 "Removing unwanted list of encrypted files ${ENCRYPTED_LIST_FILE}"
            rm -rf ${ENCRYPTED_LIST_FILE}
        else
            echo "ERROR: encryption disabled, but found list of encrypted files: ${ENCRYPTED_LIST_FILE}"
            cleanup_exit
        fi
    fi
fi

# If we did some encryption, copy aside the cleartext hashes.
# Otherwise, remove any previously copied aside files to avoid confusion.
safl="./md5sums ./tpkg-manifest"
if [ ${DID_ENCRYPT} -ne 0 ]; then
    for saf in ${safl}; do
        cp -p ${saf} ${saf}-original
    done
else
    for saf in ${safl}; do
        rm -f ${saf}-original
    done
fi

# Generate / re-generate the hashes
generate_manifest

# Sign the lfi manifest
FILE_IN=./tpkg-manifest
FILE_SIG=${FILE_IN}.sig
rm -f ${FILE_SIG}

if [ -z "${SIGN_PUBRING}" -o ! -f "${SIGN_PUBRING}" ]; then
    echo "ERROR: could not open public signing keyring: ${SIGN_PUBRING}"
    cleanup_exit
fi
if [ -z "${SIGN_SECRING}" -o ! -f "${SIGN_SECRING}" ]; then
    echo "ERROR: could not open private signing keyring: ${SIGN_SECRING}"
    cleanup_exit
fi

vecho 0 "Signing: ${FILE_IN}"        
FAILURE=0
do_verbose ${GPG} --openpgp --no-emit-version --no-auto-check-trustdb \
    --homedir ${GPG_WORKDIR} --no-random-seed-file \
    --no-options --no-default-keyring --keyring ${SIGN_PUBRING} \
    --secret-keyring ${SIGN_SECRING} \
    --cipher-algo ${CIPHER_ALGO} --digest-algo ${DIGEST_ALGO} \
    --no-verbose --batch \
    --armor --sign --detach-sign -u ${SIGN_USERID} \
    --output ${FILE_SIG} ${FILE_IN} || FAILURE=1

if [ ${FAILURE} -ne 0 ]; then
    echo "ERROR: could not sign file: ${FILE_IN}"
    cleanup_exit
fi

if [ ${DO_IN_PLACE} -ne 1 ]; then
    FAILURE=0
    if [ ! -z "${HOST_BIN_LFI}" ]; then
        ${TPKG_CREATE} -f ${STAGE_DIR} -o ${OUTPUT_FILENAME} -l ${HOST_BIN_LFI} || FAILURE=1
    else
        ${TPKG_CREATE} -f ${STAGE_DIR} -o ${OUTPUT_FILENAME} || FAILURE=1
    fi
    if [ ${FAILURE} -ne 0 ]; then
        echo "ERROR: could not repackage stage directory"
        cleanup_exit
    fi

fi

cleanup

exit 0
