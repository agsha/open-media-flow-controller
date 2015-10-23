#  Makefile

include ${PROD_TREE_ROOT}/src/mk/common.mk

INSTALL_DIRECTIVES= \
  ${SYS_INSTALL_DIR_u} \
  ${SYS_INSTALL_DIR_ul} \
  ${SYS_INSTALL_DIR_ul_bin} \
  ${SYS_INSTALL_DIR_ul_sbin} \
  ${SYS_INSTALL_DIR_ul_lib} \
  ${SYS_INSTALL_DIR_ul_etc} \
  ${SYS_INSTALL_FILE_bgpd_conf_sh} \
  ${SYS_INSTALL_FILE_bgpd_conf_lock} \
  ${SYS_INSTALL_FILE_vtysh} \
  ${SYS_INSTALL_FILE_bgpd} \
  ${SYS_INSTALL_FILE_watchquagga} \
  ${SYS_INSTALL_FILE_zebra} \
  ${SYS_INSTALL_FILE_bgpd_conf} \
  ${SYS_INSTALL_FILE_vtysh_conf} \
  ${SYS_INSTALL_FILE_zebra_conf} \
  ${SYS_INSTALL_FILE_libzebra000} \
  ${SYS_CREATE_SYMLINK_libzebra_so} \
  ${SYS_CREATE_SYMLINK_libzebra_so_0} \


CLEANFILES= \
    ${FULL_INSTALL_DIR}/usr/local/bin/bgpd-conf.sh \
    ${FULL_INSTALL_DIR}/usr/local/bin/bgpd-conf.lock \
    ${FULL_INSTALL_DIR}/usr/local/bin/vtysh \
    ${FULL_INSTALL_DIR}/usr/local/sbin/bgpd \
    ${FULL_INSTALL_DIR}/usr/local/sbin/watchquagga \
    ${FULL_INSTALL_DIR}/usr/local/sbin/zebra \
    ${FULL_INSTALL_DIR}/usr/local/etc/bgpd.conf \
    ${FULL_INSTALL_DIR}/usr/local/etc/vtysh.conf \
    ${FULL_INSTALL_DIR}/usr/local/etc/zebra.conf \
    ${FULL_INSTALL_DIR}/usr/local/lib/libzebra.so.0.0.0 \
    ${FULL_INSTALL_DIR}/usr/local/lib/libzebra.so.0 \
    ${FULL_INSTALL_DIR}/usr/local/lib/libzebra.so \


# Directory creation specs:
SYS_INSTALL_DIR_u="dir 0755 root root /usr"
SYS_INSTALL_DIR_ul="dir 0755 root root /usr/local"
SYS_INSTALL_DIR_ul_bin="dir 0755 root root /usr/local/bin"
SYS_INSTALL_DIR_ul_sbin="dir 0755 root root /usr/local/sbin"
SYS_INSTALL_DIR_ul_lib="dir 0755 root root /usr/local/lib"
SYS_INSTALL_DIR_ul_etc="dir 0755 root root /usr/local/etc"

# File installation specs:

SYS_INSTALL_FILE_bgpd_conf_sh="file   bgpd-conf.sh   0755 root root /usr/local/bin/bgpd-conf.sh"
SYS_INSTALL_FILE_bgpd_conf_lock="file bgpd-conf.lock 0755 root root /usr/local/bin/bgpd-conf.lock"
SYS_INSTALL_FILE_vtysh="file       6/vtysh             0755 root root /usr/local/bin/vtysh"
SYS_INSTALL_FILE_bgpd="file        6/bgpd              0755 root root /usr/local/sbin/bgpd"
SYS_INSTALL_FILE_watchquagga="file 6/watchquagga       0755 root root /usr/local/sbin/watchquagga"
SYS_INSTALL_FILE_zebra="file       6/zebra             0755 root root /usr/local/sbin/zebra"
SYS_INSTALL_FILE_bgpd_conf="file   6/bgpd.conf         0755 root root /usr/local/etc/bgpd.conf"
SYS_INSTALL_FILE_vtysh_conf="file  6/vtysh.conf        0755 root root /usr/local/etc/vtysh.conf"
SYS_INSTALL_FILE_zebra_conf="file  6/zebra.conf        0755 root root /usr/local/etc/zebra.conf"
SYS_INSTALL_FILE_libzebra000="file 6/libzebra.so.0.0.0 0755 root root /usr/local/lib/libzebra.so.0.0.0"

# These symlinks need to be made as well.
#   /usr/local/lib/libzebra.so -> libzebra.so.0.0.0
#   /usr/local/lib/libzebra.so.0 -> libzebra.so.0.0.0
SYS_CREATE_SYMLINK_libzebra_so="symlink   /usr/local/lib /usr/local/lib/libzebra.so.0.0.0 libzebra.so"
SYS_CREATE_SYMLINK_libzebra_so_0="symlink /usr/local/lib /usr/local/lib/libzebra.so.0.0.0 libzebra.so.0"

include ${PROD_TREE_ROOT}/src/mk/files_adv.mk

all: install
