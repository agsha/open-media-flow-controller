# Notes
#
# Juniper added comment:
#   These base_excludes are used by default when processing an rpm UNLESS you
#   create a variable "$nobaseexcludes_<name>=1" for a particular rpm.

@base_excludes=('/usr/share/doc/', '/usr/share/man/', '/usr/share/info/',
                '/usr/share/locale/', '/usr/man/', '/usr/doc/', 
                '/usr/kerberos/man/',
                '/usr/include/', '/etc/sysconfig/', '/etc/X11/', 
                '/usr/share/misc/', '/usr/lib*/debug/', '/etc/cron.d/',
                '/usr/lib/pkgconfig/'
                );

# There can only be includes or excludes for a package, not both

@excludes_bash=('/usr/bin/bashbug', '/etc/skel/.[a-zA-Z]*');

@includes_binutils=('/usr/bin/gprof', '/usr/bin/nm',
                    '/usr/bin/addr2line', '/usr/bin/strings',
                    '/usr/bin/objdump',
                    '/usr/lib*/libbfd-[0-9\.]*.so',
                    '/usr/lib*/libopcodes-[0-9\.]*.so');

@includes_kernel_utils=('/etc/rc.d/init.d/smartd', '/usr/sbin/smartd',
                        '/usr/sbin/smartctl');

@excludes_chkconfig=('/usr/sbin/alternatives', 
                     '/usr/sbin/update-alternatives', 
                     '/etc/alternatives',
                     '/var/lib/alternatives');

@includes_cyrus_sasl_lib=('/usr/lib*/lib*.so.*');

@excludes_dev=('/dev/ataraid/',
               '/dev/cciss/',
               '/dev/compaq/',
               '/dev/ida/',
               '/dev/i2o/',
               '/dev/rd/',
               '/dev/isdn*',
               '/dev/ippp*',
               '/dev/tty[A-RTV-Z]*',
               '/dev/xd*',
               '/dev/fd[2-9]*',
               '/dev/hd[m-z]*',
               '/dev/sd[a-z][a-z]*'
               );

@excludes_diff=('/usr/bin/diff3', '/usr/bin/sdiff');

@includes_device_mapper=('/lib*/lib*.so*', '/lib/udev/rules.d/*', '/sbin/dmsetup');

@excludes_e2fsprogs=('/sbin/resize2fs', '/sbin/debugfs');

@excludes_expat=('/usr/bin/');

@excludes_file_libs=('/usr/share/man/');
$nobaseexcludes_file_libs=1;

@includes_flac=('/usr/lib*/lib*.so.*');

@includes_fuse_libs=('/lib64/libfuse.so*');

@excludes_gawk=('/usr/share/', '/usr/libexec/', '/bin/pgawk', '/bin/igawk');

@excludes_gdb=('/usr/lib*/', '/usr/bin/gdbtui');

@excludes_glibc=('/usr/lib*/gconv/', '/sbin/sln', 
                 '/usr/sbin/glibc_post_upgrade', 
                 '/usr/sbin/iconvconfig*');

@includes_glibc_common=('/usr/share/zoneinfo/', '/usr/bin/ldd', 
                        '/usr/bin/lddlibc4');

if ("$BUILD_PROD_FEATURE_I18N_SUPPORT" eq "1") {
    push @includes_glibc_common, '/usr/bin/locale';
    @locales = split(' ', $BUILD_PROD_LOCALES);
    foreach $locale (@locales) {
        push @includes_glibc_common, "/usr/lib/locale/$locale.utf8/";
    }
}

# Order matters, as we may have different combinations of these two features
if ("$BUILD_PROD_FEATURE_CMC_SERVER" eq "1") {
    @includes_gnupg2=('/usr/bin/gpgv', '/usr/bin/gpgv2');
}
if ("$BUILD_PROD_FEATURE_IMAGE_SECURITY" eq "1") {
    @includes_gnupg2=('/usr/bin/gpg', '/usr/bin/gpg2', '/usr/bin/gpgv', '/usr/bin/gpgv2');
}

@excludes_grub=('/boot/', '/usr/share/grub/*/stage2_eltorito',
                '/usr/bin/mbchk');

@includes_hwdata=('/usr/share/hwdata/pci.ids');

# XXXX-EL6: must revisit initscripts excludes !
## @excludes_initscripts=(
##                       '/etc/rc.d/init.d/netfs', '/etc/rc.d/init.d/network',
## XXXX-EL6: what about /etc/sysconfig/ ???

@excludes_initscripts=('/usr/share/doc/',
                       '/usr/share/man/',
                       '/usr/share/info/',
                       '/usr/share/locale/',
                       '/etc/ppp/',
                       '/sbin/ifdown',
                       '/sbin/ifup',
                       '/sbin/ppp-watch',
                       '/etc/rc.d/init.d/netfs',
                       '/etc/rc.d/init.d/network',
                       '/usr/sbin/usernetctl',
                       '/sbin/netreport',
                       '/var/log/',
                       '/var/run/',
                       '/etc/udev/rules.d/',
                       '/etc/init/plymouth-shutdown.conf',
                       '/etc/init/quit-plymouth.conf',
                       '/etc/init/prefdm.conf',
                       '/etc/init/splash-manager.conf',
                       '/etc/X11/',
                       '/etc/NetworkManager/',
                       );


$nobaseexcludes_initscripts=1;

@excludes_iproute=('/etc/iproute2/rt_tables', '/usr/sbin/arpd');

if ("$BUILD_PROD_FEATURE_CRYPTO" eq "1") {
    @includes_ipsec_tools=('/sbin/setkey', '/usr/sbin/racoon',
                           '/usr/sbin/racoonctl');
    @excludes_openswan=('/etc/ipsec.d*');
}

##@execs_iptables=("${BUILD_PROD_TREE_ROOT}/src/base_os/${BUILD_TARGET_PLATFORM_LC}/${BUILD_TARGET_PLATFORM_VERSION_LC}/script_files/iptables-postinstall.sh iptables");

##@execs_iptables_ipv6=("${BUILD_PROD_TREE_ROOT}/src/base_os/${BUILD_TARGET_PLATFORM_LC}/${BUILD_TARGET_PLATFORM_VERSION_LC}/script_files/iptables-postinstall.sh ip6tables");

@execs_java_1_6_0_openjdk=("${BUILD_PROD_TREE_ROOT}/src/base_os/${BUILD_TARGET_PLATFORM_LC}/${BUILD_TARGET_PLATFORM_VERSION_LC}/script_files/openjdk-postinstall.sh ${BUILD_TARGET_ARCH}");

@includes_java_1_6_0_openjdk_devel=('/usr/lib/jvm/java-*-openjdk-*/bin/jdb', '/usr/lib/jvm/java-*-openjdk-*/lib/tools.jar');

@execs_java_1_6_0_openjdk_devel=("${BUILD_PROD_TREE_ROOT}/src/base_os/${BUILD_TARGET_PLATFORM_LC}/${BUILD_TARGET_PLATFORM_VERSION_LC}/script_files/openjdk-devel-postinstall.sh ${BUILD_TARGET_ARCH}");

@includes_kbd=('/bin/setfont',
               '/lib/kbd/consolefonts/alt-8x8.gz',
               '/lib/kbd/consolefonts/alt-8x14.gz',
               '/lib/kbd/consolefonts/alt-8x16.gz');

@excludes_krb5_libs=('/etc/rc.d/', '/usr/kerberos/share/');

@includes_libjpeg=('/usr/lib*/lib*.so.*');

@includes_libsndfile=('/usr/lib*/lib*.so.*');

@includes_libuser=('/usr/lib*/libuser.so.*',
                   '/usr/lib*/libuser/libuser_files.so',
                   '/usr/lib*/libuser/libuser_ldap.so',
                   '/usr/lib*/libuser/libuser_shadow.so');

@includes_libxml2=('/usr/lib*/libxml2*.so.*');

@excludes_libvirt=('/etc/libvirt', '/etc/libvirt/', '/etc/rc.d/init.d/', '/usr/libexec/libvirt_proxy');

@excludes_libvirt_client=('/etc/libvirt', '/etc/libvirt/', '/etc/rc.d/init.d/');

@excludes_logrotate=('/etc/cron.daily/logrotate');

@excludes_lvm2=('/usr/lib*/*.a', '/usr/lib*/*.a.*');

@includes_mesa_libGL=('/usr/lib*/libGL.so.*');

@includes_genisoimage=('/usr/bin/genisoimage');
@execs_genisoimage=("${BUILD_PROD_TREE_ROOT}/src/base_os/${BUILD_TARGET_PLATFORM_LC}/${BUILD_TARGET_PLATFORM_VERSION_LC}/script_files/genisoimage-postinstall.sh");

@excludes_ncurses=('/usr/bin/tack', '/usr/bin/infocmp', '/usr/bin/toe',
                   '/usr/bin/tic');

@excludes_net_tools=('/sbin/slattach', '/sbin/plipconfig');

@excludes_ntp=('/etc/rc.d/', '/etc/ntp.conf', '/etc/dhcp/');

@includes_ntpdate=('/usr/sbin/ntpdate');

@includes_openssh_clients=('/usr/bin/sftp');

@includes_openldap=('/lib*/lib*.so.*');

@excludes_pam=('/lib*/security/pam_userdb.so',
               '/lib*/security/pam_pwdb.so', '/sbin/pwdb_chkpwd',
               '/sbin/pam_timestamp_check');

@excludes_passwd=('/usr/bin/');

@excludes_pcre=('/usr/bin/pcretest', '/usr/bin/pcregrep');

@includes_popt=('/lib*/libpopt.so.*');

@excludes_procps=('/usr/bin/oldps');

@includes_rcs=('/usr/bin/ident');

@includes_rrdtool=('/usr/bin/rrdtool', '/usr/bin/rrdupdate', 
                   '/usr/lib*/librrd.so.* ', '/usr/lib*/librrd.so.*.*.*',
                   '/usr/share/rrdtool/fonts/*');

@excludes_rsync=('/etc/xinetd.d/');

@includes_sharutils=('/usr/bin/uuencode', '/usr/bin/uudecode');

@includes_syslinux=('/usr/share/syslinux/isolinux.bin');

@excludes_sysstat=('/etc/cron.d/sysstat');

@execs_tcpdump=("${BUILD_PROD_TREE_ROOT}/src/base_os/${BUILD_TARGET_PLATFORM_LC}/${BUILD_TARGET_PLATFORM_VERSION_LC}/script_files/tcpdump-postinstall.sh");

@includes_telnet_server=('/usr/sbin/in.telnetd');

@includes_unzip=('/usr/bin/zipinfo', '/usr/bin/unzip');

@excludes_usermode=('/usr/sbin/userhelper');

@excludes_util_linux_ng=('/usr/bin/chfn', '/usr/bin/chsh');

@includes_vsftpd=('/usr/sbin/vsftpd', '/etc/pam.d/vsftpd');

@includes_xorg_x11_xauth=('/usr/bin/xauth');

@includes_xinetd=('/usr/sbin/xinetd');

# Juniper added
# Set the nobaseexcludes_OpenIPMI variable so that only the excludes listed here are used,
# because we want the file /etc/sysconfig/ipmi
@excludes_OpenIPMI=('/usr/share/', '/etc/rc.d/init.d/');
$nobaseexcludes_OpenIPMI=1;

# Juniper added
# Set the nobaseexcludes_ipmitool variable so that only the excludes listed here are used,
# because we want the file /etc/sysconfig/ipmievd
@excludes_ipmitool=('/usr/share/', '/etc/rc.d/init.d/');
$nobaseexcludes_ipmitool=1;

# Juniper added
# Set the nobaseexcludes_irqbalance variable so that only the excludes listed here are used,
# because we want the file /etc/sysconfig/irqbalance
@excludes_irqbalance=('/usr/share/', '/etc/rc.d/init.d/');
$nobaseexcludes_irqbalance=1;

# Juniper added
# Set the nobaseexcludes_lm_sensors variable so that only the excludes listed here are used,
# because we want the file /etc/sysconfig/lm_sensors
@excludes_lm_sensors=('/usr/share/');
$nobaseexcludes_lm_sensors=1;

