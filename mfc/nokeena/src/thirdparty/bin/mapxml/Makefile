#  Filename: Makefile 
#  (C) Copyright 2009 Nokeena Networks, Inc.  
#  All rights reserved.
# 

WARNINGS_OVERRIDE = 1
include ${PROD_TREE_ROOT}/src/mk/common.mk

SUBDIRS= \
  xmlparser

PROD_INSTALL_PREFIX=/opt/nkn
PROG_INSTALL_DIR=bin
PROG=MapXML

CLEANFILES+=${FULL_BUILD_DIR}/HostOriginMapDTD.cpp \
	${FULL_BUILD_DIR}/ClusterMapDTD.cpp \
	${FULL_BUILD_DIR}/OriginEscalationMapDTD.cpp \
	${FULL_BUILD_DIR}/OriginRoundRobinMapDTD.cpp

CXXFLAGS +=-I ./xmlparser -I /usr/include/libxml2
LDFLAGS += -lxml2
SRCS=                 \
	mapxml.cpp   \
	mapxml_utils.c   \
	HostOriginMap.cpp \
	ClusterMap.cpp \
	OriginEscalationMap.cpp \
	OriginRoundRobinMap.cpp \
	${FULL_BUILD_DIR}/HostOriginMapDTD.cpp \
	${FULL_BUILD_DIR}/ClusterMapDTD.cpp \
	${FULL_BUILD_DIR}/OriginEscalationMapDTD.cpp \
	${FULL_BUILD_DIR}/OriginRoundRobinMapDTD.cpp \
	xmlparser/xmlParser.cpp \

ifneq (${PROD_FEATURE_I18N_SUPPORT}, 0)
	GETTEXT_PACKAGE=${PROG}
	DEFINES+=-DGETTEXT_DOMAIN=\"${GETTEXT_PACKAGE}\"
	SUBMAKEFILES+=Makefile.i18n
endif

include ${PROD_TREE_ROOT}/src/mk/prog.mk

${FULL_BUILD_DIR}/HostOriginMapDTD.cpp: dtd/HostOriginMap.dtd
	rm -rf ${FULL_BUILD_DIR}/HostOriginMapDTD.cpp
	`/bin/awk -- 'BEGIN{printf("const char* HostOriginMapDTD =\n");} {printf("\"%s\"\n", $$0)}; END{printf(";\n");}' dtd/HostOriginMap.dtd > ${FULL_BUILD_DIR}/HostOriginMapDTD.cpp`

${FULL_BUILD_DIR}/ClusterMapDTD.cpp: dtd/ClusterMap.dtd
	rm -rf ${FULL_BUILD_DIR}/ClusterMapDTD.cpp
	`/bin/awk -- 'BEGIN{printf("const char* ClusterMapDTD =\n");} {printf("\"%s\"\n", $$0)}; END{printf(";\n");}' dtd/ClusterMap.dtd > ${FULL_BUILD_DIR}/ClusterMapDTD.cpp`

${FULL_BUILD_DIR}/OriginEscalationMapDTD.cpp: dtd/OriginEscalationMap.dtd
	rm -rf ${FULL_BUILD_DIR}/OriginEscalationMapDTD.cpp
	`/bin/awk -- 'BEGIN{printf("const char* OriginEscalationMapDTD =\n");} {printf("\"%s\"\n", $$0)}; END{printf(";\n");}' dtd/OriginEscalationMap.dtd > ${FULL_BUILD_DIR}/OriginEscalationMapDTD.cpp`

${FULL_BUILD_DIR}/OriginRoundRobinMapDTD.cpp: dtd/OriginRoundRobinMap.dtd
	rm -rf ${FULL_BUILD_DIR}/OriginRoundRobinMapDTD.cpp
	`/bin/awk -- 'BEGIN{printf("const char* OriginRoundRobinMapDTD =\n");} {printf("\"%s\"\n", $$0)}; END{printf(";\n");}' dtd/OriginRoundRobinMap.dtd > ${FULL_BUILD_DIR}/OriginRoundRobinMapDTD.cpp`
