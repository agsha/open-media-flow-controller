#!/bin/sh
#
#
#
#

PATH=/usr/sbin:/usr/local/bin:/usr/xpg4/bin:/usr/ucb:/usr/bin:/bin:/usr/ccs/bin
export PATH

#
# XXX PROD_PRODUCT, PROD_CUSTOMER, PROD_ID and PROD_RELEASE should be passed
#     in, and on to the build system
#

usage()
{
    echo "usage: $0 [-j num] [-m] BUILD_TYPE BUILDS_DIR [PREV_PROD_OUTPUT_ROOT]"
    exit 1
}

PARSE=`/usr/bin/getopt 'Jj:m' "$@"`

if [ $? != 0 ] ; then
    usage
fi

eval set -- "$PARSE"

JOB_STR=
PRINT_BUILD_META=0

while true ; do
    case "$1" in
        -j) JOB_NUM=$2; JOB_STR="-j ${JOB_NUM}"; shift 2 ;; 
        -J) JOB_NUM=; JOB_STR="-j"; shift ;; 
        -m) PRINT_BUILD_META=1; shift ;;
        --) shift ; break ;;
        *) echo "$0: parse failure" >&2 ; usage ;;
    esac
done

if [ $# != 2 -a $# != 3 ]; then
    usage
fi

# Get the remaining non-optional command line args
# Build type can be "dev" (for developers) "spot" (for automatic spot check)
#                or "numbered" (for "releng" official, numbered builds)

PI_BUILD_TYPE="dev"

PI_BUILD_TYPE=$1
PI_BUILDS_DIR=$2
export PI_BUILD_TYPE
export PI_BUILDS_DIR

if [ $# -ge 3 ]; then
        PI_PREV_PROD_OUTPUT_ROOT=$3
        export PI_PREV_PROD_OUTPUT_ROOT
fi

#
# NOTE: The caller can setenv the following:
#
#  CVSROOT                (required)
#  PROD_TREE_ROOT         (required)
#  BUILD_SCM_TAG
#  BUILD_SCM_DATE
#  BUILD_SCM_INFO
#  BUILD_MAIL_TO
#  BUILD_MAIL_CC
#  PROD_PRODUCT
#  PROD_TARGET_ARCH
#  PROD_TARGET_HWNAME
#  PROD_ID
#

if [ -z "${BUILD_MAIL_TO}" ]; then
    BUILD_MAIL_TO=tms-build-nightly@company.com
fi
if [ -z "${BUILD_MAIL_CC}" ]; then
    BUILD_MAIL_CC=admin@company.com
fi
CVS=cvs
MAKE=make

# Use a tag when checking out?
SCM_USE_TAG=0
# Use a date when checking out?
SCM_USE_DATE=0
# Are we trying to check out against a different time than 'now'?
SCM_USE_DATE_NOW=0

BUILD_NOW_DATE=`date '+%Y-%m-%d %H:%M:%S'`
BUILD_NOW_DATE_FILE=`echo ${BUILD_NOW_DATE} | sed 's/-//g' | sed 's/://g' | sed 's/[^A-Za-z0-9]/-/g'`

# Normalize any BUILD_SCM_DATE we're given
if [ ! -z "${BUILD_SCM_DATE}" ]; then
    BUILD_SCM_DATE=$(date '+%Y-%m-%d %H:%M:%S' --date="${BUILD_SCM_DATE}")
fi

# Set our SCM_USE_TAG, SCM_USE_DATE and SCM_USE_DATE_NOW based on environment
if [ -z "${BUILD_SCM_TAG}" -a -z "${BUILD_SCM_DATE}" ]; then
    BUILD_SCM_TAG=HEAD
    BUILD_SCM_DATE=${BUILD_NOW_DATE}
    SCM_USE_TAG=0
    SCM_USE_DATE=1
    SCM_USE_DATE_NOW=1
elif [ -z "${BUILD_SCM_TAG}" -a ! -z "${BUILD_SCM_DATE}" ]; then
    BUILD_SCM_TAG=HEAD
    SCM_USE_TAG=0
    SCM_USE_DATE=1
    SCM_USE_DATE_NOW=0
elif [ ! -z "${BUILD_SCM_TAG}" -a -z "${BUILD_SCM_DATE}" ]; then
    BUILD_SCM_DATE=${BUILD_NOW_DATE}
    SCM_USE_TAG=1
    SCM_USE_DATE=0
    SCM_USE_DATE_NOW=1
elif [ ! -z "${BUILD_SCM_TAG}" -a ! -z "${BUILD_SCM_DATE}" ]; then
    SCM_USE_TAG=1
    SCM_USE_DATE=1
    SCM_USE_DATE_NOW=0
fi

BUILD_SCM_DATE_FILE=`echo ${BUILD_SCM_DATE} | sed 's/-//g' | sed 's/://g' | sed 's/[^A-Za-z0-9]/-/g'`


# XXX The fallback is hard-coded only for CVS right now
if [ -z "${BUILD_SCM_INFO}" ]; then
    if [ ! -z "${CVSROOT}" ]; then
        BUILD_SCM_REPOS=`basename ${CVSROOT}`
    else
        BUILD_SCM_REPOS=unknown
    fi
    BUILD_SCM_INFO=${BUILD_SCM_REPOS}/${BUILD_SCM_TAG}

    if [ ${SCM_USE_DATE_NOW} -eq 0 ]; then
        BUILD_SCM_INFO=${BUILD_SCM_INFO}-${BUILD_SCM_DATE_FILE}
    fi
fi

if [ -z "${PROD_PRODUCT}" ]; then
    PROD_PRODUCT=GENERIC
fi
PROD_PRODUCT_LC=`echo ${PROD_PRODUCT} | tr '[A-Z]' '[a-z]'`

if [ -z "${PROD_TARGET_ARCH}" ]; then
    PROD_TARGET_ARCH=`uname -p | tr '[a-z]' '[A-Z]' | sed -e 's/I?86/I386/'`
fi
PROD_TARGET_ARCH_LC=`echo ${PROD_TARGET_ARCH} | tr '[A-Z]' '[a-z]'`

if [ -z "${PROD_TARGET_HWNAME}" ]; then
    PROD_TARGET_HWNAME=
fi
PROD_TARGET_HWNAME_LC=`echo ${PROD_TARGET_HWNAME} | tr '[A-Z]' '[a-z]'`

if [ ! -z "${PROD_TARGET_HWNAME_LC}" ]; then
    HWNAME_DASHED="${PROD_TARGET_HWNAME_LC}-"
else
    HWNAME_DASHED=
fi

BUILDS_ROOT=${PI_BUILDS_DIR}
if [ ${SCM_USE_DATE_NOW} -eq 1 ]; then
    if [ ${SCM_USE_TAG} -eq 0 ]; then
        OUTPUT_DIR_NAME=build-`hostname | sed 's/^\([^\.]*\).*$/\1/'`-${PROD_PRODUCT_LC}-${PROD_TARGET_ARCH_LC}-${HWNAME_DASHED}${BUILD_SCM_DATE_FILE}
    else
        OUTPUT_DIR_NAME=build-`hostname | sed 's/^\([^\.]*\).*$/\1/'`-${PROD_PRODUCT_LC}-${PROD_TARGET_ARCH_LC}-${HWNAME_DASHED}tag-${BUILD_SCM_TAG}-${BUILD_SCM_DATE_FILE}
    fi
else
    if [ ${SCM_USE_TAG} -eq 0 ]; then
        OUTPUT_DIR_NAME=build-`hostname | sed 's/^\([^\.]*\).*$/\1/'`-${PROD_PRODUCT_LC}-${PROD_TARGET_ARCH_LC}-${HWNAME_DASHED}date-${BUILD_SCM_DATE_FILE}
    else
        OUTPUT_DIR_NAME=build-`hostname | sed 's/^\([^\.]*\).*$/\1/'`-${PROD_PRODUCT_LC}-${PROD_TARGET_ARCH_LC}-${HWNAME_DASHED}tag-${BUILD_SCM_TAG}-date-${BUILD_SCM_DATE_FILE}
    fi
fi


# un-comment to build whatever is in PROD_TREE_ROOT instead of using cvs
##DISABLE_CVS=1

OUTPUT_DIR=${BUILDS_ROOT}/${OUTPUT_DIR_NAME}
LOG_DIR=$OUTPUT_DIR/logs

PROD_OUTPUT_ROOT=${OUTPUT_DIR}/tree/output
export PROD_OUTPUT_ROOT


# This script and the makefiles use these variables, so we export them
if [ -z "${DISABLE_CVS}" ]; then
    PROD_TREE_ROOT=$OUTPUT_DIR/tree
    export PROD_TREE_ROOT
else
    if [ -z "${PROD_TREE_ROOT}" ]; then
        echo "Must set PROD_TREE_ROOT"
        exit 1
    fi
    rm -rf ${PROD_OUTPUT_ROOT}
fi


OF=$LOG_DIR/make-log.txt
MOF=$LOG_DIR/email.txt

mkdir -m 755 -p $OUTPUT_DIR
mkdir -m 755 -p $LOG_DIR
mkdir -m 755 -p $PROD_OUTPUT_ROOT

touch $OUTPUT_DIR/dir-$OUTPUT_DIR_NAME
if [ $? -ne 0 ]; then
    echo "ERROR: could not write to output directory ${OUTPUT_DIR}"
    exit 1
fi

if [ ! -d ${PROD_TREE_ROOT} ]; then
    echo "ERROR: could not find PROD_TREE_ROOT: ${PROD_TREE_ROOT}"
    exit 1
fi

# Get previous build number if PI_PREV_PROD_OUTPUT_ROOT is set
HAS_PREV_BUILD=0
PREV_BUILD_NUM=0
BUILD_NUM=1
if [ ! -z "${PI_PREV_PROD_OUTPUT_ROOT}" ]; then
    HAS_PREV_BUILD=1

    PREV_BUILD_NUM=`echo '. ${PI_PREV_PROD_OUTPUT_ROOT}/product-*/build/release/build_version.sh; echo  $BUILD_NUMBER' | sh -`
    PREV_BUILD_ID=`echo '. ${PI_PREV_PROD_OUTPUT_ROOT}/product-*/build/release/build_version.sh; echo  $BUILD_ID' | sh -`
    PREV_BUILD_DATE=`echo '. ${PI_PREV_PROD_OUTPUT_ROOT}/product-*/build/release/build_version.sh; echo  $BUILD_DATE' | sh -`

    BUILD_NUM=`expr ${PREV_BUILD_NUM} + 1`
fi


# start CVS ========================================
if [ -z "${DISABLE_CVS}" ]; then

cd $PROD_TREE_ROOT

# Do the checkout

FAILURE=0
if [ ${SCM_USE_TAG} -eq 0 -a ${SCM_USE_DATE} -eq 1 ]; then
    ${CVS} -f -Q checkout -D "${BUILD_SCM_DATE}" . || FAILURE=1
elif [ ${SCM_USE_TAG} -eq 1 -a ${SCM_USE_DATE} -eq 0 ]; then
    ${CVS} -f -Q checkout -r ${BUILD_SCM_TAG} . || FAILURE=1
elif [ ${SCM_USE_TAG} -eq 1 -a ${SCM_USE_DATE} -eq 1 ]; then
    ${CVS} -f -Q checkout -r ${BUILD_SCM_TAG} -D "${BUILD_SCM_DATE}" . || FAILURE=1
else
    ${CVS} -f -Q checkout -A . || FAILURE=1
fi
if [ ${FAILURE} -eq 1 ]; then
    echo "Checkout failed."
    exit 1
fi

# Log what versions we are building from
${CVS} -f -q status > $LOG_DIR/cvs-status.txt || FAILURE=1
if [ ${FAILURE} -eq 1 ]; then
    echo "Status failed."
    exit 1
fi

grep 'Repository revision:' $LOG_DIR/cvs-status.txt | awk '{print $3"\t"$4}' > $LOG_DIR/cvs-versions.txt

# See if there were any changes since we started, if we're a date now type
if [ ${SCM_USE_DATE_NOW} -eq 1 ]; then

    ## Note that due to bugs in cvs 1.11.23 on el6 we are not using "cvs
    ## -n update -A" to detect changes, as it always thinks files with
    ## "-ko" expansion are changed.

    if [ ${SCM_USE_TAG} -eq 1 ]; then
        ${CVS} -Q log -S -R -rBASE::${BUILD_SCM_TAG} . > $LOG_DIR/cvs-newer-log.txt
    else
        ${CVS} -Q log -S -R -rBASE:: . > $LOG_DIR/cvs-newer-log.txt
    fi

    if [ -s "$LOG_DIR/cvs-newer-log.txt" ]; then
        # We exit, as we'd probably want the new commits, and multiple directory
        # commits aren't atomic in CVS.
        echo "Changes since build of ${BUILD_SCM_INFO} on ${BUILD_NOW_DATE} started, exiting."
        exit 1
    else
        rm -f $LOG_DIR/cvs-newer-log.txt
    fi

    if [ ! -z "${PREV_BUILD_DATE}" ]; then
        ${CVS} -q log -d "${PREV_BUILD_DATE}<${BUILD_SCM_DATE}" -S . > $LOG_DIR/cvs-changes.txt
    else
        echo "- No build previous to ${BUILD_SCM_DATE} detected" > $LOG_DIR/cvs-changes.txt
    fi
fi

fi # DISABLE_CVS
# end CVS ========================================


unset PROD_CUSTOMER
unset PROD_RELEASE
unset PROD_BUILD_OUTPUT_DIR

# In case we were called from a verbose make
unset MAKEFLAGS MFLAGS

# Now that we have a checkout, set the other vars we need
PROD_CUSTOMER=`${MAKE} -s -f ${PROD_TREE_ROOT}/src/mk/common.mk print_make_var_value-PROD_CUSTOMER`
if [ -z "${PROD_CUSTOMER}" ]; then
    echo "Must set PROD_CUSTOMER"
    exit 1
fi

# Allow PROD_ID to be taken from the environment. XXX should be command line
if [ -z "${PROD_ID}" ]; then
    PROD_ID=`${MAKE} -s -f ${PROD_TREE_ROOT}/src/mk/common.mk print_make_var_value-PROD_ID`
fi
if [ -z "${PROD_ID}" ]; then
    echo "Must set PROD_ID"
    exit 1
fi

PROD_RELEASE=`${MAKE} -s -f ${PROD_TREE_ROOT}/src/mk/common.mk print_make_var_value-PROD_RELEASE`
if [ -z "${PROD_RELEASE}" ]; then
    echo "Must set PROD_RELEASE"
    exit 1
fi

PROD_BUILD_OUTPUT_DIR=`${MAKE} -s -f ${PROD_TREE_ROOT}/src/mk/common.mk print_make_var_value-PROD_BUILD_OUTPUT_DIR`
if [ -z "${PROD_BUILD_OUTPUT_DIR}" ]; then
    echo "Must set PROD_BUILD_OUTPUT_DIR"
    exit 1
fi

PROD_RELEASE_OUTPUT_DIR=`${MAKE} -s -f ${PROD_TREE_ROOT}/src/mk/common.mk print_make_var_value-PROD_RELEASE_OUTPUT_DIR`
if [ -z "${PROD_RELEASE_OUTPUT_DIR}" ]; then
    echo "Must set PROD_RELEASE_OUTPUT_DIR"
    exit 1
fi

PROD_OUTPUT_DIR=`${MAKE} -s -f ${PROD_TREE_ROOT}/src/mk/common.mk print_make_var_value-PROD_OUTPUT_DIR`
if [ -z "${PROD_OUTPUT_DIR}" ]; then
    echo "Must set PROD_OUTPUT_DIR"
    exit 1
fi

PROD_CUSTOMER_ROOT=`${MAKE} -s -f ${PROD_TREE_ROOT}/src/mk/common.mk print_make_var_value-PROD_CUSTOMER_ROOT`
if [ -z "${PROD_CUSTOMER_ROOT}" ]; then
    echo "Must set PROD_CUSTOMER_ROOT"
    exit 1
fi

por=`${MAKE} -s -f ${PROD_TREE_ROOT}/src/mk/common.mk print_make_var_value-PROD_OUTPUT_ROOT`
if [ -z "${por}" -o "${por}" != "${PROD_OUTPUT_ROOT}" ]; then
    echo "Must set PROD_OUTPUT_ROOT (consistently)"
    exit 1
fi


# First create a build_version.conf file in the release directory
cat > ${PROD_TREE_ROOT}/src/release/build_version.conf <<EOF
PI_BUILD_PROD_TREE_ROOT=${PROD_TREE_ROOT}
PI_BUILD_DATE="${BUILD_SCM_DATE}"
PI_BUILD_TYPE=${PI_BUILD_TYPE}
PI_BUILD_NUMBER=${BUILD_NUM}
PI_BUILD_SCM_INFO=${BUILD_SCM_INFO}
EOF

# start BUILD ======================================

# Now to a "make all" at the root

cd $PROD_TREE_ROOT

rm -f $OF

echo "Build log for $BUILD_SCM_INFO on \"${BUILD_NOW_DATE}\"" >> $OF
echo "==================================================" >> $OF
date '+%Y-%m-%d %H:%M:%S' >>  $OF
echo "==================================================" >> $OF

${MAKE} ${JOB_STR} -k 'PROD_BUILD_VERBOSE=1' clean >> $OF  2>&1

make_all_err=0
make_install_err=0
make_release_err=0

date '+%Y-%m-%d %H:%M:%S' >>  $OF
echo "${MAKE} ${JOB_STR} -k 'PROD_BUILD_VERBOSE=1' all" >> $OF
${MAKE} ${JOB_STR} -k 'PROD_BUILD_VERBOSE=1' all >> $OF  2>&1 || make_all_err=1

if [ "${make_all_err}" -eq 0 ]; then
    date '+%Y-%m-%d %H:%M:%S' >>  $OF
    echo "${MAKE} ${JOB_STR} -k 'PROD_BUILD_VERBOSE=1' install" >> $OF
    ${MAKE} ${JOB_STR} -k 'PROD_BUILD_VERBOSE=1' install >> $OF  2>&1 || \
        make_install_err=1

   if [ "${make_install_err}" -eq 0 ]; then
       date '+%Y-%m-%d %H:%M:%S' >>  $OF
       echo "${MAKE} ${JOB_STR} -k 'PROD_BUILD_VERBOSE=1' release" >> $OF
       ${MAKE} ${JOB_STR} -k 'PROD_BUILD_VERBOSE=1' release >> $OF  2>&1 || \
           make_release_err=1
   fi
fi

echo "==================================================" >> $OF
date '+%Y-%m-%d %H:%M:%S' >>  $OF
echo "==================================================" >> $OF

# Figure out if this was a clean build
BUILD_ERROR_LINES=`grep -2 '^.*make[^:]*: .*\*\*\*.*' $OF | grep -v =====` 

if [ -z "${BUILD_ERROR_LINES}" -a ${make_all_err} -eq 0 -a ${make_install_err} -eq 0 -a ${make_release_err} -eq 0 ]; then
    BUILD_IS_CLEAN=1
else
    BUILD_IS_CLEAN=0
fi


# end BUILD ======================================



# Run test scripts
# XXX


# Now make up an email to send

# include:
#- path to build log
#- url of build log
#- path to image
#- url of image

if [ ${BUILD_IS_CLEAN} -eq 1 ]; then
    BUILD_SF="success"
else
    BUILD_SF="FAILURE"
fi

# Load in the build information
if [ -f ${PROD_BUILD_OUTPUT_DIR}/release/build_version.sh ]; then
    . ${PROD_BUILD_OUTPUT_DIR}/release/build_version.sh
fi



## Basic email message body

MAIL_SUBJECT="Build ${BUILD_SF} for ${PROD_CUSTOMER} product ${PROD_PRODUCT} arch ${PROD_TARGET_ARCH} id ${PROD_ID} release ${PROD_RELEASE} ${BUILD_SCM_INFO} build number ${BUILD_NUMBER}"

rm -f ${MOF}
echo "To: ${BUILD_MAIL_TO}" > ${MOF}
echo "Cc: ${BUILD_MAIL_CC}" >> ${MOF}
echo "Subject: ${MAIL_SUBJECT}" >> ${MOF}
echo "" >> ${MOF}

# Now the body of the email
 
cat >> ${MOF} <<___EOF___
==================================================

Status: 
    Build ${BUILD_SF} for ${PROD_CUSTOMER} product ${PROD_PRODUCT} arch ${PROD_TARGET_ARCH} id ${PROD_ID} release ${PROD_RELEASE} .

Version Info:
    Scm info:   ${BUILD_SCM_INFO}
    Src tree:   ${PROD_TREE_ROOT}
    Cust tree:  ${PROD_CUSTOMER_ROOT}
    Customer:   ${PROD_CUSTOMER}
    Product:    ${PROD_PRODUCT}
    Product ID: ${PROD_ID}
    Release:    ${PROD_RELEASE}
    Platform:   ${BUILD_TARGET_PLATFORM_FULL_LC}
    Arch:       ${BUILD_TARGET_ARCH_LC} / ${BUILD_TARGET_CPU_LC}
    HW name:    ${BUILD_TARGET_HWNAME}
    User:       ${BUILD_HOST_REAL_USER}
    Machine:    ${BUILD_HOST_MACHINE}
    Build args: ${JOB_STR}
    Number:     ${BUILD_NUMBER}
    Version:    ${BUILD_PROD_VERSION}

Locations:
    Build Directory: ${PROD_OUTPUT_ROOT}
    Build Log: ${OF}

___EOF___

if [ ${BUILD_IS_CLEAN} -eq 1 ]; then
    image_dir=${PROD_RELEASE_OUTPUT_DIR}/image
    IMAGE_FN=
    IMAGE_BASE_FN=
    if [ "$(echo ${image_dir}/image-*.img)" != "${image_dir}/image-*.img" ]; then
        IMAGE_FN=$(echo ${image_dir}/image-*.img | head -1)
        IMAGE_BASE_FN=$(basename ${IMAGE_FN})
    fi

    # URL
    if [ ! -z "${IMAGE_FN}" -a ! -z "${RELEASE_URL_BASE}" ]; then
        IMAGE_URL="${RELEASE_URL_BASE}/${OUTPUT_DIR_NAME}/release/image/${IMAGE_BASE_FN}"
        echo "Image URL:     ${IMAGE_URL}" >> ${MOF}
        echo "" >> ${MOF}
    fi
    # Image info
    if [ ! -z "${IMAGE_FN}" ]; then
        echo "Image info:" >> ${MOF}
        IMG_SIZE=$(du -b ${IMAGE_FN} | awk '{print $1}')
        echo "    Image file name:         ${IMAGE_BASE_FN}" >> ${MOF}
        s_mb=$(echo "${IMG_SIZE}" "1048576" "/" "p" | dc)
        echo "    Image file size:         ${IMG_SIZE} (${s_mb} MB)" >> ${MOF}

        TPKG_QUERY=${PROD_TREE_ROOT}/src/base_os/common/script_files/tpkg_query.sh

        eval `${TPKG_QUERY} -i -x -f ${IMAGE_FN} | egrep '^IMAGE_|^export IMAGE_' | sed -e 's,^IMAGE_\([A-Za-z0-9_]*\)=,II_IMAGE_\1=,' -e 's,^export IMAGE_\([A-Za-z0-9_]*\)$,export II_IMAGE_\1,'`

        if [ ! -z "${II_IMAGE_PAYLOAD_UNCOMPRESSED_SIZE}" ]; then
            us_mb=$(echo "${II_IMAGE_PAYLOAD_UNCOMPRESSED_SIZE}" "1048576" "/" "p" | dc)
            echo "    Image uncompressed size: ${II_IMAGE_PAYLOAD_UNCOMPRESSED_SIZE} (${us_mb} MB)" >> ${MOF}
            echo "    Image compression type:  ${II_IMAGE_PAYLOAD_COMPRESSION_TYPE}" >> ${MOF}
        fi
        echo "" >> ${MOF}
    fi
fi

if [ ${HAS_PREV_BUILD} -eq 1 ]; then
    echo "Changed log: (since build ${PREV_BUILD_ID} of ${PREV_BUILD_DATE})" >> ${MOF}
    echo "" >> ${MOF}

    if [ -z "${DISABLE_CVS}" ]; then
        cat $LOG_DIR/cvs-changes.txt | awk -f $PROD_TREE_ROOT/src/release/parse_cvs_log.awk >> ${MOF}
    fi # DISABLE_CVS
fi

## Failed build : append error lines
if [ ${BUILD_IS_CLEAN} -eq 0 ]; then
    echo "ERROR LOGS:" >> ${MOF}
    echo "--------------------" >> ${MOF}
    echo "${BUILD_ERROR_LINES}" >> ${MOF}
    echo "--------------------" >> ${MOF}
fi

echo "" >> ${MOF}
echo "==================================================" >> ${MOF}


## Send the email

cat ${MOF} | sendmail -t ${MAIL_TO}

if [ "${PRINT_BUILD_META}" = "1" ]; then
    echo "########## BUILD META INFORMATION: START"
    echo "BUILD_OUTPUT_DIR=\"${OUTPUT_DIR}\""
    echo "BUILD_OUTPUT_DIR_NAME=\"${OUTPUT_DIR_NAME}\""
    echo "PROD_TREE_ROOT=\"${PROD_TREE_ROOT}\""
    echo "PROD_CUSTOMER_ROOT=\"${PROD_CUSTOMER_ROOT}\""
    echo "PROD_OUTPUT_ROOT=\"${PROD_OUTPUT_ROOT}\""
    echo "PROD_OUTPUT_DIR=\"${PROD_OUTPUT_DIR}\""
    echo "PROD_CUSTOMER=\"${PROD_CUSTOMER}\""
    echo "PROD_PRODUCT=\"${PROD_PRODUCT}\""
    echo "PROD_ID=\"${PROD_ID}\""
    echo "PROD_RELEASE=\"${PROD_RELEASE}\""
    echo "PROD_TARGET_ARCH=\"${PROD_TARGET_ARCH}\""
    echo "PROD_TARGET_HWNAME=\"${PROD_TARGET_HWNAME}\""
    echo "########## BUILD META INFORMATION: END"

fi

if [ ${BUILD_IS_CLEAN} -eq 1 ]; then
    exit 0
else
    exit 2
fi

