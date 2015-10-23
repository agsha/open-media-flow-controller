# Filename: bin/cb/cgi/Makefile 

WARNINGS_OVERRIDE=1

include ${PROD_TREE_ROOT}/src/mk/common.mk

PROD_INSTALL_PREFIX=/opt/nkn
PROG_INSTALL_DIR=bin
PROG=OCRP_cgi

SRCS= \
	OCRP_cgi.c \

CFLAGS=-I/usr/local/include \
	-Wall -Werror -fPIC ${BUILD_EXTRA_CFLAGS}

ifeq (${BUILD_GCOV}, 1)
        CFLAGS += -fprofile-arcs -ftest-coverage
endif


ifneq (${PROD_FEATURE_I18N_SUPPORT}, 0)
	GETTEXT_PACKAGE=${PROG}
	DEFINES+=-DGETTEXT_DOMAIN=\"${GETTEXT_PACKAGE}\"
	SUBMAKEFILES+=Makefile.i18n
endif

##LDFLAGS += -lcgic -lc
LDFLAGS += -L/usr/local/lib -lcgic -lc -lxml2 -lcurl

# XXX these should be done in a more compact way
LDADD= \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/cb/protocol/OCRP/libcb_protocol_OCRP_xml.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/cb/protocol/common/libcb_protocol_common_xml.a \
	${BUILD_EXTRA_LDADD}

include ${PROD_TREE_ROOT}/src/mk/prog.mk
