#  Makefile
#
#
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

NAME=fpd_ui_mods

ifdef STATIC_MODULES
    LIB=${NAME}
    LIB_BUILD_DIR=${ILIB_DIR}/bin/fpd/modules/ui
else
    SO_CLASS=fpd
    SO_BUILD_DIR=${LIB_DIR}/${SO_CLASS}/modules/ui
    SO_INSTALL_DIR=lib/${SO_CLASS}/modules/ui
endif

SRCS=                             \
	fpm_base.c                \
	fpm_net_cfg.c             \
	fpm_lcd.c                 \


ifneq (${PROD_FEATURE_CLUSTER},0)
	SRCS+=                      \
		fpm_cluster.c       \

endif

INCLUDES= -I../..

ifneq (${PROD_FEATURE_I18N_SUPPORT}, 0)
	GETTEXT_PACKAGE=${NAME}
	DEFINES+=-DGETTEXT_DOMAIN=\"${GETTEXT_PACKAGE}\"
	SUBMAKEFILES+=Makefile.i18n
endif

ifdef STATIC_MODULES
    include ${PROD_TREE_ROOT}/src/mk/lib.mk
else
    include ${PROD_TREE_ROOT}/src/mk/so.mk
endif
