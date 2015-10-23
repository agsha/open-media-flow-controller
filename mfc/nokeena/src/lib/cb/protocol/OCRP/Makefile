# Filename: lib/cb/protocol/OCRP/Makefile

include ${PROD_TREE_ROOT}/src/mk/common.mk

LIB=cb_protocol_OCRP_xml
LIB_BUILD_DIR=${CUST_LIB_DIR}/cb/protocol/OCRP

CLEANFILES+=${FULL_BUILD_DIR}/OCRP_DTD.c ${FULL_BUILD_DIR}/OCRPupdate_DTD.c

SRCS= \
      OCRP_XMLops.c 	\
      ${FULL_BUILD_DIR}/OCRP_DTD.c \
      ${FULL_BUILD_DIR}/OCRPupdate_DTD.c \

CFLAGS += -fPIC
CFLAGS += -D_GNU_SOURCE
CFLAGS += -I/usr/include/libxml2
CFLAGS += ${BUILD_EXTRA_CFLAGS}

ifeq (${BUILD_GCOV}, 1)
        CFLAGS += -fprofile-arcs -ftest-coverage
endif


ifneq (${PROD_FEATURE_I18N_SUPPORT}, 0)
	GETTEXT_PACKAGE=lib${LIB}
	DEFINES+=-DGETTEXT_DOMAIN=\"${GETTEXT_PACKAGE}\"
	SUBMAKEFILES+=Makefile.i18n
endif

LDADD= ${BUILD_EXTRA_LDADD}


include ${PROD_TREE_ROOT}/src/mk/lib.mk


${FULL_BUILD_DIR}/OCRP_DTD.c: dtd/OCRP.dtd
	rm -rf ${FULL_BUILD_DIR}/OCRP_DTD.c
	`/bin/awk -- 'BEGIN{printf("const char* OCRP_DTD =\n");} {printf("\"%s\"\n", $$0)}; END{printf(";\n");}' dtd/OCRP.dtd > ${FULL_BUILD_DIR}/OCRP_DTD.c`


${FULL_BUILD_DIR}/OCRPupdate_DTD.c: dtd/OCRPupdate.dtd
	rm -rf ${FULL_BUILD_DIR}/OCRPupdate_DTD.c
	`/bin/awk -- 'BEGIN{printf("const char* OCRPupdate_DTD =\n");} {printf("\"%s\"\n", $$0)}; END{printf(";\n");}' dtd/OCRPupdate.dtd > ${FULL_BUILD_DIR}/OCRPupdate_DTD.c`
