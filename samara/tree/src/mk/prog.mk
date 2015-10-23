#
#
#
#

PROG?=prog
INSTALL_TREE?=image
PROG_BUILD_DIR?=${BIN_DIR}/${PROG}
PROG_INSTALL_DIR?=bin
FULL_BUILD_DIR=${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${PROG_BUILD_DIR}
FULL_INSTALL_DIR=${PROD_INSTALL_OUTPUT_DIR}/${INSTALL_TREE}/${PROD_INSTALL_PREFIX}/${PROG_INSTALL_DIR}

-include ${PROD_TREE_ROOT}/src/mk/common.mk

# XXX static linkage
# XXX dynamic
# XXX libs to link against
# XXX local libs to make


# Set some internal source settings based on SRCS
C_SRCS=$(filter %.c,${SRCS})
CXX_SRCS=$(filter %.cc,${SRCS}) $(filter %.cpp,${SRCS})

# Derive the objects that will be needed from the SRCS
C_OBJS=$(addprefix ${FULL_BUILD_DIR}/,$(patsubst %.c,%.o,${C_SRCS}))
CXX_OBJS=$(addprefix ${FULL_BUILD_DIR}/,$(addsuffix .o,$(basename ${CXX_SRCS})))

OBJS=${C_OBJS} ${CXX_OBJS}

BUILD_DIRS=$(sort ${FULL_BUILD_DIR}/ $(dir ${OBJS}))

# This makefile template is not fully "-j" safe.  Do not remove the
# following lines.
ifneq (${MAKECMDGOALS},self-objs-parallel)
.NOTPARALLEL:
endif

# ============================== ALL ==============================
FULL_PROG_TARGET=${FULL_BUILD_DIR}/${PROG}

PROG_PRE_BUILD_TARGET?=
PROG_POST_BUILD_TARGET?=

LIBDEPS=libdeps

all: startup ensure_build_dirs ${PROG_PRE_BUILD_TARGET} all-recursive ${LIBDEPS} self-objs ${FULL_PROG_TARGET} ${PROG_POST_BUILD_TARGET} shutdown

self-objs:
	$(standard-self-objs)

self-objs-parallel: ${OBJS}

# XXX This only works for c/c++ files being linked into a program right now
${FULL_PROG_TARGET}: ${DEP_LIBS} ${DEP_ILIBS} ${DEP_CUSTLIBS} ${DEP_THIRDPARTY_LIBS} ${DEP_CUSTILIBS} ${OBJS} ${OBJADD}
	@if [ ! -z "${PROG_LINKER}" ]; then \
		prog_linker="${PROG_LINKER}" ; \
	elif test -z "${CXX_OBJS}"; then \
		prog_linker="${CC}" ; \
	else \
		prog_linker="${CXX}" ; \
	fi ; \
	if [ ! -z "${PURIFY}" -a "${PURIFY}" = "1" -a ! -z "${PURIFY_LINK}" ]; then \
		prog_linker="${PURIFY_LINK} $${prog_linker}" ; \
	fi ; \
	cmd="$${prog_linker} ${ALL_LDFLAGS} -o ${FULL_PROG_TARGET} \
		${OBJS} ${OBJADD} ${ALL_LDADD} ${ALL_LDADD_PROG}" ; \
	if test ! "${PROD_BUILD_VERBOSE}" -eq 1; then \
		echo "Linking ${FULL_PROG_TARGET} for ${PROD_TARGET_PLATFORM_FULL_LC} ${PROD_TARGET_CPU_LC}-${PROD_TARGET_CPU_WORDSIZE}"; \
	else \
		echo "$${cmd}"; \
	fi ; \
	$${cmd}
	@${ECHO} "Built binary ${PROG}"

CLEANFILES+=${FULL_PROG_TARGET} ${OBJS}

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
PROG_STRIP?=0
ifneq (${PROD_HOST_OS},SUNOS)
PROG_STRIP_FLAGS?=--strip-debug
else
PROG_STRIP_FLAGS?=-x
endif
PROG_INSTALL_FLAGS?=

PROG_PERM_OWNER?=root
PROG_PERM_GROUP?=bin
PROG_PERM_MODE?=755

ifeq (${PROG_STRIP},1)
DO_PROG_STRIP?=${STRIP} ${PROG_STRIP_FLAGS} ${FULL_INSTALL_DIR}/${PROG}
else
DO_PROG_STRIP=
endif

ifndef INSTALL_FAKE
DO_PROG_INSTALL?=${INSTALL} -o ${PROG_PERM_OWNER} \
		-g ${PROG_PERM_GROUP} -m ${PROG_PERM_MODE} \
		${PROG_INSTALL_FLAGS} ${FULL_PROG_TARGET} \
		${FULL_INSTALL_DIR}
else
DO_PROG_INSTALL?=${INSTALL} -m ${PROG_PERM_MODE} \
		${PROG_INSTALL_FLAGS} ${FULL_PROG_TARGET} \
		${FULL_INSTALL_DIR}
endif

DO_PROG_POST_INSTALL?=

install: ensure_install_dirs install-recursive ${FULL_PROG_TARGET}
	${DO_PROG_INSTALL}
	${DO_PROG_STRIP}
	${DO_PROG_POST_INSTALL}

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

C_DEPS=$(patsubst %.o,%.d,${C_OBJS})
CXX_DEPS=$(patsubst %.o,%.d,${CXX_OBJS})
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
