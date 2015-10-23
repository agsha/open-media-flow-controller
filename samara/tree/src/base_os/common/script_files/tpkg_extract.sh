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

# tpkg_extract.sh operates on a tpkg file, which is a zip file, or a
# stage directory of the same.  It verifies, decrypts and extracts
# the image in to the specified location.

# Modes:
#
# 1: from an image or stage, verify, decrypt, and extract inner files to
#    the output dir
# 2: from an image or stage, verify, decrypt, and put inner files (which
#    may be tarball(s)) in the output dir

PATH=/usr/bin:/bin:/usr/sbin:/sbin
export PATH

umask 0022

usage()
{
    echo "Usage: $0 [-v] [-i] -f FILENAME [-d EXTRACT_OUTPUT_DIR]"
    echo "          [-x UNEXTRACTED_OUTPUT_DIR] [-X] [-m META_DIR] [-c FILE_PATTERN] [-t] [-s] [-S]"
    echo "          [-k SIGN_PUBRING] [-K ENCRYPT_PUBRING] [-P ENCRYPT_SECRING] [-l HOST_BIN_LFI_PATH]"
    echo ""
    echo "-v: verbose output"

cat <<EOF

-f FILENAME : file to process
-i : in place operation: file specifies a stage (unpacked) dir.  No unzip'ing
    is done.
-d EXTRACT_OUTPUT_DIR : output directory for extracted image files
-x UNEXTRACTED_DIR : output directory for raw, un-extracted tar balls.  
    These have been verified and decrypted.
-X : only do the '-x' behavior if encryption was used
-m META_DIR : output directory for all non-image tarball
    files in the zip.  These have been verified and decrypted.
-c FILE_PATTERN : instead of just image-*.* , raw (unextract) copy out
     the unencrypted filenames matched by this pattern.  The names of
     the files start with ./ , and are as they appear in the zip file,
     so they may have '.gpg' on them.  Only allowed with '-x', not '-d'.
     Specify "-" to get all files.
-t : do not test or verify package before extracting.  Default is to test.
-s : when testing, require signed package (manifest signature)
-S : when testing, ignore any manifest signature
-k SIGN_PUBRING : keyring file to use for signature verification.  Only
    required if different than the default.
-K ENCRYPT_PUBRING: keyring used for encryption.  Only required
    if different than the default.
-P ENCRYPT_SECRING: private keyring used for decryption.  Only required
    if different than the default.
-l HOST_BIN_LFI_PATH : location of the lfi binary built for the system we are
    running on.  This is required if it cannot be automatically found.
EOF
    exit 1
}

GPG=/usr/bin/gpg
UNZIP=unzip
TPKG_QUERY=/sbin/tpkg_query.sh
if [ ! -z "${PROD_TREE_ROOT}" -a -x "${PROD_TREE_ROOT}/src/base_os/common/script_files/tpkg_query.sh" ]; then
    TPKG_QUERY=${PROD_TREE_ROOT}/src/base_os/common/script_files/tpkg_query.sh
fi

PARSE=`/usr/bin/getopt -s sh 'vf:id:x:Xm:c:tsSk:K:P:l:' "$@"`

if [ $? != 0 ] ; then
    usage
fi

eval set -- "$PARSE"

VERBOSE=0
VERBOSE_ARGS=
FILENAME=
FILE_IS_WORKDIR=0
EXTRACT_DIR=
RAW_DIR=
RAW_ON_ENCRYPT_ONLY=0
META_DIR=
HAS_FILE_PATTERN=0
FILE_PATTERN=
DO_TEST=1
REQUIRE_SIG=0
IGNORE_SIG=0
SIGN_PUBRING=/etc/pki/tpkg/trusted.gpg
SIGN_ARGS=
ENCRYPT_PUBRING=/etc/pki/tpkg/encrypt.gpg
ENCRYPT_SECRING=/etc/pki/tpkg/private/encrypt-priv.gpg
ENCRYPT_ARGS=
LFI_ARGS=
HOST_BIN_LFI=/bin/lfi
if [ ! -z "${PROD_OUTPUT_DIR}" ]; then
    HOST_BIN_LFI=${PROD_OUTPUT_DIR}/install/host/bin/lfi
fi

while true ; do
    case "$1" in
        -v) VERBOSE=$((${VERBOSE}+1))
            VERBOSE_ARGS="${VERBOSE_ARGS} -v"
            shift ;;
        -f) FILENAME=$2; shift 2 ;;
        -i) FILE_IS_WORKDIR=1; shift ;;
        -d) EXTRACT_DIR=$2; shift 2 ;;
        -x) RAW_DIR=$2; shift 2 ;;
        -X) RAW_ON_ENCRYPT_ONLY=1; shift ;;
        -m) META_DIR=$2; shift 2 ;;
        -c) HAS_FILE_PATTERN=1; FILE_PATTERN=$2; shift 2 ;;
        -t) DO_TEST=0; shift ;;
        -s) REQUIRE_SIG=1
            SIGN_ARGS="${SIGN_ARGS} $1"
            shift ;;
        -S) IGNORE_SIG=1
            SIGN_ARGS="${SIGN_ARGS} $1"
            shift ;;
        -k) SIGN_PUBRING=$2
            SIGN_ARGS="${SIGN_ARGS} $1 $2"
            shift 2 ;;
        -K) ENCRYPT_PUBRING=$2; shift 2 ;;
        -P) ENCRYPT_SECRING=$2; shift 2 ;;
        -l) HOST_BIN_LFI=$2
            LFI_ARGS="${LFI_ARGS} $1 $2"
            shift 2 ;;
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

    if [ ! -z "${TMP_WORKDIR}" ]; then
        rm -rf ${TMP_WORKDIR}
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

if [ -z "${EXTRACT_DIR}" -a -z "${RAW_DIR}" ]; then
    usage
fi

if [ "${HAS_FILE_PATTERN}" = "1" -a ! -z "${EXTRACT_DIR}" ]; then
    usage
fi

# Do we want tar to print every file
if [ ${VERBOSE} -gt 1 ]; then
    TAR_VERBOSE=v
else
    TAR_VERBOSE=
fi

# If we don't have '-w', this is where the image file will be unzip'd to
TMP_UNZIPDIR=/var/tmp/textract-unzip.$$
rm -rf ${TMP_UNZIPDIR}

# Extra work space
TMP_WORKDIR=/var/tmp/textract-wd.$$
rm -rf ${TMP_WORKDIR}

trap "cleanup_exit" HUP INT QUIT PIPE TERM

mkdir -p -m 700 ${TMP_WORKDIR}

if [ ${FILE_IS_WORKDIR} -eq 0 ]; then
    rm -rf ${TMP_UNZIPDIR}
    mkdir -m 700 ${TMP_UNZIPDIR}
    FAILURE=0
    /usr/bin/unzip -o -q -d ${TMP_UNZIPDIR} ${FILENAME} || FAILURE=1
    if [ ${FAILURE} -eq 1 ]; then
        echo "*** Could not extract image contents"
        cleanup_exit
    fi
    UZDIR=${TMP_UNZIPDIR}
else
    if [ ! -d ${FILENAME} ]; then
        echo "*** Could not find image contents for extraction"
        cleanup_exit
    fi
    UZDIR=${FILENAME}
fi
vecho 0 "- Unpacked directory is ${UZDIR}"

# Now the unzip'd package is in directory ${UZDIR}

# Verify the image before we do anything with the contents
if [ ${DO_TEST} -ne 0 ]; then

    vecho 0 "- Verifying hashes"
    FAILURE=0
    ${TPKG_QUERY} ${VERBOSE_ARGS} -t -w -f ${UZDIR} ${SIGN_ARGS} ${ENCRYPT_ARGS} ${LFI_ARGS} || FAILURE=1
    if [ ${FAILURE} -eq 1 ]; then
        echo "*** Could not verify image"
        cleanup_exit
    fi
    
fi

# The only files we ever "extract" look like 'image-*.*' .  We might raw
# copy out other files, if -c is specified, which sets FILE_PATTERN .

FILE_LIST=$(cd ${UZDIR}; find . -type f -print)
IMAGE_FILE_LIST=$(echo "${FILE_LIST}" | grep '^./image-.*\..*')
if [ "${HAS_FILE_PATTERN}" != "1" ]; then
    MATCHING_FILE_LIST=${IMAGE_FILE_LIST}
else
    if [ "${FILE_PATTERN}" != "" -a "${FILE_PATTERN}" != "-" ]; then
        MATCHING_FILE_LIST=$(echo "${FILE_LIST}" | egrep "${FILE_PATTERN}")
    else
        MATCHING_FILE_LIST=${FILE_LIST}
    fi
fi

ENCRYPTED_FILE_LIST=
if [ -f ${UZDIR}/tpkg-encrypt ]; then
    ENCRYPTED_FILE_LIST=$(cat ${UZDIR}/tpkg-encrypt)
fi
if [ -z "${MATCHING_FILE_LIST}" -a "${HAS_FILE_PATTERN}" != "1" ]; then
        echo "*** Invalid image: missing required image file"
        cleanup_exit
fi

# The caller (like writeimage) may want to extract from the tarball itself 
WANT_EXTRACT=1
if [ -z "${EXTRACT_DIR}" ]; then
    WANT_EXTRACT=0
fi

# This is all so that we can save peak space usage in the non-encryption case
WANT_RAW=0
if [ ! -z "${RAW_DIR}" ]; then
    if [ ${RAW_ON_ENCRYPT_ONLY} -eq 1 -a -z "${ENCRYPTED_FILE_LIST}" ]; then
        WANT_RAW=0
    else
        WANT_RAW=1
    fi
fi

WANT_META=0
if [ ! -z "${META_DIR}" ]; then
    WANT_META=1
fi

if [ ${WANT_RAW} -eq 1 ]; then
    mkdir -m 700 -p ${RAW_DIR}
fi

if [ ${WANT_EXTRACT} -eq 1 ]; then
    mkdir -m 700 -p ${EXTRACT_DIR}
fi

did_decrypt=0

if [ ${WANT_RAW} -eq 1 -o ${WANT_EXTRACT} -eq 1 ]; then
    vecho 2 "MFL: ${MATCHING_FILE_LIST} ."
    for xf in ${MATCHING_FILE_LIST}; do

        xff=`echo ${xf} | sed s'/\.gpg$//'`
        xfb=`basename $xf`
        xfbf=`echo ${xfb} | sed s'/\.gpg$//'`

        # First, is it encrypted?
        encr=0
        if [ ! -z "${ENCRYPTED_FILE_LIST}" ]; then
            FAILURE=0
            grep -q "^${xff}\$" ${UZDIR}/tpkg-encrypt || FAILURE=1
            if [ $? -eq 0 -a ${FAILURE} -eq 0 ]; then
                encr=1
            fi
        fi

        xfext=`echo ${xf} | sed 's/\.gpg$//' | sed 's/^.*\.\([^\.]*\)$/\1/'`

        if [ ${WANT_EXTRACT} -eq 1 ]; then
            TAR_COMPRESS=
            case "${xfext}" in
                tbz) TAR_COMPRESS="j" ;;
                tgz) TAR_COMPRESS="z" ;;
                tar) TAR_COMPRESS="" ;;
                *) echo "ERROR: bad extension for extraction in: ${xf}"; cleanup_exit ;;
            esac
        fi

        # Three things to honor:  encr, WANT_RAW , WANT_EXTRACT .
        # We must have at least one of WANT_RAW and WANT_EXTRACT

        if [ ${encr} -eq 1 ]; then
            if [ -z "${ENCRYPT_PUBRING}" -o ! -f "${ENCRYPT_PUBRING}" ]; then
                echo "*** Could not open public encryption keyring: ${ENCRYPT_PUBRING}"
                cleanup_exit
            fi
            if [ -z "${ENCRYPT_SECRING}" -o ! -f "${ENCRYPT_SECRING}" ]; then
                echo "*** Could not open private encryption keyring: ${ENCRYPT_SECRING}"
                cleanup_exit
            fi
            if [ ${WANT_RAW} -eq 1 ]; then
                vecho 0 "- Decrypting file: ${xf}"
                FAILURE=0
                do_verbose ${GPG} --homedir ${TMP_WORKDIR} \
                    --no-options --no-default-keyring \
                    --no-mdc-warning --ignore-time-conflict \
                    --keyring ${ENCRYPT_PUBRING} \
                    --secret-keyring ${ENCRYPT_SECRING} --no-verbose --quiet --batch \
                    --trust-model always --output ${RAW_DIR}/${xfbf} --decrypt ${UZDIR}/${xf} || FAILURE=1
                if [ ${FAILURE} -ne 0 ]; then
                    echo "ERROR: could not decrypt file: ${UZDIR}/${xf}"
                    cleanup_exit
                fi
                did_decrypt=1

                if [ ${WANT_EXTRACT} -eq 1 ]; then
                    vecho 0 "- Extracting from: ${RAW_DIR}/${xfbf}"
                    FAILURE=0
                    tar -xp${TAR_COMPRESS}${TAR_VERBOSE} -f ${RAW_DIR}/${xfbf} -C ${EXTRACT_DIR} 2>&1 || FAILURE=1
                    #### XXX! | grep -v '^tar:.*: time stamp.*in the future$'
                    #### (would be XXX #dep/parse: tar)
                    if [ ${FAILURE} -eq 1 ]; then
                        echo "*** Could not extract files from ${RAW_DIR}/${xfbf}"
                        cleanup_exit
                    fi
                fi
            else
                # Just encr and WANT_EXTRACT
                vecho 0 "- Decrypting and extracting: ${xf}"
                FAILURE=0
                do_verbose ${GPG} --homedir ${TMP_WORKDIR} \
                    --no-options --no-default-keyring \
                    --no-mdc-warning --ignore-time-conflict \
                    --keyring ${ENCRYPT_PUBRING} \
                    --secret-keyring ${ENCRYPT_SECRING} --no-verbose --quiet --batch \
                    --trust-model always --decrypt ${UZDIR}/${xf} | \
                    tar -xp${TAR_COMPRESS}${TAR_VERBOSE} -f - -C ${EXTRACT_DIR} 2>&1 || FAILURE=1
                #### XXX! | grep -v '^tar:.*: time stamp.*in the future$'
                #### (would be XXX #dep/parse: tar)
                if [ ${FAILURE} -eq 1 ]; then
                    echo "*** Could not decrypt and extract files from ${UZDIR}/${xf}"
                    cleanup_exit
                fi
                did_decrypt=1
            fi

        else
            # No encr
            if [ ${WANT_RAW} -eq 1 ]; then
                vecho 0 "- Copying file : ${xf}"
                cp -p ${UZDIR}/${xf} ${RAW_DIR}
            fi
            if [ ${WANT_EXTRACT} -eq 1 ]; then
                vecho 0 "- Extracting from: ${xf}"
                FAILURE=0
                tar -xp${TAR_VERBOSE} -f ${UZDIR}/${xf} -C ${EXTRACT_DIR} 2>&1 || FAILURE=1
                #### XXX! | grep -v '^tar:.*: time stamp.*in the future$'
                #### (would be XXX #dep/parse: tar)
                if [ ${FAILURE} -eq 1 ]; then
                    echo "*** Could not extract files from ${UZDIR}/${xf}"
                    cleanup_exit
                fi
            fi
        fi

    done
fi

# We want to move tpkg-manifest-original and md5sums-original to their
# non -original names, potentially overwriting any tpkg-manifest and
# md5sum files, as otherwise we could get confused if we do a create
# from this directory, or try to verify the non -original forms against
# the files.
if [ ${WANT_RAW} -eq 1 -a ${did_decrypt} -eq 1 ]; then
    if [ -f ${RAW_DIR}/md5sums-original ]; then
        mv ${RAW_DIR}/md5sums-original ${RAW_DIR}/md5sums
    fi
    if [ -f ${RAW_DIR}/tpkg-manifest-original ]; then
        mv ${RAW_DIR}/tpkg-manifest-original ${RAW_DIR}/tpkg-manifest
    fi
fi

if [ ${WANT_META} -eq 1 ]; then
    echo "XXXX NYI"
    exit 1
fi

vecho 0 "- Done"

cleanup

exit 0
