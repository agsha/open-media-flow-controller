#  Makefile
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

# This makefile converts rpm's to tar's on 'all' and dumps the tar's into
# the image install tree on 'install'

DIST_SCL_6=/volume/ssd-linux-storage01/repo/ssdlinux/scl6/release/6.6.R1.0
DIST_SCL_6_I386=${DIST_SCL_6}/i386/rpm
DIST_SCL_6_X86_64=${DIST_SCL_6}/x86_64/rpm
DIST_SCL_6_SRPMS=${DIST_SCL_6}/srpm

DIST_MODIFIED_SCL_6=${PROD_DIST_ROOT}/modified/SCL6/arch_x86_64

DIST_OTHER_X86_64=${PROD_DIST_ROOT}/other/${PROD_TARGET_PLATFORM_LC}/${PROD_TARGET_PLATFORM_VERSION_LC}/arch_x86_64

CHECK_BINARY_RPMS_FILE=../packages/pkg_bin_check.txt

PACKAGES_BINARY_RPMS= \
	${DIST_SCL_6_X86_64}/sysvinit-tools-2.87-5.dsf.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_I386}/audit-libs-2.3.7-5.SCLC6_6.R1.0.1.i686.rpm \
	${DIST_SCL_6_X86_64}/audit-libs-2.3.7-5.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/bash-4.1.2-29.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/bc-1.06.95-1.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/binutils-2.20.51.0.2-5.42.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/bind-utils-9.8.2-0.30.rc1.SCLC6_6.2.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/bzip2-1.0.5-7.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_I386}/bzip2-libs-1.0.5-7.SCLC6_6.R1.0.1.i686.rpm \
	${DIST_SCL_6_X86_64}/bzip2-libs-1.0.5-7.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_I386}/ca-certificates-2014.1.98-65.1.SCLC6_6.R1.0.1.noarch.rpm \
	${DIST_SCL_6_X86_64}/chkconfig-1.3.49.3-2.SCLC6_6.1.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/coreutils-8.4-37.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/cpio-2.10-12.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_I386}/curl-7.19.7-40.SCLC6_6.4.R1.0.1.i686.rpm \
	${DIST_SCL_6_X86_64}/curl-7.19.7-40.SCLC6_6.4.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_I386}/cyrus-sasl-lib-2.1.23-15.SCLC6_6.2.R1.0.1.i686.rpm \
	${DIST_SCL_6_X86_64}/cyrus-sasl-lib-2.1.23-15.SCLC6_6.2.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/dbus-libs-1.2.24-7.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/device-mapper-1.02.90-2.SCLC6_6.1.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_I386}/device-mapper-libs-1.02.90-2.SCLC6_6.1.R1.0.1.i686.rpm \
	${DIST_SCL_6_X86_64}/device-mapper-libs-1.02.90-2.SCLC6_6.1.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_I386}/device-mapper-event-libs-1.02.90-2.SCLC6_6.1.R1.0.1.i686.rpm \
	${DIST_SCL_6_X86_64}/device-mapper-event-libs-1.02.90-2.SCLC6_6.1.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/dialog-1.1-9.20080819.1.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/diffutils-2.8.1-28.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/dmidecode-2.12-5.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/dosfstools-3.0.9-4.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/e2fsprogs-1.41.12-21.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_I386}/e2fsprogs-libs-1.41.12-21.SCLC6_6.R1.0.1.i686.rpm \
	${DIST_SCL_6_X86_64}/e2fsprogs-libs-1.41.12-21.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/ed-1.1-3.3.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_I386}/elfutils-libelf-0.158-3.2.SCLC6_6.R1.0.1.i686.rpm \
	${DIST_SCL_6_X86_64}/elfutils-libelf-0.158-3.2.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/ethtool-3.5-5.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_I386}/expat-2.0.1-11.SCLC6_6.R1.0.1.i686.rpm \
	${DIST_SCL_6_X86_64}/expat-2.0.1-11.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/file-5.04-21.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/file-libs-5.04-21.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/findutils-4.4.2-6.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/ftp-0.17-54.SCLC6_6.R1.0.1.x86_64.rpm \
        ${DIST_SCL_6_X86_64}/fuse-libs-2.8.3-4.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/gawk-3.1.7-10.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_OTHER_X86_64}/gdb-7.2-56.el6.tms.1.x86_64.rpm \
	${DIST_OTHER_X86_64}/gdb-gdbserver-7.2-56.el6.tms.1.x86_64.rpm \
	${DIST_SCL_6_I386}/glib2-2.28.8-4.SCLC6_6.R1.0.1.i686.rpm \
	${DIST_MODIFIED_SCL_6}/glib2-2.28.8-4.SCLC6_6.R1.0.1_mfc.1.x86_64.rpm \
	${DIST_SCL_6_I386}/glibc-2.12-1.149.SCLC6_6.5.R1.0.1.i686.rpm \
	${DIST_SCL_6_X86_64}/glibc-2.12-1.149.SCLC6_6.5.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/glibc-common-2.12-1.149.SCLC6_6.5.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/gmp-4.3.1-7.SCLC6_6.2.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/grep-2.6.3-6.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/gzip-1.3.12-22.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/hdparm-9.43-4.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_I386}/hwdata-0.233-11.1.SCLC6_6.R1.0.1.noarch.rpm \
	${DIST_SCL_6_X86_64}/i2c-tools-3.1.0-3.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/initscripts-9.03.46-1.SCLC6_6.1.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/iproute-2.6.32-33.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/iputils-20071127-17.SCLC6_6.2.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/kbd-1.15-11.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_I386}/keyutils-libs-1.4-5.SCLC6_6.R1.0.1.i686.rpm \
	${DIST_SCL_6_X86_64}/keyutils-libs-1.4-5.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_I386}/krb5-libs-1.10.3-33.SCLC6_6.R1.0.1.i686.rpm \
	${DIST_SCL_6_X86_64}/krb5-libs-1.10.3-33.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/less-436-13.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_I386}/libacl-2.2.49-6.SCLC6_6.R1.0.1.i686.rpm \
	${DIST_SCL_6_X86_64}/libacl-2.2.49-6.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_I386}/libattr-2.4.44-7.SCLC6_6.R1.0.1.i686.rpm \
	${DIST_SCL_6_X86_64}/libattr-2.4.44-7.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/libblkid-2.17.2-12.18.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_I386}/libcap-2.16-5.5.SCLC6_6.R1.0.1.i686.rpm \
	${DIST_SCL_6_X86_64}/libcap-2.16-5.5.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_I386}/libcap-ng-0.6.4-3.SCLC6_6.1.R1.0.1.i686.rpm \
	${DIST_SCL_6_X86_64}/libcap-ng-0.6.4-3.SCLC6_6.1.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_I386}/libcom_err-1.41.12-21.SCLC6_6.R1.0.1.i686.rpm \
	${DIST_SCL_6_X86_64}/libcom_err-1.41.12-21.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/libcurl-7.19.7-40.SCLC6_6.4.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/libedit-2.11-4.20080712cvs.1.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_I386}/libgcc-4.4.7-11.SCLC6_6.R1.0.1.i686.rpm \
	${DIST_SCL_6_X86_64}/libgcc-4.4.7-11.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_I386}/libgssglue-0.1-11.SCLC6_6.R1.0.1.i686.rpm \
	${DIST_SCL_6_X86_64}/libgssglue-0.1-11.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_I386}/libidn-1.18-2.SCLC6_6.R1.0.1.i686.rpm \
	${DIST_SCL_6_X86_64}/libidn-1.18-2.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_I386}/libnih-1.0.1-7.SCLC6_6.R1.0.1.i686.rpm \
	${DIST_SCL_6_X86_64}/libnih-1.0.1-7.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/libpcap-1.4.0-1.20130826git2dbcaa1.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_I386}/libselinux-2.0.94-5.8.SCLC6_6.R1.0.1.i686.rpm \
	${DIST_SCL_6_X86_64}/libselinux-2.0.94-5.8.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_I386}/libsepol-2.0.41-4.SCLC6_6.R1.0.1.i686.rpm \
	${DIST_SCL_6_X86_64}/libsepol-2.0.41-4.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/libssh2-1.4.2-1.SCLC6_6.1.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_I386}/libstdc++-4.4.7-11.SCLC6_6.R1.0.1.i686.rpm \
	${DIST_SCL_6_X86_64}/libstdc++-4.4.7-11.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_I386}/libsysfs-2.1.0-7.SCLC6_6.R1.0.1.i686.rpm \
	${DIST_SCL_6_X86_64}/libsysfs-2.1.0-7.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_I386}/libtirpc-0.2.1-10.SCLC6_6.R1.0.1.i686.rpm \
	${DIST_SCL_6_X86_64}/libtirpc-0.2.1-10.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_I386}/libudev-147-2.57.SCLC6_6.R1.0.1.i686.rpm \
	${DIST_SCL_6_X86_64}/libudev-147-2.57.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_I386}/libuser-0.56.13-5.SCLC6_6.R1.0.1.i686.rpm \
	${DIST_SCL_6_X86_64}/libuser-0.56.13-5.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_I386}/libuuid-2.17.2-12.18.SCLC6_6.R1.0.1.i686.rpm \
	${DIST_SCL_6_X86_64}/libuuid-2.17.2-12.18.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/libutempter-1.1.5-4.1.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/lm_sensors-3.1.1-17.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/lm_sensors-libs-3.1.1-17.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/logrotate-3.7.8-17.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/lrzsz-0.12.20-27.1.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/lsof-4.82-4.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/ltrace-0.5-28.45svn.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/mingetty-1.08-5.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/module-init-tools-3.9-24.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_I386}/ncurses-5.7-3.20090208.SCLC6_6.R1.0.1.i686.rpm \
	${DIST_SCL_6_X86_64}/ncurses-5.7-3.20090208.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/ncurses-base-5.7-3.20090208.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_I386}/ncurses-libs-5.7-3.20090208.SCLC6_6.R1.0.1.i686.rpm \
	${DIST_SCL_6_X86_64}/ncurses-libs-5.7-3.20090208.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/net-tools-1.60-110.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/nspr-4.10.6-1.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/nss-3.16.2.3-3.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/nss-softokn-3.14.3-22.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_I386}/nss-softokn-freebl-3.14.3-22.SCLC6_6.R1.0.1.i686.rpm \
	${DIST_SCL_6_X86_64}/nss-softokn-freebl-3.14.3-22.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/nss-util-3.16.2.3-2.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/ntp-4.2.6p5-3.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/ntpdate-4.2.6p5-3.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/openldap-2.4.39-8.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/openssh-clients-5.3p1-104.SCLC6_6.1.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_I386}/openssl-1.0.1e-30.SCLC6_6.7.R1.0.1.i686.rpm \
	${DIST_SCL_6_X86_64}/openssl-1.0.1e-30.SCLC6_6.7.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_I386}/pam-1.1.1-20.SCLC6_6.R1.0.1.i686.rpm \
	${DIST_SCL_6_X86_64}/pam-1.1.1-20.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/passwd-0.77-4.SCLC6_6.2.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/patch-2.6-6.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/pax-3.4-10.1.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/pciutils-3.1.10-4.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/pciutils-libs-3.1.10-4.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_I386}/pcre-7.8-6.SCLC6_6.R1.0.1.i686.rpm \
	${DIST_SCL_6_X86_64}/pcre-7.8-6.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_I386}/popt-1.13-7.SCLC6_6.R1.0.1.i686.rpm \
	${DIST_SCL_6_X86_64}/popt-1.13-7.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/procps-3.2.8-30.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/psmisc-22.6-19.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/python-libs-2.6.6-52.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/python-crypto-2.0.1-22.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/python-paramiko-1.7.5-2.1.SCLC6_6.R1.0.1.noarch.rpm \
	${DIST_SCL_6_X86_64}/rcs-5.7-37.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_I386}/readline-6.0-4.SCLC6_6.R1.0.1.i686.rpm \
	${DIST_SCL_6_X86_64}/readline-6.0-4.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/rsync-3.0.6-12.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/sed-4.2.1-10.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/setserial-2.17-25.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_I386}/setup-2.8.14-20.SCLC6_6.1.R1.0.1.noarch.rpm \
	${DIST_SCL_6_X86_64}/sharutils-4.7-6.1.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/sqlite-3.6.20-1.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/strace-4.5.19-1.19.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/sysstat-9.0.4-27.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/tar-1.23-11.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_I386}/tcl-8.5.7-6.SCLC6_6.R1.0.1.i686.rpm \
	${DIST_SCL_6_X86_64}/tcl-8.5.7-6.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_I386}/tcp_wrappers-7.6-57.SCLC6_6.R1.0.1.i686.rpm \
	${DIST_SCL_6_X86_64}/tcp_wrappers-7.6-57.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_I386}/tcp_wrappers-libs-7.6-57.SCLC6_6.R1.0.1.i686.rpm \
	${DIST_SCL_6_X86_64}/tcp_wrappers-libs-7.6-57.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/tcpdump-4.0.0-3.20090921gitdf3cb4.2.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/tcsh-6.17-25.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/telnet-0.17-48.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/tftp-0.49-7.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/time-1.7-37.1.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/traceroute-2.0.14-2.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_I386}/tzdata-2015b-1.SCLC6_6.R1.0.1.noarch.rpm \
	${DIST_SCL_6_X86_64}/udev-147-2.57.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/unzip-6.0-2.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/upstart-0.6.5-13.SCLC6_6.3.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/util-linux-ng-2.17.2-12.18.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/vconfig-1.9-8.1.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/vim-minimal-7.2.411-1.8.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/which-2.19-6.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_I386}/zlib-1.2.3-29.SCLC6_6.R1.0.1.i686.rpm \
	${DIST_SCL_6_X86_64}/zlib-1.2.3-29.SCLC6_6.R1.0.1.x86_64.rpm \


# Skip iptables, because we install a newer version from a tar file
#	${DIST_SCL_6_X86_64}/iptables-1.4.7-14.SCLC6_6.R1.0.1.x86_64.rpm \
#	${DIST_SCL_6_X86_64}/iptables-ipv6-1.4.7-14.SCLC6_6.R1.0.1.x86_64.rpm \

ifneq (${PROD_FEATURE_WGET},0)
	PACKAGES_BINARY_RPMS+= \
		${DIST_SCL_6_X86_64}/wget-1.12-5.SCLC6_6.1.R1.0.1.x86_64.rpm \

endif

ifneq (${PROD_FEATURE_GRAPHING},0)
	PACKAGES_BINARY_RPMS+= \
		${DIST_SCL_6_I386}/freetype-2.3.11-15.SCLC6_6.1.R1.0.1.i686.rpm \
		${DIST_SCL_6_X86_64}/freetype-2.3.11-15.SCLC6_6.1.R1.0.1.x86_64.rpm \
		${DIST_SCL_6_I386}/libpng-1.2.49-1.SCLC6_6.R1.0.1.i686.rpm \
		${DIST_SCL_6_X86_64}/libpng-1.2.49-1.SCLC6_6.R1.0.1.x86_64.rpm \

endif

ifneq (${PROD_FEATURE_OLD_GRAPHING},0)
	PACKAGES_BINARY_RPMS+= \
		${DIST_OTHER_X86_64}/rrdtool-1.2.23-1.el5.rf.x86_64.rpm \
		${DIST_SCL_6_X86_64}/libart_lgpl-2.3.20-5.1.SCLC6_6.R1.0.1.x86_64.rpm \

endif

ifneq (${PROD_FEATURE_JAVA},0)
	PACKAGES_BINARY_RPMS+= \
		${DIST_SCL_6_X86_64}/java-1.6.0-openjdk-1.6.0.34-1.13.6.1.SCLC6_6.R1.0.1.x86_64.rpm \
		${DIST_SCL_6_X86_64}/java-1.6.0-openjdk-devel-1.6.0.34-1.13.6.1.SCLC6_6.R1.0.1.x86_64.rpm \

endif

ifneq (${PROD_FEATURE_SMARTD},0)
	PACKAGES_BINARY_RPMS+= \
		${DIST_SCL_6_X86_64}/smartmontools-5.43-1.SCLC6_6.R1.0.1.x86_64.rpm \


endif


ifneq (${PROD_FEATURE_XINETD},0)
	PACKAGES_BINARY_RPMS+= \
		${DIST_SCL_6_X86_64}/xinetd-2.3.14-39.SCLC6_6.R1.0.1.x86_64.rpm \

# ftpd and telnetd require xinetd
ifneq (${PROD_FEATURE_FTPD},0)
	PACKAGES_BINARY_RPMS+= \
		${DIST_UNKNOWN}/vsftpd-0.UNKNOWN.x86_64.rpm \

endif

ifneq (${PROD_FEATURE_TELNETD},0)
	PACKAGES_BINARY_RPMS+= \
		${DIST_SCL_6_X86_64}/telnet-server-0.17-48.SCLC6_6.R1.0.1.x86_64.rpm \

endif

endif # ! PROD_FEATURE_XINETD

ifneq (${PROD_FEATURE_BRIDGING},0)
	PACKAGES_BINARY_RPMS+= \
		${DIST_SCL_6_X86_64}/bridge-utils-1.2-10.SCLC6_6.R1.0.1.x86_64.rpm \
		${DIST_SCL_6_X86_64}/sysfsutils-2.1.0-7.SCLC6_6.R1.0.1.x86_64.rpm \

endif # ! PROD_FEATURE_BRIDGING

ifneq (${PROD_FEATURE_CRYPTO},0)
	ifeq (${CRYPTO_INCLUDE_RACOON},1)
		PACKAGES_BINARY_RPMS+= \
			${DIST_OTHER_X86_64}/ipsec-tools-0.6.5-14.el6.5.x86_64.rpm \

	endif

	ifeq (${CRYPTO_INCLUDE_OPENSWAN},1)
		PACKAGES_BINARY_RPMS+= \
			${DIST_SCL_6_X86_64}/fipscheck-lib-1.2.0-7.SCLC6_6.R1.0.1.x86_64.rpm \
			${DIST_OTHER_X86_64}/openswan-2.6.38dr2-9.el6.x86_64.rpm \

	endif

endif # ! PROD_FEATURE_CRYPTO

ifneq (${PROD_FEATURE_CMC_SERVER},0)
	PACKAGES_BINARY_RPMS+= \
		${DIST_SCL_6_X86_64}/gnupg2-2.0.14-8.SCLC6_6.R1.0.1.x86_64.rpm \
		${DIST_SCL_6_X86_64}/pth-2.0.7-9.3.SCLC6_6.R1.0.1.x86_64.rpm \
		${DIST_SCL_6_X86_64}/libgcrypt-1.4.5-11.SCLC6_6.R1.0.1.x86_64.rpm \
		${DIST_SCL_6_X86_64}/libgpg-error-1.7-4.SCLC6_6.R1.0.1.x86_64.rpm \

endif # ! PROD_FEATURE_CMC_SERVER

ifneq (${PROD_FEATURE_IMAGE_SECURITY},0)
	PACKAGES_BINARY_RPMS+= \
		${DIST_SCL_6_X86_64}/libusb-0.1.12-23.SCLC6_6.R1.0.1.x86_64.rpm \

endif # ! PROD_FEATURE_IMAGE_SECURITY

ifeq (${PROD_FEATURE_IMAGE_SECURITY}${PROD_FEATURE_CMC_SERVER},10)
	PACKAGES_BINARY_RPMS+= \
		${DIST_SCL_6_X86_64}/gnupg2-2.0.14-8.SCLC6_6.R1.0.1.x86_64.rpm \
		${DIST_SCL_6_X86_64}/pth-2.0.7-9.3.SCLC6_6.R1.0.1.x86_64.rpm \
		${DIST_SCL_6_X86_64}/libgcrypt-1.4.5-11.SCLC6_6.R1.0.1.x86_64.rpm \
		${DIST_SCL_6_X86_64}/libgpg-error-1.7-4.SCLC6_6.R1.0.1.x86_64.rpm \

endif # PROD_FEATURE_IMAGE_SECURITY && ! PROD_FEATURE_CMC_SERVER

# Note that cyrus-sasl-lib is potentially added above
ifneq (${PROD_FEATURE_VIRT},0)
	PACKAGES_BINARY_RPMS+= \
		${DIST_SCL_6_X86_64}/avahi-libs-0.6.25-15.SCLC6_6.R1.0.1.x86_64.rpm \
		${DIST_SCL_6_X86_64}/cyrus-sasl-2.1.23-15.SCLC6_6.2.R1.0.1.x86_64.rpm \
		${DIST_SCL_6_X86_64}/cyrus-sasl-md5-2.1.23-15.SCLC6_6.2.R1.0.1.x86_64.rpm \
		${DIST_SCL_6_X86_64}/dbus-1.2.24-7.SCLC6_6.R1.0.1.x86_64.rpm \
		${DIST_SCL_6_X86_64}/dbus-glib-0.86-6.SCLC6_6.R1.0.1.x86_64.rpm \
		${DIST_SCL_6_X86_64}/dnsmasq-2.48-14.SCLC6_6.R1.0.1.x86_64.rpm \
		${DIST_SCL_6_X86_64}/gnutls-2.8.5-14.SCLC6_6.R1.0.1.x86_64.rpm \
		${DIST_SCL_6_X86_64}/libtasn1-2.3-6.SCLC6_6.R1.0.1.x86_64.rpm \
		${DIST_SCL_6_X86_64}/hal-0.5.14-14.SCLC6_6.R1.0.1.x86_64.rpm \
		${DIST_SCL_6_X86_64}/hal-libs-0.5.14-14.SCLC6_6.R1.0.1.x86_64.rpm \
		${DIST_SCL_6_X86_64}/iscsi-initiator-utils-6.2.0.873-13.SCLC6_6.R1.0.1.x86_64.rpm \
		${DIST_SCL_6_X86_64}/libICE-1.0.6-1.SCLC6_6.R1.0.1.x86_64.rpm \
		${DIST_SCL_6_X86_64}/libSM-1.2.1-2.SCLC6_6.R1.0.1.x86_64.rpm \
		${DIST_SCL_6_X86_64}/libXmu-1.1.1-2.SCLC6_6.R1.0.1.x86_64.rpm \
		${DIST_SCL_6_X86_64}/libgcrypt-1.4.5-11.SCLC6_6.R1.0.1.x86_64.rpm \
		${DIST_SCL_6_X86_64}/libgpg-error-1.7-4.SCLC6_6.R1.0.1.x86_64.rpm \
		${DIST_SCL_6_X86_64}/libvirt-0.10.2-46.SCLC6_6.3.R1.0.1.x86_64.rpm \
		${DIST_SCL_6_X86_64}/libvirt-client-0.10.2-46.SCLC6_6.3.R1.0.1.x86_64.rpm \
		${DIST_SCL_6_X86_64}/lvm2-2.02.111-2.SCLC6_6.1.R1.0.1.x86_64.rpm \
		${DIST_SCL_6_X86_64}/nc-1.84-22.SCLC6_6.R1.0.1.x86_64.rpm \
		${DIST_SCL_6_X86_64}/nfs-utils-1.2.3-54.SCLC6_6.R1.0.1.x86_64.rpm \
		${DIST_SCL_6_X86_64}/numactl-2.0.9-2.SCLC6_6.R1.0.1.x86_64.rpm \
		${DIST_SCL_6_X86_64}/tigervnc-1.1.0-16.SCLC6_6.R1.0.1.x86_64.rpm \
		${DIST_SCL_6_X86_64}/xorg-x11-xauth-1.0.2-7.1.SCLC6_6.R1.0.1.x86_64.rpm \
		${DIST_SCL_6_X86_64}/libxcb-1.9.1-2.SCLC6_6.R1.0.1.x86_64.rpm \
		${DIST_SCL_6_X86_64}/libaio-0.3.107-10.SCLC6_6.R1.0.1.x86_64.rpm \
		${DIST_SCL_6_X86_64}/libnl-1.1.4-2.SCLC6_6.R1.0.1.x86_64.rpm \
		${DIST_SCL_6_X86_64}/libpciaccess-0.13.3-0.1.SCLC6_6.R1.0.1.x86_64.rpm \
		${DIST_SCL_6_X86_64}/netcf-libs-0.2.4-1.SCLC6_6.R1.0.1.x86_64.rpm \
		${DIST_SCL_6_X86_64}/spice-server-0.12.4-11.SCLC6_6.R1.0.1.x86_64.rpm \
		${DIST_SCL_6_X86_64}/usbredir-0.5.1-1.SCLC6_6.R1.0.1.x86_64.rpm \
		${DIST_SCL_6_X86_64}/yajl-1.0.7-3.SCLC6_6.R1.0.1.x86_64.rpm \
		${DIST_SCL_6_X86_64}/pulseaudio-libs-0.9.21-17.SCLC6_6.R1.0.1.x86_64.rpm \
		${DIST_SCL_6_X86_64}/augeas-libs-1.0.0-7.SCLC6_6.1.R1.0.1.x86_64.rpm \
		${DIST_SCL_6_X86_64}/libxslt-1.1.26-2.SCLC6_6.1.R1.0.1.x86_64.rpm \
		${DIST_SCL_6_X86_64}/libXtst-1.2.2-2.1.SCLC6_6.R1.0.1.x86_64.rpm \
		${DIST_SCL_6_X86_64}/libasyncns-0.8-1.1.SCLC6_6.R1.0.1.x86_64.rpm \
		${DIST_SCL_6_X86_64}/libjpeg-turbo-1.2.1-3.SCLC6_6.R1.0.1.x86_64.rpm \
		${DIST_SCL_6_X86_64}/libsndfile-1.0.20-5.SCLC6_6.R1.0.1.x86_64.rpm \
		${DIST_SCL_6_X86_64}/pixman-0.32.4-4.SCLC6_6.R1.0.1.x86_64.rpm \
		${DIST_SCL_6_X86_64}/flac-1.2.1-6.1.SCLC6_6.R1.0.1.x86_64.rpm \
		${DIST_SCL_6_X86_64}/libXi-1.7.2-2.2.SCLC6_6.R1.0.1.x86_64.rpm \
		${DIST_SCL_6_X86_64}/libogg-1.1.4-2.1.SCLC6_6.R1.0.1.x86_64.rpm \
		${DIST_SCL_6_X86_64}/libvorbis-1.2.3-4.SCLC6_6.1.R1.0.1.x86_64.rpm \
		${DIST_SCL_6_X86_64}/gpxe-roms-qemu-0.9.7-6.12.SCLC6_6.R1.0.1.noarch.rpm \
		${DIST_SCL_6_X86_64}/seabios-0.6.1.2-28.SCLC6_6.R1.0.1.x86_64.rpm \
		${DIST_SCL_6_X86_64}/sgabios-bin-0-0.3.20110621svn.SCLC6_6.R1.0.1.noarch.rpm \
		${DIST_SCL_6_X86_64}/vgabios-0.6b-3.7.SCLC6_6.R1.0.1.noarch.rpm \

	# Need for virt, but not part of libvirt or kvm
	PACKAGES_BINARY_RPMS+= \
		${DIST_SCL_6_X86_64}/genisoimage-1.1.9-12.SCLC6_6.R1.0.1.x86_64.rpm \
		${DIST_SCL_6_X86_64}/syslinux-4.04-3.SCLC6_6.R1.0.1.x86_64.rpm \


endif # PROD_FEATURE_VIRT

#
# Since kvm is 64-bit only, we don't have i386 versions of these RPMs,
# even those that are present on CentOS mirrors.
#
ifneq (${PROD_FEATURE_VIRT_KVM},0)
	PACKAGES_BINARY_RPMS+= \
		${DIST_SCL_6_X86_64}/SDL-1.2.14-3.SCLC6_6.R1.0.1.x86_64.rpm \
		${DIST_SCL_6_X86_64}/alsa-lib-1.0.22-3.SCLC6_6.R1.0.1.x86_64.rpm \
		${DIST_SCL_6_X86_64}/audiofile-0.2.6-11.1.SCLC6_6.R1.0.1.x86_64.rpm \
		${DIST_SCL_6_X86_64}/celt051-0.5.1.3-0.SCLC6_6.R1.0.1.x86_64.rpm \
		${DIST_SCL_6_X86_64}/fontconfig-2.8.0-5.SCLC6_6.R1.0.1.x86_64.rpm \
		${DIST_SCL_6_X86_64}/qemu-kvm-0.12.1.2-2.448.SCLC6_6.R1.0.1.x86_64.rpm \
		${DIST_SCL_6_X86_64}/qemu-img-0.12.1.2-2.448.SCLC6_6.R1.0.1.x86_64.rpm \
		${DIST_SCL_6_X86_64}/libX11-1.6.0-2.2.SCLC6_6.R1.0.1.x86_64.rpm \
		${DIST_SCL_6_X86_64}/libXau-1.0.6-4.SCLC6_6.R1.0.1.x86_64.rpm \
		${DIST_SCL_6_X86_64}/libXdmcp-1.1.1-3.SCLC6_6.R1.0.1.x86_64.rpm \
		${DIST_SCL_6_X86_64}/libXext-1.3.2-2.1.SCLC6_6.R1.0.1.x86_64.rpm \
		${DIST_SCL_6_X86_64}/libXrandr-1.4.1-2.1.SCLC6_6.R1.0.1.x86_64.rpm \
		${DIST_SCL_6_X86_64}/libXrender-0.9.8-2.1.SCLC6_6.R1.0.1.x86_64.rpm \
		${DIST_SCL_6_X86_64}/libXxf86vm-1.1.3-2.1.SCLC6_6.R1.0.1.x86_64.rpm \
		${DIST_SCL_6_X86_64}/libdrm-2.4.52-4.SCLC6_6.R1.0.1.x86_64.rpm \
		${DIST_SCL_6_X86_64}/log4cpp-1.0-13.SCLC6_6.1.R1.0.1.x86_64.rpm \
		${DIST_SCL_6_X86_64}/mesa-libGL-10.1.2-2.SCLC6_6.R1.0.1.x86_64.rpm \
		${DIST_SCL_6_X86_64}/mesa-libGLU-10.1.2-2.SCLC6_6.R1.0.1.x86_64.rpm \
		${DIST_SCL_6_X86_64}/nspr-4.10.6-1.SCLC6_6.R1.0.1.x86_64.rpm \


##		${DIST_UNKNOWN}/qcairo-0.UNKNOWN.x86_64.rpm \
##		${DIST_UNKNOWN}/qffmpeg-libs-0.UNKNOWN.x86_64.rpm \
##		${DIST_UNKNOWN}/qpixman-0.UNKNOWN.x86_64.rpm \
##		${DIST_UNKNOWN}/qspice-libs-0.UNKNOWN.x86_64.rpm \


endif # PROD_FEATURE_VIRT_KVM

ifneq (${PROD_FEATURE_XML_GW}${PROD_FEATURE_VIRT},00)
	PACKAGES_BINARY_RPMS+= \
		${DIST_SCL_6_I386}/libxml2-2.7.6-17.SCLC6_6.1.R1.0.1.i686.rpm \
		${DIST_SCL_6_X86_64}/libxml2-2.7.6-17.SCLC6_6.1.R1.0.1.x86_64.rpm \

endif # (PROD_FEATURE_XML_GW || PROD_FEATURE_VIRT)

ifneq (${PROD_FEATURE_JUNIPER_MEDIAFLOW},0)
# Note: Put bind-utils-9.8.2-0.17.rc1.el6_4.6.x86_64 in the regular section above,
# because it it easier to have it in the base samara system.
# Note: libjpeg-devel-6b-46.el6.x86_64.rpm provides only the symlink
#   ./usr/lib64/libjpeg.so -> libjpeg.so.62.0.0
# Not sure if this is really needed, not including it.
#
	PACKAGES_BINARY_RPMS+= \
	${DIST_SCL_6_X86_64}/OpenIPMI-2.0.16-14.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/OpenIPMI-libs-2.0.16-14.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_I386}/compat-libstdc++-296-2.96-144.SCLC6_6.R1.0.1.i686.rpm \
	${DIST_SCL_6_X86_64}/elfutils-libs-0.158-3.2.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_I386}/kbd-misc-1.15-11.SCLC6_6.R1.0.1.noarch.rpm \
	${DIST_SCL_6_X86_64}/oprofile-0.9.9-6.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/python-2.6.6-52.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/rpcbind-0.2.0-11.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/sg3_utils-1.28-6.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/sg3_utils-libs-1.28-6.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/xfsdump-3.0.4-4.SCLC6_6.1.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/xz-libs-4.999.9-0.5.beta.20091007git.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/bind-9.8.2-0.30.rc1.SCLC6_6.2.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/bind-chroot-9.8.2-0.30.rc1.SCLC6_6.2.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/bind-libs-9.8.2-0.30.rc1.SCLC6_6.2.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/bind-sdb-9.8.2-0.30.rc1.SCLC6_6.2.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/busybox-1.15.1-21.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/ipmitool-1.8.11-21.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/irqbalance-1.0.4-10.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/kexec-tools-2.0.0-280.SCLC6_6.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/kpartx-0.4.9-80.SCLC6_6.3.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/mdadm-3.3-6.SCLC6_6.1.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/perl-5.10.1-136.SCLC6_6.1.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/perl-libs-5.10.1-136.SCLC6_6.1.R1.0.1.x86_64.rpm \
	${DIST_SCL_6_X86_64}/xfsprogs-3.1.1-16.SCLC6_6.R1.0.1.x86_64.rpm \


endif

FULL_BUILD_DIR=${PROD_BUILD_OUTPUT_DIR}/image/packages
FULL_INSTALL_DIR=${PROD_INSTALL_OUTPUT_DIR}/image
CONFIG_FILES=${PROD_BUILD_OUTPUT_DIR}/release/build_version.pl pkg_config.pl


.DEFAULT_GOAL := all

${PROD_BUILD_OUTPUT_DIR}/release/build_version.pl:
	(cd ${PROD_SRC_ROOT}/release && $(MAKE) version)

-include ${PROD_CUSTOMER_ROOT}/${PROD_PRODUCT_LC}/src/base_os/${PROD_TARGET_PLATFORM_LC}/${PROD_TARGET_PLATFORM_VERSION_LC}/arch_${PROD_TARGET_ARCH_LC}/image_packages/Makefile.inc

include ${PROD_TREE_ROOT}/src/mk/packages.mk
