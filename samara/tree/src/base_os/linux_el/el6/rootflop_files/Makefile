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

INSTALL_TREE?=rootflop

# For files with deviations from X86:
XARCH=
ifeq (${PROD_TARGET_ARCH_FAMILY},PPC)
	XARCH=_${PROD_TARGET_ARCH_LC}
endif

INSTALL_DIRECTIVES= \
	${SYS_INSTALL_DIR_bin} \
	${SYS_INSTALL_DIR_dev} \
	${SYS_INSTALL_DIR_etc_init_d} \
	${SYS_INSTALL_DIR_etc_init_d_rcs_d} \
	${SYS_INSTALL_DIR_etc_opt} \
	${SYS_INSTALL_DIR_etc} \
	${SYS_INSTALL_DIR_etc_pki} \
	${SYS_INSTALL_DIR_etc_pki_tpkg} \
	${SYS_INSTALL_DIR_etc_pki_tpkg_private} \
	${SYS_INSTALL_DIR_initrd} \
	${SYS_INSTALL_DIR_lib} \
	${SYS_INSTALL_DIR_mnt} \
	${SYS_INSTALL_DIR_mnt_cdrom} \
	${SYS_INSTALL_DIR_opt} \
	${SYS_INSTALL_DIR_proc} \
	${SYS_INSTALL_DIR_root} \
	${SYS_INSTALL_DIR_sbin} \
	${SYS_INSTALL_DIR_sys} \
	${SYS_INSTALL_DIR_tmp} \
	${SYS_INSTALL_DIR_tmproot} \
	${SYS_INSTALL_DIR_usr_bin} \
	${SYS_INSTALL_DIR_usr_lib} \
	${SYS_INSTALL_DIR_usr_sbin} \
	${SYS_INSTALL_DIR_usr_share} \
	${SYS_INSTALL_DIR_usr} \
	${SYS_INSTALL_DIR_var_lib} \
	${SYS_INSTALL_DIR_var_lock} \
	${SYS_INSTALL_DIR_var_log} \
	${SYS_INSTALL_DIR_var_opt} \
	${SYS_INSTALL_DIR_var_run} \
	${SYS_INSTALL_DIR_var_spool} \
	${SYS_INSTALL_DIR_var_tmp} \
	${SYS_INSTALL_DIR_var} \
	${SYS_INSTALL_FILE_busybox_conf} \
	${SYS_INSTALL_FILE_fstab} \
	${SYS_INSTALL_FILE_group} \
	${SYS_INSTALL_FILE_host_conf} \
	${SYS_INSTALL_FILE_inittab${XARCH}} \
	${SYS_INSTALL_FILE_nsswitch_conf} \
	${SYS_INSTALL_FILE_passwd} \
	${SYS_INSTALL_FILE_profile} \
	${SYS_INSTALL_FILE_termcap} \
	${SYS_INSTALL_FILE_init_d_rcs} \
	${SYS_INSTALL_FILE_init_d_rcstop} \
	${SYS_INSTALL_FILE_s10tms_dhcpc} \
	${SYS_INSTALL_FILE_s34automfg} \
	${SYS_INSTALL_FILE_automfg} \
	${SYS_INSTALL_FILE_udhcpc_script} \
	${SYS_INSTALL_FILE_etc_sysctl_conf} \
	${SYS_INSTALL_FILE_imgq_sh} \
	${SYS_INSTALL_FILE_aiset_sh} \
	${SYS_INSTALL_FILE_aiget_sh} \
	${SYS_INSTALL_FILE_imgverify_sh} \
	${SYS_INSTALL_FILE_layout_settings_sh} \
	${SYS_INSTALL_FILE_tpkg_create_sh} \
	${SYS_INSTALL_FILE_tpkg_extract_sh} \
	${SYS_INSTALL_FILE_tpkg_query_sh} \
	${SYS_INSTALL_FILE_tpkg_secure_sh} \
	${SYS_INSTALL_FILE_dmidump_sh} \
	${SYS_TOUCH_FILE_messages} \
	${SYS_INSTALL_FILE_opt_feature_rootflop_sh} \
	${SYS_CREATE_SYMLINK_mtab} \

# Juniper change to the above list --
# Removed writeimage.sh and manufature.sh from the rootflop.
# We put those in the "manufacture" image.
#	${SYS_INSTALL_FILE_writeimage_sh} \
#	${SYS_INSTALL_FILE_manufacture_sh} \

ifneq (${PROD_FEATURE_CMC_CLIENT}${PROD_FEATURE_CMC_SERVER},00)
	INSTALL_DIRECTIVES+= \
		${SYS_INSTALL_FILE_cmc_rootflop_sh} \

endif

ifneq (${PROD_FEATURE_CLUSTER},0)
	INSTALL_DIRECTIVES+= \
		${SYS_INSTALL_FILE_cluster_rootflop_sh} \

endif

ifneq (${PROD_FEATURE_HW_RNG},0)
	INSTALL_DIRECTIVES+= \
		${SYS_INSTALL_FILE_s05hwrng} \

endif

ifeq (${PROD_TARGET_ARCH_FAMILY},PPC)
INSTALL_DIRECTIVES+=\
	${SYS_INSTALL_FILE_ee_conf_template} \

endif


# First, all the directories we need

SYS_INSTALL_DIR_bin="dir 0755 root root /bin"
SYS_INSTALL_DIR_dev="dir 0755 root root /dev"
SYS_INSTALL_DIR_etc_init_d="dir 0755 root root /etc/init.d"
SYS_INSTALL_DIR_etc_init_d_rcs_d="dir 0755 root root /etc/init.d/rcS.d"
SYS_INSTALL_DIR_etc_opt="dir 0755 root root /etc/opt"
SYS_INSTALL_DIR_etc="dir 0755 root root /etc"
SYS_INSTALL_DIR_etc_pki="dir 0755 root root /etc/pki"
SYS_INSTALL_DIR_etc_pki_tpkg="dir 0755 root root /etc/pki/tpkg"
SYS_INSTALL_DIR_etc_pki_tpkg_private="dir 0700 root root /etc/pki/tpkg/private"
SYS_INSTALL_DIR_initrd="dir 0755 root root /initrd"
SYS_INSTALL_DIR_lib="dir 0755 root root /lib"
SYS_INSTALL_DIR_mnt="dir 0755 root root /mnt"
SYS_INSTALL_DIR_mnt_cdrom="dir 0755 root root /mnt/cdrom"
SYS_INSTALL_DIR_opt="dir 0755 root root /opt"
SYS_INSTALL_DIR_proc="dir 0755 root root /proc"
SYS_INSTALL_DIR_root="dir 0755 root root /root"
SYS_INSTALL_DIR_sbin="dir 0755 root root /sbin"
SYS_INSTALL_DIR_sys="dir 0755 root root /sys"
SYS_INSTALL_DIR_tmp="dir 1777 root root /tmp"
SYS_INSTALL_DIR_tmproot="dir 0755 root root /tmproot"
SYS_INSTALL_DIR_usr_bin="dir 0755 root root /usr/bin"
SYS_INSTALL_DIR_usr_lib="dir 0755 root root /usr/lib"
SYS_INSTALL_DIR_usr_sbin="dir 0755 root root /usr/sbin"
SYS_INSTALL_DIR_usr_share="dir 0755 root root /usr/share"
SYS_INSTALL_DIR_usr="dir 0755 root root /usr"
SYS_INSTALL_DIR_var_lib="dir 0755 root root /var/lib"
SYS_INSTALL_DIR_var_lock="dir 0755 root root /var/lock"
SYS_INSTALL_DIR_var_log="dir 0755 root root /var/log"
SYS_INSTALL_DIR_var_opt="dir 0755 root root /var/opt"
SYS_INSTALL_DIR_var_run="dir 0755 root root /var/run"
SYS_INSTALL_DIR_var_spool="dir 0755 root root /var/spool"
SYS_INSTALL_DIR_var_tmp="dir 1777 root root /var/tmp"
SYS_INSTALL_DIR_var="dir 0755 root root /var"


# All the files

SYS_INSTALL_FILE_busybox_conf="file busybox.conf 0644 root root /etc/busybox.conf"
SYS_INSTALL_FILE_fstab="file fstab 0644 root root /etc/fstab"
SYS_INSTALL_FILE_group="file group 0644 root root /etc/group"
SYS_INSTALL_FILE_host_conf="file host.conf 0644 root root /etc/host.conf"
SYS_INSTALL_FILE_inittab="file inittab 0644 root root /etc/inittab"
SYS_INSTALL_FILE_inittab_ppc="file inittab.ppc 0644 root root /etc/inittab"
SYS_INSTALL_FILE_nsswitch_conf="file nsswitch.conf 0644 root root \
	/etc/nsswitch.conf"
SYS_INSTALL_FILE_passwd="file passwd 0644 root root /etc/passwd"
SYS_INSTALL_FILE_profile="file profile 0644 root root /etc/profile"
SYS_INSTALL_FILE_termcap="file termcap 0644 root root /etc/termcap"
SYS_INSTALL_FILE_init_d_rcs="file rcS 0755 root root /etc/init.d/rcS"
SYS_INSTALL_FILE_init_d_rcstop="file rc-stop 0755 root root /etc/init.d/rc-stop"
SYS_INSTALL_FILE_s05hwrng="file S05hwrng 0755 root root /etc/init.d/rcS.d/S05hwrng"
SYS_INSTALL_FILE_s10tms_dhcpc="file S10tms_dhcpc 0755 root root /etc/init.d/rcS.d/S10tms_dhcpc"
SYS_INSTALL_FILE_s34automfg="file S34automfg 0755 root root /etc/init.d/rcS.d/S34automfg"
SYS_INSTALL_FILE_automfg="file automfg 0755 root root /sbin/automfg"
SYS_INSTALL_FILE_udhcpc_script="file udhcpc.script 0755 root root /sbin/udhcpc.script"
SYS_INSTALL_FILE_etc_sysctl_conf="file sysctl.conf 0644 root root /etc/sysctl.conf"

# Files from other dirs
SYS_INSTALL_FILE_writeimage_sh="file ../../../common/script_files/writeimage.sh 0755 root root /sbin/writeimage.sh"
SYS_INSTALL_FILE_imgq_sh="file ../../../common/script_files/imgq.sh 0755 root root /sbin/imgq.sh"
SYS_INSTALL_FILE_manufacture_sh="file ../../../common/script_files/manufacture.sh 0755 root root /sbin/manufacture.sh"
SYS_INSTALL_FILE_aiset_sh="file ../../../common/script_files/aiset.sh 0755 root root /sbin/aiset.sh"
SYS_INSTALL_FILE_aiget_sh="file ../../../common/script_files/aiget.sh 0755 root root /sbin/aiget.sh"
SYS_INSTALL_FILE_imgverify_sh="file ../../../common/script_files/imgverify.sh 0755 root root /sbin/imgverify.sh"
SYS_INSTALL_FILE_layout_settings_sh="file ../../../common/script_files/layout_settings.sh 0755 root root /etc/layout_settings.sh"
SYS_INSTALL_FILE_ee_conf_template="file ../image_files/ee.conf.template 0644 root root /etc/ee.conf.template"
SYS_INSTALL_FILE_tpkg_create_sh="file ../../../common/script_files/tpkg_create.sh 0755 root root /sbin/tpkg_create.sh"
SYS_INSTALL_FILE_tpkg_extract_sh="file ../../../common/script_files/tpkg_extract.sh 0755 root root /sbin/tpkg_extract.sh"
SYS_INSTALL_FILE_tpkg_query_sh="file ../../../common/script_files/tpkg_query.sh 0755 root root /sbin/tpkg_query.sh"
SYS_INSTALL_FILE_tpkg_secure_sh="file ../../../common/script_files/tpkg_secure.sh 0755 root root /sbin/tpkg_secure.sh"
SYS_INSTALL_FILE_opt_feature_rootflop_sh="file ../../../common/script_files/opt_feature_rootflop.sh 0755 root root /sbin/opt_feature_rootflop.sh"
SYS_INSTALL_FILE_cmc_rootflop_sh="file ../../../common/script_files/cmc_rootflop.sh 0755 root root /sbin/cmc_rootflop.sh"
SYS_INSTALL_FILE_cluster_rootflop_sh="file ../../../common/script_files/cluster_rootflop.sh 0755 root root /sbin/cluster_rootflop.sh"
SYS_INSTALL_FILE_dmidump_sh="file ../../../common/script_files/dmidump.sh 0755 root root /sbin/dmidump.sh"


# Files to create empty
SYS_TOUCH_FILE_messages="touch 0644 root root /var/log/messages"

# Finally, the symlinks

SYS_CREATE_SYMLINK_mtab="symlink /etc /proc/mounts mtab"


include ${PROD_TREE_ROOT}/src/mk/files_adv.mk
