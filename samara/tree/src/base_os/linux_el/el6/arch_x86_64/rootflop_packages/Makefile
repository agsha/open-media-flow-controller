#  Makefile
#
#
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

# This makefile converts rpm's to tar's on 'all' and dumps the tar's into
# the image install tree on 'install'

DIST_SCL_6=/volume/ssd-linux-storage01/repo/ssdlinux/scl6/release/6.6.R1.0
DIST_SCL_6_I386=${DIST_SCL_6}/i386/rpm
DIST_SCL_6_X86_64=${DIST_SCL_6}/x86_64/rpm
DIST_SCL_6_SRPMS=${DIST_SCL_6}/srpm


CHECK_BINARY_RPMS_FILE=../packages/pkg_bin_check.txt

PACKAGES_BINARY_RPMS= \
	${DIST_SCL_6_X86_64}/bzip2-1.0.5-7.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/bzip2-libs-1.0.5-7.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/coreutils-8.4-37.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/cpio-2.10-12.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/device-mapper-1.02.90-2.SCLC6_6.1.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/dmidecode-2.12-5.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/dosfstools-3.0.9-4.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/e2fsprogs-1.41.12-21.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/e2fsprogs-libs-1.41.12-21.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/glibc-2.12-1.149.SCLC6_6.5.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/libacl-2.2.49-6.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/libattr-2.4.44-7.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/libblkid-2.17.2-12.18.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/libcom_err-1.41.12-21.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/libselinux-2.0.94-5.8.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/libsepol-2.0.41-4.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/libuuid-2.17.2-12.18.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/ncurses-base-5.7-3.20090208.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/ncurses-libs-5.7-3.20090208.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/nss-softokn-freebl-3.14.3-22.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/tar-1.23-11.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/unzip-6.0-2.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/util-linux-ng-2.17.2-12.18.SCLC6_6.R1.0.1.x86_64.rpm \

ifneq (${PROD_FEATURE_IMAGE_SECURITY},0)
	PACKAGES_BINARY_RPMS+= \
		${DIST_SCL_6_X86_64}/gnupg2-2.0.14-8.SCLC6_6.R1.0.1.x86_64.rpm \
		${DIST_SCL_6_X86_64}/libgcrypt-1.4.5-11.SCLC6_6.R1.0.1.x86_64.rpm \
		${DIST_SCL_6_X86_64}/libgpg-error-1.7-4.SCLC6_6.R1.0.1.x86_64.rpm \
		${DIST_SCL_6_X86_64}/readline-6.0-4.SCLC6_6.R1.0.1.x86_64.rpm \
		${DIST_SCL_6_X86_64}/zlib-1.2.3-29.SCLC6_6.R1.0.1.x86_64.rpm \

endif # ! PROD_FEATURE_IMAGE_SECURITY


FULL_BUILD_DIR=${PROD_BUILD_OUTPUT_DIR}/rootflop/packages
FULL_INSTALL_DIR=${PROD_INSTALL_OUTPUT_DIR}/rootflop
CONFIG_FILES=${PROD_BUILD_OUTPUT_DIR}/release/build_version.pl pkg_config.pl

.DEFAULT_GOAL := all

${PROD_BUILD_OUTPUT_DIR}/release/build_version.pl:
	(cd ${PROD_SRC_ROOT}/release && $(MAKE) version)

-include ${PROD_CUSTOMER_ROOT}/${PROD_PRODUCT_LC}/src/base_os/${PROD_TARGET_PLATFORM_LC}/${PROD_TARGET_PLATFORM_VERSION_LC}/arch_${PROD_TARGET_ARCH_LC}/rootflop_packages/Makefile.inc

include ${PROD_TREE_ROOT}/src/mk/packages.mk
