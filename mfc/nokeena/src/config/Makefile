# Makefile

include ${PROD_TREE_ROOT}/src/mk/common.mk

INSTALL_DIRECTIVES= \
	${SYS_INSTALL_FILE_installthis_nsmartd_conf} \
	${SYS_INSTALL_FILE_installthis_mdadm_events_conf} \



CLEANFILES= \
    ${FULL_INSTALL_DIR}/opt/nkn/installthis/config/nkn/nsmartd.conf \
    ${FULL_INSTALL_DIR}/opt/nkn/installthis/config/nkn/mdadm_events.conf \



SYS_INSTALL_FILE_installthis_nsmartd_conf="file nsmartd.conf 0644 root root /opt/nkn/installthis/config/nkn/nsmartd.conf"
SYS_INSTALL_FILE_installthis_mdadm_events_conf="file mdadm_events.conf 0644 root root /opt/nkn/installthis/config/nkn/mdadm_events.conf"

include ${PROD_TREE_ROOT}/src/mk/files_adv.mk

all: install
