#  Makefile
#
#
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

NAME=wiz_mods

ifdef STATIC_MODULES
    LIB=${NAME}
    LIB_BUILD_DIR=${ILIB_DIR}/bin/wizard/modules
else
    SO_CLASS=wizard
endif

SRCS=                              \
	wiz_steps.c                \
	wiz_passwords.c            \


ifneq (${PROD_FEATURE_IPV6},0)
	SRCS+=                     \
		wiz_ipv6.c         \

endif

ifneq (${PROD_FEATURE_CLUSTER},0)
	SRCS+=                      \
		wiz_cluster_steps.c \

endif

ifneq (${PROD_FEATURE_CMC_CLIENT}${PROD_FEATURE_CMC_SERVER},00)
	SRCS+=                      \
		wiz_cmc_steps.c \

endif

ifneq (${PROD_FEATURE_I18N_SUPPORT}, 0)
	GETTEXT_PACKAGE=${NAME}
	DEFINES+=-DGETTEXT_DOMAIN=\"${GETTEXT_PACKAGE}\"
	SUBMAKEFILES+=Makefile.i18n
endif

INCLUDES= -I..

ifdef STATIC_MODULES
    include ${PROD_TREE_ROOT}/src/mk/lib.mk
else
    include ${PROD_TREE_ROOT}/src/mk/so.mk
endif
