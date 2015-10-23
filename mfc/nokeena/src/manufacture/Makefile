# Makefile

# Put files into the manufacturing*.tgz file for use in
# the stripped down 64-bit manufacturing environment.

include ${PROD_TREE_ROOT}/src/mk/common.mk

# The PROD_LINUX_* settings are set in ${PROD_TREE_ROOT}/src/mk/common.mk.  E.g.
# export PROD_LINUX_ROOT?=/volume/ssd-linux-storage01/repo/ssdlinux/scl6/release/6.6.R1.0
# export PROD_LINUX_U_ROOT=${PROD_LINUX_ROOT}/SRs/6.6.R1.1
# export PROD_LINUX_X86_64_RPMS_DIR=${PROD_LINUX_ROOT}/x86_64/rpm
# export PROD_LINUX_I386_RPMS_DIR=${PROD_LINUX_ROOT}/i386/rpm
# export PROD_LINUX_SRPMS_DIR=${PROD_LINUX_ROOT}/srpm
# export PROD_LINUX_U_X86_64_RPMS_DIR=${PROD_LINUX_ROOT}/x86_64/rpm
# export PROD_LINUX_U_I386_RPMS_DIR=${PROD_LINUX_ROOT}/i386/rpm
# export PROD_LINUX_U_SRPMS_DIR=${PROD_LINUX_ROOT}/srpm

ETHTOOL_RPM=${PROD_LINUX_X86_64_RPMS_DIR}/ethtool-*.rpm
PCIUTILS_RPM=${PROD_LINUX_X86_64_RPMS_DIR}/pciutils-3.*.rpm
PCIUTILSLIB_RPM=${PROD_LINUX_X86_64_RPMS_DIR}/pciutils-libs-3.*.rpm
BASH_RPM=${PROD_LINUX_X86_64_RPMS_DIR}/bash-*.rpm
COREUTILS_RPM=${PROD_LINUX_X86_64_RPMS_DIR}/coreutils-8*.rpm
PATCH_RPM=${PROD_LINUX_X86_64_RPMS_DIR}/patch-*.rpm
LIBSTDCPP_RPM=${PROD_LINUX_X86_64_RPMS_DIR}/libstdc++-4*.rpm
GLIBC_RPM=${PROD_LINUX_U_X86_64_RPMS_DIR}/glibc-2.12-*.rpm
LIBGCC_RPM=${PROD_LINUX_X86_64_RPMS_DIR}/libgcc-*.rpm
LIBCAP=${PROD_LINUX_X86_64_RPMS_DIR}/libcap-2*.rpm
MDADM_RPM=${PROD_LINUX_X86_64_RPMS_DIR}/mdadm-*.rpm
COMPAT_LIBSTDCPP_RPM=${PROD_LINUX_I386_RPMS_DIR}/compat-libstdc++-296-*.i686.rpm

MY_RPM_DIR=${PROD_CUSTOMER_ROOT}/nokeena/src/base_os/linux_el/el6/arch_x86_64/packages
HPACUCLI_RPM=${MY_RPM_DIR}/hpacucli-9.10-22.0.x86_64.rpm
STORMAN_RPM=${MY_RPM_DIR}/StorMan-7.31-18856.x86_64.rpm

OUTPUT_DIR=${PROD_OUTPUT_DIR}/install/manufacture
RELEASE_DIR=${PROD_OUTPUT_DIR}/release/manufacture
TGZ_FILE=${RELEASE_DIR}/manufacture-${BUILD_CUSTOMER_SUFFIX}.tgz
FULL_INSTALL_DIR=${PROD_INSTALL_OUTPUT_DIR}/image/${PROD_INSTALL_PREFIX}/release


.PHONY: default
default:
	@echo Only the clean, install and release targets are useful

clean:
	rm -rf ${OUTPUT_DIR}
	rm -rf ${RELEASE_DIR}

found_newer.txt:
	sh find_newer.sh ${PROD_LINUX_U_ROOT}

.PHONY: install
install: \
  ${OUTPUT_DIR}/sbin/manufacture.sh \
  ${OUTPUT_DIR}/sbin/writeimage.sh \
  ${OUTPUT_DIR}/etc/customer_rootflop.sh \
  ${OUTPUT_DIR}/sbin/parse_manuf_opt.sh \
  ${OUTPUT_DIR}/sbin/vxa_functions.dot \
  ${OUTPUT_DIR}/sbin/pre-install.sh \
  ${OUTPUT_DIR}/sbin/post-install.sh \
  ${OUTPUT_DIR}/sbin/install-mfc \
  ${OUTPUT_DIR}/sbin/make_raid.sh \
  ${OUTPUT_DIR}/sbin/grub_setup_raid.sh \
  ${OUTPUT_DIR}/sbin/eth-setup \
  ${OUTPUT_DIR}/sbin/eth-setup-interactive \
  ${OUTPUT_DIR}/sbin/eth-setup-menu \
  ${OUTPUT_DIR}/sbin/vxa-eth-setup.sh \
  ${OUTPUT_DIR}/sbin/mvt-printout \
  ${OUTPUT_DIR}/sbin/eth-test \
  ${OUTPUT_DIR}/usr/sbin/mke2fs \
  ${OUTPUT_DIR}/sbin/dmidecode \
  ${OUTPUT_DIR}/usr/sbin/smartctl \
  ${OUTPUT_DIR}/sbin/lsiutil \
  ${OUTPUT_DIR}/sbin/sas2ircu \
  ${OUTPUT_DIR}/sbin/findslottodcno.sh \
  ${OUTPUT_DIR}/sbin/detect_hdd.sh \
  ${OUTPUT_DIR}/usr/sbin/hpacucli \
  ${OUTPUT_DIR}/usr/StorMan/arcconf \
  ${OUTPUT_DIR}/usr/lib64/libstdc++.so.6.0.13 \
  ${OUTPUT_DIR}/usr/lib/libstdc++-3-libc6.2-2-2.10.0.so \
  ${OUTPUT_DIR}/lib64/libm-2.12.so \
  ${OUTPUT_DIR}/lib64/libgcc_s-4.4.7-20120601.so.1 \
  ${OUTPUT_DIR}/lib64/libcap.so.2.16 \
  ${OUTPUT_DIR}/lib64/libpci.so.3.1.10 \
  ${OUTPUT_DIR}/sbin/disks_common.sh \
  ${OUTPUT_DIR}/sbin/ethtool \
  ${OUTPUT_DIR}/sbin/mdadm \
  ${OUTPUT_DIR}/sbin/lspci \
  ${OUTPUT_DIR}/usr/sbin/update-pciids \
  ${OUTPUT_DIR}/usr/share/hwdata/pci.ids \
  ${OUTPUT_DIR}/bin/bash \
  ${OUTPUT_DIR}/bin/df \
  ${OUTPUT_DIR}/bin/ls \
  ${OUTPUT_DIR}/usr/bin/csplit \
  ${OUTPUT_DIR}/usr/bin/head \
  ${OUTPUT_DIR}/usr/bin/tail \
  ${OUTPUT_DIR}/usr/bin/tee \
  ${OUTPUT_DIR}/usr/bin/base64 \
  ${OUTPUT_DIR}/usr/bin/patch \
  ${OUTPUT_DIR}/sbin/postencode.sh \
  ${OUTPUT_DIR}/sbin/eth-info.sh \
  ${OUTPUT_DIR}/sbin/dmidecode-info.sh \
  ${OUTPUT_DIR}/sbin/grub-install.patch \


${OUTPUT_DIR}/sbin/manufacture.sh: ${PROD_TREE_ROOT}/src/base_os/common/script_files/manufacture.sh ${OUTPUT_DIR}/sbin
	@echo Copy manufacture.sh
	@cp ${PROD_TREE_ROOT}/src/base_os/common/script_files/manufacture.sh $@
	@touch $@
	@chown ${REAL_USER} $@
	@chmod 755 $@
	@ls -l $@

${OUTPUT_DIR}/sbin/writeimage.sh: ${PROD_TREE_ROOT}/src/base_os/common/script_files/writeimage.sh ${OUTPUT_DIR}/sbin
	@echo Copy writeimage.sh
	@cp ${PROD_TREE_ROOT}/src/base_os/common/script_files/writeimage.sh $@
	@touch $@
	@chown ${REAL_USER} $@
	@chmod 755 $@
	@ls -l $@

${OUTPUT_DIR}/etc/customer_rootflop.sh: ../base_os/common/script_files/customer_rootflop.sh ${OUTPUT_DIR}/etc
	@echo Copy customer_rootflop.sh
	@cp ../base_os/common/script_files/customer_rootflop.sh $@
	@touch $@
	@chown ${REAL_USER} $@
	@chmod 755 $@
	@ls -l $@

${OUTPUT_DIR}/sbin/parse_manuf_opt.sh: parse_manuf_opt.sh ${OUTPUT_DIR}/sbin
	@echo Copy parse_manuf_opt.sh
	@cp parse_manuf_opt.sh $@
	@touch $@
	@chown ${REAL_USER} $@
	@chmod 755 $@
	@ls -l $@

${OUTPUT_DIR}/sbin/vxa_functions.dot: vxa_functions.dot ${OUTPUT_DIR}/sbin
	@echo Copy vxa_functions.dot
	@cp vxa_functions.dot $@
	@touch $@
	@chown ${REAL_USER} $@
	@chmod 755 $@
	@ls -l $@

${OUTPUT_DIR}/sbin/pre-install.sh: pre-install.sh ${OUTPUT_DIR}/sbin
	@echo Copy pre-install.sh
	@cp pre-install.sh $@
	@touch $@
	@chown ${REAL_USER} $@
	@chmod 755 $@
	@ls -l $@

${OUTPUT_DIR}/sbin/post-install.sh: post-install.sh ${OUTPUT_DIR}/sbin
	@echo Copy post-install.sh
	@cp post-install.sh $@
	@touch $@
	@chown ${REAL_USER} $@
	@chmod 755 $@
	@ls -l $@

${OUTPUT_DIR}/sbin/install-mfc: install-mfc ${OUTPUT_DIR}/sbin
	@echo Copy install-mfc
	@cp install-mfc $@
	@touch $@
	@chown ${REAL_USER} $@
	@chmod 755 $@
	@ls -l $@

${OUTPUT_DIR}/sbin/make_raid.sh: make_raid.sh ${OUTPUT_DIR}/sbin
	@echo Copy make_raid.sh
	@cp make_raid.sh $@
	@touch $@
	@chown ${REAL_USER} $@
	@chmod 755 $@
	@ls -l $@

${OUTPUT_DIR}/sbin/grub_setup_raid.sh: grub_setup_raid.sh ${OUTPUT_DIR}/sbin
	@echo Copy grub_setup_raid.sh
	@cp grub_setup_raid.sh $@
	@touch $@
	@chown ${REAL_USER} $@
	@chmod 755 $@
	@ls -l $@

${OUTPUT_DIR}/sbin/eth-setup: eth-setup ${OUTPUT_DIR}/sbin
	@echo Copy eth-setup
	@cp eth-setup $@
	@touch $@
	@chown ${REAL_USER} $@
	@chmod 755 $@
	@ls -l $@

${OUTPUT_DIR}/sbin/eth-setup-interactive: eth-setup-interactive ${OUTPUT_DIR}/sbin
	@echo Copy eth-setup-interactive
	@cp eth-setup-interactive $@
	@touch $@
	@chown ${REAL_USER} $@
	@chmod 755 $@
	@ls -l $@

${OUTPUT_DIR}/sbin/eth-setup-menu: eth-setup-menu ${OUTPUT_DIR}/sbin
	@echo Copy eth-setup-menu
	@cp eth-setup-menu $@
	@touch $@
	@chown ${REAL_USER} $@
	@chmod 755 $@
	@ls -l $@

${OUTPUT_DIR}/sbin/vxa-eth-setup.sh: ../install_files/vxa/vxa-eth-setup.sh ${OUTPUT_DIR}/sbin
	@echo Copy vxa-eth-setup.sh
	@cp ../install_files/vxa/vxa-eth-setup.sh $@
	@touch $@
	@chown ${REAL_USER} $@
	@chmod 755 $@
	@ls -l $@

${OUTPUT_DIR}/sbin/mvt-printout: mvt-printout ${OUTPUT_DIR}/sbin
	@echo Copy mvt-printout
	@cp mvt-printout $@
	@touch $@
	@chown ${REAL_USER} $@
	@chmod 755 $@
	@ls -l $@

${OUTPUT_DIR}/sbin/eth-test: eth-test ${OUTPUT_DIR}/sbin
	@echo Copy eth-test
	@cp eth-test $@
	@touch $@
	@chown ${REAL_USER} $@
	@chmod 755 $@
	@ls -l $@

${OUTPUT_DIR}/usr/sbin/mke2fs: mke2fs ${OUTPUT_DIR}/usr/sbin
	@echo Copy mke2fs
	@cp mke2fs $@
	@touch $@
	@chown ${REAL_USER} $@
	@chmod 755 $@
	@ls -l $@

${OUTPUT_DIR}/sbin/dmidecode: ../thirdparty/binary/dmidecode/6/dmidecode ${OUTPUT_DIR}/sbin
	@echo Copy from ../thirdparty/binary/dmidecode/6
	@cp ../thirdparty/binary/dmidecode/6/dmidecode  $@
	@touch $@
	@chown ${REAL_USER} $@
	@chmod 755 $@
	@ls -l $@

${OUTPUT_DIR}/usr/sbin/smartctl: ../thirdparty/binary/smartctl/smartctl ${OUTPUT_DIR}/usr/sbin
	@echo Copy from ../thirdparty/binary/smartctl/
	@cp ../thirdparty/binary/smartctl/smartctl  $@
	@touch $@
	@chown ${REAL_USER} $@
	@chmod 755 $@
	@ls -l $@

${OUTPUT_DIR}/sbin/lsiutil: ../thirdparty/binary/lsiutil/lsiutil.32 ${OUTPUT_DIR}/sbin
	@echo Copy from ../thirdparty/binary/lsiutil/
	@cp ../thirdparty/binary/lsiutil/lsiutil.32 $@
	@touch $@
	@chown ${REAL_USER} $@
	@chmod 755 $@
	@ls -l $@

${OUTPUT_DIR}/sbin/sas2ircu: ../thirdparty/binary/sas2ircu/sas2ircu.32 ${OUTPUT_DIR}/sbin
	@echo Copy from ../thirdparty/binary/sas2ircu/
	@cp ../thirdparty/binary/sas2ircu/sas2ircu.32  $@
	@touch $@
	@chown ${REAL_USER} $@
	@chmod 755 $@
	@ls -l $@

${OUTPUT_DIR}/sbin/findslottodcno.sh:  ../install_files/generic_cli_bin/findslottodcno.sh ${OUTPUT_DIR}/sbin
	@echo Copy ../install_files/generic_cli_bin/findslottodcno.sh
	@cp ../install_files/generic_cli_bin/findslottodcno.sh $@
	@touch $@
	@chown ${REAL_USER} $@
	@chmod 755 $@
	@ls -l $@

${OUTPUT_DIR}/sbin/detect_hdd.sh:  ../install_files/generic_cli_bin/detect_hdd.sh ${OUTPUT_DIR}/sbin
	@echo Copy ../install_files/generic_cli_bin/detect_hdd.sh
	@cp ../install_files/generic_cli_bin/detect_hdd.sh $@
	@touch $@
	@chown ${REAL_USER} $@
	@chmod 755 $@
	@ls -l $@

# Note hpacucli-8.20-18.noarch.rpm has these files:
# 9392512 Apr 10  2012 ./opt/compaq/hpacucli/bld/.hpacucli
# 6458488 Apr 10  2012 ./opt/compaq/hpacucli/bld/.hpacuscripting
#     505 Apr 10  2012 ./opt/compaq/hpacucli/bld/hpacucli
#    7725 Apr 10  2012 ./opt/compaq/hpacucli/bld/hpacucli-9.10-22.0.x86_64.txt
#   20943 Apr 10  2012 ./opt/compaq/hpacucli/bld/hpacucli.license
#     535 Apr 10  2012 ./opt/compaq/hpacucli/bld/hpacuscripting
# 2397364 Apr 10  2012 ./opt/compaq/hpacucli/bld/libcpqimgr-x86_64.so
#     132 Apr 10  2012 ./opt/compaq/hpacucli/bld/mklocks.sh
#     390 Apr 10  2012 ./usr/man/man8/hpacucli.8.gz
#     505 Apr 10  2012 ./usr/sbin/hpacucli
#     535 Apr 10  2012 ./usr/sbin/hpacuscripting
${OUTPUT_DIR}/usr/sbin/hpacucli: ${HPACUCLI_RPM} ${OUTPUT_DIR}/usr/sbin ${OUTPUT_DIR}/opt/compaq/hpacucli/bld
	@echo Extract from ${HPACUCLI_RPM}
	@cd ${OUTPUT_DIR} ; \
	rpm2cpio ${HPACUCLI_RPM} | cpio -icdu ./usr/sbin/hpacucli \
	./opt/compaq/hpacucli/bld/.hpacucli \
	./opt/compaq/hpacucli/bld/hpacucli \
	./opt/compaq/hpacucli/bld/hpacucli-8.20-18.linux.txt \
	./opt/compaq/hpacucli/bld/hpacucli.license \
	./opt/compaq/hpacucli/bld/libcpqimgr.so \
	./opt/compaq/hpacucli/bld/libhwmim3.so \
	./opt/compaq/hpacucli/bld/libossingleton.so
	@chown ${REAL_USER} $@
	@chown -R ${REAL_USER} ${OUTPUT_DIR}/opt/compaq
	@chmod 755 $@
	@chmod 755 ${OUTPUT_DIR}/opt/compaq/hpacucli/bld/.hpacucli
	@chmod 755 ${OUTPUT_DIR}/opt/compaq/hpacucli/bld/hpacucli
	@ls -l $@ ${OUTPUT_DIR}/opt/compaq/hpacucli/bld/.??* ${OUTPUT_DIR}/opt/compaq/hpacucli/bld/*

# Note StorMan-6.10-18451.rpm has all files under ./usr/StorMan/
${OUTPUT_DIR}/usr/StorMan/arcconf: ${STORMAN_RPM}
	@echo Extract from ${STORMAN_RPM}
	@cd ${OUTPUT_DIR} ; \
	rpm2cpio ${STORMAN_RPM} | cpio -icdu './usr/StorMan/*'
	rm -rf ${OUTPUT_DIR}/usr/StorMan/help
	rm -rf ${OUTPUT_DIR}/usr/StorMan/sun-jre150linux32.tgz
	rm -rf ${OUTPUT_DIR}/usr/StorMan/*.jar
	rm -rf ${OUTPUT_DIR}/usr/StorMan/*.html
	rm -rf ${OUTPUT_DIR}/usr/StorMan/StorMan.sh
	@chown -R ${REAL_USER} ${OUTPUT_DIR}/usr/StorMan
	@chmod 755 $@
	@ls -ld ${OUTPUT_DIR}/usr/StorMan/*

# Note: libstdc++-4.4.6-4.el6.x86_64.rpm has these files:
#  ./usr/lib64/libstdc++.so.6 -> libstdc++.so.6.0.13
#  ./usr/lib64/libstdc++.so.6.0.13
${OUTPUT_DIR}/usr/lib64/libstdc++.so.6.0.13: ${LIBSTDCPP_RPM} ${OUTPUT_DIR}/usr/lib64
	@echo Extract from ${LIBSTDCPP_RPM}
	@cd ${OUTPUT_DIR} ; \
	rpm2cpio ${LIBSTDCPP_RPM} | cpio -icdu './usr/lib64/*'
	@chown -R ${REAL_USER} ${@}
	@chmod 755 $@
	@ls -ld ${OUTPUT_DIR}/usr/lib64/libstdc++.*

# Note: The compat-libstdc++-296-2.96-138.i386.rpm has these files:
#  ./usr/lib/libstdc++-3-libc6.2-2-2.10.0.so
#  ./usr/lib/libstdc++-libc6.2-2.so.3 -> libstdc++-3-libc6.2-2-2.10.0.so
# Note: libstdc++-libc6.2-2.so.3 is needed by hpacucli
${OUTPUT_DIR}/usr/lib/libstdc++-3-libc6.2-2-2.10.0.so: ${COMPAT_LIBSTDCPP_RPM} ${OUTPUT_DIR}/usr/lib
	@echo Extract from ${COMPAT_LIBSTDCPP_RPM}
	@cd ${OUTPUT_DIR} ; \
	rpm2cpio ${COMPAT_LIBSTDCPP_RPM} | cpio -icdu
	@chown -R ${REAL_USER} ${OUTPUT_DIR}/usr/lib/libstdc++-*
	@chmod 755 $@
	@ls -ld ${OUTPUT_DIR}/usr/lib/libstdc++-*

# Note: glibc-2.12-1.80.el6_3.5.x86_64.rpm has LOTS of files.
# These are needed by smartctl and arcconf:
#  ./lib64/libm-2.12.so
#  ./lib64/libm.so.6 -> libm-2.12.so
${OUTPUT_DIR}/lib64/libm-2.12.so: ${GLIBC_RPM} ${OUTPUT_DIR}/lib64
	@echo Extract from ${GLIBC_RPM}
	@cd ${OUTPUT_DIR} ; \
	rpm2cpio ${GLIBC_RPM} | cpio -icdu ./lib64/libm-2.12.so ./lib64/libm.so.6
	@chown -R ${REAL_USER} ${OUTPUT_DIR}/lib64/libm*
	@chmod 755 $@
	@ls -ld ${OUTPUT_DIR}/lib64/libm*

# Note: The libgcc-4.4.6-*.rpm has these files:
# ./lib64/libgcc_s-4.4.7-20120601.so.1
# ./lib64/libgcc_s.so.1 -> libgcc_s-4.4.7-20120601.so.1
# ./usr/share/doc/libgcc-4.4.7
# ./usr/share/doc/libgcc-4.4.7/COPYING.LIB
${OUTPUT_DIR}/lib64/libgcc_s-4.4.7-20120601.so.1: ${LIBGCC_RPM} ${OUTPUT_DIR}/lib64
	@echo Extract from ${LIBGCC_RPM}
	@cd ${OUTPUT_DIR} ; \
	rpm2cpio ${LIBGCC_RPM} | cpio -icdu ./lib64/libgcc**
	@chown -R ${REAL_USER} ${OUTPUT_DIR}/lib64/libgcc*
	@chmod 755 $@
	@ls -ld ${OUTPUT_DIR}/lib64/libgcc*

# Note: The pciutils-libs-3.1.10-2.el6.x86_64.rpm has these files:
#  ./lib64/libpci.so.3.1.10
#  ./lib64/libpci.so.3 -> libpci.so.3.1.10
#  required for lspci to work
${OUTPUT_DIR}/lib64/libpci.so.3.1.10: ${PCIUTILSLIB_RPM} ${OUTPUT_DIR}/lib64
	@echo Extract from ${PCIUTILSLIB_RPM}
	@cd ${OUTPUT_DIR} ; \
	rpm2cpio ${PCIUTILSLIB_RPM} | cpio -icdu ./lib64/libpci**
	@chown -R ${REAL_USER} ${OUTPUT_DIR}/lib64/libpci*

# Note: The libcap-2.16-5.5.el6.x86_64.rpm has these files:
#  ./lib64/libcap.so.2.16
#  ./lib64/libcap.so.2 -> libcap.so.2.16
${OUTPUT_DIR}/lib64/libcap.so.2.16: ${LIBCAP} ${OUTPUT_DIR}/lib64
	@echo Extract from ${LIBCAP}
	@cd ${OUTPUT_DIR} ; \
	rpm2cpio ${LIBCAP} | cpio -icdu ./lib64/libcap**
	@chown -R ${REAL_USER} ${OUTPUT_DIR}/lib64/libcap*
	@chmod 755 $@
	@ls -ld ${OUTPUT_DIR}/lib64/libcap*

${OUTPUT_DIR}/sbin/disks_common.sh:  ../install_files/disks_common.sh ${OUTPUT_DIR}/sbin
	@echo Copy disks_common.sh
	@cp ../install_files/disks_common.sh $@
	@touch $@
	@chown ${REAL_USER} $@
	@chmod 755 $@
	@ls -l $@

${OUTPUT_DIR}/sbin/ethtool: ${ETHTOOL_RPM} ${OUTPUT_DIR}/sbin
	@echo Extract from ${ETHTOOL_RPM}
	@cd ${OUTPUT_DIR} ; \
	rpm2cpio ${ETHTOOL_RPM} | cpio -icdu ./sbin/ethtool
	@chown ${REAL_USER} $@
	@chmod 755 $@
	@ls -l $@

${OUTPUT_DIR}/sbin/mdadm: ${MDADM_RPM} ${OUTPUT_DIR}/sbin
	@echo Extract from ${MDADM_RPM}
	@cd ${OUTPUT_DIR} ; \
	rpm2cpio ${MDADM_RPM} | cpio -icdu ./sbin/mdadm
	@chown ${REAL_USER} $@
	@chmod 755 $@
	@ls -l $@

${OUTPUT_DIR}/sbin/lspci: ${PCIUTILS_RPM} ${OUTPUT_DIR}/sbin
	@echo Extract from ${PCIUTILS_RPM}
	@cd ${OUTPUT_DIR} ; \
	rpm2cpio ${PCIUTILS_RPM} | cpio -icdu ./sbin/lspci
	@chown ${REAL_USER} $@
	@chmod 755 $@
	@ls -l $@

${OUTPUT_DIR}/usr/sbin/update-pciids: ${PCIUTILS_RPM} ${OUTPUT_DIR}/sbin
	@echo Extract from ${PCIUTILS_RPM}
	@cd ${OUTPUT_DIR} ; \
	rpm2cpio ${PCIUTILS_RPM} | cpio -icdu ./usr/sbin/update-pciids
	@chown ${REAL_USER} $@
	@chmod 755 $@
	@ls -l $@

${OUTPUT_DIR}/usr/share/hwdata/pci.ids: /usr/share/hwdata/pci.ids ${OUTPUT_DIR}/usr/share/hwdata
	@echo Get latest pci.ids file using /sbin/update-pciids
	-/usr/sbin/update-pciids
	cp /usr/share/hwdata/pci.ids $@
	@chown ${REAL_USER} $@
	@chmod 644 $@
	@ls -l $@

${OUTPUT_DIR}/bin/bash: ${BASH_RPM} ${OUTPUT_DIR}/bin
	@echo Extract from ${BASH_RPM}
	@cd ${OUTPUT_DIR} ; \
	rpm2cpio ${BASH_RPM} | cpio -icdu ./bin/bash
	@chown ${REAL_USER} $@
	@chmod 755 $@
	@ls -l $@

${OUTPUT_DIR}/bin/df: ${COREUTILS_RPM} ${OUTPUT_DIR}/bin
	@echo Extract from ${COREUTILS_RPM}
	@cd ${OUTPUT_DIR} ; \
	rpm2cpio ${COREUTILS_RPM} | cpio -icdu ./bin/df
	@chown ${REAL_USER} $@
	@chmod 755 $@
	@ls -l $@

${OUTPUT_DIR}/bin/ls: ${COREUTILS_RPM} ${OUTPUT_DIR}/bin
	@echo Extract from ${COREUTILS_RPM}
	@cd ${OUTPUT_DIR} ; \
	rpm2cpio ${COREUTILS_RPM} | cpio -icdu ./bin/ls
	@chown ${REAL_USER} $@
	@chmod 755 $@
	@ls -l $@

${OUTPUT_DIR}/usr/bin/csplit: ${COREUTILS_RPM} ${OUTPUT_DIR}/usr/bin
	@echo Extract from ${COREUTILS_RPM}
	@cd ${OUTPUT_DIR} ; \
	rpm2cpio ${COREUTILS_RPM} | cpio -icdu ./usr/bin/csplit
	@chown ${REAL_USER} $@
	@chmod 755 $@
	@ls -l $@

${OUTPUT_DIR}/usr/bin/head: ${COREUTILS_RPM} ${OUTPUT_DIR}/usr/bin
	@echo Extract from ${COREUTILS_RPM}
	@cd ${OUTPUT_DIR} ; \
	rpm2cpio ${COREUTILS_RPM} | cpio -icdu ./usr/bin/head
	@chown ${REAL_USER} $@
	@chmod 755 $@
	@ls -l $@

${OUTPUT_DIR}/usr/bin/tail: ${COREUTILS_RPM} ${OUTPUT_DIR}/usr/bin
	@echo Extract from ${COREUTILS_RPM}
	@cd ${OUTPUT_DIR} ; \
	rpm2cpio ${COREUTILS_RPM} | cpio -icdu ./usr/bin/tail
	@chown ${REAL_USER} $@
	@chmod 755 $@
	@ls -l $@

${OUTPUT_DIR}/usr/bin/tee: ${COREUTILS_RPM} ${OUTPUT_DIR}/usr/bin
	@echo Extract from ${COREUTILS_RPM}
	@cd ${OUTPUT_DIR} ; \
	rpm2cpio ${COREUTILS_RPM} | cpio -icdu ./usr/bin/tee
	@chown ${REAL_USER} $@
	@chmod 755 $@
	@ls -l $@

${OUTPUT_DIR}/usr/bin/base64: ${COREUTILS_RPM} ${OUTPUT_DIR}/usr/bin
	@echo Extract from ${COREUTILS_RPM}
	@cd ${OUTPUT_DIR} ; \
	rpm2cpio ${COREUTILS_RPM} | cpio -icdu ./usr/bin/base64
	@chown ${REAL_USER} $@
	@chmod 755 $@
	@ls -l $@

${OUTPUT_DIR}/usr/bin/patch: ${PATCH_RPM} ${OUTPUT_DIR}/usr/bin
	@echo Extract from ${PATCH_RPM}
	@cd ${OUTPUT_DIR} ; \
	rpm2cpio ${PATCH_RPM} | cpio -icdu ./usr/bin/patch
	@chown ${REAL_USER} $@
	@chmod 755 $@
	@ls -l $@

# postencode.sh use base64
${OUTPUT_DIR}/sbin/postencode.sh: ../install_files/postencode.sh ${OUTPUT_DIR}/sbin
	@echo Copy postencode.sh
	@cp ../install_files/postencode.sh $@
	@touch $@
	@chown ${REAL_USER} $@
	@chmod 755 $@
	@ls -l $@

${OUTPUT_DIR}/sbin/eth-info.sh: ../install_files/eth-info.sh ${OUTPUT_DIR}/sbin
	@echo Copy eth-info.sh
	@cp ../install_files/eth-info.sh $@
	@touch $@
	@chown ${REAL_USER} $@
	@chmod 755 $@
	@ls -l $@

${OUTPUT_DIR}/sbin/dmidecode-info.sh: ../install_files/dmidecode-info.sh ${OUTPUT_DIR}/sbin
	@echo Copy dmidecode-info.sh
	@cp ../install_files/dmidecode-info.sh $@
	@touch $@
	@chown ${REAL_USER} $@
	@chmod 755 $@
	@ls -l $@

${OUTPUT_DIR}/sbin/grub-install.patch: ../thirdparty/patches/grub-install/grub-install.patch ${OUTPUT_DIR}/sbin
	@echo Copy grub-install.patch
	@cp ../thirdparty/patches/grub-install/grub-install.patch $@
	@touch $@
	@chown ${REAL_USER} $@
	@chmod 644 $@
	@ls -l $@

${OUTPUT_DIR}:
	mkdir -p $@
	@chown ${REAL_USER} $@

${OUTPUT_DIR}/root:
	mkdir -p $@
	@chown ${REAL_USER} $@

${OUTPUT_DIR}/etc:
	mkdir -p $@
	@chown ${REAL_USER} $@

${OUTPUT_DIR}/sbin:
	mkdir -p $@
	@chown ${REAL_USER} $@

${OUTPUT_DIR}/bin:
	mkdir -p $@
	@chown ${REAL_USER} $@

${OUTPUT_DIR}/lib64:
	mkdir -p $@
	@chown ${REAL_USER} $@

${OUTPUT_DIR}/usr:
	mkdir -p $@
	@chown ${REAL_USER} $@

${OUTPUT_DIR}/usr/bin:
	mkdir -p $@
	@chown ${REAL_USER} $@

${OUTPUT_DIR}/usr/lib:
	mkdir -p $@
	@chown ${REAL_USER} $@

${OUTPUT_DIR}/usr/lib64:
	mkdir -p $@
	@chmod 777 $@
	@chown ${REAL_USER} $@

${OUTPUT_DIR}/usr/sbin:
	mkdir -p $@
	@chown ${REAL_USER} $@

${OUTPUT_DIR}/usr/share:
	mkdir -p $@
	@chown ${REAL_USER} $@

${OUTPUT_DIR}/usr/share/hwdata:
	mkdir -p $@
	@chown ${REAL_USER} $@

${OUTPUT_DIR}/opt/compaq/hpacucli/bld:
	mkdir -p $@
	@chown ${REAL_USER} $@

${RELEASE_DIR}:
	mkdir -p $@
	@chown ${REAL_USER} $@


.PHONY: release
release: install ${RELEASE_DIR}
	.  ${FULL_INSTALL_DIR}/build_version.sh ; \
	build_suffix=$${BUILD_PROD_NAME}-$${BUILD_TARGET_ARCH_LC}$${BUILD_TARGET_HWNAME_LC:+-$${BUILD_TARGET_HWNAME_LC}}-`echo $${BUILD_DATE} | sed 's/[-:]//g' | sed 's/ /-/g'` ;\
	echo BUILD_CUSTOMER_SUFFIX=$${BUILD_CUSTOMER_SUFFIX} ;\
	test -n "$${BUILD_CUSTOMER_SUFFIX}" && build_suffix=$${BUILD_CUSTOMER_SUFFIX} ;\
	export TGZ_FILE=${RELEASE_DIR}/manufacture-$${build_suffix}.tgz ;\
	echo build_suffix=$${build_suffix} ;\
	echo TGZ_FILE=$${TGZ_FILE} ;\
	cd ${OUTPUT_DIR} ;\
	tar -zcvf $${TGZ_FILE} * ;\
	ls -l $${TGZ_FILE}

.PHONY: env
env:
	set
