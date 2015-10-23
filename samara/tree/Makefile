#  Makefile

include ${PROD_TREE_ROOT}/src/mk/common.mk


SUBDIRS=\
	src \
	${PROD_CUSTOMER_ROOT} \

SRCTMPDIR=${PROD_OUTPUT_ROOT}/product-nokeena-x86_64/release/opensource/tmp
LIBCHARTDIR=${SRCTMPDIR}/nokeena/src/thirdparty/lib/chartdirector
CHARTDIR_LICENSE=${SRCTMPDIR}/nokeena/src/bin/dashboard/lib/chartdir.lic
QOSMOS_DPI=${SRCTMPDIR}/nokeena/src/thirdparty/binary/qosmos_dpi
QOSMOS_DPI_STUBS=${SRCTMPDIR}/nokeena/src/thirdparty/binary/qosmos_dpi_stubs

BIN_DPI=${SRCTMPDIR}/nokeena/src/bin/dpi
BIN_DPI_STUBS=${SRCTMPDIR}/nokeena/src/bin/dpi_stubs

LIB_DPI_URIF=${SRCTMPDIR}/nokeena/src/lib/nvsd/dpi_urif
LIB_DPI_URIF_STUBS=${SRCTMPDIR}/nokeena/src/lib/nvsd/dpi_urif_stubs

BINARY_LSIUTIL=${SRCTMPDIR}/nokeena/src/thirdparty/binary/lsiutil
BINARY_SAS2IRCU=${SRCTMPDIR}/nokeena/src/thirdparty/binary/sas2ircu

#
# External TMS design, architecture and developer documentation, not part of
# the runtime product.
#
ifneq (${PROD_FEATURE_EXTERNAL_DOC},0)
	SUBDIRS+=   \
		doc \

endif

#
# Test programs and tools, not part of the runtime product.
#
ifneq (${PROD_FEATURE_TEST},0)
	SUBDIRS+=    \
		test \

endif


include ${PROD_TREE_ROOT}/src/mk/subdir.mk

#
# Only the source and customer trees make releases
#
release:
	${MAKE} -C src release
	${MAKE} -C ${PROD_CUSTOMER_ROOT} release

release-opensource:
	mkdir -p ${PRODUCT_DIR}/release/opensource
#
# Create binary samara archive
#
	${MAKE} -f ${PROD_TREE_ROOT}/Makefile-mk-bin-samara \
    "SAMARA_TREE_ROOT=${PROD_TREE_ROOT}" \
    "BUILD_TREE_ROOT=${PROD_OUTPUT_ROOT}/product-nokeena-x86_64" \
    "DEST_ROOT=${PROD_OUTPUT_ROOT}/product-nokeena-x86_64/release/opensource" \
    "DEST_DIR_TARNAME=samara-bin-${NKN_BUILD_SCM_INFO_SHORT}"
#
# Create MFC source archive
#
	mkdir -p ${SRCTMPDIR}
	(cd ${PROD_CUSTOMER_ROOT}; tar --exclude-vcs -cf - \
		 License.txt README Makefile dist nokeena | (cd ${SRCTMPDIR}; tar xf -))
#
# chartdirector adjustments
#
	if test -e ${LIBCHARTDIR}/free-about.txt && \
	   test -e ${LIBCHARTDIR}/free-libchartdir.so.5.0.0 ; then \
	  rm -rf ${LIBCHARTDIR}/about.txt ; \
	  rm -rf ${LIBCHARTDIR}/libchartdir.so.5.0.0 ; \
	  mv ${LIBCHARTDIR}/free-about.txt ${LIBCHARTDIR}/about.txt ; \
	  mv ${LIBCHARTDIR}/free-libchartdir.so.5.0.0 ${LIBCHARTDIR}/libchartdir.so.5.0.0 ; \
	  rm -rf ${CHARTDIR_LICENSE} ; \
	  touch ${CHARTDIR_LICENSE} ; \
	fi ;
#
# qosmos_dpi adjustments
#
	if test -d ${QOSMOS_DPI} && \
	   test -d ${QOSMOS_DPI_STUBS} ; then \
	  rm -rf ${QOSMOS_DPI} ; \
	  mv ${QOSMOS_DPI_STUBS} ${QOSMOS_DPI} ; \
	fi ; 
#
	if test -d ${BIN_DPI} && \
	   test -d ${BIN_DPI_STUBS} ; then \
	  rm -rf ${BIN_DPI} ; \
	  mv ${BIN_DPI_STUBS} ${BIN_DPI} ; \
	fi ;
#
	if test -d  ${LIB_DPI_URIF} && \
	   test -d ${LIB_DPI_URIF_STUBS} ; then \
	  rm -rf ${LIB_DPI_URIF} ; \
	  mv ${LIB_DPI_URIF_STUBS} ${LIB_DPI_URIF} ; \
	fi ;
#
# lsiutil adjustments
#
	if test -e ${BINARY_LSIUTIL}/About.txt || \
	   test -f ${BINARY_LSIUTIL}/License.txt ; then \
	  rm -rf ${BINARY_LSIUTIL}/*.txt ; \
	  rm -rf ${BINARY_LSIUTIL}/lsiutil.32 ; \
	fi ;
#
# sas2ircu adjustments
#
	if test -e ${BINARY_SAS2IRCU}/About.txt || \
	   test -f ${BINARY_SAS2IRCU}/License.txt ; then \
	  rm -rf ${BINARY_SAS2IRCU}/*.txt ; \
	  rm -rf ${BINARY_SAS2IRCU}/sas2ircu.32 ; \
	fi ;
#
	(cd ${SRCTMPDIR}; tar --exclude-vcs -czf \
	    ${PROD_OUTPUT_ROOT}/product-nokeena-x86_64/release/opensource/mfc-src-${NKN_BUILD_SCM_INFO_SHORT}.tar.gz  License.txt README Makefile dist nokeena )
	rm -rf ${SRCTMPDIR}
