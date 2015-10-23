#!/bin/sh
#
#
#
#

PATH=/usr/sbin:/usr/local/bin:/usr/xpg4/bin:/usr/ucb:/usr/bin:/bin:/usr/ccs/bin
export PATH

usage()
{
    echo "usage: $0 [-f build_version.conf] [PROD_TREE_ROOT BUILD_DATE BUILD_TYPE BUILD_NUMBER [BUILD_SCM_INFO]]"
    exit 1
}

if [ $# != 2 -a $# != 4 -a $# != 5 ]; then
    usage
fi

if [ $# -eq 2 ]; then
    if [ -f $2 ]; then
        . $2
    else
        echo "Could not find config file: $2"
        echo ""
        usage
    fi
fi

if [ $# -ge 4 ]; then
    PI_BUILD_PROD_TREE_ROOT=$1
    PI_BUILD_DATE=$2
    PI_BUILD_TYPE=$3
    PI_BUILD_NUMBER=$4
fi
if [ $# -eq 5 ]; then
    PI_BUILD_SCM_INFO=$5
fi


# ==================================================
# This script creates version files that C, make, and bourne shell can use
#  to learn about the version of the product.
# ==================================================

# The elements we define:
#
# BUILD_PROD_TREE_ROOT
# BUILD_PROD_CUSTOMER_ROOT
# BUILD_PROD_OUTPUT_ROOT
# BUILD_PROD_ROOT_PREFIX
# BUILD_DATE
# BUILD_TYPE
# BUILD_NUMBER
# BUILD_ID
# BUILD_SCM_INFO
# BUILD_SCM_INFO_SHORT
# BUILD_TMS_SRCS_ID
# BUILD_TMS_SRCS_RELEASE_ID
# BUILD_TMS_SRCS_BUNDLES
# BUILD_TMS_SRCS_DATE
# BUILD_TMS_SRCS_CUSTOMER
# BUILD_HOST_MACHINE
# BUILD_HOST_USER
# BUILD_HOST_REAL_USER
# BUILD_HOST_OS
# BUILD_HOST_OS_LC
# BUILD_HOST_PLATFORM
# BUILD_HOST_PLATFORM_LC
# BUILD_HOST_PLATFORM_VERSION
# BUILD_HOST_PLATFORM_VERSION_LC
# BUILD_HOST_PLATFORM_FULL
# BUILD_HOST_PLATFORM_FULL_LC
# BUILD_HOST_ARCH
# BUILD_HOST_ARCH_LC
# BUILD_HOST_CPU
# BUILD_HOST_CPU_LC
# BUILD_TARGET_OS
# BUILD_TARGET_OS_LC
# BUILD_TARGET_PLATFORM
# BUILD_TARGET_PLATFORM_LC
# BUILD_TARGET_PLATFORM_VERSION
# BUILD_TARGET_PLATFORM_VERSION_LC
# BUILD_TARGET_PLATFORM_FULL
# BUILD_TARGET_PLATFORM_FULL_LC
# BUILD_TARGET_ARCH
# BUILD_TARGET_ARCH_LC
# BUILD_TARGET_ARCH_FAMILY
# BUILD_TARGET_ARCH_FAMILY_LC
# BUILD_TARGET_CPU
# BUILD_TARGET_CPU_LC
# BUILD_TARGET_HWNAME
# BUILD_TARGET_HWNAME_LC
# BUILD_TARGET_HWNAMES_COMPAT
# BUILD_TARGET_HWNAMES_COMPAT_LC
# BUILD_PROD_PRODUCT
# BUILD_PROD_PRODUCT_LC
# BUILD_PROD_CUSTOMER
# BUILD_PROD_CUSTOMER_LC
# BUILD_PROD_ID
# BUILD_PROD_ID_LC
# BUILD_PROD_NAME
# BUILD_PROD_RELEASE
# BUILD_PROD_VERSION
# BUILD_PROD_LOCALES
# BUILD_PROD_FEATURES
#
# Elements we may define:
# BUILD_PROD_FEATURE_FRONT_PANEL
# BUILD_PROD_FEATURE_FP_CONFIG_LCD
# BUILD_PROD_FEATURE_SCHED
# BUILD_PROD_FEATURE_WIZARD
# BUILD_PROD_FEATURE_NTP_CLIENT
# BUILD_PROD_FEATURE_DHCP_CLIENT
# BUILD_PROD_FEATURE_AAA
# BUILD_PROD_FEATURE_RADIUS
# BUILD_PROD_FEATURE_TACACS
# BUILD_PROD_FEATURE_LDAP
# BUILD_PROD_FEATURE_XINETD
# BUILD_PROD_FEATURE_FTPD
# BUILD_PROD_FEATURE_TELNETD
# BUILD_PROD_FEATURE_GRAPHING
# BUILD_PROD_FEATURE_SMARTD
# BUILD_PROD_FEATURE_TMS_MIB
# BUILD_PROD_FEATURE_WEB_REF_UI
# BUILD_PROD_FEATURE_LOCKDOWN
# BUILD_PROD_FEATURE_CLUSTER
# BUILD_PROD_FEATURE_CMC_CLIENT
# BUILD_PROD_FEATURE_CMC_SERVER
# BUILD_PROD_FEATURE_GPT_PARTED
# BUILD_PROD_FEATURE_OLD_GRAPHING
#

BUILD_EXTRA_VARS=

# First validate our arguments (PI_xxx) XXX

if [ "$PI_BUILD_TYPE" != "dev" -a "$PI_BUILD_TYPE" != "spot" -a "$PI_BUILD_TYPE" != "numbered" ]; then
    PI_BUILD_TYPE="dev"
fi

if [ -z "${DATE}" ]; then
    DATE=date
fi
BUILD_DATE=$PI_BUILD_DATE
if [ ! -z "${BUILD_DATE}" ]; then
    # Normalize whatever date we're given.  This is to cover a date
    # specified in the conf file.  Nothing should change in the
    # (normal) src/release/Makefile case (as it already uses this
    # format).
    BUILD_DATE=$(${DATE} '+%Y-%m-%d %H:%M:%S' --date="${BUILD_DATE}")
else
    # Fallback on the current time.  This should only come up when a
    # conf file is in use, and the date in the conf file is missing or
    # empty, as usually the src/release/Makefile or conf file will
    # provide a date.
    BUILD_DATE=$(${DATE} '+%Y-%m-%d %H:%M:%S')
fi
BUILD_TYPE=$PI_BUILD_TYPE
BUILD_NUMBER=$PI_BUILD_NUMBER
BUILD_SCM_INFO=$PI_BUILD_SCM_INFO

# XXX The fallback is hard-coded only for CVS right now
if [ -z "${BUILD_SCM_INFO}" ]; then
    if [ ! -z "${CVSROOT}" ]; then
        BUILD_SCM_INFO=`basename ${CVSROOT}`/HEAD
    else
        BUILD_SCM_INFO=unknown
    fi
fi

if [ -z "${BUILD_SCM_INFO_SHORT}" ]; then
    BUILD_SCM_INFO_SHORT=${BUILD_SCM_INFO}
fi

case "$BUILD_TYPE" in
    numbered)
    BUILD_ID="#${BUILD_NUMBER}";
    ;;

    spot)
    BUILD_ID="spot";
    BUILD_NUMBER=0;
    ;;

    dev)
    BUILD_ID="#${BUILD_NUMBER}-${BUILD_TYPE}";
    ;;
esac

# In case we were called from a verbose make
unset MAKEFLAGS

if [ -z "${MAKE}" ]; then
    MAKE=make
fi

BUILD_PROD_TREE_ROOT=`${MAKE} -s -f ${PROD_TREE_ROOT}/src/mk/common.mk print_make_var_value-PROD_TREE_ROOT`
BUILD_PROD_CUSTOMER_ROOT=`${MAKE} -s -f ${PROD_TREE_ROOT}/src/mk/common.mk print_make_var_value-PROD_CUSTOMER_ROOT`
BUILD_PROD_OUTPUT_ROOT=`${MAKE} -s -f ${PROD_TREE_ROOT}/src/mk/common.mk print_make_var_value-PROD_OUTPUT_ROOT`
BUILD_PROD_ROOT_PREFIX=`${MAKE} -s -f ${PROD_TREE_ROOT}/src/mk/common.mk print_make_var_value-PROD_ROOT_PREFIX`

BUILD_TMS_SRCS_RELEASE_ID=`${MAKE} -s -f ${PROD_TREE_ROOT}/src/mk/common.mk print_make_var_value-PROD_TMS_SRCS_RELEASE_ID`
BUILD_TMS_SRCS_BUNDLES=`${MAKE} -s -f ${PROD_TREE_ROOT}/src/mk/common.mk print_make_var_value-PROD_TMS_SRCS_BUNDLES`
if [ -z "${BUILD_TMS_SRCS_BUNDLES}" ]; then
    BUILD_TMS_SRCS_ID="${BUILD_TMS_SRCS_RELEASE_ID}"
else
    BUILD_TMS_SRCS_ID="${BUILD_TMS_SRCS_RELEASE_ID} ${BUILD_TMS_SRCS_BUNDLES}"
fi

BUILD_TMS_SRCS_DATE=`${MAKE} -s -f ${PROD_TREE_ROOT}/src/mk/common.mk print_make_var_value-PROD_TMS_SRCS_DATE`
BUILD_TMS_SRCS_CUSTOMER=`${MAKE} -s -f ${PROD_TREE_ROOT}/src/mk/common.mk print_make_var_value-PROD_TMS_SRCS_CUSTOMER`

BUILD_HOST_MACHINE=`${MAKE} -s -f ${PROD_TREE_ROOT}/src/mk/common.mk print_make_var_value-PROD_HOST_MACHINE`
BUILD_HOST_MACHINE_SHORT=`${MAKE} -s -f ${PROD_TREE_ROOT}/src/mk/common.mk print_make_var_value-PROD_HOST_MACHINE_SHORT`
BUILD_HOST_USER=`${MAKE} -s -f ${PROD_TREE_ROOT}/src/mk/common.mk print_make_var_value-PROD_HOST_USER`
BUILD_HOST_REAL_USER=`${MAKE} -s -f ${PROD_TREE_ROOT}/src/mk/common.mk print_make_var_value-PROD_HOST_REAL_USER`

BUILD_HOST_OS=`${MAKE} -s -f ${PROD_TREE_ROOT}/src/mk/common.mk print_make_var_value-PROD_HOST_OS`
BUILD_HOST_OS_LC=`${MAKE} -s -f ${PROD_TREE_ROOT}/src/mk/common.mk print_make_var_value-PROD_HOST_OS_LC`
BUILD_HOST_PLATFORM=`${MAKE} -s -f ${PROD_TREE_ROOT}/src/mk/common.mk print_make_var_value-PROD_HOST_PLATFORM`
BUILD_HOST_PLATFORM_LC=`${MAKE} -s -f ${PROD_TREE_ROOT}/src/mk/common.mk print_make_var_value-PROD_HOST_PLATFORM_LC`
BUILD_HOST_PLATFORM_VERSION=`${MAKE} -s -f ${PROD_TREE_ROOT}/src/mk/common.mk print_make_var_value-PROD_HOST_PLATFORM_VERSION`
BUILD_HOST_PLATFORM_VERSION_LC=`${MAKE} -s -f ${PROD_TREE_ROOT}/src/mk/common.mk print_make_var_value-PROD_HOST_PLATFORM_VERSION_LC`
BUILD_HOST_PLATFORM_FULL=`${MAKE} -s -f ${PROD_TREE_ROOT}/src/mk/common.mk print_make_var_value-PROD_HOST_PLATFORM_FULL`
BUILD_HOST_PLATFORM_FULL_LC=`${MAKE} -s -f ${PROD_TREE_ROOT}/src/mk/common.mk print_make_var_value-PROD_HOST_PLATFORM_FULL_LC`
BUILD_HOST_ARCH=`${MAKE} -s -f ${PROD_TREE_ROOT}/src/mk/common.mk print_make_var_value-PROD_HOST_ARCH`
BUILD_HOST_ARCH_LC=`${MAKE} -s -f ${PROD_TREE_ROOT}/src/mk/common.mk print_make_var_value-PROD_HOST_ARCH_LC`
BUILD_HOST_CPU=`${MAKE} -s -f ${PROD_TREE_ROOT}/src/mk/common.mk print_make_var_value-PROD_HOST_CPU`
BUILD_HOST_CPU_LC=`${MAKE} -s -f ${PROD_TREE_ROOT}/src/mk/common.mk print_make_var_value-PROD_HOST_CPU_LC`

BUILD_TARGET_OS=`${MAKE} -s -f ${PROD_TREE_ROOT}/src/mk/common.mk print_make_var_value-PROD_TARGET_OS`
BUILD_TARGET_OS_LC=`${MAKE} -s -f ${PROD_TREE_ROOT}/src/mk/common.mk print_make_var_value-PROD_TARGET_OS_LC`
BUILD_TARGET_PLATFORM=`${MAKE} -s -f ${PROD_TREE_ROOT}/src/mk/common.mk print_make_var_value-PROD_TARGET_PLATFORM`
BUILD_TARGET_PLATFORM_LC=`${MAKE} -s -f ${PROD_TREE_ROOT}/src/mk/common.mk print_make_var_value-PROD_TARGET_PLATFORM_LC`
BUILD_TARGET_PLATFORM_VERSION=`${MAKE} -s -f ${PROD_TREE_ROOT}/src/mk/common.mk print_make_var_value-PROD_TARGET_PLATFORM_VERSION`
BUILD_TARGET_PLATFORM_VERSION_LC=`${MAKE} -s -f ${PROD_TREE_ROOT}/src/mk/common.mk print_make_var_value-PROD_TARGET_PLATFORM_VERSION_LC`
BUILD_TARGET_PLATFORM_FULL=`${MAKE} -s -f ${PROD_TREE_ROOT}/src/mk/common.mk print_make_var_value-PROD_TARGET_PLATFORM_FULL`
BUILD_TARGET_PLATFORM_FULL_LC=`${MAKE} -s -f ${PROD_TREE_ROOT}/src/mk/common.mk print_make_var_value-PROD_TARGET_PLATFORM_FULL_LC`
BUILD_TARGET_ARCH=`${MAKE} -s -f ${PROD_TREE_ROOT}/src/mk/common.mk print_make_var_value-PROD_TARGET_ARCH`
BUILD_TARGET_ARCH_LC=`${MAKE} -s -f ${PROD_TREE_ROOT}/src/mk/common.mk print_make_var_value-PROD_TARGET_ARCH_LC`
BUILD_TARGET_ARCH_FAMILY=`${MAKE} -s -f ${PROD_TREE_ROOT}/src/mk/common.mk print_make_var_value-PROD_TARGET_ARCH_FAMILY`
BUILD_TARGET_ARCH_FAMILY_LC=`${MAKE} -s -f ${PROD_TREE_ROOT}/src/mk/common.mk print_make_var_value-PROD_TARGET_ARCH_FAMILY_LC`
BUILD_TARGET_CPU=`${MAKE} -s -f ${PROD_TREE_ROOT}/src/mk/common.mk print_make_var_value-PROD_TARGET_CPU`
BUILD_TARGET_CPU_LC=`${MAKE} -s -f ${PROD_TREE_ROOT}/src/mk/common.mk print_make_var_value-PROD_TARGET_CPU_LC`
BUILD_TARGET_HWNAME=`${MAKE} -s -f ${PROD_TREE_ROOT}/src/mk/common.mk print_make_var_value-PROD_TARGET_HWNAME`
BUILD_TARGET_HWNAME_LC=`${MAKE} -s -f ${PROD_TREE_ROOT}/src/mk/common.mk print_make_var_value-PROD_TARGET_HWNAME_LC`
BUILD_TARGET_HWNAMES_COMPAT=`${MAKE} -s -f ${PROD_TREE_ROOT}/src/mk/common.mk print_make_var_value-PROD_TARGET_HWNAMES_COMPAT`
BUILD_TARGET_HWNAMES_COMPAT_LC=`${MAKE} -s -f ${PROD_TREE_ROOT}/src/mk/common.mk print_make_var_value-PROD_TARGET_HWNAMES_COMPAT_LC`
BUILD_PROD_FEATURES_ENABLED=`${MAKE} -s -f ${PROD_TREE_ROOT}/src/mk/common.mk print_make_var_value-PROD_FEATURES_ENABLED`
BUILD_PROD_FEATURES_ENABLED_LC=`echo $BUILD_PROD_FEATURES_ENABLED | tr '[A-Z]' '[a-z]'`
BUILD_PROD_LOCALES=`${MAKE} -s -f ${PROD_TREE_ROOT}/src/mk/common.mk print_make_var_value-PROD_LOCALES`

BUILD_PROD_PRODUCT=`${MAKE} -s -f ${PROD_TREE_ROOT}/src/mk/common.mk print_make_var_value-PROD_PRODUCT`
BUILD_PROD_PRODUCT_LC=`${MAKE} -s -f ${PROD_TREE_ROOT}/src/mk/common.mk print_make_var_value-PROD_PRODUCT_LC`
BUILD_PROD_CUSTOMER=`${MAKE} -s -f ${PROD_TREE_ROOT}/src/mk/common.mk print_make_var_value-PROD_CUSTOMER`
BUILD_PROD_CUSTOMER_LC=`${MAKE} -s -f ${PROD_TREE_ROOT}/src/mk/common.mk print_make_var_value-PROD_CUSTOMER_LC`
BUILD_PROD_ID=`${MAKE} -s -f ${PROD_TREE_ROOT}/src/mk/common.mk print_make_var_value-PROD_ID`
BUILD_PROD_ID_LC=`echo $BUILD_PROD_ID | tr '[A-Z]' '[a-z]'`
BUILD_PROD_RELEASE=`${MAKE} -s -f ${PROD_TREE_ROOT}/src/mk/common.mk print_make_var_value-PROD_RELEASE`

for pfe in ${BUILD_PROD_FEATURES_ENABLED}; do
    eval 'BUILD_PROD_FEATURE_'${pfe}'='`${MAKE} -s -f ${PROD_TREE_ROOT}/src/mk/common.mk print_make_var_value-PROD_FEATURE_${pfe}`
done

# Set the customer root the lower case product name so we can read in any
# customer.sh .


eval `${MAKE} -s -f ${PROD_TREE_ROOT}/src/mk/common.mk print_make_var_export-PROD_CUSTOMER_ROOT`
eval `${MAKE} -s -f ${PROD_TREE_ROOT}/src/mk/common.mk print_make_var_export-PROD_PRODUCT_LC`

if [ -f ${PROD_CUSTOMER_ROOT}/${PROD_PRODUCT_LC}/src/base_os/common/script_files/customer.sh ]; then
    . ${PROD_CUSTOMER_ROOT}/${PROD_PRODUCT_LC}/src/base_os/common/script_files/customer.sh
fi

# uname -s : operating system implementation name
BUILD_PROD_NAME=${BUILD_PROD_PRODUCT_LC}

if [ "$HAVE_MKVER_GRAFT_1" = "y" ]; then
    mkver_graft_1
fi

# uname -v : operating system version
BUILD_PROD_VERSION="${BUILD_PROD_NAME} ${BUILD_PROD_RELEASE} ${BUILD_ID} ${BUILD_DATE} ${BUILD_TARGET_ARCH_LC}${BUILD_TARGET_HWNAME_LC:+ ${BUILD_TARGET_HWNAME_LC}} ${BUILD_HOST_REAL_USER}@${BUILD_HOST_MACHINE_SHORT}:${BUILD_SCM_INFO_SHORT}"

# Customers can update the variables already set, and add new ones to
# the BUILD_EXTRA_VARS variable, which will cause them to be included in
# the build_version.* files.

if [ "$HAVE_MKVER_GRAFT_2" = "y" ]; then
    mkver_graft_2
fi

for xv in ${BUILD_EXTRA_VARS}; do
    okay=0
    case "${xv}" in
        BUILD_[A-Z0-9_]*) okay=1;;
    esac
    if [ ${okay} -ne 1 ]; then
        echo "Error: bad BUILD_EXTRA_VARS: ${xv}"
        exit 1
    fi
done

echo "${BUILD_PROD_RELEASE}" | grep -q '["&,\\`@]'
if [ $? -eq 0 ]; then
    echo "Error: bad characters in BUILD_PROD_RELEASE : ${BUILD_PROD_RELEASE}"
    exit 1
fi
echo "${BUILD_PROD_VERSION}" | grep -q '["&,\\`]'
if [ $? -eq 0 ]; then
    echo "Error: bad characters in BUILD_PROD_VERSION : ${BUILD_PROD_VERSION}"
    exit 1
fi
echo "${BUILD_PROD_VERSION}" | grep -q '@.*@'
if [ $? -eq 0 ]; then
    echo "Error: bad multiple @'s in BUILD_PROD_VERSION : ${BUILD_PROD_VERSION}"
    exit 1
fi
echo "${BUILD_PROD_VERSION}" | grep -q "'"
if [ $? -eq 0 ]; then
    echo "Error: bad character ' in BUILD_PROD_VERSION : ${BUILD_PROD_VERSION}"
    exit 1
fi

# Set our function-name-suitable versioning variables
BV_FNAME_BUILD_TMS_SRCS_ID=$(echo "${BUILD_TMS_SRCS_ID}" | sed 's/[^a-zA-Z0-9_]/_/g' | tr '[A-Z]' '[a-z]')
BV_FNAME_BUILD_PROD_VERSION=$(echo "${BUILD_PROD_VERSION}" | sed 's/[^a-zA-Z0-9_]/_/g' | tr '[A-Z]' '[a-z]')

if [ -z "${BV_FNAME_BUILD_PROD_VERSION}" -o -z "${BV_FNAME_BUILD_TMS_SRCS_ID}" ]; then
    echo "Error: invalid versioning function names"
    exit 1
fi

# ==================================================
# By this point, all the BUILD_xxx vars are set correctly, and do not change.  
#
# Now we're on to file generation
# ==================================================

eval `${MAKE} -s -f ${PROD_TREE_ROOT}/src/mk/common.mk print_make_var_export-PROD_BUILD_OUTPUT_DIR`

MKVER_OUTDIR=${PROD_BUILD_OUTPUT_DIR}/release

# Filenames of different versions
MVF_C=${MKVER_OUTDIR}/build_version.c
MVF_SH=${MKVER_OUTDIR}/build_version.sh
MVF_MK=${MKVER_OUTDIR}/build_version.mk
MVF_TXT=${MKVER_OUTDIR}/build_version.txt
MVF_PL=${MKVER_OUTDIR}/build_version.pl

BUILD_AUTO_GEN="Automatically generated file: DO NOT EDIT!"

# Output C names, 

cat > ${MVF_C} <<EOF
#include <stdlib.h>
#include "build_version.h"

/*
 * ${BUILD_AUTO_GEN}
 */

const char build_prod_tree_root[] = "${BUILD_PROD_TREE_ROOT}";
const char build_prod_customer_root[] = "${PROD_CUSTOMER_ROOT}";
const char build_prod_output_root[] = "${PROD_OUTPUT_ROOT}";
const char build_prod_root_prefix[] = "${PROD_ROOT_PREFIX}";
const char build_date[] = "${BUILD_DATE}";
const char build_type[] = "${BUILD_TYPE}";
const char build_number[] = "${BUILD_NUMBER}";
const long int build_number_int = ${BUILD_NUMBER};
const char build_id[] = "${BUILD_ID}";
const char build_scm_info[] = "${BUILD_SCM_INFO}";
const char build_scm_info_short[] = "${BUILD_SCM_INFO_SHORT}";
const char build_tms_srcs_id[] = "${BUILD_TMS_SRCS_ID}";
const char build_tms_srcs_release_id[] = "${BUILD_TMS_SRCS_RELEASE_ID}";
const char build_tms_srcs_bundles[] = "${BUILD_TMS_SRCS_BUNDLES}";
const char build_tms_srcs_date[] = "${BUILD_TMS_SRCS_DATE}";
const char build_tms_srcs_customer[] = "${BUILD_TMS_SRCS_CUSTOMER}";
const char build_host_machine[] = "${BUILD_HOST_MACHINE}";
const char build_host_user[] = "${BUILD_HOST_USER}";
const char build_host_real_user[] = "${BUILD_HOST_REAL_USER}";
const char build_host_os[] = "${BUILD_HOST_OS}";
const char build_host_os_lc[] = "${BUILD_HOST_OS_LC}";
const char build_host_platform[] = "${BUILD_HOST_PLATFORM}";
const char build_host_platform_lc[] = "${BUILD_HOST_PLATFORM_LC}";
const char build_host_platform_version[] = "${BUILD_HOST_PLATFORM_VERSION}";
const char build_host_platform_version_lc[] = "${BUILD_HOST_PLATFORM_VERSION_LC}";
const char build_host_platform_full[] = "${BUILD_HOST_PLATFORM_FULL}";
const char build_host_platform_full_lc[] = "${BUILD_HOST_PLATFORM_FULL_LC}";
const char build_host_arch[] = "${BUILD_HOST_ARCH}";
const char build_host_arch_lc[] = "${BUILD_HOST_ARCH_LC}";
const char build_host_cpu[] = "${BUILD_HOST_CPU}";
const char build_host_cpu_lc[] = "${BUILD_HOST_CPU_LC}";
const char build_target_os[] = "${BUILD_TARGET_OS}";
const char build_target_os_lc[] = "${BUILD_TARGET_OS_LC}";
const char build_target_platform[] = "${BUILD_TARGET_PLATFORM}";
const char build_target_platform_lc[] = "${BUILD_TARGET_PLATFORM_LC}";
const char build_target_platform_version[] = "${BUILD_TARGET_PLATFORM_VERSION}";
const char build_target_platform_version_lc[] = "${BUILD_TARGET_PLATFORM_VERSION_LC}";
const char build_target_platform_full[] = "${BUILD_TARGET_PLATFORM_FULL}";
const char build_target_platform_full_lc[] = "${BUILD_TARGET_PLATFORM_FULL_LC}";
const char build_target_arch[] = "${BUILD_TARGET_ARCH}";
const char build_target_arch_lc[] = "${BUILD_TARGET_ARCH_LC}";
const char build_target_arch_family[] = "${BUILD_TARGET_ARCH_FAMILY}";
const char build_target_arch_family_lc[] = "${BUILD_TARGET_ARCH_FAMILY_LC}";
const char build_target_cpu[] = "${BUILD_TARGET_CPU}";
const char build_target_cpu_lc[] = "${BUILD_TARGET_CPU_LC}";
const char build_target_hwname[] = "${BUILD_TARGET_HWNAME}";
const char build_target_hwname_lc[] = "${BUILD_TARGET_HWNAME_LC}";
const char build_target_hwnames_compat[] = "${BUILD_TARGET_HWNAMES_COMPAT}";
const char build_target_hwnames_compat_lc[] = "${BUILD_TARGET_HWNAMES_COMPAT_LC}";
const char build_prod_product[] = "${BUILD_PROD_PRODUCT}";
const char build_prod_product_lc[] = "${BUILD_PROD_PRODUCT_LC}";
const char build_prod_customer[] = "${BUILD_PROD_CUSTOMER}";
const char build_prod_customer_lc[] = "${BUILD_PROD_CUSTOMER_LC}";
const char build_prod_id[] = "${BUILD_PROD_ID}";
const char build_prod_id_lc[] = "${BUILD_PROD_ID_LC}";
const char build_prod_name[] = "${BUILD_PROD_NAME}";
const char build_prod_release[] = "${BUILD_PROD_RELEASE}";
const char build_prod_version[] = "${BUILD_PROD_VERSION}";
const char build_prod_features_enabled[] = "${BUILD_PROD_FEATURES_ENABLED}";
const char build_prod_features_enabled_lc[] = "${BUILD_PROD_FEATURES_ENABLED_LC}";
const char build_prod_locales[] = "${BUILD_PROD_LOCALES}";

/*
 * If 'keep_ref' is true, we will keep a reference to the memory leaked
 * here.  A referenced leak will be free'd on any subsequent call with
 * 'keep_ref' true, and a new referenced leak will potentially be made
 * (0 means "unleak" with no new leak).  If keep_ref is false, the leak
 * is a new, separate leak, and there's no way to "unleak" it.
 */

#define BV_FORCE_LEAK(keep_ref, leak_size)                           \\
   do {                                                              \\
        static void *leak = NULL;                                    \\
        static unsigned long refd_leak_size = 0;                     \\
                                                                     \\
        if (keep_ref && refd_leak_size > 0) {                        \\
            if (leak) {                                              \\
                free(leak);                                          \\
                leak = NULL;                                         \\
            }                                                        \\
            refd_leak_size = 0;                                      \\
        }                                                            \\
                                                                     \\
        if (leak_size > 0) {                                         \\
            leak = malloc(leak_size);                                \\
            if (!keep_ref) {                                         \\
                leak = NULL;  /* Definitely leak */                  \\
            }                                                        \\
            else {                                                   \\
                refd_leak_size = leak_size;                          \\
            }                                                        \\
        }                                                            \\
   } while(0)

static int
bv_versions_leak_N_prod_version_V_${BV_FNAME_BUILD_PROD_VERSION}(int keep_ref,
                                                      unsigned long leak_size)
{
    BV_FORCE_LEAK(keep_ref, leak_size);
    return(0);
}

static int
bv_versions_leak_N_tms_srcs_id_V_${BV_FNAME_BUILD_TMS_SRCS_ID}(int keep_ref, 
                                                      unsigned long leak_size)
{
    return(bv_versions_leak_N_prod_version_V_${BV_FNAME_BUILD_PROD_VERSION}(
                                                                 keep_ref, 
                                                                 leak_size));
}

int
bv_versions_leak_init(int keep_ref, unsigned long leak_size)
{
    return(bv_versions_leak_N_tms_srcs_id_V_${BV_FNAME_BUILD_TMS_SRCS_ID}(
                                                           keep_ref, 
                                                           leak_size));
}

EOF
for xv in ${BUILD_EXTRA_VARS}; do
    xvl=$(echo ${xv} | tr '[A-Z]' '[a-z]')
    eval 'xvv="${'${xv}'}"'
    echo "const char ${xvl}[] = \"${xvv}\";" >> ${MVF_C}
done

# Output sh (bourne shell) names

cat > ${MVF_SH} <<EOF
# ${BUILD_AUTO_GEN}
#
BUILD_PROD_TREE_ROOT="${BUILD_PROD_TREE_ROOT}"
export BUILD_PROD_TREE_ROOT
BUILD_PROD_CUSTOMER_ROOT="${BUILD_PROD_CUSTOMER_ROOT}"
export BUILD_PROD_CUSTOMER_ROOT
BUILD_PROD_OUTPUT_ROOT="${BUILD_PROD_OUTPUT_ROOT}"
export BUILD_PROD_OUTPUT_ROOT
BUILD_PROD_ROOT_PREFIX="${BUILD_PROD_ROOT_PREFIX}"
export BUILD_PROD_ROOT_PREFIX
BUILD_DATE="${BUILD_DATE}"
export BUILD_DATE
BUILD_TYPE="${BUILD_TYPE}"
export BUILD_TYPE
BUILD_NUMBER="${BUILD_NUMBER}"
export BUILD_NUMBER
BUILD_ID="${BUILD_ID}"
export BUILD_ID
BUILD_SCM_INFO="${BUILD_SCM_INFO}"
export BUILD_SCM_INFO
BUILD_SCM_INFO_SHORT="${BUILD_SCM_INFO_SHORT}"
export BUILD_SCM_INFO_SHORT
BUILD_TMS_SRCS_ID="${BUILD_TMS_SRCS_ID}"
export BUILD_TMS_SRCS_ID
BUILD_TMS_SRCS_RELEASE_ID="${BUILD_TMS_SRCS_RELEASE_ID}"
export BUILD_TMS_SRCS_RELEASE_ID
BUILD_TMS_SRCS_BUNDLES="${BUILD_TMS_SRCS_BUNDLES}"
export BUILD_TMS_SRCS_BUNDLES
BUILD_TMS_SRCS_DATE="${BUILD_TMS_SRCS_DATE}"
export BUILD_TMS_SRCS_DATE
BUILD_TMS_SRCS_CUSTOMER="${BUILD_TMS_SRCS_CUSTOMER}"
export BUILD_TMS_SRCS_CUSTOMER
BUILD_HOST_MACHINE="${BUILD_HOST_MACHINE}"
export BUILD_HOST_MACHINE
BUILD_HOST_USER="${BUILD_HOST_USER}"
export BUILD_HOST_USER
BUILD_HOST_REAL_USER="${BUILD_HOST_REAL_USER}"
export BUILD_HOST_REAL_USER
BUILD_HOST_OS="${BUILD_HOST_OS}"
export BUILD_HOST_OS
BUILD_HOST_OS_LC="${BUILD_HOST_OS_LC}"
export BUILD_HOST_OS_LC
BUILD_HOST_PLATFORM="${BUILD_HOST_PLATFORM}"
export BUILD_HOST_PLATFORM
BUILD_HOST_PLATFORM_LC="${BUILD_HOST_PLATFORM_LC}"
export BUILD_HOST_PLATFORM_LC
BUILD_HOST_PLATFORM_VERSION="${BUILD_HOST_PLATFORM_VERSION}"
export BUILD_HOST_PLATFORM_VERSION
BUILD_HOST_PLATFORM_VERSION_LC="${BUILD_HOST_PLATFORM_VERSION_LC}"
export BUILD_HOST_PLATFORM_VERSION_LC
BUILD_HOST_PLATFORM_FULL="${BUILD_HOST_PLATFORM_FULL}"
export BUILD_HOST_PLATFORM_FULL
BUILD_HOST_PLATFORM_FULL_LC="${BUILD_HOST_PLATFORM_FULL_LC}"
export BUILD_HOST_PLATFORM_FULL_LC
BUILD_HOST_ARCH="${BUILD_HOST_ARCH}"
export BUILD_HOST_ARCH
BUILD_HOST_ARCH_LC="${BUILD_HOST_ARCH_LC}"
export BUILD_HOST_ARCH_LC
BUILD_HOST_CPU="${BUILD_HOST_CPU}"
export BUILD_HOST_CPU
BUILD_HOST_CPU_LC="${BUILD_HOST_CPU_LC}"
export BUILD_HOST_CPU_LC
BUILD_TARGET_OS="${BUILD_TARGET_OS}"
export BUILD_TARGET_OS
BUILD_TARGET_OS_LC="${BUILD_TARGET_OS_LC}"
export BUILD_TARGET_OS_LC
BUILD_TARGET_PLATFORM="${BUILD_TARGET_PLATFORM}"
export BUILD_TARGET_PLATFORM
BUILD_TARGET_PLATFORM_LC="${BUILD_TARGET_PLATFORM_LC}"
export BUILD_TARGET_PLATFORM_LC
BUILD_TARGET_PLATFORM_VERSION="${BUILD_TARGET_PLATFORM_VERSION}"
export BUILD_TARGET_PLATFORM_VERSION
BUILD_TARGET_PLATFORM_VERSION_LC="${BUILD_TARGET_PLATFORM_VERSION_LC}"
export BUILD_TARGET_PLATFORM_VERSION_LC
BUILD_TARGET_PLATFORM_FULL="${BUILD_TARGET_PLATFORM_FULL}"
export BUILD_TARGET_PLATFORM_FULL
BUILD_TARGET_PLATFORM_FULL_LC="${BUILD_TARGET_PLATFORM_FULL_LC}"
export BUILD_TARGET_PLATFORM_FULL_LC
BUILD_TARGET_ARCH="${BUILD_TARGET_ARCH}"
export BUILD_TARGET_ARCH
BUILD_TARGET_ARCH_LC="${BUILD_TARGET_ARCH_LC}"
export BUILD_TARGET_ARCH_LC
BUILD_TARGET_ARCH_FAMILY="${BUILD_TARGET_ARCH_FAMILY}"
export BUILD_TARGET_ARCH_FAMILY
BUILD_TARGET_ARCH_FAMILY_LC="${BUILD_TARGET_ARCH_FAMILY_LC}"
export BUILD_TARGET_ARCH_FAMILY_LC
BUILD_TARGET_CPU="${BUILD_TARGET_CPU}"
export BUILD_TARGET_CPU
BUILD_TARGET_CPU_LC="${BUILD_TARGET_CPU_LC}"
export BUILD_TARGET_CPU_LC
BUILD_TARGET_HWNAME="${BUILD_TARGET_HWNAME}"
export BUILD_TARGET_HWNAME
BUILD_TARGET_HWNAME_LC="${BUILD_TARGET_HWNAME_LC}"
export BUILD_TARGET_HWNAME_LC
BUILD_TARGET_HWNAMES_COMPAT="${BUILD_TARGET_HWNAMES_COMPAT}"
export BUILD_TARGET_HWNAMES_COMPAT
BUILD_TARGET_HWNAMES_COMPAT_LC="${BUILD_TARGET_HWNAMES_COMPAT_LC}"
export BUILD_TARGET_HWNAMES_COMPAT_LC
BUILD_PROD_PRODUCT="${BUILD_PROD_PRODUCT}"
export BUILD_PROD_PRODUCT
BUILD_PROD_PRODUCT_LC="${BUILD_PROD_PRODUCT_LC}"
export BUILD_PROD_PRODUCT_LC
BUILD_PROD_CUSTOMER="${BUILD_PROD_CUSTOMER}"
export BUILD_PROD_CUSTOMER
BUILD_PROD_CUSTOMER_LC="${BUILD_PROD_CUSTOMER_LC}"
export BUILD_PROD_CUSTOMER_LC
BUILD_PROD_ID="${BUILD_PROD_ID}"
export BUILD_PROD_ID
BUILD_PROD_ID_LC="${BUILD_PROD_ID_LC}"
export BUILD_PROD_ID_LC
BUILD_PROD_NAME="${BUILD_PROD_NAME}"
export BUILD_PROD_NAME
BUILD_PROD_RELEASE="${BUILD_PROD_RELEASE}"
export BUILD_PROD_RELEASE
BUILD_PROD_VERSION="${BUILD_PROD_VERSION}"
export BUILD_PROD_VERSION
BUILD_PROD_FEATURES_ENABLED="${BUILD_PROD_FEATURES_ENABLED}"
export BUILD_PROD_FEATURES_ENABLED
BUILD_PROD_FEATURES_ENABLED_LC="${BUILD_PROD_FEATURES_ENABLED_LC}"
export BUILD_PROD_FEATURES_ENABLED_LC
BUILD_PROD_LOCALES="${BUILD_PROD_LOCALES}"
export BUILD_PROD_LOCALES
EOF

for pfe in ${BUILD_PROD_FEATURES_ENABLED}; do
    eval 'curr_value="${BUILD_PROD_FEATURE_'${pfe}'}"'
    echo 'BUILD_PROD_FEATURE_'${pfe}'="'${curr_value}'"' >> ${MVF_SH}
    echo "export BUILD_PROD_FEATURE_${pfe}" >> ${MVF_SH}
done
for xv in ${BUILD_EXTRA_VARS}; do
    xvl=$(echo ${xv} | tr '[A-Z]' '[a-z]')
    eval 'xvv="${'${xv}'}"'
    echo "${xv}=\"${xvv}\"" >> ${MVF_SH}
    echo "export ${xv}" >> ${MVF_SH}
done

# Output makefile names

cat > ${MVF_MK} <<EOF
# ${BUILD_AUTO_GEN}
#
export BUILD_PROD_TREE_ROOT="${BUILD_PROD_TREE_ROOT}"
export BUILD_PROD_CUSTOMER_ROOT="${BUILD_PROD_CUSTOMER_ROOT}"
export BUILD_PROD_OUTPUT_ROOT="${BUILD_PROD_OUTPUT_ROOT}"
export BUILD_PROD_ROOT_PREFIX="${BUILD_PROD_ROOT_PREFIX}"
export BUILD_DATE="${BUILD_DATE}"
export BUILD_TYPE="${BUILD_TYPE}"
export BUILD_NUMBER="${BUILD_NUMBER}"
export BUILD_ID="${BUILD_ID}"
export BUILD_SCM_INFO="${BUILD_SCM_INFO}"
export BUILD_SCM_INFO_SHORT="${BUILD_SCM_INFO_SHORT}"
export BUILD_TMS_SRCS_ID="${BUILD_TMS_SRCS_ID}"
export BUILD_TMS_SRCS_RELEASE_ID="${BUILD_TMS_SRCS_RELEASE_ID}"
export BUILD_TMS_SRCS_BUNDLES="${BUILD_TMS_SRCS_BUNDLES}"
export BUILD_TMS_SRCS_DATE="${BUILD_TMS_SRCS_DATE}"
export BUILD_TMS_SRCS_CUSTOMER="${BUILD_TMS_SRCS_CUSTOMER}"
export BUILD_HOST_MACHINE="${BUILD_HOST_MACHINE}"
export BUILD_HOST_USER="${BUILD_HOST_USER}"
export BUILD_HOST_REAL_USER="${BUILD_HOST_REAL_USER}"
export BUILD_HOST_OS="${BUILD_HOST_OS}"
export BUILD_HOST_OS_LC="${BUILD_HOST_OS_LC}"
export BUILD_HOST_PLATFORM="${BUILD_HOST_PLATFORM}"
export BUILD_HOST_PLATFORM_LC="${BUILD_HOST_PLATFORM_LC}"
export BUILD_HOST_PLATFORM_VERSION="${BUILD_HOST_PLATFORM_VERSION}"
export BUILD_HOST_PLATFORM_VERSION_LC="${BUILD_HOST_PLATFORM_VERSION_LC}"
export BUILD_HOST_PLATFORM_FULL="${BUILD_HOST_PLATFORM_FULL}"
export BUILD_HOST_PLATFORM_FULL_LC="${BUILD_HOST_PLATFORM_FULL_LC}"
export BUILD_HOST_ARCH="${BUILD_HOST_ARCH}"
export BUILD_HOST_ARCH_LC="${BUILD_HOST_ARCH_LC}"
export BUILD_HOST_CPU="${BUILD_HOST_CPU}"
export BUILD_HOST_CPU_LC="${BUILD_HOST_CPU_LC}"
export BUILD_TARGET_OS="${BUILD_TARGET_OS}"
export BUILD_TARGET_OS_LC="${BUILD_TARGET_OS_LC}"
export BUILD_TARGET_PLATFORM="${BUILD_TARGET_PLATFORM}"
export BUILD_TARGET_PLATFORM_LC="${BUILD_TARGET_PLATFORM_LC}"
export BUILD_TARGET_PLATFORM_VERSION="${BUILD_TARGET_PLATFORM_VERSION}"
export BUILD_TARGET_PLATFORM_VERSION_LC="${BUILD_TARGET_PLATFORM_VERSION_LC}"
export BUILD_TARGET_PLATFORM_FULL="${BUILD_TARGET_PLATFORM_FULL}"
export BUILD_TARGET_PLATFORM_FULL_LC="${BUILD_TARGET_PLATFORM_FULL_LC}"
export BUILD_TARGET_ARCH="${BUILD_TARGET_ARCH}"
export BUILD_TARGET_ARCH_LC="${BUILD_TARGET_ARCH_LC}"
export BUILD_TARGET_ARCH_FAMILY="${BUILD_TARGET_ARCH_FAMILY}"
export BUILD_TARGET_ARCH_FAMILY_LC="${BUILD_TARGET_ARCH_FAMILY_LC}"
export BUILD_TARGET_CPU="${BUILD_TARGET_CPU}"
export BUILD_TARGET_CPU_LC="${BUILD_TARGET_CPU_LC}"
export BUILD_TARGET_HWNAME="${BUILD_TARGET_HWNAME}"
export BUILD_TARGET_HWNAME_LC="${BUILD_TARGET_HWNAME_LC}"
export BUILD_TARGET_HWNAMES_COMPAT="${BUILD_TARGET_HWNAMES_COMPAT}"
export BUILD_TARGET_HWNAMES_COMPAT_LC="${BUILD_TARGET_HWNAMES_COMPAT_LC}"
export BUILD_PROD_PRODUCT="${BUILD_PROD_PRODUCT}"
export BUILD_PROD_PRODUCT_LC="${BUILD_PROD_PRODUCT_LC}"
export BUILD_PROD_CUSTOMER="${BUILD_PROD_CUSTOMER}"
export BUILD_PROD_CUSTOMER_LC="${BUILD_PROD_CUSTOMER_LC}"
export BUILD_PROD_ID="${BUILD_PROD_ID}"
export BUILD_PROD_ID_LC="${BUILD_PROD_ID_LC}"
export BUILD_PROD_NAME="${BUILD_PROD_NAME}"
export BUILD_PROD_RELEASE="${BUILD_PROD_RELEASE}"
export BUILD_PROD_VERSION="${BUILD_PROD_VERSION}"
export BUILD_PROD_FEATURES_ENABLED="${BUILD_PROD_FEATURES_ENABLED}"
export BUILD_PROD_FEATURES_ENABLED_LC="${BUILD_PROD_FEATURES_ENABLED_LC}"
export BUILD_PROD_LOCALES="${BUILD_PROD_LOCALES}"
EOF

for pfe in ${BUILD_PROD_FEATURES_ENABLED}; do
    eval 'curr_value="${BUILD_PROD_FEATURE_'${pfe}'}"'
    echo 'export BUILD_PROD_FEATURE_'${pfe}'="'${curr_value}'"' >> ${MVF_MK}
done

for xv in ${BUILD_EXTRA_VARS}; do
    xvl=$(echo ${xv} | tr '[A-Z]' '[a-z]')
    eval 'xvv="${'${xv}'}"'
    echo "export ${xv}=\"${xvv}\"" >> ${MVF_MK}
done


# Output perl names

cat > ${MVF_PL} <<EOF
# ${BUILD_AUTO_GEN}
#
\$BUILD_PROD_TREE_ROOT="${BUILD_PROD_TREE_ROOT}";
\$BUILD_PROD_CUSTOMER_ROOT="${BUILD_PROD_CUSTOMER_ROOT}";
\$BUILD_PROD_OUTPUT_ROOT="${BUILD_PROD_OUTPUT_ROOT}";
\$BUILD_PROD_ROOT_PREFIX="${BUILD_PROD_ROOT_PREFIX}";
\$BUILD_DATE="${BUILD_DATE}";
\$BUILD_TYPE="${BUILD_TYPE}";
\$BUILD_NUMBER="${BUILD_NUMBER}";
\$BUILD_ID="${BUILD_ID}";
\$BUILD_SCM_INFO="${BUILD_SCM_INFO}";
\$BUILD_SCM_INFO_SHORT="${BUILD_SCM_INFO_SHORT}";
\$BUILD_TMS_SRCS_ID="${BUILD_TMS_SRCS_ID}";
\$BUILD_TMS_SRCS_RELEASE_ID="${BUILD_TMS_SRCS_RELEASE_ID}";
\$BUILD_TMS_SRCS_BUNDLES="${BUILD_TMS_SRCS_BUNDLES}";
\$BUILD_TMS_SRCS_DATE="${BUILD_TMS_SRCS_DATE}";
\$BUILD_TMS_SRCS_CUSTOMER="${BUILD_TMS_SRCS_CUSTOMER}";
\$BUILD_HOST_MACHINE="${BUILD_HOST_MACHINE}";
\$BUILD_HOST_USER="${BUILD_HOST_USER}";
\$BUILD_HOST_REAL_USER="${BUILD_HOST_REAL_USER}";
\$BUILD_HOST_OS="${BUILD_HOST_OS}";
\$BUILD_HOST_OS_LC="${BUILD_HOST_OS_LC}";
\$BUILD_HOST_PLATFORM="${BUILD_HOST_PLATFORM}";
\$BUILD_HOST_PLATFORM_LC="${BUILD_HOST_PLATFORM_LC}";
\$BUILD_HOST_PLATFORM_VERSION="${BUILD_HOST_PLATFORM_VERSION}";
\$BUILD_HOST_PLATFORM_VERSION_LC="${BUILD_HOST_PLATFORM_VERSION_LC}";
\$BUILD_HOST_PLATFORM_FULL="${BUILD_HOST_PLATFORM_FULL}";
\$BUILD_HOST_PLATFORM_FULL_LC="${BUILD_HOST_PLATFORM_FULL_LC}";
\$BUILD_HOST_ARCH="${BUILD_HOST_ARCH}";
\$BUILD_HOST_ARCH_LC="${BUILD_HOST_ARCH_LC}";
\$BUILD_HOST_CPU="${BUILD_HOST_CPU}";
\$BUILD_HOST_CPU_LC="${BUILD_HOST_CPU_LC}";
\$BUILD_TARGET_OS="${BUILD_TARGET_OS}";
\$BUILD_TARGET_OS_LC="${BUILD_TARGET_OS_LC}";
\$BUILD_TARGET_PLATFORM="${BUILD_TARGET_PLATFORM}";
\$BUILD_TARGET_PLATFORM_LC="${BUILD_TARGET_PLATFORM_LC}";
\$BUILD_TARGET_PLATFORM_VERSION="${BUILD_TARGET_PLATFORM_VERSION}";
\$BUILD_TARGET_PLATFORM_VERSION_LC="${BUILD_TARGET_PLATFORM_VERSION_LC}";
\$BUILD_TARGET_PLATFORM_FULL="${BUILD_TARGET_PLATFORM_FULL}";
\$BUILD_TARGET_PLATFORM_FULL_LC="${BUILD_TARGET_PLATFORM_FULL_LC}";
\$BUILD_TARGET_ARCH="${BUILD_TARGET_ARCH}";
\$BUILD_TARGET_ARCH_LC="${BUILD_TARGET_ARCH_LC}";
\$BUILD_TARGET_ARCH_FAMILY="${BUILD_TARGET_ARCH_FAMILY}";
\$BUILD_TARGET_ARCH_FAMILY_LC="${BUILD_TARGET_ARCH_FAMILY_LC}";
\$BUILD_TARGET_CPU="${BUILD_TARGET_CPU}";
\$BUILD_TARGET_CPU_LC="${BUILD_TARGET_CPU_LC}";
\$BUILD_TARGET_HWNAME="${BUILD_TARGET_HWNAME}";
\$BUILD_TARGET_HWNAME_LC="${BUILD_TARGET_HWNAME_LC}";
\$BUILD_TARGET_HWNAMES_COMPAT="${BUILD_TARGET_HWNAMES_COMPAT}";
\$BUILD_TARGET_HWNAMES_COMPAT_LC="${BUILD_TARGET_HWNAMES_COMPAT_LC}";
\$BUILD_PROD_PRODUCT="${BUILD_PROD_PRODUCT}";
\$BUILD_PROD_PRODUCT_LC="${BUILD_PROD_PRODUCT_LC}";
\$BUILD_PROD_CUSTOMER="${BUILD_PROD_CUSTOMER}";
\$BUILD_PROD_CUSTOMER_LC="${BUILD_PROD_CUSTOMER_LC}";
\$BUILD_PROD_ID="${BUILD_PROD_ID}";
\$BUILD_PROD_ID_LC="${BUILD_PROD_ID_LC}";
\$BUILD_PROD_NAME="${BUILD_PROD_NAME}";
\$BUILD_PROD_RELEASE="${BUILD_PROD_RELEASE}";
\$BUILD_PROD_VERSION="${BUILD_PROD_VERSION}";
\$BUILD_PROD_FEATURES_ENABLED="${BUILD_PROD_FEATURES_ENABLED}";
\$BUILD_PROD_FEATURES_ENABLED_LC="${BUILD_PROD_FEATURES_ENABLED_LC}";
\$BUILD_PROD_LOCALES="${BUILD_PROD_LOCALES}";
EOF

for pfe in ${BUILD_PROD_FEATURES_ENABLED}; do
    eval 'curr_value="${BUILD_PROD_FEATURE_'${pfe}'}"'
    echo '$BUILD_PROD_FEATURE_'${pfe}'="'${curr_value}'";' >> ${MVF_PL}
done

for xv in ${BUILD_EXTRA_VARS}; do
    xvl=$(echo ${xv} | tr '[A-Z]' '[a-z]')
    eval 'xvv="${'${xv}'}"'
    echo "\$${xv}=\"${xvv}\";" >> ${MVF_PL}
done


# Output text-oriented names

cat > ${MVF_TXT} <<EOF
# ${BUILD_AUTO_GEN}
#
BUILD_PROD_TREE_ROOT:             ${BUILD_PROD_TREE_ROOT}
BUILD_PROD_CUSTOMER_ROOT:         ${BUILD_PROD_CUSTOMER_ROOT}
BUILD_PROD_OUTPUT_ROOT:           ${BUILD_PROD_OUTPUT_ROOT}
BUILD_PROD_ROOT_PREFIX:           ${BUILD_PROD_ROOT_PREFIX}
BUILD_DATE:                       ${BUILD_DATE}
BUILD_TYPE:                       ${BUILD_TYPE}
BUILD_NUMBER:                     ${BUILD_NUMBER}
BUILD_ID:                         ${BUILD_ID}
BUILD_SCM_INFO                    ${BUILD_SCM_INFO}
BUILD_SCM_INFO_SHORT              ${BUILD_SCM_INFO_SHORT}
BUILD_TMS_SRCS_ID                 ${BUILD_TMS_SRCS_ID}
BUILD_TMS_SRCS_RELEASE_ID         ${BUILD_TMS_SRCS_RELEASE_ID}
BUILD_TMS_SRCS_BUNDLES            ${BUILD_TMS_SRCS_BUNDLES}
BUILD_TMS_SRCS_DATE               ${BUILD_TMS_SRCS_DATE}
BUILD_TMS_SRCS_CUSTOMER           ${BUILD_TMS_SRCS_CUSTOMER}
BUILD_HOST_MACHINE:               ${BUILD_HOST_MACHINE}
BUILD_HOST_USER:                  ${BUILD_HOST_USER}
BUILD_HOST_REAL_USER:             ${BUILD_HOST_REAL_USER}
BUILD_HOST_OS:                    ${BUILD_HOST_OS}
BUILD_HOST_OS_LC:                 ${BUILD_HOST_OS_LC}
BUILD_HOST_PLATFORM:              ${BUILD_HOST_PLATFORM}
BUILD_HOST_PLATFORM_LC:           ${BUILD_HOST_PLATFORM_LC}
BUILD_HOST_PLATFORM_VERSION:      ${BUILD_HOST_PLATFORM_VERSION}
BUILD_HOST_PLATFORM_VERSION_LC:   ${BUILD_HOST_PLATFORM_VERSION_LC}
BUILD_HOST_PLATFORM_FULL:         ${BUILD_HOST_PLATFORM_FULL}
BUILD_HOST_PLATFORM_FULL_LC:      ${BUILD_HOST_PLATFORM_FULL_LC}
BUILD_HOST_ARCH:                  ${BUILD_HOST_ARCH}
BUILD_HOST_ARCH_LC:               ${BUILD_HOST_ARCH_LC}
BUILD_HOST_CPU:                   ${BUILD_HOST_CPU}
BUILD_HOST_CPU_LC:                ${BUILD_HOST_CPU_LC}
BUILD_TARGET_OS:                  ${BUILD_TARGET_OS}
BUILD_TARGET_OS_LC:               ${BUILD_TARGET_OS_LC}
BUILD_TARGET_PLATFORM:            ${BUILD_TARGET_PLATFORM}
BUILD_TARGET_PLATFORM_LC:         ${BUILD_TARGET_PLATFORM_LC}
BUILD_TARGET_PLATFORM_VERSION:    ${BUILD_TARGET_PLATFORM_VERSION}
BUILD_TARGET_PLATFORM_VERSION_LC: ${BUILD_TARGET_PLATFORM_VERSION_LC}
BUILD_TARGET_PLATFORM_FULL:       ${BUILD_TARGET_PLATFORM_FULL}
BUILD_TARGET_PLATFORM_FULL_LC:    ${BUILD_TARGET_PLATFORM_FULL_LC}
BUILD_TARGET_ARCH:                ${BUILD_TARGET_ARCH}
BUILD_TARGET_ARCH_LC:             ${BUILD_TARGET_ARCH_LC}
BUILD_TARGET_ARCH_FAMILY:         ${BUILD_TARGET_ARCH_FAMILY}
BUILD_TARGET_ARCH_FAMILY_LC:      ${BUILD_TARGET_ARCH_FAMILY_LC}
BUILD_TARGET_CPU:                 ${BUILD_TARGET_CPU}
BUILD_TARGET_CPU_LC:              ${BUILD_TARGET_CPU_LC}
BUILD_TARGET_HWNAME:              ${BUILD_TARGET_HWNAME}
BUILD_TARGET_HWNAME_LC:           ${BUILD_TARGET_HWNAME_LC}
BUILD_TARGET_HWNAMES_COMPAT:      ${BUILD_TARGET_HWNAMES_COMPAT}
BUILD_TARGET_HWNAMES_COMPAT_LC:   ${BUILD_TARGET_HWNAMES_COMPAT_LC}
BUILD_PROD_PRODUCT:               ${BUILD_PROD_PRODUCT}
BUILD_PROD_PRODUCT_LC:            ${BUILD_PROD_PRODUCT_LC}
BUILD_PROD_CUSTOMER:              ${BUILD_PROD_CUSTOMER}
BUILD_PROD_CUSTOMER_LC:           ${BUILD_PROD_CUSTOMER_LC}
BUILD_PROD_ID:                    ${BUILD_PROD_ID}
BUILD_PROD_ID_LC:                 ${BUILD_PROD_ID_LC}
BUILD_PROD_NAME:                  ${BUILD_PROD_NAME}
BUILD_PROD_RELEASE:               ${BUILD_PROD_RELEASE}
BUILD_PROD_VERSION:               ${BUILD_PROD_VERSION}
BUILD_PROD_FEATURES_ENABLED:      ${BUILD_PROD_FEATURES_ENABLED}
BUILD_PROD_FEATURES_ENABLED_LC:   ${BUILD_PROD_FEATURES_ENABLED_LC}
BUILD_PROD_LOCALES:               ${BUILD_PROD_LOCALES}
EOF

for pfe in ${BUILD_PROD_FEATURES_ENABLED}; do
    eval 'curr_value="${BUILD_PROD_FEATURE_'${pfe}'}"'
    printf '%-35s: %s\n' 'BUILD_PROD_FEATURE_'${pfe} ${curr_value} >> ${MVF_TXT}
done

for xv in ${BUILD_EXTRA_VARS}; do
    xvl=$(echo ${xv} | tr '[A-Z]' '[a-z]')
    eval 'xvv="${'${xv}'}"'
    printf '%-35s: %s\n' "${xv}" "${xvv}" >> ${MVF_TXT}
done
