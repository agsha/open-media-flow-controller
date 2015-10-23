#  Makefile
#
#
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

LIB=gcl

SRCS= \
	gcl.c \
	bnode.c \
	bnode_proto.c \
	bnode_wire.c \
	bnode_types.c \
	bnode_types_wire.c \
	mdc_array.c \
	mdc_main.c \
	mdc_misc.c \
	mdc_moni.c \
	mdc_remote_reg.c \
	mdc_backlog.c \

ifneq (${PROD_FEATURE_CLUSTER},0)
	SRCS+= \
	gen_proto.c \

endif

CFLAGS=

# XXX these should be done in a more compact way
LDADD= \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libcommon/libcommon.a


ifneq (${PROD_FEATURE_I18N_SUPPORT}, 0)
	GETTEXT_PACKAGE=lib${LIB}
	DEFINES+=-DGETTEXT_DOMAIN=\"${GETTEXT_PACKAGE}\"
	SUBMAKEFILES+=Makefile.i18n
endif

include ${PROD_TREE_ROOT}/src/mk/lib.mk
