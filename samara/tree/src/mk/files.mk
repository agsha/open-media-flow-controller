#
# 
#
#

INSTALL_TREE?=image
FILES_INSTALL_DIR?=/etc
FULL_BUILD_DIR=${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${FILES_INSTALL_DIR}
FILES_INSTALL_PREFIX?=${PROD_INSTALL_PREFIX}
FULL_INSTALL_DIR=${PROD_INSTALL_OUTPUT_DIR}/${INSTALL_TREE}/${FILES_INSTALL_PREFIX}/${FILES_INSTALL_DIR}

-include ${PROD_TREE_ROOT}/src/mk/common.mk

# This makefile template is not "-j" safe.  Do not remove the following line
.NOTPARALLEL:

BUILD_DIRS=${FULL_BUILD_DIR}

# ============================== ALL ==============================

all: startup ensure_build_dirs all-recursive shutdown

CLEANFILES+=

startup:
	$(standard-startup)

shutdown:
	$(standard-shutdown)

ensure_build_dirs:
	@for subdir in ${BUILD_DIRS}; do \
		if test ! -d $${subdir}/; then \
			mkdir -m 755 -p $${subdir}/; \
			if test ! -d $${subdir}/; then \
				${ECHO} "Unable to create $${subdir}."; \
				 exit 1; \
			 fi; \
			 ${ECHO} "Created directory $${subdir}"; \
	    	fi; \
	done

# ============================== INSTALL ==============================
FILES_INSTALL_FLAGS?=
DO_FILES_POST_INSTALL?=

FILES_PERM_OWNER?=root
FILES_PERM_GROUP?=bin
FILES_PERM_MODE?=644

ifndef INSTALL_FAKE
FILES_INSTALL?=${INSTALL} -o ${FILES_PERM_OWNER} \
		-g ${FILES_PERM_GROUP} -m ${FILES_PERM_MODE} \
		${FILES_INSTALL_FLAGS} ${FILES} \
		${FULL_INSTALL_DIR}
else
FILES_INSTALL?=${INSTALL} -m ${FILES_PERM_MODE} \
		${FILES_INSTALL_FLAGS} ${FILES} \
		${FULL_INSTALL_DIR}
endif


install: ensure_install_dirs install-recursive ${FILES}
	${FILES_INSTALL}
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


# Recursion
all-recursive install-recursive depend-recursive \
  clean-recursive realclean-recursive:
	$(recurse-subdirs)
	$(recurse-submakefiles)
