#  Makefile for files that we put on the rootflop image.

# Note:
#   /root/.profile is used when logging in as root on the serial console.
#   /.profile is used when the shell is started for the vga console.
# 

include ${PROD_TREE_ROOT}/src/mk/common.mk

INSTALL_TREE?=rootflop

INSTALL_DIRECTIVES= \
	${SYS_INSTALL_FILE_s09_modprobe} \
	${SYS_INSTALL_FILE_s30_mfc_install} \
	${SYS_INSTALL_FILE_overlay_manuf} \
	${SYS_INSTALL_FILE_root_profile} \
	${SYS_INSTALL_FILE_slash_profile} \
	${SYS_INSTALL_FILE_automatic_install} \
	${SYS_INSTALL_FILE_interactive_install} \
	${SYS_INSTALL_FILE_show_install_progress} \
	${SYS_INSTALL_FILE_pxe_install} \
	${SYS_INSTALL_FILE_eula_txt} \
	${SYS_INSTALL_FILE_eula_pdf} \
	${SYS_INSTALL_FILE_inittab} \

SYS_INSTALL_FILE_s09_modprobe="file S09_modprobe 0755 root root /etc/init.d/rcS.d/S09_modprobe"
SYS_INSTALL_FILE_s30_mfc_install="file S30_mfc_install 0755 root root /etc/init.d/rcS.d/S30_mfc_install"
SYS_INSTALL_FILE_overlay_manuf="file overlay-manuf.sh 0755 root root /sbin/overlay-manuf.sh"
SYS_INSTALL_FILE_root_profile="file root.profile 0755 root root /root/.profile"
SYS_INSTALL_FILE_slash_profile="file slash.profile 0755 root root /.profile"
SYS_INSTALL_FILE_automatic_install="file automatic-install.sh 0755 root root /sbin/automatic-install.sh"
SYS_INSTALL_FILE_interactive_install="file interactive-install.sh 0755 root root /sbin/interactive-install.sh"
SYS_INSTALL_FILE_show_install_progress="file show-install-progress.sh 0755 root root /sbin/show-install-progress.sh"
SYS_INSTALL_FILE_pxe_install="file pxe-install 0755 root root /sbin/pxe-install"
SYS_INSTALL_FILE_eula_txt="file ../../../install_files/eula.txt 0444 root root /etc/eula.txt"
SYS_INSTALL_FILE_eula_pdf="file ../../../install_files/eula.pdf 0444 root root /etc/eula.pdf"
SYS_INSTALL_FILE_inittab="file inittab 0644 root root /etc/inittab"

include ${PROD_TREE_ROOT}/src/mk/files_adv.mk
