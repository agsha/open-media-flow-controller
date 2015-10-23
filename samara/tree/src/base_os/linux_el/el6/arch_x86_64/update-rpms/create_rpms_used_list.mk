include ${PROD_TREE_ROOT}/src/mk/common.mk
include ${PROD_CUSTOMER_ROOT}/nokeena/src/mk/customer.mk
include outputdir/M.inc

#	@for ITEM in ${PACKAGES_BINARY_RPMS} ; do case $${ITEM} in *rpm) basename $${ITEM} >> outputdir/rpms.used.list ;; esac ; done

all:
	@if [ ! -d outputdir ] ; then mkdir outputdir ; fi
	@rm -f outputdir/rpms.used.list
	@for ITEM in ${PACKAGES_BINARY_RPMS} ; do basename $${ITEM} >> outputdir/rpms.used.list ; done

