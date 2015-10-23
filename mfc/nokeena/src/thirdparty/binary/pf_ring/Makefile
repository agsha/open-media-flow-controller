#  Makefile

include ${PROD_TREE_ROOT}/src/mk/common.mk

INSTALL_DIRECTIVES= \
  ${SYS_INSTALL_DIR_lib64} \
  ${SYS_INSTALL_FILE_libpfring} \


CLEANFILES= \
    ${FULL_INSTALL_DIR}/lib64/libpfring.so \


# Directory creation specs:
SYS_INSTALL_DIR_lib64="dir 0755 root root /lib64"

# File installation specs:

SYS_INSTALL_FILE_libpfring="file  libpfring.so  0755 root root /lib64/libpfring.so"


include ${PROD_TREE_ROOT}/src/mk/files_adv.mk

all: install
