# Makefile to put thirdparty binary files into the MFC package.  These binary files
# are NOT built from source each time MFC is built.
#
# Note: If thirdparty files are added or removed from the MFC package, and
#   if the files are GPL or have some other license that needs to have the
#   source made available, update nokeena/src/release/freedist/Makefile.inc

# License types:
#   virtualenv       MIT style

include ${PROD_TREE_ROOT}/src/mk/common.mk

INSTALL_DIRECTIVES= \
  ${SYS_INSTALL_DIR_usr} \
  ${SYS_INSTALL_DIR_opt} \
  ${SYS_INSTALL_TAR_GZ_python27} \
  ${SYS_INSTALL_TAR_GZ_dpianalyzer} \

CLEANFILES= \
  ${FULL_INSTALL_DIR}/usr/bin/python2.7 \
  ${FULL_INSTALL_DIR}/usr/bin/easy_install \
  ${FULL_INSTALL_DIR}/usr/bin/easy_install-2.7 \
  ${FULL_INSTALL_DIR}/usr/bin/pip \
  ${FULL_INSTALL_DIR}/usr/bin/pip2 \
  ${FULL_INSTALL_DIR}/usr/bin/pip2.7 \
  ${FULL_INSTALL_DIR}/usr/bin/virtualenv \
  ${FULL_INSTALL_DIR}/usr/bin/virtualenv-2.7 \
  ${FULL_INSTALL_DIR}/usr/lib/libpython2.7.a \

CLEANDIRS= \
  ${FULL_INSTALL_DIR}/usr/lib/python2.7/ \
  ${FULL_INSTALL_DIR}/opt/dpi-analyzer/bin/ \
  ${FULL_INSTALL_DIR}/opt/dpi-analyzer/include/ \
  ${FULL_INSTALL_DIR}/opt/dpi-analyzer/lib/ \


# Directory creation specs:
SYS_INSTALL_DIR_usr="dir       0755 root root /usr"
SYS_INSTALL_DIR_opt="dir       0755 root root /opt"

# File installation specs:
SYS_INSTALL_TAR_GZ_python27="tar-gz python2.7.tgz /usr"
SYS_INSTALL_TAR_GZ_dpianalyzer="tar-gz dpi-analyzer.tgz /opt"

include ${PROD_TREE_ROOT}/src/mk/files_adv.mk

all: install

clean-all: clean
	${RM} -rf ${CLEANFILES}
