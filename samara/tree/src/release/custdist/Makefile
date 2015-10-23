#  Makefile
#
# 
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

#
# This makefile creates a tarball and a cd image from the source tree.
# These will then be given to a customer.
#

# On build (all): the cvs export is done into build/custdist/source .  A
# manifest of all the files is made.  A tarball is produced of
# the exported sources.
#
# On install: the generated files are copied under install/custdist.
#
# On release: If encryption is enabled, each of the files is encrypted.
# md5sum's are produced.  A tarball of the files under install/custdist is
# made.  An iso of the same content is also made.
#

CUSTDIST_ENCRYPT=0

FULL_BUILD_DIR=${PROD_BUILD_OUTPUT_DIR}/custdist
CUSTDIST_WORK_DIR=${PROD_BUILD_OUTPUT_DIR}/custdist-work

ALL_BUILD_DIRS=${FULL_BUILD_DIR} ${CUSTDIST_WORK_DIR}

CLEANFILES+=${CUSTDIST_MODIFIED_SOURCES_BALLS}

ADD_CVS_TAG=
USE_CVS_TAG=

# This makefile is not "-j" safe.  Do not remove the following line
.NOTPARALLEL:

.PHONY: export_tree

# ============================== ALL ==============================
all: startup ensure_build_dirs export_tree shutdown

#  Could check various things there for validity, like ${PROD_SRC_ROOT}
startup:
	$(build_times-startup)

shutdown:
	$(build_times-shutdown)

ensure_build_dirs:
	@for subdir in ${ALL_BUILD_DIRS}; do \
		if test ! -d $${subdir}/; then \
			mkdir -p $${subdir}/; \
			if test ! -d $${subdir}/; then \
				${ECHO} "Unable to create $${subdir}."; \
				 exit 1; \
			 fi; \
			 ${ECHO} "Created directory $${subdir}"; \
	    	fi; \
	done

export_tree:
	.  ${PROD_BUILD_OUTPUT_DIR}/release/build_version.sh ; \
	${RM} -rf ${CUSTDIST_WORK_DIR} ;\
	mkdir ${CUSTDIST_WORK_DIR} ;\
	${RM} -rf ${FULL_BUILD_DIR} ;\
	mkdir ${FULL_BUILD_DIR} ;\
	if [ -z "${ADD_CVS_TAG}" -a -z "${USE_CVS_TAG}" ]; then \
		${PROD_TREE_ROOT}/src/release/customer-export.sh \
			-d "\"$${BUILD_DATE}\"" \
			-w ${CUSTDIST_WORK_DIR} -o ${FULL_BUILD_DIR} ; \
	elif [ ! -z "${ADD_CVS_TAG}" -a -z "${USE_CVS_TAG}" ]; then \
		${PROD_TREE_ROOT}/src/release/customer-export.sh \
			-d "\"$${BUILD_DATE}\"" -a ${ADD_CVS_TAG} \
			-w ${CUSTDIST_WORK_DIR} -o ${FULL_BUILD_DIR} ; \
	else \
		${PROD_TREE_ROOT}/src/release/customer-export.sh \
			-r ${USE_CVS_TAG} \
			-w ${CUSTDIST_WORK_DIR} -o ${FULL_BUILD_DIR} ; \
	fi ;\


# ============================== INSTALL ==============================

FULL_INSTALL_DIR=${PROD_INSTALL_OUTPUT_DIR}/custdist
ALL_INSTALL_DIRS=${FULL_INSTALL_DIR} ${FULL_INSTALL_DIR}/source \
		 ${FULL_INSTALL_DIR} ${FULL_INSTALL_DIR}/info
 
install: ensure_install_dirs
	(cd ${FULL_BUILD_DIR}; find . -name 'tms-export-src*' -type f -print0 | xargs -0 -i -r install -m 644 {} ${FULL_INSTALL_DIR}/source) 
	(cd ${FULL_BUILD_DIR}; find . -name 'tms-export-info*' -type f -print0 | xargs -0 -i -r install -m 644 {} ${FULL_INSTALL_DIR}/info) 

ensure_install_dirs:
	@for subdir in ${ALL_INSTALL_DIRS}; do \
		if test ! -d $${subdir}/; then \
			mkdir -p $${subdir}/; \
			if test ! -d $${subdir}/; then \
				${ECHO} "Unable to create $${subdir}."; \
				 exit 1; \
			 fi; \
			 ${ECHO} "Created directory $${subdir}"; \
	    	fi; \
	done

# ============================== RELEASE ==============================
FULL_RELEASE_DIR=${PROD_RELEASE_OUTPUT_DIR}/custdist
release: ensure_release_dir process_install distball distiso distinfo

ENCRYPT_FILE_COMMAND=gpg -q -z 1 -c --cipher-algo AES -o

process_install:
	${RM} -rf ${FULL_RELEASE_DIR}/stage
	mkdir -p ${FULL_RELEASE_DIR}/stage
	${RM} -f ${FULL_RELEASE_DIR}/stage/md5sums-orig
	(cd ${FULL_INSTALL_DIR}/source; find . -type f -print0 | xargs -0 -r ${MD5SUM} > ${FULL_RELEASE_DIR}/stage/md5sums-orig)
	cp ${PROD_TREE_ROOT}/README ${FULL_RELEASE_DIR}/stage
	if [ "${CUSTDIST_ENCRYPT}" -eq 1 ]; then \
		(cd ${FULL_INSTALL_DIR}/source; find . -type f \! -name '[A-Z\.]*' -print0 | xargs -0 -i -r ${ENCRYPT_FILE_COMMAND} ${FULL_RELEASE_DIR}/stage/`basename {}`.gpg {}) ;\
		(cd ${FULL_RELEASE_DIR}/stage; find . -type f \! -name 'md5sum*' -print0 | xargs -0 -r ${MD5SUM} > ${FULL_RELEASE_DIR}/stage/md5sums) ;\
		cat ${FULL_RELEASE_DIR}/stage/md5sums-orig ${FULL_RELEASE_DIR}/stage/md5sums > ${FULL_RELEASE_DIR}/stage/md5sums-all ;\
	else \
		(cd ${FULL_INSTALL_DIR}/source; find . -type f -print0 | xargs -0 -i -r install -m 644 {} ${FULL_RELEASE_DIR}/stage) ;\
		mv ${FULL_RELEASE_DIR}/stage/md5sums-orig ${FULL_RELEASE_DIR}/stage/md5sums ;\
	fi ;\


ensure_release_dir:
	@if test ! -d ${FULL_RELEASE_DIR}/; then \
		mkdir -p ${FULL_RELEASE_DIR}; \
		if test ! -d ${FULL_RELEASE_DIR}/; then \
			${ECHO} "Unable to create ${FULL_RELEASE_DIR}."; \
			 exit 1; \
		fi; \
		${ECHO} "Created directory ${FULL_RELEASE_DIR}"; \
	 fi

distball:
	@rm -rf ${FULL_RELEASE_DIR}/custdistball
	mkdir -m 755 -p ${FULL_RELEASE_DIR}/custdistball
	@. ${PROD_BUILD_OUTPUT_DIR}/release/build_version.sh ; \
	DISTBALL_ROOT_NAME=custdist-`echo $${BUILD_DATE} | sed 's/[-:]//g' | sed 's/ /-/g'`; \
	DISTBALL_COMPRESSED_FULL_NAME=$${DISTBALL_ROOT_NAME}.tgz; \
	echo "Making dist tarball into ${FULL_RELEASE_DIR}/custdistball/$${DISTBALL_COMPRESSED_FULL_NAME}" ;\
        (cd ${FULL_RELEASE_DIR}/stage; tar -czf ${FULL_RELEASE_DIR}/custdistball/$${DISTBALL_COMPRESSED_FULL_NAME} .);\

distinfo:
	@rm -rf ${FULL_RELEASE_DIR}/custdistinfo
	mkdir -m 755 -p ${FULL_RELEASE_DIR}/custdistinfo
	(cd ${FULL_INSTALL_DIR}/info; find . -name 'tms-export-info*' -type f -print0 | xargs -0 -i -r install -m 644 {} ${FULL_RELEASE_DIR}/custdistinfo)

distiso:
	@rm -rf ${FULL_RELEASE_DIR}/custdistiso
	mkdir -m 755 -p ${FULL_RELEASE_DIR}/custdistiso
	@. ${PROD_BUILD_OUTPUT_DIR}/release/build_version.sh ; \
	DISTISO_ROOT_NAME=custdist-`echo $${BUILD_DATE} | sed 's/[-:]//g' | sed 's/ /-/g'`; \
	DISTISO_FULL_NAME=$${DISTISO_ROOT_NAME}.iso; \
	echo "Making dist iso into ${FULL_RELEASE_DIR}/custdistiso/$${DISTISO_FULL_NAME}" ;\
        (cd ${FULL_RELEASE_DIR}/stage; mkisofs -quiet -R -J -o ${FULL_RELEASE_DIR}/custdistiso/$${DISTISO_FULL_NAME} .);\


# ============================== CLEAN ==============================
clean:
	${RM} ${CLEANFILES} 

# ============================== DEPEND ==============================
depend:

# ============================== REALCLEAN ==============================
realclean: clean
