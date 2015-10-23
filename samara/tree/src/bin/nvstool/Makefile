#  Makefile
#
# 
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

PROG=nvstool

SRCS= \
	nt_main.c \

INCLUDES=-I${PROD_TREE_ROOT}/src/lib/libnvs

CFLAGS+=

LDADD= \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libnvs/libnvs.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libcommon/libcommon.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libtc_st/libtc_st.a \
	-lz \


SUBMAKEFILES=Makefile.rootflop

include ${PROD_TREE_ROOT}/src/mk/prog.mk
