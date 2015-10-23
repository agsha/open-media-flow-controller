# Makefile to get external files into the package

include ${PROD_TREE_ROOT}/src/mk/common.mk

TOP_BUILD_DIR=${PROD_OUTPUT_DIR}/build
TOP_INSTALL_DIR=${PROD_OUTPUT_DIR}/install
TOP_RELEASE_DIR=${PROD_OUTPUT_DIR}/release

EXTERNAL_PKG_DIR=${TOP_BUILD_DIR}/externalpkg
IMAGE_RELEASE_DIR=${PROD_OUTPUT_DIR}/release/image

DMI_INSTALL_DIR=${PROD_OUTPUT_DIR}/install/dmi
DMI_RELEASE_DIR=${PROD_OUTPUT_DIR}/release/dmi


.PHONY: default
default:
	echo Nothing to do for default

#  ${EXTERNAL_PKG_DIR}/jcf-gw/pkg.info \


#${EXTERNAL_PKG_DIR}/jcf-gw/pkg.info: jcf-gw.info ${EXTERNAL_PKG_DIR}/jcf-gw
#	@echo $@
#	./download_pkg.sh jcf-gw.info ${EXTERNAL_PKG_DIR}/jcf-gw

.PHONY: clean
clean:
	rm -rf ${EXTERNAL_PKG_DIR}
	rm -rf ${DMI_INSTALL_DIR}
	rm -rf ${DMI_RELEASE_DIR}

.PHONY: install

#install: ${INSTALL_DIR}/image
#	./install_pkg.sh ${EXTERNAL_PKG_DIR} ${TOP_INSTALL_DIR}/image

.PHONY: release
release: \
  ${DMI_RELEASE_DIR}/pre_img_install.sh \


${DMI_RELEASE_DIR}/pre_img_install.sh: ${DMI_RELEASE_DIR}
	echo make release in the nokeena/src/external directory
	MY_DIR=`pwd` ;\
	. ${PROD_CUSTOMER_ROOT}/${PROD_PRODUCT}/src/mk/build_settings.dot ;\
	if [ ! -z "$${DMI_PKG}" ] ; then \
	  PKG_FILE=/data/archive/builds/dmi/$${DMI_PKG}.img.zip ;\
	  if [ -f "$${PKG_FILE}" ] ; then \
	    cp -a $${PKG_FILE} ${DMI_RELEASE_DIR} ;\
	    cp -a pre_img_install.sh ${DMI_RELEASE_DIR} ;\
	    echo cd ${DMI_RELEASE_DIR} ;\
	    cd ${DMI_RELEASE_DIR} ;\
	    $${MY_DIR}/insert_file.sh ${IMAGE_RELEASE_DIR}/image-mfc-*.img $${DMI_PKG}.img.zip pre_img_install.sh ;\
	  fi ;\
	fi
	ls -l $${PKG_FILE} ${IMAGE_RELEASE_DIR}/image-mfc-*.img

${EXTERNAL_PKG_DIR}:
	mkdir -p $@
	@chown ${REAL_USER} $@

#${EXTERNAL_PKG_DIR}/jcf-gw: ${EXTERNAL_PKG_DIR}
#	mkdir -p $@
#	@chown ${REAL_USER} $@

${INSTALL_DIR}/image:
	mkdir -p $@
	@chown ${REAL_USER} $@

${DMI_INSTALL_DIR}:
	mkdir -p $@
	@chown ${REAL_USER} $@

${DMI_RELEASE_DIR}:
	mkdir -p $@
	@chown ${REAL_USER} $@

.PHONY: env
env:
	set
