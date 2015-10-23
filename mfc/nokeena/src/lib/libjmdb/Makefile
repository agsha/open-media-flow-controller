# Makefile

include ${PROD_TREE_ROOT}/src/mk/common.mk

LIB=jmdb
LIB_BUILD_DIR=${CUST_LIB_DIR}/libjmdb
PROD_INSTALL_PREFIX=/opt/nkn
LIB_INSTALL_DIR=lib

SRCS= \
	jmd_common.c \


INCLUDES= \
	-I${PROD_TREE_ROOT}/src/bin/mgmtd \
	-I${PROD_TREE_ROOT}/src/lib/libevent \
	-I${PROD_TREE_ROOT}/src/lib/libgcl \
	-I${PROD_TREE_ROOT}/src/include/bin/mgmtd \
	-I${PROD_TREE_ROOT}/src/include/lib/libgcl \

LDADD=

include ${PROD_TREE_ROOT}/src/mk/lib.mk
