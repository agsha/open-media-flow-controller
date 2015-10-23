#  Makefile
#
#
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

PROG=mddbreq

SRCS= \
	mddbr_main.c \

ifneq (${PROD_FEATURE_I18N_SUPPORT}, 0)
	GETTEXT_PACKAGE=${PROG}
	DEFINES+=-DGETTEXT_DOMAIN=\"${GETTEXT_PACKAGE}\"
	SUBMAKEFILES+=Makefile.i18n
endif

ifeq (${PROD_TARGET_PLATFORM_FULL}-${IMAGE_OLD_MFG_BINARYS},LINUX_EL_EL5-1)
	# This is a backward compatibility setting, for older remanufactures
	# XXX-64
	export PROD_TARGET_CPU_WORDSIZE=32
	LDFLAGS+=-static
	PROG_STRIP=1
endif

LDADD= \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libmdb/libmdb.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libtexpr/libtexpr.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libgcl/libgcl.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libcommon/libcommon.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libtc_st/libtc_st.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libtacl/libtacl.a \

SUBMAKEFILES= \
	Makefile.host \
	Makefile.rootflop \

include ${PROD_TREE_ROOT}/src/mk/prog.mk
