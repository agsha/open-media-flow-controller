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

# tpkg_create.sh operates on a stage directory, and either modifies the
# directory in place (if '-i' is specified), or modifies the directory,
# produces a zip file, and then removes the stage directory.
#
#
# INPUT files:
#
# 'tpkg-vars': The stage may have the special input file: 'tpkg-vars'
# which says which sh file(s) in the package will be read in to
# determine the exposed sh-style variables returned by 'tpkg_query.sh
# -i'.  This is typically meta information for end-users about the
# image, like versioning or content information.  If missing, it is
# assumed such information is solely in a file called 'build_version.sh'
# .  This 'tpkg-vars' file is REQUIRED (but may be empty) if you do not
# include a 'build_version.sh' file.  If encryption is later used, the
# files listed in 'tpkg-vars' may not be encrypted.
#
# 'tpkg-encrypt': The stage has the special input file: 'tpkg-encrypt'
# which lists which files in the stage will be encrypted.  This list may
# not contain any of the 'tpkg-*' files, the 'md5sums' file, or any file
# listed in 'tpkg-vars' .  If you ask for encryption (from tpkg_secure),
# but do not supply this file, one is auto-created for you.
#
# image-*.tbz: The main files to be included must be named image-*.tbz .
#
# OUTPUT files:
#
# 'tpkg-manifest': This is an lfi file containing md5 and sha256 hashes of all
# files in the final pkg, excluding 'tpkg-manifest' and 'tpkg-manifest.sig' .
#
# 'tpkg-manifest-original' : When encryption is used, this is a manifest of
# the pkg before encryption, as described in 'tpkg-manifest'.
#
# 'md5sums': An md5sum output file, present for legacy image installation.
# It covers all files in the final pkg, excluding 'md5sums', 'tpkg-manifest'
# and 'tpkg-manifest.sig'
#
# 'md5sums-original': When encryption is used, this is the same md5sums
# before encryption.
#
# 'tpkg-manifest.sig': A gpg signature of the final 'tpkg-manifest' .
# This is made by later calling tpkg_secure.sh on a created image file
# or output stage.


PATH=/usr/bin:/bin:/usr/sbin:/sbin
export PATH

umask 0022

usage()
{
    echo "Usage: $0 [-v] -f FILENAME -i [-l HOST_BIN_LFI_PATH] [-E]"
    echo "       $0 [-v] -f FILENAME -o OUTPUT_FILENAME [-l HOST_BIN_LFI_PATH] [-E]"
    echo ""
    echo "-v: verbose output"

cat <<EOF

-f FILENAME: input file to process, currently must always be a stage directory
-l HOST_BIN_LFI_PATH: location of the lfi binary built for the system we are
    running on.  This is required if it cannot be automatically found.
-E: input has a tpkg_encrypt file, but files are not (yet) encrypted.  The
    caller will run tpkg_secure.sh later.
-i: in place operation: output is to the input stage dir.  No zip'ing or
    unzip'ing is done.
-o OUTPUTFILE: output file (not valid if -i specified)

EOF
    exit 1
}

MD5SUM=md5sum
ZIP=zip
HOST_BIN_LFI=/bin/lfi
ENCRYPTED_LIST_FILE=./tpkg-encrypt
if [ ! -z "${PROD_OUTPUT_DIR}" ]; then
    HOST_BIN_LFI=${PROD_OUTPUT_DIR}/install/host/bin/lfi
fi

PARSE=`/usr/bin/getopt 'vf:l:iEo:' "$@"`

if [ $? != 0 ] ; then
    usage
fi

eval set -- "$PARSE"

VERBOSE=0
FILENAME=
DO_IN_PLACE=0
ENCRYPT_FOLLOWS=0
OUTPUT_FILENAME=

while true ; do
    case "$1" in
        -v) VERBOSE=$((${VERBOSE}+1)); shift ;;
        -f) FILENAME=$2; shift 2 ;;
        -l) HOST_BIN_LFI=$2; shift 2 ;;
        -i) DO_IN_PLACE=1; shift ;;
        -E) ENCRYPT_FOLLOWS=1; shift ;;
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

}

# ==================================================
# Cleanup when called from 'trap' for ^C, signal, or fatal error
# ==================================================
cleanup_exit()
{
    cleanup
    exit 1
}

# XXXX Should support things already in a ZIP file, unzip them, do our
# stuff, and re-ZIP at the end.
if [ ${DO_IN_PLACE} -eq 1 -a ! -z "${OUTPUT_FILENAME}" ]; then
    usage
fi
if [ ${DO_IN_PLACE} -eq 0 -a -z "${OUTPUT_FILENAME}" ]; then
    usage
fi

if [ -z "${HOST_BIN_LFI}" -o ! -x "${HOST_BIN_LFI}" ]; then
    echo "ERROR: could not find lfi program.  Specify -l option."
    cleanup_exit
fi

STAGE_DIR=${FILENAME}

if [ -z "${STAGE_DIR}" -o ! -d ${STAGE_DIR} ]; then
    echo "ERROR: could not find tpkg directory: ${STAGE_DIR}"
    cleanup_exit
fi

cd ${STAGE_DIR}

trap "cleanup_exit" HUP INT QUIT PIPE TERM

# Clean out any old temp files
rm -f ./md5sums.temp* ./tpkg-manifest.temp*

# md5sums
find . -type f \! \( -name tpkg-manifest -o -name tpkg-manifest.\* -o -name md5sums -o -name md5sums.\* \) -print0 | xargs -0 -r ${MD5SUM} | sort -k 2 > ./md5sums.temp
# only update md5sums if the new one is different
changed=1
if [ -f ./md5sums ]; then
    cmp -s ./md5sums ./md5sums.temp && changed=0
fi
if [ ${changed} -ne 0 ]; then
    mv ./md5sums.temp ./md5sums
fi
rm -f ./md5sums.temp 

# tpkg-manifest for md5 and sha256
FAILURE=0
find . -type f \! \( -name tpkg-manifest -o -name tpkg-manifest.\* \) -print0 | xargs -0 -r ${HOST_BIN_LFI} -tgunm -a md5 | sort -k 14 > ./tpkg-manifest.temp || FAILURE=1
if [ ${FAILURE} -ne 0 ]; then
    echo "ERROR: could not create manifest file"
    cleanup_exit
fi

FAILURE=0
${HOST_BIN_LFI} -tgunm -a sha256 ./tpkg-manifest.temp > /dev/null 2>&1 || FAILURE=1
if [ ${FAILURE} -eq 0 ]; then
    find . -type f \! \( -name tpkg-manifest -o -name tpkg-manifest.\* \) -print0 | xargs -0 -r ${HOST_BIN_LFI} -tgunm -a sha256 | sort -k 14 >> ./tpkg-manifest.temp || FAILURE=1
else
    FAILURE=0
    ${HOST_BIN_LFI} -tgunm -a sha1 ./tpkg-manifest.temp > /dev/null 2>&1 || FAILURE=1
    if [ ${FAILURE} -ne 0 ]; then
        echo "WARNING: No sha256 or sha1 support"
        FAILURE=0
    else
        echo "WARNING: No sha256 support, using sha1"
        find . -type f \! \( -name tpkg-manifest -o -name tpkg-manifest.\* \) -print0 | xargs -0 -r ${HOST_BIN_LFI} -tgunm -a sha1 | sort -k 14 >> ./tpkg-manifest.temp || FAILURE=1
    fi
fi
if [ ${FAILURE} -ne 0 ]; then
    echo "ERROR: could not create manifest file"
    cleanup_exit
fi

cat ./tpkg-manifest.temp | sort -k 14 -k 12 -k 8 > ./tpkg-manifest.temp2
rm -f ./tpkg-manifest.temp

# only update tpkg-manifest if the new one is different
changed=1
if [ -f ./tpkg-manifest ]; then
    cmp -s ./tpkg-manifest ./tpkg-manifest.temp2 && changed=0
fi 
if [ ${changed} -ne 0 ]; then
    mv ./tpkg-manifest.temp2 ./tpkg-manifest
fi
rm -f ./tpkg-manifest.temp2 

# If the manifest wasn't identical to an existing manifest, 
# deleting any signature that might be left around
if [ ${changed} -ne 0 ]; then
    rm -f ./tpkg-manifest.sig
fi

# Make sure that if we are going to make a zip that tpkg-encrypt is
# okay, and matches reality as well as expected (see -E flag).
if [ ! -z "${OUTPUT_FILENAME}" -a ! -z "${ENCRYPTED_LIST_FILE}" -a -f "${ENCRYPTED_LIST_FILE}" ]; then
    FL=`cat ${ENCRYPTED_LIST_FILE}`
    for f in ${FL}; do

        # Validate the filename, so we don't escape from our subtree or
        # encrypt our metadata
        nobad_f=`echo ${f} | grep -v '\.\.' | grep '^\./' | grep -v '^\./tpkg-' | grep -v '^./md5sums' | sed 's/[^A-Za-z0-9_\./-]//g'`
        if [ -z "${nobad_f}" -o "${nobad_f}" != "${f}" ]; then
            echo "ERROR: bad filename given for encrypted file"
            cleanup_exit
        fi

        f_gpg=${f}.gpg

        # Make sure the encrypted file exists, unless ENCRYPT_FOLLOWS
        # (-E) is passed in
        if [ ${ENCRYPT_FOLLOWS} -eq 0 ]; then
            if [ ! -f ${f_gpg} ]; then
                if [ -f ${f} ]; then
                    echo "ERROR: ${f_gpg} not found, but listed in ${ENCRYPTED_LIST_FILE} .  If you are about to run tpkg_secure.sh, re-run with -E"
                    cleanup_exit
                else
                    echo "ERROR: ${f_gpg} not found, but listed in ${ENCRYPTED_LIST_FILE} ."
                    cleanup_exit
                fi
            fi
        else
            if [ ! -f ${f} ]; then
                if [ -f ${f_gpg} ]; then
                    echo "ERROR: ${f} not found, but listed in ${ENCRYPTED_LIST_FILE} .  If you are about to run tpkg_secure.sh, re-run without -E"
                    cleanup_exit
                else
                    echo "ERROR: ${f} not found, but listed in ${ENCRYPTED_LIST_FILE} ."
                fi
            fi
        fi
    done
fi

# If they specified an output file, zip out to it
if [ ! -z "${OUTPUT_FILENAME}" ]; then
    FAILURE=0
    rm -f ${OUTPUT_FILENAME}
    ${ZIP} -qr0 ${OUTPUT_FILENAME} . || FAILURE=1
    if [ ${FAILURE} -ne 0 ]; then
        echo "ERROR: could not create ZIP file"
        cleanup_exit
    fi
fi

cleanup

exit 0
