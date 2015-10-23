#  Makefile

# The makefile $PROD_TREE_ROOT/src/release/freedist/Makefile includes this Makefile.inc file.
# The Makefile includes these other include files depending on the arch setting:
#   $PROD_TREE_ROOT/src/base_os/linux_el/el5/arch_i386/packages/source_pkgs.inc (not used)
#   $PROD_TREE_ROOT/src/base_os/linux_el/el5/arch_x86_64/packages/source_pkgs.inc (used!)

# The Makefile concatinates several FREEDIST_CUSTOMER_UNMODIFIED_SRPMS_* settings
# together to handle the situation where the same source tree is used to build
# several OS and ARCH types.  We do not, so we only need to specify one: 
#   FREEDIST_CUSTOMER_UNMODIFIED_SRPMS_COMMON
#
# Similarly we only need to specify the following for specifying source trees:
#   FREEDIST_CUSTOMER_MODIFIED_SOURCES_COMMON

PROD_CUSTOMER_SRC_ROOT=${PROD_CUSTOMER_ROOT}/${PROD_PRODUCT_LC}/src
SRC_THIRDPARTY_PATCHES=${PROD_CUSTOMER_SRC_ROOT}/thirdparty/patches

THIRDPARTY_SRPMS=${PROD_CUSTOMER_ROOT}/dist/thirdparty-src
THIRDPARTY_SRC=${PROD_CUSTOMER_ROOT}/dist/thirdparty-src

# Source in the MFC source 'dist' tree
DIST_BUILDMACHINE_SRC=${PROD_CUSTOMER_ROOT}/dist/build-machine/build-machine-src
DIST_THIRDPARTY_SRC=${PROD_CUSTOMER_ROOT}/dist/thirdparty-src

FREEDIST_CUSTOMER_UNMODIFIED_SRPMS_COMMON=\
  ${THIRDPARTY_SRPMS}/bind/bind-9.3.6-4.P1.el5.src.rpm \
  ${THIRDPARTY_SRPMS}/compat-gcc/compat-gcc-296-2.96-138.src.rpm \
  ${THIRDPARTY_SRPMS}/nspr/nspr-4.7.0.99.2-1.el5.src.rpm \
  ${THIRDPARTY_SRPMS}/nss/nss-3.12.3.99.3-1.el5.centos.2.src.rpm \
  ${THIRDPARTY_SRPMS}/OpenIPMI/OpenIPMI-2.0.16-7.el5.src.rpm \
  ${THIRDPARTY_SRPMS}/oprofile/oprofile-0.9.3-18.el5.src.rpm \
  ${THIRDPARTY_SRPMS}/perl/perl-5.8.8-15.el5_2.1.src.rpm \
  ${THIRDPARTY_SRPMS}/portmap/portmap-4.0-65.2.2.1.src.rpm \
  ${THIRDPARTY_SRPMS}/python/python-2.4.3-21.el5.src.rpm \
  ${THIRDPARTY_SRPMS}/xfsdump/xfsdump-2.2.46-1.el5.centos.src.rpm \
  ${THIRDPARTY_SRPMS}/xfsprogs/xfsprogs-2.9.4-1.el5.centos.src.rpm \
  ${THIRDPARTY_SRPMS}/dbus/dbus-1.1.2-14.el5.src.rpm \
  ${THIRDPARTY_SRC}/cxgb3toe/cxgb3toe-1.3.1.10.tar.gz \
  ${THIRDPARTY_SRC}/ixgbe/ixgbe-3.4.24.tar.gz \
  ${THIRDPARTY_SRC}/quagga/quagga-0.99.18.tar.gz \
  ${THIRDPARTY_SRC}/smartmon/smartmontools-5.40.tar.gz \
  ${DIST_THIRDPARTY_SRC}/dmidecode/dmidecode-2.11.tar.gz \
  ${DIST_THIRDPARTY_SRC}/ffmpeg/ffmpeg-r26402.tgz \
  ${DIST_THIRDPARTY_SRC}/gpac/gpac-0.4.5.tgz \
  ${DIST_THIRDPARTY_SRC}/sdparm/sdparm-1.03.tgz \
  ${DIST_THIRDPARTY_SRC}/yamdi/yamdi-1.8.tar.gz \
  ${DIST_BUILDMACHINE_SRC}/GeoIP/GeoIP-1.4.8.tar.gz \
  ${DIST_BUILDMACHINE_SRC}/c-ares/c-ares-1.7.1.tar.gz \
  ${DIST_BUILDMACHINE_SRC}/cgic/cgic205.tar.gz \
  ${DIST_BUILDMACHINE_SRC}/clisp/clisp-2.49.tar.bz2 \
  ${DIST_BUILDMACHINE_SRC}/curl/curl-7.20.1.tar.gz \
  ${DIST_BUILDMACHINE_SRC}/libcprops/libcprops-0.1.8.zip \
  ${DIST_BUILDMACHINE_SRC}/libpcap/libpcap-1.2.0rc1.tar.gz \
  ${DIST_BUILDMACHINE_SRC}/libsigsegv/libsigsegv-2.8.tar.gz \
  ${DIST_BUILDMACHINE_SRC}/openssl/openssl-1.0.1p.tar.gz \
  ${DIST_BUILDMACHINE_SRC}/python-2.7.2/Python-2.7.2.tgz \
  ${DIST_BUILDMACHINE_SRC}/python-utilities/distribute-0.6.49.tar.gz \
  ${DIST_BUILDMACHINE_SRC}/python-utilities/virtualenv-1.9.1.tar.gz \
  ${DIST_BUILDMACHINE_SRC}/x264/x264-snapshot-20110126-2245.tar.bz2 \
  ${DIST_THIRDPARTY_SRC}/pf_ring/PF_RING-6.0.0.tar.gz \
  ${DIST_THIRDPARTY_SRC}/pf_ring/PF_RING.patch \
  ${DIST_THIRDPARTY_SRC}/wget/wget-1.13.4.tar.gz \
  ${DIST_THIRDPARTY_SRC}/wget/wget-1.13.4.nkn.patch \
  ${SRC_THIRDPARTY_PATCHES}/grub-install/grub-install.patch \


######################################################################################
# Notes:
######################################################################################
# nokeena/src/thirdparty/bin subdirs:
# This has source that is built each time.
#   ftpd: Free and BSD style license for some BSD files.
#         Attribution required for files that have NeetBSD copyrights.
#         OK to distribute source, but not required.
#   iotest31: Free-to-use license.  No attribution or source release requirement.
#         OK to distribute source, but not required.
#   mapurl: Currently Juniper only code.  No disclosures needed.
#          Perhaps in the future we might need to OpenSource it.
#   mapxml: Most code is Juniper only code.  One subdir has non-Juniper code:
#         mapxml/xmlparser:  BSD style license.  OK to distribute source, but not required.
#         Attribution required for files in the xmlparser subdir.
#   ngrep: BSD style, GPLv2, MIT.  Modified with at patch file.
#         Attribution required.  Source distribution required.

######################################################################################
# nokeena/src/thirdparty/binary subdirs:
# This has pre-built binaries.
# The source is in dist/thirdparty-src
# and in /data/archive/thirdparty/srpms
# and in /data/archive/thirdparty/src
# and in /data/archive/thirdparty/src-private (qosmos)
#   dmidecode
#   ffmpeg
#   gpac
#   hdparm
#   lsiutil
#   pf_ring
#   qosmos_dpi  (Juniper private source)
#   qosmos_dpi_stubs (public-ok stubs)
#   quagga
#   sas2ircu
#   sdparm
#   smartctl
#   ssh-43p2
#   tcl_84
#   tw_cli
#   virtualenv
#   wget
#   yamdi
# Notes:
# hdparm: BSD-Style Open Source License, no source distribution needed.
# sdparm: BSD-Style Open Source License, no source distribution needed.
# ffmpeg: LGPL
# smartctl: GPL.  An older version is a part of Centos, but we want to use
#                 the latest source, 5.40

######################################################################################
# nokeena/src/thirdparty/install-from-buildmachine:
#   GeoIP
#   c-ares
#   curl
#   libcprops
#   libpcap
#   openssl
# The following libs are built from source and installed on the build machine:
#   libGeoIP, libcares, libcurl, libcprops, libpcap
#
# c-ares: MIT license.  No need to distribute source
# curl:   MIT-like license.  No need to distribute source
# openssl: Apache-like license.  No need to distribute source.

# The makefile nokeena/src/thirdparty/binary/install-from-buildmachine/Makefile
# copies the following from the build machine into the product:
#
# From: /usr/local/lib/libcares.so.2.0.0       To: /usr/local/lib/libcares.so.2.0.0
# From: /usr/local/lib/libcurl.so.4.2.0        To: /usr/local/lib/libcurl.so.4.2.0
# From: /usr/local/lib/libcprops.so.10.2.0     To: /usr/local/lib/libcprops.so.10.2.0
# From: /usr/local/lib/libGeoIP.so.1.4.8       To: /usr/local/lib/libGeoIP.so.1.4.8
# From: /usr/local/lib/libpcap.so.1.2.0        To: /usr/local/lib/libpcap.so.1.2.0
# From: /usr/local/openssl/bin/openssl         To: /usr/local/openssl/bin/openssl
# From: /usr/local/openssl/bin/c_rehash        To: /usr/local/openssl/bin/c_rehash
# From: /usr/local/openssl/openssl/openssl.cnf To: /usr/local/openssl/openssl/openssl.cnf

#####################################################################################
# nokeena/src/thirdparty/lib subdirs:
#   chartdirector:  We have licensed this.  Must NOT distribute licensed version.
#   ketama:  BSD style license.  OK to distribute source, but not required.
#   stack:   BSD license (came from BSD source tree)

######################################################################################
