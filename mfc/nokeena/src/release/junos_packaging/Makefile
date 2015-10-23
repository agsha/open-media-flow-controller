# Makefile

include ${PROD_TREE_ROOT}/src/mk/common.mk
RELEASE_DIR=${PROD_OUTPUT_DIR}/release
IMAGE_DIR=${RELEASE_DIR}/image
TMP_DIR=${RELEASE_DIR}/pacifica_TMP

.PHONY: release
release: mfc-mk.x86.sh
	@echo In ${PWD}
	@echo Target: release
	@rm -rf "${TMP_DIR}"
	@mkdir  "${TMP_DIR}"
	rm -rf ${IMAGE_DIR}/mfc--*
	@echo Execute: mfc-mk.x86.sh ${IMAGE_DIR} ${TMP_DIR} default
	@bash mfc-mk.x86.sh ${IMAGE_DIR} ${TMP_DIR} default
	rm -rf "${TMP_DIR}"

# This prod_release target is for testing by hand to force production signing.
prod_release: mfc-mk.x86.sh
	@echo In ${PWD}
	@echo Target: release
	@rm -rf "${TMP_DIR}"
	@mkdir  "${TMP_DIR}"
	rm -rf ${IMAGE_DIR}/mfc--*
	@echo Execute: mfc-mk.x86.sh ${IMAGE_DIR} ${TMP_DIR} prod
	@bash mfc-mk.x86.sh ${IMAGE_DIR} ${TMP_DIR} prod
	rm -rf "${TMP_DIR}"

.PHONY: clean
clean:
	rm -rf "${TMP_DIR}"
	rm -rf ${IMAGE_DIR}/mfc--*
