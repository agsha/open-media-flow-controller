include ${PROD_TREE_ROOT}/src/mk/common.mk


INSTALL_DIRECTIVES= ${SYS_INSTALL_DIR_lib} ${SYS_INSTALL_FILE_libtcl} ${SYMLINK_LIB_tcl}

CLEANFILES= \
	${FULL_INSTALL_DIR}/opt/nkn/lib/libtcl8.4.so \


SYS_INSTALL_DIR_lib="dir 0755 root root /opt/nkn/lib"
SYS_INSTALL_FILE_libtcl="file  libtcl8.4.so  0755 root root /opt/nkn/lib/libtcl8.4.so"

include ${PROD_TREE_ROOT}/src/mk/files_adv.mk

all: install

