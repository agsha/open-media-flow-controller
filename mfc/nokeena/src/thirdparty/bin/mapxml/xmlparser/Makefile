#  Filename: Makefile 
#  (C) Copyright 2014 Nokeena Networks, Inc.  
#  All rights reserved.
# 

WARNINGS_OVERRIDE = 1
include ${PROD_TREE_ROOT}/src/mk/common.mk

FREEDIST_DIR=${PROD_OUTPUT_ROOT}/product-nokeena-x86_64/freedist

.PHONY: install attribution source_archive ensure_freedist_dir

install: attribution

clean: clean_self

clean_self:
	$(RM) ${FREEDIST_DIR}/build/xmlparser.tgz
	$(RM) ${FREEDIST_DIR}/attrib/Attribution-xmlparser.txt

sudo_clean:
	-sudo $(RM) ${FREEDIST_DIR}/build/xmlparser.tgz
	-sudo $(RM) ${FREEDIST_DIR}/attrib/Attribution-xmlparser.txt

attribution: ensure_freedist_dir ${FREEDIST_DIR}/attrib/Attribution-xmlparser.txt

source_archive: ensure_freedist_dir ${FREEDIST_DIR}/build/xmlparser.tgz

ensure_freedist_dir:
	@if [ ! -d ${FREEDIST_DIR} ] ; then mkdir -p ${FREEDIST_DIR} ; fi
	-@chown ${REAL_USER} ${FREEDIST_DIR}
	@if [ ! -d ${FREEDIST_DIR}/build ] ; then mkdir -p ${FREEDIST_DIR}/build ; fi
	-@chown ${REAL_USER} ${FREEDIST_DIR}/build
	@if [ ! -d ${FREEDIST_DIR}/attrib ] ; then mkdir -p ${FREEDIST_DIR}/attrib ; fi
	-@chown ${REAL_USER} ${FREEDIST_DIR}/attrib

${FREEDIST_DIR}/build/xmlparser.tgz:
	@echo Create source archive for xmlparser
	@tar -czf ${FREEDIST_DIR}/build/xmlparser.tgz `find * -type f | grep -v '\.svn' | grep -v Makefile'$$'`
	@ls -l ${FREEDIST_DIR}/build/xmlparser.tgz
	
${FREEDIST_DIR}/attrib/Attribution-xmlparser.txt:  Attribution-xmlparser.txt
	@echo Copy attribution file for xmlparser
	@cp Attribution-xmlparser.txt ${FREEDIST_DIR}/attrib/Attribution-xmlparser.txt
	@ls -l  ${FREEDIST_DIR}/attrib/Attribution-xmlparser.txt
