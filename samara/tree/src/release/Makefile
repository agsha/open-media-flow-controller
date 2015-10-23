#  Makefile
#
#
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

# Do we want to build the ISO and tarball of freely distributable software
ifdef DO_BUILD_FREEDIST
SUBDIRS=freedist
endif

# This makefile is not "-j" safe.  Do not remove the following line
.NOTPARALLEL:

.PHONY: ensure_tool_dirs version version_force version_clean
.PHONY: release release-recursive
.PHONY: auto_ensure_tool_versions
.PHONY: image ensure_image_dirs mfg floppies ensure_mfg_dirs
.PHONY: mfgcd ensure_mfgcd_dirs mfgusb ensure_mfgusb_dirs
.PHONY: fskels

# ============================== ALL ==============================
all: startup ensure_build_dirs version all-recursive shutdown


ifneq (${PROD_TARGET_OS},SUNOS)
    MAYBE_ENSURE_TOOL_VERSIONS=ensure_tool_versions
    MAYBE_AUTO_ENSURE_TOOL_VERSIONS=auto_ensure_tool_versions
else
    MAYBE_ENSURE_TOOL_VERSIONS=
    MAYBE_AUTO_ENSURE_TOOL_VERSIONS=
endif

#  Could check various things there for validity, like ${PROD_SRC_ROOT}
startup: ${MAYBE_ENSURE_TOOL_VERSIONS}
	$(build_times-startup)

shutdown:
	$(build_times-shutdown)

# This gets called once by the top-level make of each make invocation.
# We use it to do things like potentially update the versioning information.
# We also use it to make sure of the make and compiler version.
common_startup: ${MAYBE_AUTO_ENSURE_TOOL_VERSIONS} version
	@RES_CC_VERSION=$$(${CC} -dumpversion | tail -1) ; \
	RES_MAKE_VERSION=$$(${MAKE} --version | head -1 | ${SED} 's/^GNU Make \(version \)*\([0-9\.]*\).*/\2/') ; \
	if [ "${PROD_FEATURE_JAVA}" = 1 ]; then \
		RES_JAVAC_VERSION=$$([ ! -z "${PROD_TARGET_JAVAC_VERSION}" -a -x ${JAVAC} ] && ${JAVAC} -version 2>&1 | head -1 | ${SED} 's/^javac \(.*\)/\1/') ; \
	fi ; \
	RES_ENV_TOOLS_MATCH=1 ; \
	if [ "$${RES_CC_VERSION}" != "${PROD_TARGET_CC_VERSION}" ]; then \
		RES_ENV_TOOLS_MATCH=0 ; \
	fi ; \
	if [ "$${RES_MAKE_VERSION}" != "${PROD_HOST_MAKE_VERSION}" ]; then \
		RES_ENV_TOOLS_MATCH=0 ; \
	fi ; \
	if [ "${PROD_FEATURE_JAVA}" = 1 ]; then \
		if [ "$${RES_JAVAC_VERSION}" != "${PROD_TARGET_JAVAC_VERSION}" ]; then \
			RES_ENV_TOOLS_MATCH=0 ; \
		fi ; \
	fi ; \
	if [ ! "$${RES_ENV_TOOLS_MATCH}" -eq 1 ]; then \
		${ECHO} "Makefile:0: *** Environment tools do not have required versions."; \
		echo "host: ${PROD_HOST_PLATFORM_FULL} ${PROD_HOST_ARCH}" ; \
		echo "target: ${PROD_TARGET_PLATFORM_FULL} ${PROD_TARGET_ARCH} / ${PROD_TARGET_CPU}" ; \
		echo "gcc: $${RES_CC_VERSION} (req: ${PROD_TARGET_CC_VERSION})" ; \
		echo "make: $${RES_MAKE_VERSION} (req: ${PROD_HOST_MAKE_VERSION})" ; \
		if [ "${PROD_FEATURE_JAVA}" = "1" ]; then \
			echo "javac: $${RES_JAVAC_VERSION} (req: ${PROD_TARGET_JAVAC_VERSION})" ; \
		fi; \
		if test -z "${IGNORE_NO_ENV_TOOLS_MATCH}"; then \
			exit 1; \
		fi; \
	fi

	@if [ ! "${RES_WORDSIZE_MATCH}" -eq 1 ]; then \
		${ECHO} "Makefile:0: *** Wordsize ${PROD_TARGET_CPU_WORDSIZE} is not valid for ${PROD_TARGET_CPU_LC}.  Must be one of: ${PROD_TARGET_CPU_SUPPORTED_WORDSIZES} "; \
		exit 1; \
	fi

# If I18N is on, make sure they specified some locales
	@if [ "${PROD_FEATURE_I18N_SUPPORT}" = "1" -a -z "${PROD_LOCALES}" ]; then \
		${ECHO} "Makefile:0: *** Must set PROD_LOCALES if I18N is enabled" ;\
		exit 1; \
	fi

# Force the version to be updated
	@if [ "${PROD_VERSION_FORCE_UPDATE}" = "1" ]; then \
		(cd ${PROD_SRC_ROOT}/release && ${MAKE} version_force) || exit 1 ;\
	fi


FULL_BUILD_DIR=${PROD_BUILD_OUTPUT_DIR}/release

ensure_build_dirs:
	@if test ! -d ${FULL_BUILD_DIR}/; then \
		mkdir -p ${FULL_BUILD_DIR}; \
		if test ! -d ${FULL_BUILD_DIR}/; then \
			${ECHO} "Unable to create ${FULL_BUILD_DIR}."; \
			 exit 1; \
		 fi; \
		 ${ECHO} "Created directory ${FULL_BUILD_DIR}"; \
	 fi


# The basic tools that are required to build the product for this target
-include ${PROD_TREE_ROOT}/src/base_os/${PROD_TARGET_PLATFORM_LC}/${PROD_TARGET_PLATFORM_VERSION_LC}/arch_${PROD_TARGET_ARCH_LC}/build_required.inc


# Graft point for customer-specific additions to the list of required rpms.
# If you are adding an rpm to the image which has a library, you will want
# to ensure that the build system has the exact same version, so that the
# header files you are building against match the library in question.
#
# To do this you should add the rpm name into the file referenced in the
# 'include' directive below.  Additions should be of the form:
# TOOLS_REQUIRED_RPMS_HOST_LINUX_EL_EL6+=myrpm-3.4.5.x86_64
#
-include ${PROD_CUSTOMER_ROOT}/${PROD_PRODUCT_LC}/src/base_os/${PROD_TARGET_PLATFORM_LC}/${PROD_TARGET_PLATFORM_VERSION_LC}/arch_${PROD_TARGET_ARCH_LC}/build_required.inc

FSKEL_VAR_TAR_OPTIONS=

# If we should try to produce manufacturing boot and root floppy images
# that are sized to be put on actually floppy disks.  This means they
# would be padded out to something like 1440K or 2880K.  This is not
# needed for PXE, CDROM or USB-based manufactures.  Those using a
# hypervisor like VMware are recommended to use the 'mfgcd' image
# instead of enabling this.
MFG_PRODUCE_FD_FLOPPIES?=0

# General settings for the release, such as for image signing.
# Also, you could set FSKEL_VAR_TAR_OPTIONS to exclude certain files
# in /var from being restored on every boot.
-include ${PROD_CUSTOMER_ROOT}/${PROD_PRODUCT_LC}/src/release/Makefile.inc

auto_ensure_tool_versions: ${FULL_BUILD_DIR}/.did_ensure_tool_versions
	@

CUST_BUILD_REQUIRED_INC=${PROD_CUSTOMER_ROOT}/${PROD_PRODUCT_LC}/src/base_os/${PROD_TARGET_PLATFORM_LC}/${PROD_TARGET_PLATFORM_VERSION_LC}/arch_${PROD_TARGET_ARCH_LC}/build_required.inc
HAS_CUST_BUILD_REQUIRED_INC:=$(shell test -f ${CUST_BUILD_REQUIRED_INC} && echo 1)
ifeq (${HAS_CUST_BUILD_REQUIRED_INC},1)
MAYBE_CUST_BUILD_REQUIRED_INC=${CUST_BUILD_REQUIRED_INC}
else
MAYBE_CUST_BUILD_REQUIRED_INC=
endif

${FULL_BUILD_DIR}/.did_ensure_tool_versions: ${MAYBE_CUST_BUILD_REQUIRED_INC} $(wildcard ${PROD_TREE_ROOT}/src/base_os/${PROD_TARGET_PLATFORM_LC}/${PROD_TARGET_PLATFORM_VERSION_LC}/arch_*/build_required.inc)
	@[ -d $(@D) ] || mkdir -p $(@D)
	@if [ ! -z "${MAYBE_ENSURE_TOOL_VERSIONS}" ]; then \
		${MAKE} ${MAYBE_ENSURE_TOOL_VERSIONS} || exit 1 ; \
	fi ; \
	touch $@

CLEANFILES+=${FULL_BUILD_DIR}/.did_ensure_tool_versions

TOOLS_REQUIRED_RPMS=${TOOLS_REQUIRED_RPMS_HOST_${PROD_TARGET_PLATFORM_FULL}}

ensure_tool_versions:
	@if [ -z "${PROD_TARGET_COMPILE_FOR_TEXT}" ]; then \
		PTCFT="" ; \
	else \
		PTCFT=" ($(strip ${PROD_TARGET_COMPILE_FOR_TEXT})) " ; \
	fi ; \
	RPM_PATH="$$(which rpm)" ; \
	if [ ! -z "${TOOLS_REQUIRED_RPMS}" -a "${PROD_TARGET_ARCH_FAMILY}" = "${PROD_HOST_ARCH_FAMILY}" -a "$${RPM_PATH}" != "/bin/rpm" ]; then \
		${ECHO} "*** PATH is finding the wrong \"rpm\" ${RPM} binary: $${RPM_PATH} should be /bin/rpm" ; \
		exit 1 ; \
	fi ; \
	if [ "${PROD_HOST_PLATFORM_FULL}" = "${PROD_TARGET_PLATFORM_FULL}" ]; then \
		PTPS="" ; \
	else \
		PTPS="${PROD_TARGET_PLATFORM_FULL} / " ; \
	fi ; \
	echo "Ensuring build system RPMs have correct versions on host ${PROD_HOST_PLATFORM_FULL} ${PROD_HOST_ARCH} for target $${PTPS}${PROD_TARGET_ARCH} / ${PROD_TARGET_CPU}$${PTCFT}"
	@FAIL_COUNT=0 ;\
	MISSING_LIST="$${PREV_MISSING_LIST}" ;\
	BADVERSION_LIST="$${PREV_BADVERSION_LIST}" ;\
	if [ -z "${TOOLS_REQUIRED_RPMS}" ]; then \
		${ECHO} "List of required RPMs not found on host ${PROD_HOST_PLATFORM_FULL} ${PROD_HOST_ARCH} for $${PTPS}${PROD_TARGET_ARCH}" ;\
		FAIL_COUNT=1 ;\
	fi ; \
	for reqrpm_full in ${TOOLS_REQUIRED_RPMS}; do \
		reqrpm=`echo "$${reqrpm_full}" | awk '{print $$1}'` ; \
		reqrpm_extra=`echo "$${reqrpm_full}" | sed "s/^$${reqrpm}\( *\)\(.*\)\( *\)$$/\2/"` ; \
		FAIL=0 ;\
		FAILQ1=0 ;\
		FAILQ2=0 ;\
		EXTRA_MSG_HAVE= ;\
		EXTRA_MSG_NEED= ;\
		if test "${PROD_BUILD_VERBOSE}" -eq 1; then \
			echo "Checking for RPM $${reqrpm} $${reqrpm_extra}" ;\
		fi ; \
		rout=`${RPM} --queryformat '%{NAME}-%{VERSION}-%{RELEASE}.%{ARCH}\n' -q $${reqrpm}` || FAIL=1 ;\
		if [ $${FAIL} -ne 0 -o "$${rout}" != "$${reqrpm}" ]; then \
			FAILQ1=1 ; \
		fi ; \
		if [ $${FAILQ1} -eq 0 -a ! -z "$${reqrpm_extra}" ]; then \
			FAIL=0 ;\
			match_pkgid_list=`echo "$${reqrpm_extra}" | grep pkgid= | sed 's/^.*=\(.*\)$$/\1/' | sed 's/,/ /g'` ; \
			if [ ! -z "$${match_pkgid_list}" ]; then \
				EXTRA_MSG_NEED=" pkgid $${match_pkgid_list}" ; \
				have_match=0 ;\
				for match_pkgid in $${match_pkgid_list}; do \
					FAIL=0 ; \
					rout=`${RPM} --queryformat '%{NAME}-%{VERSION}-%{RELEASE}.%{ARCH}\n' -q --pkgid $${match_pkgid}` || FAIL=1 ;\
					if [ $${FAIL} -eq 0 -a "$${rout}" = "$${reqrpm}" ]; then \
						break ; \
					fi ; \
				done ;\
				if [ $${FAIL} -ne 0 -o "$${rout}" != "$${reqrpm}" ]; then \
					emv=`${RPM} --queryformat '%{PKGID}\n' -q $${reqrpm}` ;\
					EXTRA_MSG_HAVE=" pkgid $${emv}" ;\
					FAILQ2=1 ; \
				fi ; \
			fi ; \
		fi ; \
		if [ $${FAILQ1} -ne 0 -o "$${FAILQ2}" -ne 0 ]; then \
			FAILVER=0 ;\
			base_reqrpm=`echo $${reqrpm} | sed 's/^\(.*\)-\([^-]*\)-\([^-]*\)\.\([^-\.]*\)$$/\1.\4/'` ;\
			base_rout=`${RPM} --queryformat '%{NAME}-%{VERSION}-%{RELEASE}.%{ARCH}\n' -q $${base_reqrpm}` || FAILVER=1 ;\
			if [ $${FAILVER} -ne 0 -o -z "$${base_rout}" ]; then \
				${ECHO} "Required RPM not found for: $${base_reqrpm}" ;\
				${ECHO} "    Need version: $${reqrpm}" ;\
				MISSING_LIST="$${MISSING_LIST} $${reqrpm}" ;\
			else \
				${ECHO} "Required RPM not found for: $${base_reqrpm}" ;\
				${ECHO} "    Have version: $${base_rout}$${EXTRA_MSG_HAVE}" ;\
				${ECHO} "    Need version: $${reqrpm}$${EXTRA_MSG_NEED}" ;\
				BADVERSION_LIST="$${BADVERSION_LIST} $${reqrpm}" ;\
			fi ; \
			FAIL_COUNT=`expr $${FAIL_COUNT} + 1` ;\
		fi; \
	done ;\
	if test $${FAIL_COUNT} -ne 0 -a -z "${IGNORE_NO_ENV_TOOLS_MATCH}"; then \
		if [ ! -z "$${MISSING_LIST}" ]; then \
			${ECHO} "" ;\
			${ECHO} "To add missing RPMs, try as 'root':" ;\
			${ECHO} "" ;\
			${ECHO} "yum install $${MISSING_LIST}" ;\
		fi ;\
		if [ ! -z "$${BADVERSION_LIST}" ]; then \
			${ECHO} "" ;\
			${ECHO} "NOTE: this won't work if an installed RPM version is newer than required" ;\
			${ECHO} "To update older RPMs to the required versions, try as 'root':" ;\
			${ECHO} "" ;\
			${ECHO} "yum update $${BADVERSION_LIST}" ;\
		fi ;\
		${ECHO} "" ;\
		${ECHO} "Failed to verify $$(echo ${TOOLS_REQUIRED_RPMS} | wc -w) RPMs." ;\
	fi; \
	if test $${FAIL_COUNT} -eq 0 ; then \
		${ECHO} "Verified $$(echo ${TOOLS_REQUIRED_RPMS} | wc -w) RPMs." ;\
	fi; \
	if [ "${ENSURE_TOOL_VERSION_SINGLE}" != "1" ]; then \
                echo PROD_TARGET_HOST=${PROD_TARGET_HOST} ; \
                echo PROD_TARGET_ARCH=${PROD_TARGET_ARCH} ; \
		if [ -z "${PROD_TARGET_HOST}" -a "${PROD_TARGET_ARCH}" = "X86_64" ]; then \
			export PREV_MISSING_LIST="$${MISSING_LIST}" ;\
			export PREV_BADVERSION_LIST="$${BADVERSION_LIST}" ;\
			echo SKIP!!!! ${MAKE} PROD_TARGET_ARCH=I386 ENSURE_TOOL_VERSION_SINGLE=1 ensure_tool_versions || exit 1 ; \
		fi ; \
		if [ "${PROD_TARGET_ARCH}" != "I386" -a "${PROD_TARGET_ARCH}" != "X86_64" ]; then \
			export PREV_MISSING_LIST="$${MISSING_LIST}" ;\
			export PREV_BADVERSION_LIST="$${BADVERSION_LIST}" ;\
			if [ -z "${PROD_TARGET_HOST}" ]; then \
				${MAKE} PROD_TARGET_HOST=1 ENSURE_TOOL_VERSION_SINGLE=1 ensure_tool_versions || exit 1 ; \
			fi ; \
		fi ; \
	fi; \
	unset PREV_MISSING_LIST ;\
	unset PREV_BADVERSION_LIST ;\
	if test $${FAIL_COUNT} -ne 0 -a -z "${IGNORE_NO_ENV_TOOLS_MATCH}"; then \
		${ECHO} "" ;\
		${ECHO} "NOTE: If the suggested commands do not work, or you cannot find the required" ;\
		${ECHO} "NOTE: RPMs, please see the Samara Technical Guide section on common build" ;\
		${ECHO} "NOTE: questions." ;\
		${ECHO} "" ;\
		exit 1; \
	elif [ "${ENSURE_TOOL_VERSION_SINGLE}" != "1" ]; then \
		if [ ! -d ${FULL_BUILD_DIR}/ ]; then \
			${MAKE} ensure_build_dirs ; \
		fi ; \
		touch ${FULL_BUILD_DIR}/.did_ensure_tool_versions ; \
	fi

# What we'll generate: .c, .mk, .sh, and .txt
MKVER_OUTPUTS=$(addprefix ${FULL_BUILD_DIR}/build_version,.c .mk .sh .txt .pl)

version_force: version_clean version

version: ensure_build_dirs ${MKVER_OUTPUTS}

CONF_FILE=${PROD_TREE_ROOT}/src/release/build_version.conf
HAS_CONF_FILE:=$(shell test -f ${CONF_FILE} && echo 1)

ifeq (${HAS_CONF_FILE},1)
MAYBE_CONF_FILE=${CONF_FILE}
else
MAYBE_CONF_FILE=
endif

${MKVER_OUTPUTS}: ${MAYBE_CONF_FILE} ${PROD_TREE_ROOT}/src/release/mkver.sh ${PROD_TREE_ROOT}/src/include/build_version.h ${PROD_TREE_ROOT}/src/mk/common.mk Makefile
	@echo "Making build version information files"; \
	DATE=${DATE} ;\
	export DATE ;\
	if [ -f ${CONF_FILE} ]; then \
		echo ${PROD_TREE_ROOT}/src/release/mkver.sh -f ${CONF_FILE} ;\
		${PROD_TREE_ROOT}/src/release/mkver.sh -f ${CONF_FILE} || exit 1 ;\
	else \
		datestr=`${DATE} '+%Y-%m-%d %H:%M:%S'`; \
		echo ${PROD_TREE_ROOT}/src/release/mkver.sh "${PROD_TREE_ROOT}" "$$datestr" "dev" "1" "" ; \
		${PROD_TREE_ROOT}/src/release/mkver.sh "${PROD_TREE_ROOT}" "$$datestr" "dev" "1" "" || exit 1; \
	fi

version_clean:
	@echo "Removing old build version information files"
	@${RM} -f ${MKVER_OUTPUTS}

CLEANFILES+=${MKVER_OUTPUTS}

# ============================== INSTALL ==============================

FULL_INSTALL_DIR=${PROD_INSTALL_OUTPUT_DIR}/image/${PROD_INSTALL_PREFIX}/release
FULL_IMAGE_INSTALL_DIR=${PROD_INSTALL_OUTPUT_DIR}/image/etc
FULL_IMAGE_PKI_INSTALL_DIR=${PROD_INSTALL_OUTPUT_DIR}/image/etc/pki
FULL_IMAGE_TPKG_INSTALL_DIR=${PROD_INSTALL_OUTPUT_DIR}/image/etc/pki/tpkg
FULL_IMAGE_TPKG_PRIVATE_INSTALL_DIR=${PROD_INSTALL_OUTPUT_DIR}/image/etc/pki/tpkg/private
FULL_ROOTFLOP_INSTALL_DIR=${PROD_INSTALL_OUTPUT_DIR}/rootflop/etc
FULL_ROOTFLOP_PKI_INSTALL_DIR=${PROD_INSTALL_OUTPUT_DIR}/rootflop/etc/pki
FULL_ROOTFLOP_TPKG_INSTALL_DIR=${PROD_INSTALL_OUTPUT_DIR}/rootflop/etc/pki/tpkg
FULL_ROOTFLOP_TPKG_PRIVATE_INSTALL_DIR=${PROD_INSTALL_OUTPUT_DIR}/rootflop/etc/pki/tpkg/private
FULL_FREEDIST_INSTALL_DIR=${PROD_INSTALL_OUTPUT_DIR}/freedist/dist/release
FULL_CUSTDIST_INSTALL_DIR=${PROD_INSTALL_OUTPUT_DIR}/custdist
ALL_INSTALL_DIRS=\
	${FULL_INSTALL_DIR} \
	${FULL_IMAGE_INSTALL_DIR} \
	${FULL_IMAGE_PKI_INSTALL_DIR} \
	${FULL_IMAGE_TPKG_INSTALL_DIR} \
	${FULL_IMAGE_TPKG_PRIVATE_INSTALL_DIR} \
	${FULL_ROOTFLOP_INSTALL_DIR} \
	${FULL_ROOTFLOP_PKI_INSTALL_DIR} \
	${FULL_ROOTFLOP_TPKG_INSTALL_DIR} \
	${FULL_ROOTFLOP_TPKG_PRIVATE_INSTALL_DIR} \
	${FULL_FREEDIST_INSTALL_DIR} \
	${FULL_CUSTDIST_INSTALL_DIR} \

# We install two copies of this in the image, as we really want it under
# /opt/tms, but sometimes only the root disk will be mountable, so dump one
# in /etc as well.

install: ensure_install_dirs install-recursive
	${INSTALL} -m 644 ${FULL_BUILD_DIR}/build_version.txt ${FULL_INSTALL_DIR}
	${INSTALL} -m 755 ${FULL_BUILD_DIR}/build_version.sh ${FULL_INSTALL_DIR}
	${INSTALL} -m 644 ${FULL_BUILD_DIR}/build_version.txt ${FULL_IMAGE_INSTALL_DIR}
	${INSTALL} -m 755 ${FULL_BUILD_DIR}/build_version.sh ${FULL_IMAGE_INSTALL_DIR}
	${INSTALL} -m 644 ${FULL_BUILD_DIR}/build_version.txt ${FULL_ROOTFLOP_INSTALL_DIR}
	${INSTALL} -m 755 ${FULL_BUILD_DIR}/build_version.sh ${FULL_ROOTFLOP_INSTALL_DIR}
	${INSTALL} -m 644 ${FULL_BUILD_DIR}/build_version.txt ${FULL_FREEDIST_INSTALL_DIR}
	${INSTALL} -m 755 ${FULL_BUILD_DIR}/build_version.sh ${FULL_FREEDIST_INSTALL_DIR}
	${INSTALL} -m 644 ${FULL_BUILD_DIR}/build_version.txt ${FULL_FREEDIST_INSTALL_DIR}
	${INSTALL} -m 755 ${FULL_BUILD_DIR}/build_version.sh ${FULL_FREEDIST_INSTALL_DIR}

	if [ "${PROD_FEATURE_IMAGE_SECURITY}" = "1" -a "${IMAGE_PKG_INSTALL_KEYS_DISABLE}" != "1" ]; then \
	    if [ ! -z "${IMAGE_PKG_SIGN_KEYRING}" ]; then \
		${INSTALL} -m 644 ${IMAGE_PKG_SIGN_KEYRING} ${FULL_IMAGE_TPKG_INSTALL_DIR}/trusted.gpg ;\
		${INSTALL} -m 644 ${IMAGE_PKG_SIGN_KEYRING} ${FULL_ROOTFLOP_TPKG_INSTALL_DIR}/trusted.gpg ;\
	    fi; \
	    if [ ! -z "${IMAGE_PKG_ENCRYPT_SECRET_KEYRING}" -a ! -z "${IMAGE_PKG_ENCRYPT_KEYRING}" ]; then \
		${INSTALL} -m 400 ${IMAGE_PKG_ENCRYPT_KEYRING} ${FULL_IMAGE_TPKG_INSTALL_DIR}/encrypt.gpg ;\
		${INSTALL} -m 400 ${IMAGE_PKG_ENCRYPT_KEYRING} ${FULL_ROOTFLOP_TPKG_INSTALL_DIR}/encrypt.gpg ;\
		${INSTALL} -m 400 ${IMAGE_PKG_ENCRYPT_SECRET_KEYRING} ${FULL_IMAGE_TPKG_PRIVATE_INSTALL_DIR}/encrypt-priv.gpg ;\
		${INSTALL} -m 400 ${IMAGE_PKG_ENCRYPT_SECRET_KEYRING} ${FULL_ROOTFLOP_TPKG_PRIVATE_INSTALL_DIR}/encrypt-priv.gpg ;\
	    fi; \
	fi; \

ensure_install_dirs:
	@for subdir in ${ALL_INSTALL_DIRS}; do \
		if test ! -d $${subdir}/; then \
			mkdir -p $${subdir}/; \
			if test ! -d $${subdir}/; then \
				${ECHO} "Unable to create $${subdir}."; \
				 exit 1; \
			 fi; \
			 ${ECHO} "Created directory $${subdir}"; \
	    	fi; \
	done

# ============================== RELEASE ==============================
#
# Later this may make more than just the installable image (such as boot stuff)
#

# Append to this variable any customer specific release dependencies
RELEASE_DEPS_EXTRA?=

release: mfg image ${RELEASE_DEPS_EXTRA} release-recursive

# For backward compatibility
floppies: mfg

# ============================== IMAGE ==============================
# The 'install' target MUST be called from the top before this target
# is called.

FULL_IMAGE_DIR=${PROD_RELEASE_OUTPUT_DIR}/image

ensure_image_dirs:
	@if test ! -d ${FULL_IMAGE_DIR}/; then \
		mkdir -p ${FULL_IMAGE_DIR}; \
		if test ! -d ${FULL_IMAGE_DIR}/; then \
			${ECHO} "Unable to create ${FULL_IMAGE_DIR}."; \
			 exit 1; \
		 fi; \
		 ${ECHO} "Created directory ${FULL_IMAGE_DIR}"; \
	 fi


# XXX file permissions


# The plan here is to:
#    - generate a manifest that has md5s and permission info for every file, copy in
#    - make a .tar of the installed files
#    - make a .tbz of the .tar
#    - make a stage with the .tbz and some version files
#    - md5sum the files of the stage: this md5sum is to check for corruption
#    - lfi the files of the stage
#    - make a zip of the stage (.tbz, the md5sum, the lfis, and the build
#      version files)
#
#    Encryption and signing (optional, also can be signed but not encrypted):
#      - rename the existing md5sum and lfi files to have -original in names
#      - encrypt the image .tbz file in the stage using gpg, and rename
#            to .tbz.gpg
#      - md5sum the files of the stage: this md5sum is to check for corruption
#      - lfi the files of the stage
#      - sign the lfi files in the stage using gpg


# This can be 'bzip2', 'gzip', or 'tar'.
#
# bzip2 is smaller, but gzip can be rsync'd better.  tar is for testing, as
# the unzip'd image is much bigger, although it does produce reasonable
# images due to zip's compression.

IMAGE_COMPRESSION_TYPE_DEFAULT=bzip2
ifeq (${PROD_TARGET_ARCH},PPC)
	IMAGE_COMPRESSION_TYPE_DEFAULT=gzip
endif

IMAGE_COMPRESSION_TYPE?=${IMAGE_COMPRESSION_TYPE_DEFAULT}

ifeq ($(strip ${IMAGE_COMPRESSION_TYPE}),)
	IMAGE_COMPRESSION_TYPE=bzip2
endif

IMAGE_COMPRESSED_EXTENSION=INVALID
ifeq (${IMAGE_COMPRESSION_TYPE},bzip2)
	IMAGE_COMPRESSED_EXTENSION=tbz
endif
ifeq (${IMAGE_COMPRESSION_TYPE},gzip)
	IMAGE_COMPRESSED_EXTENSION=tgz
endif
ifeq (${IMAGE_COMPRESSION_TYPE},tar)
	IMAGE_COMPRESSED_EXTENSION=tar
endif
ifeq (${IMAGE_COMPRESSION_TYPE},none)
	IMAGE_COMPRESSED_EXTENSION=tar
endif

IMAGE_PKG_SIGN?=0
IMAGE_PKG_SIGN_KEYRING?=
IMAGE_PKG_SIGN_USER?=

# Customers can set this to relax the requirement that the build system
# will only produce signed images if PROD_FEATURE_RESTRICT_CMDS is on.
# Note that does not allow unsigned images to be installed, only built.
IMAGE_PKG_SIGN_RESTRICT_OVERRIDE?=0

IMAGE_PKG_ENCRYPT?=0
IMAGE_PKG_ENCRYPT_SECRET_FILE?=0
HOST_BIN_LFI=${PROD_INSTALL_OUTPUT_DIR}/host/${BIN_DIR}/lfi
TPKG_CREATE_PATH=${PROD_TREE_ROOT}/src/base_os/common/script_files/tpkg_create.sh
TPKG_SECURE_PATH=${PROD_TREE_ROOT}/src/base_os/common/script_files/tpkg_secure.sh
TPKG_QUERY_PATH=${PROD_TREE_ROOT}/src/base_os/common/script_files/tpkg_query.sh

# Ensure that if using PROD_FEATURE_RESTRICT_CMDS and
# PROD_FEATURE_IMAGE_SECURITY that images are signed.  Note that
# elsewhere we ensure that PROD_FEATURE_IMAGE_SECURITY is always on if
# PROD_FEATURE_RESTRICT_CMDS is.

ifeq (${PROD_FEATURE_RESTRICT_CMDS}${PROD_FEATURE_IMAGE_SECURITY},11)
    ifneq (${IMAGE_PKG_SIGN_RESTRICT_OVERRIDE},1)
        ifneq (${IMAGE_PKG_SIGN},1)
            $(error Image signing must be enabled when PROD_FEATURE_RESTRICT_CMDS is enabled, or security may be compromised)
        endif
    endif
endif



# By default, we will turn on IMAGE_INCLUDE_MFG if virtualization is in use, so
# we can manufacture the image file onto the VM from inside the VM.
#
# This will increase image size by about 5 MB.
ifeq (${PROD_FEATURE_VIRT},1)
	IMAGE_INCLUDE_MFG?=1
else
	IMAGE_INCLUDE_MFG?=0
endif

# Juniper added: test -n "$${BUILD_CUSTOMER_SUFFIX}" && build_suffix=$${BUILD_CUSTOMER_SUFFIX}
image: ensure_image_dirs fskels
	@if test ! -f ${FULL_INSTALL_DIR}/build_version.sh; then \
		${ECHO} "Error: must do install before making image"; \
		exit 1; \
	fi

	@if [ "${IMAGE_INCLUDE_MFG}" = "1" ]; then \
		if [ -z "$$(find ${PROD_RELEASE_OUTPUT_DIR}/rootflop/ -type f -print)" -o -z "$$(find ${PROD_RELEASE_OUTPUT_DIR}/bootflop/)" ]; then \
			${ECHO} "Error: must make 'mfg' before image"; \
			exit 1; \
		fi ; \
	fi

	@${RM} -f ${PROD_INSTALL_OUTPUT_DIR}/MANIFEST ;\
	${RM} -f ${PROD_INSTALL_OUTPUT_DIR}/image/etc/MANIFEST ;\
	touch ${PROD_INSTALL_OUTPUT_DIR}/MANIFEST ;\
	${ECHO} "Generating image manifest" ; \
	(cd ${PROD_INSTALL_OUTPUT_DIR}/image; find . -print0 | xargs -0 -r ${PROD_INSTALL_OUTPUT_DIR}/host/${BIN_DIR}/lfi > ${PROD_INSTALL_OUTPUT_DIR}/MANIFEST ) || exit 1 ;\
	${INSTALL} -m 644 ${PROD_INSTALL_OUTPUT_DIR}/MANIFEST ${PROD_INSTALL_OUTPUT_DIR}/image/etc/MANIFEST || exit 1

	@.  ${FULL_INSTALL_DIR}/build_version.sh ; \
	build_suffix=$${BUILD_PROD_NAME}-$${BUILD_TARGET_ARCH_LC}$${BUILD_TARGET_HWNAME_LC:+-$${BUILD_TARGET_HWNAME_LC}}-`echo $${BUILD_DATE} | sed 's/[-:]//g' | sed 's/ /-/g'` ;\
	test -n "$${BUILD_CUSTOMER_SUFFIX}" && build_suffix=$${BUILD_CUSTOMER_SUFFIX} ;\
	IMAGE_ROOT_NAME=image-$${build_suffix}; \
	IMAGE_FULL_NAME=$${IMAGE_ROOT_NAME}.tar; \
	IMAGE_COMPRESSED_FULL_NAME=$${IMAGE_ROOT_NAME}.${IMAGE_COMPRESSED_EXTENSION}; \
	IMAGE_FINAL_FULL_NAME=$${IMAGE_ROOT_NAME}.img; \
	${ECHO} "Tar'ing up image contents" ; \
	${RM} -f ${FULL_IMAGE_DIR}/$${IMAGE_FULL_NAME} ${FULL_IMAGE_DIR}/$${IMAGE_COMPRESSED_FULL_NAME} ;\
	(cd ${PROD_INSTALL_OUTPUT_DIR}/image; ${TAR} -cpf ${FULL_IMAGE_DIR}/$${IMAGE_FULL_NAME} . ) || exit 1 ;\
	IMAGE_UNCOMPRESSED_SIZE=`du -b ${FULL_IMAGE_DIR}/$${IMAGE_FULL_NAME} | awk '{print $$1}'` ;\
	${ECHO} "Compressing image with ${IMAGE_COMPRESSION_TYPE}" ;\
	if [ -z "${IMAGE_COMPRESSION_TYPE}" -o "${IMAGE_COMPRESSION_TYPE}" = "bzip2" ]; then \
		${BZIP} -9 -c ${FULL_IMAGE_DIR}/$${IMAGE_FULL_NAME} > ${FULL_IMAGE_DIR}/$${IMAGE_COMPRESSED_FULL_NAME} || exit 1 ;\
	elif [ "${IMAGE_COMPRESSION_TYPE}" = "gzip" ]; then \
		${GZIP} -9 --rsyncable -c ${FULL_IMAGE_DIR}/$${IMAGE_FULL_NAME} > ${FULL_IMAGE_DIR}/$${IMAGE_COMPRESSED_FULL_NAME} || exit 1 ;\
	elif [ "${IMAGE_COMPRESSION_TYPE}" = "tar" -o "${IMAGE_COMPRESSION_TYPE}" = "none" ]; then \
		if [ ${FULL_IMAGE_DIR}/$${IMAGE_FULL_NAME} != ${FULL_IMAGE_DIR}/$${IMAGE_COMPRESSED_FULL_NAME} ]; then \
			${CP} -p ${FULL_IMAGE_DIR}/$${IMAGE_FULL_NAME} ${FULL_IMAGE_DIR}/$${IMAGE_COMPRESSED_FULL_NAME} || exit 1;\
		fi; \
	else \
		${ECHO} "ERROR: invalid IMAGE_COMPRESSION_TYPE: ${IMAGE_COMPRESSION_TYPE}" ;\
		exit 1 ; \
	fi; \
	if [ ${FULL_IMAGE_DIR}/$${IMAGE_FULL_NAME} != ${FULL_IMAGE_DIR}/$${IMAGE_COMPRESSED_FULL_NAME} ]; then \
		${RM} -f ${FULL_IMAGE_DIR}/$${IMAGE_FULL_NAME} ;\
	fi; \
	IMAGE_SIZE=`du -b ${FULL_IMAGE_DIR}/$${IMAGE_COMPRESSED_FULL_NAME} | awk '{print $$1}'` ;\
	${RM} -f ${FULL_IMAGE_DIR}/$${IMAGE_FINAL_FULL_NAME} ;\
	${RM} -r ${FULL_IMAGE_DIR}/stage ;\
	mkdir -p ${FULL_IMAGE_DIR}/stage ;\
	cp -p ${FULL_IMAGE_DIR}/$${IMAGE_COMPRESSED_FULL_NAME} ${FULL_IMAGE_DIR}/stage || exit 1 ;\
	${INSTALL} -m 755 ${FULL_BUILD_DIR}/build_version.sh ${FULL_IMAGE_DIR}/stage || exit 1 ;\
	${INSTALL} -m 755 ${FULL_BUILD_DIR}/build_version.txt ${FULL_IMAGE_DIR}/stage || exit 1 ;\
	printf "IMAGE_PAYLOAD_SIZE=$${IMAGE_SIZE}\nexport IMAGE_PAYLOAD_SIZE\nIMAGE_PAYLOAD_UNCOMPRESSED_SIZE=$${IMAGE_UNCOMPRESSED_SIZE}\nexport IMAGE_PAYLOAD_UNCOMPRESSED_SIZE\nIMAGE_PAYLOAD_COMPRESSION_TYPE=${IMAGE_COMPRESSION_TYPE}\nexport IMAGE_PAYLOAD_COMPRESSION_TYPE\n" > ${FULL_IMAGE_DIR}/stage/image_vars.sh ;\
	printf "./build_version.sh\n./image_vars.sh\n" > ${FULL_IMAGE_DIR}/stage/tpkg-vars ;\
	\
	if [ "${IMAGE_INCLUDE_MFG}" = "1" -a "$${BUILD_TARGET_ARCH_FAMILY}" = "X86" ]; then \
		SOURCE_BOOTFLOP_LINUX_NAME=vmlinuz-bootflop-$${build_suffix} ;\
		SOURCE_ROOTFLOP_NAME=rootflop-$${build_suffix}.img ;\
		\
		MFG_KERNEL_NAME=mfg-kernel-$${build_suffix} ;\
		MFG_INITRD_NAME=mfg-initrd-$${build_suffix} ;\
		\
		${ECHO} "Copying in manufacturing environment" ;\
		cp -p ${PROD_RELEASE_OUTPUT_DIR}/bootflop/$${SOURCE_BOOTFLOP_LINUX_NAME} ${FULL_IMAGE_DIR}/stage/$${MFG_KERNEL_NAME} || exit 1 ;\
		cp -p ${PROD_RELEASE_OUTPUT_DIR}/rootflop/$${SOURCE_ROOTFLOP_NAME} ${FULL_IMAGE_DIR}/stage/$${MFG_INITRD_NAME} || exit 1 ;\
		\
		printf "./mfg_vars.sh\n" >> ${FULL_IMAGE_DIR}/stage/tpkg-vars ;\
		printf "MFG_KERNEL_NAME=\"$${MFG_KERNEL_NAME}\"\n" > ${FULL_IMAGE_DIR}/stage/mfg_vars.sh ;\
		printf "MFG_INITRD_NAME=\"$${MFG_INITRD_NAME}\"\n" >> ${FULL_IMAGE_DIR}/stage/mfg_vars.sh ;\
		MFG_KERNEL_SIZE=`du -b ${FULL_IMAGE_DIR}/stage/$${MFG_KERNEL_NAME} | awk '{print $$1}'` ;\
		printf "MFG_KERNEL_PAYLOAD_SIZE=$${MFG_KERNEL_SIZE}\n" >> ${FULL_IMAGE_DIR}/stage/mfg_vars.sh ;\
		MFG_INITRD_SIZE=`du -b ${FULL_IMAGE_DIR}/stage/$${MFG_INITRD_NAME} | awk '{print $$1}'` ;\
		printf "MFG_INITRD_PAYLOAD_SIZE=$${MFG_INITRD_SIZE}\n" >> ${FULL_IMAGE_DIR}/stage/mfg_vars.sh ;\
		\
		${PROD_TREE_ROOT}/src/release/mfg_vars.sh -M ${PROD_INSTALL_OUTPUT_DIR}/image >> ${FULL_IMAGE_DIR}/stage/mfg_vars.sh ;\
		\
	fi; \
	\
	EARGS= ;\
	signed=0 ;\
	signed_arg= ;\
	encr=0 ;\
	if [ "${PROD_FEATURE_IMAGE_SECURITY}" = "1" -a \
		! -z "${IMAGE_PKG_SIGN}" -a ${IMAGE_PKG_SIGN} -ne 0 ]; then \
	    signed=1 ;\
	    signed_arg=-s ;\
	    if [ ! -z "${IMAGE_PKG_ENCRYPT}" -a ${IMAGE_PKG_ENCRYPT} -ne 0 ]; then \
		EARGS="-e -K ${IMAGE_PKG_ENCRYPT_KEYRING} -P ${IMAGE_PKG_ENCRYPT_SECRET_KEYRING} -U ${IMAGE_PKG_ENCRYPT_KEYNAME}" ; \
		encr=1 ;\
	    fi; \
	    ${TPKG_CREATE_PATH} -i -f ${FULL_IMAGE_DIR}/stage -l ${HOST_BIN_LFI} || exit 1 ;\
	    if [ $${encr} -eq 0 ]; then \
		${ECHO} "Signing image contents" ;\
	    else \
		${ECHO} "Signing and encrypting image contents" ;\
	    fi; \
	    ${TPKG_SECURE_PATH} -i -f ${FULL_IMAGE_DIR}/stage -l ${HOST_BIN_LFI} $${EARGS} \
		-k ${IMAGE_PKG_SIGN_KEYRING} -p ${IMAGE_PKG_SIGN_SECRET_KEYRING} -u ${IMAGE_PKG_SIGN_KEYNAME}  || exit 1 ;\
	fi; \
	${ECHO} "Generating final image from stage" ;\
	${TPKG_CREATE_PATH} -o ${FULL_IMAGE_DIR}/$${IMAGE_FINAL_FULL_NAME} -f ${FULL_IMAGE_DIR}/stage -l ${HOST_BIN_LFI} || exit 1 ;\
	${RM} -r ${FULL_IMAGE_DIR}/stage ;\
	${RM} -f ${FULL_IMAGE_DIR}/$${IMAGE_COMPRESSED_FULL_NAME} ;\
	${ECHO} "Verifying final image" ;\
	${TPKG_QUERY_PATH} -t -f ${FULL_IMAGE_DIR}/$${IMAGE_FINAL_FULL_NAME} $${signed_arg} -k ${IMAGE_PKG_SIGN_KEYRING} -l ${HOST_BIN_LFI} || exit 1 ;\
	${ECHO} "Created image ${FULL_IMAGE_DIR}/$${IMAGE_FINAL_FULL_NAME}" ;\

# If we lose a whole partition (like /var in particular), we want some
# files such that system startup is still possible
fskels:
	@if [ ! -f ${FULL_INSTALL_DIR}/build_version.sh -o ! -d ${PROD_INSTALL_OUTPUT_DIR}/image/var -o ! -d ${PROD_INSTALL_OUTPUT_DIR}/image/etc/fskel ]; then \
		${ECHO} "Error: must do install before making image"; \
		exit 1; \
	fi

	@${ECHO} "Making /var fallback filesystem skeleton" ;\
	${RM} -rf ${PROD_INSTALL_OUTPUT_DIR}/image/etc/fskel/fskel-10-k-base_var.tgz ;\

	(cd ${PROD_INSTALL_OUTPUT_DIR}/image; ${TAR} --exclude='./var/home/*' ${FSKEL_VAR_TAR_OPTIONS} -cpzf ${PROD_INSTALL_OUTPUT_DIR}/image/etc/fskel/fskel-10-k-base_var.tgz ./var ) || exit 1

	@${ECHO} "Making /config fallback filesystem skeleton" ;\
	${RM} -rf ${PROD_INSTALL_OUTPUT_DIR}/image/etc/fskel/fskel-20-k-base_config.tgz ;\
	(cd ${PROD_INSTALL_OUTPUT_DIR}/image; ${TAR} -cpzf ${PROD_INSTALL_OUTPUT_DIR}/image/etc/fskel/fskel-20-k-base_config.tgz `find ./config -mindepth 1 -type d -print` ) || exit 1

# ============================== MFG  ==============================
# The 'install' target MUST be called from the top before this target
# is called.

FULL_MFG_DIRS= \
	${PROD_RELEASE_OUTPUT_DIR}/rootflop \
	${PROD_RELEASE_OUTPUT_DIR}/bootflop

ifeq (${PROD_FEATURE_UBOOT},1)
	FULL_MFG_DIRS+= \
		        ${PROD_RELEASE_OUTPUT_DIR}/u-boot \

endif

ensure_mfg_dirs:
	@for subdir in ${FULL_MFG_DIRS}; do \
		if test ! -d $${subdir}/; then \
			mkdir -p $${subdir}/; \
			if test ! -d $${subdir}/; then \
				${ECHO} "Unable to create $${subdir}."; \
				 exit 1; \
			 fi; \
			 ${ECHO} "Created directory $${subdir}"; \
	    	fi; \
	done

# Juniper added: test -n "$${BUILD_CUSTOMER_SUFFIX}" && build_suffix=$${BUILD_CUSTOMER_SUFFIX}
mfg: ensure_mfg_dirs
	@if test ! -f ${FULL_INSTALL_DIR}/build_version.sh; then \
		${ECHO} "Error: must do install before making manufacturing environment"; \
		exit 1; \
	fi

	@.  ${FULL_INSTALL_DIR}/build_version.sh ; \
	export CROSS_PATH_PREADD ; \
	build_suffix=$${BUILD_PROD_NAME}-$${BUILD_TARGET_ARCH_LC}$${BUILD_TARGET_HWNAME_LC:+-$${BUILD_TARGET_HWNAME_LC}}-`echo $${BUILD_DATE} | sed 's/[-:]//g' | sed 's/ /-/g'` ;\
	test -n "$${BUILD_CUSTOMER_SUFFIX}" && build_suffix=$${BUILD_CUSTOMER_SUFFIX} ;\
	if test "$${BUILD_TARGET_ARCH_FAMILY}" = "X86" ; then \
		if test "${MFG_PRODUCE_FD_FLOPPIES}" = "1" ; then \
			BOOTFLOP_NAME=bootflop-$${build_suffix}.img; \
		else \
			BOOTFLOP_NAME= ; \
		fi ; \
		ROOTFLOP_NAME=rootflop-$${build_suffix}.img; \
		BOOTFLOP_LINUX_NAME=vmlinuz-bootflop-$${build_suffix}; \
		KERNEL_IMAGE_OUTPUT_NAME=vmlinuz ; \
	elif test "$${BUILD_TARGET_ARCH_FAMILY}" = "PPC" ; then \
		BOOTFLOP_NAME= ; \
		ROOTFLOP_NAME=rootfs-$${build_suffix} ; \
		ROOTFLOP_RAW_NAME=rootflop-$${build_suffix}.img ; \
		BOOTFLOP_LINUX_NAME=vmlinuz-bootflop-$${build_suffix}; \
		KERNEL_IMAGE_OUTPUT_NAME=vmlinuz ; \
		KERNEL_FDT_OUTPUT_NAME=fdt ; \
		BOOTFLOP_FDT_NAME=fdt-$${build_suffix} ; \
		if [ "${PROD_FEATURE_UBOOT}" = "1" ]; then \
			UBOOT_INSTALL_NAME=u-boot.bin ; \
			UBOOT_RELEASE_NAME=u-boot-$${build_suffix}.bin ; \
		fi ; \
	else \
		${ECHO} "Error: unsupported target arch family: $${BUILD_TARGET_ARCH_FAMILY}" ; \
		exit 1 ; \
	fi ; \
	PAD_ARGS= ; \
	if [ "$${BUILD_TARGET_ARCH_FAMILY}" != "X86" -o "${MFG_PRODUCE_FD_FLOPPIES}" != "1" ]; then \
		PAD_ARGS=-p ; \
		BOOTFLOP_NAME= ; \
	fi; \
	ROOTFLOP_ARGS= ; \
	if test ! -z "$${ROOTFLOP_NAME}" ; then \
		ROOTFLOP_ARGS="-r $${ROOTFLOP_NAME}" ; \
	fi ; \
	if test ! -z "$${ROOTFLOP_RAW_NAME}" ; then \
		ROOTFLOP_ARGS="$${ROOTFLOP_ARGS} -R $${ROOTFLOP_RAW_NAME}" ; \
	fi ; \
	BOOTFLOP_ARGS= ; \
	if test ! -z "$${BOOTFLOP_NAME}" ; then \
		BOOTFLOP_ARGS="-b $${BOOTFLOP_NAME}" ; \
	fi ; \
	ARCH_ARGS="-a ${PROD_TARGET_ARCH_FAMILY_LC}" ;\
	${CP} -p ${PROD_INSTALL_OUTPUT_DIR}/bootflop/$${KERNEL_IMAGE_OUTPUT_NAME} ${PROD_RELEASE_OUTPUT_DIR}/bootflop/$${BOOTFLOP_LINUX_NAME} || exit 1; \
	if test "$${BUILD_TARGET_ARCH_FAMILY}" = "PPC" ; then \
		${CP} -p ${PROD_INSTALL_OUTPUT_DIR}/bootflop/$${KERNEL_FDT_OUTPUT_NAME} ${PROD_RELEASE_OUTPUT_DIR}/bootflop/$${BOOTFLOP_FDT_NAME} || exit 1; \
	fi ; \
	if [ "${PROD_FEATURE_UBOOT}" = "1" ]; then \
		${CP} -p ${PROD_INSTALL_OUTPUT_DIR}/u-boot/$${BUILD_TARGET_HWNAME_LC}/$${UBOOT_INSTALL_NAME} ${PROD_RELEASE_OUTPUT_DIR}/u-boot/$${UBOOT_RELEASE_NAME} || exit 1; \
	fi ; \
	${PROD_TREE_ROOT}/src/release/mkbootfl.sh $${ROOTFLOP_ARGS} $${BOOTFLOP_ARGS} $${PAD_ARGS} $${ARCH_ARGS} || exit 1; \
	${ECHO} "Created manufacturing environment."


FULL_MFGCD_DIRS= \
	${PROD_RELEASE_OUTPUT_DIR}/mfgcd \


ensure_mfgcd_dirs:
	@for subdir in ${FULL_MFGCD_DIRS}; do \
		if test ! -d $${subdir}/; then \
			mkdir -p $${subdir}/; \
			if test ! -d $${subdir}/; then \
				${ECHO} "Unable to create $${subdir}."; \
				 exit 1; \
			 fi; \
			 ${ECHO} "Created directory $${subdir}"; \
	    	fi; \
	done

ISOLINUX_BIN=/usr/share/syslinux/isolinux.bin

# Juniper added: test -n "$${BUILD_CUSTOMER_SUFFIX}" && build_suffix=$${BUILD_CUSTOMER_SUFFIX}
mfgcd: ensure_mfgcd_dirs
	@if test ! -f ${FULL_INSTALL_DIR}/build_version.sh; then \
		${ECHO} "Error: must do install before making mfg ISO"; \
		exit 1; \
	fi

	@. ${FULL_INSTALL_DIR}/build_version.sh ; \
	\
	build_suffix=$${BUILD_PROD_NAME}-$${BUILD_TARGET_ARCH_LC}$${BUILD_TARGET_HWNAME_LC:+-$${BUILD_TARGET_HWNAME_LC}}-`echo $${BUILD_DATE} | sed 's/[-:]//g' | sed 's/ /-/g'` ;\
	test -n "$${BUILD_CUSTOMER_SUFFIX}" && build_suffix=$${BUILD_CUSTOMER_SUFFIX} ;\
	MFGCD_ISO_NAME=mfgcd-$${build_suffix}.iso; \
	export MFGCD_ISO_NAME ; \
	\
	ROOTFLOP_NAME=rootflop-$${build_suffix}.img; \
	export ROOTFLOP_NAME ; \
	\
	BOOTFLOP_LINUX_NAME=vmlinuz-bootflop-$${build_suffix}; \
	export BOOTFLOP_LINUX_NAME ; \
	\
	IMAGE_NAME=image-$${build_suffix}.img; \
	export IMAGE_NAME ; \
	\
	if test ! -z "${ISOLINUX_BIN}" ; then \
		ISOLINUX_BIN=${ISOLINUX_BIN} ; \
		export ISOLINUX_BIN ; \
	fi ; \
	\
	${PROD_TREE_ROOT}/src/release/mkmfgcd.sh || exit 1; \
	${ECHO} "Created mfgcd ISO."


FULL_MFGUSB_DIRS= \
	${PROD_RELEASE_OUTPUT_DIR}/mfgusb \


ensure_mfgusb_dirs:
	@for subdir in ${FULL_MFGUSB_DIRS}; do \
		if test ! -d $${subdir}/; then \
			mkdir -p $${subdir}/; \
			if test ! -d $${subdir}/; then \
				${ECHO} "Unable to create $${subdir}."; \
				 exit 1; \
			 fi; \
			 ${ECHO} "Created directory $${subdir}"; \
	    	fi; \
	done

# Juniper added test -n "$${BUILD_CUSTOMER_SUFFIX}" && build_suffix=$${BUILD_CUSTOMER_SUFFIX}
mfgusb: ensure_mfgusb_dirs
	@if test ! -f ${FULL_INSTALL_DIR}/build_version.sh; then \
		${ECHO} "Error: must do install before making mfg USB"; \
		exit 1; \
	fi

	@. ${FULL_INSTALL_DIR}/build_version.sh ; \
	\
	build_suffix=$${BUILD_PROD_NAME}-$${BUILD_TARGET_ARCH_LC}$${BUILD_TARGET_HWNAME_LC:+-$${BUILD_TARGET_HWNAME_LC}}-`echo $${BUILD_DATE} | sed 's/[-:]//g' | sed 's/ /-/g'` ;\
	test -n "$${BUILD_CUSTOMER_SUFFIX}" && build_suffix=$${BUILD_CUSTOMER_SUFFIX} ;\
	\
	MFGUSB_ZIP_NAME=mfgusb-$${build_suffix}.zip; \
	export MFGUSB_ZIP_NAME ; \
	\
	ARCH_ARGS="-a ${PROD_TARGET_ARCH_FAMILY_LC}" ;\
	ROOTFLOP_NAME=rootflop-$${build_suffix}.img; \
	export ROOTFLOP_NAME ; \
	unset SUBDIR_ARGS ; \
	if test "$${BUILD_TARGET_ARCH_FAMILY}" = "PPC" ; then \
		SUBDIR_ARGS="-d $${BUILD_TARGET_HWNAME_LC}" ; \
		BOOTFLOP_FDT_NAME=fdt-$${build_suffix} ; \
		export BOOTFLOP_FDT_NAME ; \
	fi ; \
	\
	BOOTFLOP_LINUX_NAME=vmlinuz-bootflop-$${build_suffix}; \
	export BOOTFLOP_LINUX_NAME ; \
	\
	IMAGE_NAME=image-$${build_suffix}.img; \
	export IMAGE_NAME ; \
	\
	if [ ! -z "${MFGUSB_ROOTFS_SIZE}" ]; then \
		export ROOTFS_SIZE="${MFGUSB_ROOTFS_SIZE}" ;\
	fi ; \
	${PROD_TREE_ROOT}/src/release/mkmfgusb.sh ${MKMFGUSB_ARGS} $${ARCH_ARGS} $${SUBDIR_ARGS} || exit 1; \
	${ECHO} "Created mfgusb ZIP."


# ============================== CLEAN ==============================
clean: clean-recursive
	${RM} ${CLEANFILES}

# ============================== DEPEND ==============================
depend: depend-recursive

# ============================== REALCLEAN ==============================
realclean: realclean-recursive clean

# Recursion
all-recursive install-recursive release-recursive depend-recursive \
  clean-recursive realclean-recursive:
	$(recurse-subdirs)
