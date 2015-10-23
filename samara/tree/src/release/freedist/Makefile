#  Makefile
#
# Based on:
# Updated by Juniper.
# We needed significant changes to be able to produce a proper source release.
#
#

PROD_BUILD_TIMES=1
include ${PROD_TREE_ROOT}/src/mk/common.mk

# This makefile creates a set of tarballs from the source tree, as well as
# copies SRPMS.
# It then makes a final tarball.
# The goal is to include all software licensed in ways that require
# distribution, as the GPL does.
#
# Note that the actual SRPMs are NOT included in the base Samara distribution.
# Instead the SRPMs are on cmbu-storage01.juniper.net under the
#    /data/archive/tallmaple/DIST_SRCS/
# directory, in a subdirectory per major release.
# You will need to mirror the needed subdirs or NFS mount /data before
# using this Makefile.
#
# Customers should add in any GPL licensed components, and any other
# components where the license requires source distribution, using
# the graft points provided below.  This is true for both binary RPMs
# and imported sources.  Then the archive image will contain the added
# sources / SRPMs as well.
#


# On build (all): the tarballs are made into freedist/build
#
# On install: the contents of freedist/build and the SRPMS are copied into 
#      freedist/install, along with any support doc files
#
# On release: a tarball of files under freedist/install is made.

# The target "distiso" creates a iso of the files under freedist/install
# for burning on a data CD. (Not normally done anymore)

# Graft point for customer-specific files to distribute.
# The customer could set the following:
#
#  # Note that all of these must be absolute paths:
#
#  FREEDIST_CUSTOMER_UNMODIFIED_SRPMS_COMMON
#  FREEDIST_CUSTOMER_UNMODIFIED_SRPMS_LINUX_COMMON
#  FREEDIST_CUSTOMER_UNMODIFIED_SRPMS_LINUX_EL_COMMON
#  FREEDIST_CUSTOMER_UNMODIFIED_SRPMS_LINUX_EL_EL5
#  FREEDIST_CUSTOMER_UNMODIFIED_SRPMS_LINUX_EL_EL5_I386
#
#
#  # The path that all the FREEDIST_CUSTOMER_MODIFIED_SOURCES_* paths are
#  # relative to is PROD_CUSTOMER_SRC_ROOT.
#
#  PROD_CUSTOMER_SRC_ROOT
#  FREEDIST_CUSTOMER_MODIFIED_SOURCES_COMMON
#  FREEDIST_CUSTOMER_MODIFIED_SOURCES_LINUX_COMMON
#  FREEDIST_CUSTOMER_MODIFIED_SOURCES_LINUX_EL_COMMON
#  FREEDIST_CUSTOMER_MODIFIED_SOURCES_LINUX_EL_EL5
#  FREEDIST_CUSTOMER_MODIFIED_SOURCES_LINUX_EL_EL5_PPC
#
-include ${PROD_CUSTOMER_ROOT}/${PROD_PRODUCT_LC}/src/release/freedist/Makefile.inc

PROD_CUSTOMER_SRC_ROOT?=${PROD_CUSTOMER_ROOT}/${PROD_PRODUCT_LC}/src

FREEDIST_OUTPUT_DIR=${PROD_OUTPUT_DIR}/freedist
FULL_BUILD_DIR=${FREEDIST_OUTPUT_DIR}/build
FULL_INSTALL_DIR=${FREEDIST_OUTPUT_DIR}/install
FULL_RELEASE_DIR=${FREEDIST_OUTPUT_DIR}/release


############### Unmodified SRPMS (source RPMS)
#
#
# XXX the SRPMs should probably be checked into the source tree, but they
#     are just so big.
#
# Note that /data is NFS mounted from the host cmbu-storage01.juniper.net

FREEDIST_BASE_UNMODIFIED_SRPMS_PATH?=/data/archive/tallmaple/DIST_SRCS/samara-3.3.2

FREEDIST_BASE_UNMODIFIED_SRPMS_OTHER=\

FREEDIST_BASE_UNMODIFIED_SRPMS_COMMON=\

ifneq (${PROD_FEATURE_UBOOT},0)
	FREEDIST_BASE_UNMODIFIED_SRPMS_COMMON+=\
		${PROD_DIST_ROOT}/eldk/u-boot/u-boot-2009.01.tar.bz2 \
		${PROD_DIST_ROOT}/eldk/u-boot/u-boot-2010.06.tar.bz2 \
		${PROD_DIST_ROOT}/freescale/u-boot-2009.11-5.tbz \

endif

### FREEDIST_BASE_UNMODIFIED_SRPMS_LINUX_COMMON=\
### 	${PROD_DIST_ROOT}/centos/5/os/SRPMS/grub-0.97-13.5.src.rpm \
### 	${PROD_DIST_ROOT}/centos/5/os/SRPMS/ntp-4.2.4p2-1.fc6.src.rpm \
###         ${PROD_DIST_ROOT}/centos/5/updates/SRPMS/kernel-2.6.18-348.3.1.el5.src.rpm \
###         ${PROD_DIST_ROOT}/centos/6/updates/Source/SPackages/kernel-2.6.32-358.2.1.el6.src.rpm \
###         ${PROD_DIST_ROOT}/centos/6/os/Source/SPackages/grub-0.97-77.el6.src.rpm \
### 

FREEDIST_BASE_UNMODIFIED_SRPMS_LINUX_COMMON=\
        ${PROD_DIST_ROOT}/centos/6/scl6.6/updates/Source/SPackages/kernel-2.6.32-504.16.2.SCLC6_6.R1.1.1.src.rpm \


# XXX Not an SRPM right now
FREEDIST_BASE_UNMODIFIED_SRPMS_LINUX_EL_EL5_PPC=\
	${PROD_DIST_ROOT}/kernel-org/linux-2.6.27.tar.bz2 \
	${PROD_DIST_ROOT}/kernel-org/linux-2.6.34.tar.bz2 \
	${PROD_DIST_ROOT}/eldk/linux-denx/linux-denx-v2.6.32.60-0-g3d92466.tbz \
	${PROD_DIST_ROOT}/freescale/kernel-2.6.32.13-4.src.rpm \


-include ${PROD_TREE_ROOT}/src/base_os/${PROD_TARGET_PLATFORM_LC}/${PROD_TARGET_PLATFORM_VERSION_LC}/arch_${PROD_TARGET_ARCH_LC}/packages/source_pkgs.inc

FREEDIST_BASE_UNMODIFIED_SRPMS=\
	${FREEDIST_BASE_UNMODIFIED_SRPMS_OTHER} \
	${FREEDIST_BASE_UNMODIFIED_SRPMS_COMMON} \
	${FREEDIST_BASE_UNMODIFIED_SRPMS_${PROD_TARGET_OS}_COMMON} \
	${FREEDIST_BASE_UNMODIFIED_SRPMS_${PROD_TARGET_PLATFORM}_COMMON} \
	${FREEDIST_BASE_UNMODIFIED_SRPMS_${PROD_TARGET_PLATFORM_FULL}} \
	${FREEDIST_BASE_UNMODIFIED_SRPMS_${PROD_TARGET_PLATFORM_FULL}_${PROD_TARGET_ARCH}} \

FREEDIST_CUSTOMER_UNMODIFIED_SRPMS=\
	${FREEDIST_CUSTOMER_UNMODIFIED_SRPMS_OTHER} \
	${FREEDIST_CUSTOMER_UNMODIFIED_SRPMS_COMMON} \
	${FREEDIST_CUSTOMER_UNMODIFIED_SRPMS_${PROD_TARGET_OS}_COMMON} \
	${FREEDIST_CUSTOMER_UNMODIFIED_SRPMS_${PROD_TARGET_PLATFORM}_COMMON} \
	${FREEDIST_CUSTOMER_UNMODIFIED_SRPMS_${PROD_TARGET_PLATFORM_FULL}} \
	${FREEDIST_CUSTOMER_UNMODIFIED_SRPMS_${PROD_TARGET_PLATFORM_FULL}_${PROD_TARGET_ARCH}} \

FREEDIST_UNMODIFIED_SRPMS=\
	${FREEDIST_BASE_UNMODIFIED_SRPMS} \
	${FREEDIST_CUSTOMER_UNMODIFIED_SRPMS} 


############### Modified sources

FREEDIST_BASE_MODIFIED_SOURCES_COMMON=\
    busybox-1.20.2.tgz        ${PROD_SRC_ROOT}/base_os/common/busybox \
    ssmtp-2.60.4.tgz          ${PROD_SRC_ROOT}/base_os/common/ssmtp \
    parted-1.7.1.tgz          ${PROD_SRC_ROOT}/base_os/common/parted \

ifneq (${PROD_FEATURE_UBOOT},0)
	FREEDIST_BASE_MODIFIED_SOURCES_COMMON+= \
		u-boot.tgz    ${PROD_SRC_ROOT}/base_os/common/u-boot \
		u-boot-nvs-c.tgz ${PROD_SRC_ROOT}/lib/libnvs/nvs.c \
		u-boot-nvs-h.tgz ${PROD_SRC_ROOT}/lib/libnvs/nvs.h \

endif

FREEDIST_BASE_MODIFIED_SOURCES_LINUX_COMMON=\
    vixie_cron-3.0.1.tgz      ${PROD_SRC_ROOT}/base_os/linux_common/cron \
    pam_radius-1.3.16.tgz     ${PROD_SRC_ROOT}/base_os/linux_common/pam/pam_radius \
    pam_tacplus-1.2.9.tgz     ${PROD_SRC_ROOT}/base_os/linux_common/pam/pam_tacplus \
    pam_ldap-nss_ldap-253-22.el5_4.tgz ${PROD_SRC_ROOT}/base_os/linux_common/pam/pam_ldap \
    sysklogd-1.4.1_12.tgz     ${PROD_SRC_ROOT}/base_os/linux_common/syslogd \
    zeroconf-0.9.tgz          ${PROD_SRC_ROOT}/base_os/linux_common/zeroconf \
    grub-0.97.tgz             ${PROD_SRC_ROOT}/base_os/linux_common/grub \

ifneq (${PROD_FEATURE_CLUSTER},0)
	FREEDIST_BASE_MODIFIED_SOURCES_LINUX_COMMON+= \
		howl-0.9.6.tgz    ${PROD_SRC_ROOT}/base_os/linux_common/howl \

endif

### FREEDIST_BASE_MODIFIED_SOURCES_LINUX_EL_EL5 =\
###     pam-0_99_6_2_6_el5_5_2.tgz ${PROD_SRC_ROOT}/base_os/linux_el/el5/pam/pam_unix \
###     ntp-4.2.4p2.tgz           ${PROD_SRC_ROOT}/base_os/linux_el/el5/ntp \
###     kernel_linux_redhat-2.6.tgz ${PROD_SRC_ROOT}/base_os/linux_el/el5/kernel/patches \
### 
FREEDIST_BASE_MODIFIED_SOURCES_LINUX_EL_EL5 =

### FREEDIST_BASE_MODIFIED_SOURCES_LINUX_EL_EL6 =\
###     pam-0_99_6_2_6_el5_5_2.tgz ${PROD_SRC_ROOT}/base_os/linux_el/el5/pam/pam_unix \
###     kernel_linux_redhat-2.6.tgz ${PROD_SRC_ROOT}/base_os/linux_el/el6/kernel/patches \
###     glib2-2.22.5-7.el6.tgz     ${PROD_SRC_ROOT}/base_os/linux_el/el6/glib2/patches \

FREEDIST_BASE_MODIFIED_SOURCES_LINUX_EL_EL6 =\
    pam-0_99_6_2_6_el5_5_2.tgz ${PROD_SRC_ROOT}/base_os/linux_el/el6/pam/pam_unix \
    glib2-2.22.5-7.el6.tgz     ${PROD_SRC_ROOT}/base_os/linux_el/el6/glib2/patches \

### FREEDIST_BASE_MODIFIED_SOURCES_LINUX_EL_EL5_PPC =\
###     kernel_linux_redhat-2.6-ppc-src.tgz ${PROD_SRC_ROOT}/base_os/linux_el/el5/kernel/arch_ppc/src  \
###     kernel_linux_redhat-2.6-ppc-patches.tgz ${PROD_SRC_ROOT}/base_os/linux_el/el5/kernel/arch_ppc/patches  \
REEDIST_BASE_MODIFIED_SOURCES_LINUX_EL_EL5_PPC =

FREEDIST_BASE_MODIFIED_SOURCES=\
	${FREEDIST_BASE_MODIFIED_SOURCES_COMMON} \
	${FREEDIST_BASE_MODIFIED_SOURCES_${PROD_TARGET_OS}_COMMON} \
	${FREEDIST_BASE_MODIFIED_SOURCES_${PROD_TARGET_PLATFORM}_COMMON} \
	${FREEDIST_BASE_MODIFIED_SOURCES_${PROD_TARGET_PLATFORM_FULL}} \
	${FREEDIST_BASE_MODIFIED_SOURCES_${PROD_TARGET_PLATFORM_FULL}_${PROD_TARGET_ARCH}} \

FREEDIST_CUSTOMER_MODIFIED_SOURCES=\
	${FREEDIST_CUSTOMER_MODIFIED_SOURCES_COMMON} \
	${FREEDIST_CUSTOMER_MODIFIED_SOURCES_${PROD_TARGET_OS}_COMMON} \
	${FREEDIST_CUSTOMER_MODIFIED_SOURCES_${PROD_TARGET_PLATFORM}_COMMON} \
	${FREEDIST_CUSTOMER_MODIFIED_SOURCES_${PROD_TARGET_PLATFORM_FULL}} \
	${FREEDIST_CUSTOMER_MODIFIED_SOURCES_${PROD_TARGET_PLATFORM_FULL}_${PROD_TARGET_ARCH}} \


FREEDIST_BASE_MODIFIED_SOURCES_BALLS=$(foreach l,${FREEDIST_BASE_MODIFIED_SOURCES},$(filter-out ${PROD_SRC_ROOT}/%,$l))
FREEDIST_BASE_MODIFIED_SOURCES_INSTALL_BALLS=$(addprefix ${FULL_BUILD_DIR}/,${FREEDIST_BASE_MODIFIED_SOURCES_BALLS})
FREEDIST_BASE_MODIFIED_SOURCES_DIRS=$(foreach l,${FREEDIST_BASE_MODIFIED_SOURCES},$(filter ${PROD_SRC_ROOT}/%,$l))

FREEDIST_CUSTOMER_MODIFIED_SOURCES_BALLS=$(foreach l,${FREEDIST_CUSTOMER_MODIFIED_SOURCES},$(filter-out ${PROD_CUSTOMER_SRC_ROOT}/%,$l))
FREEDIST_CUSTOMER_MODIFIED_SOURCES_INSTALL_BALLS=$(addprefix ${FULL_BUILD_DIR}/,${FREEDIST_CUSTOMER_MODIFIED_SOURCES_BALLS})
FREEDIST_CUSTOMER_MODIFIED_SOURCES_DIRS=$(foreach l,${FREEDIST_CUSTOMER_MODIFIED_SOURCES},$(filter ${PROD_CUSTOMER_SRC_ROOT}/%,$l))


FREEDIST_MODIFIED_SOURCES_BALLS=\
	${FREEDIST_BASE_MODIFIED_SOURCES_BALLS} \
	${FREEDIST_CUSTOMER_MODIFIED_SOURCES_BALLS} 

FREEDIST_MODIFIED_SOURCES_INSTALL_BALLS=\
	${FREEDIST_BASE_MODIFIED_SOURCES_INSTALL_BALLS} \
	${FREEDIST_CUSTOMER_MODIFIED_SOURCES_INSTALL_BALLS} 

FREEDIST_MODIFIED_SOURCES_DIRS=\
	${FREEDIST_BASE_MODIFIED_SOURCES_DIRS} \
	${FREEDIST_CUSTOMER_MODIFIED_SOURCES_DIRS} 


CLEANFILES+=${FREEDIST_MODIFIED_SOURCES_BALLS}

# This makefile is not "-j" safe.  Do not remove the following line
.NOTPARALLEL:

.PHONY: all startup shutdown
.PHONY:ensure_freedist_dir ensure_build_dirs ensure_install_dirs ensure_release_dir
.PHONY:build make_tar_balls customer_make_build
.PHONY:install install_version_files customer_make_install
.PHONY:release customer_make_release distball distiso

# ============================== ALL ==============================
all: startup build install release shutdown

startup:

shutdown:
	$(build_times-shutdown)

ensure_freedist_dir:
	@if test ! -d ${FREEDIST_OUTPUT_DIR}/; then \
		mkdir -m 777 -p ${FREEDIST_OUTPUT_DIR}; \
		if test ! -d ${FREEDIST_OUTPUT_DIR}/; then \
			${ECHO} "Unable to create ${FREEDIST_OUTPUT_DIR}."; \
			 exit 1; \
		 fi; \
		 ${ECHO} "Created directory ${FREEDIST_OUTPUT_DIR}"; \
	 fi

ensure_build_dirs: ensure_freedist_dir
	@if test ! -d ${FULL_BUILD_DIR}/; then \
		mkdir -m 755 -p ${FULL_BUILD_DIR}; \
		if test ! -d ${FULL_BUILD_DIR}/; then \
			${ECHO} "Unable to create ${FULL_BUILD_DIR}."; \
			 exit 1; \
		 fi; \
		 ${ECHO} "Created directory ${FULL_BUILD_DIR}"; \
	 fi

ensure_install_dirs: ensure_freedist_dir
	@if test ! -d ${FULL_INSTALL_DIR}/; then \
		mkdir -m 755 -p ${FULL_INSTALL_DIR}; \
		if test ! -d ${FULL_INSTALL_DIR}/; then \
			${ECHO} "Unable to create ${FULL_INSTALL_DIR}."; \
			 exit 1; \
		 fi; \
		 ${ECHO} "Created directory ${FULL_INSTALL_DIR}"; \
	 fi
	@if [ ! -d        ${FULL_INSTALL_DIR}/dist ] ; then \
	  mkdir -m 755 -p ${FULL_INSTALL_DIR}/dist ; fi
	@if [ ! -d        ${FULL_INSTALL_DIR}/dist/modified ] ; then \
	  mkdir -m 755 -p ${FULL_INSTALL_DIR}/dist/modified ; fi
	@if [ ! -d        ${FULL_INSTALL_DIR}/dist/unmodified ] ; then \
	  mkdir -m 755 -p ${FULL_INSTALL_DIR}/dist/unmodified ; fi
	@if [ ! -d        ${FULL_INSTALL_DIR}/dist/release ] ; then \
	  mkdir -m 755 -p ${FULL_INSTALL_DIR}/dist/release ; fi

ensure_release_dir: ensure_freedist_dir
	@if test ! -d ${FULL_RELEASE_DIR}/; then \
		mkdir -m 755 -p ${FULL_RELEASE_DIR}; \
		if test ! -d ${FULL_RELEASE_DIR}/; then \
			${ECHO} "Unable to create ${FULL_RELEASE_DIR}."; \
			 exit 1; \
		 fi; \
		 ${ECHO} "Created directory ${FULL_RELEASE_DIR}"; \
	 fi


# ============================== BUILD ==============================
build: make_tar_balls customer_make_build

make_tar_balls: ensure_build_dirs
	echo RULE: make_tar_balls
	@for count in `seq 1 $(words ${FREEDIST_MODIFIED_SOURCES_BALLS})` ; do \
		curr_ball=`echo ${FREEDIST_MODIFIED_SOURCES_INSTALL_BALLS} | awk '{print $$'$$count'}'` ;\
		curr_dir=`echo ${FREEDIST_MODIFIED_SOURCES_DIRS} | awk '{print $$'$$count'}'` ;\
		rm -f $${curr_ball} ;\
		if [ -z "$${curr_dir}" -o \( ! -d "$${curr_dir}" -a ! -f "$${curr_dir}" \) ]; then \
			echo "ERROR: missing dir $${curr_dir} for $${curr_ball}" ;\
			exit 1 ;\
		fi ;\
		echo "Making tarball of sources to $${curr_ball}" ;\
		(cd `dirname "$${curr_dir}"`; tar --exclude=.svn -czf $${curr_ball} `basename "$${curr_dir}"`) || exit 1;\
	done;

customer_make_build:
	if test -f ${PROD_CUSTOMER_ROOT}/${PROD_PRODUCT_LC}/src/release/freedist/Makefile.cust; then \
	  cd ${PROD_CUSTOMER_ROOT}/${PROD_PRODUCT_LC}/src/release/freedist ; make -f Makefile.cust build; \
	fi


# ============================== INSTALL ==============================
install: ensure_install_dirs install_version_files customer_make_install
	install -m 644 ${PROD_SRC_ROOT}/release/freedist/README.freedist ${FULL_INSTALL_DIR}/dist/README || exit 1
	install -m 644 ${PROD_SRC_ROOT}/release/freedist/gpl-2.0.txt ${FULL_INSTALL_DIR}/dist || exit 1
	install -m 644 ${PROD_SRC_ROOT}/release/freedist/lgpl-2.1.txt ${FULL_INSTALL_DIR}/dist || exit 1
	install -m 644 ${PROD_SRC_ROOT}/release/freedist/gpl-3.0.txt ${FULL_INSTALL_DIR}/dist || exit 1
	install -m 644 ${PROD_SRC_ROOT}/release/freedist/lgpl-3.0.txt ${FULL_INSTALL_DIR}/dist || exit 1
	install -m 644 ${PROD_SRC_ROOT}/release/freedist/About-pgpkey.txt ${FULL_INSTALL_DIR}/dist/release || exit 1
	install -m 644 ${PROD_SRC_ROOT}/release/freedist/e8562897.pgpkey.txt ${FULL_INSTALL_DIR}/dist/release || exit 1
	@all_srpms='${FREEDIST_UNMODIFIED_SRPMS}'; \
	for srpm in $$all_srpms; do \
		echo "Installing unmodified SRPM from $${srpm}"; \
		if test ! -e $${srpm} ; then \
			${ECHO} "*** Unable to find file $${srpm}" ; exit 1; \
		fi; \
		install -m 644 $${srpm} -p ${FULL_INSTALL_DIR}/dist/unmodified || exit 1;\
	done;
	@all_balls='${FREEDIST_MODIFIED_SOURCES_INSTALL_BALLS}'; \
	for ball in $$all_balls; do \
		echo "Installing modified sources in tarball $${ball}"; \
		if test ! -e $${ball} ; then \
			${ECHO} "Unable to find file $${ball}"; \
			 exit 1; \
		fi; \
		install -m 644 $${ball} ${FULL_INSTALL_DIR}/dist/modified || exit 1;\
	done;

install_version_files: ensure_install_dirs
	if [ -f ${PROD_BUILD_OUTPUT_DIR}/release/build_version.txt ] ; then \
	  install -m 644 ${PROD_BUILD_OUTPUT_DIR}/release/build_version.txt ${FULL_INSTALL_DIR} || exit 1 ; \
	fi
	if [ -f ${PROD_BUILD_OUTPUT_DIR}/release/build_version.sh  ] ; then \
	  install -m 755 ${PROD_BUILD_OUTPUT_DIR}/release/build_version.sh  ${FULL_INSTALL_DIR} || exit 1 ; \
	fi
	grep ^PROD_TMS_SRCS_VERSION:= ${PROD_TREE_ROOT}/src/release/version/release.tms.mk \
	   | cut -f2 -d= > ${FULL_INSTALL_DIR}/dist/release/samara_version.txt

customer_make_install: ensure_install_dirs
	if test -f ${PROD_CUSTOMER_ROOT}/${PROD_PRODUCT_LC}/src/release/freedist/Makefile.cust; then \
	  cd ${PROD_CUSTOMER_ROOT}/${PROD_PRODUCT_LC}/src/release/freedist ; make -f Makefile.cust install; \
	fi

# ============================== RELEASE ==============================
release: customer_make_release distball
	@echo make release DONE ==========

customer_make_release: ensure_release_dir
	@echo customer_make_release =============================================
	if test -f ${PROD_CUSTOMER_ROOT}/${PROD_PRODUCT_LC}/src/release/freedist/Makefile.cust; then \
	  cd ${PROD_CUSTOMER_ROOT}/${PROD_PRODUCT_LC}/src/release/freedist ; make -f Makefile.cust release; \
	fi

distball: ensure_release_dir
	@echo distball =============================================
	@DIST_ROOT_NAME=freedist ; \
	if [ -f ${FULL_INSTALL_DIR}/mfc_version.txt ] ; then \
	    .   ${FULL_INSTALL_DIR}/mfc_version.txt ; \
	    DIST_ROOT_NAME=freedist-`echo $${RELEASE_VER}_$${ID_1}_$${ID_2}` ; \
	fi ; \
	DISTBALL_COMPRESSED_FULL_NAME=$${DIST_ROOT_NAME}.tgz; \
	echo "Making dist tarball into ${FULL_RELEASE_DIR}/$${DISTBALL_COMPRESSED_FULL_NAME}" ;\
	(cd ${FULL_INSTALL_DIR}; tar -czf ${FULL_RELEASE_DIR}/$${DISTBALL_COMPRESSED_FULL_NAME} dist) || exit 1;\
	echo FREEDIST_FILENAME=$${DISTBALL_COMPRESSED_FULL_NAME} >> ${FREEDIST_OUTPUT_DIR}/freedistinfo.sh

# Note that distiso is NOT automatically made as part of "make release".
distiso: ensure_release_dir
	@echo distiso =============================================
	@DIST_ROOT_NAME=freedist ; \
	if [ -f ${FULL_INSTALL_DIR}/mfc_version.txt ] ; then \
	    .   ${FULL_INSTALL_DIR}/mfc_version.txt ; \
	    DIST_ROOT_NAME=freedist-`echo $${RELEASE_VER}_$${ID_1}_$${ID_2}` ; \
	fi ; \
	DISTISO_FULL_NAME=$${DIST_ROOT_NAME}.iso; \
	echo "Making dist iso into ${FULL_RELEASE_DIR}/$${DISTISO_FULL_NAME}" ;\
	(cd ${FULL_INSTALL_DIR}/dist; mkisofs -quiet -R -J -o ${FULL_RELEASE_DIR}/$${DISTISO_FULL_NAME} .) || exit 1;\
	echo FREEDISTISO_FILENAME=$${DISTISO_FULL_NAME} >> ${FREEDIST_OUTPUT_DIR}/freedistinfo.sh


# ============================== CLEAN ==============================
clean:
	rm -rf ${FREEDIST_OUTPUT_DIR}
	@if [ -d ${FREEDIST_OUTPUT_DIR} ] ; then echo Failed to remove ${FREEDIST_OUTPUT_DIR} ; fi

sudoclean:
	sudo rm -rf ${FREEDIST_OUTPUT_DIR}
	sudo mkdir -m 777 ${FREEDIST_OUTPUT_DIR}

# ============================== DEPEND ==============================
depend:

# ============================== REALCLEAN ==============================
realclean: clean
