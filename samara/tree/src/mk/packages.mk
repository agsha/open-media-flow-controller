#
#
#
#

PACKAGES_BINARY_RPMS?=
PACKAGES_BINARY_RPMS_CHECK?=1
PACKAGES_SOURCE_RPM?=
PACKAGES_SOURCE_RPM_CHECK?=1
FULL_BUILD_DIR?=${PROD_BUILD_OUTPUT_DIR}/image/packages
FULL_INSTALL_DIR?=${PROD_INSTALL_OUTPUT_DIR}/image
FULL_INSTALL_DIR_EXTRA?=
CONFIG_FILES?=pkg_config.pl

CHECK_BINARY_RPMS_FILE?=pkg_bin_check.txt
CHECK_SOURCE_RPM_FILE?=pkg_src_check.txt
PACKAGES_SOURCE_RPM?=must_set_source_rpm_manually
PACKAGES_SOURCE_RPM_DIR?=common
PACKAGES_SOURCE_RPM_SPEC_FILE?=must_set_spec_file_manually
PACKAGES_SOURCE_RPM_BUILD_ARGS?=
PACKAGES_SOURCE_RPM_PRE_PREP_RUN?=
PACKAGES_SOURCE_RPM_PRE_PREP_PATCHES?=
PACKAGES_SOURCE_RPM_PRE_PREP_PATCHES_APPLY_SUBDIR?=
PACKAGES_SOURCE_RPM_PRE_PREP_PATCHES_ARGS?=-p1 -f
PACKAGES_SOURCE_RPM_PRE_PREP_OVERLAYS?=
PACKAGES_SOURCE_RPM_POST_PREP_RUN?=
PACKAGES_SOURCE_RPM_POST_PREP_ARGS?=
PACKAGES_SOURCE_RPM_PATCHES?=
PACKAGES_SOURCE_RPM_PATCHES_APPLY_SUBDIR?=
PACKAGES_SOURCE_RPM_PATCHES_ARGS?=-p1 -f
PACKAGES_SOURCE_RPM_RPMBUILD?=0
PACKAGES_SOURCE_RPM_RPMBUILD_PRECMDS?=
PACKAGES_SOURCE_RPM_RPMBUILD_ARGS?=
PACKAGES_SOURCE_RPM_RPMBUILD_RPM?=
PACKAGES_BASEEXCLUDE_DISABLE?=0

PATCH?=/usr/bin/patch

COMMON_RULES_OVERRIDE=1

HOST_BIN_LFI=${PROD_BUILD_OUTPUT_DIR}/host/${BIN_DIR}/lfi/lfi

-include ${PROD_TREE_ROOT}/src/mk/common.mk

PACKAGES_BINARY_PATH=${FULL_BUILD_DIR}/binary_rpms
PACKAGES_BINARY_RPMS_FINAL=$(sort ${PACKAGES_BINARY_RPMS})
PACKAGES_BINARY_RPMS_DUP=$(addprefix ${PACKAGES_BINARY_PATH}/,$(notdir ${PACKAGES_BINARY_RPMS_FINAL}))

PACKAGES_BINARY_OUTPUT_PATH=${FULL_BUILD_DIR}/binary_output
PACKAGES_BINARY_OUTPUT_BALLS=$(addprefix ${PACKAGES_BINARY_OUTPUT_PATH}/,$(patsubst %.rpm,%.tar,$(notdir ${PACKAGES_BINARY_RPMS_FINAL})))

CHECK_BINARY_RPMS_FILE_FINAL=${PACKAGES_BINARY_OUTPUT_PATH}/.check_binary_rpms.txt
CHECK_BINARY_RPMS_MATCH_FILE=${PACKAGES_BINARY_OUTPUT_PATH}/.check_binary_rpms_match.txt
CHECK_BINARY_RPMS_OUTPUT=${PACKAGES_BINARY_OUTPUT_PATH}/.output_check_binary_rpms.txt

PACKAGES_SOURCE_PATH=${FULL_BUILD_DIR}/source_rpm
PACKAGES_SOURCE_RPM_DUP=$(addprefix ${PACKAGES_SOURCE_PATH}/,$(notdir ${PACKAGES_SOURCE_RPM}))

PACKAGES_SOURCE_OUTPUT_PATH=${FULL_BUILD_DIR}/source_output/${PACKAGES_SOURCE_RPM_DIR}

ifneq (${PACKAGES_SOURCE_RPM_RPMBUILD_RPM},)
PACKAGES_SOURCE_OUTPUT_BALL=$(addprefix ${PACKAGES_SOURCE_OUTPUT_PATH}/,$(patsubst %.rpm,%.tar,$(notdir ${PACKAGES_SOURCE_RPM_RPMBUILD_RPM})))
else
PACKAGES_SOURCE_OUTPUT_BALL=
endif

CHECK_SOURCE_RPM_FILE_FINAL=${PACKAGES_SOURCE_OUTPUT_PATH}/.check_source_rpm.txt
CHECK_SOURCE_RPM_MATCH_FILE=${PACKAGES_SOURCE_OUTPUT_PATH}/.check_source_rpm_match.txt
CHECK_SOURCE_RPM_OUTPUT=${PACKAGES_SOURCE_OUTPUT_PATH}/.output_check_source_rpm.txt

DONE_BINARY_RPMS_DUP_FILE=${PACKAGES_BINARY_PATH}/.done_binary_dups-${PROD_TARGET_PLATFORM_LC}-${PROD_TARGET_PLATFORM_VERSION_LC}-${PROD_TARGET_ARCH_LC}

DONE_SOURCE_RPM_DUP_FILE=${PACKAGES_SOURCE_PATH}/.done_source_dups-${PROD_TARGET_PLATFORM_LC}-${PROD_TARGET_PLATFORM_VERSION_LC}-${PROD_TARGET_ARCH_LC}

BUILD_DIRS=\
	${FULL_BUILD_DIR} \
	${PACKAGES_BINARY_PATH} \
	${PACKAGES_SOURCE_PATH} \
	${PACKAGES_BINARY_OUTPUT_PATH} \
	${PACKAGES_SOURCE_OUTPUT_PATH} \


.SUFFIXES:
.PHONY: ensure_build_dirs prepare_binary_rpms prepare_source_rpm \
	check_binary_rpms check_source_rpm \
	convert_binary_rpms convert_source_rpm \
	host_bin_lfi

.DEFAULT_GOAL := all

# This makefile template is not "-j" safe.  Do not remove the following line
.NOTPARALLEL:

# ============================== ALL ==============================
all: startup ensure_build_dirs \
	prepare_binary_rpms prepare_source_rpm  \
	check_binary_rpms check_source_rpm \
	convert_binary_rpms convert_source_rpm \
	all-recursive \
	shutdown

host_bin_lfi:
	${MAKE} -C ${PROD_TREE_ROOT}/src/bin/lfi

prepare_binary_rpms: ${DONE_BINARY_RPMS_DUP_FILE}

${PACKAGES_BINARY_RPMS_DUP} ${DONE_BINARY_RPMS_DUP_FILE}: Makefile ${CONFIG_FILES}
	@echo "Duplicating binary rpms from distributions"
	@echo PROD_DIST_ROOT=${PROD_DIST_ROOT}
	@echo PROD_LINUX_ROOT=${PROD_LINUX_ROOT}
	@rm -f ${DONE_BINARY_RPMS_DUP_FILE}
	@rm -f ${PACKAGES_BINARY_PATH}/.done_binary_dups-*
	@rm -f ${PACKAGES_BINARY_PATH}/*.rpm
	@fail=0 ; \
	for rpm in ${PACKAGES_BINARY_RPMS_FINAL}; do \
		rpm_fn=`basename $${rpm}` ; \
		rm -f ${PACKAGES_BINARY_PATH}/$${rpm_fn} ; \
		rm -f ${PACKAGES_BINARY_OUTPUT_PATH}/`echo $${rpm_fn} | sed 's,.rpm,.tar,'`; \
		if [ ! -r $${rpm} ]; then \
			echo "ERROR: missing rpm $${rpm}" ; \
			fail=1 ; \
			continue ; \
		fi ; \
		echo "Copying in $${rpm_fn}" ; \
		cp -p $${rpm} ${PACKAGES_BINARY_PATH}/$${rpm_fn} || fail=1 ; \
	done ; \
	if [ $$fail -ne 0 ]; then \
		exit 1 ;\
	fi
	@touch $@

prepare_source_rpm: ${DONE_SOURCE_RPM_DUP_FILE}

${PACKAGES_SOURCE_RPM_DUP} ${DONE_SOURCE_RPM_DUP_FILE}: Makefile ${CONFIG_FILES}
	@echo PROD_DIST_ROOT=${PROD_DIST_ROOT}
	@echo PROD_LINUX_ROOT=${PROD_LINUX_ROOT}
	@echo "Duplicating rpms from distributions"
	@rm -f ${DONE_SOURCE_RPM_DUP_FILE}
	@rm -f ${PACKAGES_SOURCE_PATH}/.done_source_dups-*
	@rm -f ${PACKAGES_SOURCE_PATH}/*.rpm
	@for rpm in ${PACKAGES_SOURCE_RPM}; do \
		rpm_fn=`basename $${rpm}` ; \
		rm -f ${PACKAGES_SOURCE_PATH}/$${rpm_fn} ; \
		rm -f ${PACKAGES_SOURCE_OUTPUT_PATH}/`echo $${rpm_fn} | sed 's,.rpm,.tar,'`; \
		if [ ! -r $${rpm} ]; then \
			echo "ERROR: missing rpm $${rpm}" ; \
			exit 1; \
		fi ; \
		echo "Copying in $${rpm_fn}" ; \
		cp -p $${rpm} ${PACKAGES_SOURCE_PATH}/$${rpm_fn} || exit 1; \
	done
	@touch $@

check_binary_rpms: host_bin_lfi
	@if [ -z "${PACKAGES_BINARY_RPMS_FINAL}" -o "${PACKAGES_BINARY_RPMS_CHECK}" != "1" ]; then \
		exit 0; \
	fi; \
	if [ -z "${CHECK_BINARY_RPMS_FILE}" -o ! -r "${CHECK_BINARY_RPMS_FILE}" ]; then \
	    echo "ERROR: no CHECK_BINARY_RPMS_FILE found"; \
	    exit 1; \
	fi; \
	echo "Verifying binary rpms in: ${PACKAGES_BINARY_PATH}" ; \
	rm -f ${CHECK_BINARY_RPMS_FILE_FINAL} ; \
	rm -f ${CHECK_BINARY_RPMS_MATCH_FILE} ; \
	echo $(notdir ${PACKAGES_BINARY_RPMS_FINAL}) | tr ' ' '\n' | sort -n > ${CHECK_BINARY_RPMS_MATCH_FILE} ; \
	cat ${CHECK_BINARY_RPMS_FILE} | grep -F -f ${CHECK_BINARY_RPMS_MATCH_FILE} - | sort -k 14 > ${CHECK_BINARY_RPMS_FILE_FINAL} ; \
	( cd ${PACKAGES_BINARY_PATH}; 	${HOST_BIN_LFI} -tmnug -c ${CHECK_BINARY_RPMS_FILE_FINAL}) || exit 1 ; \
	PACKAGES_FOUND=`(cd ${PACKAGES_BINARY_PATH}; find . -type f -name '[^.]*' | sort -n)` ; \
	NUM_FOUND=`echo "$${PACKAGES_FOUND}" | wc -l | awk '{print $$1}'` ; \
	NUM_EXPECTED=`wc -l ${CHECK_BINARY_RPMS_FILE_FINAL} | awk '{print $$1}'` ; \
	if [ "$${NUM_FOUND}" -ne "$${NUM_EXPECTED}" ]; then \
		echo "*** Error: expected $${NUM_EXPECTED} packages, but found $${NUM_FOUND}" ; \
		DIFF_FILE1=${PACKAGES_BINARY_OUTPUT_PATH}/.diff1.$$$$ ; \
		echo "$${PACKAGES_FOUND}" > $${DIFF_FILE1} ; \
		DIFF_FILE2=${PACKAGES_BINARY_OUTPUT_PATH}/.diff2.$$$$ ; \
		cat ${CHECK_BINARY_RPMS_FILE_FINAL} | awk '{print $$14}' > $${DIFF_FILE2} ; \
		diff $${DIFF_FILE1} $${DIFF_FILE2} ; \
		rm $${DIFF_FILE1} $${DIFF_FILE2} ; \
		exit 1 ; \
	fi ; \

check_source_rpm: host_bin_lfi
	@echo RULE check_source_rpm in tree/src/mk/packages.mk
	@echo PACKAGES_SOURCE_RPM=${PACKAGES_SOURCE_RPM}
	@echo CHECK_SOURCE_RPM_FILE=${CHECK_SOURCE_RPM_FILE}
	@if [ -z "${PACKAGES_SOURCE_RPM}" -o "${PACKAGES_SOURCE_RPM_CHECK}" != "1" ]; then \
		exit 0; \
	fi; \
	if [ -z "${CHECK_SOURCE_RPM_FILE}" -o ! -r "${CHECK_SOURCE_RPM_FILE}" ]; then \
	    echo "ERROR: no CHECK_SOURCE_RPM_FILE (${CHECK_SOURCE_RPM_FILE}) found"; \
	    exit 1; \
	fi; \
	echo "Verifying source rpm in: ${PACKAGES_SOURCE_PATH}" ; \
	rm -f ${CHECK_SOURCE_RPM_FILE_FINAL} ; \
	rm -f ${CHECK_SOURCE_RPM_MATCH_FILE} ; \
	echo $(notdir ${PACKAGES_SOURCE_RPM}) | tr ' ' '\n' > ${CHECK_SOURCE_RPM_MATCH_FILE} ; \
	cat ${CHECK_SOURCE_RPM_FILE} | grep -F -f ${CHECK_SOURCE_RPM_MATCH_FILE} - > ${CHECK_SOURCE_RPM_FILE_FINAL} ; \
	echo =========== ; \
	echo ${CHECK_SOURCE_RPM_MATCH_FILE} ; \
	cat ${CHECK_SOURCE_RPM_MATCH_FILE} ; \
	echo =========== ; \
	echo ${CHECK_SOURCE_RPM_FILE_FINAL} ; \
	cat ${CHECK_SOURCE_RPM_FILE_FINAL} ; \
	echo =========== ; \
	echo "cd ${PACKAGES_SOURCE_PATH}; ${HOST_BIN_LFI} -tmnug -c ${CHECK_SOURCE_RPM_FILE_FINAL}" ; \
	( cd ${PACKAGES_SOURCE_PATH}; 	${HOST_BIN_LFI} -tmnug -c ${CHECK_SOURCE_RPM_FILE_FINAL}) || exit 1 ; \
	NUM_FOUND=`find ${PACKAGES_SOURCE_PATH} -type f -name '[^.]*' | wc -l | awk '{print $$1}'` ; \
	NUM_EXPECTED=`wc -l ${CHECK_SOURCE_RPM_FILE_FINAL} | awk '{print $$1}'` ; \
	if [ "$${NUM_FOUND}" -ne "$${NUM_EXPECTED}" ]; then \
		echo "*** Error: expected $${NUM_EXPECTED} packages, but found $${NUM_FOUND}" ; \
		exit 1 ; \
	fi ; \

convert_binary_rpms:
	@echo RULE convert_binary_rpms in tree/src/mk/packages.mk
	@if [ -z "${PACKAGES_BINARY_RPMS_FINAL}" ]; then \
		exit 0; \
	else \
	    echo "Tar ball output directory is ${PACKAGES_BINARY_OUTPUT_PATH}"; \
	fi; \
	if [ `id -u` != 0 ]; then \
		if [ -z "${PACKAGES_ROOT_OVERRIDE}" ]; then \
		    echo "ERROR: must be run as root or permission will be incorrect"; \
		    exit 1; \
		else \
		    echo "WARNING: package file permission will be incorrect"; \
		    UNRPM_FLAGS="-r" ; \
		fi ;\
	fi ; \
	if [ ! -z "${PACKAGES_BASEEXCLUDE_DISABLE}" -a "${PACKAGES_BASEEXCLUDE_DISABLE}" -eq 1 ]; then \
	    UNRPM_FLAGS="$${UNRPM_FLAGS} -b" ; \
	fi ; \
	UNRPM_FLAGS="$${UNRPM_FLAGS} -s ${STRIP}" ; \
	for rpm in ${PACKAGES_BINARY_RPMS_FINAL}; do \
		rpm_fn=`basename $${rpm}` ; \
		ball_fn=`echo $${rpm_fn} | sed 's,.rpm,.tar,'` ; \
		if test ! -r ${PACKAGES_BINARY_OUTPUT_PATH}/$${ball_fn} \
			-o ${PACKAGES_BINARY_PATH}/$${rpm_fn} -nt ${PACKAGES_BINARY_OUTPUT_PATH}/$${ball_fn} ; then \
			rm -f ${PACKAGES_BINARY_OUTPUT_PATH}/$${ball_fn} ; \
			cmd="${PROD_TREE_ROOT}/src/release/unrpm.pl $${UNRPM_FLAGS} convert_single ${PACKAGES_BINARY_PATH}/$${rpm_fn} ${PACKAGES_BINARY_OUTPUT_PATH} ${CONFIG_FILES}"; \
			if test ! "${PROD_BUILD_VERBOSE}" -eq 1; then \
				echo "Creating tar ball from binary rpm for $${rpm_fn}"; \
			else \
				echo "$${cmd}"; \
			fi ; \
			$${cmd} || exit 1 ; \
		fi ; \
	done

convert_source_rpm:
	@if [ -z "${PACKAGES_SOURCE_RPM}" ]; then \
		exit 0; \
	fi; \
	echo RULE: convert_source_rpm in tree/src/mk/packages.mk ; \
	echo PACKAGES_SOURCE_OUTPUT_PATH=${PACKAGES_SOURCE_OUTPUT_PATH} ; \
	rpm=${PACKAGES_SOURCE_RPM} ; \
	rpm_fn=`basename $${rpm}` ; \
	tmppath=`mktemp -d -t -p /var/tmp "rpm-tmp.XXXXXXXX"` ; \
	unset MAKEFLAGS MFLAGS ;\
	echo Test for ${PACKAGES_SOURCE_OUTPUT_PATH}/.done_source_extract ; \
	if [ ! -r ${PACKAGES_SOURCE_OUTPUT_PATH}/.done_source_extract ]; then \
		rm -f ${PACKAGES_SOURCE_OUTPUT_PATH}/.done_source_preprep_patch ; \
		rm -f ${PACKAGES_SOURCE_OUTPUT_PATH}/.done_source_extract ; \
		rm -f ${PACKAGES_SOURCE_OUTPUT_PATH}/.done_source_patch ; \
		rm -f ${PACKAGES_SOURCE_OUTPUT_PATH}/.done_source_rpmbuild ; \
		rm -rf ${PACKAGES_SOURCE_OUTPUT_PATH}/BUILD ; \
		rm -rf ${PACKAGES_SOURCE_OUTPUT_PATH}/RPMS ; \
		rm -rf ${PACKAGES_SOURCE_OUTPUT_PATH}/SOURCES ; \
		rm -rf ${PACKAGES_SOURCE_OUTPUT_PATH}/SPECS ; \
		rm -rf ${PACKAGES_SOURCE_OUTPUT_PATH}/SRPMS ; \
		if [ ! -z "${PACKAGES_SOURCE_OUTPUT_BALL}" ]; then \
			rm -f "${PACKAGES_SOURCE_OUTPUT_BALL}" ; \
		fi ; \
		mkdir -p ${PACKAGES_SOURCE_OUTPUT_PATH}/BUILD ; \
		mkdir -p ${PACKAGES_SOURCE_OUTPUT_PATH}/RPMS ; \
		mkdir -p ${PACKAGES_SOURCE_OUTPUT_PATH}/SOURCES ; \
		mkdir -p ${PACKAGES_SOURCE_OUTPUT_PATH}/SPECS ; \
		mkdir -p ${PACKAGES_SOURCE_OUTPUT_PATH}/SRPMS ; \
		cmd="${PROD_TREE_ROOT}/src/release/unrpm.pl -r -b extract_single ${PACKAGES_SOURCE_PATH}/$${rpm_fn} ${PACKAGES_SOURCE_OUTPUT_PATH}/SOURCES ${CONFIG_FILES}"; \
		if test ! "${PROD_BUILD_VERBOSE}" -eq 1; then \
			echo "Extracting raw sources from source rpm for $${rpm_fn}"; \
		else \
			echo "$${cmd}"; \
		fi ; \
		$${cmd} || exit 1; \
		cp -p ${PACKAGES_SOURCE_OUTPUT_PATH}/SOURCES/${PACKAGES_SOURCE_RPM_SPEC_FILE} ${PACKAGES_SOURCE_OUTPUT_PATH}/SPECS ; \
		if [ ! -r "${PACKAGES_SOURCE_OUTPUT_PATH}/.done_source_preprep" ]; then \
			patch_apply_dir="${PACKAGES_SOURCE_OUTPUT_PATH}/${PACKAGES_SOURCE_RPM_PRE_PREP_PATCHES_APPLY_SUBDIR}" ; \
			pwd ; \
			echo "PACKAGES_SOURCE_RPM_PRE_PREP_PATCHES=${PACKAGES_SOURCE_RPM_PRE_PREP_PATCHES}" ; \
			for patch in ${PACKAGES_SOURCE_RPM_PRE_PREP_PATCHES}; do \
				cmd="${PATCH} ${PACKAGES_SOURCE_RPM_PRE_PREP_PATCHES_ARGS} -N -s -d $${patch_apply_dir} -i $${patch}"; \
				if test ! "${PROD_BUILD_VERBOSE}" -eq 1; then \
					echo "Patching (pre prep) sources with `basename $${patch}`"; \
					echo "$${cmd}"; \
				else \
					echo "$${cmd}"; \
				fi ; \
				$${cmd} || exit 1 ; \
			done ; \
			if [ ! -z "${PACKAGES_SOURCE_RPM_PRE_PREP_RUN}" \
			     -a -x "${PACKAGES_SOURCE_RPM_PRE_PREP_RUN}" ]; then \
				echo "Running pre prep script ${PACKAGES_SOURCE_RPM_PRE_PREP_RUN}" ; \
				(cd ${PACKAGES_SOURCE_OUTPUT_PATH} && "${PACKAGES_SOURCE_RPM_PRE_PREP_RUN}") || exit 1 ; \
			fi ; \
			for olf in ${PACKAGES_SOURCE_RPM_PRE_PREP_OVERLAYS}; do \
				cmd="cp -p $${olf} ${PACKAGES_SOURCE_OUTPUT_PATH}/SOURCES"; \
				if test ! "${PROD_BUILD_VERBOSE}" -eq 1; then \
					echo "Copying in (pre prep) source from `basename $${olf}`"; \
				else \
					echo "$${cmd}"; \
				fi ; \
				$${cmd} || exit 1 ; \
			done ; \
			touch ${PACKAGES_SOURCE_OUTPUT_PATH}/.done_source_preprep ; \
		fi ; \
		echo "Preparing sources for $${rpm_fn}" ; \
		${PACKAGES_SOURCE_RPM_RPMBUILD_PRECMDS} rpmbuild --define '_topdir '${PACKAGES_SOURCE_OUTPUT_PATH} --define '_tmppath '$${tmppath} ${PACKAGES_SOURCE_OUTPUT_PATH}/SPECS/${PACKAGES_SOURCE_RPM_SPEC_FILE} -bp ${PACKAGES_SOURCE_RPM_BUILD_ARGS}  || exit 1; \
		echo "Done preparing sources for $${rpm_fn}" ; \
		if [ ! -z "${PACKAGES_SOURCE_RPM_POST_PREP_RUN}" \
		     -a -x "${PACKAGES_SOURCE_RPM_POST_PREP_RUN}" ]; then \
			cmd="${PACKAGES_SOURCE_RPM_POST_PREP_RUN} ${PACKAGES_SOURCE_RPM_POST_PREP_RUN_ARGS}" ; \
			echo "Running post prep script $${cmd} in ${PACKAGES_SOURCE_OUTPUT_PATH}/BUILD" ; \
			(cd ${PACKAGES_SOURCE_OUTPUT_PATH}/BUILD && $${cmd} ) || exit 1 ; \
		fi ; \
		touch ${PACKAGES_SOURCE_OUTPUT_PATH}/.done_source_extract ; \
	fi ; \
	if [ ! -z "${PACKAGES_SOURCE_RPM_PATCHES}" -a ! -r "${PACKAGES_SOURCE_OUTPUT_PATH}/.done_source_patch" ]; then \
		rm -f ${PACKAGES_SOURCE_OUTPUT_PATH}/.done_source_rpmbuild ; \
		patch_apply_dir="${PACKAGES_SOURCE_OUTPUT_PATH}/BUILD/${PACKAGES_SOURCE_RPM_PATCHES_APPLY_SUBDIR}" ; \
		pwd ; \
		echo "PACKAGES_SOURCE_RPM_PATCHES=${PACKAGES_SOURCE_RPM_PATCHES}" ; \
		for patch in ${PACKAGES_SOURCE_RPM_PATCHES}; do \
			cmd="${PATCH} ${PACKAGES_SOURCE_RPM_PATCHES_ARGS} -N -s -d $${patch_apply_dir} -i $${patch}"; \
			if test ! "${PROD_BUILD_VERBOSE}" -eq 1; then \
				echo "Patching sources with `basename $${patch}`"; \
				echo "$${cmd}"; \
			else \
				echo "$${cmd}"; \
			fi ; \
			$${cmd} || exit 1 ; \
		done ; \
		touch ${PACKAGES_SOURCE_OUTPUT_PATH}/.done_source_patch ; \
	fi ; \
	if [ ! -z "${PACKAGES_SOURCE_RPM_RPMBUILD}" -a "${PACKAGES_SOURCE_RPM_RPMBUILD}" -eq 1 -a ! -r "${PACKAGES_SOURCE_OUTPUT_PATH}/.done_source_rpmbuild" ]; then \
		if [ `id -u` != 0 ]; then \
			if [ -z "${PACKAGES_ROOT_OVERRIDE}" ]; then \
			    echo "ERROR: must be run as root or permission will be incorrect"; \
			    exit 1; \
			else \
			    echo "WARNING: package file permission will be incorrect"; \
			    UNRPM_FLAGS="-r" ; \
			fi ;\
		fi ; \
		if [ ! -z "${PACKAGES_SOURCE_OUTPUT_BALL}" ]; then \
			rm -f "${PACKAGES_SOURCE_OUTPUT_BALL}" ; \
		fi ; \
		rm -f ${PACKAGES_SOURCE_OUTPUT_PATH}/.done_source_extract ; \
		if test ! "${PROD_BUILD_VERBOSE}" -eq 1; then \
			echo "Creating new SRPM after any patches for $${rpm_fn}"; \
		else \
			echo "${PACKAGES_SOURCE_RPM_RPMBUILD_PRECMDS} rpmbuild ${PACKAGES_SOURCE_RPM_RPMBUILD_ARGS}  --define '_topdir '${PACKAGES_SOURCE_OUTPUT_PATH} --define '_tmppath '$${tmppath} -bs ${PACKAGES_SOURCE_OUTPUT_PATH}/SPECS/${PACKAGES_SOURCE_RPM_SPEC_FILE}" ; \
		fi ; \
		${PACKAGES_SOURCE_RPM_RPMBUILD_PRECMDS} rpmbuild ${PACKAGES_SOURCE_RPM_RPMBUILD_ARGS}  --define '_topdir '${PACKAGES_SOURCE_OUTPUT_PATH} --define '_tmppath '$${tmppath} -bs ${PACKAGES_SOURCE_OUTPUT_PATH}/SPECS/${PACKAGES_SOURCE_RPM_SPEC_FILE} || exit 1 ; \
		if test ! "${PROD_BUILD_VERBOSE}" -eq 1; then \
			echo "Creating new RPM from new SRPM for $${rpm_fn}"; \
		else \
			echo "${PACKAGES_SOURCE_RPM_RPMBUILD_PRECMDS} rpmbuild ${PACKAGES_SOURCE_RPM_RPMBUILD_ARGS}  --define '_topdir '${PACKAGES_SOURCE_OUTPUT_PATH} --define '_tmppath '$${tmppath} --rebuild ${PACKAGES_SOURCE_OUTPUT_PATH}/SRPMS/*.src.rpm" ; \
		fi ; \
		${PACKAGES_SOURCE_RPM_RPMBUILD_PRECMDS} rpmbuild ${PACKAGES_SOURCE_RPM_RPMBUILD_ARGS}  --define '_topdir '${PACKAGES_SOURCE_OUTPUT_PATH} --define '_tmppath '$${tmppath} --rebuild ${PACKAGES_SOURCE_OUTPUT_PATH}/SRPMS/*.src.rpm || exit 1 ; \
		rpm_fn=`basename ${PACKAGES_SOURCE_RPM_RPMBUILD_RPM}` ; \
		ball_fn=`echo $${rpm_fn} | sed 's,.rpm,.tar,'` ; \
		rm -f ${PACKAGES_SOURCE_OUTPUT_PATH}/$${ball_fn} ; \
		cmd="${PROD_TREE_ROOT}/src/release/unrpm.pl $${UNRPM_FLAGS} convert_single ${PACKAGES_SOURCE_OUTPUT_PATH}/RPMS/${PACKAGES_SOURCE_RPM_RPMBUILD_RPM} ${PACKAGES_SOURCE_OUTPUT_PATH} ${PACKAGES_SOURCE_RPM_RPMBUILD_PKG_CONFIG_FILES}"; \
		if test ! "${PROD_BUILD_VERBOSE}" -eq 1; then \
			echo "Creating tar ball from source-rpm-generated binary rpm for $${rpm_fn}"; \
		else \
			echo "$${cmd}"; \
		fi ; \
		$${cmd} || exit 1 ; \
		rmdir $${tmppath} ; \
		touch ${PACKAGES_SOURCE_OUTPUT_PATH}/.done_source_extract ; \
		touch ${PACKAGES_SOURCE_OUTPUT_PATH}/.done_source_rpmbuild ; \
	fi ; \
	if [ -d "$${tmppath}" ]; then \
		rmdir $${tmppath} ; \
	fi ; \


startup:
	$(standard-startup)

shutdown:
	$(standard-shutdown)

ensure_build_dirs:
	@for subdir in ${BUILD_DIRS}; do \
		if test ! -d $${subdir}/; then \
			mkdir -m 755 -p $${subdir}/; \
			if test ! -d $${subdir}/; then \
				${ECHO} "Unable to create $${subdir}."; \
				 exit 1; \
			 fi; \
			 ${ECHO} "Created directory $${subdir}"; \
	    	fi; \
	done


CLEANFILES+= \
	${PACKAGES_BINARY_RPMS_DUP} \
	${PACKAGES_BINARY_OUTPUT_BALLS} \
	${DONE_BINARY_RPMS_DUP_FILE} \
	${CHECK_BINARY_RPMS_FILE_FINAL} \
	${CHECK_BINARY_RPMS_MATCH_FILE} \
	${CHECK_BINARY_RPMS_MATCH_FINAL} \
	${CHECK_BINARY_RPMS_OUTPUT} \
	${PACKAGES_SOURCE_RPM_DUP} \
	${PACKAGES_SOURCE_OUTPUT_BALL} \
	${DONE_SOURCE_RPM_DUP_FILE} \
	${CHECK_SOURCE_RPM_FILE_FINAL} \
	${CHECK_SOURCE_RPM_MATCH_FILE} \
	${CHECK_SOURCE_RPM_MATCH_FINAL} \

CLEANDIRS+= \
	${PACKAGES_SOURCE_OUTPUT_PATH} \


# ============================== INSTALL ==============================

install: ensure_install_dirs install-recursive
	@if [ ! \( -z "${PACKAGES_BINARY_RPMS_FINAL}" -a -z "${PACKAGES_SOURCE_OUTPUT_BALL}" \) -a `id -u` != 0 ]; then \
		if [ -z "${PACKAGES_ROOT_OVERRIDE}" ]; then \
		    echo "ERROR: must be run as root or permission will be incorrect"; \
		    exit 1; \
		else \
		    echo "WARNING: package file permission will be incorrect"; \
		fi ;\
	fi ; \
	all_output_balls='${PACKAGES_BINARY_OUTPUT_BALLS} ${PACKAGES_SOURCE_OUTPUT_BALL}'; \
        failcom='exit 1'; \
        for f in x $$MAKEFLAGS; do \
          case $$f in \
            *=* | --[!k]*);; \
            *k*) failcom='fail=yes';; \
          esac; \
        done; \
	for ball in $$all_output_balls; do \
		echo "Install by extracting binary rpm $${ball}"; \
		(cd ${FULL_INSTALL_DIR} && ${TAR} -xpf $${ball}) || eval $$failcom; \
		if [ ! -z "${FULL_INSTALL_DIR_EXTRA}" ]; then \
			(cd ${FULL_INSTALL_DIR_EXTRA} && ${TAR} -xpf $${ball}) || eval $$failcom; \
		fi; \
	done; \
	test -z "$$fail"

ensure_install_dirs:
	@for subdir in ${FULL_INSTALL_DIR} ${FULL_INSTALL_DIR_EXTRA} ; do \
		if test ! -d $${subdir}/; then \
			mkdir -m 755 -p $${subdir}; \
			if test ! -d $${subdir}/; then \
				${ECHO} "Unable to create $${subdir}."; \
				 exit 1; \
			 fi; \
			 ${ECHO} "Created directory $${subdir}"; \
		 fi; \
	done

# ============================== CLEAN ==============================
clean: clean-recursive
	${RM} ${CLEANFILES}
	${RM} -rf ${CLEANDIRS}

# ============================== DEPEND ==============================
depend: depend-recursive

# ============================== REALCLEAN ==============================
realclean: realclean-recursive clean

# ============================== CREATE-TRANSLATIONS =========================
i18n-create-catalogs: i18n-create-catalogs-recursive

# ============================== UPDATE-CATALOGS =========================
i18n-update-catalogs: i18n-update-catalogs-recursive

# Recursion
all-recursive install-recursive depend-recursive \
  clean-recursive realclean-recursive \
  i18n-create-catalogs-recursive \
  i18n-update-catalogs-recursive:
	$(recurse-subdirs)
	$(recurse-submakefiles)
