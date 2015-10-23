# This is included from ${PROD_TREE_ROOT}/mk/common.mk
# 
# Define an identifier for your company.  You must set this.  It should be
# all uppercase.
#
PROD_CUSTOMER=JUNIPER

#
# Define an identifier for your product.  You need one of these even if you
# have only one product.  It should be all uppercase.  This could be
# overridden by include.mk if you have multiple product id's built from the
# same product tree.
#
PROD_ID?=MFC

#
# Set PROD_RELEASE here.  Generally it is expected be based on PROD_ID.
# NOTE: PROD_RELEASE is already set at this point via the
# ${PROD_TREE_ROOT}/src/release/mkver.sh graft functions
# in base_os/generic/script_files/customer.sh:
#   BUILD_PROD_RELEASE="${NKN_BUILD_PROD_RELEASE}"
# So the setting here is not normally used because NKN_BUILD_PROD_RELEASE
# is normally set in the build environment.
ifeq (${PROD_ID},MFC)
  PROD_RELEASE?="15.0.0"
endif
# NOTE: When the Copyright Year Needs Changing, two files need updating.
#                --------- ---- ----- -------- 
# These are:
#   include/customer.h
#   bin/web/templates/customer-defines.tem
# Note: The copyright year is automatically set at build time in the generated
# file copyright.txt by the script release/mkmfg_graft.sh.  This file is placed
# on the installation CD filesystem in the root.  It is not used otherwise.
#

export PROD_DIST_ROOT?=${PROD_CUSTOMER_ROOT}/dist
export DIST_EL_EL6?=${PROD_DIST_ROOT}/centos/6


# PROD_LINUX_* settings define the locations of Linux binary and source rpms.
export PROD_LINUX_ROOT?=/volume/ssd-linux-storage01/repo/ssdlinux/scl6/release/6.6.R1.0
export PROD_LINUX_U_ROOT?=/volume/ssd-linux-storage01/repo/ssdlinux/scl6/release/6.6.R1.0/SRs/6.6.R1.1
export PROD_LINUX_X86_64_RPMS_DIR=${PROD_LINUX_ROOT}/x86_64/rpm
export PROD_LINUX_I386_RPMS_DIR=${PROD_LINUX_ROOT}/i386/rpm
export PROD_LINUX_SRPMS_DIR=${PROD_LINUX_ROOT}/srpm
export PROD_LINUX_U_X86_64_RPMS_DIR=${PROD_LINUX_ROOT}/x86_64/rpm
export PROD_LINUX_U_I386_RPMS_DIR=${PROD_LINUX_ROOT}/i386/rpm
export PROD_LINUX_U_SRPMS_DIR=${PROD_LINUX_ROOT}/srpm


ifdef BUILD_ZVM
  CUSTOMER_CC_WRAPPER=/usr/local/zvm-release/bin/zcc --zcfg=$(PROD_CUSTOMER_ROOT)/$(PROD_PRODUCT)/src/mk/zmake.cfg --zarch=x86_64 --zcovdb=$(PROD_OUTPUT_ROOT) --zroot=/log/zvm
  CUSTOMER_CXX_WRAPPER=${CUSTOMER_CC_WRAPPER}
  CUSTOMER_LD_WRAPPER=
  CUSTOMER_DEFINES+= -D__ZVM__
else
  CUSTOMER_CC_WRAPPER=
  CUSTOMER_CXX_WRAPPER=
  CUSTOMER_LD_WRAPPER=
endif

# The following WARNING_* settings are used in $PROD_TREE_ROOT/src/mk/common.mk
#
# When WARNINGS_OVERRIDE is 0 or not defined then these are set:
#  PRE_C_WARNINGS+= -ll -Wextra -Wshadow -Wpointer-arith -Wredundant-decls -Wundef -Winit-self -Wmissing-declarations -Wbad-function-cast -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs -Wwrite-strings
#
# When WARNINGS_EXTRA_OVERRIDE is 0 or not defined then these are set:
#  PRE_C_WARNINGS+= -Wformat-security -Wmissing-format-attribute -Winline 
#
# When WARNINGS_ARE_ERRORS is 1 or not defined then these are set:
#   PRE_C_WARNINGS+=-Werror
#   PRE_CXX_WARNINGS+=-Werror
#
ifndef WARNINGS_OVERRIDE
  ifndef BUILD_WARNINGS_SUPPRESS
    WARNINGS_OVERRIDE=0
  else
    # Suppress warnings
    WARNINGS_OVERRIDE=1
  endif
endif
ifndef WARNINGS_EXTRA_OVERRIDE
  ifndef BUILD_WARNINGS_NO_EXTRA
    WARNINGS_EXTRA_OVERRIDE=0
  else
    # Suppress warnings
    WARNINGS_EXTRA_OVERRIDE=1
  endif
endif
ifndef WARNINGS_ARE_ERRORS
  ifndef BUILD_WARNINGS_OK
    WARNINGS_ARE_ERRORS=1
  else
    # Suppress errors
    WARNINGS_ARE_ERRORS=0
  endif
endif

KERNEL_USE_VER=2.6.32-el6
export KERNEL_USE_VER

#
# Define __JUNIPER_DEV__ to indicate that we are doing a DEV
# build rather than a QA or RC build.  We might have slightly
# different functionality for this type of build.
#
ifeq (${NKN_BUILD_TYPE},dev)
  CUSTOMER_DEFINES+= -D__JUNIPER_DEV_BUILD__
endif

export PROD_CUSTOMER
export PROD_ID
export PROD_RELEASE
export CUSTOMER_CC_WRAPPER
export CUSTOMER_LD_WRAPPER
export CUSTOMER_CXX_WRAPPER

IMAGE_PKG_SIGN_RESTRICT_OVERRIDE=1
export IMAGE_PKG_SIGN_RESTRICT_OVERRIDE

# ==============================================
# JUNIPER's MFC related feature set
# ==============================================
#

# Set the build to be be verbose 
PROD_BUILD_VERBOSE=1

# Various features that can be omitted.  Set to 0 if you want to disable.
# All of the first group of features are built by default (i.e. they
# are implicitly set to 1).
#
## PROD_FEATURE_SCHED=1
PROD_FEATURE_WIZARD=0
PROD_FEATURE_ACCOUNTING=0
PROD_FEATURE_CHANGE_AUDITING=0
PROD_FEATURE_ACTION_AUDITING=0
## PROD_FEATURE_NTP_CLIENT=1
## PROD_FEATURE_DHCP_CLIENT=1
## PROD_FEATURE_ZEROCONF=1
## PROD_FEATURE_AAA=1
## PROD_FEATURE_ACCOUNTING=1
## PROD_FEATURE_RADIUS=1
## PROD_FEATURE_TACACS=1
## PROD_FEATURE_LDAP=1
## PROD_FEATURE_XINETD=1
PROD_FEATURE_FTPD=0
## PROD_FEATURE_TELNETD=1
## PROD_FEATURE_GRAPHING=1
## PROD_FEATURE_SMARTD=1
## PROD_FEATURE_TMS_MIB=1
PROD_FEATURE_TMS_MIB=0
## PROD_FEATURE_WEB_REF_UI=1
## PROD_FEATURE_MULTI_USER=1
## PROD_FEATURE_RENAME_IFS=1
PROD_FEATURE_KERNEL_IMAGE_UNI=0
## PROD_FEATURE_KERNEL_IMAGE_SMP=1
## PROD_FEATURE_KERNEL_BOOTFLOPPY_UNI=1
## PROD_FEATURE_MULTITHREADING=1
## PROD_FEATURE_CRYPTO=1
PROD_FEATURE_IMAGE_SECURITY=0
PROD_FEATURE_VLAN=0
PROD_FEATURE_CHANGE_AUDITING=0
PROD_FEATURE_ACTION_AUDITING=0
## PROD_FEATURE_XML_GW=1
## PROD_FEATURE_OBFUSCATE_NONADMIN=1
## PROD_FEATURE_CAPABS=1

#
# Various features that can be enabled.  Set to 1 if you want to enable.
# All of this group of features are disabled by default (i.e. they
# are implicitly set to 0).
#
# PROD_FEATURE_JUNIPER_MEDIAFLOW=1 causes certain rpms to be included.
#  See Samara tree/src/base_os/linux_el/el6/arch_x86_64/image_packages/Makefile
PROD_FEATURE_JUNIPER_MEDIAFLOW=1
## PROD_FEATURE_FRONT_PANEL=0
## PROD_FEATURE_FP_CONFIG_LCD=0
## PROD_FEATURE_HW_RNG=0
## PROD_FEATURE_LOCKDOWN=0
## PROD_FEATURE_CMC_CLIENT=0
## PROD_FEATURE_CMC_CLIENT_LICENSED=0
## PROD_FEATURE_CMC_SERVER=0
## PROD_FEATURE_CMC_SERVER_LICENSED=0
## PROD_FEATURE_I18N_SUPPORT=0
## PROD_FEATURE_I18N_EXTRAS=0
## PROD_FEATURE_CLUSTER=0
## PROD_FEATURE_EXTERNAL_DOC=0
## PROD_FEATURE_TEST=0
## PROD_FEATURE_KERNELS_EXTERNAL=0
# The BRIDGING feature is required when the VIRT feature is enabled.
PROD_FEATURE_BRIDGING=1
PROD_FEATURE_BONDING=1
PROD_FEATURE_WEB_COMBINED_LOGIN=1
PROD_FEATURE_GPT_PARTED=1
## PROD_FEATURE_UBOOT=0
## PROD_FEATURE_EETOOL=0
## PROD_FEATURE_NVS=0
# Removed PROD_FEATURE_RESTRICT_CMDS in 12.3.7/halifax
## PROD_FEATURE_RESTRICT_CMDS=1
## PROD_FEATURE_SHELL_ACCESS=0 NEW in Ginkgo
## PROD_FEATURE_DEV_ENV=0
PROD_FEATURE_SECURE_NODES=1
## PROD_FEATURE_SNMP_SETS=0
## PROD_FEATURE_JAVA=0
## PROD_FEATURE_OLD_GRAPHING=0
PROD_FEATURE_AUTH_SMTP=1
PROD_FEATURE_UCD_SNMP_MIB=1
## PROD_FEATURE_SNMP_AGENTX=0
PROD_FEATURE_IPTABLES=1
## PROD_FEATURE_REMOTE_GCL=0
PROD_FEATURE_IPV6=1
PROD_FEATURE_VIRT=1
PROD_FEATURE_VIRT_KVM=1
## PROD_FEATURE_VIRT_XEN=0 (Not not ready for use in Ginkgo)
## PROD_FEATURE_VIRT_LICENSED=1
## PROD_FEATURE_ACLS=0
## PROD_FEATURE_ADMIN_NOT_SUPERUSER=0
## PROD_FEATURE_WGET=0      NEW in Ginkgo
## PROD_FEATURE_OLD_GDB=0   NEW in Elm
## PROD_FEATURE_OLD_RSYNC=0 NEW in Ginkgo

# from /build/samara/current-285/tree/customer/demo/src/mk/customer.mk <FIR>
#
# Uncomment this to disable all TMS-provided IPv4-related functions.
# This may be a useful part of an IPv6 transition, as first changing all
# uint32 -> bn_ipv4addr, etc., will make it more clear how and where to
# make changes for IPv6.  This is strongly recommended for all
# customers turning on PROD_FEATURE_IPV6.
#
CUST_PRE_DEFINES=\
        -DTMSLIBS_DISABLE_IPV4_UINT32=1 \



# ==============================================
# PLEASE DO NOT EDIT THE LINES BELOW 
# THESE ARE THE DEFAULT SETTINGS USED AS AN FYI
# ==============================================
#
# Define a customized optional feature.  The Samara platform defines
# several PROD_FEATUREs of its own, listed in src/mk/common.mk at the
# definitions of PROD_FEATURES_DEFAULT_ENABLED and
# PROD_FEATURES_DEFAULT_DISABLED (showing which ones are enabled, or
# disabled, by default).  The code here shows how to define your own
# optional features.
#
# Quite simply, just add the name of the feature, in all caps, to
# one of those two variables.  Here, we show a feature that is enabled
# by default.  If you want to enable or disable a feature for certain
# of your PROD_IDs, you can set the corresponding PROD_FEATURE as you
# do with any of the base platform's features.
#
#PROD_FEATURES_DEFAULT_ENABLED+= \
#	MFCFUNCS\

#
# Listing of base platform PROD_FEATUREs that are enabled by default.
# This should mirror what you see defined with PROD_FEATURES_DEFAULT_ENABLED
# in common.mk.  Set to 0 if you want to disable.
#

## PROD_FEATURE_ACCOUNTING=1
## PROD_FEATURE_SCHED=1
## PROD_FEATURE_WIZARD=1
## PROD_FEATURE_NTP_CLIENT=1
## PROD_FEATURE_DHCP_CLIENT=1
## PROD_FEATURE_ZEROCONF=1
## PROD_FEATURE_AAA=1
## PROD_FEATURE_RADIUS=1
## PROD_FEATURE_TACACS=1
## PROD_FEATURE_LDAP=1
## PROD_FEATURE_XINETD=1
## PROD_FEATURE_FTPD=1
## PROD_FEATURE_TELNETD=1
## PROD_FEATURE_GRAPHING=1
## PROD_FEATURE_SMARTD=1
## PROD_FEATURE_TMS_MIB=1
## PROD_FEATURE_WEB_REF_UI=1
## PROD_FEATURE_MULTI_USER=1
## PROD_FEATURE_RENAME_IFS=1
## PROD_FEATURE_MULTITHREADING=1
## PROD_FEATURE_CRYPTO=1
## PROD_FEATURE_IMAGE_SECURITY=1
## PROD_FEATURE_VLAN=1
## PROD_FEATURE_CHANGE_AUDITING=1
## PROD_FEATURE_ACTION_AUDITING=1
## PROD_FEATURE_XML_GW=1
## PROD_FEATURE_OBFUSCATE_NONADMIN=1
## PROD_FEATURE_CAPABS=1

## PROD_FEATURE_KERNEL_IMAGE_SMP=1
## PROD_FEATURE_KERNEL_BOOTFLOPPY_UNI=1

#
# Listing of base platform PROD_FEATUREs that are disabled by default.
# This should mirror what you see defined with PROD_FEATURES_DEFAULT_DISABLED
# in common.mk.  Set to 1 if you want to enable.
#

## PROD_FEATURE_KERNEL_IMAGE_UNI=0
## PROD_FEATURE_JUNIPER_MEDIAFLOW=0
## PROD_FEATURE_FRONT_PANEL=0
## PROD_FEATURE_FP_CONFIG_LCD=0
## PROD_FEATURE_HW_RNG=0
## PROD_FEATURE_LOCKDOWN=0
## PROD_FEATURE_CMC_CLIENT=0
## PROD_FEATURE_CMC_CLIENT_LICENSED=0
## PROD_FEATURE_CMC_SERVER=0
## PROD_FEATURE_CMC_SERVER_LICENSED=0
## PROD_FEATURE_I18N_SUPPORT=0
## PROD_FEATURE_I18N_EXTRAS=0
## PROD_FEATURE_CLUSTER=0
## PROD_FEATURE_EXTERNAL_DOC=0
## PROD_FEATURE_TEST=0
## PROD_FEATURE_KERNELS_EXTERNAL=0
## PROD_FEATURE_BRIDGING=0
## PROD_FEATURE_BONDING=0
## PROD_FEATURE_WEB_COMBINED_LOGIN=0
## PROD_FEATURE_GPT_PARTED=0
## PROD_FEATURE_UBOOT=0
## PROD_FEATURE_EETOOL=0
## PROD_FEATURE_NVS=0
## PROD_FEATURE_RESTRICT_CMDS=0
## PROD_FEATURE_SHELL_ACCESS=0 NEW in Ginkgo
## PROD_FEATURE_DEV_ENV=0
## PROD_FEATURE_SECURE_NODES=0
## PROD_FEATURE_SNMP_SETS=0
## PROD_FEATURE_JAVA=0
## PROD_FEATURE_OLD_GRAPHING=0
## PROD_FEATURE_AUTH_SMTP=0
## PROD_FEATURE_UCD_SNMP_MIB=0
## PROD_FEATURE_SNMP_AGENTX=0
## PROD_FEATURE_IPTABLES=0
## PROD_FEATURE_REMOTE_GCL=0
## PROD_FEATURE_IPV6=0
## PROD_FEATURE_VIRT=0
## PROD_FEATURE_VIRT_KVM=0
## PROD_FEATURE_VIRT_XEN=0 (Not not ready for use in Ginkgo)
## PROD_FEATURE_VIRT_LICENSED=0
## PROD_FEATURE_ACLS=0
## PROD_FEATURE_ADMIN_NOT_SUPERUSER=0 (Not not ready for use in Ginkgo)
## PROD_FEATURE_WGET=0      NEW in Ginkgo
## PROD_FEATURE_OLD_GDB=0   NEW in Elm
## PROD_FEATURE_OLD_RSYNC=0 NEW in Ginkgo

