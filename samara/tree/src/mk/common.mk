# Common code for Makefiles

CURR_MAKEFILE_NAME:=$(firstword ${MAKEFILE_LIST})
CURR_MAKEFILE_DIR:=${CURDIR}
ifeq ("$(strip ${CURR_MAKEFILE_NAME})","Makefile")
	CURR_MAKEFILE:=
else
	CURR_MAKEFILE:=-f ${CURR_MAKEFILE_NAME}
endif

# PROD_TREE_ROOT, the root of the Samara source tree, MUST be set in the environment.

ifeq ("$(strip ${PROD_TREE_ROOT})","")
        $(error PROD_TREE_ROOT must be set to the root of the Samara source tree.)
endif
export PROD_TREE_ROOT

# PROD_CUSOMTER_ROOT, the root of the application source tree, MUST be set in the environment.
ifeq ("$(strip ${PROD_CUSTOMER_ROOT})","")
        $(error PROD_CUSTOMER_ROOT must be set to the root of the application source tree.)
endif
export PROD_CUSTOMER_ROOT

export PROD_SRC_ROOT:=${PROD_TREE_ROOT}/src
export PROD_OUTPUT_ROOT?=${PROD_TREE_ROOT}/output

PROD_BUILD_VERBOSE?=0
PROD_BUILD_TIMES?=0
PROD_INSTALL_PREFIX?=/opt/tms
PROD_FEATURES_DEFAULT_ENABLED=
PROD_FEATURES_DEFAULT_DISABLED=

# For any extra global user includes
# Note that this is included twice, once before customer.mk and once after.
# It is needed before because it may define PROD_PRODUCT, which is needed
# to find the correct customer.mk.  It is needed after because it may want
# to override settings which were defaulted by customer.mk, such as PROD_ID.
-include ${PROD_SRC_ROOT}/mk/include.mk

PROD_PRODUCT?=nokeena
ifndef PROD_PRODUCT_LC
        export PROD_PRODUCT_LC:=$(shell echo ${PROD_PRODUCT} | tr '[A-Z]' '[a-z]')
endif
export PROD_PRODUCT

export PROD_PRODUCT_ROOT?=${PROD_CUSTOMER_ROOT}/${PROD_PRODUCT_LC}


# The target id lets us specify a target settings file to include below.
# The 'auto' file uses the host compiler, and targets the same platform as
# the host.
PROD_TARGET_ID?=auto

# For product-specific global variables
-include ${PROD_CUSTOMER_ROOT}/${PROD_PRODUCT_LC}/src/mk/customer.mk

# Include this again to give it a chance to override anything defaulted
# by customer.mk, such as the PROD_ID.
-include ${PROD_SRC_ROOT}/mk/include.mk

# PROD_DIST_ROOT?= is set in ${PROD_CUSTOMER_ROOT}/${PROD_PRODUCT_LC}/src/mk/customer.mk
# DIST_EL_EL6?=    is set in ${PROD_CUSTOMER_ROOT}/${PROD_PRODUCT_LC}/src/mk/customer.mk

# PROD_LINUX_* settings define the locations of Linux binary and source rpms.
# These are set are set in ${PROD_CUSTOMER_ROOT}/${PROD_PRODUCT_LC}/src/mk/customer.mk
#export PROD_LINUX_ROOT
#export PROD_LINUX_U_ROOT
#export PROD_LINUX_X86_64_RPMS_DIR
#export PROD_LINUX_I386_RPMS_DIR
#export PROD_LINUX_SRPMS_DIR
#export PROD_LINUX_U_X86_64_RPMS_DIR
#export PROD_LINUX_U_I386_RPMS_DIR
#export PROD_LINUX_U_SRPMS_DIR

# Include TMS versioning and bundle information
ifndef TMS_SRCS_RELEASE_DONE
include ${PROD_SRC_ROOT}/release/version/release.tms.mk
export TMS_SRCS_RELEASE_DONE:=1
endif

# If we localize Samara, PROD_BASE_I18N_ROOT would want to change to
# ${PROD_TREE_ROOT}/i18n.  But PROD_PRODUCT_I18N_ROOT will probably
# always want to remain the same.
export PROD_BASE_I18N_ROOT?=${PROD_PRODUCT_ROOT}/i18n
export PROD_PRODUCT_I18N_ROOT?=${PROD_PRODUCT_ROOT}/i18n

PROD_CUSTOMER?=UNSET_CUSTOMER
ifndef PROD_CUSTOMER_LC
        export PROD_CUSTOMER_LC:=$(shell echo ${PROD_CUSTOMER} | tr '[A-Z]' '[a-z]')
endif
export PROD_CUSTOMER

# Make sure these got set to something
export PROD_ID?=UNSET_ID
export PROD_RELEASE?=UNSET_RELEASE


# Define some variables shared between our makefiles
export PROD_OUTPUT_DIR=${PROD_OUTPUT_ROOT}/product-${PROD_PRODUCT_LC}-${PROD_TARGET_ID_NAME}
export PROD_BUILD_ROOT=${PROD_OUTPUT_DIR}/build
export PROD_BUILD_OUTPUT_DIR=${PROD_BUILD_ROOT}
export PROD_INSTALL_ROOT=${PROD_OUTPUT_DIR}/install
export PROD_INSTALL_OUTPUT_DIR=${PROD_INSTALL_ROOT}
export PROD_RELEASE_ROOT=${PROD_OUTPUT_DIR}/release
export PROD_RELEASE_OUTPUT_DIR=${PROD_RELEASE_ROOT}






# A few binaries we always use from the host system, regardless of the
# target

ECHO=echo
SED?=sed
INSTALL?=install
CP?=cp
MV?=mv
RM?=rm
DATE?=date

# If we haven't already done so, compute some internal (reserved) variables,
# and from those export PROD_HOST_*

ifndef RES_HOST_DONE
 
	# 
	# Figure out about the operating system we are running on
	#
	# This will set:
	#      RES_HOST_OS
	#      RES_HOST_PLATFORM
	#      RES_HOST_PLATFORM_VERSION
	# 

	RES_HOST_OS:=$(shell uname -s | tr '[a-z]' '[A-Z]')


	#
	# Figure out who we are: we may need to know if we're root or not
	#
	ID=id -un
	ifndef RES_WHOAMI
		RES_WHOAMI:=$(shell ${ID})
	endif
	ifndef RES_REAL_USER
		RES_SUDO_USER:=${SUDO_USER}
		RES_LOGNAME:=${LOGNAME}
		ifeq ("$(strip ${RES_SUDO_USER})","")
			ifeq ("$(strip ${RES_LOGNAME})","")
				RES_REAL_USER:=${RES_WHOAMI}
			else
				RES_REAL_USER:=${RES_LOGNAME}
			endif
		else
			RES_REAL_USER:=${RES_SUDO_USER}
		endif
	endif

	ifeq (${RES_HOST_OS},LINUX)
		RES_HOST_PLATFORM:=$(shell test -f /etc/fedora-release && echo LINUX_FEDORA)
		ifeq (${RES_HOST_PLATFORM},)
			RES_HOST_PLATFORM:=$(shell test -f /etc/redhat-release && egrep -iq '^.*(enterprise|centos).*release .*$$' /etc/redhat-release && echo LINUX_EL)
		endif
		ifeq (${RES_HOST_PLATFORM},)
			RES_HOST_PLATFORM:=$(shell test -f /etc/redhat-release && echo LINUX_REDHAT)
		endif
		ifeq (${RES_HOST_PLATFORM},)
			RES_HOST_PLATFORM:=$(shell test -f /etc/issue && egrep -iq '^.*ELDK version 4.2 build 2008-04-01.*$$' /etc/issue && echo LINUX_EL)
		endif
		ifeq (${RES_HOST_PLATFORM},)
                        $(error Unknown host platform.)
		endif

		ifeq (${RES_HOST_PLATFORM},LINUX_FEDORA)
			RES_HOST_PLATFORM_VERSION=$(shell echo CORE`cat /etc/fedora-release | sed 's/^.* release \(.*\) (.*)$$/\1/'`)
		endif
		ifeq (${RES_HOST_PLATFORM},LINUX_EL)
			RES_HOST_PLATFORM_VERSION=$(shell echo EL`cat /etc/redhat-release | sed 's/^.* release \([^ .]*\).*$$/\1/'`)
		endif
		ifeq (${RES_HOST_PLATFORM},LINUX_REDHAT)
			RES_HOST_PLATFORM_VERSION=RH9
		endif
	endif

	RES_HOST_PLATFORM_FULL:=${RES_HOST_PLATFORM}_${RES_HOST_PLATFORM_VERSION}

	# 
	# Figure out about the hardware we are running on
	#
	# This will set:
	#      RES_HOST_ARCH
	#      RES_HOST_ARCH_FAMILY
	#      RES_HOST_CPU
	#      RES_HOST_CPU_SUPPORTED_WORDSIZES
	# 
	# RES_HOST_ARCH is the machine architecture, which for now can take on:
	#               i386, x86_64, sparc, sparc64, ppc, ppc64
	# RES_HOST_ARCH_FAMILY is the machine architecture family, for now:
	#               x86, sparc, ppc
	# RES_HOST_CPU is the machine CPU, which can take on:
	#               i386, i686, x86_64, sparc, sparc64, ppc, ppc64
	# RES_HOST_CPU_SUPPORTED_WORDSIZES is a list that can take on:
	#		"32", "64", "32 64", "64 32"

	RES_HOST_ARCH_RAW:=$(shell uname -p | tr '[a-z]' '[A-Z]')
	RES_HOST_ARCH:=${RES_HOST_ARCH_RAW}
	ifneq (${PROD_HOST_ARCH_OVERRIDE},)
		RES_HOST_ARCH:=${PROD_HOST_ARCH_OVERRIDE}
		RES_HOST_ARCH_RAW:=${RES_HOST_ARCH}
	endif

	# Map the arch to a canonical value
	ifeq (${RES_HOST_ARCH_RAW},I686)
		RES_HOST_ARCH:=I386
	endif
	ifeq (${RES_HOST_ARCH_RAW},I386)
		RES_HOST_ARCH:=I386
	endif
	ifeq (${RES_HOST_ARCH_RAW},I586)
		RES_HOST_ARCH:=I386
	endif
	ifeq (${RES_HOST_ARCH_RAW},I486)
		RES_HOST_ARCH:=I386
	endif
	ifeq (${RES_HOST_ARCH_RAW},ATHLON)
		RES_HOST_ARCH:=I386
	endif
	ifeq (${RES_HOST_ARCH_RAW},X86_64)
		RES_HOST_ARCH:=X86_64
	endif
	ifeq (${RES_HOST_ARCH_RAW},AMD64)
		RES_HOST_ARCH:=X86_64
	endif
	ifeq (${RES_HOST_ARCH_RAW},SPARC)
		ifeq ($(shell uname -m),sun4u)
			RES_HOST_ARCH:=SPARC64
		else
			RES_HOST_ARCH:=SPARC
		endif
	endif
	ifeq (${RES_HOST_ARCH_RAW},PPC)
		RES_HOST_ARCH:=PPC
	endif
	ifeq (${RES_HOST_ARCH_RAW},PPC64)
		RES_HOST_ARCH:=PPC64
	endif

	RES_HOST_ARCH_FAMILY:=${RES_HOST_ARCH}
	ifneq ($(strip $(filter I386 X86_64,${RES_HOST_ARCH})),)
		RES_HOST_ARCH_FAMILY:=X86
	endif
	ifneq ($(strip $(filter SPARC SPARC64,${RES_HOST_ARCH})),)
		RES_HOST_ARCH_FAMILY:=SPARC
	endif
	ifneq ($(strip $(filter PPC PPC64,${RES_HOST_ARCH})),)
		RES_HOST_ARCH_FAMILY:=PPC
	endif

	RES_HOST_CPU_RAW:=$(shell uname -p | tr '[a-z]' '[A-Z]')
        RES_HOST_CPU_SUPPORTED_WORDSIZES=32
	ifneq (${PROD_HOST_CPU_OVERRIDE},)
		RES_HOST_CPU:=${PROD_HOST_CPU_OVERRIDE}
		RES_HOST_CPU_RAW:=${RES_HOST_CPU}
	endif

	# Map the cpu to a canonical value
	ifeq (${RES_HOST_CPU_RAW},I686)
		RES_HOST_CPU:=I686
	endif
	ifeq (${RES_HOST_CPU_RAW},I386)
		RES_HOST_CPU:=I386
	endif
	ifeq (${RES_HOST_CPU_RAW},I586)
		RES_HOST_CPU:=I386
	endif
	ifeq (${RES_HOST_CPU_RAW},I486)
		RES_HOST_CPU:=I386
	endif
	ifeq (${RES_HOST_CPU_RAW},ATHLON)
		RES_HOST_CPU:=I386
	endif
	ifeq (${RES_HOST_CPU_RAW},X86_64)
		RES_HOST_CPU:=X86_64
		RES_HOST_CPU_SUPPORTED_WORDSIZES=64 32
	endif
	ifeq (${RES_HOST_CPU_RAW},AMD64)
		RES_HOST_CPU:=X86_64
		RES_HOST_CPU_SUPPORTED_WORDSIZES=64
	endif
	ifeq (${RES_HOST_CPU_RAW},SPARC)
		ifeq ($(shell uname -m),sun4u)
			RES_HOST_CPU:=SPARC64
			RES_HOST_CPU_SUPPORTED_WORDSIZES=32 64
		else
			RES_HOST_CPU:=SPARC
		endif
	endif
	ifeq (${RES_HOST_CPU_RAW},PPC)
		RES_HOST_CPU:=PPC
	endif
	ifeq (${RES_HOST_CPU_RAW},PPC64)
		RES_HOST_CPU:=PPC64
		# XXX if we ever supported building on PowerPC 64 bit,
		# this would be an issue
		RES_HOST_CPU_SUPPORTED_WORDSIZES=32
	endif

	#
	# We use the first supported word size for this CPU
	#
	RES_HOST_CPU_DEFAULT_WORDSIZE=$(word 1,${RES_HOST_CPU_SUPPORTED_WORDSIZES})

	# Export everything we computed here
	export PROD_HOST_MACHINE:=$(shell hostname)
	export PROD_HOST_MACHINE_SHORT:=$(shell echo ${PROD_HOST_MACHINE} | ${SED} 's/^\([^\.]*\).*$$/\1/')
	export PROD_HOST_USER:=${RES_WHOAMI}
	export PROD_HOST_REAL_USER:=${RES_REAL_USER}

	export PROD_HOST_OS:=${RES_HOST_OS}
	export PROD_HOST_OS_LC:=$(shell echo ${PROD_HOST_OS} | tr '[A-Z]' '[a-z]')
	export PROD_HOST_PLATFORM:=${RES_HOST_PLATFORM}
	export PROD_HOST_PLATFORM_LC:=$(shell echo ${PROD_HOST_PLATFORM} | tr '[A-Z]' '[a-z]')
	export PROD_HOST_PLATFORM_VERSION:=${RES_HOST_PLATFORM_VERSION}
	export PROD_HOST_PLATFORM_VERSION_LC:=$(shell echo ${PROD_HOST_PLATFORM_VERSION} | tr '[A-Z]' '[a-z]')
	export PROD_HOST_PLATFORM_FULL:=${RES_HOST_PLATFORM_FULL}
	export PROD_HOST_PLATFORM_FULL_LC:=$(shell echo ${PROD_HOST_PLATFORM_FULL} | tr '[A-Z]' '[a-z]')

	export PROD_HOST_ARCH:=${RES_HOST_ARCH}
	export PROD_HOST_ARCH_LC:=$(shell echo ${PROD_HOST_ARCH} | tr '[A-Z]' '[a-z]')
	export PROD_HOST_ARCH_FAMILY:=${RES_HOST_ARCH_FAMILY}
	export PROD_HOST_ARCH_FAMILY_LC:=$(shell echo ${PROD_HOST_ARCH_FAMILY} | tr '[A-Z]' '[a-z]')
	export PROD_HOST_CPU:=${RES_HOST_CPU}
	export PROD_HOST_CPU_LC:=$(shell echo ${PROD_HOST_CPU} | tr '[A-Z]' '[a-z]')
	export PROD_HOST_CPU_SUPPORTED_WORDSIZES:=${RES_HOST_CPU_SUPPORTED_WORDSIZES}
	export PROD_HOST_CPU_DEFAULT_WORDSIZE:=${RES_HOST_CPU_DEFAULT_WORDSIZE}

	# Say we've done it
	export RES_HOST_DONE=1
endif

# If set, the target file (under release) specifies everything we need to
# know about the target. If not set, ('auto'), we'll automatically choose
# the target platform to be the host's platform.

# If we should try to use the toolchain from the host system
PROD_TARGET_TOOLCHAIN_DIR?=
PROD_TARGET_TOOLCHAIN_PREFIX?=
PROD_TARGET_TOOLCHAIN_EXTRA_DIR?=
PROD_TARGET_TOOLCHAIN_TARGET_ROOT?=

# If the output directory includes the platform name
PROD_TARGET_PLATFORM_USE_PREFIX?=0

PROD_TARGET_HWNAME_REQUIRED?=0
PROD_TARGET_HWNAME?=
PROD_TARGET_HWNAMES_COMPAT?=${PROD_TARGET_HWNAME}

# Read in target architecture specific os, platform, arch, cpu and tool
# chain info

ifneq (${PROD_TARGET_ID},auto)
	ifndef PROD_TARGET_HOST
		include ${PROD_SRC_ROOT}/release/target/Makefile.${PROD_TARGET_ID}.inc
	else
		include ${PROD_SRC_ROOT}/release/target/Makefile.host.inc
	endif
else
	include ${PROD_SRC_ROOT}/release/target/Makefile.auto.inc
endif

ifndef PROD_TARGET_OS
        $(error Must set PROD_TARGET_OS)
endif
ifndef PROD_TARGET_PLATFORM
        $(error Must set PROD_TARGET_PLATFORM)
endif
ifndef PROD_TARGET_PLATFORM_VERSION
        $(error Must set PROD_TARGET_PLATFORM_VERSION)
endif
ifndef PROD_TARGET_PLATFORM_FULL
        $(error Must set PROD_TARGET_PLATFORM_FULL)
endif

ifndef PROD_TARGET_ARCH
        $(error Must set PROD_TARGET_ARCH)
endif
ifndef PROD_TARGET_CPU
        $(error Must set PROD_TARGET_CPU)
endif
ifndef PROD_TARGET_CPU_SUPPORTED_WORDSIZES
        $(error Must set PROD_TARGET_CPU_SUPPORTED_WORDSIZES)
endif
ifndef PROD_TARGET_CPU_DEFAULT_WORDSIZE
        $(error Must set PROD_TARGET_CPU_DEFAULT_WORDSIZE)
endif
ifndef PROD_TARGET_CPU_ENDIAN
        $(error Must set PROD_TARGET_CPU_ENDIAN)
endif
ifndef PROD_TARGET_CPU_ALIGNMENT
        $(error Must set PROD_TARGET_CPU_ALIGNMENT)
endif

ifndef PROD_TARGET_CC_VERSION
        $(error Must set PROD_TARGET_CC_VERSION)
endif

ifndef PROD_TARGET_ARCH_FAMILY
	PROD_TARGET_ARCH_FAMILY:=${PROD_TARGET_ARCH}
	ifneq ($(strip $(filter I386 X86_64,${PROD_TARGET_ARCH})),)
		PROD_TARGET_ARCH_FAMILY:=X86
	endif
	ifneq ($(strip $(filter SPARC SPARC64,${PROD_TARGET_ARCH})),)
		PROD_TARGET_ARCH_FAMILY:=SPARC
	endif
	ifneq ($(strip $(filter PPC PPC64,${PROD_TARGET_ARCH})),)
		PROD_TARGET_ARCH_FAMILY:=PPC
	endif
endif
PROD_TARGET_ARCH_FAMILY_LC:=$(shell echo ${PROD_TARGET_ARCH_FAMILY} | tr '[A-Z]' '[a-z]')

ifeq (${PROD_TARGET_HWNAME_REQUIRED},1)
	ifeq ($(strip ${PROD_TARGET_HWNAME}),)
                $(error Must set PROD_TARGET_HWNAME)
	endif
	ifeq ($(strip $(filter ${PROD_TARGET_HWNAME},${PROD_TARGET_HWNAMES_COMPAT})),)
                $(error PROD_TARGET_HWNAMES_COMPAT must contain PROD_TARGET_HWNAME)
	endif

endif

ifneq ($(strip ${PROD_TARGET_HWNAME}),)
	PROD_TARGET_HWNAME:=$(shell echo ${PROD_TARGET_HWNAME} | sed 's/[^a-zA-Z0-9_]/_/g' | tr '[a-z]' '[A-Z]')
	PROD_TARGET_HWNAME_LC:=$(shell echo ${PROD_TARGET_HWNAME} | tr '[A-Z]' '[a-z]')
	PROD_TARGET_HWNAME_FILE:=${PROD_TARGET_HWNAME_LC}
	PROD_TARGET_HWNAME_FILE_DASH:=-${PROD_TARGET_HWNAME_FILE}
	PROD_TARGET_HWNAME_FILE_UNDER:=_${PROD_TARGET_HWNAME_FILE}
else
	PROD_TARGET_HWNAME:=
	PROD_TARGET_HWNAME_LC:=
	PROD_TARGET_HWNAME_FILE:=
	PROD_TARGET_HWNAME_FILE_DASH:=
	PROD_TARGET_HWNAME_FILE_UNDER:=
endif

ifneq ($(strip ${PROD_TARGET_HWNAMES_COMPAT}),)
	PROD_TARGET_HWNAMES_COMPAT:=$(shell echo ${PROD_TARGET_HWNAMES_COMPAT} | sed 's/[^a-zA-Z0-9_ ]/_/g' | tr '[a-z]' '[A-Z]')
	PROD_TARGET_HWNAMES_COMPAT_LC:=$(shell echo ${PROD_TARGET_HWNAMES_COMPAT} | tr '[A-Z]' '[a-z]')

else
	PROD_TARGET_HWNAMES_COMPAT:=
	PROD_TARGET_HWNAMES_COMPAT_LC:=
endif


# 4.1.2 -> 040102, so it can be numerically compared
export PROD_TARGET_CC_VERSION_NUMBER:=$(shell echo "${PROD_TARGET_CC_VERSION}" | awk -F'.' '{printf "%02d%02d%02d", $$1, $$2, $$3}')
CC_VERSION_ATLEAST=$(shell if [ ${PROD_TARGET_CC_VERSION_NUMBER} -ge $(1) ]; then echo "$(2)"; else echo "$(3)"; fi;)
CC_VERSION_EQUALS=$(shell if [ ${PROD_TARGET_CC_VERSION_NUMBER} -eq $(1) ]; then echo "$(2)"; else echo "$(3)"; fi;)


# Set the wordsize we will ask the compiler to use (32 or 64)
ifeq (${PROD_TARGET_CPU_WORDSIZE},)
	PROD_TARGET_CPU_WORDSIZE=${PROD_TARGET_CPU_DEFAULT_WORDSIZE}
endif

export PROD_TARGET_ID
export PROD_TARGET_ID_NAME?=${PROD_TARGET_ID}

PROD_TARGET_OS_LC:=$(shell echo ${PROD_TARGET_OS} | tr '[A-Z]' '[a-z]')
PROD_TARGET_PLATFORM_LC:=$(shell echo ${PROD_TARGET_PLATFORM} | tr '[A-Z]' '[a-z]')
PROD_TARGET_PLATFORM_VERSION_LC:=$(shell echo ${PROD_TARGET_PLATFORM_VERSION} | tr '[A-Z]' '[a-z]')
PROD_TARGET_PLATFORM_FULL_LC:=$(shell echo ${PROD_TARGET_PLATFORM_FULL} | tr '[A-Z]' '[a-z]')

PROD_TARGET_ARCH_LC:=$(shell echo ${PROD_TARGET_ARCH} | tr '[A-Z]' '[a-z]')
PROD_TARGET_CPU_LC=$(shell echo ${PROD_TARGET_CPU} | tr '[A-Z]' '[a-z]')





# The build tools: binutils and gcc.
ifneq (${BUILDTOOLS_OVERRIDE},1)

AR=${PROD_TARGET_TOOLCHAIN_DIR}${PROD_TARGET_TOOLCHAIN_PREFIX}ar
AS=${PROD_TARGET_TOOLCHAIN_DIR}${PROD_TARGET_TOOLCHAIN_PREFIX}as
# Juniper added $(CUSTOMER_CXX_WRAPPER)
CXX=$(CUSTOMER_CXX_WRAPPER) ${PROD_TARGET_TOOLCHAIN_DIR}${PROD_TARGET_TOOLCHAIN_PREFIX}g++ 
# Juniper added $(CUSTOMER_CC_WRAPPER)
CC=$(CUSTOMER_CC_WRAPPER) ${PROD_TARGET_TOOLCHAIN_DIR}${PROD_TARGET_TOOLCHAIN_PREFIX}gcc 
CPP=${PROD_TARGET_TOOLCHAIN_DIR}${CC} -E
# Juniper added $(CUSTOMER_LD_WRAPPER)
LD=$(CUSTOMER_LD_WRAPPER) ${PROD_TARGET_TOOLCHAIN_DIR}${PROD_TARGET_TOOLCHAIN_PREFIX}ld 
NM=${PROD_TARGET_TOOLCHAIN_DIR}${PROD_TARGET_TOOLCHAIN_PREFIX}nm
OBJCOPY=${PROD_TARGET_TOOLCHAIN_DIR}${PROD_TARGET_TOOLCHAIN_PREFIX}objcopy
OBJDUMP=${PROD_TARGET_TOOLCHAIN_DIR}${PROD_TARGET_TOOLCHAIN_PREFIX}objdump
RANLIB=${PROD_TARGET_TOOLCHAIN_DIR}${PROD_TARGET_TOOLCHAIN_PREFIX}ranlib
SIZE=${PROD_TARGET_TOOLCHAIN_DIR}${PROD_TARGET_TOOLCHAIN_PREFIX}size
STRIP=${PROD_TARGET_TOOLCHAIN_DIR}${PROD_TARGET_TOOLCHAIN_PREFIX}strip

##ADDR2LINE?=${PROD_TARGET_TOOLCHAIN_DIR}${PROD_TARGET_TOOLCHAIN_PREFIX}addr2line
##GCOV?=${PROD_TARGET_TOOLCHAIN_DIR}${PROD_TARGET_TOOLCHAIN_PREFIX}gcov
##GDB?=${PROD_TARGET_TOOLCHAIN_DIR}${PROD_TARGET_TOOLCHAIN_PREFIX}gdb
##GPROF?=${PROD_TARGET_TOOLCHAIN_DIR}${PROD_TARGET_TOOLCHAIN_PREFIX}gprof
##READELF?=${PROD_TARGET_TOOLCHAIN_DIR}${PROD_TARGET_TOOLCHAIN_PREFIX}readelf
##STRINGS?=${PROD_TARGET_TOOLCHAIN_DIR}${PROD_TARGET_TOOLCHAIN_PREFIX}strings

RPM=${PROD_TARGET_TOOLCHAIN_EXTRA_DIR}${PROD_TARGET_TOOLCHAIN_PREFIX}rpm

endif # BUILDTOOLS_OVERRIDE

# Other support tools used during a build
BISON?=bison
BZIP?=bzip2
FLEX?=flex
GZIP?=gzip
MD5SUM=md5sum
PATCH?=patch
TAR?=tar
UNZIP?=unzip
ZIP?=zip

# I18N related tools
MSGEN?=msgen
MSGFMT?=msgfmt
MSGINIT?=msginit
MSGMERGE?=msgmerge
XGETTEXT?=xgettext

PURIFY?=
PURIFY_LINK?=purify

# Make sure we keep using Gnu make
export MAKE

# Java settings
ifeq (${PROD_HOST_PLATFORM_FULL},LINUX_EL_EL5)
	ifeq (${PROD_HOST_ARCH},X86_64)
		JAVA_JDK_ROOT?=/usr/lib/jvm/java-1.6.0-openjdk-1.6.0.0.x86_64
	else
		JAVA_JDK_ROOT?=/usr/lib/jvm/java-1.6.0-openjdk-1.6.0.0
	endif
else ifeq (${PROD_HOST_PLATFORM_FULL},LINUX_EL_EL6)
	ifeq (${PROD_HOST_ARCH},X86_64)
		JAVA_JDK_ROOT?=/usr/lib/jvm/java-1.6.0-openjdk-1.6.0.0.x86_64
	else
		JAVA_JDK_ROOT?=/usr/lib/jvm/java-1.6.0-openjdk-1.6.0.0
	endif
else
	JAVA_JDK_ROOT?=/usr/lib/jvm/java-openjdk
endif
JAVA_LIB_SUFFIX=-pic

# XXX? what about PROD_TARGET_TOOLCHAIN_DIR and _PREFIX ?
JAVAC=${JAVA_JDK_ROOT}/bin/javac
JAVAH=${JAVA_JDK_ROOT}/bin/javah

# Set the lib output directory, which is based on the wordsize

ifneq ($(words ${PROD_TARGET_CPU_SUPPORTED_WORDSIZES}),1)
    ifeq (${PROD_TARGET_CPU_WORDSIZE},${PROD_TARGET_CPU_DEFAULT_WORDSIZE})
	DIR_WORDSIZE_SUFFIX_${PROD_TARGET_CPU_WORDSIZE}=
    else
	DIR_WORDSIZE_SUFFIX_${PROD_TARGET_CPU_WORDSIZE}=${PROD_TARGET_CPU_WORDSIZE}
    endif
else
	DIR_WORDSIZE_SUFFIX_${PROD_TARGET_CPU_WORDSIZE}=
endif

BIN_DIR_BASE=bin${DIR_WORDSIZE_SUFFIX_${PROD_TARGET_CPU_WORDSIZE}}
BIN_DIR=${BIN_DIR_BASE}${OBJ_TYPE_SUFFIX}
LIB_DIR_BASE=lib${DIR_WORDSIZE_SUFFIX_${PROD_TARGET_CPU_WORDSIZE}}
LIB_DIR=${LIB_DIR_BASE}${OBJ_TYPE_SUFFIX}
ILIB_DIR_BASE=ilib${DIR_WORDSIZE_SUFFIX_${PROD_TARGET_CPU_WORDSIZE}}
ILIB_DIR=${ILIB_DIR_BASE}${OBJ_TYPE_SUFFIX}
CUST_LIB_DIR_BASE=custlib${DIR_WORDSIZE_SUFFIX_${PROD_TARGET_CPU_WORDSIZE}}
CUST_LIB_DIR=${CUST_LIB_DIR_BASE}${OBJ_TYPE_SUFFIX}
CUST_ILIB_DIR_BASE=custilib${DIR_WORDSIZE_SUFFIX_${PROD_TARGET_CPU_WORDSIZE}}
CUST_ILIB_DIR=${CUST_ILIB_DIR_BASE}${OBJ_TYPE_SUFFIX}
THIRDPARTY_LIB_DIR_BASE=thirdparty${DIR_WORDSIZE_SUFFIX_${PROD_TARGET_CPU_WORDSIZE}}
THIRDPARTY_LIB_DIR=${THIRDPARTY_LIB_DIR_BASE}${OBJ_TYPE_SUFFIX}



#  So we don't actually try to set the owners and perms right now
ifneq (${PROD_HOST_USER},root)
	INSTALL_FAKE=1
endif


# The user can set CFLAGS and LDFLAGS, but we may add in some of our own

# Different OSs have a different supported maximum XOPEN source version
ifeq (${PROD_TARGET_OS},LINUX)
	XOS_VER=600
endif


# ==================================================
# PRE_ variable settings
# ==================================================

ifneq (${WARNINGS_OVERRIDE},1)
    PRE_C_WARNINGS= \
	-Wall \
	-Wextra \
	-Wshadow \
	-Wpointer-arith \
	-Wredundant-decls \
	-Wundef \
	-Winit-self \
	-Wmissing-declarations \
	-Wbad-function-cast \
	-Wstrict-prototypes \
	-Wmissing-prototypes \
	-Wnested-externs \
	-Wwrite-strings \
	\
	-Wno-unused \

ifneq (${WARNINGS_EXTRA_OVERRIDE},1)
    PRE_C_WARNINGS+= \
	-Wformat-security \
	-Wmissing-format-attribute \
	-Winline \

### Future candidates for above (not -O2), one-off testing
##	-O2 \
##	-Wdeclaration-after-statement \
##	-Wmissing-include-dirs \

### Not yet clean for these
##	-Wswitch-enum \
##	-Wswitch-default \

### Future gcc version
##	-Wlogical-op \

## ifeq (${PROD_TARGET_PLATFORM_FULL}-${PROD_TARGET_ARCH_FAMILY},LINUX_EL_EL6-X86)
##    PRE_C_WARNINGS+= \
## 	-Wframe-larger-than=25000 \
##
## endif

endif

#
# Be CAREFUL using this option!  It may help you find unused variables,
# but before you check in any changes found with this, try building at 
# least with SAMARA_DEMO_MINIMAL and SAMARA_DEMO_ALL.  Some variables
# are declared unconditionally, but only used conditionally based on
# PROD_FEATUREs.  So this could trigger false warnings if you build with
# a PROD_ID that does not define the PROD_FEATURE that makes use of the 
# variable, and fixing the warning could cause you to break the build 
# under a different PROD_ID!
#
# Technically even building with these two PROD_IDs is not sufficient,
# since variables can be used under other conditions besides PROD_FEATUREs.
# It's safest to search for the variable you're about to delete, to make
# sure it is really not used under any circumstances.
#
# ifneq (${WARNINGS_EXTRA_OVERRIDE},1)
#     PRE_C_WARNINGS+=-Wunused-variable
# endif

##      Warnings we wish we could turn on, but can't in practice:
##
##	-Wcast-qual \
##	-Wcast-align \
##	-O2 -Wstrict-aliasing=2 \
##
##	-Wno-unused-parameter \   # Note, after removing "-Wno-unused"
##	-Wno-unused-label \
##
##	-Wswitch-default \
##	-Wswitch-enum \
##	-Wconversion \
##	-Wno-sign-conversion \
##	-Wunreachable-code \
##	-Woverlength-strings \
##	-Wlogical-op \
##	-Wc++-compat \

##      Other compiler flags for us to consider, not necessarily warning flags:
##
##	-Wp,-D_FORTIFY_SOURCE=2 \
##	-fstack-protector \
##	--param=ssp-buffer-size=4 \
##	-fpie \
##	-fexceptions \
##	-fasynchronous-unwind-tables \


    PRE_CXX_WARNINGS= \
	-Wall \
	-Wextra \
	-Wshadow \
	-Wpointer-arith \
	-Wredundant-decls \
	-Wundef \
	-Winit-self \
	\
	-Wno-unused \

ifneq (${WARNINGS_EXTRA_OVERRIDE},1)
    PRE_CXX_WARNINGS+= \
	-Wformat-security \
	-Wmissing-format-attribute \
	-Winline \

endif

else
    PRE_C_WARNINGS=
    PRE_CXX_WARNINGS=
endif

# Note, the linker part of this is below
ifneq (${WARNINGS_ARE_ERRORS},0)
	PRE_C_WARNINGS+=-Werror
	PRE_CXX_WARNINGS+=-Werror
endif

ifneq (${DEFINES_OVERRIDE},1) 
	PRE_DEFINES=
	ifneq ("$(strip ${XOS_VER})","")
		PRE_DEFINES+=-D_XOPEN_SOURCE=${XOS_VER}
	endif
	PRE_DEFINES+= -D_REENTRANT 
else
	PRE_DEFINES=
endif

PRE_DEFINES+=-DPROD_PRODUCT=${PROD_PRODUCT} \
             -DPROD_PRODUCT_${PROD_PRODUCT}=1 \
	     -DPROD_CUSTOMER=${PROD_CUSTOMER} \
             -DPROD_CUSTOMER_${PROD_CUSTOMER}=1 \
             -DPROD_ID=${PROD_ID} \
             -DPROD_ID_${PROD_ID}=1 \
             -DPROD_TARGET_OS=${PROD_TARGET_OS} \
	     -DPROD_TARGET_OS_${PROD_TARGET_OS}=1 \
             -DPROD_TARGET_PLATFORM=${PROD_TARGET_PLATFORM} \
	     -DPROD_TARGET_PLATFORM_${PROD_TARGET_PLATFORM}=1 \
             -DPROD_TARGET_PLATFORM_VERSION=${PROD_TARGET_PLATFORM_VERSION} \
	     -DPROD_TARGET_PLATFORM_VERSION_${PROD_TARGET_PLATFORM_VERSION}=1 \
             -DPROD_TARGET_PLATFORM_FULL=${PROD_TARGET_PLATFORM_FULL} \
	     -DPROD_TARGET_PLATFORM_FULL_${PROD_TARGET_PLATFORM_FULL}=1 \
             -DPROD_TARGET_ARCH=${PROD_TARGET_ARCH} \
	     -DPROD_TARGET_ARCH_${PROD_TARGET_ARCH}=1 \
             -DPROD_TARGET_ARCH_FAMILY=${PROD_TARGET_ARCH_FAMILY} \
	     -DPROD_TARGET_ARCH_FAMILY_${PROD_TARGET_ARCH_FAMILY}=1 \
             -DPROD_TARGET_CPU=${PROD_TARGET_CPU} \
	     -DPROD_TARGET_CPU_${PROD_TARGET_CPU}=1 \
	     -DPROD_TARGET_CPU_WORDSIZE=${PROD_TARGET_CPU_WORDSIZE} \
	     -DPROD_TARGET_CPU_WORDSIZE_${PROD_TARGET_CPU_WORDSIZE}=1 \
	     -DPROD_TARGET_CPU_ENDIAN=${PROD_TARGET_CPU_ENDIAN} \
	     -DPROD_TARGET_CPU_ENDIAN_${PROD_TARGET_CPU_ENDIAN}=1 \
	     -DPROD_TARGET_CPU_ALIGNMENT=${PROD_TARGET_CPU_ALIGNMENT} \
	     -DPROD_TARGET_CPU_ALIGNMENT_${PROD_TARGET_CPU_ALIGNMENT}=1 \

ifeq (${LICENSE_REQUIRES_REDIST},1)
    PRE_DEFINES+=\
	-DNON_REDIST_HEADERS_PROHIBITED=1 \

endif

# Determine which features are enabled for the build (or not).
#
# Individual features can be enabled by doing PROD_FEATURE_featurename=1
# in the customer.mk .  The can be disabled by doing
# PROD_FEATURE_featurename=0 .
#
# PROD_FEATURES_DEFAULT_ENABLED and PROD_FEATURES_DEFAULT_DISABLED
# together must list all possible features of the product.  Then, the
# build system, based on these and the individual
# PROD_FEATURE_featurename settings, will set PROD_FEATURES_ENABLED ,
# PROD_FEATURES_DISABLED , and build the PRE_DEFINES list to have a
# #define for each enabled feature.  It will also set / normalize the
# PROD_FEATURE_featurename settings so that they are always 0 or 1.
#
# Customers, in their customer.mk file, can add new, customer-specific 
# features by doing PROD_FEATURES_DEFAULT_ENABLED+=MYFEATURE or 
# PROD_FEATURES_DEFAULT_DISABLED+=MYFEATURE .  Customers should not
# modify the lists in this file.
#

PROD_FEATURES_DEFAULT_ENABLED+= \
	ACCOUNTING \
	SCHED \
	WIZARD \
	NTP_CLIENT \
	DHCP_CLIENT \
	ZEROCONF \
	AAA \
	RADIUS \
	TACACS \
	LDAP \
	XINETD \
	FTPD \
	TELNETD \
	GRAPHING \
	SMARTD \
	TMS_MIB \
	WEB_REF_UI \
	MULTI_USER \
	RENAME_IFS \
	MULTITHREADING \
	CRYPTO \
	IMAGE_SECURITY \
	VLAN \
	CHANGE_AUDITING \
	ACTION_AUDITING \
	XML_GW \
	OBFUSCATE_NONADMIN \
	CAPABS \

ifeq (${PROD_TARGET_ARCH_FAMILY},PPC)
	PROD_FEATURES_DEFAULT_ENABLED+= \
	    KERNEL_IMAGE_UNI \
	    KERNEL_IMAGE_SMP \
	    KERNEL_BOOTFLOPPY_UNI \

else
	# x86 defaults to a single SMP kernel in the image, as there's
	# no difference between UNI and SMP kernels for it.

	PROD_FEATURES_DEFAULT_ENABLED+= \
	    KERNEL_IMAGE_SMP \
	    KERNEL_BOOTFLOPPY_UNI \

	PROD_FEATURES_DEFAULT_DISABLED+= \
	    KERNEL_IMAGE_UNI \

endif # !PPC


# Juniper added:
# PROD_FEATURE_JUNIPER_MEDIAFLOW for extra features needed for MFC/JCE support:
PROD_FEATURES_DEFAULT_DISABLED+= \
	JUNIPER_MEDIAFLOW \
	FRONT_PANEL \
	FP_CONFIG_LCD \
	HW_RNG \
	LOCKDOWN \
	CMC_CLIENT \
	CMC_CLIENT_LICENSED \
	CMC_SERVER \
	CMC_SERVER_LICENSED \
	I18N_SUPPORT \
	I18N_EXTRAS \
	CLUSTER \
	EXTERNAL_DOC \
	TEST \
	KERNELS_EXTERNAL \
	BRIDGING \
	BONDING \
	WEB_COMBINED_LOGIN \
	GPT_PARTED \
	UBOOT \
	EETOOL \
	NVS \
	RESTRICT_CMDS \
	SHELL_ACCESS \
	DEV_ENV \
	SECURE_NODES \
	SNMP_SETS \
	JAVA \
	OLD_GRAPHING \
	AUTH_SMTP \
	UCD_SNMP_MIB \
	SNMP_AGENTX \
	IPTABLES \
	REMOTE_GCL \
	IPV6 \
	VIRT \
	VIRT_KVM \
	VIRT_XEN \
	VIRT_LICENSED \
	ACLS \
	ACLS_DYNAMIC \
	ACLS_QUERY_ENFORCE \
	ADMIN_NOT_SUPERUSER \
	WGET \
	OLD_GDB \
	OLD_RSYNC \

# XXX/EMT: the ADMIN_NOT_SUPERUSER feature is not yet complete, so it is
# not permitted to be enabled.

# XXX: the VIRT_XEN feature is not yet complete and should not be
# enabled by a customer.

# Args: 1: feature name (all caps)
define process_feature_default_enabled
ifneq (${PROD_FEATURE_$(1)},0)
	PROD_FEATURE_$(1)=1
	PROD_FEATURES_ENABLED+=$(1)
	PRE_DEFINES+=-DPROD_FEATURE_$(1)=1
else
	PROD_FEATURES_DISABLED+=$(1)
endif
endef

# Args: 1: feature name (all caps)
define process_feature_default_disabled
ifneq ($${PROD_FEATURE_$(1)},1)
	PROD_FEATURE_$(1)=0
	PROD_FEATURES_DISABLED+=$(1)
else
	PROD_FEATURES_ENABLED+=$(1)
	PRE_DEFINES+=-DPROD_FEATURE_$(1)=1
endif
endef

# Args: 1: feature name (all caps)
define process_feature_check_exists
ifneq ($$(strip $$(filter $(1),$${PROD_FEATURES_DEFAULT_DISABLED} $${PROD_FEATURES_DEFAULT_ENABLED})),$$(strip $(1)))
                $$(error Unknown PROD_FEATURE: $(1))
endif
endef

# Now go through all the features, and set our make variables accordingly

PROD_FEATURES_ENABLED=
PROD_FEATURES_DISABLED=

$(foreach f,${PROD_FEATURES_DEFAULT_ENABLED},$(eval $(call process_feature_default_enabled,${f})))
$(foreach f,${PROD_FEATURES_DEFAULT_DISABLED},$(eval $(call process_feature_default_disabled,${f})))
$(foreach f,$(subst PROD_FEATURE_,,$(filter PROD_FEATURE_%,${.VARIABLES})),$(eval $(call process_feature_check_exists,${f})))

# The "default defaults" for crypto are currently to include both
# subsystems.
ifeq ($(strip ${CRYPTO_INCLUDE_RACOON}),)
	CRYPTO_INCLUDE_RACOON=1
endif
ifeq ($(strip ${CRYPTO_INCLUDE_OPENSWAN}),)
	CRYPTO_INCLUDE_OPENSWAN=1
endif

ifeq (${PROD_FEATURE_CRYPTO},1)
	ifneq (${CRYPTO_INCLUDE_RACOON},0)
		PRE_DEFINES+=-DCRYPTO_INCLUDE_RACOON=1
	endif
	ifneq (${CRYPTO_INCLUDE_OPENSWAN},0)
		PRE_DEFINES+=-DCRYPTO_INCLUDE_OPENSWAN=1
	endif
endif

# ==================================================
# Check features, bundles and any related tool versions
# ==================================================

ifeq (${PROD_FEATURE_RESTRICT_CMDS}${PROD_FEATURE_IMAGE_SECURITY},10)
        $(error Cannot enable PROD_FEATURE_RESTRICT_CMDS unless you have also enabled PROD_FEATURE_IMAGE_SECURITY)
endif

ifeq (${PROD_FEATURE_SHELL_ACCESS}${PROD_FEATURE_RESTRICT_CMDS},10)
        $(error Cannot enable PROD_FEATURE_SHELL_ACCESS unless you have also enabled PROD_FEATURE_RESTRICT_CMDS)
endif

ifeq (${PROD_FEATURE_JAVA},1)
	ifeq ("$(strip ${PROD_TARGET_JAVAC_VERSION})","")
                $(warning Java not supported, but PROD_FEATURE_JAVA=1)
	endif
endif

# XXX/EMT: bug 14150
ifeq (${PROD_FEATURE_SNMP_SETS}${PROD_FEATURE_ACLS},11)
        $(error Cannot enable both PROD_FEATURE_SNMP_SETS and PROD_FEATURE_ACLS)
endif

ifeq (${PROD_FEATURE_ACLS}${PROD_FEATURE_SECURE_NODES}${PROD_FEATURE_OBFUSCATE_NONADMIN},100)
        $(error If PROD_FEATURE_ACLS is enabled, then either PROD_FEATURE_SECURE_NODES or PROD_FEATURE_OBFUSCATE_NONADMIN must be enabled also)
endif

ifeq (${PROD_FEATURE_VIRT_XEN},1)
        $(warning Xen is not supported, but PROD_FEATURE_VIRT_XEN=1)
endif

ifeq (${PROD_FEATURE_CAPABS}${PROD_FEATURE_ACLS},00)
        $(error Must enable either PROD_FEATURE_CAPABS or PROD_FEATURE_ACLS)
endif

ifeq (${PROD_FEATURE_CAPABS}${PROD_FEATURE_ACLS},11)
        $(error Cannot enable both PROD_FEATURE_CAPABS and PROD_FEATURE_ACLS)
endif

ifeq (${PROD_FEATURE_ADMIN_NOT_SUPERUSER}${PROD_FEATURE_ACLS},10)
        $(error Cannot enable PROD_FEATURE_ADMIN_NOT_SUPERUSER unless you have also enabled PROD_FEATURE_ACLS)
endif

ifeq (${PROD_FEATURE_ACLS}${PROD_FEATURE_ACLS_DYNAMIC},01)
        $(error Cannot enable PROD_FEATURE_ACLS_DYNAMIC unless you have also enabled PROD_FEATURE_ACLS)
endif

ifeq (${PROD_FEATURE_ACLS}${PROD_FEATURE_ACLS_QUERY_ENFORCE},01)
        $(error Cannot enable PROD_FEATURE_ACLS_QUERY_ENFORCE unless you have also enabled PROD_FEATURE_ACLS)
endif

ifeq (${PROD_FEATURE_ADMIN_NOT_SUPERUSER},1)
        $(error PROD_FEATURE_ADMIN_NOT_SUPERUSER may not be enabled -- this feature is not yet complete)
endif

# If we're just testing the RPM versions, ignore this test
ifneq (${ENSURE_TOOL_VERSION_SINGLE},1)
	ifeq (${PROD_FEATURE_VIRT_KVM},1)
		ifneq (${PROD_TARGET_ARCH},X86_64)
                        $(error KVM only supported on X86_64, but PROD_FEATURE_VIRT_KVM=1)
		endif
	endif
endif

ifeq (${PROD_TARGET_ARCH_FAMILY},PPC)
	ifeq (${PROD_FEATURE_GPT_PARTED},1)
                $(warning Parted not supported on PowerPC, but PROD_FEATURE_GPT_PARTED=1)
	endif
	ifeq (${PROD_FEATURE_I18N_SUPPORT},1)
                $(warning I18n not supported on PowerPC, but PROD_FEATURE_I18N_SUPPORT=1)
	endif
	ifeq (${PROD_FEATURE_I18N_EXTRAS},1)
                $(warning I18n not supported on PowerPC, but PROD_FEATURE_I18N_EXTRAS=1)
	endif
	ifeq (${PROD_FEATURE_OLD_GRAPHING},1)
                $(warning Old graphing not supported on PowerPC, but PROD_FEATURE_OLD_GRAPHING=1)
	endif
	ifeq (${PROD_FEATURE_JAVA},1)
                $(warning Java not supported on PowerPC, but PROD_FEATURE_JAVA=1)
	endif
	ifneq (${PROD_FEATURE_UBOOT},1)
                $(error U-boot is required on PowerPC, but PROD_FEATURE_UBOOT=0)
	endif
	ifeq (${PROD_FEATURE_NVS}${PROD_FEATURE_EETOOL},00)
                $(error Must have at least one of PROD_FEATURE_EETOOL and PROD_FEATURE_NVS on PowerPC)
	endif
	ifeq (${PROD_HOST_PLATFORM_FULL},LINUX_EL_EL6)
                $(error PowerPC build not supported on EL6 (only EL5), but PROD_TARGET_ARCH_FAMILY=PPC)
	endif
endif

ifeq (${PROD_FEATURE_CLUSTER},1)
	ifneq (${PROD_TMS_SRCS_BUNDLE_CLUSTER},1)
                $(error Cluster bundle sources not present, but PROD_FEATURE_CLUSTER=1)
	endif
endif

ifeq (${PROD_FEATURE_CMC_CLIENT},1)
	ifneq (${PROD_TMS_SRCS_BUNDLE_CMC},1)
                $(error CMC bundle sources not present, but PROD_FEATURE_CMC_CLIENT=1)
	endif
endif
ifeq (${PROD_FEATURE_CMC_SERVER},1)
	ifneq (${PROD_TMS_SRCS_BUNDLE_CMC},1)
                $(error CMC bundle sources not present, but PROD_FEATURE_CMC_SERVER=1)
	endif
endif

ifeq (${PROD_FEATURE_VIRT},0)
	ifneq (${PROD_FEATURE_VIRT_XEN}${PROD_FEATURE_VIRT_KVM},00)
                $(warning KVM or Xen enabled, but PROD_FEATURE_VIRT=0)
	endif
endif

ifeq (${PROD_FEATURE_BRIDGING},0)
	ifneq (${PROD_FEATURE_VIRT},0)
                $(error Virtualization enabled, but PROD_FEATURE_BRIDGING=0)
	endif
endif

ifeq (${PROD_TARGET_PLATFORM_FULL}-${PROD_FEATURE_OLD_GDB},LINUX_EL_EL6-1)
        $(error PROD_FEATURE_OLD_GDB not supported under EL6)
endif

ifeq (${PROD_TARGET_PLATFORM_FULL}-${PROD_FEATURE_OLD_RSYNC},LINUX_EL_EL6-1)
        $(error PROD_FEATURE_OLD_RSYNC not supported under EL6)
endif

ifeq (${PROD_TARGET_PLATFORM_FULL}-${PROD_FEATURE_OLD_GRAPHING},LINUX_EL_EL6-1)
        $(error PROD_FEATURE_OLD_GRAPHING not supported under EL6)
endif

ifeq (${PROD_FEATURE_CRYPTO},1)
	ifeq (${CRYPTO_INCLUDE_RACOON}${CRYPTO_INCLUDE_OPENSWAN},00)
               $(error Crypto enabled, but neither Racoon nor Openswan included)
	endif

	# This is necessary until bug 14538 (using 'setkey', from the
	# ipsec-tools RPM) is fixed, or we unconditionally include just
	# 'setkey' from the Racoon RPM in this setup.
	ifeq (${CRYPTO_INCLUDE_RACOON},0)
               $(error Crypto enabled, but Racoon not included)
	endif
endif

ifeq (${PROD_FEATURE_VIRT},1)
	ifeq (${PROD_FEATURE_VIRT_XEN},${PROD_FEATURE_VIRT_KVM})
                $(warning Must enable exactly one of KVM or Xen when PROD_FEATURE_VIRT=1)
	endif
	ifeq (${PROD_FEATURE_MULTITHREADING},0)
                $(warning Must enable PROD_FEATURE_MULTITHREADING if PROD_FEATURE_VIRT=1)
	endif
endif

ifndef RES_WORDSIZE_MATCH
	export RES_WORDSIZE_MATCH=1

	ifneq ($(filter ${PROD_TARGET_CPU_WORDSIZE},${PROD_TARGET_CPU_SUPPORTED_WORDSIZES}),${PROD_TARGET_CPU_WORDSIZE})
		export RES_WORDSIZE_MATCH=0
	endif
endif

ifndef RES_COMMON_STARTUP_DONE
	RES_COMMON_STARTUP_FORCE=1
	export RES_COMMON_STARTUP_DONE=1
endif


    PRE_DEFINES+= -D_LARGEFILE_SOURCE          \
                  -D_LARGEFILE64_SOURCE        \
                  -D_FILE_OFFSET_BITS=64

PRE_INCLUDES= -I${PROD_SRC_ROOT}/include \
	      -I${PROD_CUSTOMER_ROOT}/${PROD_PRODUCT_LC}/src/include

PRE_CFLAGS= -pipe -fsigned-char -fmerge-constants
PRE_CXXFLAGS= -pipe -fsigned-char -fmerge-constants
PRE_LDFLAGS=
PRE_LDADD=
PRE_LDADD_PROG=
PRE_LDADD_LIB=
PRE_LDADD_SO=

PRE_TARGET_FLAGS=

# Fortify is not yet enabled, and needs testing first.
# Note that Fortify needs -O, which we do not have, and do not support.
# Current known issues: openssh, snmp, syslogd, cron, parted, busybox
# and zile.
## ifneq (${FORTIFY_OVERRIDE},1)
##	PRE_CFLAGS+=-Wp,-D_FORTIFY_SOURCE=2
## endif

# Stack protector is not yet enabled, and needs testing first.
# Also login's Makefile would need fixups.
## ifeq (${PROD_TARGET_ARCH_FAMILY},X86)
##     ifneq (${STACK_PROTECTOR_OVERRIDE},1)
## 	PRE_CFLAGS+= \
## 		-fstack-protector \
## 		--param=ssp-buffer-size=4 \
##
##     endif
## endif

ifneq (${DEBUG_OVERRIDE},1)
	PRE_CFLAGS+=-g
	PRE_CXXFLAGS+=-g
	PRE_LDFLAGS+=-g
endif

# Relro and relro are not yet enabled.  Both need testing for correctness
# and performance.
## ifneq (${RELRO_OVERRIDE},1)
## 	PRE_LDFLAGS+=-Wl,-z,relro
## endif
## ifneq (${RELRO_NOW_OVERRIDE},1)
## 	PRE_LDFLAGS+=-Wl,-z,now
## endif


# Note, the compiler part of this is above
ifneq (${WARNINGS_ARE_ERRORS},0)
	PRE_LDFLAGS+=-Wl,--fatal-warnings
endif

ifeq (${PROD_TARGET_OS},LINUX)
ifneq (${DEFINES_OVERRIDE},1)
	PRE_CFLAGS+=-std=c99
	PRE_CXXFLAGS+=-std=c++98
endif
endif

ifdef PROD_ROOT_PREFIX
	PRE_CFLAGS+=-DPROD_ROOT_PREFIX=\"${PROD_ROOT_PREFIX}\"
	PRE_CXXFLAGS+=-DPROD_ROOT_PREFIX=\"${PROD_ROOT_PREFIX}\"
endif

# If an invalid wordsize is selected, use the default
ifneq ($(filter ${PROD_TARGET_CPU_WORDSIZE},${PROD_TARGET_CPU_SUPPORTED_WORDSIZES}),${PROD_TARGET_CPU_WORDSIZE})
	PROD_TARGET_CPU_WORDSIZE=${PROD_TARGET_CPU_DEFAULT_WORDSIZE}
endif

# Sparc base setting XXX there is currently no good story for Sparc 32 vs 64
ifeq (${PROD_TARGET_ARCH},SPARC)
    PRE_TARGET_FLAGS+= -mcpu=v9 -D__EXTENSIONS__
    ifeq (${PROD_TARGET_CPU_WORDSIZE},64)
	PRE_TARGET_FLAGS+=-m64
    else
	PRE_TARGET_FLAGS+=-m32
    endif
endif
ifeq (${PROD_TARGET_ARCH},SPARC64)
    PRE_TARGET_FLAGS+= -mcpu=v9 -D__EXTENSIONS__
    ifeq (${PROD_TARGET_CPU_WORDSIZE},64)
	PRE_TARGET_FLAGS+=-m64
    else
	PRE_TARGET_FLAGS+=-m32
    endif
endif

# For x86, set the target flags for 32 or 64 bit targets
ifeq (${PROD_HOST_ARCH_FAMILY},X86)

    ifeq (${PROD_TARGET_ARCH},I386)
	ifeq (${PROD_TARGET_PLATFORM_FULL},LINUX_EL_EL5)
	    PRE_TARGET_FLAGS+=-m32 -march=i386
	else
	    PRE_TARGET_FLAGS+=-m32 -march=i686
	endif
    endif

    ifeq (${PROD_TARGET_ARCH},X86_64)
        ifeq (${PROD_TARGET_CPU_WORDSIZE},64)
            PRE_TARGET_FLAGS+=-m64
	else
	    ifeq (${PROD_TARGET_PLATFORM_FULL},LINUX_EL_EL5)
	        PRE_TARGET_FLAGS+=-m32 -march=i386
	    else
	        PRE_TARGET_FLAGS+=-m32 -march=i686
	    endif
	endif
    endif
endif

# NOTE: This is not the normal PowerPC, which is cross-built.  This is
# only for development and test purposes when using an actual PowerPC
# system to build natively (which is not a supported way to build the
# product).
ifeq (${PROD_HOST_ARCH},PPC64)

    ifeq (${PROD_TARGET_ARCH},PPC)
	PRE_TARGET_FLAGS+=-m32
    endif
    ifeq (${PROD_TARGET_ARCH},PPC64)
        ifeq (${PROD_TARGET_CPU_WORDSIZE},64)
            PRE_TARGET_FLAGS+=-m64
	else
	    PRE_TARGET_FLAGS+=-m32
	endif
    endif
endif

# Profiling
do_profile=0
ifndef PROD_TARGET_HOST
    ifeq (${PROFILE},1)
	do_profile=1
    endif
else
    ifeq (${PROFILE_HOST},1)
	do_profile=1
    endif
endif
ifeq (${do_profile},1)
	PRE_CFLAGS+=-pg
	PRE_CXXFLAGS+=-pg
	PRE_LDFLAGS+=-pg
endif

# Dmalloc
ifeq (${DMALLOC}, 1)
	PRE_DEFINES+=-DDMALLOC
endif

# TCMalloc
ifeq (${TCMALLOC}, 1)
	PRE_DEFINES+=-DTCMALLOC
endif

# Java
ifeq (${PROD_FEATURE_JAVA},1)
     ifeq (${OBJ_JNI_NATIVE},1)
	PRE_INCLUDES+= -I${JAVA_JDK_ROOT}/include \
		       -I${JAVA_JDK_ROOT}/include/linux \
		       -I${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/java/sources/jniinclude

	PRE_CFLAGS+= -fexceptions \
		     -fasynchronous-unwind-tables
	PRE_CXXFLAGS+= -fexceptions \
		     -fasynchronous-unwind-tables
     endif

endif

# Modules
ifeq (${USES_MODULES},1)
    ifneq (${STATIC_MODULES},1)
        ifeq (${PROD_TARGET_OS},LINUX)
                PRE_LDFLAGS+=-Wl,--export-dynamic
                PRE_LDADD+=-Wl,-whole-archive
        endif
    else

        # This makes the linker search through all the provided archives
        #  to resolve any circular dependencies.
        ifeq (${PROD_TARGET_OS},LINUX)
                PRE_LDADD+=-Wl,--start-group
        endif
    endif
endif


do_static_debug_libc=0
ifeq (${PROFILE_LIBC}, 1)
	do_static_debug_libc=1
endif
ifeq (${DEBUG_LIBC}, 1)
	do_static_debug_libc=1
endif
ifeq (${do_static_debug_libc}, 1)
	PRE_LDFLAGS+=-static -L/usr/lib/debug/usr/lib
endif

# ==================================================
# POST_ variable settings
# ==================================================

POST_INCLUDES=-I.
POST_DEFINES=
POST_C_WARNINGS=
POST_CXX_WARNINGS=
POST_CFLAGS=
POST_CXXFLAGS=
POST_LDFLAGS=
POST_LDADD=
POST_LDADD_PROG=
POST_LDADD_LIB=
POST_LDADD_SO=


# Dmalloc
ifeq (${DMALLOC}, 1)
	POST_LDADD_PROG+=-ldmalloc
endif

# TCMalloc and associated Google performance tools
ifeq (${TCMALLOC}, 1)
    ifneq (${PROD_TARGET_ARCH}, I386)
            $(error TCMalloc currently only supported for 32-bit targets)
    endif

    # We only want this for binaries, not libs or so's
    POST_LDADD_PROG+=-L/usr/local/lib -ltcmalloc_and_profiler -lstdc++ -lm -lpthread
endif

# If customer turns on optimizer, turn off strict aliasing.
# This is required for correctness, given our current "type punning".
ifneq ($(filter -O%,${PRE_CFLAGS} ${PRE_TARGET_FLAGS} ${PRE_C_WARNINGS} \
		${PRE_DEFINES} ${PRE_INCLUDES} \
		${CUST_PRE_CFLAGS} ${CUST_PRE_C_WARNINGS} ${CUST_PRE_DEFINES} \
		${CUST_PRE_INCLUDES} \
		${CFLAGS} ${WARNINGS} ${DEFINES} ${INCLUDES} \
		${CUST_POST_CFLAGS} ${CUST_POST_C_WARNINGS} ${CUST_POST_DEFINES} \
		${CUST_POST_INCLUDES} \
		${POST_CFLAGS} ${POST_C_WARNINGS} ${POST_DEFINES} ${POST_INCLUDES}),)
	POST_CFLAGS+=-fno-strict-aliasing
endif
ifneq ($(filter -O%,${PRE_CXXFLAGS} ${PRE_TARGET_FLAGS} ${PRE_CXX_WARNINGS} \
		${PRE_DEFINES} ${PRE_INCLUDES} \
		${CUST_PRE_CXXFLAGS} ${CUST_PRE_CXX_WARNINGS} ${CUST_PRE_DEFINES} \
		${CUST_PRE_INCLUDES} \
		${CXXFLAGS} ${WARNINGS} ${DEFINES} ${INCLUDES} \
		${CUST_POST_CXXFLAGS} ${CUST_POST_CXX_WARNINGS} ${CUST_POST_DEFINES} \
		${CUST_POST_INCLUDES} \
		${POST_CXXFLAGS} ${POST_CXX_WARNINGS} ${POST_DEFINES} \
		${POST_INCLUDES}),)
	POST_CXXFLAGS+=-fno-strict-aliasing
endif

# See if the customer has turned off all warnings with '-w' .
# Alternatives are given in the warning message below.  We very much do
# not want our code used without warnings enabled, as it's caused problems
# before.
ifneq (${ALLOW_DISABLE_ALL_WARNINGS},1)
	ifneq ($(filter -w,${PRE_CFLAGS} ${PRE_TARGET_FLAGS} ${PRE_C_WARNINGS} \
		${PRE_DEFINES} ${PRE_INCLUDES} \
		${CFLAGS} ${WARNINGS} ${DEFINES} ${INCLUDES} \
		${POST_CFLAGS} ${POST_C_WARNINGS} ${POST_DEFINES} ${POST_INCLUDES} \
		${PRE_CXXFLAGS} ${PRE_CXX_WARNINGS} \
		${CXXFLAGS} \
		${POST_CXXFLAGS} ${POST_CXX_WARNINGS}),)
            $(warning *** Do not set -w , as it disables all warnings. )
            $(warning *** Use WARNINGS_ARE_ERRORS=1 to cause warnings to not fail the build)
            $(warning *** Use WARNINGS_EXTRA_OVERRIDE=1 or WARNINGS_OVERRIDE=1 if warnings cannot be addressed)
	endif
endif

# Modules
ifeq (${USES_MODULES},1)
    ifneq (${STATIC_MODULES},1)
        ifeq (${PROD_TARGET_OS},LINUX)
            POST_LDADD+=-Wl,-no-whole-archive
        endif
    else
        ifeq (${PROD_TARGET_OS},LINUX)
            POST_LDADD+=-Wl,--end-group
        endif
    endif

    ifeq (${PROD_TARGET_OS},LINUX)
        POST_LDADD+=-ldl
    endif
endif

ifneq (${LIBRT_OVERRIDE},1)
    ifeq (${PROD_TARGET_OS},LINUX)
        ifneq ($(strip $(filter libcommon,${DEP_LIB_NAMES})),)
            POST_LDADD+=-lrt
        endif
    endif
endif

ifeq (${PROFILE_LIBC}, 1)
    # Note: you'll have to build your own libc_p.a on EL5 and later
    POST_LDADD+=-lc_p
endif


# ==================================================
# ALL_ variable settings
# ==================================================

# Juniper added: ${CUSTOMER_DEFINES} ${CUSTOMER_INCLUDES}
ALL_CFLAGS=${PRE_CFLAGS} ${PRE_TARGET_FLAGS} ${PRE_C_WARNINGS} \
	   ${CUSTOMER_DEFINES} ${CUSTOMER_INCLUDES} \
	${PRE_DEFINES} ${PRE_INCLUDES} \
	${CUST_PRE_CFLAGS} ${CUST_PRE_C_WARNINGS} ${CUST_PRE_DEFINES} \
	${CUST_PRE_INCLUDES} \
	${CFLAGS} ${WARNINGS} ${DEFINES} ${INCLUDES} \
	${CUST_POST_CFLAGS} ${CUST_POST_C_WARNINGS} ${CUST_POST_DEFINES} \
	${CUST_POST_INCLUDES} \
	${POST_CFLAGS} ${POST_C_WARNINGS} ${POST_DEFINES} ${POST_INCLUDES}

# Juniper added: ${CUSTOMER_DEFINES} ${CUSTOMER_INCLUDES}
ALL_CXXFLAGS=${PRE_CXXFLAGS} ${PRE_TARGET_FLAGS} ${PRE_CXX_WARNINGS} \
	   ${CUSTOMER_DEFINES} ${CUSTOMER_INCLUDES} \
	${PRE_DEFINES} ${PRE_INCLUDES} \
	${CUST_PRE_CXXFLAGS} ${CUST_PRE_CXX_WARNINGS} ${CUST_PRE_DEFINES} \
	${CUST_PRE_INCLUDES} \
	${CXXFLAGS} ${WARNINGS} ${DEFINES} ${INCLUDES} \
	${CUST_POST_CXXFLAGS} ${CUST_POST_CXX_WARNINGS} ${CUST_POST_DEFINES} \
	${CUST_POST_INCLUDES} \
	${POST_CXXFLAGS} ${POST_CXX_WARNINGS} ${POST_DEFINES} \
	${POST_INCLUDES}

ALL_LDFLAGS=${PRE_TARGET_FLAGS} ${PRE_LDFLAGS} ${LDFLAGS} ${POST_LDFLAGS}

ALL_LDADD=${PRE_LDADD} ${LDADD} ${POST_LDADD}

ALL_LDADD_PROG=${PRE_LDADD_PROG} ${LDADD_PROG} ${POST_LDADD_PROG}
ALL_LDADD_LIB=${PRE_LDADD_LIB} ${LDADD_LIB} ${POST_LDADD_LIB}
ALL_LDADD_SO=${PRE_LDADD_SO} ${LDADD_SO} ${POST_LDADD_SO}


# ============================== defines ==============================

# This is used to recurse into subdirectories.  Must be called from a rule
# named TARGETNAME-recursive

define recurse-subdirs
	+@all_subdirs='${SUBDIRS}'; \
	if [ ! -z "$${all_subdirs}" ]; then \
	  target_name=`echo $@ | ${SED} 's/-recursive//'`; \
          failcom='exit 1'; \
          for f in x $$MAKEFLAGS; do \
            case $$f in \
              *=* | --[!k]*);; \
              *k*) failcom='fail=yes';; \
            esac; \
          done; \
	  for subdir in $$all_subdirs; do \
	  	(cd $$subdir && ${MAKE} $$target_name) || eval $$failcom; \
	  done; \
	fi; \
	test -z "$$fail"
endef


# This is used to recurse into makefiles in the same directory as the parent
# makefile.  Must be called from a rule named TARGETNAME-recursive

define recurse-submakefiles
	+@all_submakefiles='${SUBMAKEFILES}'; \
	if [ ! -z "$${all_submakefiles}" ]; then \
	  target_name=`echo $@ | ${SED} 's/-recursive//'`; \
          failcom='exit 1'; \
          for f in x $$MAKEFLAGS; do \
            case $$f in \
              *=* | --[!k]*);; \
              *k*) failcom='fail=yes';; \
            esac; \
          done; \
	  for submf in $$all_submakefiles; do \
		submf_dir=`dirname $$submf` ; \
		submf_file=`basename $$submf` ; \
	  	(cd $$submf_dir && ${MAKE} -f $$submf_file $$target_name) || eval $$failcom; \
	  done; \
	fi; \
	test -z "$$fail"
endef

ifeq (${PROD_BUILD_TIMES}${DATETIME_START_NUMBER},1)
	# This is a := on purpose
	DATETIME_START_NUMBER:=$(shell ${DATE} '+%s.%N' | sed 's/\.\(...\)......$$/.\1/')
endif

define build_times-startup
	@if [ "${PROD_BUILD_TIMES}" -eq 1 ]; then \
		BUILD_TIME_START_NUMBER="${DATETIME_START_NUMBER}" ; \
	fi
endef

define build_times-shutdown
	@if [ "${PROD_BUILD_TIMES}" -eq 1 ]; then \
		BUILD_TIME_END_NUMBER=`${DATE} '+%s.%N' | sed 's/\.\(...\)......$$/.\1/'` ;\
		BUILD_TIME_DURATION=`echo "$${BUILD_TIME_END_NUMBER}" "${DATETIME_START_NUMBER}" "-" "p" | dc` ; \
		echo "TIME: dir "`pwd`" took $${BUILD_TIME_DURATION} seconds" ; \
	fi
endef

define standard-startup
	@if [ "${PROD_BUILD_TIMES}" -eq 1 ]; then \
		BUILD_TIME_START_NUMBER="${DATETIME_START_NUMBER}" ; \
	fi
	+@if [ ! -z "${RES_COMMON_STARTUP_FORCE}" ]; then \
		(cd ${PROD_SRC_ROOT}/release; ${MAKE} common_startup) || exit 1 ; \
	fi
endef

define standard-shutdown
	@if [ "${PROD_BUILD_TIMES}" -eq 1 ]; then \
		BUILD_TIME_END_NUMBER=`${DATE} '+%s.%N' | sed 's/\.\(...\)......$$/.\1/'` ;\
		BUILD_TIME_DURATION=`echo "$${BUILD_TIME_END_NUMBER}" "${DATETIME_START_NUMBER}" "-" "p" | dc` ; \
		echo "TIME: dir "`pwd`" took $${BUILD_TIME_DURATION} seconds" ; \
	fi
endef

# This settings allows a binary to be built without attempting to build
# the libraries that it depends on.
ifneq (${COMMON_LIBDEPS_OVERRIDE},1)

# Look through LDADD and find the names of the base .a's we need
DEP_LIBS=$(filter ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/%,${LDADD})
# The names of the libraries we depend on ("libcommon")
DEP_LIB_NAMES=$(patsubst %/,%,$(dir $(subst ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/,,${DEP_LIBS})))
# The directories we'll have to make in (".../src/lib/libcli")
DEP_LIB_DIRS=$(addprefix ${PROD_SRC_ROOT}/lib/,${DEP_LIB_NAMES})


# Look through LDADD and find the names of the base "internal" .a's we need
# Internal libraries are those that only one component uses
DEP_ILIBS=$(filter ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${ILIB_DIR}/%,${LDADD})
# The names of the internal libraries we depend on
DEP_ILIB_NAMES=$(dir $(subst ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${ILIB_DIR}/,,${DEP_ILIBS}))
# The directories we'll have to make in (".../src/lib/libcli")
DEP_ILIB_DIRS=$(addprefix ${PROD_SRC_ROOT}/,${DEP_ILIB_NAMES})


# Look through LDADD and find the names of the customer .a's we need
DEP_CUSTLIBS=$(filter ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/%,${LDADD})
# The names of the libraries we depend on ("libfoo")
DEP_CUSTLIB_NAMES=$(patsubst %/,%,$(dir $(subst ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/,,${DEP_CUSTLIBS})))
# The directories we'll have to make in (".../customer/CUST/src/lib/libfoo")
DEP_CUSTLIB_DIRS=$(addprefix ${PROD_CUSTOMER_ROOT}/${PROD_PRODUCT_LC}/src/lib/,${DEP_CUSTLIB_NAMES})

# Look through LDADD and find the names of the customer thirdparty .a's we need
DEP_THIRDPARTY_LIBS=$(filter ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${THIRDPARTY_LIB_DIR}/%,${LDADD})
# The names of the libraries we depend on ("libfoo")
DEP_THIRDPARTY_LIB_NAMES=$(patsubst %/,%,$(dir $(subst ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${THIRDPARTY_LIB_DIR}/,,${DEP_THIRDPARTY_LIBS})))
# The directories we'll have to make in (".../customer/CUST/src/lib/libfoo")
DEP_THIRDPARTY_LIB_DIRS=$(addprefix ${PROD_CUSTOMER_ROOT}/${PROD_PRODUCT_LC}/src/thirdparty/lib/,${DEP_THIRDPARTY_LIB_NAMES})

# Look through LDADD and find the names of the customer "internal" .a's we need
# Internal libraries are those that only one component uses
DEP_CUSTILIBS=$(filter ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_ILIB_DIR}/%,${LDADD})
# The names of the internal libraries we depend on
DEP_CUSTILIB_NAMES=$(dir $(subst ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_ILIB_DIR}/,,${DEP_CUSTILIBS}))
# The directories we'll have to make in (".../customer/CUST/src/lib/libcli")
DEP_CUSTILIB_DIRS=$(addprefix ${PROD_CUSTOMER_ROOT}/${PROD_PRODUCT_LC}/src/,${DEP_CUSTILIB_NAMES})

endif # ! COMMON_LIBDEPS_OVERRIDE

# ============================== rules ==============================

ifneq (${COMMON_RULES_OVERRIDE},1)

# Clear out the suffix list, and set it explicitly
.SUFFIXES:
.SUFFIXES: .o .c .cc .cpp .y .l .s .a .so .mo .po

.PHONY: all install depend clean realclean release self-objs self-objs-parallel i18n-create-catalogs i18n-update-catalogs startup shutdown ensure_build_dirs ensure_install_dirs common_startup libdeps all-recursive install-recursive depend-recursive clean-recursive realclean-recursive release-recursive i18n-create-catalogs-recursive i18n-update-catalogs-recursive


########## C sources


########## C sources

# Generate dependency files from c files
${FULL_BUILD_DIR}/%.d : %.c
	@[ -d $(@D) ] || mkdir -p $(@D)
	@if test -d BIN_DIST; then \
	    echo "touch $@"; \
	    touch $@; \
	elif test -d `dirname \`dirname $<\``/BIN_DIST; then \
	    echo "touch $@"; \
	    touch $@; \
	elif test -f `dirname $<`/`basename $< .c`.o; then \
	    echo "touch $@"; \
	    touch $@; \
	else \
	    echo "${CC} ${ALL_CFLAGS} $< -E -MM -MP -MF $@" ; \
	    ${CC} ${ALL_CFLAGS} $< -E -MM -MP -MF $@ ; \
	fi ; \

ifneq (${OBJ_PIC},1)

# Compile c files to .o:  generate .d files as we go, it's no slower
${FULL_BUILD_DIR}/%.o: %.c
	@[ -d $(@D) ] || mkdir -p $(@D)
	@if test -d BIN_DIST; then \
	    echo "${CP} BIN_DIST/$(@F) $@" ; \
	    ${CP} BIN_DIST/$(@F) $@ ; \
	elif test -d `dirname \`dirname $<\``/BIN_DIST; then \
	    echo "${CP} `dirname \`dirname $<\``/BIN_DIST/`basename $< .c`.o $@"; \
	    ${CP} `dirname \`dirname $<\``/BIN_DIST/`basename $< .c`.o $@; \
	elif test -f `dirname $<`/`basename $< .c`.o; then \
	    echo "${CP} `dirname $<`/`basename $< .c`.o $@"; \
	    ${CP} `dirname $<`/`basename $< .c`.o $@; \
	else \
	    echo "${CC} ${ALL_CFLAGS} -MMD -MP -c $< -o $@" ; \
	    ${CC} ${ALL_CFLAGS} -MMD -MP -c $< -o $@ ; \
	fi ; \

else

# Compile c files to PIC .o:  generate .d files as we go, it's no slower
${FULL_BUILD_DIR}/%.o: %.c
	@[ -d $(@D) ] || mkdir -p $(@D)
	@if test -d BIN_DIST; then \
	    echo "${CP} BIN_DIST/$(@F) $@" ; \
	    ${CP} BIN_DIST/$(@F) $@ ; \
	elif test -d `dirname \`dirname $<\``/BIN_DIST; then \
	    echo "${CP} `dirname \`dirname $<\``/BIN_DIST/`basename $< .c`.o $@"; \
	    ${CP} `dirname \`dirname $<\``/BIN_DIST/`basename $< .c`.o $@; \
	elif test -f `dirname $<`/`basename $< .c`.o; then \
	    echo "${CP} `dirname $<`/`basename $< .c`.o $@"; \
	    ${CP} `dirname $<`/`basename $< .c`.o $@; \
	else \
	    echo "${CC} -fPIC -DPIC ${ALL_CFLAGS} -MMD -MP -c $< -o $@" ; \
	    ${CC} -fPIC -DPIC ${ALL_CFLAGS} -MMD -MP -c $< -o $@ ; \
	fi ; \

endif

# Compile c files to .so :  generate .d files as we go, it's no slower
${FULL_BUILD_DIR}/%.so: %.c
	@[ -d $(@D) ] || mkdir -p $(@D)
	@if test -d BIN_DIST ; then \
	    echo "touch $(patsubst %.so,%.lo,$@)"; \
	    touch $(patsubst %.so,%.lo,$@); \
	    echo "cp BIN_DIST/$(@F) $@"; \
	    cp BIN_DIST/$(@F) $@; \
	elif test -d `dirname \`dirname $<\``/BIN_DIST; then \
	    echo "touch $(patsubst %.so,%.lo,$@)"; \
	    touch $(patsubst %.so,%.lo,$@); \
	    echo "${CP} `dirname \`dirname $<\``/BIN_DIST/`basename $< .c`.so $@;" \
	    ${CP} `dirname \`dirname $<\``/BIN_DIST/`basename $< .c`.so $@; \
	else \
	    echo "${CC} -fPIC -DPIC ${ALL_CFLAGS} -MMD -MP -MT $@ -c $< -o $(patsubst %.so,%.lo,$@)" ; \
	    ${CC} -fPIC -DPIC ${ALL_CFLAGS} -MMD -MP -MT $@ -c $< -o $(patsubst %.so,%.lo,$@) || exit 1; \
	    echo "${CC} -fPIC -shared ${ALL_CFLAGS} $(patsubst %.so,%.lo,$@) -o $@ ${ALL_LDADD} ${ALL_LDADD_SO}" ; \
	    ${CC} -fPIC -shared ${ALL_CFLAGS} $(patsubst %.so,%.lo,$@) -o $@ ${ALL_LDADD} ${ALL_LDADD_SO} || exit 1; \
	    echo "${RM} $(patsubst %.so,%.lo,$@)" ; \
	    ${RM} $(patsubst %.so,%.lo,$@) || exit 1; \
	fi ; \

########## C++ sources

# Generate dependency files from C++ files
define cxx_dep
	@[ -d $(@D) ] || mkdir -p $(@D)
	@cmd="${CXX} ${ALL_CXXFLAGS} $< -E -MM -MP -MF $@" ; \
	if test ! "${PROD_BUILD_VERBOSE}" -eq 1; then \
		echo "Making dependency for $<"; \
	else \
		echo "$${cmd}"; \
	fi; \
	$${cmd}
endef

${FULL_BUILD_DIR}/%.d : %.cc
	$(cxx_dep)

${FULL_BUILD_DIR}/%.d : %.cpp
	$(cxx_dep)


ifneq (${OBJ_PIC},1)

# Compile c++ files to .o:  generate .d files as we go, it's no slower
define cxx_to_o
	@[ -d $(@D) ] || mkdir -p $(@D)
	@cmd="${CXX} ${ALL_CXXFLAGS} -MMD -MP -c $< -o $@" ; \
	if test ! "${PROD_BUILD_VERBOSE}" -eq 1; then \
		echo "Compiling ${PROD_TARGET_COMPILE_FOR_TEXT}$<"; \
	else \
		echo "$${cmd}"; \
	fi; \
	$${cmd}
endef

${FULL_BUILD_DIR}/%.o: %.cc
	$(cxx_to_o)

${FULL_BUILD_DIR}/%.o: %.cpp
	$(cxx_to_o)

else

# Compile c++ files to PIC .o:  generate .d files as we go, it's no slower
define cxx_to_o_pic
	@[ -d $(@D) ] || mkdir -p $(@D)
	@cmd="${CXX} -fPIC -DPIC ${ALL_CXXFLAGS} -MMD -MP -c $< -o $@" ; \
	if test ! "${PROD_BUILD_VERBOSE}" -eq 1; then \
		echo "Compiling as PIC ${PROD_TARGET_COMPILE_FOR_TEXT}$<"; \
	else \
		echo "$${cmd}"; \
	fi; \
	$${cmd}
endef

${FULL_BUILD_DIR}/%.o: %.cc
	$(cxx_to_o_pic)

${FULL_BUILD_DIR}/%.o: %.cpp
	$(cxx_to_o_pic)

endif

########## Parallel helper rules

# Start a sub-make to do the parallel part if '-j' was specified.  This
# will expand to nothing if there is no '-j' on the make command line.
# Note that this calls the "self-objs-parallel" rule with '-j' active. 
define standard-self-objs
	@$(if $(and $(findstring j,$(value MAKEFLAGS)),$(filter-out 0,${PROD_BUILD_VERBOSE})) ,echo "Parallel make portion starting")
	@$(if $(findstring j,$(value MAKEFLAGS)),+${MAKE} -s self-objs-parallel || exit 1,)
	@$(if $(and $(findstring j,$(value MAKEFLAGS)),$(filter-out 0,${PROD_BUILD_VERBOSE})) ,echo "Parallel make portion ending")
endef


########## General helper rules

print_make_var-%:
	@echo "${*}=$($*)"

print_make_var_def-%:
	@echo '${*}=$(value $*)'

print_make_var_export-%:
	@echo "${*}=$($*)"
	@echo "export ${*}"

print_make_var_value-%:
	@echo "$($*)"

endif # ! COMMON_RULES_OVERRIDE


