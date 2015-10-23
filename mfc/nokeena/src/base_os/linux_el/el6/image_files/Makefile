# Makefile

include ${PROD_TREE_ROOT}/src/mk/common.mk

#
# See src/mk/files_adv.mk for comments explaining the different types
# of directives available.
#

INSTALL_DIRECTIVES= \
	${SYS_INSTALL_DIR_slash} \
	${SYS_INSTALL_DIR_etc} \
	${SYS_INSTALL_FILE_inittab} \
	${SYS_INSTALL_FILE_UDEV_RULE} \
	${SYS_INSTALL_FILE_SERIAL_CONF} \


# First, all the directories we need

SYS_INSTALL_DIR_slash="dir 0755 root root /"
SYS_INSTALL_DIR_etc="dir 0755 root root /etc"

# All the files.  Some of these overwrite stuff coming from the base os

# This is needed so that localhost can be looked up before we first write it
SYS_INSTALL_FILE_inittab="file inittab 0644 root root /etc/inittab"

# udev rules update
SYS_INSTALL_FILE_UDEV_RULE="file 60-persistent-storage.rules 0755 root root /lib/udev/rules.d/60-persistent-storage.rules"

# serial conf update
SYS_INSTALL_FILE_SERIAL_CONF="file serial.conf 0755 root root /etc/init/serial.conf"

include ${PROD_TREE_ROOT}/src/mk/files_adv.mk
