#  Makefile
#
#
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

PROG=crypt

SRCS= \
	cpt_main.c \

ifeq (${PROD_TARGET_PLATFORM_FULL}-${IMAGE_OLD_MFG_BINARYS},LINUX_EL_EL5-1)
	# This is a backward compatibility setting, for older remanufactures
	# XXX-64
	export PROD_TARGET_CPU_WORDSIZE=32
	LDFLAGS+=-static
	PROG_STRIP=1
endif

LDADD= \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libtcrypt/libtcrypt.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libcommon/libcommon.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libtc_st/libtc_st.a \
	-lcrypt \

SUBMAKEFILES= \
	Makefile.rootflop \

include ${PROD_TREE_ROOT}/src/mk/prog.mk
