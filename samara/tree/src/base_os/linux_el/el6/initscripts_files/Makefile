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

INSTALL_DIRECTIVES= \
	${SYS_INSTALL_FILE_functions} \
	${SYS_INSTALL_FILE_halt} \
	${SYS_INSTALL_FILE_rc} \
	${SYS_INSTALL_FILE_rc_local} \
	${SYS_INSTALL_FILE_rc_sysinit} \
	${SYS_INSTALL_FILE_init_control_alt_delete_conf} \
	${SYS_INSTALL_FILE_init_kexec_disable_conf} \
	${SYS_INSTALL_FILE_init_rc_conf} \
	${SYS_INSTALL_FILE_init_rcS_conf} \
	${SYS_INSTALL_FILE_init_rcS_emergency_conf} \
	${SYS_INSTALL_FILE_init_rcS_sulogin_conf} \
	${SYS_INSTALL_FILE_init_start_ttys_conf} \
	${SYS_INSTALL_FILE_init_serial_conf} \
	${SYS_INSTALL_FILE_init_tty_conf} \
	${SYS_INSTALL_FILE_udev_start_udev} \


# Note: all of the directories we need for this are created by
# the Makefile in image_files, which is responsible for overall
# directory structure.

SYS_INSTALL_FILE_functions="file functions 0755 root root \
	/etc/rc.d/init.d/functions"
SYS_INSTALL_FILE_halt="file halt 0755 root root /etc/rc.d/init.d/halt"
SYS_INSTALL_FILE_rc="file rc 0755 root root /etc/rc.d/rc"
SYS_INSTALL_FILE_rc_local="file rc.local 0755 root root /etc/rc.d/rc.local"
SYS_INSTALL_FILE_rc_sysinit="file rc.sysinit 0755 root root \
	/etc/rc.d/rc.sysinit"
SYS_INSTALL_FILE_init_control_alt_delete_conf="file control-alt-delete.conf 0644 root root /etc/init/control-alt-delete.conf"
SYS_INSTALL_FILE_init_kexec_disable_conf="file kexec-disable.conf 0644 root root /etc/init/kexec-disable.conf"
SYS_INSTALL_FILE_init_rc_conf="file rc.conf 0644 root root /etc/init/rc.conf"
SYS_INSTALL_FILE_init_rcS_conf="file rcS.conf 0644 root root /etc/init/rcS.conf"
SYS_INSTALL_FILE_init_rcS_emergency_conf="file rcS-emergency.conf 0644 root root /etc/init/rcS-emergency.conf"
SYS_INSTALL_FILE_init_rcS_sulogin_conf="file rcS-sulogin.conf 0644 root root /etc/init/rcS-sulogin.conf"
SYS_INSTALL_FILE_init_start_ttys_conf="file start-ttys.conf 0644 root root /etc/init/start-ttys.conf"
SYS_INSTALL_FILE_init_serial_conf="file serial.conf 0644 root root /etc/init/serial.conf"
SYS_INSTALL_FILE_init_tty_conf="file tty.conf 0644 root root /etc/init/tty.conf"
SYS_INSTALL_FILE_udev_start_udev="file start_udev 0755 root root /sbin/start_udev"

include ${PROD_TREE_ROOT}/src/mk/files_adv.mk
