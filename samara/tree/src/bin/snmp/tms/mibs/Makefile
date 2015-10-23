#  Makefile
#
# 
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

#
# See src/mk/files_adv.mk for comments explaining the different types
# of directives available.
#

# XXX: /usr/share isn't specific to us, and should probably be
# created by image_files.  But it isn't installed until after us.
# Ideally there should probably be a separate Makefile that runs before
# everyone and creates all the directories, but we'll leave that for
# another day.

INSTALL_DIRECTIVES= \
	${SYS_INSTALL_DIR_usr_share} \
	${SYS_INSTALL_DIR_usr_share_snmp} \
	${SYS_INSTALL_DIR_usr_share_snmp_mibs} \


ifneq (${PROD_FEATURE_TMS_MIB},0)
	INSTALL_DIRECTIVES+= \
		${SYS_INSTALL_FILE_tms_mib} \

endif


SYS_INSTALL_DIR_usr_share="dir 0755 root root /usr/share"
SYS_INSTALL_DIR_usr_share_snmp="dir 0755 root root /usr/share/snmp"
SYS_INSTALL_DIR_usr_share_snmp_mibs="dir 0755 root root /usr/share/snmp/mibs"

SYS_INSTALL_FILE_tms_mib="file TMS-MIB.txt 0644 root root /usr/share/snmp/mibs/TMS-MIB.txt"

include ${PROD_TREE_ROOT}/src/mk/files_adv.mk
