#  Makefile that copies binary files from the build machine to the MFC package.
#
# These are built from source with no rpms, and must be installed on
# each build machine before the MFC product can be built.
#  libcares, libcurl, libcprops, libGeoIP, libpcap
#
include ${PROD_TREE_ROOT}/src/mk/common.mk


INSTALL_DIRECTIVES= \
  ${SYS_INSTALL_DIR_lib64} \
  ${SYS_INSTALL_DIR_usr_lib64} \
  ${SYS_INSTALL_DIR_usr_local} \
  ${SYS_INSTALL_DIR_usr_local_lib} \
  ${SYS_INSTALL_DIR_opt_nkn} \
  ${SYS_INSTALL_DIR_opt_nkn_bin} \
  ${SYS_INSTALL_DIR_usr} \
  ${SYS_INSTALL_DIR_usr_bin} \
  ${SYS_INSTALL_DIR_usr_lib} \
  ${SYS_INSTALL_FILE_libcares_so_200} \
  ${SYS_CREATE_SYMLINK_libcares_so_2} \
  ${SYS_CREATE_SYMLINK_libcares_so} \
  ${SYS_INSTALL_FILE_libcurl_so_420} \
  ${SYS_CREATE_SYMLINK_libcurl_so_4} \
  ${SYS_CREATE_SYMLINK_libcurl_so} \
  ${SYS_INSTALL_FILE_libcprops_so_1020} \
  ${SYS_CREATE_SYMLINK_libcprops_so_10} \
  ${SYS_CREATE_SYMLINK_libcprops_so} \
  ${SYS_INSTALL_FILE_libGeoIP_so_148} \
  ${SYS_CREATE_SYMLINK_libGeoIP_so_1} \
  ${SYS_CREATE_SYMLINK_libGeoIP_so} \
  ${SYS_INSTALL_FILE_libpcap_so_120} \
  ${SYS_CREATE_SYMLINK_libpcap_so_1} \
  ${SYS_CREATE_SYMLINK_libpcap_so} \

ifeq "${BUILD_USE_SAMARA_SSL}" ""
INSTALL_DIRECTIVES+= \
  ${SYS_INSTALL_DIR_usr_local_openssl} \
  ${SYS_INSTALL_DIR_usr_local_openssl_bin} \
  ${SYS_INSTALL_DIR_usr_local_openssl_openssl} \
  ${SYS_INSTALL_FILE_usr_local_openssl_bin_openssl} \
  ${SYS_INSTALL_FILE_usr_local_openssl_bin_c_rehash} \
  ${SYS_INSTALL_FILE_usr_local_openssl_openssl_openssl_cnf} \
  ${SYS_CREATE_SYMLINK_usr_local_openssl_openssl_certs} \
  ${SYS_CREATE_SYMLINK_usr_local_openssl_openssl_private}
endif


CLEANFILES= \
  ${FULL_INSTALL_DIR}/usr/local/lib/libcares.so.2.0.0 \
  ${FULL_INSTALL_DIR}/usr/local/lib/libcares.so.2 \
  ${FULL_INSTALL_DIR}/usr/local/lib/libcares.so \
  ${FULL_INSTALL_DIR}/usr/local/lib/libcurl.so.4.2.0 \
  ${FULL_INSTALL_DIR}/usr/local/lib/libcurl.so.4 \
  ${FULL_INSTALL_DIR}/usr/local/lib/libcurl.so \
  ${FULL_INSTALL_DIR}/usr/local/lib/libcprops.so.10.2.0 \
  ${FULL_INSTALL_DIR}/usr/local/lib/libcprops.so.10 \
  ${FULL_INSTALL_DIR}/usr/local/lib/libcprops.so \
  ${FULL_INSTALL_DIR}/usr/local/lib/libGeoIP.so.1.4.8 \
  ${FULL_INSTALL_DIR}/usr/local/lib/libGeoIP.so.1 \
  ${FULL_INSTALL_DIR}/usr/local/lib/libGeoIP.so \
  ${FULL_INSTALL_DIR}/usr/local/lib/libpcap.so.10.2.0 \
  ${FULL_INSTALL_DIR}/usr/local/lib/libpcap.so.10 \
  ${FULL_INSTALL_DIR}/usr/local/lib/libpcap.so \
  ${FULL_INSTALL_DIR}/usr/local/openssl/bin/openssl \
  ${FULL_INSTALL_DIR}/usr/local/openssl/openssl/openssl.cnf \
  ${FULL_INSTALL_DIR}/usr/local/openssl/openssl/certs \
  ${FULL_INSTALL_DIR}/usr/local/openssl/openssl/private \

# Directory creation specs:
SYS_INSTALL_DIR_lib64="dir         0755 root root /lib64"
SYS_INSTALL_DIR_usr_lib64="dir     0755 root root /usr/lib64"
SYS_INSTALL_DIR_usr_local="dir     0755 root root /usr/local"
SYS_INSTALL_DIR_usr_local_lib="dir 0755 root root /usr/local/lib"
SYS_INSTALL_DIR_usr_local_openssl="dir 0755 root root /usr/local/openssl"
SYS_INSTALL_DIR_usr_local_openssl_bin="dir 0755 root root /usr/local/openssl/bin"
SYS_INSTALL_DIR_usr_local_openssl_openssl="dir 0755 root root /usr/local/openssl/openssl"
SYS_INSTALL_DIR_opt_nkn="dir       0755 root root /opt/nkn"
SYS_INSTALL_DIR_opt_nkn_bin="dir   0755 root root /opt/nkn/bin"
SYS_INSTALL_DIR_usr="dir           0755 root root /usr"
SYS_INSTALL_DIR_usr_bin="dir       0755 root root /usr/bin"
SYS_INSTALL_DIR_usr_lib="dir       0755 root root /usr/lib"


# File installation specs:

# SPECIAL: Install from the build machine

SYS_INSTALL_FILE_libcares_so_200="file /usr/local/lib/libcares.so.2.0.0  0755 root root /usr/local/lib/libcares.so.2.0.0"
SYS_CREATE_SYMLINK_libcares_so_2="symlink /usr/local/lib /usr/local/lib/libcares.so.2.0.0 libcares.so.2"
SYS_CREATE_SYMLINK_libcares_so="symlink   /usr/local/lib /usr/local/lib/libcares.so.2.0.0 libcares.so"

SYS_INSTALL_FILE_libcurl_so_420="file /usr/local/lib/libcurl.so.4.2.0    0755 root root /usr/local/lib/libcurl.so.4.2.0"
SYS_CREATE_SYMLINK_libcurl_so_4="symlink  /usr/local/lib /usr/local/lib/libcurl.so.4.2.0 libcurl.so.4"
SYS_CREATE_SYMLINK_libcurl_so="symlink    /usr/local/lib /usr/local/lib/libcurl.so.4.2.0 libcurl.so"

SYS_INSTALL_FILE_libcprops_so_1020="file /usr/local/lib/libcprops.so.10.2.0    0755 root root /usr/local/lib/libcprops.so.10.2.0"
SYS_CREATE_SYMLINK_libcprops_so_10="symlink  /usr/local/lib /usr/local/lib/libcprops.so.10.2.0 libcprops.so.10"
SYS_CREATE_SYMLINK_libcprops_so="symlink     /usr/local/lib /usr/local/lib/libcprops.so.10.2.0 libcprops.so"

SYS_INSTALL_FILE_libGeoIP_so_148="file /usr/local/lib/libGeoIP.so.1.4.8  0755 root root /usr/local/lib/libGeoIP.so.1.4.8"
SYS_CREATE_SYMLINK_libGeoIP_so_1="symlink /usr/local/lib /usr/local/lib/libGeoIP.so.1.4.8 libGeoIP.so.1"
SYS_CREATE_SYMLINK_libGeoIP_so="symlink   /usr/local/lib /usr/local/lib/libGeoIP.so.1.4.8 libGeoIP.so"

SYS_INSTALL_FILE_libpcap_so_120="file /usr/local/lib/libpcap.so.1.2.0    0755 root root /usr/local/lib/libpcap.so.1.2.0"
SYS_CREATE_SYMLINK_libpcap_so_1="symlink  /usr/local/lib /usr/local/lib/libpcap.so.1.2.0 libpcap.so.1"
SYS_CREATE_SYMLINK_libpcap_so="symlink    /usr/local/lib /usr/local/lib/libpcap.so.1.2.0 libpcap.so"

SYS_INSTALL_FILE_usr_local_openssl_bin_openssl="file          /usr/local/openssl/bin/openssl         0755 root root /usr/local/openssl/bin/openssl"
SYS_INSTALL_FILE_usr_local_openssl_bin_c_rehash="file          /usr/local/openssl/bin/c_rehash         0755 root root /usr/local/openssl/bin/c_rehash"
SYS_INSTALL_FILE_usr_local_openssl_openssl_openssl_cnf="file  /usr/local/openssl/openssl/openssl.cnf 0755 root root /usr/local/openssl/openssl/openssl.cnf"
SYS_CREATE_SYMLINK_usr_local_openssl_openssl_certs="symlink   /usr/local/openssl/openssl /etc/pki/tls/certs   certs"
SYS_CREATE_SYMLINK_usr_local_openssl_openssl_private="symlink /usr/local/openssl/openssl /etc/pki/tls/private private"

include ${PROD_TREE_ROOT}/src/mk/files_adv.mk

all: install
