#  Makefile
#
include ${PROD_TREE_ROOT}/src/mk/common.mk

INSTALL_DIRECTIVES= \
	${SYS_INSTALL_FILE_customer_sh} \
	${SYS_INSTALL_FILE_customer_rootflop_sh} \
	${SYS_INSTALL_FILE_var_upgrade_nokeena_sh} \


# NOTE: Some of these files also need to be on the rootflop.  For this
# to be done, directives need to be added to ../rootflop_files/Makefile
# For instance, the customer_rootflop.sh file.

SYS_INSTALL_FILE_customer_sh="file customer.sh 0644 root root \
	/etc/customer.sh"
SYS_INSTALL_FILE_customer_rootflop_sh="file customer_rootflop.sh 0644 \
	root root /etc/customer_rootflop.sh"
SYS_INSTALL_FILE_var_upgrade_nokeena_sh="file var_upgrade_nokeena.sh 0644 \
	root root /sbin/var_upgrade_nokeena.sh"

include ${PROD_TREE_ROOT}/src/mk/files_adv.mk
