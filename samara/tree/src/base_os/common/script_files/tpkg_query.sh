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

# This script is used to verify a tpkg file, like a system image, or to
# query information about the package.  Internally these files are currently
# zips.

PATH=/usr/bin:/bin:/usr/sbin:/sbin
export PATH

umask 0022


usage()
{
    echo "usage: $0 -t [-w] -f imagefile.img [-s] [-S] [-k SIGN_PUBRING] [-l HOST_BIN_LFI_PATH]"
    echo "usage: $0 -i [-w] [-x] -f imagefile.img"
    echo ""
    echo "-t : test package: exit 0 if no problems"
    echo "-i : print info variables from the package suitable for sh eval"
    echo "-w : file is work directory, no unzip'ing will be done"
    echo "-x : add 'export VARNAME' to the variable output"
    echo "-s : require signed package (manifest signature)"
    echo "-S : ignore any manifest signature"
    echo "-m : use old-style md5sums instead of tpkg-manifest"
    echo "-k SIGN_PUBRING: keyring file to use for signature verification."
    echo "   Only required if different than the default."
    echo "-l HOST_BIN_LFI_PATH: location of the lfi binary built for the system we are"
    echo "   running on.  This is required if it cannot be automatically found"
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

DEFAULT_INFO_FILES="build_version.sh"
MD5SUMS=md5sums
GPG=/usr/bin/gpg

# If we don't have '-w', this is where the image file will be unzip'd to
UNZIPDIR=/var/tmp/tquery-unzip.$$
# Extra work space
TMP_WORKDIR=/var/tmp/tquery-tmp.$$

SIGN_PUBRING=/etc/pki/tpkg/trusted.gpg

HOST_BIN_LFI=/bin/lfi
if [ ! -z "${PROD_OUTPUT_DIR}" ]; then
    HOST_BIN_LFI=${PROD_OUTPUT_DIR}/install/host/bin/lfi
fi

PARSE=`/usr/bin/getopt 'vtif:l:sSmk:wx' "$@"`

if [ $? != 0 ] ; then
    usage
fi

eval set -- "$PARSE"

VERBOSE=0
DO_TEST=0
REQUIRE_SIG=0
IGNORE_SIG=0
DO_INFO=0
INFO_EXPORT=0
HAVE_FILE=0
FILE_IS_WORKDIR=0
USE_MD5SUMS=0

while true ; do
    case "$1" in
        -v) VERBOSE=$((${VERBOSE}+1)); shift ;;
        -t) DO_TEST=1; shift ;;
        -s) REQUIRE_SIG=1; shift ;;
        -S) IGNORE_SIG=1; shift ;;
        -m) USE_MD5SUMS=1; shift ;;
        -i) DO_INFO=1; shift ;;
        -k) SIGN_PUBRING=$2; shift 2 ;;
        -l) HOST_BIN_LFI=$2; shift 2 ;;
        -f) HAVE_FILE=1; IMAGE_FILE=$2; shift 2 ;;
        -w) FILE_IS_WORKDIR=1; shift ;;
        -x) INFO_EXPORT=1; shift ;;
        --) shift ; break ;;
        *) echo "tpkg_query.sh: parse failure" >&2 ; usage ;;
    esac
done

if [ $DO_TEST -eq 0 -a $DO_INFO -eq 0 ]; then
    usage
fi

if [ $HAVE_FILE -eq 0 ]; then
    usage
fi

if [ $REQUIRE_SIG -eq 1 -a $IGNORE_SIG -eq 1 ]; then
    usage
fi

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

    if [ ! -z "${UNZIPDIR}" ]; then
        rm -rf ${UNZIPDIR}
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

##################################################
# Do basic (zip level, not crypto/hash) integrity checking
##################################################

FAILURE=0
if [ ${FILE_IS_WORKDIR} -eq 0 ]; then
    if [ ! -r ${IMAGE_FILE} -o ! -f ${IMAGE_FILE} ]; then
        echo "*** Could not read file ${IMAGE_FILE}"
        exit 1
    fi

    if [ ${DO_TEST} -eq 1 ]; then
        FAILURE=0
        do_verbose /usr/bin/unzip -qqt ${IMAGE_FILE} || FAILURE=1
        if [ ${FAILURE} -eq 1 ]; then
            echo "*** Invalid image: corrupted file"
            exit 1
        fi
    fi
    
    FAILURE=0
    # XXX #dep/parse: unzip 
    FILE_LIST="$(/usr/bin/unzip -qql ${IMAGE_FILE} | awk '{print $4}' | sort)" || \
        FAILURE=1
    if [ ${FAILURE} -eq 1 ]; then
        echo "*** Invalid image: could not get file list"
        exit 1
    fi
else
    if [ ! -r ${IMAGE_FILE} -o ! -d ${IMAGE_FILE} ]; then
        echo "*** Could not read directory ${IMAGE_FILE}"
        exit 1
    fi

    FILE_LIST_DS="$( (cd ${IMAGE_FILE}; find . -type f -print | sort) )" || FAILURE=1
    FILE_LIST="$( echo "${FILE_LIST_DS}" | sed 's/^\.\///' )" || FAILURE=1
    if [ ${FAILURE} -eq 1 ]; then
        echo "*** Invalid image: could not get file list"
        exit 1
    fi
fi
    
HAS_MD5SUMS=0
FAILURE=0
echo "${FILE_LIST}" | grep -q "^md5sums$" || FAILURE=1
if [ $? -eq 0 -a ${FAILURE} -eq 0 ]; then
    HAS_MD5SUMS=1
fi

HAS_TPKG_MANIFEST=0
FAILURE=0
echo "${FILE_LIST}" | grep -q "^tpkg-manifest$" || FAILURE=1
if [ $? -eq 0 -a ${FAILURE} -eq 0 ]; then
    HAS_TPKG_MANIFEST=1
fi

HAS_TPKG_INFO=0
FAILURE=0
echo "${FILE_LIST}" | grep -q "^tpkg-vars$" || FAILURE=1
if [ $? -eq 0 -a ${FAILURE} -eq 0 ]; then
    HAS_TPKG_INFO=1
fi

HAS_IMAGE_FILE=0
FAILURE=0
echo "${FILE_LIST}" | grep -q '^image-*.*$' || FAILURE=1
if [ $? -eq 0 -a ${FAILURE} -eq 0 ]; then
    HAS_IMAGE_FILE=1
fi

if [ ${HAS_MD5SUMS} -eq 0 -a ${HAS_TPKG_MANIFEST} -eq 0 ]; then
    echo "*** Invalid image: hashes not found"
    exit 1
fi

if [ ${HAS_IMAGE_FILE} -eq 0 ]; then
    echo "*** Invalid image: missing required image file"
    exit 1
fi


if [ ${DO_TEST} -eq 1 ]; then

    # Then make sure the image contains an image ball and an md5sums file.
    # Next get a list of all the files, and walk this, doing an md5sum on
    # each.  Make a list of just the hashes (no files names) sorted by
    # filename, and compare it to the original hash list (no filenames)
    # sorted by filename.

    HAS_TPKG_MANIFEST_SIG=0
    FAILURE=0
    echo "${FILE_LIST}" | grep -q "^tpkg-manifest\.sig$" || FAILURE=1
    if [ $? -eq 0 -a ${FAILURE} -eq 0 ]; then
        HAS_TPKG_MANIFEST_SIG=1
    fi

    HAS_TPKG_MANIFEST_ORIGINAL=0
    FAILURE=0
    echo "${FILE_LIST}" | grep -q "^tpkg-manifest-original" || FAILURE=1
    if [ $? -eq 0 -a ${FAILURE} -eq 0 ]; then
        HAS_TPKG_MANIFEST_ORIGINAL=1
    fi

    HAS_TPKG_ENCRYPT=0
    FAILURE=0
    echo "${FILE_LIST}" | grep -q "^tpkg-encrypt$" || FAILURE=1
    if [ $? -eq 0 -a ${FAILURE} -eq 0 ]; then
        HAS_TPKG_ENCRYPT=1
    fi

    # Make sure we have a manifest if we have a signature!
    if [ ${IGNORE_SIG} -eq 0 ]; then
        if [ ${HAS_TPKG_MANIFEST_SIG} -eq 1 -a ${HAS_TPKG_MANIFEST} -eq 0 ]; then
            echo "*** Could not find required manifest"
            exit 1
        fi
    fi

    # Set up cleanup_exit to get rid of this work dir

    trap "cleanup_exit" HUP INT QUIT PIPE TERM

    # Setup temporary work dir we'll need
    rm -f ${TMP_WORKDIR}
    FAILURE=0
    mkdir -m 700 -p ${TMP_WORKDIR} || FAILURE=1
    if [ ${FAILURE} -eq 1 ]; then
        echo "*** Could not make temporary work directory ${TMP_WORKDIR}"
        cleanup_exit
    fi

    ###############
    # Extract / copy all the meta files (tpkg-* and md5sums)
    ###############
    m5f=
    if [ ${HAS_MD5SUMS} -ne 0 ]; then
        m5f=${MD5SUMS}
    fi
    if [ ${FILE_IS_WORKDIR} -eq 0 ]; then
        # See if we should try to extract any tpkg-* files
        FAILURE=0
        echo "${FILE_LIST}" | grep -q 'tpkg-.*$' || FAILURE=1
        if [ $? -eq 0 -a ${FAILURE} -eq 0 ]; then
            tpp='tpkg-*'
        else
            tpp=
        fi

        FAILURE=0
        /usr/bin/unzip -d ${TMP_WORKDIR} -qq ${IMAGE_FILE} ${tpp} ${m5f} || FAILURE=1
        if [ ${FAILURE} -eq 1 ]; then
            echo "*** Could not extract package information files"
            cleanup_exit
        fi
    else
        ( cd ${IMAGE_FILE} ; cp ${m5f} ${TMP_WORKDIR} ) || FAILURE=1
        ( cd ${IMAGE_FILE}
            FL=`find . -name 'tpkg-*' -print`
            if [ ! -z "${FL}" ]; then
                cp ${FL} ${TMP_WORKDIR}
            fi
            ) || FAILURE=1
        if [ ${FAILURE} -eq 1 ]; then
            echo "*** Could not extract package information files"
            cleanup_exit
        fi
    fi

    ###############
    # Signature check, if present
    ###############
    if [ ${REQUIRE_SIG} -eq 1 -a ${HAS_TPKG_MANIFEST_SIG} -eq 0 ]; then
        echo "*** Could not find required signature"
        cleanup_exit
    fi

    if [ ${HAS_TPKG_MANIFEST_SIG} -eq 1 -a ${IGNORE_SIG} -eq 0 ]; then
        if [ ! -f ${TMP_WORKDIR}/tpkg-manifest -o ! -f ${TMP_WORKDIR}/tpkg-manifest.sig ]; then
            echo "*** Could not find manifest and signature"
            cleanup_exit
        fi

        if [ ! -f ${SIGN_PUBRING} ]; then
            echo "*** Could not find public key to verify manifest signature."
            cleanup_exit
        fi
        FAILURE=0
        do_verbose ${GPG} --no-auto-check-trustdb \
            --homedir ${TMP_WORKDIR} --no-random-seed-file \
            --no-options --no-default-keyring --keyring ${SIGN_PUBRING} \
            --no-mdc-warning --ignore-time-conflict \
            --no-verbose --quiet --batch \
            --trust-model always --verify \
            ${TMP_WORKDIR}/tpkg-manifest.sig ${TMP_WORKDIR}/tpkg-manifest || FAILURE=1

        if [ ${FAILURE} -eq 1 ]; then
            echo "*** Could not verify manifest signature"
            cleanup_exit
        fi

    fi

    ################
    # Validate hashes match files
    ################
    # There are the following scenarios for the two hash files:
    #
    # a)  HAS_MD5SUMS !HAS_TPKG_MANIFEST
    # b) !HAS_MD5SUMS  HAS_TPKG_MANIFEST
    # c)  HAS_MD5SUMS  HAS_TPKG_MANIFEST
    #
    # case a) is all 'old' style (no tpkg files) packages.  case b) we
    # don't currently generate (because of backward compatability
    # issues) but might later, and if so we just check the TPKG_MANIFEST
    # .  In case c) we prefer the TPKG_MANIFEST , as it has more info in
    # it, and might be signed.  In this case we make sure the md5s match
    # what the manifest has, if it has md5s.


    if [ ${HAS_TPKG_MANIFEST} -eq 1 -a ${USE_MD5SUMS} -eq 0 ]; then

        # We have a TPKG_MANIFEST , and now must verify it.  We extract
        # the whole package, and then use lfi to verify.  
        #
        # XXXX This implementation means that we will use either a lot
        # of memory or disk.  It could be done without needing to do
        # this, as the old implementation does below.  In the writeimage
        # case, it calls tpkg_extract -> tpkg_query after having done
        # the unzip itself anyway, so it so no worse, but other callers
        # may not do this, and could get significantly worse
        # performance.  It could also be an added issue in the flash
        # case for peak disk usage.

        if [ ${FILE_IS_WORKDIR} -eq 0 ]; then
            rm -rf ${UNZIPDIR}
            mkdir -m 700 ${UNZIPDIR}
            FAILURE=0
            /usr/bin/unzip -o -q -d ${UNZIPDIR} ${IMAGE_FILE} || FAILURE=1
            if [ ${FAILURE} -eq 1 ]; then
                echo "*** Could not extract image contents for verification"
                cleanup_exit
            fi
        fi

        # Verify that the tpkg-manifest covers all the files in the
        # image (excepting the manifest and the manifest signature)

        cat ${TMP_WORKDIR}/tpkg-manifest | awk '{print $14}' | sort | uniq > ${TMP_WORKDIR}/.amfl
        if [ ${FILE_IS_WORKDIR} -eq 0 ]; then
            (cd ${UNZIPDIR};   find . -type f -print | grep -v '^\.$' | grep -v '^./tpkg-manifest$' | grep -v '^./tpkg-manifest\.sig$' | sort > ${TMP_WORKDIR}/.apfl)
        else
            (cd ${IMAGE_FILE}; find . -type f -print | grep -v '^\.$' | grep -v '^./tpkg-manifest$' | grep -v '^./tpkg-manifest\.sig$' | sort > ${TMP_WORKDIR}/.apfl)
        fi

        different=1
        cmp -s ${TMP_WORKDIR}/.apfl ${TMP_WORKDIR}/.amfl && different=0
        if [ ${different} -ne 0 ]; then
            echo "ERROR: hash file list does not match package file list"
            cleanup_exit
        fi

        # Verify that the tpkg-manifest and md5sums agree with each other
        if [ ${HAS_MD5SUMS} -eq 1 ]; then
            rm -f ${TMP_WORKDIR}/.tpm ${TMP_WORKDIR}/.md5s

            # We're taking just the md5sum and the filename out of the lfi
            # line.  We're also removing the line for md5sums, as the
            # md5sums file won't have that.

            cat ${TMP_WORKDIR}/tpkg-manifest | sed 's/^lfiv1 [^ ]* [^ ]* [^ ]* [^ ]* [^ ]* [^ ]* [^ ]* [^ ]* [^ ]* [^ ]* \(.*\)/\1/' | grep '^md5' | grep -v ' \./md5sums$' | awk '{print $3 " " $2}' | sort > ${TMP_WORKDIR}/.tpm
            cat ${TMP_WORKDIR}/${MD5SUMS} | awk '{print $2 " " $1}' | sort > ${TMP_WORKDIR}/.md5s
            
            if [ -s ${TMP_WORKDIR}/.tpm ]; then
                different=1
                cmp -s ${TMP_WORKDIR}/.tpm ${TMP_WORKDIR}/.md5s && different=0
                if [ ${different} -ne 0 ]; then
                    echo "ERROR: inconsistent hashes in package."
                    cleanup_exit
                fi
            fi
        fi 

        if [ -z "${HOST_BIN_LFI}" -o ! -x "${HOST_BIN_LFI}" ]; then
            echo "ERROR: could not find lfi program.  Specify -l option."
            cleanup_exit
        fi

        FAILURE=0
        if [ ${FILE_IS_WORKDIR} -eq 0 ]; then
            lfio="$(${HOST_BIN_LFI} -q -TGUNM -r ${UNZIPDIR} -c ${TMP_WORKDIR}/tpkg-manifest 2>&1 )" || FAILURE=1
        else
            lfio="$(${HOST_BIN_LFI} -q -TGUNM -r ${IMAGE_FILE} -c ${TMP_WORKDIR}/tpkg-manifest 2>&1 )" || FAILURE=1
        fi
        if [ ${FAILURE} -eq 1 ]; then
            echo "*** Invalid image: manifest verify failed ${lfio}"
            cleanup_exit
        fi

    else 
        # No tpkg-manifest, so an 'old' style image, which uses "md5sums"

        if [ ${HAS_MD5SUMS} -eq 0 ]; then
            echo "ERROR: could not find hashes in package."
            cleanup_exit
        fi

        # Verify that md5sums covers all the files in the image
        # (excepting md5sums)

        # Note, use the non- ./ (DS) version here, unlike above
        cat ${TMP_WORKDIR}/md5sums | awk '{print $2}' | sort | uniq | sed 's/^\.\///' > ${TMP_WORKDIR}/.amfl
        echo "${FILE_LIST}" | grep -v '^md5sums$' | grep -v '^tpkg-manifest$' | grep -v '^tpkg-manifest\.sig$' > ${TMP_WORKDIR}/.apfl

        different=1
        cmp -s ${TMP_WORKDIR}/.apfl ${TMP_WORKDIR}/.amfl && different=0
        if [ ${different} -ne 0 ]; then
            echo "ERROR: hash file list does not match package file list"
            cleanup_exit
        fi

        FAILURE=0
        MD5LIST="$(cat ${TMP_WORKDIR}/${MD5SUMS} | awk '{print $2 " "  $1}' | sort | awk '{print $2}')" || FAILURE=1
        if [ ${FAILURE} -eq 1 -o -z "${MD5LIST}" ]; then
            echo "*** Invalid image: hashes could not be extracted"
            cleanup_exit
        fi
        
        SHL=
        for sif in ${MD5LIST}; do
            SHL="${SHL} ${sif}"
        done
        
        HL=
        for tif in ${FILE_LIST}; do
            if [ "${tif}" = "${MD5SUMS}" -o "${tif}" = "tpkg-manifest" -o "${tif}" = "tpkg-manifest.sig" ]; then
                continue
            fi
            
            ## echo "Testing $tif"

            FAILURE=0

            if [ ${FILE_IS_WORKDIR} -eq 0 ]; then
                ZFH=`/usr/bin/unzip -qqp ${IMAGE_FILE} $tif | md5sum -` || FAILURE=1
            else
                ZFH=`cat ${IMAGE_FILE}/$tif | md5sum -` || FAILURE=1
            fi
            if [ ${FAILURE} -eq 1 ]; then
                echo "*** Invalid image: could not hash $tif"
                cleanup_exit
            fi
            
            JUST_HASH=`echo $ZFH | awk '{print $1}'`
            HL="${HL} ${JUST_HASH}"
        done
        
        if [ "${HL}" != "${SHL}" ]; then
            echo "*** Invalid image: hashes do not match"
##        echo "HL: ${HL}" | tr ' ' '\n'
##        echo ""
##        echo "SHL: ${SHL}" | tr ' ' '\n'
            cleanup_exit
        fi
    fi
fi


if [ ${DO_INFO} -eq 1 ]; then
    # The plan is to extract the info file(s), (build_version.sh for images),
    # grep'ing for just the lines that look like they are setting variables.
    # We'll add in "export VARNAME" lines if requested to do so

    INFO_FILES=

    if [ ${HAS_TPKG_INFO} -eq 1 ]; then
        FAILURE=0
        if [ ${FILE_IS_WORKDIR} -eq 0 ]; then
            # XXX #dep/parse: unzip 
            INFO_FILES="$(/usr/bin/unzip -qqp ${IMAGE_FILE} tpkg-vars | sed 's/^\.\///')" || FAILURE=1
        else
            # XXX #dep/parse: unzip 
            INFO_FILES="$(cat ${IMAGE_FILE}/tpkg-vars | sed 's/^\.\///')" || FAILURE=1
        fi
        if [ ${FAILURE} -eq 1 ]; then
            echo "*** Invalid image: could not extract tpkg-vars"
            exit 1
        fi
    else
        INFO_FILES="${DEFAULT_INFO_FILES}"
    fi

    if [ ! -z "${INFO_FILES}" ]; then
        FAILURE=0
        if [ ${FILE_IS_WORKDIR} -eq 0 ]; then
            # XXX #dep/parse: unzip 
            INFO_VARS="$(/usr/bin/unzip -qqp ${IMAGE_FILE} ${INFO_FILES} | grep '^[^=][^=]*=.*')" || FAILURE=1
        else
            # XXX #dep/parse: unzip 
            INFO_VARS="$( (cd ${IMAGE_FILE}; cat ${INFO_FILES}) | grep '^[^=][^=]*=.*')" || FAILURE=1
        fi
        
        if [ ${FAILURE} -eq 1 ]; then
            echo "*** Could not extract image info"
            exit 1
        fi

        if [ ${INFO_EXPORT} -eq 1 ]; then
            # Add the separate 'export VARNAME' lines
            echo "$INFO_VARS" | grep '^[^=][^=]*=.*$' | sed 's/^\([^=][^=]*\)=\(.*\)$/\1=\2\nexport \1/'
        else
            echo "$INFO_VARS"
        fi
    fi

fi

cleanup

exit 0
