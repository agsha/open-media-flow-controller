#  Makefile
#
#
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

NAME=fpd_io_mods

ifdef STATIC_MODULES
    LIB=${NAME}
    LIB_BUILD_DIR=${ILIB_DIR}/bin/fpd/modules/io
else
    SO_CLASS=fpd
    SO_BUILD_DIR=${LIB_DIR}/${SO_CLASS}/modules/io
    SO_INSTALL_DIR=lib/${SO_CLASS}/modules/io
endif

SRCS=                             \
	fpm_io_adv_lcm100_ser.c   \
	fpm_io_cfontz_633_ser.c   \
	fpm_io_exec.c             \
	fpm_io_stdio.c            \

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
