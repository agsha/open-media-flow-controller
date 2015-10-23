#  Makefile
#
#
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

PROG=stats_fix_file

SRCS=                      \
	sff_main.c         \

CFLAGS= -I${PROD_TREE_ROOT}/src/bin/statsd

LDADD=  \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libgcl/libgcl.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libcommon/libcommon.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libtc_st/libtc_st.a \

INCLUDES= \
	-I${PROD_TREE_ROOT}/src/lib/libgcl \

include ${PROD_TREE_ROOT}/src/mk/prog.mk
