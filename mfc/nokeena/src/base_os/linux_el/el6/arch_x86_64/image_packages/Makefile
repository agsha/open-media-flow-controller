#  Makefile for adding binary RPMs to the MFC package.
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

# This makefile converts rpm's to tar's on 'all' and dumps the tar's into
# the image install tree on 'install'

CHECK_BINARY_RPMS_FILE=../packages/pkg_bin_check.txt

PACKAGES_BINARY_RPMS= \
  ../packages/MegaCli-8.00.29-1.i386.rpm \
  ../packages/StorMan-7.31-18856.x86_64.rpm \
  ../packages/hpacucli-9.10-22.0.x86_64.rpm \
  ../packages/nash-5.1.19.6-54.x86_64.rpm \
  ../packages/rssh-2.3.3-2.el6.rf.x86_64.rpm \

FULL_BUILD_DIR=${PROD_BUILD_OUTPUT_DIR}/image/cust_packages
CONFIG_FILE=pkg_config.pl

include ${PROD_TREE_ROOT}/src/mk/packages.mk
