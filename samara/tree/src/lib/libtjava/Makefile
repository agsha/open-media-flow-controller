#  Makefile
#
# 
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

OBJ_JNI_NATIVE=1

LIB=tjava

SRCS= \
	java_utils.c \

LDADD=

#ifneq (${PROD_FEATURE_I18N_SUPPORT}, 0)
#	GETTEXT_PACKAGE=lib${LIB}
#	DEFINES+=-DGETTEXT_DOMAIN=\"${GETTEXT_PACKAGE}\"
#	SUBMAKEFILES+=Makefile.i18n
#endif

include ${PROD_TREE_ROOT}/src/mk/lib.mk
