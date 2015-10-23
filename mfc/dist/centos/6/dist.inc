# Included from ${PROD_DIST_ROOT}/tree/src/base_os/linux_el/el6/kernel/extract/Makefile
# Included from ${PROD_DIST_ROOT}/tree/src/base_os/linux_el/el6/kernel/Makefile
#
# Note that DIST_EL_EL6 is set in customer.mk to this directory, like this:
#     DIST_EL_EL6?=${PROD_DIST_ROOT}/centos/6

#USE_KERNEL_REV=2.6.32_358.2.1.el6
USE_KERNEL_REV=2.6.32_504.16.2.SCL6_6
ifeq (${USE_KERNEL_REV},2.6.32_358.2.1.el6)
  BASE_KERNEL_VERSION=2.6.32
  EXTRA_KERNEL_VERSION=358.2.1.el6
  EXTRACT_KERNEL_BUILD_SUBDIR=kernel-2.6.32
  EXTRACT_KERNEL_DIRNAME=linux-2.6.32
  EXTRACT_USE_RPM=1
  KERNEL_IS_KERNEL_DOT_ORG=0
  DIST_VER=el6.4
  DIST_EL_EL6_I386?=${DIST_EL_EL6}/${DIST_VER}/os/i386/Packages
  DIST_EL_EL6_X86_64?=${DIST_EL_EL6}/${DIST_VER}/os/x86_64/Packages
  DIST_EL_EL6_SRPMS?=${DIST_EL_EL6}/${DIST_VER}/os/Source/SPackages
  DIST_EL_EL6_UPDATES?=${DIST_EL_EL6}/${DIST_VER}/updates
  DIST_EL_EL6_UPDATES_I386?=${DIST_EL_EL6_UPDATES}/i386/RPMS
  DIST_EL_EL6_UPDATES_X86_64?=${DIST_EL_EL6_UPDATES}/x86_64/RPMS
  DIST_EL_EL6_UPDATES_SRPMS?=${DIST_EL_EL6_UPDATES}//Source/SPackages
  DIST_KERNEL_SOURCE_DIR=${DIST_EL_EL6_UPDATES_SRPMS}
  CHECK_SOURCE_RPM_FILE=${DIST_KERNEL_SOURCE_DIR}/pkg_src_check.txt
  PACKAGES_SOURCE_RPM=${DIST_KERNEL_SOURCE_DIR}/kernel-2.6.32-358.2.1.el6.src.rpm
  PACKAGES_SOURCE_RPM_DIR=kernel
  PACKAGES_SOURCE_RPM_SPEC_FILE=kernel.spec
  PACKAGES_SOURCE_RPM_BUILD_ARGS=--target x86_64
  PACKAGES_SOURCE_RPM_PRE_PREP_PATCHES=${DIST_EL_EL6}/${DIST_VER}/extract/kernel-2.6.32_el6-tms-preprep1.patch
  PACKAGES_SOURCE_RPM_POST_PREP_RUN=${DIST_EL_EL6}/${DIST_VER}/extract/postprep.sh
  PACKAGES_SOURCE_RPM_POST_PREP_RUN_ARGS=2.6.32
  PACKAGES_SOURCE_RPM_PATCHES=
  BASE_PATCHES_DIR?=${DIST_EL_EL6}/${DIST_VER}/patches
  BASE_PATCHES?=$(addprefix ${BASE_PATCHES_DIR}/, \
                tms-build-2.6.32-el6.patch \
                tms-phy-unused-statics.patch \
               )
else
  BASE_KERNEL_VERSION=2.6.32
  EXTRA_KERNEL_VERSION=504.16.2.SCL6_6
  EXTRACT_KERNEL_BUILD_SUBDIR=kernel-2.6.32-504.16.2.el6
  EXTRACT_KERNEL_DIRNAME=linux-2.6.32-504.16.2.SCLC6_6.x86_64
  # The above are used like this to specify the extract dir in kernel/Makefile:
  # EXTRACT_SRC_ROOT=${PROD_BUILD_OUTPUT_DIR}/dist/kernel/source_output/kernel/BUILD/${EXTRACT_KERNEL_BUILD_SUBDIR}/${EXTRACT_KERNEL_DIRNAME}/
  EXTRACT_USE_RPM=1
  KERNEL_IS_KERNEL_DOT_ORG=0
  DIST_VER=scl6.6
  DIST_EL_EL6_I386?=${DIST_EL_EL6}/${DIST_VER}/os/i386/Packages
  DIST_EL_EL6_X86_64?=${DIST_EL_EL6}/${DIST_VER}/os/x86_64/Packages
  DIST_EL_EL6_SRPMS?=${DIST_EL_EL6}/${DIST_VER}/os/Source/SPackages
  DIST_EL_EL6_UPDATES?=${DIST_EL_EL6}/${DIST_VER}/updates
  DIST_EL_EL6_UPDATES_I386?=${DIST_EL_EL6_UPDATES}/i386/RPMS
  DIST_EL_EL6_UPDATES_X86_64?=${DIST_EL_EL6_UPDATES}/x86_64/RPMS
  DIST_EL_EL6_UPDATES_SRPMS?=${DIST_EL_EL6_UPDATES}/Source/SPackages
  DIST_KERNEL_SOURCE_DIR=${DIST_EL_EL6_UPDATES_SRPMS}
  CHECK_SOURCE_RPM_FILE=${DIST_KERNEL_SOURCE_DIR}/pkg_src_check.txt
  PACKAGES_SOURCE_RPM=${DIST_KERNEL_SOURCE_DIR}/kernel-2.6.32-504.16.2.SCLC6_6.R1.1.1.src.rpm
  PACKAGES_SOURCE_RPM_DIR=kernel
  PACKAGES_SOURCE_RPM_SPEC_FILE=kernel.spec
  PACKAGES_SOURCE_RPM_BUILD_ARGS=--target x86_64
  PACKAGES_SOURCE_RPM_PRE_PREP_PATCHES=${DIST_EL_EL6}/${DIST_VER}/extract/kernel-2.6.32_el6-tms-preprep1.patch
  # Post prep script and args.
  PACKAGES_SOURCE_RPM_POST_PREP_RUN=${DIST_EL_EL6}/${DIST_VER}/extract/postprep.sh
  # The following first arg needs to match the "kversion" setting in the kernel spec file.
  # The following second arg needs to match the kernel version + "distro_build" + "dist" settings in the kernel spec file.
  # This is because the spec file extracts the source in dirs by these names.
  PACKAGES_SOURCE_RPM_POST_PREP_RUN_ARG1=2.6.32-504.16.2.el6
  PACKAGES_SOURCE_RPM_POST_PREP_RUN_ARG2=2.6.32-504.16.2.SCLC6_6"

  PACKAGES_SOURCE_RPM_PATCHES=
  BASE_PATCHES_DIR?=${DIST_EL_EL6}/${DIST_VER}/patches
  BASE_PATCHES?=$(addprefix ${BASE_PATCHES_DIR}/, \
                samara-build-2.6.32-el6.patch \
                samara-phy-unused-statics.patch \
                samara-fix-igb.patch \
               )
endif

