#  Makefile
#
# 
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

LIB=mdb

SRCS= \
	mdb_adv_query.c \
	mdb_changes.c \
	mdb_db.c \
	mdb_dbbe.c \
	mdb_mon.c \
	mdb_reg.c \


INCLUDES= \
	-I${PROD_TREE_ROOT}/src/bin/mgmtd \
	-I${PROD_TREE_ROOT}/src/lib/libevent \
	-I${PROD_TREE_ROOT}/src/lib/libgcl \

LDADD=

ifneq (${PROD_FEATURE_I18N_SUPPORT}, 0)
	GETTEXT_PACKAGE=lib${LIB}
	DEFINES+=-DGETTEXT_DOMAIN=\"${GETTEXT_PACKAGE}\"
	SUBMAKEFILES+=Makefile.i18n
endif

include ${PROD_TREE_ROOT}/src/mk/lib.mk
