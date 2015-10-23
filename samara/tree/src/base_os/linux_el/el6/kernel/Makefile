#  Makefile

include ${PROD_TREE_ROOT}/src/mk/common.mk

# This makefile is not "-j" safe.  Do not remove the following line
.NOTPARALLEL:

.PHONY: version extract_sources
.PHONY: build_image_kernel_uni build_image_kernel_smp build_bootfloppy_kernel_uni
.PHONY: menuconfig_image_kernel_uni menuconfig_image_kernel_smp menuconfig_bootfloppy_kernel_uni
.PHONY: config_image_kernel_uni config_image_kernel_smp config_bootfloppy_kernel_uni
.PHONY: symtree_image_kernel_uni symtree_image_kernel_smp symtree_bootfloppy_kernel_uni
.PHONY: patch_image_kernel_uni patch_image_kernel_smp patch_bootfloppy_kernel_uni
.PHONY: install_image_kernel_uni install_image_kernel_smp install_bootfloppy_kernel_uni
.PHONY: print_kernel_version_image_kernel_uni print_kernel_version_image_kernel_smp print_kernel_version_bootfloppy_kernel_uni
.PHONY: print_kernel_version_active_image_kernel_uni print_kernel_version_active_image_kernel_smp print_kernel_version_active_bootfloppy_kernel_uni
.PHONY: print_kernel_versions_active_all_image print_kernel_versions_active_all_bootfloppy

#
# Set and export ARCH from PROD_TARGET_ARCH so that the Linux kernel makefile
# will use our concept of it, and not auto-detect it from uname.
#
# NOTE: This assumes that Linux kernel ARCH setting is named the same way as
# our PROD_TARGET_ARCH, unless special-cased below
#
ARCH_ORIG:=${ARCH}
ifeq (${PROD_TARGET_ARCH_LC},ppc)
    export ARCH=powerpc
else
    export ARCH=${PROD_TARGET_ARCH_LC}
endif

PATH_ORIG:=${PATH}
ifneq ($(strip ${CROSS_PATH_PREADD}),)
    export PATH:=${CROSS_PATH_PREADD}:${PATH}
endif

DEPMOD_ORIG:=${DEPMOD}
ifneq ($(strip ${CROSS_DEPMOD}),)
    export DEPMOD:=${CROSS_DEPMOD}
endif

# By default, strip kernel modules when we install them
export INSTALL_MOD_STRIP=1

KERNEL_DIR=${PROD_TREE_ROOT}/src/base_os/${PROD_TARGET_PLATFORM_LC}/${PROD_TARGET_PLATFORM_VERSION_LC}/kernel

ifeq (${PROD_TARGET_ARCH_FAMILY},X86)
	KERNEL_IMAGE_NAME=bzImage
	KERNEL_IMAGE_OUTPUT_NAME=vmlinuz
	KERNEL_ALL_TARGETS=${KERNEL_IMAGE_NAME}
	KERNEL_ALL_EXTRA_TARGETS=
	KERNEL_OVERLAY_DIR=src
	KERNEL_PATCHES_DIR=patches
	KERNEL_UNI_SUFFIX=-uni
	KERNEL_SMP_SUFFIX=-smp
	KERNEL_CONF_USE_HWNAME?=0
endif

#
# By default, we now build each kernel with -j, even if the main build
# is not running with -j .  We give a -j of 1.25*num_cpus to stop the load
# from getting too high.  Customers can set EXTRA_KERNEL_MAKE_FLAGS
# themselves (in their kernel Makefile.inc) if they want different
# behavior.
#
num_cpus:=$(shell cat /proc/cpuinfo | egrep '^processor.*: ' | wc -l)
# make sure we end up with a number, even if we got confused
j_arg:=$(shell echo ${num_cpus} | awk '{if (($$1 + 0) <= 0) {print 1} else { print int(($$1 + 0)*1.25)}}')
ifneq ($(strip ${j_arg}),)
    ifneq (${j_arg},1)
	EXTRA_KERNEL_MAKE_FLAGS?=-j${j_arg}
    endif
endif

#######
# NOTE: DIST_EL_EL6 is set in customer.inc via common.mk to "${PROD_CUSTOMER_ROOT}/dist/centos/6"
# The makefile ${DIST_EL_EL6}/dist.inc sets:
#   BASE_KERNEL_VERSION       e.g. 2.6.32
#   EXTRA_KERNEL_VERSION      e.g. 358.2.1.el6 or 504.16.2.SCL6_6
#   EXTRACT_KERNEL_DIRNAME    e.g. linux-2.6.32 or linux-2.6.32-504.16.2.SCLC6_6
#   EXTRACT_USE_RPM           e.g. 1  (Always 1)
#   KERNEL_IS_KERNEL_DOT_ORG  e.g. 0  (Always 0)
#######
include ${DIST_EL_EL6}/dist.inc

# Note: by default, we no longer build IMAGE_KERNEL_UNI , as
# IMAGE_KERNEL_SMP also works well with uni systems on x86.

# Set BUILD_* to express our intent about which kernels to build here
ifneq (${PROD_FEATURE_KERNEL_IMAGE_UNI},0)
	BUILD_IMAGE_KERNEL_UNI=1
else
	BUILD_IMAGE_KERNEL_UNI=0
endif
ifneq (${PROD_FEATURE_KERNEL_IMAGE_SMP},0)
	BUILD_IMAGE_KERNEL_SMP=1
else
	BUILD_IMAGE_KERNEL_SMP=0
endif
ifneq (${PROD_FEATURE_KERNEL_BOOTFLOPPY_UNI},0)
	BUILD_BOOTFLOPPY_KERNEL_UNI=1
else
	BUILD_BOOTFLOPPY_KERNEL_UNI=0
endif

ifeq (${PROD_FEATURE_DEV_ENV},1)
	BUILD_IMAGE_KERNEL_UNI=0
	BUILD_IMAGE_KERNEL_SMP=0
	BUILD_BOOTFLOPPY_KERNEL_UNI=0
endif

# Note that this feature has been deprecated but not yet removed
ifeq (${PROD_FEATURE_KERNELS_EXTERNAL},1)
	BUILD_IMAGE_KERNEL_UNI=0
	BUILD_IMAGE_KERNEL_SMP=0
	BUILD_BOOTFLOPPY_KERNEL_UNI=0
endif


# Graft point for customer-specific additions to build options for kernel.
# e.g. a customer may want to specify to use their config files
#
# Also, customers may append options to the kernel target build flags, by
# adding one or more of:
#
#     export KCFLAGS+=...
#     export KCPPFLAGS+=...
#     export KAFLAGS+=...
#
# These append to the C compiler, preprocessor for C and assembler, and
# assembler flags.

# For MFC, the following Makefile.inc sets KERNEL_CONFIG_ROOT
-include ${PROD_CUSTOMER_ROOT}/${PROD_PRODUCT_LC}/src/base_os/${PROD_TARGET_PLATFORM_LC}/${PROD_TARGET_PLATFORM_VERSION_LC}/kernel/Makefile.inc

ifneq (${KERNEL_IS_KERNEL_DOT_ORG},1)
	OLDCONFIG_TARGET=nonint_oldconfig
else
	OLDCONFIG_TARGET=oldconfig
endif

# HERE
EXTRA_KERNEL_VERSION_PREFIX?=-

EXTRAVERSION_IMAGE_KERNEL_UNI=${EXTRA_KERNEL_VERSION_PREFIX}${EXTRA_KERNEL_VERSION}${PROD_CUSTOMER}uni${PROD_TARGET_HWNAME_FILE_DASH}
EXTRAVERSION_IMAGE_KERNEL_SMP=${EXTRA_KERNEL_VERSION_PREFIX}${EXTRA_KERNEL_VERSION}${PROD_CUSTOMER}smp${PROD_TARGET_HWNAME_FILE_DASH}
EXTRAVERSION_BOOTFLOPPY_KERNEL_UNI=${EXTRA_KERNEL_VERSION_PREFIX}${EXTRA_KERNEL_VERSION}${PROD_CUSTOMER}bfuni${PROD_TARGET_HWNAME_FILE_DASH}

FULL_BUILD_DIR=${PROD_BUILD_OUTPUT_DIR}/image/kernel
BUILD_DIRS=

ifeq (${BUILD_IMAGE_KERNEL_UNI}-${BUILD_IMAGE_KERNEL_SMP},1-1)
    ifeq ($(strip ${KERNEL_UNI_SUFFIX}${KERNEL_SMP_SUFFIX}),)
        $(error Multiple system kernels are not supported in this configuration)
    else ifeq (${KERNEL_UNI_SUFFIX},${KERNEL_SMP_SUFFIX})
        $(error Multiple system kernels are not supported in this configuration)
    endif
endif

ifeq (${BUILD_IMAGE_KERNEL_UNI},1)
	BUILD_DIRS+=${PROD_BUILD_OUTPUT_DIR}/base_os/${PROD_TARGET_PLATFORM_LC}/${PROD_TARGET_PLATFORM_VERSION_LC}/kernel/image_kernel_uni
endif
ifeq (${BUILD_IMAGE_KERNEL_SMP},1)
	BUILD_DIRS+=${PROD_BUILD_OUTPUT_DIR}/base_os/${PROD_TARGET_PLATFORM_LC}/${PROD_TARGET_PLATFORM_VERSION_LC}/kernel/image_kernel_smp
endif
ifeq (${BUILD_BOOTFLOPPY_KERNEL_UNI},1)
	BUILD_DIRS+=${PROD_BUILD_OUTPUT_DIR}/base_os/${PROD_TARGET_PLATFORM_LC}/${PROD_TARGET_PLATFORM_VERSION_LC}/kernel/bootfloppy_kernel_uni
endif

ifneq (${KERNEL_CONF_USE_HWNAME},1)
	KERNEL_CONF_HWNAME_FILE_DASH=
	KERNEL_CONF_HWNAME_FILE_UNDER=
else
	KERNEL_CONF_HWNAME_FILE_DASH=${PROD_TARGET_HWNAME_FILE_DASH}
	KERNEL_CONF_HWNAME_FILE_UNDER=${PROD_TARGET_HWNAME_FILE_UNDER}
endif

KERNEL_CONFIG_ROOT?=${KERNEL_DIR}/arch_${PROD_TARGET_ARCH_LC}
IMAGE_KERNEL_UNI_CONFIG?=image-kernel-uni-${PROD_TARGET_ARCH_LC}${KERNEL_CONF_HWNAME_FILE_DASH}.conf
IMAGE_KERNEL_SMP_CONFIG?=image-kernel-smp-${PROD_TARGET_ARCH_LC}${KERNEL_CONF_HWNAME_FILE_DASH}.conf
BOOTFLOPPY_KERNEL_UNI_CONFIG?=bootfloppy-kernel-uni-${PROD_TARGET_ARCH_LC}${KERNEL_CONF_HWNAME_FILE_DASH}.conf

# Part of our automatic -j support, so that we do not pass our -j if the
# user has already passed -j on the command line.
ifneq (${EXTRA_KERNEL_MAKE_FLAGS},)
	EXTRA_KERNEL_MAKE_FLAGS_NO_J=$(filter-out -j%, ${EXTRA_KERNEL_MAKE_FLAGS})
endif
define set_extra_flags
	echo set_extra_flags ;\
	mf_has_j=0 ; \
        for f in x ${MAKEFLAGS}; do \
            case $$f in \
                *=* | --[!j]*);; \
                *j*) mf_has_j=1;; \
            esac; \
        done; \
	if [ $${mf_has_j} -eq 1 ]; then \
		extra_flags=${EXTRA_KERNEL_MAKE_FLAGS_NO_J} ; \
		echo SETTING extra_flags=${EXTRA_KERNEL_MAKE_FLAGS_NO_J} ; \
	else \
		extra_flags=${EXTRA_KERNEL_MAKE_FLAGS} ; \
		echo NOT setting extra_flags=${EXTRA_KERNEL_MAKE_FLAGS} ; \
		extra_flags=${EXTRA_KERNEL_MAKE_FLAGS_NO_J} ; \
		echo SETTING extra_flags=${EXTRA_KERNEL_MAKE_FLAGS_NO_J} ; \
	fi ; \
	echo The extra flags are: ; \
	echo ${extra_flags}
endef


# ============================== ALL ==============================

build_kernel_targets=
ifeq (${BUILD_IMAGE_KERNEL_UNI},1)
	build_kernel_targets+= build_image_kernel_uni
endif
ifeq (${BUILD_IMAGE_KERNEL_SMP},1)
	build_kernel_targets+= build_image_kernel_smp
endif
ifeq (${BUILD_BOOTFLOPPY_KERNEL_UNI},1)
	build_kernel_targets+= build_bootfloppy_kernel_uni
endif


ALL_KERNELS=
ALL_KERNEL_VERSIONS=
ALL_IMAGE_KERNELS=
ALL_IMAGE_KERNEL_VERSIONS=
ALL_BOOTFLOPPY_KERNELS=
ALL_BOOTFLOPPY_KERNEL_VERSIONS=
ifeq (${BUILD_IMAGE_KERNEL_UNI},1)
	ALL_IMAGE_KERNELS+=image_kernel_uni
	ALL_IMAGE_KERNEL_VERSIONS+=${BASE_KERNEL_VERSION}${EXTRAVERSION_IMAGE_KERNEL_UNI}
	ALL_KERNELS+=image_kernel_uni
	ALL_KERNEL_VERSIONS+=${BASE_KERNEL_VERSION}${EXTRAVERSION_IMAGE_KERNEL_UNI}
endif
ifeq (${BUILD_IMAGE_KERNEL_SMP},1)
	ALL_KERNELS+=image_kernel_smp
	ALL_IMAGE_KERNELS+=image_kernel_smp
	ALL_IMAGE_KERNEL_VERSIONS+=${BASE_KERNEL_VERSION}${EXTRAVERSION_IMAGE_KERNEL_SMP}
	ALL_KERNEL_VERSIONS+=${BASE_KERNEL_VERSION}${EXTRAVERSION_IMAGE_KERNEL_SMP}
endif
ifeq (${BUILD_BOOTFLOPPY_KERNEL_UNI},1)
	ALL_KERNELS+=bootfloppy_kernel_uni
	ALL_BOOTFLOPPY_KERNELS+=bootfloppy_kernel_uni
	ALL_BOOTFLOPPY_KERNEL_VERSIONS+=${BASE_KERNEL_VERSION}${EXTRAVERSION_BOOTFLOPPY_KERNEL_UNI}
	ALL_KERNEL_VERSIONS+=${BASE_KERNEL_VERSION}${EXTRAVERSION_BOOTFLOPPY_KERNEL_UNI}
endif

ifneq ("$(strip ${build_kernel_targets})","")
all: startup ensure_build_dirs version extract_sources ${build_kernel_targets} shutdown
else
all:
	@echo "Not building any kernels from base tree"
endif

version:
	@export ARCH="${ARCH_ORIG}" ; \
	export PATH="${PATH_ORIG}" ; \
	export DEPMOD="${DEPMOD_ORIG}" ; \
	${MAKE} -C ${PROD_SRC_ROOT}/release version

print_kernel_versions_active_all_image:
	@echo "${ALL_IMAGE_KERNEL_VERSIONS}"

print_kernel_versions_active_all_bootfloppy:
	@echo "${ALL_BOOTFLOPPY_KERNEL_VERSIONS}"

ifeq ($(strip ${EXTRACT_USE_RPM}),1)
extract_sources: version
	@export ARCH="${ARCH_ORIG}" ; \
	export PATH="${PATH_ORIG}" ; \
	export DEPMOD="${DEPMOD_ORIG}" ; \
	${MAKE} -C extract

EXTRACT_MAKEFILE=
endif

# Note that the kernel build process happens in a symbolic tree copy, not in
# the source tree.
#
# The process is: the kernel sources are extracted from the SRPM as
# specified by .../extract/Makefile (and associated customer Makefile.inc).
# A symlink tree is then constructed which fully mirrors the extracted
# sources.  Then the files in BASE_SRC_ROOT and FULL_CUSTOMER_SRC_ROOT are
# symlinked into the symlink tree, overriding any existing files with the
# same name.  Finally, patches are applied from BASE_PATCHES and
# CUSTOMER_PATCHES.  The sources are not modified further.
#
# The kernel is configured after symlinking in the specified kernel config
# file.
#
# The kernel and modules are built, and then kernel and modules are installed.
#

ifneq ($(strip ${EXTRACT_USE_RPM}),0)

	EXTRACT_SRC_ROOT=${PROD_BUILD_OUTPUT_DIR}/dist/kernel/source_output/kernel/BUILD/${EXTRACT_KERNEL_BUILD_SUBDIR}/${EXTRACT_KERNEL_DIRNAME}/

else

	EXTRACT_SRC_ROOT=${PROD_BUILD_OUTPUT_DIR}/dist/kernel/extract/${EXTRACT_KERNEL_DIRNAME}/

endif

BASE_SRC_ROOT?=${KERNEL_DIR}/${KERNEL_OVERLAY_DIR}
# NOTE: BASE_PATCHES_DIR and BASE_PATCHES is set in ${DIST_EL_EL6}/dist.inc
#BASE_PATCHES_DIR?=${KERNEL_DIR}/${KERNEL_PATCHES_DIR}/
#BASE_PATCHES?=$(addprefix ${BASE_PATCHES_DIR}, \
#		tms-build-2.6.32-el6.patch \
#		tms-phy-unused-statics.patch \
#	       )

PATCH_ARGS=-p2 -f
CUSTOMER_PATCH_ARGS?=${PATCH_ARGS}

FULL_CUSTOMER_SRC_ROOT?=${PROD_CUSTOMER_ROOT}/${PROD_PRODUCT_LC}/src/base_os/${PROD_TARGET_PLATFORM_LC}/${PROD_TARGET_PLATFORM_VERSION_LC}/kernel/${KERNEL_OVERLAY_DIR}
FULL_CUSTOMER_SRC_ROOT_PREFIX?=
CUSTOMER_PATCHES?=

SYMTREE_IMAGE_KERNEL_UNI=${PROD_BUILD_OUTPUT_DIR}/base_os/${PROD_TARGET_PLATFORM_LC}/${PROD_TARGET_PLATFORM_VERSION_LC}/kernel/image_kernel_uni
SYMTREE_IMAGE_KERNEL_SMP=${PROD_BUILD_OUTPUT_DIR}/base_os/${PROD_TARGET_PLATFORM_LC}/${PROD_TARGET_PLATFORM_VERSION_LC}/kernel/image_kernel_smp
SYMTREE_BOOTFLOPPY_KERNEL_UNI=${PROD_BUILD_OUTPUT_DIR}/base_os/${PROD_TARGET_PLATFORM_LC}/${PROD_TARGET_PLATFORM_VERSION_LC}/kernel/bootfloppy_kernel_uni

# ========== Image kernel uni

print_kernel_version_image_kernel_uni:
	@echo "${BASE_KERNEL_VERSION}${EXTRAVERSION_IMAGE_KERNEL_UNI}"

ifeq (${BUILD_IMAGE_KERNEL_UNI},1)
print_kernel_version_active_image_kernel_uni:
	@echo "${BASE_KERNEL_VERSION}${EXTRAVERSION_IMAGE_KERNEL_UNI}"
else
print_kernel_version_active_image_kernel_uni:
	@ :
endif

build_image_kernel_uni: config_image_kernel_uni
	@echo build_image_kernel_uni in tree/src/base_os/linux_el/el6/kernel/Makefile
	@ \
	pwd ; \
	export LC_ALL=C ; \
	$(set_extra_flags) ; \
	echo "Building image_kernel_uni" ; \
	( cd ${SYMTREE_IMAGE_KERNEL_UNI} ; \
	  EXTRAVERSION="${EXTRAVERSION_IMAGE_KERNEL_UNI}" ; \
	  export EXTRAVERSION ; \
	  echo "${MAKE} $${extra_flags} ${KERNEL_ALL_TARGETS}" ; \
	  ${MAKE} $${extra_flags} ${KERNEL_ALL_TARGETS} || exit 1; \
	  echo "${MAKE} $${extra_flags} ${KERNEL_ALL_EXTRA_TARGETS}" ; \
	  ${MAKE} $${extra_flags} ${KERNEL_ALL_EXTRA_TARGETS} || exit 1 ; \
	  echo "${MAKE} $${extra_flags} modules" ; \
	  ${MAKE} $${extra_flags} modules || exit 1)

menuconfig_image_kernel_uni: config_image_kernel_uni
	@echo menuconfig_image_kernel_uni in tree/src/base_os/linux_el/el6/kernel/Makefile
	@( cd ${SYMTREE_IMAGE_KERNEL_UNI} ; \
	  pwd ; \
	  export LC_ALL=C ; \
	  echo "" ; \
	  echo "NOTE: this will modify your source tree" ; \
	  echo "" ; \
	  echo "Doing 'make menuconfig' for image_kernel_uni" ; \
	  ${MAKE} menuconfig || exit 1 ; \
	  echo "Copying new config file back to source tree" ; \
          cp -p ${SYMTREE_IMAGE_KERNEL_UNI}/.config \
		${KERNEL_CONFIG_ROOT}/${IMAGE_KERNEL_UNI_CONFIG} \
		 || exit 1; \
	  echo "" ; \
	  echo "NOTE: modified source file ${KERNEL_CONFIG_ROOT}/${IMAGE_KERNEL_UNI_CONFIG}" ; \
	  echo "" ; \
	)

config_image_kernel_uni: extract_sources symtree_image_kernel_uni patch_image_kernel_uni
	@echo config_image_kernel_uni in tree/src/base_os/linux_el/el6/kernel/Makefile
	@( cd ${SYMTREE_IMAGE_KERNEL_UNI} ; \
	  pwd ; \
	  export LC_ALL=C ; \
	  echo "Configuring image_kernel_uni with ${IMAGE_KERNEL_UNI_CONFIG}" ; \
	  EXTRAVERSION="${EXTRAVERSION_IMAGE_KERNEL_UNI}" ; \
	  export EXTRAVERSION ; \
	  rm -f ${SYMTREE_IMAGE_KERNEL_UNI}/.config ; \
          cp -p ${KERNEL_CONFIG_ROOT}/${IMAGE_KERNEL_UNI_CONFIG} \
		${SYMTREE_IMAGE_KERNEL_UNI}/.config || exit 1; \
	  ${MAKE} ${OLDCONFIG_TARGET} < /dev/null > /dev/null || exit 1; \
	  ${MAKE} modules_prepare || exit 1; \
	)

symtree_image_kernel_uni: ${SYMTREE_IMAGE_KERNEL_UNI}/.done_links

${SYMTREE_IMAGE_KERNEL_UNI}/.done_links:
	@echo "Making symlink tree for image_kernel_uni ."
	rm -rf ${SYMTREE_IMAGE_KERNEL_UNI}
	mkdir -p ${SYMTREE_IMAGE_KERNEL_UNI}
	cp --symbolic-link --recursive ${EXTRACT_SRC_ROOT} \
		${SYMTREE_IMAGE_KERNEL_UNI} || exit 1
	mv ${SYMTREE_IMAGE_KERNEL_UNI}/* \
		${SYMTREE_IMAGE_KERNEL_UNI}/.cp_dirname || exit 1
	find ${SYMTREE_IMAGE_KERNEL_UNI}/.cp_dirname -mindepth 1 -maxdepth 1 -print0 | \
		xargs -0 -i -r mv {} ${SYMTREE_IMAGE_KERNEL_UNI} || exit 1
	rmdir ${SYMTREE_IMAGE_KERNEL_UNI}/.cp_dirname || exit 1
	@if [ ! -z "${BASE_SRC_ROOT}" -a -d "${BASE_SRC_ROOT}" ]; then \
		echo "Overlaying files for image_kernel_uni ." ; \
		${PROD_TREE_ROOT}/src/release/symtree.sh ${BASE_SRC_ROOT} \
			${SYMTREE_IMAGE_KERNEL_UNI} || exit 1 ; \
	fi
	@if [ ! -z "${FULL_CUSTOMER_SRC_ROOT}" -a -d "${FULL_CUSTOMER_SRC_ROOT}" ]; then \
		echo "Overlaying customer files for image_kernel_uni ." ; \
		mkdir -p ${SYMTREE_IMAGE_KERNEL_UNI}/${FULL_CUSTOMER_SRC_ROOT_PREFIX} ; \
		${PROD_TREE_ROOT}/src/release/symtree.sh ${FULL_CUSTOMER_SRC_ROOT} \
			${SYMTREE_IMAGE_KERNEL_UNI}/${FULL_CUSTOMER_SRC_ROOT_PREFIX} || exit 1 ; \
	fi
	@touch ${SYMTREE_IMAGE_KERNEL_UNI}/.done_links

patch_image_kernel_uni: ${SYMTREE_IMAGE_KERNEL_UNI}/.done_patches

${SYMTREE_IMAGE_KERNEL_UNI}/.done_patches:
	@echo RULE: ${SYMTREE_IMAGE_KERNEL_UNI}/.done_patches: ;
	@echo In: tree/src/base_os/linux_el/el6/kernel/Makefile
	@if [ ! -z "${BASE_PATCHES}" -o ! -z "${CUSTOMER_PATCHES}" ]; then \
		echo "Applying patches for image_kernel_uni ." ; \
	fi
	@ \
	 pwd ; \
	 export LC_ALL=C ; \
	 echo BASE_PATCHES=${BASE_PATCHES} ; \
	 for patch in ${BASE_PATCHES}; do \
		echo "Patching sources with `basename $${patch}`"; \
		echo "${PATCH} ${PATCH_ARGS} -N -s -d ${SYMTREE_IMAGE_KERNEL_UNI} -i $${patch}" ; \
		${PATCH} ${PATCH_ARGS} -N -s -d ${SYMTREE_IMAGE_KERNEL_UNI} -i $${patch} || exit 1; \
	done
	@ \
	 pwd ; \
	 export LC_ALL=C ; \
	 echo CUSTOMER_PATCHES=${CUSTOMER_PATCHES} ; \
	 for patch in ${CUSTOMER_PATCHES}; do \
		echo "Patching sources with `basename $${patch}`"; \
		echo "${PATCH} ${CUSTOMER_PATCH_ARGS} -N -s -d ${SYMTREE_IMAGE_KERNEL_UNI} -i $${patch}" ; \
		${PATCH} ${CUSTOMER_PATCH_ARGS} -N -s -d ${SYMTREE_IMAGE_KERNEL_UNI} -i $${patch} || exit 1; \
	done
	@touch ${SYMTREE_IMAGE_KERNEL_UNI}/.done_patches


# ========== Image kernel smp

print_kernel_version_image_kernel_smp:
	@echo "${BASE_KERNEL_VERSION}${EXTRAVERSION_IMAGE_KERNEL_SMP}"

ifeq (${BUILD_IMAGE_KERNEL_SMP},1)
print_kernel_version_active_image_kernel_smp:
	@echo "${BASE_KERNEL_VERSION}${EXTRAVERSION_IMAGE_KERNEL_SMP}"
else
print_kernel_version_active_image_kernel_smp:
	@ :
endif

build_image_kernel_smp: config_image_kernel_smp
	@echo RULE: build_image_kernel_smp in tree/src/base_os/linux_el/el6/kernel/Makefile
	@ \
	 pwd ; \
	 export LC_ALL=C ; \
	 $(set_extra_flags) ; \
	set ; \
	echo "Building image_kernel_smp" ; \
	( cd ${SYMTREE_IMAGE_KERNEL_SMP} ; \
	  echo Changed dir to: ; \
	  pwd ; \
	  export LC_ALL=C ; \
	  EXTRAVERSION="${EXTRAVERSION_IMAGE_KERNEL_SMP}" ; \
	  export EXTRAVERSION ; \
          echo "====STEP 1================" ; \
	  echo "${MAKE} $${extra_flags} ${KERNEL_ALL_TARGETS}" ; \
	  ${MAKE} $${extra_flags} ${KERNEL_ALL_TARGETS} || exit 1; \
          echo "====STEP 1 success ================" ; \
          echo "====STEP 2================" ; \
	  echo "${MAKE} $${extra_flags} ${KERNEL_ALL_EXTRA_TARGETS}" ; \
          echo "====STEP 2 success ================" ; \
          echo "====STEP 3================" ; \
	  ${MAKE} $${extra_flags} ${KERNEL_ALL_EXTRA_TARGETS} || exit 1 ; \
          echo "====STEP 3 success ================" ; \
          echo "====STEP 4================" ; \
	  echo "${MAKE} $${extra_flags} modules" ; \
	  ${MAKE} $${extra_flags} modules || exit 1 ; \
          echo "====STEP 4 success =================" ; \
          echo "====Finished build_image_kernel_smp ================" ; \
	)

menuconfig_image_kernel_smp: config_image_kernel_smp
	@echo RULE: menuconfig_image_kernel_smp in tree/src/base_os/linux_el/el6/kernel/Makefile
	@echo Configuring      image_kernel_smp with ${KERNEL_CONFIG_ROOT}/${IMAGE_KERNEL_SMP_CONFIG}
	@ls -l  ${KERNEL_CONFIG_ROOT}/${IMAGE_KERNEL_SMP_CONFIG}
	@wc     ${KERNEL_CONFIG_ROOT}/${IMAGE_KERNEL_SMP_CONFIG}
	@md5sum ${KERNEL_CONFIG_ROOT}/${IMAGE_KERNEL_SMP_CONFIG}
	@ \
	 pwd ; \
	 export LC_ALL=C ; \
	 ( cd ${SYMTREE_IMAGE_KERNEL_SMP} ; \
	  pwd ; \
	  export LC_ALL=C ; \
	  echo "" ; \
	  echo "NOTE: this will modify your source tree" ; \
	  echo "" ; \
	  echo "Doing 'make menuconfig' for image_kernel_smp" ; \
	  ${MAKE} menuconfig || exit 1 ; \
	  echo "Copying new config file back to source tree" ; \
          cp -p ${SYMTREE_IMAGE_KERNEL_SMP}/.config \
		${KERNEL_CONFIG_ROOT}/${IMAGE_KERNEL_SMP_CONFIG} \
		 || exit 1; \
	  echo "" ; \
	  echo "NOTE: modified source file ${KERNEL_CONFIG_ROOT}/${IMAGE_KERNEL_SMP_CONFIG}" ; \
	  echo "" ; \
	)

config_image_kernel_smp: extract_sources symtree_image_kernel_smp patch_image_kernel_smp
	@echo RULE: config_image_kernel_smp in tree/src/base_os/linux_el/el6/kernel/Makefile
	@echo Configuring  image_kernel_smp with ${KERNEL_CONFIG_ROOT}/${IMAGE_KERNEL_SMP_CONFIG}
	@ls -l  ${KERNEL_CONFIG_ROOT}/${IMAGE_KERNEL_SMP_CONFIG}
	@wc     ${KERNEL_CONFIG_ROOT}/${IMAGE_KERNEL_SMP_CONFIG}
	@md5sum ${KERNEL_CONFIG_ROOT}/${IMAGE_KERNEL_SMP_CONFIG}
	@ \
	 pwd ; \
	 export LC_ALL=C ; \
	 ( cd ${SYMTREE_IMAGE_KERNEL_SMP} ; \
	  pwd ; \
	  export LC_ALL=C ; \
	  EXTRAVERSION="${EXTRAVERSION_IMAGE_KERNEL_SMP}" ; \
	  export EXTRAVERSION ; \
	  rm -f ${SYMTREE_IMAGE_KERNEL_SMP}/.config ; \
          cp -p ${KERNEL_CONFIG_ROOT}/${IMAGE_KERNEL_SMP_CONFIG} \
		${SYMTREE_IMAGE_KERNEL_SMP}/.config || exit 1; \
	  ${MAKE} ${OLDCONFIG_TARGET} < /dev/null || exit 1; \
	  ${MAKE} modules_prepare || exit 1; \
	)

symtree_image_kernel_smp: ${SYMTREE_IMAGE_KERNEL_SMP}/.done_links

${SYMTREE_IMAGE_KERNEL_SMP}/.done_links:
	@echo "Making symlink tree for image_kernel_smp ."
	@echo EXTRACT_SRC_ROOT=${EXTRACT_SRC_ROOT}
	@echo BASE_KERNEL_VERSION=${BASE_KERNEL_VERSION}
	@echo EXTRACT_KERNEL_BUILD_SUBDIR=${EXTRACT_KERNEL_BUILD_SUBDIR}
	@echo EXTRACT_KERNEL_DIRNAME=${EXTRACT_KERNEL_DIRNAME}
	@if [ ! -d ${EXTRACT_SRC_ROOT} ] ; then \
	  echo Internal ERROR, EXTRACT_SRC_ROOT is not set properly ; \
	  echo No such dir as ${EXTRACT_SRC_ROOT} ; \
	  exit 1 ; \
	fi
	rm -rf ${SYMTREE_IMAGE_KERNEL_SMP}
	mkdir -p ${SYMTREE_IMAGE_KERNEL_SMP}
	cp --symbolic-link --recursive ${EXTRACT_SRC_ROOT} \
		${SYMTREE_IMAGE_KERNEL_SMP} || exit 1
	mv ${SYMTREE_IMAGE_KERNEL_SMP}/* \
		${SYMTREE_IMAGE_KERNEL_SMP}/.cp_dirname || exit 1
	find ${SYMTREE_IMAGE_KERNEL_SMP}/.cp_dirname -mindepth 1 -maxdepth 1 -print0 | \
		xargs -0 -i -r mv {} ${SYMTREE_IMAGE_KERNEL_SMP} || exit 1
	rmdir ${SYMTREE_IMAGE_KERNEL_SMP}/.cp_dirname || exit 1
	@if [ ! -z "${BASE_SRC_ROOT}" -a -d "${BASE_SRC_ROOT}" ]; then \
		echo "Overlaying files for image_kernel_smp ." ; \
		${PROD_TREE_ROOT}/src/release/symtree.sh ${BASE_SRC_ROOT} \
			${SYMTREE_IMAGE_KERNEL_SMP} || exit 1 ; \
	fi
	@if [ ! -z "${FULL_CUSTOMER_SRC_ROOT}" -a -d "${FULL_CUSTOMER_SRC_ROOT}" ]; then \
		echo "Overlaying customer files for image_kernel_smp ." ; \
		mkdir -p ${SYMTREE_IMAGE_KERNEL_SMP}/${FULL_CUSTOMER_SRC_ROOT_PREFIX} ; \
		${PROD_TREE_ROOT}/src/release/symtree.sh ${FULL_CUSTOMER_SRC_ROOT} \
			${SYMTREE_IMAGE_KERNEL_SMP}/${FULL_CUSTOMER_SRC_ROOT_PREFIX} || exit 1 ; \
	fi
	@echo Touching ${SYMTREE_IMAGE_KERNEL_SMP}/.done_links
	@touch ${SYMTREE_IMAGE_KERNEL_SMP}/.done_links

patch_image_kernel_smp: ${SYMTREE_IMAGE_KERNEL_SMP}/.done_patches

${SYMTREE_IMAGE_KERNEL_SMP}/.done_patches:
	@echo RULE: ${SYMTREE_IMAGE_KERNEL_SMP}/.done_patches: ;
	@echo In: tree/src/base_os/linux_el/el6/kernel/Makefile
	@if [ ! -z "${BASE_PATCHES}" -o ! -z "${CUSTOMER_PATCHES}" ]; then \
		echo "Applying patches for image_kernel_smp ." ; \
	fi
	@ \
	 pwd ; \
	 export LC_ALL=C ; \
	 echo BASE_PATCHES=${BASE_PATCHES} ; \
	 for patch in ${BASE_PATCHES}; do \
		echo "Patching sources with `basename $${patch}`"; \
		echo "${PATCH} ${PATCH_ARGS} -N -s -d ${SYMTREE_IMAGE_KERNEL_SMP} -i $${patch}" ; \
		${PATCH} ${PATCH_ARGS} -N -s -d ${SYMTREE_IMAGE_KERNEL_SMP} -i $${patch} || exit 1; \
	done
	@ \
	 pwd ; \
	 export LC_ALL=C ; \
	 echo CUSTOMER_PATCHES=${CUSTOMER_PATCHES} ; \
	 for patch in ${CUSTOMER_PATCHES}; do \
		echo "Patching sources with `basename $${patch}`"; \
		echo "${PATCH} ${CUSTOMER_PATCH_ARGS} -N -s -d ${SYMTREE_IMAGE_KERNEL_SMP} -i $${patch}" ; \
		${PATCH} ${CUSTOMER_PATCH_ARGS} -N -s -d ${SYMTREE_IMAGE_KERNEL_SMP} -i $${patch} || exit 1; \
	done
	@echo Touching ${SYMTREE_IMAGE_KERNEL_SMP}/.done_patches
	@touch ${SYMTREE_IMAGE_KERNEL_SMP}/.done_patches

# ========== Bootfloppy kernel uni

print_kernel_version_bootfloppy_kernel_uni:
	@echo "${BASE_KERNEL_VERSION}${EXTRAVERSION_BOOTFLOPPY_KERNEL_UNI}"

ifeq (${BUILD_BOOTFLOPPY_KERNEL_UNI},1)
print_kernel_version_active_bootfloppy_kernel_uni:
	@echo "${BASE_KERNEL_VERSION}${EXTRAVERSION_BOOTFLOPPY_KERNEL_UNI}"
else
print_kernel_version_active_bootfloppy_kernel_uni:
	@ :
endif

build_bootfloppy_kernel_uni: config_bootfloppy_kernel_uni
	@echo RULE: build_bootfloppy_kernel_uni in tree/src/base_os/linux_el/el6/kernel/Makefile
	@ \
	 pwd ; \
	 export LC_ALL=C ; \
	 $(set_extra_flags) ; \
	echo "Building bootfloppy_kernel_uni" ; \
	( cd ${SYMTREE_BOOTFLOPPY_KERNEL_UNI} ; \
	  pwd ; \
	  export LC_ALL=C ; \
	  EXTRAVERSION="${EXTRAVERSION_BOOTFLOPPY_KERNEL_UNI}" ; \
	  export EXTRAVERSION ; \
          echo "==STEP 1================build_bootfloppy_kernel_uni" ; \
	  echo "${MAKE} $${extra_flags} ${KERNEL_ALL_TARGETS}" ; \
	  ${MAKE} $${extra_flags} ${KERNEL_ALL_TARGETS} || exit 1; \
          echo "==STEP 2================build_bootfloppy_kernel_uni" ; \
	  echo "${MAKE} $${extra_flags} ${KERNEL_ALL_EXTRA_TARGETS}" ; \
	  ${MAKE} $${extra_flags} ${KERNEL_ALL_EXTRA_TARGETS} || exit 1 ; \
          echo "==STEP 3================build_bootfloppy_kernel_uni" ; \
	  echo "${MAKE} $${extra_flags} modules" ; \
	  ${MAKE} $${extra_flags} modules || exit 1 ; \
          echo "====Finished build_bootfloppy_kernel_uni ================" ; \
	)

menuconfig_bootfloppy_kernel_uni: config_bootfloppy_kernel_uni
	@echo RULE: menuconfig_bootfloppy_kernel_uni in tree/src/base_os/linux_el/el6/kernel/Makefile
	@echo Configuring      bootfloppy_kernel_uni with ${KERNEL_CONFIG_ROOT}/${BOOTFLOPPY_KERNEL_UNI_CONFIG}
	@ls -l  ${KERNEL_CONFIG_ROOT}/${BOOTFLOPPY_KERNEL_UNI_CONFIG}
	@wc     ${KERNEL_CONFIG_ROOT}/${BOOTFLOPPY_KERNEL_UNI_CONFIG}
	@md5sum ${KERNEL_CONFIG_ROOT}/${BOOTFLOPPY_KERNEL_UNI_CONFIG}
	@( cd ${SYMTREE_BOOTFLOPPY_KERNEL_UNI} ; \
	  pwd ; \
	  export LC_ALL=C ; \
	  echo "" ; \
	  echo "NOTE: this will modify your source tree" ; \
	  echo "" ; \
	  echo "Doing 'make menuconfig' for bootfloppy_kernel_uni" ; \
	  ${MAKE} menuconfig || exit 1 ; \
	  echo "Copying new config file back to source tree" ; \
          cp -p ${SYMTREE_BOOTFLOPPY_KERNEL_UNI}/.config \
		${KERNEL_CONFIG_ROOT}/${BOOTFLOPPY_KERNEL_UNI_CONFIG} \
		 || exit 1; \
	  echo "" ; \
	  echo "NOTE: modified source file ${KERNEL_CONFIG_ROOT}/${BOOTFLOPPY_KERNEL_UNI_CONFIG}" ; \
	  echo "" ; \
	)

config_bootfloppy_kernel_uni: extract_sources symtree_bootfloppy_kernel_uni patch_bootfloppy_kernel_uni
	@echo RULE: config_bootfloppy_kernel_uni in tree/src/base_os/linux_el/el6/kernel/Makefile
	@echo Configuring  bootfloppy_kernel_uni with ${KERNEL_CONFIG_ROOT}/${BOOTFLOPPY_KERNEL_UNI_CONFIG}
	@ls -l  ${KERNEL_CONFIG_ROOT}/${BOOTFLOPPY_KERNEL_UNI_CONFIG}
	@wc     ${KERNEL_CONFIG_ROOT}/${BOOTFLOPPY_KERNEL_UNI_CONFIG}
	@md5sum ${KERNEL_CONFIG_ROOT}/${BOOTFLOPPY_KERNEL_UNI_CONFIG}
	@( cd ${SYMTREE_BOOTFLOPPY_KERNEL_UNI} ; \
	  pwd ; \
	  export LC_ALL=C ; \
	  EXTRAVERSION="${EXTRAVERSION_BOOTFLOPPY_KERNEL_UNI}" ; \
	  export EXTRAVERSION ; \
	  rm -f ${SYMTREE_BOOTFLOPPY_KERNEL_UNI}/.config ; \
          cp -p ${KERNEL_CONFIG_ROOT}/${BOOTFLOPPY_KERNEL_UNI_CONFIG} \
		${SYMTREE_BOOTFLOPPY_KERNEL_UNI}/.config || exit 1; \
	  ${MAKE} ${OLDCONFIG_TARGET} < /dev/null > /dev/null || exit 1; \
	  ${MAKE} modules_prepare || exit 1; \
	)

symtree_bootfloppy_kernel_uni: ${SYMTREE_BOOTFLOPPY_KERNEL_UNI}/.done_links

${SYMTREE_BOOTFLOPPY_KERNEL_UNI}/.done_links:
	@echo RULE: ${SYMTREE_BOOTFLOPPY_KERNEL_UNI}/.done_links: ;
	@echo "Making symlink tree for bootfloppy_kernel_uni ."
	rm -rf ${SYMTREE_BOOTFLOPPY_KERNEL_UNI}
	mkdir -p ${SYMTREE_BOOTFLOPPY_KERNEL_UNI}
	cp --symbolic-link --recursive ${EXTRACT_SRC_ROOT} \
		${SYMTREE_BOOTFLOPPY_KERNEL_UNI} || exit 1
	mv ${SYMTREE_BOOTFLOPPY_KERNEL_UNI}/* \
		${SYMTREE_BOOTFLOPPY_KERNEL_UNI}/.cp_dirname || exit 1
	find ${SYMTREE_BOOTFLOPPY_KERNEL_UNI}/.cp_dirname -mindepth 1 -maxdepth 1 -print0 | \
		xargs -0 -i -r mv {} ${SYMTREE_BOOTFLOPPY_KERNEL_UNI} || exit 1
	rmdir ${SYMTREE_BOOTFLOPPY_KERNEL_UNI}/.cp_dirname || exit 1
	@if [ ! -z "${BASE_SRC_ROOT}" -a -d "${BASE_SRC_ROOT}" ]; then \
		echo "Overlaying files for bootfloppy_kernel_uni ." ; \
		${PROD_TREE_ROOT}/src/release/symtree.sh ${BASE_SRC_ROOT} \
			${SYMTREE_BOOTFLOPPY_KERNEL_UNI} || exit 1 ; \
	fi
	@if [ ! -z "${FULL_CUSTOMER_SRC_ROOT}" -a -d "${FULL_CUSTOMER_SRC_ROOT}" ]; then \
		echo "Overlaying customer files for bootfloppy_kernel_uni ." ; \
		mkdir -p ${SYMTREE_BOOTFLOPPY_KERNEL_UNI}/${FULL_CUSTOMER_SRC_ROOT_PREFIX} ; \
		${PROD_TREE_ROOT}/src/release/symtree.sh ${FULL_CUSTOMER_SRC_ROOT} \
			${SYMTREE_BOOTFLOPPY_KERNEL_UNI}/${FULL_CUSTOMER_SRC_ROOT_PREFIX} || exit 1 ; \
	fi
	@echo Touching ${SYMTREE_BOOTFLOPPY_KERNEL_UNI}/.done_links
	@touch ${SYMTREE_BOOTFLOPPY_KERNEL_UNI}/.done_links

patch_bootfloppy_kernel_uni: ${SYMTREE_BOOTFLOPPY_KERNEL_UNI}/.done_patches

${SYMTREE_BOOTFLOPPY_KERNEL_UNI}/.done_patches:
	@echo RULE: ${SYMTREE_BOOTFLOPPY_KERNEL_UNI}/.done_patches: ;
	@echo In: tree/src/base_os/linux_el/el6/kernel/Makefile
	@if [ ! -z "${BASE_PATCHES}" -o ! -z "${CUSTOMER_PATCHES}" ]; then \
		echo "Applying patches for bootfloppy_kernel_uni ." ; \
	fi
	@ \
	 pwd ; \
	 export LC_ALL=C ; \
	 echo BASE_PATCHES=${BASE_PATCHES} ; \
	 for patch in ${BASE_PATCHES}; do \
		echo "Patching sources with `basename $${patch}`"; \
		echo "${PATCH} ${PATCH_ARGS} -N -s -d ${SYMTREE_BOOTFLOPPY_KERNEL_UNI} -i $${patch}" ; \
		${PATCH} ${PATCH_ARGS} -N -s -d ${SYMTREE_BOOTFLOPPY_KERNEL_UNI} -i $${patch} || exit 1; \
	done
	@ \
	 pwd ; \
	 export LC_ALL=C ; \
	 echo CUSTOMER_PATCHES=${CUSTOMER_PATCHES} ; \
	 for patch in ${CUSTOMER_PATCHES}; do \
		echo "Patching sources with `basename $${patch}`"; \
		echo "${PATCH} ${CUSTOMER_PATCH_ARGS} -N -s -d ${SYMTREE_BOOTFLOPPY_KERNEL_UNI} -i $${patch}" ; \
		${PATCH} ${CUSTOMER_PATCH_ARGS} -N -s -d ${SYMTREE_BOOTFLOPPY_KERNEL_UNI} -i $${patch} || exit 1; \
	done
	@echo Touching ${SYMTREE_BOOTFLOPPY_KERNEL_UNI}/.done_patches
	@touch ${SYMTREE_BOOTFLOPPY_KERNEL_UNI}/.done_patches


# ==========

#  Could check various things there for validity, like ${PROD_SRC_ROOT}
startup:
	$(build_times-startup)

shutdown:
	$(build_times-shutdown)

ensure_build_dirs:
	@for subdir in ${BUILD_DIRS}; do \
		if test ! -d $${subdir}/; then \
			mkdir -p $${subdir}/; \
			if test ! -d $${subdir}/; then \
				${ECHO} "Unable to create $${subdir}."; \
				 exit 1; \
			 fi; \
			 ${ECHO} "Created directory $${subdir}"; \
	    	fi; \
	done


CLEANFILES+=\

ifeq (${BUILD_IMAGE_KERNEL_UNI},1)
	CLEANFILES+=${SYMTREE_IMAGE_KERNEL_UNI}
endif
ifeq (${BUILD_IMAGE_KERNEL_SMP},1)
	CLEANFILES+=${SYMTREE_IMAGE_KERNEL_SMP}
endif
ifeq (${BUILD_BOOTFLOPPY_KERNEL_UNI},1)
	CLEANFILES+=${SYMTREE_BOOTFLOPPY_KERNEL_UNI}
endif


# ============================== INSTALL ==============================

FULL_INSTALL_DIR=${PROD_INSTALL_OUTPUT_DIR}/image
INSTALL_DIRS= \
	${PROD_INSTALL_OUTPUT_DIR}/image \
	${PROD_INSTALL_OUTPUT_DIR}/image/boot \
	${PROD_INSTALL_OUTPUT_DIR}/image/lib/modules \
	${PROD_INSTALL_OUTPUT_DIR}/bootflop \
	${PROD_INSTALL_OUTPUT_DIR}/rootflop \
	${PROD_INSTALL_OUTPUT_DIR}/rootflop/lib/modules \

install_kernel_targets=
ifeq (${BUILD_IMAGE_KERNEL_UNI},1)
	install_kernel_targets+= install_image_kernel_uni
endif
ifeq (${BUILD_IMAGE_KERNEL_SMP},1)
	install_kernel_targets+= install_image_kernel_smp
endif
ifeq (${BUILD_BOOTFLOPPY_KERNEL_UNI},1)
	install_kernel_targets+= install_bootfloppy_kernel_uni
endif

# If we're on x86 and have only one kernel, make a symlink to it with
# the name of the _other_ kernel type (uni vs. smp).  This is done to
# avoid problems when upgrading from an older image with only uni to one
# with only smp (or vice-versa).  When both uni and smp kernels are
# present, no symlinks are needed, and writeimage should always be able
# to use the right one.  This process also makes symlinks to the other
# kernel output files, like the config file.  Note that we do not do
# this for PowerPC, as a FAT filesystem may be in use for /boot, and
# attempting to make symlinks on FAT will cause writeimage to fail.

# Customer may set this to '1' to never make the kernel file symlinks
DISABLE_IMAGE_KERNEL_SYMLINKS?=0

maybe_make_image_kernel_symlinks=
ifeq (${PROD_TARGET_ARCH_FAMILY}-${DISABLE_IMAGE_KERNEL_SYMLINKS},X86-0)
	ifneq (${BUILD_IMAGE_KERNEL_UNI},${BUILD_IMAGE_KERNEL_SMP})
		ifeq (${BUILD_IMAGE_KERNEL_SMP},1)
			maybe_make_image_kernel_symlinks=install_symlinks_image_kernel_smp
		endif
		ifeq (${BUILD_IMAGE_KERNEL_UNI},1)
			maybe_make_image_kernel_symlinks=install_symlinks_image_kernel_uni
		endif
	endif
endif

ifneq ("$(strip ${install_kernel_targets})","")
install: ensure_install_dirs ${install_kernel_targets} ${maybe_make_image_kernel_symlinks}
else
install:
endif


ifndef INSTALL_FAKE
DO_FILE_INSTALL=${INSTALL} -o 0 -g 0 -m 644
DO_BIN_INSTALL=${INSTALL} -o 0 -g 0 -m 755
else
DO_FILE_INSTALL=${INSTALL} -m 644
DO_BIN_INSTALL=${INSTALL} -m 755
endif

install_image_kernel_uni:
	@rm -f ${PROD_INSTALL_OUTPUT_DIR}/image/boot/System.map${KERNEL_UNI_SUFFIX}
	${DO_FILE_INSTALL} ${SYMTREE_IMAGE_KERNEL_UNI}/System.map \
		${PROD_INSTALL_OUTPUT_DIR}/image/boot/System.map${KERNEL_UNI_SUFFIX}
	@rm -f ${PROD_INSTALL_OUTPUT_DIR}/image/boot/config${KERNEL_UNI_SUFFIX}
	${DO_FILE_INSTALL} ${SYMTREE_IMAGE_KERNEL_UNI}/.config \
		${PROD_INSTALL_OUTPUT_DIR}/image/boot/config${KERNEL_UNI_SUFFIX}
	@rm -f ${PROD_INSTALL_OUTPUT_DIR}/image/boot/${KERNEL_IMAGE_OUTPUT_NAME}${KERNEL_UNI_SUFFIX}
	${DO_FILE_INSTALL} ${SYMTREE_IMAGE_KERNEL_UNI}/arch/${ARCH}/boot/${KERNEL_IMAGE_NAME} \
		${PROD_INSTALL_OUTPUT_DIR}/image/boot/${KERNEL_IMAGE_OUTPUT_NAME}${KERNEL_UNI_SUFFIX}
	if test ! -z "${KERNEL_FDT_NAME}"; then \
		rm -f ${PROD_INSTALL_OUTPUT_DIR}/image/boot/${KERNEL_FDT_OUTPUT_NAME}${KERNEL_UNI_SUFFIX} ; \
		${DO_FILE_INSTALL} \
			${SYMTREE_BOOTFLOPPY_KERNEL_UNI}/arch/${ARCH}/boot/${KERNEL_FDT_NAME} \
			${PROD_INSTALL_OUTPUT_DIR}/image/boot/${KERNEL_FDT_OUTPUT_NAME}${KERNEL_UNI_SUFFIX} ; \
	fi
	mkdir -m 755 -p ${PROD_INSTALL_OUTPUT_DIR}/image/lib/modules
	( cd ${SYMTREE_IMAGE_KERNEL_UNI} ; \
	  EXTRAVERSION="${EXTRAVERSION_IMAGE_KERNEL_UNI}" ; \
	  export EXTRAVERSION ; \
	  ${MAKE} INSTALL_MOD_PATH=${PROD_INSTALL_OUTPUT_DIR}/image modules_install || exit 1)

# We have a "uni" kernel only, so make "smp" symlinks to it
install_symlinks_image_kernel_uni:
	if [ ! -z "${KERNEL_UNI_SUFFIX}" -a ! -z "${KERNEL_SMP_SUFFIX}" -a \( ! -e "${KERNEL_IMAGE_OUTPUT_NAME}${KERNEL_SMP_SUFFIX}" -o -L "${KERNEL_IMAGE_OUTPUT_NAME}${KERNEL_SMP_SUFFIX}" \) ]; then \
		( cd ${PROD_INSTALL_OUTPUT_DIR}/image/boot ; \
		rm -f "${KERNEL_IMAGE_OUTPUT_NAME}${KERNEL_SMP_SUFFIX}" ; \
		ln -sf ${KERNEL_IMAGE_OUTPUT_NAME}${KERNEL_UNI_SUFFIX} ${KERNEL_IMAGE_OUTPUT_NAME}${KERNEL_SMP_SUFFIX} || exit $? ; \
		rm -f System.map${KERNEL_SMP_SUFFIX} ; \
		ln -sf System.map${KERNEL_UNI_SUFFIX} System.map${KERNEL_SMP_SUFFIX} || exit $? ; \
		rm -f config${KERNEL_SMP_SUFFIX} ; \
		ln -sf config${KERNEL_UNI_SUFFIX} config${KERNEL_SMP_SUFFIX} || exit $? ; \
		if test ! -z "${KERNEL_FDT_NAME}"; then \
			rm -f ${KERNEL_FDT_OUTPUT_NAME}${KERNEL_SMP_SUFFIX} ; \
			ln -sf ${KERNEL_FDT_OUTPUT_NAME}${KERNEL_UNI_SUFFIX} ${KERNEL_FDT_OUTPUT_NAME}${KERNEL_SMP_SUFFIX} || exit $? ; \
		fi) || exit $?; \
	fi

install_image_kernel_smp:
	@echo RULE: install_image_kernel_smp in tree/src/base_os/linux_el/el6/kernel/Makefile
	@rm -f ${PROD_INSTALL_OUTPUT_DIR}/image/boot/System.map${KERNEL_SMP_SUFFIX}
	echo 1==; set -x ;\
	  export LC_ALL=C ; \
	${DO_FILE_INSTALL} ${SYMTREE_IMAGE_KERNEL_SMP}/System.map \
		${PROD_INSTALL_OUTPUT_DIR}/image/boot/System.map${KERNEL_SMP_SUFFIX}
	@rm -f ${PROD_INSTALL_OUTPUT_DIR}/image/boot/config${KERNEL_SMP_SUFFIX}
	echo 2==; set -x ;\
	  export LC_ALL=C ; \
	${DO_FILE_INSTALL} ${SYMTREE_IMAGE_KERNEL_SMP}/.config \
		${PROD_INSTALL_OUTPUT_DIR}/image/boot/config${KERNEL_SMP_SUFFIX}
	@rm -f ${PROD_INSTALL_OUTPUT_DIR}/image/boot/${KERNEL_IMAGE_OUTPUT_NAME}${KERNEL_SMP_SUFFIX}
	echo 3==; set -x ;\
	  export LC_ALL=C ; \
	${DO_FILE_INSTALL} ${SYMTREE_IMAGE_KERNEL_SMP}/arch/${ARCH}/boot/${KERNEL_IMAGE_NAME} \
		${PROD_INSTALL_OUTPUT_DIR}/image/boot/${KERNEL_IMAGE_OUTPUT_NAME}${KERNEL_SMP_SUFFIX}
	if test ! -z "${KERNEL_FDT_NAME}"; then \
		rm -f ${PROD_INSTALL_OUTPUT_DIR}/image/boot/${KERNEL_FDT_OUTPUT_NAME}${KERNEL_SMP_SUFFIX} ; \
		${DO_FILE_INSTALL} \
			${SYMTREE_BOOTFLOPPY_KERNEL_UNI}/arch/${ARCH}/boot/${KERNEL_FDT_NAME} \
			${PROD_INSTALL_OUTPUT_DIR}/image/boot/${KERNEL_FDT_OUTPUT_NAME}${KERNEL_SMP_SUFFIX} ; \
	fi
	mkdir -m 755 -p ${PROD_INSTALL_OUTPUT_DIR}/image/lib/modules
	( cd ${SYMTREE_IMAGE_KERNEL_SMP} ; \
	  echo 4==; set -x ;\
	  export LC_ALL=C ; \
	  EXTRAVERSION="${EXTRAVERSION_IMAGE_KERNEL_SMP}" ; \
	  export EXTRAVERSION ; \
	  ${MAKE} INSTALL_MOD_PATH=${PROD_INSTALL_OUTPUT_DIR}/image modules_install || exit 1)

# We have an "smp" kernel only, so make "uni" symlinks to it
install_symlinks_image_kernel_smp:
	@echo RULE: install_symlinks_image_kernel_smp in tree/src/base_os/linux_el/el6/kernel/Makefile
	set -x ; \
	  export LC_ALL=C ; \
	if [ ! -z "${KERNEL_SMP_SUFFIX}" -a ! -z "${KERNEL_UNI_SUFFIX}" -a \( ! -e "${KERNEL_IMAGE_OUTPUT_NAME}${KERNEL_UNI_SUFFIX}" -o -L "${KERNEL_IMAGE_OUTPUT_NAME}${KERNEL_UNI_SUFFIX}" \) ]; then \
		( cd ${PROD_INSTALL_OUTPUT_DIR}/image/boot ; \
		rm -f "${KERNEL_IMAGE_OUTPUT_NAME}${KERNEL_UNI_SUFFIX}" ; \
		ln -sf ${KERNEL_IMAGE_OUTPUT_NAME}${KERNEL_SMP_SUFFIX} ${KERNEL_IMAGE_OUTPUT_NAME}${KERNEL_UNI_SUFFIX} || exit $? ; \
		rm -f System.map${KERNEL_UNI_SUFFIX} ; \
		ln -sf System.map${KERNEL_SMP_SUFFIX} System.map${KERNEL_UNI_SUFFIX} || exit $? ; \
		rm -f config${KERNEL_UNI_SUFFIX} ; \
		ln -sf config${KERNEL_SMP_SUFFIX} config${KERNEL_UNI_SUFFIX} || exit $? ; \
		if test ! -z "${KERNEL_FDT_NAME}"; then \
			rm -f ${KERNEL_FDT_OUTPUT_NAME}${KERNEL_UNI_SUFFIX} ; \
			ln -sf ${KERNEL_FDT_OUTPUT_NAME}${KERNEL_SMP_SUFFIX} ${KERNEL_FDT_OUTPUT_NAME}${KERNEL_UNI_SUFFIX} || exit $? ; \
		fi) || exit $?; \
	fi

install_bootfloppy_kernel_uni:
	@echo RULE: install_bootfloppy_kernel_uni in tree/src/base_os/linux_el/el6/kernel/Makefile
	set -x ; \
	  export LC_ALL=C ; \
	${DO_FILE_INSTALL} \
		${SYMTREE_BOOTFLOPPY_KERNEL_UNI}/arch/${ARCH}/boot/${KERNEL_IMAGE_NAME} \
		${PROD_INSTALL_OUTPUT_DIR}/bootflop/${KERNEL_IMAGE_OUTPUT_NAME}
	if test ! -z "${KERNEL_FDT_NAME}"; then \
		${DO_FILE_INSTALL} \
			${SYMTREE_BOOTFLOPPY_KERNEL_UNI}/arch/${ARCH}/boot/${KERNEL_FDT_NAME} \
			${PROD_INSTALL_OUTPUT_DIR}/bootflop/${KERNEL_FDT_OUTPUT_NAME} ; \
	fi
	mkdir -m 755 -p ${PROD_INSTALL_OUTPUT_DIR}/rootflop/lib/modules
	( cd ${SYMTREE_BOOTFLOPPY_KERNEL_UNI} ; \
	  EXTRAVERSION="${EXTRAVERSION_BOOTFLOPPY_KERNEL_UNI}" ; \
	  export EXTRAVERSION ; \
	  ${MAKE} INSTALL_MOD_PATH=${PROD_INSTALL_OUTPUT_DIR}/rootflop modules_install || exit 1)

ensure_install_dirs:
	@echo RULE: ensure_install_dirs in tree/src/base_os/linux_el/el6/kernel/Makefile
	@for subdir in ${INSTALL_DIRS}; do \
		if test ! -d $${subdir}/; then \
			mkdir -p $${subdir}/; \
			if test ! -d $${subdir}/; then \
				${ECHO} "Unable to create $${subdir}."; \
				 exit 1; \
			 fi; \
			 ${ECHO} "Created directory $${subdir}"; \
	    	fi; \
	done

# ============================== CLEAN ==============================

clean:
	${RM} -rf ${CLEANFILES}
	${MAKE} -C extract ${EXTRACT_MAKEFILE} clean

# ============================== DEPEND ==============================
depend:

# ============================== REALCLEAN ==============================
realclean: clean
	${MAKE} -C extract ${EXTRACT_MAKEFILE} realclean

