#
# 
#
#

SO_CLASS?=generic

INSTALL_TREE?=image
SO_BUILD_DIR?=${LIB_DIR}/${SO_CLASS}/modules
SO_INSTALL_DIR?=lib/${SO_CLASS}/modules
FULL_BUILD_DIR=${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${SO_BUILD_DIR}
FULL_INSTALL_DIR=${PROD_INSTALL_OUTPUT_DIR}/${INSTALL_TREE}/${PROD_INSTALL_PREFIX}/${SO_INSTALL_DIR}

-include ${PROD_TREE_ROOT}/src/mk/common.mk


# Set some internal source settings based on SRCS
C_SRCS=$(filter %.c,${SRCS})
CXX_SRCS=$(filter %.cc,${SRCS}) $(filter %.cpp,${SRCS})

# Derive the objects that will be needed from the SRCS
C_SO_OBJS=$(addprefix ${FULL_BUILD_DIR}/,$(patsubst %.c,%.so,${C_SRCS}))
CXX_SO_OBJS=$(addprefix ${FULL_BUILD_DIR}/,$(addsuffix .o,$(basename ${CXX_SRCS})))

SO_OBJS=${C_SO_OBJS} ${CXX_SO_OBJS}

BUILD_DIRS=$(sort ${FULL_BUILD_DIR}/ $(dir ${SO_OBJS}))

LIBDEPS=libdeps

# This makefile template is not fully "-j" safe.  Do not remove the
# following lines.
ifneq (${MAKECMDGOALS},self-objs-parallel)
.NOTPARALLEL:
endif

.PHONY: so_objs

# ============================== ALL ==============================
all: startup ensure_build_dirs ${LIBDEPS} all-recursive self-objs so_objs shutdown

so_objs: ${SO_OBJS}

self-objs:
	$(standard-self-objs)

self-objs-parallel: ${SO_OBJS}

CLEANFILES+=${SO_OBJS}

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

libdeps:
	@dep_lib_dirs='${DEP_LIB_DIRS} ${DEP_ILIB_DIRS} ${DEP_CUSTLIB_DIRS} ${DEP_THIRDPARTY_LIB_DIRS} ${DEP_CUSTILIB_DIRS}'; \
        failcom='exit 1'; \
        for f in x $$MAKEFLAGS; do \
          case $$f in \
            *=* | --[!k]*);; \
            *k*) failcom='fail=yes';; \
          esac; \
        done; \
	for subdir in $$dep_lib_dirs; do \
		(cd $$subdir && ${MAKE} all) || eval $$failcom; \
	done; \
	test -z "$$fail"


# ============================== INSTALL ==============================
SO_INSTALL_FLAGS?=

SO_PERM_OWNER=root
SO_PERM_GROUP=bin
SO_PERM_MODE=755

ifndef INSTALL_FAKE
SO_INSTALL?=${INSTALL} -o ${SO_PERM_OWNER} \
		-g ${SO_PERM_GROUP} -m ${SO_PERM_MODE} \
		${SO_INSTALL_FLAGS} ${SO_OBJS} \
		${FULL_INSTALL_DIR}
else
SO_INSTALL?=${INSTALL} -m ${SO_PERM_MODE} \
		${SO_INSTALL_FLAGS} ${SO_OBJS} \
		${FULL_INSTALL_DIR}
endif


install: ensure_install_dirs install-recursive so_objs
	${SO_INSTALL}

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

C_DEPS=$(patsubst %.so,%.d,${C_SO_OBJS})
CXX_DEPS=$(patsubst %.so,%.d,${CXX_SO_OBJS})
ALL_DEPS=${C_DEPS} ${CXX_DEPS}

depend: depend-recursive ${ALL_DEPS}

NODEPEND_TARGETS:=clean realclean startup shutdown

# Include .d files to get dependencies
#
# Note: we don't try to include any .d files if we're doing a 'clean', or if
# the given .d file does not already exist.  This might cause the .d files
# to be generated on their own, which should not be required.  The .d files
# are made in parallel with normal compilation.
#
ifeq (${MAKECMDGOALS},)
    ifneq (${.DEFAULT_GOAL},)
        THIS_GOAL:=all
    else
        THIS_GOAL:=${.DEFAULT_GOAL}
    endif
else
    THIS_GOAL:=${MAKECMDGOALS}
endif

ifneq ($(filter-out ${NODEPEND_TARGETS},${THIS_GOAL}),)
    dotd:=$(wildcard ${ALL_DEPS})
    ifneq (${dotd},)
        -include ${dotd}
    endif
endif

# ============================== REALCLEAN ==============================
realclean: realclean-recursive clean
	${RM} ${ALL_DEPS}


# ============================== CREATE-TRANSLATIONS =========================
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
