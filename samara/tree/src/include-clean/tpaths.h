/*
 *
 * tpaths.h
 *
 *
 *
 */

#ifndef __TPATHS_H_
#define __TPATHS_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

#include "ttypes.h"
#include "md_client.h"

/* ========================================================================= */
/** \file tpaths.h This file contains paths used in components we write. 
 * \ingroup lc
 *
 * Note: we generally try to have a \#define in all caps, and an
 * 'extern const char ...[]' in lowercase, for each entry.  The \#define is
 * helpful for forming paths by concatenation other ones together, and
 * in cases where you must have a preprocessor constant to make the
 * code compile.  The const variables are preferable to be used from
 * code wherever possible because you can print them in the debugger.
 * They should be 'extern' (and defined in tpaths.c using the \#define)
 * so as not to include a copy of the string in everyone who includes us.
 *
 * CAUTION: some of the paths in this file (e.g. WEB_HTTPD_CONF_PATH)
 * are put into the configuration database through initial values.
 * So if you change them here, you may need to add an upgrade function
 * to the appropriate mgmtd module.
 *
 * NOTE: some of the paths in this file are mirrored by
 * src/java/packages/com/tallmaple/common/Tpaths.java,
 * and should be kept in sync.
 *
 * XXX/EMT: we should have a tpaths.sh to define paths we use from our
 * shell scripts.  Currently, the paths or filenames are just
 * hardwired (e.g. in sysdump.sh).
 */

/*
 * XXX/EMT: there are some path dependencies also in 
 * src/base_os/apache/src/include/ap_config_auto.h and
 * src/base_os/apache/src/include/ap_config_layout.h
 * although perhaps they can be overridden in Apache's config file.
 */


/* ========================================================================= */
/**
 * @name OS-specific paths
 *
 * This area contains paths that are OS-specific, so we keep a separate
 * section for every target OS we support.
 *
 * NOTE: if you add or remove a path from here, make sure to do so for
 * all target OSs!
 */
/*@{*/

/* ............................................................................
 * Linux
 */
#ifdef PROD_TARGET_OS_LINUX

#define PROG_PING_PATH "/bin/ping"
#define PROG_PING6_PATH "/bin/ping6"
#define PROG_TRACEROUTE_PATH "/bin/traceroute"
#define PROG_CURL_PATH "/usr/bin/curl"
#define FILE_WEB_LOGIN_MSG_PATH "/etc/issue.net"

#ifdef PROD_FEATURE_WGET
#define PROG_WGET_PATH "/usr/bin/wget"
#endif

#endif /* PROD_TARGET_OS_LINUX */

/* ............................................................................
 * FreeBSD
 */
#ifdef PROD_TARGET_OS_FREEBSD

#define PROG_PING_PATH "/sbin/ping"
#define PROG_PING6_PATH "/sbin/ping6"
#define PROG_TRACEROUTE_PATH "/usr/sbin/traceroute"
#define PROG_CURL_PATH "/usr/local/bin/curl"
#define FILE_WEB_LOGIN_MSG_PATH "/etc/motd"

#ifdef PROD_FEATURE_WGET
#define PROG_WGET_PATH "/usr/local/bin/wget"
#endif

#endif /* PROD_TARGET_OS_FREEBSD */

/* ............................................................................
 * SunOS
 */
#ifdef PROD_TARGET_OS_SUNOS

#define PROG_PING_PATH "/usr/sbin/ping"
#define PROG_PING6_PATH "/usr/sbin/ping6"
#define PROG_TRACEROUTE_PATH "/bin/traceroute"
#define PROG_CURL_PATH "/usr/bin/curl"
#define FILE_WEB_LOGIN_MSG_PATH "/etc/issue.net"

#ifdef PROD_FEATURE_WGET
#define PROG_WGET_PATH "/usr/bin/wget"
#endif

#endif /* PROD_TARGET_OS_SUNOS */

/*@}*/


/* ========================================================================= */
/**
 * @name General path prefixes
 */
/*@{*/

#ifndef PROD_ROOT_PREFIX
#define PROD_ROOT_PREFIX ""
#endif
extern const char prod_root_prefix[];

#define PROD_STATIC_ROOT_NO_PREFIX                    "/opt/tms"
#define PROD_STATIC_ROOT             PROD_ROOT_PREFIX "/opt/tms"
#define PROD_VAR_ROOT                PROD_ROOT_PREFIX "/var/opt/tms"
#define PROD_TMS_BIN_DIR             PROD_STATIC_ROOT "/bin"
#define PROD_CONF_OUTPUT_ROOT   PROD_ROOT_PREFIX "/var/opt/tms"
#define MD_GEN_OUTPUT_PATH      PROD_CONF_OUTPUT_ROOT "/output"
#define PROD_TMP_ROOT           PROD_ROOT_PREFIX "/tmp"
#define PROD_VTMP_ROOT          PROD_ROOT_PREFIX "/vtmp"
#define PROD_VAR_STMP_ROOT      PROD_ROOT_PREFIX "/var/stmp"
#define PROD_CONFIG_ROOT        PROD_ROOT_PREFIX "/config"
#define PROD_BOOTMGR_ROOT       PROD_ROOT_PREFIX "/bootmgr"

extern const char prod_static_root_no_prefix[];
extern const char prod_static_root[];
extern const char prod_var_root[];
extern const char prod_conf_output_root[];
extern const char md_gen_output_path[];
extern const char prod_tmp_root[];
extern const char prod_vtmp_root[];
extern const char prod_var_stmp_root[];
extern const char prod_config_root[];
extern const char prod_bootmgr_root[];
extern const char prod_dev_strip_prefix[];

/*@}*/


/* ========================================================================= */
/**
 * @name Base system binaries and scripts
 *
 * These are files that come from the Base OS, so you would expect to
 * find them in their usual places even in a development environment.
 */
/*@{*/

/* ......................................................................... */
/* Network applications
 */

#define PROG_TCPDUMP_PATH "/usr/sbin/tcpdump"
#define PROG_TELNET_PATH "/usr/bin/telnet"
#define PROG_TFTP_PATH "/usr/bin/tftp"
#define XINETD_PATH "/usr/sbin/xinetd"
#define PROG_IPTABLES_V4_PATH "/sbin/iptables"
#define PROG_IPTABLES_V6_PATH "/sbin/ip6tables"
#define PROG_DHCP_CLIENT "/sbin/dhclient"
#define PROG_DHCP6_CLIENT "/sbin/dhclient"

#define PROG_ZEROCONF_PATH "/usr/bin/zeroconf"
#define PROG_NTPDATE_PATH "/usr/sbin/ntpdate"
#define PROG_NTPQ_PATH "/usr/sbin/ntpq"
#define PROG_SSMTP_PATH "/sbin/ssmtp"

#define ZEROCONF_SCRIPT_PATH "/etc/zeroconf"
#define DHCP_CLIENT_SCRIPT_PATH "/sbin/dhclient-script"
#define DHCP6_CLIENT_SCRIPT_PATH "/sbin/dhclient6-script"

extern const char prog_ping_path[];
extern const char prog_ping6_path[];
extern const char prog_traceroute_path[];
extern const char prog_tcpdump_path[];
extern const char prog_telnet_path[];
extern const char prog_curl_path[];
extern const char prog_tftp_path[];
extern const char xinetd_path[];
extern const char prog_iptables_v4_path[];
extern const char prog_iptables_v6_path[];
extern const char prog_dhcp_client[];
extern const char prog_dhcp6_client[];
extern const char prog_ntpdate_path[];
extern const char prog_ntpq_path[];
extern const char prog_ssmtp_path[];
extern const char file_web_login_msg_path[];

#ifdef PROD_FEATURE_WGET
extern const char prog_wget_path[];
#endif


/* ......................................................................... */
/* Lower-level networking
 */

#define PROG_MII_TOOL_PATH "/sbin/mii-tool"
#define PROG_IFCONFIG_PATH "/sbin/ifconfig"
#define PROG_ROUTE_PATH  "/sbin/route"
#define PROG_BTCTL_PATH "/usr/sbin/brctl"
#define PROG_ARP_PATH "/sbin/arp"
#define PROG_ARPING_PATH "/usr/sbin/arping"
#define PROG_IP_PATH "/sbin/ip"
#define PROG_NAMEIF_PATH "/sbin/nameif"

extern const char prog_mii_tool_path[];
extern const char prog_ifconfig_path[];
extern const char prog_route_path[];
extern const char prog_brctl_path[];
extern const char prog_arp_path[];
extern const char prog_arping_path[];
extern const char prog_ip_path[];
extern const char prog_zeroconf[];
extern const char prog_nameif_path[];

#ifdef PROD_FEATURE_BONDING
#define PROG_IFENSLAVE_PATH "/sbin/ifenslave"
extern const char prog_ifenslave_path[];
#endif /* PROD_FEATURE_BONDING */

#ifdef PROD_FEATURE_VLAN
#define PROG_VCONFIG_PATH "/sbin/vconfig"
extern const char prog_vconfig_path[];
#endif /* PROD_FEATURE_VLAN */


/* ......................................................................... */
/* Security
 */

#define PROG_SSH_PATH "/usr/bin/ssh"
#define PROG_SSH_KEYGEN_PATH "/usr/bin/ssh-keygen"
#define PROG_SCP_BINARY_NAME "scp"
#define PROG_SFTP_BINARY_NAME "sftp"
#define PROG_SCP_PATH "/usr/bin/" PROG_SCP_BINARY_NAME
#define PROG_SFTP_PATH "/usr/bin/" PROG_SFTP_BINARY_NAME
#define PROG_SFTP_SERVER_PATH "/usr/libexec/openssh/sftp-server"
#define GPGV_PATH "/usr/bin/gpgv"

#define PROG_SETKEY_PATH "/sbin/setkey"
/* Deprecated:  #define PROG_RACOON_PATH "/usr/sbin/racoon" */
/*
 * XXX/SML: As of the fix for bug 13725, racoon must no longer be invoked
 * directly.  The new racoon_startup.sh wrapper is now responsible for handling
 * IPsec policy and database table environment setup prior to racoon
 * invocation.  All crypto daemon startup (presumably only in md_pm.c)
 * should now be invoked through PROG_CRYPTO_STARTUP_PATH.  Note that
 * PROG_CRYPTO_DAEMON_PATH, which replaces PROG_RACOON_PATH PM,
 * is only used by PM for reporting (e.g. on failure) against the real
 * racoon executable exec'd by the wrapper.
 */
#define PROG_CRYPTO_DAEMON_PATH "/usr/sbin/racoon"
#define PROG_CRYPTO_STARTUP_PATH "/usr/sbin/racoon_startup.sh"
#define PROG_RACOONCTL_PATH "/usr/sbin/racoonctl"
#define PROG_OPENSSL_PATH "/usr/bin/openssl"

/* Openswan pluto IPsec (pluto) startup script: */
/* XXXXX/SML: maybe rename PROG_IPSEC_PATH-->PROG_IPSEC_STARTUP or _INIT */
#define PROG_IPSEC_PATH "/usr/sbin/ipsec_startup.sh"
#define PROG_PLUTO_PATH "/usr/libexec/ipsec/pluto"
#define PLUTO_WAIT_FILENAME "/var/run/pluto/ipsec.plutowait"
#define PROG_IPSEC "/usr/sbin/ipsec"

extern const char prog_ssh_path[];
extern const char prog_ssh_keygen_path[];
extern const char prog_scp_path[];
extern const char prog_sftp_server_path[];
extern const char prog_openssl_path[];
extern const char customer_ssl_vars_path[];
extern const char gpgv_path[];

#ifdef PROD_FEATURE_CRYPTO
extern const char prog_setkey_path[];
extern const char prog_crypto_startup_path[];
extern const char prog_racoonctl_path[];
#endif

#define ACCTD_PATH PROD_STATIC_ROOT "/bin/acctd"


/* ......................................................................... */
/* Miscellaneous
 */

#define PROG_BASH_PATH "/bin/bash"
#define PROG_SH_PATH "/bin/sh"
#define PROG_LDD_PATH "/usr/bin/ldd"
#define PROG_DATE_PATH "/bin/date"
#define PROG_DD_PATH "/bin/dd"
#define PROG_GCORE_PATH "/usr/bin/gcore"
#define PROG_GSTACK_PATH "/usr/bin/gstack"

extern const char prog_bash_path[];
extern const char prog_cli_unix_shell_path[];
extern const char prog_standard_shell_path[];
extern const char prog_sh_path[];
extern const char prog_ldd_path[];
extern const char prog_date_path[];
extern const char prog_dd_path[];
extern const char prog_gcore_path[];
extern const char prog_gstack_path[];

#define PROG_MODPROBE_PATH "/sbin/modprobe"
#define PROG_RMMOD_PATH "/sbin/rmmod"
#define PROG_LSMOD_PATH "/sbin/lsmod"

extern const char prog_modprobe_path[];
extern const char prog_rmmod_path[];
extern const char prog_lsmod_path[];

#define PROG_LAST_PATH "/usr/bin/last"
#define PROG_CROND_PATH "/usr/sbin/crond"
#define PROG_CRONTAB_PATH "/usr/bin/crontab"
#define PROG_KILL_PATH "/usr/bin/kill"
#ifdef PROD_TARGET_OS_FREEBSD
#define PROG_MOUNT_PATH "/sbin/mount"
#else
#define PROG_MOUNT_PATH "/bin/mount"
#endif
#define PROG_SYSLOGD_PATH "/sbin/syslogd"
#define PROG_KLOGD_PATH "/sbin/klogd"
#define PROG_LOGROTATE_PATH "/usr/sbin/logrotate_wrapper.sh"
#define PROG_NOLOGIN_PATH "/sbin/nologin"
#define PROG_TAIL_PATH "/usr/bin/tail"
#define PROG_TOP_PATH "/usr/bin/top"
#define PROG_TAR_PATH "/bin/tar"
#define PROG_BZCAT_PATH "/usr/bin/bzcat"
#define PROG_ZCAT_PATH "/bin/zcat"
#define PROG_CAT_PATH "/bin/cat"
#define PROG_GREP_PATH "/bin/grep"
#define PROG_ZGREP_PATH "/usr/bin/zgrep"
#define PROG_GZIP_PATH "/bin/gzip"
#define PROG_GUNZIP_PATH "/bin/gunzip"
#define PROG_UNZIP_PATH "/usr/bin/unzip"
#define PROG_SHUTDOWN_PATH "/sbin/shutdown"
#define PROG_TCLSH_PATH "/usr/bin/tclsh"
#define PROG_SERVICE_PATH "/sbin/service"
#define PROG_CHKCONFIG_PATH "/sbin/chkconfig"
#define PROG_SLEEP_PATH "/bin/sleep"
#define PROG_MINGETTY_PATH "/sbin/mingetty"
#define PROG_MKE2FS_PATH "/sbin/mke2fs"
#define PROG_MKE4FS_PATH "/sbin/mke4fs"
#define PROG_MKFS_VFAT_PATH "/sbin/mkfs.vfat"
#define PROG_SYNC_PATH "/bin/sync"
#define PROG_CP_PATH "/bin/cp"

#ifdef PROD_TARGET_OS_FREEBSD
#define PROG_TOUCH_PATH "/usr/bin/touch"
#else
#define PROG_TOUCH_PATH "/bin/touch"
#endif

extern const char prog_last_path[];
extern const char prog_crond_path[];
extern const char prog_crontab_path[];
extern const char prog_kill_path[];
extern const char prog_mount_path[];
extern const char prog_syslogd_path[];
extern const char prog_klogd_path[];
extern const char prog_logrotate_path[];
extern const char prog_nologin_path[];
extern const char prog_tail_path[];
extern const char prog_top_path[];
extern const char prog_tar_path[];
extern const char prog_bzcat_path[];
extern const char prog_zcat_path[];
extern const char prog_cat_path[];
extern const char prog_grep_path[];
extern const char prog_zgrep_path[];
extern const char prog_gzip_path[];
extern const char prog_gunzip_path[];
extern const char prog_unzip_path[];
extern const char prog_shutdown_path[];
extern const char prog_tclsh_path[];
extern const char prog_service_path[];
extern const char prog_chkconfig_path[];
extern const char prog_sleep_path[];
extern const char prog_mingetty_path[];
extern const char prog_mke2fs_path[];
extern const char prog_mke4fs_path[];
extern const char prog_mkfs_vfat_path[];
extern const char prog_sync_path[];
extern const char prog_cp_path[];
extern const char prog_touch_path[];

#ifdef PROD_FEATURE_OLD_GRAPHING
#define PROG_RRDTOOL_PATH "/usr/bin/rrdtool"
extern const char prog_rrdtool_path[];
#endif

#ifdef PROD_FEATURE_JAVA
#define PROG_JAVA_PATH        PROD_ROOT_PREFIX "/usr/bin/java"
#define PROD_JAVA_ROOT        PROD_ROOT_PREFIX "/opt/tms/java"
#define PROD_JAVA_CLASSPATH   PROD_JAVA_ROOT "/classes"
#define PROD_JAVA_LIB_PATH    PROD_STATIC_ROOT "/java/lib"
extern const char prog_java_path[];
extern const char prod_java_root[];
extern const char prod_java_classpath[];
extern const char prod_java_lib_path[];
#endif /* PROD_FEATURE_JAVA */

/*@}*/


/* ========================================================================= */
/**
 * @name Tall Maple binaries and scripts
 *
 * These files come from the Samara product.  In a development
 * environment, they need to be prefixed with PROD_ROOT_PREFIX to
 * be found.
 *
 * XXX/EMT: should httpd have PROD_ROOT_PREFIX?  Perhaps we will
 * launch it ourselves and give it our own config file, with a
 * non-privileged port to listen on?  Ditto SNMPKEYS.
 */
/*@{*/

#define MD_PATH                   PROD_STATIC_ROOT "/bin/mgmtd"
#define MDREQ_PATH                PROD_STATIC_ROOT "/bin/mdreq"
#define PM_PATH                   PROD_STATIC_ROOT "/bin/pm"
#define WSMD_PATH                 PROD_STATIC_ROOT "/bin/wsmd"
#define CLI_SHELL_PATH            PROD_STATIC_ROOT "/bin/cli"
#define SNMPD_PATH                PROD_STATIC_ROOT "/bin/snmpd"
#define PM_CRASH_HANDLER_PATH     PROD_STATIC_ROOT "/bin/pm_cs"
#define CGI_LAUNCHER_PATH         PROD_STATIC_ROOT "/lib/web/cgi-bin/launch"
#define REQUEST_HANDLER_PATH      PROD_STATIC_ROOT "/lib/web/handlers/rh"
#define STATSD_PATH               PROD_STATIC_ROOT "/bin/statsd"
#define PROG_COREINFO_PATH        PROD_TMS_BIN_DIR "/coreinfo"
#define CLI_PAGER_PATH            PROD_ROOT_PREFIX "/usr/bin/less"
#define WIZARD_PATH               PROD_STATIC_ROOT "/bin/wizard"
#define SCHED_PATH                PROD_STATIC_ROOT "/bin/sched"
#define FPD_PATH                  PROD_STATIC_ROOT "/bin/fpd"
#define PROGRESS_PATH             PROD_STATIC_ROOT "/bin/progress"
#define XG_PATH                   PROD_STATIC_ROOT "/bin/xg"

extern const char md_path[];
extern const char mdreq_path[];
extern const char pm_path[];
extern const char wsmd_path[];
extern const char cli_shell_path[];
extern const char snmpd_path[];
extern const char pm_crash_handler_path[];
extern const char prog_coreinfo_path[];
extern const char prog_cli_pager_path[];
extern const char progress_path[];
extern const char xg_path[];

#ifdef PROD_FEATURE_VIRT
#define TVIRTD_PATH               PROD_STATIC_ROOT "/bin/tvirtd"
#define LIBVIRTD_PATH             "/usr/sbin/libvirtd"
#define TCONS_PATH                PROD_STATIC_ROOT "/bin/tcons"
#define VNCVIEWER_PATH            "/usr/bin/vncviewer"

#ifdef VIRT_QEMU_KVM_PATH
#define QEMU_KVM_PATH             VIRT_QEMU_KVM_PATH
#else
#define QEMU_KVM_PATH             "/usr/libexec/qemu-kvm"
#endif

#define VIRSH_PATH                "/usr/bin/virsh"
extern const char tvirtd_path[];
extern const char tcons_path[];
extern const char vncviewer_path[];
extern const char libvirtd_path[];
extern const char qemu_kvm_path[];
extern const char virsh_path[];
#endif /* PROD_FEATURE_VIRT */

/*
 * NOTE: TBN_DEFAULT_LOGFILE would like to be defined here, but is
 * not.  Instead, it is defined in
 * src/base_os/linux_common/pam/pam_tallybyname/src/pam_tallybyname_common.h
 * This is because we need to include it from a non-TMS component, and 
 * tpaths.h includes md_client.h, and so gets polluted.
 */

#define PAM_TALLYBYNAME_PATH      PROD_STATIC_ROOT "/bin/pam_tallybyname"
extern const char pam_tallybyname_path[];

#ifdef PROD_FEATURE_WIZARD
extern const char wizard_path[];
#endif
#ifdef PROD_FEATURE_SCHED
extern const char sched_path[];
#endif
#ifdef PROD_FEATURE_FRONT_PANEL
extern const char fpd_path[];
#endif

#define HTTPD_PATH                PROD_ROOT_PREFIX "/usr/sbin/httpd"
#define SNMPKEYS_PATH             PROD_ROOT_PREFIX "/usr/bin/snmpkeys"
#define SSHD_PATH                 PROD_ROOT_PREFIX "/usr/sbin/sshd"

extern const char httpd_path[];
extern const char snmpkeys_path[];
extern const char sshd_path[];

#define PROG_TAIL_GREP_PATH       PROD_ROOT_PREFIX "/sbin/tailgrep.sh"
#define SYSDUMP_GENERATE_PATH     PROD_ROOT_PREFIX "/sbin/sysdump.sh"
#define MAKEMAIL_PATH             PROD_ROOT_PREFIX "/sbin/makemail.sh"
#define AFAIL_PATH                PROD_ROOT_PREFIX "/sbin/afail.sh"
#define NTPD_WRAPPER_PATH         PROD_ROOT_PREFIX "/usr/sbin/ntpd_wrapper.sh"
#define NTPD_PATH                 PROD_ROOT_PREFIX "/usr/sbin/ntpd"

extern const char prog_tail_grep_path[];
extern const char sysdump_generate_path[];
extern const char makemail_path[];
extern const char afail_path[];
extern const char ntpd_wrapper_path[];
extern const char ntpd_path[];

#define WRITEIMAGE_PATH          PROD_ROOT_PREFIX "/sbin/writeimage.sh"
#define REMANUFACTURE_PATH       PROD_ROOT_PREFIX "/sbin/remanufacture.sh"
#define IMGQ_PATH                PROD_ROOT_PREFIX "/sbin/imgq.sh"
#define TPKG_QUERY_PATH          PROD_ROOT_PREFIX "/sbin/tpkg_query.sh"
#define TPKG_EXTRACT_PATH        PROD_ROOT_PREFIX "/sbin/tpkg_extract.sh"
#define IMGTOISO_PATH            PROD_ROOT_PREFIX "/sbin/imgtoiso.sh"
#define RKN_PATH                 PROD_ROOT_PREFIX "/sbin/rkn.sh"
#define AIGET_PATH               PROD_ROOT_PREFIX "/sbin/aiget.sh"
#define AISET_PATH               PROD_ROOT_PREFIX "/sbin/aiset.sh"
#define VPART_PATH               PROD_ROOT_PREFIX "/sbin/vpart.sh"
#define DMIDUMP_PATH             PROD_ROOT_PREFIX "/sbin/dmidump.sh"
#define GET_LAYOUT_DISKS_PATH    PROD_ROOT_PREFIX "/sbin/get_layout_disks.sh"

extern const char writeimage_path[];
extern const char remanufacture_path[];
extern const char imgq_path[];
extern const char tpkg_query[];
extern const char tpkg_extract_path[];
extern const char imgtoiso_path[];
extern const char rkn_path[];
extern const char aiget_path[];
extern const char aiset_path[];
extern const char vpart_path[];
extern const char dmidump_path[];
extern const char get_layout_disks_path[];

#define MD_MODULE_PATH               PROD_STATIC_ROOT "/lib/md/modules"
#define CLI_MODULE_PATH              PROD_STATIC_ROOT "/lib/cli/modules"
#define SNMP_MODULE_PATH             PROD_STATIC_ROOT "/lib/snmp/modules"
#define STATS_MODULE_PATH            PROD_STATIC_ROOT "/lib/stats/modules"
#define WIZ_MODULE_PATH              PROD_STATIC_ROOT "/lib/wizard/modules"
#define FP_IO_MODULE_PATH            PROD_STATIC_ROOT "/lib/fpd/modules/io"
#define FP_UI_MODULE_PATH            PROD_STATIC_ROOT "/lib/fpd/modules/ui"
#define PRO_MODULE_PATH              PROD_STATIC_ROOT "/lib/progress/modules"

/*@}*/


/* ========================================================================= */
/**
 * @name Other directory and file paths: fixed/system
 *
 * These are paths of other directories and nonexecutable files, which
 * have fixed paths (i.e. do not want PROD_ROOT_PREFIX).  This is
 * generally because (a) they are part of the base system; (b) they do
 * not link to one of our generated files; (c) a user must be root to
 * create or modify them anyway; and (d) maybe a non-root user could
 * read them.  So it was judged better to share these in a development
 * environment.
 */
/*@{*/

/* ............................................................................
 * Zeroconf
 */

#define ZEROCONF_PID_FILE_DIR "/var/run"
#define ZEROCONF_PID_FILENAME_PREFIX "/var/run/zeroconf-"
#define ZEROCONF_PID_FILENAME_SUFFIX ".pid"
#define ZEROCONF_PID_FILENAME_FMT "/var/run/zeroconf-%s.pid"

extern const char zeroconf_pid_file_dir[];
extern const char zeroconf_pid_filename_prefix[];
extern const char zeroconf_pid_filename_suffix[];
extern const char zeroconf_pid_filename_fmt[];

/* ............................................................................
 * DHCP
 *
 * Note: we use different pid filenames for main dhclient, and 'release'
 * processes, lest we get the two confused with each other when reading
 * the pid file.
 */

#define DHCP_PID_FILE_DIR "/var/run"
#define DHCP_PID_FILENAME_SUFFIX ".pid"

#define DHCP_PID_FILENAME_PREFIX "/var/run/dhclient-"
#define DHCP_PID_FILENAME_FMT "/var/run/dhclient-%s.pid"
#define DHCP_PID_RELEASE_FILENAME_FMT "/var/run/dhclient-%s.release.pid"
#define DHCP_LEASE_FILENAME_FMT "/var/run/dhclient-%s.leases"
#define DHCP_CONFIG_PATH "/etc/dhcp/dhclient.conf"

#define DHCP6_PID_FILENAME_PREFIX "/var/run/dhclient6-"
#define DHCP6_PID_FILENAME_FMT "/var/run/dhclient6-%s.pid"
#define DHCP6_PID_RELEASE_FILENAME_FMT "/var/run/dhclient6-%s.release.pid"
#define DHCP6_LEASE_FILENAME_FMT "/var/run/dhclient6-%s.leases"
#define DHCP6_CONFIG_PATH "/etc/dhcp/dhclient6.conf"

extern const char dhcp_pid_file_dir[];
extern const char dhcp_pid_filename_suffix[];

extern const char dhcp_pid_filename_prefix[];
extern const char dhcp_pid_filename_fmt[];
extern const char dhcp_pid_release_filename_fmt[];
extern const char dhcp_lease_filename_fmt[];
extern const char dhcp_config_path[];

extern const char dhcp6_pid_filename_prefix[];
extern const char dhcp6_pid_filename_fmt[];
extern const char dhcp6_pid_release_filename_fmt[];
extern const char dhcp6_lease_filename_fmt[];
extern const char dhcp6_config_path[];

/* ............................................................................
 * Miscellaneous
 */

#define SYSTEM_LOG_PATH "/var/log"
#define FILESPEC_WEB_LOGS_PATH SYSTEM_LOG_PATH "/web_*_log"
#define SYSLOGD_PID_FILE_PATH "/var/run/syslogd.pid"
#define KLOGD_PID_FILE_PATH "/var/run/klogd.pid"
#define FILE_SYSLOG_PID_PATH "/var/run/syslogd.pid"

extern const char system_log_path[];
extern const char syslogd_pid_file_path[];
extern const char klogd_pid_file_path[];
extern const char filespec_web_logs_path[];
extern const char file_syslog_pid_path[];

#define CDROM_DEVICE_PATH "/dev/cdrom"
extern const char cdrom_device_path[];

/*
 * NOTE: currently this file is only written to from three sections of 
 * code, which cannot be running concurrently, so no file locking is
 * currently needed:
 *
 *   1. From md_systemlog_log_external().  This can be called either:
 *      (a) via md_systemlog_log() from a mgmtd module
 *      (b) via the "/logging/actions/systemlog/log" action, by GCL clients
 *          of mgmtd.
 *
 *   2. From firstboot.sh, while the system is booting, to record the 
 *      image version being booted.
 *
 *   3. From scrub.sh, while the system is being scrubbed, to record 
 *      the scrubbing.
 */
#define FILE_SYSTEMLOG_PATH SYSTEM_LOG_PATH "/systemlog";
extern const char file_systemlog_path[];

/*
 * Note: this value is also hardcoded (in escaped form) in md_syslog.c
 * in md_syslog_initial_values.
 */
#ifdef PROD_TARGET_OS_SUNOS
#define FILE_SYSLOG_PATH "/var/adm/messages"
#else
#define FILE_SYSLOG_PATH SYSTEM_LOG_PATH "/messages";
#endif

extern const char file_syslog_path[];

#define PROD_CRON_D_ROOT        MD_GEN_OUTPUT_PATH "/cron.d"
#define FILE_WTMP_PATH          "/var/log/wtmp"
#define FILE_BTMP_PATH          "/var/log/btmp"
#define NTP_WORKING_DIR         "/etc/ntp"
#define VCSA_WORKING_DIR        "/dev"
#define PCAP_WORKING_DIR        "/var/arpwatch"   /* XXX/EMT: does not exist */
#define NOBODY_WORKING_DIR      "/"
#define NFS_WORKING_DIR         "/var/lib/nfs"
#define RPCBIND_WORKING_DIR     "/var/cache/rpcbind"
#define FILE_HTTPD_PID_PATH     "/var/run/httpd.pid"

extern const char prod_cron_d_root[];
extern const char file_wtmp_path[];
extern const char file_btmp_path[];
extern const char ntp_working_dir[];
extern const char vcsa_working_dir[];
extern const char pcap_working_dir[];
extern const char file_httpd_pid_path[];

#ifdef PROD_TARGET_OS_SUNOS
#define ZONEINFO_PATH "/usr/share/lib/zoneinfo"
#else
#define ZONEINFO_PATH "/usr/share/zoneinfo"
#endif

extern const char zoneinfo_path[];

#define GETTEXT_DOMAIN_DIR "/usr/share/locale"
#define locale_avail_dir "/usr/lib/locale"

#define SKEL_DIR "/etc/skel"
#define SKEL_BASH_LOGOUT SKEL_DIR "/.bash_logout"
#define SKEL_BASH_PROFILE SKEL_DIR "/.bash_profile"
#define SKEL_BASHRC SKEL_DIR "/.bashrc"

extern const char skel_bash_logout_name[];
extern const char skel_bash_profile_name[];
extern const char skel_bashrc_name[];

#define path_standard "/usr/bin:/bin:/usr/sbin:/sbin"


/* ......................................................................... */
/* Where we get various stats from: /proc, /sys
 */
#ifdef PROD_TARGET_OS_LINUX

#define dir_proc "/proc"
#define file_interface_stats "/proc/net/dev"
#define file_route_ipv4_stats "/proc/net/route"
#define file_route_ipv6_stats "/proc/net/ipv6_route"
#define file_if_addr_ipv6 "/proc/net/if_inet6"
#define file_arp_stats "/proc/net/arp"
#define file_uptime_stats "/proc/uptime"
#define file_loadavg_stats "/proc/loadavg"
#define file_cpuinfo "/proc/cpuinfo"
#define file_meminfo_stats "/proc/meminfo"
#define file_vm_stats "/proc/vmstat"
#define file_disk_stats "/proc/diskstats"
#define file_general_stats "/proc/stat"
#define file_procinfo_pid_fmt "/proc/%d/status"
#define file_boot_id "/proc/sys/kernel/random/boot_id"
#define dir_sysctl_arp "/proc/sys/net/ipv4/neigh/default"
#define file_sysctl_arp_gc_thresh2 dir_sysctl_arp "/gc_thresh2"
#define file_sysctl_arp_gc_thresh3 dir_sysctl_arp "/gc_thresh3"
#define dir_proc_sys "/proc/sys"
#define file_pageio_stats "/proc/vmstat"

#define SYS_BLOCK_DEVICE_DIR "/sys/block"

/* The following bond and bridge defines are purposefully unconditional */

/* Test if an interface is a bond master, file is list of slaves */
#define BONDING_MASTER_SLAVES_PATH_FMT "/sys/class/net/%s/bonding/slaves"

/* Exists if an interface is a bond slave, symlink to bond master */
#define BONDING_SLAVE_MASTER_PATH_FMT "/sys/class/net/%s/master"

/* Exists if 'bridge' kernel module is loaded */
#define BRIDGE_ENABLED_PATH "/proc/sys/net/bridge"

/* Test if an interface is a bridge group */
#define BRIDGE_BRIDGEGROUP_PATH_FMT "/sys/class/net/%s/bridge"

/* Exists if an interface is a bridge member, symlink to bridgegroup */
#define BRIDGE_MEMBER_BRIDGEGROUP_PATH_FMT "/sys/class/net/%s/brport/bridge"

#ifdef PROD_FEATURE_BONDING
#define BONDING_MASTERS_PATH "/sys/class/net/bonding_masters"
#define BONDING_MASTER_MODE_PATH_FMT "/sys/class/net/%s/bonding/mode"
#define BONDING_MASTER_XMIT_HASH_POLICY_PATH_FMT "/sys/class/net/%s/bonding/xmit_hash_policy"
#define BONDING_MASTER_MIIMON_PATH_FMT "/sys/class/net/%s/bonding/miimon"
#define BONDING_MASTER_UPDELAY_PATH_FMT "/sys/class/net/%s/bonding/updelay"
#define BONDING_MASTER_DOWNDELAY_PATH_FMT "/sys/class/net/%s/bonding/downdelay"
#endif


#define SYS_INTERFACE_DIR "/sys/class/net"
extern const char sys_interface_dir[];

#define VLAN_CONFIG_PATH "/proc/net/vlan/config"
#define VLAN_STATS_DIR "/proc/net/vlan"
extern const char vlan_stats_dir[];

/*
 * Per-process stats.
 *
 * For /proc/PID/stat, see 'man proc' for info on contents.
 *
 * We only have #defines, not 'static const char []'s, because we want
 * gcc to be able to validate this format string with the printf args.
 */
#define FILE_PROC_STAT_FMT     "/proc/%" LPRI_d_ID_T "/stat"
#define FILE_PROC_STATUS_FMT   "/proc/%" LPRI_d_ID_T "/status"
#define FILE_PROC_IO_STATS_FMT "/proc/%" LPRI_d_ID_T "/io"
#define FILE_PROC_LIMIT_FMT    "/proc/%" LPRI_d_ID_T "/limits"
#define FILE_PROC_CWD          "/proc/%" LPRI_d_ID_T "/cwd"
#define DIR_PROC_FDS_FMT       "/proc/%" LPRI_d_ID_T "/fd"

#else /* PROD_TARGET_OS_LINUX */

#define file_pageio_stats "/proc/stat"

#endif /* ! PROD_TARGET_OS_LINUX */

#define FILE_DEV_ZERO "/dev/zero"
extern const char file_dev_zero[];

/*@}*/


/* ========================================================================= */
/**
 * @name Other directory and file paths: variable
 *
 * These are paths of other directories and nonexecutable files,
 * which are created or read by Samara components, and which 
 * should generally be prefixed with PROD_ROOT_PREFIX so they can
 * be used in a development environment.
 */
/*@{*/

#define FILE_ETC_PASSWD_PATH      MD_GEN_OUTPUT_PATH "/passwd"
#define FILE_ETC_GROUP_PATH       MD_GEN_OUTPUT_PATH "/group"
#define FILE_ETC_SHADOW_PATH      MD_GEN_OUTPUT_PATH "/shadow"

extern const char file_etc_passwd_path[];
extern const char file_etc_group_path[];
extern const char file_etc_shadow_path[];

#define FILE_LOGROTATE_CONF_PATH  PROD_ROOT_PREFIX "/etc/logrotate.conf"
#define FILE_LOGROTATE_CRONTAB    PROD_ROOT_PREFIX "/etc/cron.d/logrotate"
#define DIR_LOGROTATE_D_CONF_PATH PROD_ROOT_PREFIX "/etc/logrotate.d"
#define PAM_RADIUS_AUTH_CONF_FILENAME "pam_radius_auth.conf"
#define PAM_RADIUS_AUTH_CONF_PATH PROD_ROOT_PREFIX "/etc/raddb/" PAM_RADIUS_AUTH_CONF_FILENAME
#define SNMP_CONFIG_FILE_NAME     "snmpd.conf"
#define SNMP_CONFIG_FILE_PATH    PROD_ROOT_PREFIX "/etc/" SNMP_CONFIG_FILE_NAME
#define SSMTP_CONFIG_FILE         PROD_ROOT_PREFIX "/etc/ssmtp.conf"
#define SSMTP_CONFIG_FILE_AUTO    PROD_ROOT_PREFIX "/etc/ssmtp-auto.conf"
#define PM_PID_FILE_PATH          PROD_ROOT_PREFIX "/var/run/pm.pid"
#define SYSTEM_LOG_DEV_PATH       PROD_ROOT_PREFIX "/var/log"
#define FILE_HTTPD_PID_DEV_PATH   PROD_ROOT_PREFIX "/var/run/httpd.pid"

#define PAM_SYSTEM_AUTH_PATH      MD_GEN_OUTPUT_PATH "/system-auth"
#define PAM_TACACS_CONF_PATH      MD_GEN_OUTPUT_PATH "/pam_tacplus_server.conf"
#define PAM_LDAP_CONF_PATH        MD_GEN_OUTPUT_PATH "/pam_ldap.conf"

extern const char file_logrotate_conf_path[];
extern const char file_logrotate_crontab[];
extern const char dir_logrotate_d_conf_path[];
extern const char pam_radius_auth_conf_filename[];
extern const char pam_radius_auth_conf_path[];
extern const char snmp_config_file_name[];
extern const char snmp_config_file_path[];
extern const char ssmtp_config_file[];
extern const char ssmtp_config_file_auto[];
extern const char pm_pid_file_path[];
extern const char system_log_dev_path[];
extern const char file_httpd_pid_dev_path[];

extern const char pam_system_auth_path[];
extern const char pam_tacacs_conf_path[];
extern const char pam_ldap_conf_path[];


/*
 * These are abstract names for UNIX domain sockets, meaning they do not
 * exist in the filesystem.  The first character will be replaced with '\0'
 * to signify this.
 */
#define PM_SELF_DETECT_SOCK_PATH  PROD_ROOT_PREFIX "#pm-running"
extern const char pm_self_detect_sock_path[];

#define IMAGE_DIR_PATH     PROD_VAR_ROOT "/images"
#define IMAGE_TEMP_DIR_PATH     PROD_VAR_ROOT "/images/.temp"
#define SYSDUMPS_DIR_PATH  PROD_VAR_ROOT "/sysdumps"
#define SYSDUMP_FILENAME_PREFIX "sysdump-"
#define TCPDUMPS_DIR_PATH  PROD_VAR_ROOT "/tcpdumps"
#define PM_SNAPSHOT_DIR    PROD_VAR_ROOT "/snapshots"
#define PM_RUNNING_DIR     PM_SNAPSHOT_DIR "/.running"
#define PM_STAGING_DIR     PM_SNAPSHOT_DIR "/.staging"
#define WEB_GRAPHS_PATH    PROD_STATIC_ROOT "/lib/web/content/graphs"
extern const char image_dir_path[];
extern const char image_temp_dir_path[];
extern const char sysdumps_dir_path[];
extern const char sysdump_filename_prefix[];
extern const char tcpdumps_dir_path[];
extern const char pm_snapshot_dir[];
extern const char pm_running_dir[];
extern const char pm_staging_dir[];
#ifdef PROD_FEATURE_GRAPHING
extern const char web_graphs_path[];
#endif /* PROD_FEATURE_GRAPHING */

#define CLISH_WORKING_DIR             PM_RUNNING_DIR "/cli"
#define CGI_LAUNCHER_WORKING_DIR      PM_RUNNING_DIR "/launch"
#define REQUEST_HANDLER_WORKING_DIR   PM_RUNNING_DIR "/rh"
#define HTTPD_WORKING_DIR             PM_RUNNING_DIR "/httpd"
#define SCHED_WORKING_DIR             PM_RUNNING_DIR "/sched"
#define SCHED_OUTPUT_DIR_PATH         PROD_VAR_ROOT "/sched"
#define WIZARD_WORKING_DIR            PM_RUNNING_DIR "/wizard"
#define USE_WIZARD_PATH               PROD_VAR_ROOT "/.usewizard"

extern const char clish_working_dir[];
extern const char cgi_launcher_working_dir[];
extern const char request_handler_working_dir[];
extern const char httpd_working_dir[];
#ifdef PROD_FEATURE_SCHED
extern const char sched_working_dir[];
extern const char sched_output_dir_path[];
#endif
#ifdef PROD_FEATURE_WIZARD
extern const char wizard_working_dir[];
#endif

#define MD_MFG_PATH       PROD_CONFIG_ROOT "/mfg"
#define MD_MFDB_PATH      MD_MFG_PATH "/mfdb"
#define MD_MFINCDB_PATH   MD_MFG_PATH "/mfincdb"

extern const char md_mfg_path[];
extern const char md_mfdb_path[];
extern const char md_mfincdb_path[];

#define FILE_LOCALTIME_PATH  MD_GEN_OUTPUT_PATH "/localtime"
#define FILE_LOCALE_PATH     MD_GEN_OUTPUT_PATH "/locale"

extern const char file_localtime_path[];
extern const char file_locale_path[];

#define LAU_SCRIPT_HANDLER_PATH      PROD_STATIC_ROOT "/lib/web/handlers"
#define WEB_CONTENT_PATH             PROD_STATIC_ROOT "/lib/web/content"
#define WEB_MODULE_PATH              PROD_STATIC_ROOT "/lib/web/modules"
#define WEB_ADDON_TEMPLATE_DIR_PATH  PROD_STATIC_ROOT "/lib/web/templates/add-on"
#define WEB_FONTS_PATH               PROD_STATIC_ROOT "/lib/web/fonts"
#define WEB_IMAGES_PATH              PROD_STATIC_ROOT "/lib/web/content/images"
#define WEB_HTTPD_ROOT               PROD_STATIC_ROOT "/lib/web"
#define WEB_HTTPD_CONF_NAME         "httpd.conf"
#define WEB_HTTPD_CONF_PATH         MD_GEN_OUTPUT_PATH "/" WEB_HTTPD_CONF_NAME
#define WEB_MIME_TYPES_PATH         PROD_STATIC_ROOT "/lib/web/conf/mime.types"
#define WEB_SSL_CERT_PATH           MD_GEN_OUTPUT_PATH "/web/conf"
#define WEB_SSL_HOST_FILE_PATH      WEB_SSL_CERT_PATH "/server.hostname"
#define WEB_SSL_CERT_FILE_PATH      WEB_SSL_CERT_PATH "/server.crt"
#define WEB_SSL_CERT_KEY_FILE_PATH  WEB_SSL_CERT_PATH "/server.key"
#define WEB_RH_DEBUG_PATH           MD_GEN_OUTPUT_PATH "/rhdebug"
#define WEB_LAU_DEBUG_PATH          MD_GEN_OUTPUT_PATH "/laudebug"
#define XG_DEBUG_PATH               MD_GEN_OUTPUT_PATH "/xgdebug"

extern const char lau_script_handler_path[];
extern const char web_content_path[];
extern const char web_module_path[];
extern const char web_addon_template_dir_path[];
extern const char web_fonts_path[];
extern const char web_images_path[];
extern const char web_httpd_root[];
extern const char web_httpd_conf_name[];
extern const char web_httpd_conf_path[];
extern const char web_mime_types_path[];
extern const char web_ssl_cert_path[];
extern const char web_ssl_cert_file_path[];
extern const char web_ssl_host_file_path[];
extern const char web_ssl_cert_key_file_path[];

/*
 * This is the directory from which we expose system images for download.
 * Note that it is different from the one clustering uses for itself,
 * "sysimages".  Note also that "images" cannot be used here because
 * we already have a top-level "images" directory (under the content
 * directory).
 *
 * XXX/EMT: this value is mirrored in setup-upgrade.tem.
 */
#define WEB_SYSTEM_IMAGES_DIR "system_images"
extern const char web_system_images_dir[];

#define CERT_MGR_CACHE_DIR_NAME         "certs"
#define CERT_MGR_CACHE_DIR_PATH         MD_GEN_OUTPUT_PATH "/" \
    CERT_MGR_CACHE_DIR_NAME
#define CERT_MGR_CACHE_CERT_EXT         "crt"
#define CERT_MGR_CACHE_PRIV_KEY_EXT     "key"
#define CERT_MGR_CA_CERT_DIR_NAME       "cacerts"
#define CERT_MGR_DEFAULT_CACERTS_DIR_PATH MD_GEN_OUTPUT_PATH "/" \
    CERT_MGR_CA_CERT_DIR_NAME
#define CERT_MGR_SYSTEM_CERT_LINK_NAME  "system_cert"
#define CERT_MGR_DEFAULT_CERT_LINK_NAME "default_cert"
#define CERT_MGR_SYSTEM_CERT_LINK_PATH  CERT_MGR_CACHE_DIR_PATH "/" \
    CERT_MGR_SYSTEM_CERT_LINK_NAME
#define CERT_MGR_DEFAULT_CERT_LINK_PATH CERT_MGR_CACHE_DIR_PATH "/" \
    CERT_MGR_DEFAULT_CERT_LINK_NAME

/*
 * Package of CA certs that comes with the base OS.
 */
#define CERT_CLIENT_DEFAULT_TLS_CACERTFILE "/etc/pki/tls/cert.pem"

extern const char cert_mgr_cache_dir_path[];
extern const char cert_mgr_cache_cert_ext[];
extern const char cert_mgr_cache_priv_key_ext[];
extern const char cert_mgr_default_cacerts_dir_path[];
extern const char cert_mgr_system_cert_link_path[];
extern const char cert_mgr_default_cert_link_path[];
extern const char cert_client_default_tls_cacertfile[];

#define STATS_DATA_FILE_PATH    PROD_VAR_ROOT "/stats"
#define STATS_REPORT_FILE_PATH  PROD_VAR_ROOT "/stats/reports"
#define LWT_TEMPLATE_DIR_PATH   PROD_STATIC_ROOT "/lib/web/templates"
#define GRAPHS_FILE_PATH        PROD_VAR_ROOT "/web/graphs"

extern const char stats_data_file_path[];
extern const char stats_report_file_path[];
extern const char lwt_template_dir_path[];
extern const char graphs_file_path[];

#define RACOON_CONF_NAME "racoon.conf"
#define RACOON_CONF_PATH MD_GEN_OUTPUT_PATH "/racoon/" RACOON_CONF_NAME
#define RACOON_STATIC_DIR   PROD_ROOT_PREFIX "/etc/racoon"
#define RACOON_PSK_PATH     RACOON_STATIC_DIR "/psk.txt"
#define RACOON_CERTS_PATH   RACOON_STATIC_DIR "/certs"

/* ln -s /var/opt/tms/output/pluto/ipsec.conf /etc/ipsec.conf (main file) */
/* ln -s /var/opt/tms/output/pluto/ipsec.d/ /etc/ipsec.d/     (included)  */
/* /etc/ipsec.secrets to include /etc/ipsec.d/ipsec.secrets/              */
/* /etc/ipsec.conf to include /etc/ipsec.d/ipsec.conf/                    */
#define PLUTO_CONF_MAIN_DIR MD_GEN_OUTPUT_PATH "/pluto"
#define PLUTO_CONF_CONN_DIR   PLUTO_CONF_MAIN_DIR "/ipsec.d"
#define PLUTO_CONF_FILENAME "ipsec.conf"
#define PLUTO_CONF_MAIN_FILE_PATH PLUTO_CONF_MAIN_DIR "/" PLUTO_CONF_FILENAME
#define PLUTO_CONF_CONN_FILE_PATH PLUTO_CONF_CONN_DIR "/" PLUTO_CONF_FILENAME
#define PLUTO_CONF_SECRETS_FILENAME "ipsec.secrets"
#define PLUTO_CONF_CERTS_FILENAME "certs"
#define PLUTO_CONF_CONN_SECRETS_FILE_PATH PLUTO_CONF_CONN_DIR "/" \
                                          PLUTO_CONF_SECRETS_FILENAME
#define PLUTO_CONF_CONN_CERTS_FILE_PATH PLUTO_CONF_CONN_DIR "/" \
                                        PLUTO_CONF_CERTS_FILENAME
#define PLUTO_PID_FILENAME "/var/run/pluto/pluto.pid"

#ifdef PROD_FEATURE_CRYPTO
extern const char racoon_conf_name[];
extern const char racoon_conf_path[];
extern const char racoon_static_dir[];
extern const char racoon_psk_path[];
extern const char racoon_certs_path[];

extern const char pluto_conf_main_file_path[];
extern const char pluto_conf_conn_file_path[];
extern const char pluto_conf_conn_secrets_file_path[];
extern const char pluto_conf_conn_certs_file_path[];
extern const char pluto_pid_filename[];
extern const char pluto_wait_filename[];
#endif /* PROD_FEATURE_CRYPTO */

#ifdef PROD_FEATURE_ACCOUNTING
/* future placeholder */
#endif /* PROD_FEATURE_ACCOUNTING */

#define SYSTEM_UNEXPECTED_SHUTDOWN_PATH  PROD_VAR_ROOT "/.unexpected_shutdown"
#define SYSTEM_DETECTED_UNEXPECTED_SHUTDOWN_PATH  PROD_VAR_ROOT "/.detected_unexpected_shutdown"
#define LCGI_TMP_TEMPLATE_PATH PROD_ROOT_PREFIX "/var/tmp/lcgi_tmp_file_XXXXXX"
#define CLI_TMP_SYSDUMP_PATH   PROD_TMP_ROOT "/sysdump_body"
#define MD_TMP_EMAIL_BODY_PATH   PROD_TMP_ROOT "/email_body"
#define FONTS_PATH             PROD_STATIC_ROOT "/lib/fonts"
#define XINETD_CONF_NAME       "xinetd.conf"
#define XINETD_CONF_PATH       MD_GEN_OUTPUT_PATH "/" XINETD_CONF_NAME
#define SYSLOG_LOGLEVEL_NAME   "sysloglevel"
#define SYSLOG_LOGLEVEL_MGMTD_NAME   "sysloglevel_mgmtd"
#define LOGGING_SYSTEM_OPTIONS_NAME "log_options"
#define RESTRICTED_CMDS_LICENSE_FILENAME  "restricted_cmds_license"

/* XXX/EMT: mirrored in resetpw.sh */
#define SYSTEM_ADMIN_PASSWORD_RESET_PATH PROD_VAR_ROOT "/.admin_pw_reset"

/*
 * Filename to append to directory into which we want to place the output
 * of a process run with lc_launch().  This is not concatenated with the
 * default path (PROD_VTMP_ROOT) here, because the library lets the
 * caller override which directory to use.
 */
#define PROC_OUTPUT_FILENAME  "proc-output"
extern const char proc_output_filename[];

#define FILE_SYSLOG_LOGLEVEL_PATH MD_GEN_OUTPUT_PATH "/" SYSLOG_LOGLEVEL_NAME
#define FILE_SYSLOG_LOGLEVEL_MGMTD_PATH MD_GEN_OUTPUT_PATH "/" SYSLOG_LOGLEVEL_MGMTD_NAME
#define FILE_LOGGING_SYSTEM_OPTIONS_PATH MD_GEN_OUTPUT_PATH "/" LOGGING_SYSTEM_OPTIONS_NAME
#define FILE_RESTRICTED_CMDS_LICENSE_PATH MD_GEN_OUTPUT_PATH "/" RESTRICTED_CMDS_LICENSE_FILENAME

extern const char lcgi_tmp_template_path[];
extern const char cli_tmp_sysdump_path[];
extern const char md_tmp_email_body_path[];
extern const char fonts_path[];
extern const char xinetd_conf_name[];
extern const char xinetd_conf_path[];
extern const char file_syslog_loglevel_path[];
extern const char file_syslog_loglevel_mgmtd_path[];
extern const char file_logging_system_options_path[];
extern const char file_restricted_cmds_license_path[];

#define BOOTMGR_PASSWD_FILENAME "bootmgr_pass"
#define BOOTMGR_PASSWD_PATH MD_GEN_OUTPUT_PATH "/" BOOTMGR_PASSWD_FILENAME
#define UBOOT_ROOT PROD_BOOTMGR_ROOT "/u-boot"
#define UBOOT_IMAGE_CONFIG_PATH_A UBOOT_ROOT "/uic-a.conf"
#define UBOOT_IMAGE_CONFIG_PATH_B UBOOT_ROOT "/uic-b.conf"

extern const char bootmgr_passwd_filename[];
extern const char bootmgr_passwd_path[];
extern const char uboot_image_config_path_a[];
extern const char uboot_image_config_path_b[];

#define PROD_CONFIG_DB_ROOT PROD_CONFIG_ROOT "/db"
extern const char md_db_path[];

#define PROD_CONFIG_TEXT_ROOT PROD_CONFIG_ROOT "/text"
extern const char prod_config_text_root[];

#define MD_TEMPLATE_PATH PROD_STATIC_ROOT "/lib/md/templates"
extern const char md_template_path[];

/* Directory where (gcl) unix domain sockets are made */
#define GCL_SOCK_PATH PROD_VTMP_ROOT "/gcl-socks/"
extern const char gcl_sock_path[];

#define HOME_ROOT_DIR          PROD_ROOT_PREFIX "/var/home"

/* NOTE: this path is also hardwired into afail.sh */
#define MD_EMAIL_NOTIFIES_PATH MD_GEN_OUTPUT_PATH "/notifies.txt"
extern const char md_email_notifies_path[];

extern const char home_root_dir[];

#define SSHD_WORKING_DIR        PROD_ROOT_PREFIX "/var/empty/sshd"
#define SSH_GLOBAL_KNOWN_HOSTS_PATH PROD_ROOT_PREFIX "/etc/ssh/ssh_known_hosts"
#define SSH_HOST_KEY_DIR_PATH   PROD_ROOT_PREFIX "/var/lib/ssh"
#define RSA1_HOST_KEY_PATH      SSH_HOST_KEY_DIR_PATH "/ssh_host_key"
#define RSA1_HOST_PUB_KEY_PATH  SSH_HOST_KEY_DIR_PATH "/ssh_host_key.pub"
#define RSA2_HOST_KEY_PATH      SSH_HOST_KEY_DIR_PATH "/ssh_host_rsa_key"
#define RSA2_HOST_PUB_KEY_PATH  SSH_HOST_KEY_DIR_PATH "/ssh_host_rsa_key.pub"
#define DSA2_HOST_KEY_PATH      SSH_HOST_KEY_DIR_PATH "/ssh_host_dsa_key"
#define DSA2_HOST_PUB_KEY_PATH  SSH_HOST_KEY_DIR_PATH "/ssh_host_dsa_key.pub"
#define GPG_PUBLIC_KEY_PATH     PROD_ROOT_PREFIX "/etc/pki/tms-gpg/GPG-KEY"

extern const char sshd_working_dir[];
extern const char ssh_global_known_hosts_path[];
extern const char ssh_host_key_dir_path[];
extern const char rsa1_host_key_path[];
extern const char rsa1_host_pub_key_path[];
extern const char rsa2_host_key_path[];
extern const char rsa2_host_pub_key_path[];
extern const char dsa2_host_key_path[];
extern const char dsa2_host_pub_key_path[];
extern const char gpg_public_key_path[];

#define LAST_DOBINCP_PATH "/opt/tms/release/last_dobincp"
extern const char last_dobincp_path[];

#ifdef PROD_FEATURE_VIRT

#define VIRT_DIR_PATH             PROD_VAR_ROOT "/virt"
#define VM_SAVE_DIR_PATH          VIRT_DIR_PATH "/vm_save"
#define VM_SAVE_TEMP_DIR_PATH     VIRT_DIR_PATH "/vm_save/.temp"
#define VIRT_CONF_PATH MD_GEN_OUTPUT_PATH "/libvirt"
#define VIRT_LIBVIRTD_CONF_FILENAME "libvirtd.conf"
#define VIRT_QEMU_CONF_FILENAME "qemu.conf"
#define LIBVIRT_LOGS_SPEC "/var/log/libvirt/qemu/*.log"
#define VIRT_POOLS_DIR_PATH VIRT_DIR_PATH "/pools"

extern const char virt_conf_path[];
extern const char virt_libvirtd_conf_filename[];
extern const char virt_qemu_conf_filename[];
extern const char vm_save_dir_path[];
extern const char vm_save_temp_dir_path[];
extern const char libvirt_logs_spec[];
extern const char virt_pools_dir_path[];

/*
 * This can be overridden from customer.h.
 */
#ifndef VIRT_DEFAULT_POOL
#define VIRT_DEFAULT_POOL         "default"
#endif

#define VIRT_POOL_TEMP_DIR          ".temp"
#define VIRT_POOL_TEMP_NAME_SUFFIX  "-TEMP"

#endif /* PROD_FEATURE_VIRT */

/* ............................................................................
 * CLI-related files
 */
#define CLI_RC_LINK_PATH PROD_ROOT_PREFIX "/etc/clirc"
extern const char cli_rc_link_path[];

#define CLI_RC_FILE_PATH MD_GEN_OUTPUT_PATH "/clirc"
extern const char cli_rc_file_path[];

#define CLISH_HISTORY_FILENAME ".cli_history"
extern const char clish_history_filename[];

/*@}*/


/* ========================================================================= */
/**
 * @name File modes, owners, and other non-path information
 */
/*@{*/

#define MODE_LOGROTATE_CRONTAB 0644
enum { graph_file_mode = 0640 };
enum { graph_file_uid = (uid_t) 0 };
enum { graph_file_gid = (gid_t) 48 }; /* Matches number in md_passwd.c */

enum { sched_output_dir_mode = 0755 };
enum { sched_output_file_mode = 0600 };

enum { ssh_dir_priv_mode = 0700 };
enum { ssh_file_priv_mode = 0600 };
enum { ssh_file_mode = 0644 };
enum { ssh_hostkey_file_owner = (uid_t) 0 };
enum { ssh_hostkey_file_group = (gid_t) 0 };

enum { cert_mgr_cache_cert_file_mode = 0644 };
enum { cert_mgr_cache_priv_key_file_mode = 0640 };
enum { cert_mgr_cache_file_owner = (uid_t) 0 };
enum { cert_mgr_cache_file_group = (gid_t) 0 };

enum { web_ssl_host_file_mode = 0644 };
enum { web_ssl_host_file_owner = (uid_t) 0 };
enum { web_ssl_host_file_group = (gid_t) 48 };

enum { md_gen_output_path_mode = 0755 };
enum { md_gen_conf_file_owner = (uid_t) -1 };  /* XXX do not set for now */
enum { md_gen_conf_file_group = (gid_t) -1 };
enum { md_gen_conf_file_mode = 0644 };
enum { md_gen_conf_file_priv_mode = 0600 };

enum { crypto_file_priv_mode = 0600 };
enum { crypto_file_mode = 0644 };
enum { crypto_file_owner = (uid_t) 0 };
enum { crypto_file_group = (gid_t) 0 };

enum { md_db_path_mode = 0755 };
enum { md_db_file_owner = (uid_t) -1 };  /* XXX do not set for now */
enum { md_db_file_group = (gid_t) -1 };
enum { md_db_file_mode = 0640 };
enum { md_active_file_owner = (uid_t) -1 };  /* XXX do not set for now */
enum { md_active_file_group = (gid_t) -1 };
enum { md_active_file_mode = 0644 };

enum { gcl_sock_socket_mode = 0777 };
enum { use_wizard_mode = 0644 };

#ifdef PROD_FEATURE_VIRT
#define VIRT_IMAGE_FILE_MODE_STR   "0660"
#define VIRT_IMAGE_FILE_OWNER_STR  "0"
#define VIRT_IMAGE_FILE_GROUP_STR  "3002" /* mgt_virt */

#define VIRT_POOL_FILE_MODE        0660
#define VIRT_POOL_FILE_OWNER       0
#define VIRT_POOL_FILE_GROUP       mgt_virt

extern const char virt_image_file_mode_str[];
extern const char virt_image_file_owner_str[];
extern const char virt_image_file_group_str[];

extern const uint32 virt_pool_file_mode;
extern const uint32 virt_pool_file_owner;
extern const uint32 virt_pool_file_group;

/* Modes for saved Virtual Machine state files */
enum {
    vi_saved_vm_owner = (uid_t) 0,
    vi_saved_vm_group = (gid_t) mgt_virt,
    vi_saved_vm_mode =          0640
};
#endif

/* Modes, etc., to be given to stats files */
#define STATS_FILE_MODE   0640
#define STATS_FILE_OWNER  0
#define STATS_FILE_GROUP  mgt_stats

extern const uint32 stats_file_mode;
extern const uid_t stats_file_owner;
extern const gid_t stats_file_group;

enum {
    /*
     * These directory modes are a bit odd.  Note that the owner is
     * apache.diag.  Our goals are:
     *
     *   (a) Let admin read and write the directory.  This happens
     *       automatically, even though it is not reflected in the 
     *       owner/group/mode, because admin is uid 0.
     *
     *   (b) Under older releases (Fir, and early Ginkgo before the overlay
     *       with this functionality was released), make the directory 
     *       always readable by Apache.  Cluster upgrade depends on this,
     *       and we don't want to break that if someone upgrades to a newer
     *       release, and then downgrades.  So Apache can read the directory
     *       because the owner has read permissions.
     *
     *   (c) Under newer releases, make the directory served by Apache 
     *       iff image serving is enabled.  Since the directory is always
     *       readable by Apache, this is accomplished instead by rewriting
     *       the Apache config file (see httpd.conf template and md_web.c)
     *
     *   (d) Under newer releases with PROD_FEATURE_ACLS, make the directory
     *       readable by users in the 'diag' auth groups iff image serving
     *       is enabled.  This is accomplished by fixing the group read/exec
     *       bits according to the image serving flag.
     */
    md_image_dir_mode_serve_all_disabled = 0500,
    md_image_dir_mode_serve_all_enabled = 0550,

    /*
     * We control access to image files by the directory, not by
     * individual file permissions.  We want files to always be
     * world-readable.
     */
    md_image_file_mode = 0644,

    /*
     * As with images, access to the directory is controlled.
     * But be extra careful and leave world-readability off these,
     * in case they get copied elsewhere.
     */
    md_tcpdump_file_mode = 0640,

};

/*@}*/


/* ========================================================================= */
/**
 * @name CMC: Central Management Console feature
 */
/*@{*/

#ifdef PROD_FEATURE_CMC_CLIENT
#define RENDV_CLIENT_PATH PROD_STATIC_ROOT "/bin/rendv_client"
extern const char rendv_client_path[];
#endif /* PROD_FEATURE_CMC_CLIENT */

#ifdef PROD_FEATURE_CMC_SERVER
#define RBMD_PATH PROD_STATIC_ROOT "/bin/rbmd"
extern const char rbmd_path[];
#endif /* PROD_FEATURE_CMC_SERVER */

#define RGP_PATH          PROD_STATIC_ROOT_NO_PREFIX "/bin/rgp"
#define RGP_RENDV_PATH    PROD_STATIC_ROOT "/bin/rgp_rendv"
#define RGP_RAW_PATH      PROD_STATIC_ROOT "/bin/rgp_raw"
#define RGP_WORKING_DIR   PM_RUNNING_DIR "/rgp"
#define RGP_DEBUG_PATH    MD_GEN_OUTPUT_PATH "/rgpdebug"

extern const char rgp_path[];
extern const char rgp_rendv_path[];
extern const char rgp_raw_path[];
extern const char rgp_working_dir[];

#if defined(PROD_FEATURE_CMC_CLIENT) || defined(PROD_FEATURE_CMC_SERVER) || \
    defined(PROD_FEATURE_REMOTE_GCL)

/*
 * These constants are mirrored in Makefiles that create directories
 * in the image, e.g. src/base_os/linux_el/el6/image_files/Makefile .
 */
#define CMC_SSH_DIR_PATH                PROD_VAR_ROOT "/cmc/ssh"
#define CMC_SSH_KNOWN_HOSTS_PATH        CMC_SSH_DIR_PATH "/known_hosts"
#define CMC_SSH_KEY_DIR_PATH            CMC_SSH_DIR_PATH "/keys"
#define CMC_SSH_TRUSTED_HOSTS_PATH      PROD_VAR_ROOT "/cmc/trusted_hosts"
#define CMC_SSH_TRUSTED_HOSTS_SIG_PATH  PROD_VAR_ROOT "/cmc/trusted_hosts.sig"

/*
 * XXX/EMT: not yet used, pending bug 14858.
 */
#define CMC_PREFERRED_IMAGE_NAME        "cmc-image.img"
#define CMC_PREFERRED_IMAGE_PATH        IMAGE_DIR_PATH "/" CMC_PREFERRED_IMAGE_NAME

extern const char cmc_ssh_dir_path[];
extern const char cmc_ssh_known_hosts_path[];
extern const char cmc_ssh_key_dir_path[];
extern const char cmc_ssh_trusted_hosts_path[];
extern const char cmc_ssh_trusted_hosts_sig_path[];
extern const char cmc_preferred_image_name[];
extern const char cmc_preferred_image_path[];

#endif /* PROD_FEATURE_CMC_CLIENT || PROD_FEATURE_CMC_SERVER ||
          PROD_FEATURE_REMOTE_GCL */

/*@}*/


/* ========================================================================= */
/**
 * @name Clustering/HA feature
 */
/*@{*/

#ifdef PROD_FEATURE_CLUSTER
#define CLUSTERD_PATH          PROD_STATIC_ROOT "/bin/clusterd"
#define CL_UPGRADE_PATH        PROD_ROOT_PREFIX "/sbin/cluster_upgrade_wrapper.sh"
/* NOTE: this cluster output path is burned into a few places, like templates */
#define CL_UPGRADE_OUTPUT_PATH PROD_ROOT_PREFIX "/tmp/cluster_upgrade.out"
#define CL_CLUSTERWIDE_PATH    PROD_ROOT_PREFIX "/sbin/cluster_wide.tcl"
#define CL_DISCO_MDNS_PATH           PROD_ROOT_PREFIX "/usr/bin/mDNSResponder"
#define CL_DISCO_MDNS_PID_PATH       PROD_ROOT_PREFIX "/var/run/mDNSResponder.pid"
#define CL_DB_MASTER_CFG_SYNC_PATH  PROD_CONFIG_DB_ROOT "/master_cfg_sync"
#define CL_DB_MASTER_CFG_SYNC_INCR_PATH  PROD_CONFIG_DB_ROOT "/master_cfg_sync_incr"
#define CL_MASTER_TIMESTAMP_PATH PROD_ROOT_PREFIX "/var/run/cluster_master_tstamp"

extern const char clusterd_path[];
extern const char cl_upgrade_path[];
extern const char cl_upgrade_output_path[];
extern const char cl_clusterwide_path[];
extern const char cl_disco_mdns_path[];
extern const char cl_disco_mdns_pid_path[];
extern const char cl_db_master_cfg_sync_path[];
extern const char cl_db_master_cfg_sync_incr_path[];
extern const char cl_master_timestamp_path[];

#endif /* PROD_FEATURE_CLUSTER */

/*@}*/

#ifdef __cplusplus
}
#endif

#endif /* __TPATHS_H_ */
