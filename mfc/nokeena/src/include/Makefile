# Makefile

include ${PROD_TREE_ROOT}/src/mk/common.mk

ifneq (${PROD_FEATURE_I18N_SUPPORT}, 0)
	GETTEXT_PACKAGE=mfd_incl
	DEFINES+=-DGETTEXT_DOMAIN=\"${GETTEXT_PACKAGE}\"
	SUBMAKEFILES+=Makefile.i18n
endif

# Just to get SUBMAKEFILES to work.
include ${PROD_TREE_ROOT}/src/mk/subdir.mk
