# Filename: ngrep/Makefile
#
# By :  Ramanand Narayanan (ramanandn@juniper.net)
#
#  (C) Copyright 2012 Juniper Networks, Inc.  
#  All rights reserved.
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

PROD_INSTALL_PREFIX=/opt/nkn
PROG_INSTALL_DIR=debugbin
PROG=ngrep

INSTALL_TREE?=image
PROG_BUILD_DIR?=${BIN_DIR}/${PROG}
FULL_BUILD_DIR=${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${PROG_BUILD_DIR}
FULL_INSTALL_DIR=${PROD_INSTALL_OUTPUT_DIR}/${INSTALL_TREE}/${PROD_INSTALL_PREFIX}/${PROG_INSTALL_DIR}
FREEDIST_DIR=${PROD_OUTPUT_ROOT}/product-nokeena-x86_64/freedist

# Local Macros
BIN_NGREP = $(PROD_BUILD_OUTPUT_DIR)/image/bin/ngrep/ngrep

CFLAGS += -I. -O2 `pkg-config --cflags glib-2.0` -I/usr/local/include/pcap
CFLAGS += -DLINUX -DHAVE_CONFIG_H  -D_BSD_SOURCE=1 -D__FAVOR_BSD=1 -DSTDC_HEADERS=1 -DHAVE_STRING_H=1 -DHAVE_ALLOCA_H=1 -DHAVE_ALLOCA=1 
LDFLAGS += -L$(PROJECTLIB) -lpthread `pkg-config --cflags --libs glib-2.0 gthread-2.0` -lpcap

SRCS = regex.c \
       ngrep.c \

OBJS = $(addprefix ${FULL_BUILD_DIR}/,$(patsubst %.c,%.o,${SRCS}))
DEPS = $(addprefix ${FULL_BUILD_DIR}/,$(patsubst %.c,%.dep,${SRCS}))


# Rules 
.PHONY: clean clean_self
.PHONY: all setup bin install attribution source_archive ensure_freedist_dir

all: setup bin
	@echo
	@echo "----------NGREP Build Completed----------" 
	@echo

setup:
	@mkdir -p  $(FULL_BUILD_DIR)

bin: $(BIN_NGREP)

$(BIN_NGREP): $(OBJS) 
	$(CC) $(LDFLAGS) -o $@ $(OBJS) 

${FULL_BUILD_DIR}/%.o : %.c
	$(CC) -MD -c $(CFLAGS) -o $@ $<

install: attribution
	@if [ ! -d ${FULL_INSTALL_DIR} ] ; then echo Create install dir: ${FULL_INSTALL_DIR}; mkdir -p ${FULL_INSTALL_DIR}; fi
	$(CP) $(BIN_NGREP) ${FULL_INSTALL_DIR}

clean: clean_self

clean_self: 
	-$(RM) $(BIN_NGREP)
	-$(RM) ${FREEDIST_DIR}/build/ngrep.tgz
	-$(RM) ${FREEDIST_DIR}/attrib/Attribution-ngrep.txt

sudo_clean: 
	-sudo $(RM) $(BIN_NGREP)
	-sudo $(RM) ${FREEDIST_DIR}/build/ngrep.tgz
	-sudo $(RM) ${FREEDIST_DIR}/attrib/Attribution-ngrep.txt

attribution: ensure_freedist_dir ${FREEDIST_DIR}/attrib/Attribution-ngrep.txt

source_archive: ensure_freedist_dir ${FREEDIST_DIR}/build/ngrep.tgz

ensure_freedist_dir:
	@if [ ! -d ${FREEDIST_DIR} ] ; then mkdir -p ${FREEDIST_DIR} ; fi
	-@chown ${REAL_USER} ${FREEDIST_DIR}
	@if [ ! -d ${FREEDIST_DIR}/build ] ; then mkdir -p ${FREEDIST_DIR}/build ; fi
	-@chown ${REAL_USER} ${FREEDIST_DIR}/build
	@if [ ! -d ${FREEDIST_DIR}/attrib ] ; then mkdir -p ${FREEDIST_DIR}/attrib ; fi
	-@chown ${REAL_USER} ${FREEDIST_DIR}/attrib

${FREEDIST_DIR}/build/ngrep.tgz:
	@echo Create source archive for ngrep
	@tar -czf ${FREEDIST_DIR}/build/ngrep.tgz `find * -type f | grep -v '\.svn' | grep -v Makefile'$$'`
	@ls -l ${FREEDIST_DIR}/build/ngrep.tgz
	
${FREEDIST_DIR}/attrib/Attribution-ngrep.txt: Attribution-ngrep.txt
	@cp Attribution-ngrep.txt ${FREEDIST_DIR}/attrib/Attribution-ngrep.txt
	@ls -l ${FREEDIST_DIR}/attrib/Attribution-ngrep.txt

# End of Makefile
