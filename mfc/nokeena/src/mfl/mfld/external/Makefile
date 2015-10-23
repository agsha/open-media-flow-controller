# Makefile
# Take nknlogd and some other files from the build output location and
# package it up as a separately installable app "Media Flow Logger" (MFL).
# E.g.:
# product-nokeena-x86_64/install/image/opt/nkn/sbin/nknlogd
# product-nokeena-x86_64/install/rootflop/etc/eula.txt

include ${PROD_TREE_ROOT}/src/mk/common.mk
include ../../Makefile.inc
# Makefile.inc sets:
# INSTALL_TREE=mfl-image
# PROD_INSTALL_PREFIX=mfl-inst

INST_ROOT=${PROD_OUTPUT_DIR}/install/${INSTALL_TREE}
OUTPUT_DIR=${INST_ROOT}/${PROD_INSTALL_PREFIX}
RELEASE_DIR=${PROD_OUTPUT_DIR}/release/${INSTALL_TREE}

BIN_OUT_DIR=${OUTPUT_DIR}/bin
INFO_OUT_DIR=${OUTPUT_DIR}/info
OTHER_OUT_DIR=${OUTPUT_DIR}/other

FROM_IMAGE_DIR=${PROD_OUTPUT_ROOT}/product-nokeena-x86_64/install/image
FROM_ROOTFLOP_DIR=${PROD_OUTPUT_ROOT}/product-nokeena-x86_64/install/rootflop

.PHONY: default
default:
	@echo Only the clean, install and release targets are useful

clean:
	rm -rf ${OUTPUT_DIR}
	rm -rf ${RELEASE_DIR}

.PHONY: install
install: \
  ${BIN_OUT_DIR}/mfld \
  ${INFO_OUT_DIR}/eula.txt \
  ${INFO_OUT_DIR}/version.sh \
  ${OTHER_OUT_DIR}/mfc-ver.txt \
  ${OTHER_OUT_DIR}/mfc-ver.sh
	@echo  Contents of ${OUTPUT_DIR}
	@ls -l ${OUTPUT_DIR}

# ###############################################
# Rule for copying files from $PROD_OUTPUT_ROOT

# Pick up these files from the image output dirs
NKNLOGD_FILE_FROM=${FROM_IMAGE_DIR}/opt/nkn/sbin/nknlogd
VER_TXT_FROM=${FROM_IMAGE_DIR}/etc/build_version.txt
VER_SH_FROM=${FROM_IMAGE_DIR}/etc/build_version.sh
EULA_FILE_FROM=${FROM_ROOTFLOP_DIR}/etc/eula.txt

${NKNLOGD_FILE_FROM}:
	echo Error, file does not exist: $@
	echo Normal build has not been done yet
	exit 1

${VER_TXT_FROM}:
	echo Error, file does not exist: $@
	echo Normal build has not been done yet
	exit 1

${VER_SH_FROM}:
	echo Error, file does not exist: $@
	echo Normal build has not been done yet
	exit 1

${EULA_FILE_FROM}:
	echo Error, file does not exist: $@
	echo Normal build has not been done yet
	exit 1


${BIN_OUT_DIR}/mfld: ${NKNLOGD_FILE_FROM} ${BIN_OUT_DIR}
	@echo Copy mfld
	@cp ${NKNLOGD_FILE_FROM} $@
	@touch $@
	@chown ${REAL_USER} $@
	@chmod 755 $@
	@ls -l $@

${INFO_OUT_DIR}/eula.txt: ${EULA_FILE_FROM} ${INFO_OUT_DIR}
	@echo Copy eula.txt
	@cp ${EULA_FILE_FROM} $@
	@touch $@
	@chown ${REAL_USER} $@
	@chmod 755 $@
	@ls -l $@

${INFO_OUT_DIR}/version.sh: ${VER_SH_FROM} ${OTHER_OUT_DIR}
	@echo Copy mfc-ver.sh
	@sh make_mfl_version.sh ${VER_SH_FROM} > $@;
	@chown ${REAL_USER} $@
	@chmod 755 $@
	@ls -l $@
	@cat $@

${OTHER_OUT_DIR}/mfc-ver.txt: ${VER_TXT_FROM} ${OTHER_OUT_DIR}
	@echo Copy mfc-ver.txt
	@cp ${VER_TXT_FROM} $@
	@touch $@
	@chown ${REAL_USER} $@
	@chmod 755 $@
	@ls -l $@

${OTHER_OUT_DIR}/mfc-ver.sh: ${VER_SH_FROM} ${OTHER_OUT_DIR}
	@echo Copy mfc-ver.sh
	@cp ${VER_SH_FROM} $@
	@touch $@
	@chown ${REAL_USER} $@
	@chmod 755 $@
	@ls -l $@


# ###############################################
# Rules for making output directories

${BIN_OUT_DIR}:
	mkdir -p $@
	@chown ${REAL_USER} $@

${INFO_OUT_DIR}:
	mkdir -p $@
	@chown ${REAL_USER} $@

${OTHER_OUT_DIR}:
	mkdir -p $@
	@chown ${REAL_USER} $@

${RELEASE_DIR}:
	mkdir -p $@
	@chown ${REAL_USER} $@


.PHONY: release
release: install ${RELEASE_DIR}
	if [ ! -f  ${VER_SH_FROM} ] ; then\
	  echo Cannot create MFL install tar file ;\
	  echo Missing output file ${VER_SH_FROM} ;\
	  exit 1 ;\
	fi
	set -x ;\
	.  ${VER_SH_FROM} ;\
	build_suffix=$${BUILD_PROD_NAME}-$${BUILD_TARGET_ARCH_LC}$${BUILD_TARGET_HWNAME_LC:+-$${BUILD_TARGET_HWNAME_LC}}-`echo $${BUILD_DATE} | sed 's/[-:]//g' | sed 's/ /-/g'` ;\
	echo BUILD_CUSTOMER_SUFFIX=$${BUILD_CUSTOMER_SUFFIX} ;\
	test -n "$${BUILD_CUSTOMER_SUFFIX}" && build_suffix=$${BUILD_CUSTOMER_SUFFIX} ;\
	export TGZ_FILE=${RELEASE_DIR}/mfl-$${build_suffix}.tgz ;\
	echo build_suffix=$${build_suffix} ;\
	echo TGZ_FILE=$${TGZ_FILE} ;\
	cd ${INST_ROOT} ;\
	tar -zcvf $${TGZ_FILE} ${PROD_INSTALL_PREFIX} ;\
	ls -l $${TGZ_FILE}

.PHONY: env
env:
	set
