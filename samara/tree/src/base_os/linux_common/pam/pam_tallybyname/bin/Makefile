#  Makefile
#
# 
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

PROG=pam_tallybyname
PROD_INSTALL_PREFIX=
PROG_INSTALL_DIR=/opt/tms/bin

vpath %.c ../src

SRCS=   \
	pam_tallybyname_bin.c \
	pam_tallybyname_common.c \
	sqlite_utils.c \

INCLUDES=\
	-I../src \
	-I../../include \

LDADD+=\
	-lsqlite3 \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${ILIB_DIR}/base_os/linux_common/pam/lib/libpam_common/libpam_common.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libcommon/libcommon.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libtc_st/libtc_st.a \

include ${PROD_TREE_ROOT}/src/mk/prog.mk
