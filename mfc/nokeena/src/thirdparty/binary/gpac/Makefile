#  Makefile

include ${PROD_TREE_ROOT}/src/mk/common.mk

INSTALL_DIRECTIVES= \
  ${SYS_INSTALL_DIR_opt_nkn} \
  ${SYS_INSTALL_DIR_opt_nkn_bin} \
  ${SYS_INSTALL_DIR_usr_lib64} \
  ${SYS_INSTALL_FILE_mp4box} \
  ${SYS_INSTALL_FILE_libgpac_so} \


CLEANFILES= \
    ${FULL_INSTALL_DIR}/opt/nkn/bin/MP4Box \
    ${FULL_INSTALL_DIR}/usr/lib64/libgpac.so \


# Directory creation specs:
SYS_INSTALL_DIR_opt_nkn="dir 0755 root root /opt/nkn"
SYS_INSTALL_DIR_opt_nkn_bin="dir 0755 root root /opt/nkn/bin"
SYS_INSTALL_DIR_usr_lib64="dir 0755 root root /usr/lib64"

# File installation specs:

SYS_INSTALL_FILE_mp4box="file  6/MP4Box  0755 root root /opt/nkn/bin/MP4Box"
SYS_INSTALL_FILE_libgpac_so="file  6/libgpac.so  0755 root root /usr/lib64/libgpac.so"


include ${PROD_TREE_ROOT}/src/mk/files_adv.mk

all: install
