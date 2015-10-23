#  Makefile
#
#
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

PROG=genlicense

SRCS= \
	gl_main.c \

ifeq (${PROD_TARGET_PLATFORM_FULL}-${IMAGE_OLD_MFG_BINARYS},LINUX_EL_EL5-1)
	# This is a backward compatibility setting, for older remanufactures
	# XXX-64
	export PROD_TARGET_CPU_WORDSIZE=32
	LDFLAGS+=-static
endif

LDADD= \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/liblicense/liblicense.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libgcl/libgcl.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libcommon/libcommon.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libtc_st/libtc_st.a \

PROG_PERM_MODE=750

SUBMAKEFILES= \
	Makefile.host \
	Makefile.rootflop \

include ${PROD_TREE_ROOT}/src/mk/prog.mk
