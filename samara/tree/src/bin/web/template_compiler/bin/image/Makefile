#  Makefile
#
#
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

PROG=tmplcompiler

# Do not install the tmplcompiler, as it is not needed.  We build it to
# make sure it compiles on both the host and target, and to use as a
# potential debugging tool.
DO_PROG_INSTALL=
# Make sure we do not try to strip something we have not installed.
# Remove if you remove the previous line.
DO_PROG_STRIP=

vpath %.c ../../src
vpath %.c ../../src/BIN_DIST/image

SRCS= \
	tc_main.c

# XXX these should be done in a more compact way
LDADD=  \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libwebtmpl/libwebtmpl.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libcommon/libcommon.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libtc_st/libtc_st.a \


include ${PROD_TREE_ROOT}/src/mk/prog.mk

