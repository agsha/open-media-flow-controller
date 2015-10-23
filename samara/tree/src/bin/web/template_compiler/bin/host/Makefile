#  Makefile
#
# 
#
#

export PROD_TARGET_HOST=1

include ${PROD_TREE_ROOT}/src/mk/common.mk

PROG=tmplcompiler

export INSTALL_TREE=host
PROD_INSTALL_PREFIX=

vpath %.c ../../src
vpath %.c ../../src/BIN_DIST/host

SRCS= \
	tc_main.c

# XXX these should be done in a more compact way
LDADD=  \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libwebtmpl/libwebtmpl.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libcommon/libcommon.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libtc_st/libtc_st.a \


include ${PROD_TREE_ROOT}/src/mk/prog.mk

