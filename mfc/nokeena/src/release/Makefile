# Makefile

include ${PROD_TREE_ROOT}/src/mk/common.mk

RELEASE_DIR=${PROD_OUTPUT_DIR}/release
INSTALL_DIR=${PROD_OUTPUT_DIR}/install

.PHONY: default
default:
	@echo Only the clean and release targets are useful

.PHONY: clean
clean:
	rm -rf ${RELEASE_DIR}/mkmfg
	rm -rf ${RELEASE_DIR}/pacifica_TMP
	rm -rf ${RELEASE_DIR}/image/as-mlc-*.img
	$(MAKE) -C junos_packaging clean

${RELEASE_DIR}:
	mkdir -p $@
	@chown ${REAL_USER} $@

${RELEASE_DIR}/mkmfg: ${RELEASE_DIR}
	mkdir -p $@
	@chown ${REAL_USER} $@

${RELEASE_DIR}/image: ${RELEASE_DIR}
	mkdir -p $@
	@chown ${REAL_USER} $@

${RELEASE_DIR}/mkmfg/mkmfg_graft.sh: mkmfg_graft.sh ${RELEASE_DIR}/mkmfg
	@echo Copy mkmfg_graft.sh
	@cp mkmfg_graft.sh $@
	@touch $@
	@chown ${REAL_USER} $@
	@chmod 755 $@
	@ls -l $@

${RELEASE_DIR}/mkmfg/eula.txt: ${INSTALL_DIR}/rootflop/etc/eula.txt ${RELEASE_DIR}/mkmfg
	@echo Copy eula.txt
	@cp ${INSTALL_DIR}/rootflop/etc/eula.txt $@
	@touch $@
	@chown ${REAL_USER} $@
	@chmod 755 $@
	@ls -l $@

${RELEASE_DIR}/mkmfg/eula.pdf: ${INSTALL_DIR}/rootflop/etc/eula.pdf ${RELEASE_DIR}/mkmfg
	@echo Copy eula.pdf
	@cp ${INSTALL_DIR}/rootflop/etc/eula.pdf $@
	@touch $@
	@chown ${REAL_USER} $@
	@chmod 755 $@
	@ls -l $@

${RELEASE_DIR}/image/force.img: update_image_for_pacifica.sh ${RELEASE_DIR}/image
	@echo Update the image for Pacifica
	./update_image_for_pacifica.sh -d ${RELEASE_DIR}/pacifica_TMP ${RELEASE_DIR}/image/image*.img

.PHONY: release
release: ${RELEASE_DIR}/mkmfg/mkmfg_graft.sh \
  ${RELEASE_DIR}/mkmfg/eula.txt \
  ${RELEASE_DIR}/mkmfg/eula.pdf \
  ${RELEASE_DIR}/mkmfg/eula.pdf \
  ${RELEASE_DIR}/image/force.img
	$(MAKE) -C junos_packaging release


.PHONY: env
env:
	set
