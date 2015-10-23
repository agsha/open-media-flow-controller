#  Makefile
#
# 
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

LIB=cli

#
# NOTE: linking with this library requires linking with libtacl also.
#

#
# Note that cli_acls.c and cli_capabilities.c are included unconditionally
# on purpose.  Some of their code is required even if the corresponding
# feature is disabled.  e.g. cli_acls.c has cli_acl_add() et al.,
# which are used unconditionally by module registration.
#

SRCS=                              \
	cli_acls.c                 \
	cli_capabilities.c         \
	cli_completion.c           \
	cli_exec.c                 \
	cli_file_utils.c           \
	cli_help.c                 \
	cli_main.c                 \
	cli_mgmt_module.c          \
	cli_modules.c              \
	cli_notif_utils.c          \
	cli_output.c               \
	cli_parsing.c              \
	cli_prefixes.c             \
	cli_registration.c         \
	cli_revmap.c               \
	cli_standard_handlers.c    \
	cli_utils.c                \

ifdef STATIC_MODULES
    CFLAGS+=-DSTATIC_MODULES
endif

INCLUDES=

ifneq (${PROD_FEATURE_I18N_SUPPORT}, 0)
	GETTEXT_PACKAGE=lib${LIB}
	DEFINES+=-DGETTEXT_DOMAIN=\"${GETTEXT_PACKAGE}\"
	SUBMAKEFILES+=Makefile.i18n
endif

# Graft point for customer-specific additions to build options for libcli.
# e.g. a customer may want to add more source files to libcli build by
# adding stuff to SRCS.
-include ${PROD_CUSTOMER_ROOT}/${PROD_PRODUCT_LC}/src/lib/libcli/Makefile.inc

include ${PROD_TREE_ROOT}/src/mk/lib.mk
