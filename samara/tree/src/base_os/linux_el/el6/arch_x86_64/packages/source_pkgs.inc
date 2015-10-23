#  Makefile

include ${PROD_TREE_ROOT}/src/mk/common.mk

FREEDIST_BASE_UNMODIFIED_SRPMS_PATH?=${PROD_DIST_ROOT}
DIST_SCL_6=/volume/ssd-linux-storage01/repo/ssdlinux/scl6/release/6.6.R1.0
DIST_SCL_6_SRPMS=${DIST_SCL_6}/srpm
DIST_SCL_6_UPDATE1=${DIST_SCL_6}/SRs/6.6.R1.1
DIST_SCL_6_UPDATE1_SRPMS=${DIST_SCL_6_UPDATE1}/srpm
DIST_SCL_6_UPDATE2=${DIST_SCL_6}/SRs/6.6.R1.2
DIST_SCL_6_UPDATE2_SRPMS=${DIST_SCL_6_UPDATE2}/srpm
DIST_SCL_6_UPDATE3=${DIST_SCL_6}/SRs/6.6.R1.3
DIST_SCL_6_UPDATE3_SRPMS=${DIST_SCL_6_UPDATE3}/srpm

### DIST_MODIFIED_SCL_6_SRPMS=${FREEDIST_BASE_UNMODIFIED_SRPMS_PATH}/modified/SCL6/SRPMS
DIST_MODIFIED_SCL_6_SRPMS=${PROD_CUSTOMER_ROOT}/dist/modified/SCL6/SRPMS

### FREEDIST_BASE_UNMODIFIED_SRPMS_LINUX_EL_EL6+= \
### 	${FREEDIST_BASE_UNMODIFIED_SRPMS_PATH}/other/el6/gdb-7.2-56.el6.tms.1.src.rpm \
### 	${FREEDIST_BASE_UNMODIFIED_SRPMS_PATH}/other/el6/ipsec-tools-0.6.5-14.el6.5.src.rpm \
### 	${FREEDIST_BASE_UNMODIFIED_SRPMS_PATH}/other/el6/openswan-2.6.38dr2-9.el6.src.rpm \
### 	${FREEDIST_BASE_UNMODIFIED_SRPMS_PATH}/other/el6/iptables-1.4.12.2.tar.bz2 \
### 
FREEDIST_BASE_UNMODIFIED_SRPMS_LINUX_EL_EL6+=

#	${DIST_UNKNOWN}/vsftpd-0.UNKNOWN.src.rpm \

## Removed from FREEDIST_BASE_UNMODIFIED_SRPMS_LINUX_EL_EL6
##   ${DIST_EL_EL6_SRPMS}/gdb-7.2-60.el6.src.rpm \

FREEDIST_BASE_UNMODIFIED_SRPMS_LINUX_EL_EL6= \
	${DIST_SCL_6_SRPMS}/OpenIPMI-2.0.16-14.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/acl-2.2.49-6.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/alsa-lib-1.0.22-3.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/attr-2.4.44-7.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/audiofile-0.2.6-11.1.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/audit-2.3.7-5.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/augeas-1.0.0-7.SCLC6_6.1.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/avahi-0.6.25-15.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/bash-4.1.2-29.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/bc-1.06.95-1.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/bind-9.8.2-0.30.rc1.SCLC6_6.2.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/binutils-2.20.51.0.2-5.42.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/bridge-utils-1.2-10.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/busybox-1.15.1-21.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/bzip2-1.0.5-7.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/ca-certificates-2014.1.98-65.1.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/cdrkit-1.1.9-12.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/celt051-0.5.1.3-0.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/chkconfig-1.3.49.3-2.SCLC6_6.1.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/compat-gcc-32-3.2.3-69.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/coreutils-8.4-37.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/cpio-2.10-12.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/curl-7.19.7-40.SCLC6_6.4.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/cyrus-sasl-2.1.23-15.SCLC6_6.2.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/dbus-1.2.24-7.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/dbus-glib-0.86-6.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/device-mapper-multipath-0.4.9-80.SCLC6_6.3.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/dialog-1.1-9.20080819.1.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/diffutils-2.8.1-28.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/dmidecode-2.12-5.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/dnsmasq-2.48-14.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/dosfstools-3.0.9-4.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/e2fsprogs-1.41.12-21.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/ed-1.1-3.3.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/elfutils-0.158-3.2.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/ethtool-3.5-5.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/expat-2.0.1-11.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/file-5.04-21.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/findutils-4.4.2-6.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/fipscheck-1.2.0-7.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/flac-1.2.1-6.1.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/fontconfig-2.8.0-5.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/freetype-2.3.11-15.SCLC6_6.1.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/ftp-0.17-54.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/fuse-2.8.3-4.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/gawk-3.1.7-10.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/gcc-4.4.7-11.SCLC6_6.R1.0.1.src.rpm \
	${DIST_MODIFIED_SCL_6_SRPMS}/glib2-2.28.8-4.SCLC6_6.R1.0.1_mfc.1.src.rpm \
	${DIST_SCL_6_SRPMS}/glibc-2.12-1.149.SCLC6_6.5.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/gmp-4.3.1-7.SCLC6_6.2.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/gnupg2-2.0.14-8.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/gnutls-2.8.5-14.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/gpxe-0.9.7-6.12.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/grep-2.6.3-6.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/grub-0.97-93.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/gzip-1.3.12-22.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/hal-0.5.14-14.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/hdparm-9.43-4.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/hwdata-0.233-11.1.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/i2c-tools-3.1.0-3.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/initscripts-9.03.46-1.SCLC6_6.1.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/ipmitool-1.8.11-21.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/iproute-2.6.32-33.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/iputils-20071127-17.SCLC6_6.2.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/irqbalance-1.0.4-10.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/iscsi-initiator-utils-6.2.0.873-13.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/java-1.6.0-openjdk-1.6.0.34-1.13.6.1.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/kbd-1.15-11.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/kexec-tools-2.0.0-280.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/keyutils-1.4-5.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/krb5-1.10.3-33.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/less-436-13.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/libaio-0.3.107-10.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/libart_lgpl-2.3.20-5.1.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/libasyncns-0.8-1.1.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/libcap-2.16-5.5.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/libcap-ng-0.6.4-3.SCLC6_6.1.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/libdrm-2.4.52-4.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/libedit-2.11-4.20080712cvs.1.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/libgcrypt-1.4.5-11.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/libgpg-error-1.7-4.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/libgssglue-0.1-11.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/libICE-1.0.6-1.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/libidn-1.18-2.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/libjpeg-turbo-1.2.1-3.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/libnih-1.0.1-7.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/libnl-1.1.4-2.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/libogg-1.1.4-2.1.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/libpcap-1.4.0-1.20130826git2dbcaa1.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/libpciaccess-0.13.3-0.1.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/libpng-1.2.49-1.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/libselinux-2.0.94-5.8.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/libsepol-2.0.41-4.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/libSM-1.2.1-2.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/libsndfile-1.0.20-5.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/libssh2-1.4.2-1.SCLC6_6.1.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/libtasn1-2.3-6.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/libtirpc-0.2.1-10.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/libusb-0.1.12-23.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/libuser-0.56.13-5.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/libutempter-1.1.5-4.1.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/libvirt-0.10.2-46.SCLC6_6.3.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/libvorbis-1.2.3-4.SCLC6_6.1.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/libX11-1.6.0-2.2.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/libXau-1.0.6-4.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/libxcb-1.9.1-2.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/libXdmcp-1.1.1-3.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/libXext-1.3.2-2.1.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/libXi-1.7.2-2.2.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/libxml2-2.7.6-17.SCLC6_6.1.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/libXmu-1.1.1-2.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/libXrandr-1.4.1-2.1.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/libXrender-0.9.8-2.1.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/libxslt-1.1.26-2.SCLC6_6.1.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/libXtst-1.2.2-2.1.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/libXxf86vm-1.1.3-2.1.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/lm_sensors-3.1.1-17.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/log4cpp-1.0-13.SCLC6_6.1.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/logrotate-3.7.8-17.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/lrzsz-0.12.20-27.1.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/lsof-4.82-4.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/ltrace-0.5-28.45svn.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/lvm2-2.02.111-2.SCLC6_6.1.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/mdadm-3.3-6.SCLC6_6.1.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/mesa-10.1.2-2.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/mingetty-1.08-5.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/module-init-tools-3.9-24.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/nc-1.84-22.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/ncurses-5.7-3.20090208.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/netcf-0.2.4-1.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/net-tools-1.60-110.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/nfs-utils-1.2.3-54.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/nspr-4.10.6-1.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/nss-3.16.2.3-3.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/nss-softokn-3.14.3-22.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/nss-util-3.16.2.3-2.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/ntp-4.2.6p5-3.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/numactl-2.0.9-2.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/openldap-2.4.39-8.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/openssh-5.3p1-104.SCLC6_6.1.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/openssl-1.0.1e-30.SCLC6_6.7.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/oprofile-0.9.9-6.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/pam-1.1.1-20.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/passwd-0.77-4.SCLC6_6.2.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/patch-2.6-6.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/pax-3.4-10.1.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/pciutils-3.1.10-4.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/pcre-7.8-6.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/perl-5.10.1-136.SCLC6_6.1.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/pixman-0.32.4-4.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/popt-1.13-7.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/procps-3.2.8-30.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/psmisc-22.6-19.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/pth-2.0.7-9.3.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/pulseaudio-0.9.21-17.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/python-2.6.6-52.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/python-crypto-2.0.1-22.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/python-paramiko-1.7.5-2.1.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/qemu-kvm-0.12.1.2-2.448.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/rcs-5.7-37.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/readline-6.0-4.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/rpcbind-0.2.0-11.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/rsync-3.0.6-12.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/SDL-1.2.14-3.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/seabios-0.6.1.2-28.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/sed-4.2.1-10.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/setserial-2.17-25.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/setup-2.8.14-20.SCLC6_6.1.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/sg3_utils-1.28-6.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/sgabios-0-0.3.20110621svn.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/sharutils-4.7-6.1.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/smartmontools-5.43-1.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/spice-server-0.12.4-11.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/sqlite-3.6.20-1.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/strace-4.5.19-1.19.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/sysfsutils-2.1.0-7.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/syslinux-4.04-3.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/sysstat-9.0.4-27.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/sysvinit-2.87-5.dsf.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/tar-1.23-11.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/tcl-8.5.7-6.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/tcpdump-4.0.0-3.20090921gitdf3cb4.2.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/tcp_wrappers-7.6-57.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/tcsh-6.17-25.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/telnet-0.17-48.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/tftp-0.49-7.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/tigervnc-1.1.0-16.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/time-1.7-37.1.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/traceroute-2.0.14-2.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/tzdata-2015b-1.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/udev-147-2.57.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/unzip-6.0-2.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/upstart-0.6.5-13.SCLC6_6.3.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/usbredir-0.5.1-1.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/util-linux-ng-2.17.2-12.18.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/vconfig-1.9-8.1.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/vgabios-0.6b-3.7.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/vim-7.2.411-1.8.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/wget-1.12-5.SCLC6_6.1.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/which-2.19-6.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/xfsdump-3.0.4-4.SCLC6_6.1.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/xfsprogs-3.1.1-16.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/xinetd-2.3.14-39.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/xorg-x11-xauth-1.0.2-7.1.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/xz-4.999.9-0.5.beta.20091007git.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/yajl-1.0.7-3.SCLC6_6.R1.0.1.src.rpm \
	${DIST_SCL_6_SRPMS}/zlib-1.2.3-29.SCLC6_6.R1.0.1.src.rpm \
