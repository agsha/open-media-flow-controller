#
#
#
#

#
# This Makefile template supports 8 types of directives for manipulating
# the install dir:
#
# SYS_INSTALL_DIR_var_opt="dir 0755 root root /var/opt"
# SYS_INSTALL_FILE_grub_conf="file grub.conf 0755 root root /bootmgr/boot/grub.conf"
# SYS_TOUCH_FILE_messages="touch 0644 root root /var/log/messages"
# SYS_CREATE_SYMLINK_hosts="symlink /etc /var/opt/tms/output/hosts hosts"
# SYS_CREATE_SPECIAL_dev_exa="special c 7 8 0750 root root /dev/exa"
# SYS_INSTALL_TAR="tar test.tar /var/opt/test"
# SYS_INSTALL_TAR_GZ="tar-gz test.tar.gz /var/opt/test"
# SYS_INSTALL_TAR_BZ="tar-bz test.tar.bz2 /var/opt/test"
#
# With the last three, the owner, group, and mode come out of the tar file
# itself, so they are not specified in the directive.
#

INSTALL_TREE?=image
INSTALL_DIRECTIVES?=

# Warning: do not use this override unless you are sure you understand
# the implications, which could include writing outside the output tree.
ifneq ($(strip ${FULL_INSTALL_DIR_OVERRIDE}),1)
FULL_INSTALL_DIR=${PROD_INSTALL_OUTPUT_DIR}/${INSTALL_TREE}
else
FULL_INSTALL_DIR?=${PROD_INSTALL_OUTPUT_DIR}/${INSTALL_TREE}
endif

# CHECK_SOURCE_DIR is the directory where the files to be verified live.
# CHECK_SOURCE_FILES is the name of an 'lfi' file which contains hashes,
# etc.  This is like pkg_src_check.txt in other contexts.

CHECK_SOURCE_DIR?=
CHECK_SOURCE_FILES?=

-include ${PROD_TREE_ROOT}/src/mk/common.mk

# This makefile template is not "-j" safe.  Do not remove the following line
.NOTPARALLEL:

.PHONY: host_bin_lfi verify_inputs

.DEFAULT_GOAL := all

# ============================== ALL ==============================
FILESADV_PRE_ALL_TARGET?=
FILESADV_POST_ALL_TARGET?=

all: startup ${FILESADV_PRE_ALL_TARGET} verify_inputs ${FILESADV_POST_ALL_TARGET} all-recursive shutdown

CLEANFILES+=

startup:
	$(standard-startup)

shutdown:
	$(standard-shutdown)

HOST_BIN_LFI=${PROD_BUILD_OUTPUT_DIR}/host/${BIN_DIR}/lfi/lfi

host_bin_lfi:
	${MAKE} -C ${PROD_TREE_ROOT}/src/bin/lfi

ifneq ("$(strip ${CHECK_SOURCE_FILES})","")

verify_inputs: host_bin_lfi
	@for sf in ${CHECK_SOURCE_FILES}; do \
		echo "Verifying files in: ${CHECK_SOURCE_DIR}" ; \
		( cd ${CHECK_SOURCE_DIR}; ${HOST_BIN_LFI} -tmnug -c $${sf}) || exit 1 ; \
	done

else

verify_inputs:

endif

# ============================== INSTALL ==============================
DO_FILES_POST_INSTALL?=
FILESADV_PRE_INSTALL_TARGET?=
FILESADV_POST_INSTALL_TARGET?=

install: ensure_install_dirs ${FILESADV_PRE_INSTALL_TARGET} fa_install ${FILESADV_POST_INSTALL_TARGET} install-recursive 

fa_install: ensure_install_dirs
	@for directive in ${INSTALL_DIRECTIVES}; do \
		wc=0 ; \
		for w in $${directive} ; do \
			eval 'da_'$${wc}'="$${w}"' ; \
			wc=$$(($${wc} + 1)) ; \
		done ; \
	        da_count=$${wc} ; \
		case "$${da_0}" in \
		     dir) \
			if [ $${da_count} -ne 5 ]; then \
			   echo "ERROR: wrong # of args to $${directive}" ; \
			   exit 1; \
			fi ; \
			mkdir -p ${FULL_INSTALL_DIR}/$${da_4} ; \
			if [ $${?} -ne 0 ]; then \
			   echo "ERROR: mkdir of $${da_4} failed" ; \
			   exit 1; \
			fi ; \
			if [ "x${INSTALL_FAKE}" != "x1" ]; then \
			    chown $${da_2}:$${da_3} \
                                ${FULL_INSTALL_DIR}/$${da_4} ; \
			fi ; \
			if [ $${?} -ne 0 ]; then \
			   echo "ERROR: mkdir/chown of $${da_4} failed" ; \
			   exit 1; \
			fi ; \
			chmod $${da_1} ${FULL_INSTALL_DIR}/$${da_4} ; \
			if [ $${?} -ne 0 ]; then \
			   echo "ERROR: mkdir/chmod of $${da_4} failed" ; \
			   exit 1; \
			fi ; \
			echo "Made image dir $${da_4}"; \
			;; \
		     file) \
			if [ $${da_count} -ne 6 ]; then \
			   echo "ERROR: wrong # of args to $${directive}" ; \
			   exit 1; \
			fi ; \
			if [ "x${INSTALL_FAKE}" != "x1" ]; then \
                            install -m $${da_2} -o $${da_3} -g $${da_4} \
                                $${da_1} ${FULL_INSTALL_DIR}/$${da_5} ; \
			else \
                            install -m $${da_2} \
                                $${da_1} ${FULL_INSTALL_DIR}/$${da_5} ; \
			fi ; \
			if [ $${?} -ne 0 ]; then \
			   echo "ERROR: install of $${da_1} failed" ; \
			   exit 1; \
			fi ; \
			echo "Installed image file to $${da_5}"; \
			;; \
		     dir_nochown) \
			if [ $${da_count} -ne 3 ]; then \
			   echo "ERROR: wrong # of args to $${directive}" ; \
			   exit 1; \
			fi ; \
			mkdir -p ${FULL_INSTALL_DIR}/$${da_2} ; \
			if [ $${?} -ne 0 ]; then \
			   echo "ERROR: mkdir of $${da_4} failed" ; \
			   exit 1; \
			fi ; \
			chmod $${da_1} ${FULL_INSTALL_DIR}/$${da_2} ; \
			if [ $${?} -ne 0 ]; then \
			   echo "ERROR: mkdir/chmod of $${da_4} failed" ; \
			   exit 1; \
			fi ; \
			echo "Made image dir $${da_2}"; \
			;; \
		     file_nochown) \
			if [ $${da_count} -ne 4 ]; then \
			   echo "ERROR: wrong # of args to $${directive}" ; \
			   exit 1; \
			fi ; \
			install -m $${da_2} \
				$${da_1} ${FULL_INSTALL_DIR}/$${da_3} ; \
			if [ $${?} -ne 0 ]; then \
			   echo "ERROR: install of $${da_1} failed" ; \
			   exit 1; \
			fi ; \
			echo "Installed image file to $${da_3}"; \
			;; \
		     touch) \
			if [ $${da_count} -ne 5 ]; then \
			   echo "ERROR: wrong # of args to $${directive}" ; \
			   exit 1; \
			fi ; \
			touch ${FULL_INSTALL_DIR}/$${da_4} ; \
			if [ $${?} -ne 0 ]; then \
			   echo "ERROR: touch of $${da_4} failed" ; \
			   exit 1; \
			fi ; \
			if [ "x${INSTALL_FAKE}" != "x1" ]; then \
			    chown $${da_2}:$${da_3} \
				${FULL_INSTALL_DIR}/$${da_4} ; \
			fi ; \
			if [ $${?} -ne 0 ]; then \
			   echo "ERROR: touch/chown of $${da_4} failed" ; \
			   exit 1; \
			fi ; \
			chmod $${da_1} ${FULL_INSTALL_DIR}/$${da_4} ; \
			if [ $${?} -ne 0 ]; then \
			   echo "ERROR: touch/chmod of $${da_4} failed" ; \
			   exit 1; \
			fi ; \
			echo "Touched image file $${da_4}"; \
			;; \
		     symlink) \
			if [ $${da_count} -ne 4 ]; then \
			   echo "ERROR: wrong # of args to $${directive}" ; \
			   exit 1; \
			fi ; \
			case "$${da_3}" in \
                            .|..|*/*|*\\*) \
                                echo "ERROR: invalid symlink target filename: $${da_3}" ; \
                                exit 1 ; \
                                ;; \
			esac ; \
			(cd ${FULL_INSTALL_DIR}/$${da_1}; \
				ln -sfT ${PROD_ROOT_PREFIX}$${da_2} $${da_3}); \
			if [ $${?} -ne 0 ]; then \
			   echo "ERROR: symlink to $${da_3} failed" ; \
			   exit 1; \
			fi ; \
			echo "Symlinked in image to $${da_1}/$${da_3}"; \
			;; \
		     special) \
			if [ $${da_count} -ne 8 ]; then \
			   echo "ERROR: wrong # of args to $${directive}" ; \
			   exit 1; \
			fi ; \
			rm -f ${FULL_INSTALL_DIR}/$${da_7} ; \
			mknod -m $${da_4} ${FULL_INSTALL_DIR}/$${da_7} $${da_1} $${da_2} $${da_3} ; \
			if [ $${?} -ne 0 ]; then \
			   echo "ERROR: mknod of $${da_7} failed" ; \
			   exit 1; \
			fi ; \
			chown $${da_5}:$${da_6} \
				${FULL_INSTALL_DIR}/$${da_7} ; \
			if [ $${?} -ne 0 ]; then \
			   echo "ERROR: special/chown of $${da_7} failed" ; \
			   exit 1; \
			fi ; \
			echo "Created special file $${da_7}"; \
			;; \
		     tar) \
			if [ $${da_count} -ne 3 ]; then \
			   echo "ERROR: wrong # of args to $${directive}" ; \
			   exit 1; \
			fi ; \
			if [ "x${INSTALL_FAKE}" != "x1" ]; then \
			   tar -C ${FULL_INSTALL_DIR}/$${da_2} -p -xf $${da_1} ; \
			else \
			   tar -C ${FULL_INSTALL_DIR}/$${da_2} -xf $${da_1} ; \
			fi ; \
			if [ $${?} -ne 0 ]; then \
			   echo "ERROR: untar of $${da_1} to $${da_2} failed" ; \
			   exit 1; \
			fi ; \
			echo "Untarred $${da_1} to $${da_2}"; \
			;; \
		     tar-gz) \
			if [ $${da_count} -ne 3 ]; then \
			   echo "ERROR: wrong # of args to $${directive}" ; \
			   exit 1; \
			fi ; \
			if [ "x${INSTALL_FAKE}" != "x1" ]; then \
			   tar -C ${FULL_INSTALL_DIR}/$${da_2} -p -zxf $${da_1} ; \
			else \
			   tar -C ${FULL_INSTALL_DIR}/$${da_2} -zxf $${da_1} ; \
			fi ; \
			if [ $${?} -ne 0 ]; then \
			   echo "ERROR: gunzip/untar of $${da_1} to $${da_2} failed" ; \
			   exit 1; \
			fi ; \
			echo "Gunzipped/untarred $${da_1} to $${da_2}"; \
			;; \
		     tar-bz) \
			if [ $${da_count} -ne 3 ]; then \
			   echo "ERROR: wrong # of args to $${directive}" ; \
			   exit 1; \
			fi ; \
			if [ "x${INSTALL_FAKE}" != "x1" ]; then \
			   tar -C ${FULL_INSTALL_DIR}/$${da_2} -p -jxf $${da_1} ; \
			else \
			   tar -C ${FULL_INSTALL_DIR}/$${da_2} -jxf $${da_1} ; \
			fi ; \
			if [ $${?} -ne 0 ]; then \
			   echo "ERROR: bunzip/untar of $${da_1} to $${da_2} failed" ; \
			   exit 1; \
			fi ; \
			echo "Bunzipped/untarred $${da_1} to $${da_2}"; \
			;; \
		     *) \
			echo "ERROR: invalid directive $${da_0}"; \
			exit 1; \
		esac; \
	done
	${DO_FILES_POST_INSTALL}

# XXX make all the install directories we need
ensure_install_dirs:
	@if test ! -d ${FULL_INSTALL_DIR}/; then \
		mkdir -m 755 -p ${FULL_INSTALL_DIR}; \
		if test ! -d ${FULL_INSTALL_DIR}/; then \
			${ECHO} "Unable to create ${FULL_INSTALL_DIR}."; \
			 exit 1; \
		 fi; \
		 ${ECHO} "Created directory ${FULL_INSTALL_DIR}"; \
	 fi


# ============================== CLEAN ==============================
clean: clean-recursive
	${RM} ${CLEANFILES}

# ============================== DEPEND ==============================

depend: depend-recursive

# ============================== REALCLEAN ==============================
realclean: realclean-recursive clean

# ============================== CREATE-TRANLSATIONS =====================
i18n-create-catalogs: i18n-create-catalogs-recursive

# ============================== UPDATE-CATALOGS =========================
i18n-update-catalogs: i18n-update-catalogs-recursive


# Recursion
all-recursive install-recursive depend-recursive \
  clean-recursive realclean-recursive \
  i18n-create-catalogs-recursive \
  i18n-update-catalogs-recursive:
	$(recurse-subdirs)
	$(recurse-submakefiles)
