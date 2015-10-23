#
#
#
#

LIB?=unknown

INSTALL_TREE?=image
LIB_BUILD_DIR?=${LIB_DIR}/lib${LIB}
LIB_INSTALL_DIR?=lib
FULL_BUILD_DIR=${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_BUILD_DIR}
FULL_INSTALL_DIR=${PROD_INSTALL_OUTPUT_DIR}/${INSTALL_TREE}/${PROD_INSTALL_PREFIX}/${LIB_INSTALL_DIR}
LIB_DYNAMIC?=0
LIB_DYNAMIC_PREFIX?=lib
LIB_DYNAMIC_SUFFIX?=
LIB_DYNAMIC_SONAME?=.1
LIB_DYNAMIC_SONAME_EMPTY?=0
LIB_DYNAMIC_REALNAME?=.1.0.0
LIB_LIBDEPS?=0
JNIDEPS=

# Java JNI library support.  Produces a loadable module from C/C++ files
ifeq (${LIB_JAVA_NATIVE_LIB},1)
	export OBJ_TYPE_SUFFIX=${JAVA_LIB_SUFFIX}
	export OBJ_PIC=1
	export OBJ_JNI_NATIVE=1
	LIB_DYNAMIC=1
	LIB_DYNAMIC_PREFIX=lib
	LIB_DYNAMIC_SUFFIX=lib
	LIB_DYNAMIC_SONAME=
	LIB_DYNAMIC_SONAME_EMPTY=1
	LIB_DYNAMIC_REALNAME=
	LIB_LIBDEPS=1
	FULL_INSTALL_DIR=${PROD_INSTALL_OUTPUT_DIR}/${INSTALL_TREE}/${PROD_INSTALL_PREFIX}/java/lib

	JAVA_BUILD_DIR=java
	JAVA_SOURCE_OUTPUT_DIR=${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${JAVA_BUILD_DIR}/sources
	JAVA_JNI_DIR=${JAVA_SOURCE_OUTPUT_DIR}/jniinclude
	JNI_HEADERS=$(addprefix ${JAVA_JNI_DIR},$(patsubst %.c,%.h,${JNI_SRCS}))

	SRCS+= ${JNI_SRCS}
	JNIDEPS=jnideps
endif

ifeq (${LIB_DYNAMIC},1)
    OBJ_PIC=1
    LIB_LIBDEPS=1

    # Install dynamic libraries, as they could get used as part of the running image
    NO_INSTALL=0
else
    OBJ_PIC?=0

    # For now, there is no need to install static libraries
    NO_INSTALL=1
endif

ifneq (${LIB_DYNAMIC_SONAME},)
	LIB_DYNAMIC_SONAME_ARG=-Wl,-soname,lib${LIB}${LIB_DYNAMIC_SUFFIX}.so${LIB_DYNAMIC_SONAME}
else
	ifeq (${LIB_DYNAMIC_SONAME_EMPTY},1)
		LIB_DYNAMIC_SONAME_ARG=-Wl,-soname,lib${LIB}.so
	else
		LIB_DYNAMIC_SONAME_ARG?=
	endif
endif

-include ${PROD_TREE_ROOT}/src/mk/common.mk


# Set some internal source settings based on SRCS
C_SRCS=$(filter %.c,${SRCS})
CXX_SRCS=$(filter %.cc,${SRCS}) $(filter %.cpp,${SRCS})

# Derive the objects that will be needed from the SRCS
C_OBJS=$(addprefix ${FULL_BUILD_DIR}/,$(patsubst %.c,%.o,${C_SRCS}))
CXX_OBJS=$(addprefix ${FULL_BUILD_DIR}/,$(addsuffix .o,$(basename ${CXX_SRCS})))

STATIC_OBJS=${C_OBJS} ${CXX_OBJS}
DYNAMIC_OBJS=${C_OBJS} ${CXX_OBJS}

BUILD_DIRS=$(sort ${FULL_BUILD_DIR}/ $(dir ${STATIC_OBJS} ${DYNAMIC_OBJS}))

# This makefile template is not fully "-j" safe.  Do not remove the
# following lines.
ifneq (${MAKECMDGOALS},self-objs-parallel)
.NOTPARALLEL:
endif

# ============================== ALL ==============================
LIB_STATIC_TARGET=lib${LIB}.a
FULL_LIB_STATIC_TARGET=${FULL_BUILD_DIR}/${LIB_STATIC_TARGET}
LIB_DYNAMIC_TARGET=${LIB_DYNAMIC_PREFIX}${LIB}${LIB_DYNAMIC_SUFFIX}.so${LIB_DYNAMIC_REALNAME}
FULL_LIB_DYNAMIC_TARGET=${FULL_BUILD_DIR}/${LIB_DYNAMIC_TARGET}

ifeq (${LIB_DYNAMIC},0)
    FULL_LIB_TARGETS=${FULL_LIB_STATIC_TARGET}
    LIB_TARGETS=${LIB_STATIC_TARGET}
else
    FULL_LIB_TARGETS=${FULL_LIB_DYNAMIC_TARGET}
    LIB_TARGETS=${LIB_DYNAMIC_TARGET}
endif

LIB_PRE_BUILD_TARGET?=
LIB_POST_BUILD_TARGET?=


ifeq (${LIB_LIBDEPS},1)
	LIBDEPS=libdeps
else
	# There is no need for libs depending on libs unless this lib is static
	ifeq (${LIB_DYNAMIC},0)
            LIBDEPS=
	else
            LIBDEPS=libdeps
	endif

endif

ifneq ($(strip ${LIBDEPS}),)
    LIBDEP_LIST=${DEP_LIBS} ${DEP_ILIBS} ${DEP_CUSTLIBS} ${DEP_THIRDPARTY_LIBS} ${DEP_CUSTILIBS}
else
    LIBDEP_LIST=
endif

ifneq ($(strip ${JNIDEPS}),)
    JNIDEP_LIST=${JNI_HEADERS}
else
    JNIDEP_LIST=
endif

ALL_DEP_PREREQS=${LIBDEP_LIST}
ALL_DEP_ORDER_PREREQS=${JNIDEP_LIST}

.PHONY: lib_${LIB} jnideps

all: startup ensure_build_dirs ${LIB_PRE_BUILD_TARGET} ${JNIDEPS} all-recursive ${LIBDEPS} self-objs lib_${LIB} ${LIB_POST_BUILD_TARGET} shutdown

lib_${LIB}: ${FULL_LIB_TARGETS}

self-objs:
	$(standard-self-objs)

ifeq (${LIB_DYNAMIC},0)
self-objs-parallel: ${STATIC_OBJS}

else
self-objs-parallel: ${DYNAMIC_OBJS}

endif

# XXX This only works for c/c++ files being linked into a library right now
${FULL_LIB_STATIC_TARGET}: ${ALL_DEP_PREREQS} ${STATIC_OBJS} ${ARADD} | ${ALL_DEP_ORDER_PREREQS}
	@rm -f ${FULL_LIB_STATIC_TARGET}
	@cmd1="${AR} -cq $@ $^" ; \
	cmd2="${RANLIB} $@" ; \
	if test ! "${PROD_BUILD_VERBOSE}" -eq 1; then \
		echo "Linking ${FULL_LIB_STATIC_TARGET} for ${PROD_TARGET_PLATFORM_FULL_LC} ${PROD_TARGET_CPU_LC}-${PROD_TARGET_CPU_WORDSIZE}"; \
		$${cmd1} || exit 1; \
		$${cmd2} || exit 1; \
	else \
		echo "$${cmd1}"; \
		$${cmd1} || exit 1; \
		echo "$${cmd2}"; \
		$${cmd2} || exit 1; \
	fi
	@${ECHO} "Built static library lib${LIB}"

${FULL_LIB_DYNAMIC_TARGET}: ${ALL_DEP_PREREQS} ${DYNAMIC_OBJS} ${ARADD} | ${ALL_DEP_ORDER_PREREQS}
	@rm -f ${FULL_LIB_DYNAMIC_TARGET}
	@if [ ! -z "${LIB_LINKER}" ]; then \
		lib_linker="${LIB_LINKER}" ; \
	elif test -z "${CXX_OBJS}"; then \
		lib_linker="${CC}" ; \
	else \
		lib_linker="${CXX}" ; \
	fi ; \
	if [ ! -z "${PURIFY}" -a "${PURIFY}" = "1" -a ! -z "${PURIFY_LINK}" ]; then \
		lib_linker="${PURIFY_LINK} $${lib_linker}" ; \
	fi ; \
	cmd="$${lib_linker} -shared ${LIB_DYNAMIC_SONAME_ARG} ${ALL_LDFLAGS} \
		-o $@ $^ ${ALL_LDADD} ${ALL_LDADD_LIB}" ; \
	if test ! "${PROD_BUILD_VERBOSE}" -eq 1; then \
		echo "Linking ${FULL_LIB_DYNAMIC_TARGET} for ${PROD_TARGET_PLATFORM_FULL_LC} ${PROD_TARGET_CPU_LC}-${PROD_TARGET_CPU_WORDSIZE}"; \
		$${cmd} || exit 1; \
	else \
		echo "$${cmd}"; \
		$${cmd} || exit 1; \
	fi
	@${ECHO} "Built dynamic library ${LIB_DYNAMIC_PREFIX}${LIB}"

CLEANFILES+=${FULL_LIB_STATIC_TARGET} ${FULL_LIB_DYNAMIC_TARGET} ${STATIC_OBJS} ${DYNAMIC_OBJS}

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

ifneq ($(strip ${JNI_HEADERS}),)

${JNI_HEADERS}: jnideps

jnideps:
	@echo "Starting JNI deps for ${JNI_CLASS_ROOTS}" ; \
        failcom='exit 1'; \
        for f in x $$MAKEFLAGS; do \
          case $$f in \
            *=* | --[!k]*);; \
            *k*) failcom='fail=yes';; \
          esac; \
        done; \
	for subdir in ${JNI_CLASS_ROOTS}; do \
		(cd $$subdir && ${MAKE} all) || eval $$failcom; \
	done; \
	test -z "$$fail"

else
jnideps:
	echo "No JNI deps"

endif

# ============================== INSTALL ==============================
# Note that libraries are only potentially stripped if installed
LIB_STRIP?=0
ifneq (${PROD_HOST_OS},SUNOS)
LIB_STRIP_FLAGS?=--strip-debug
else
LIB_STRIP_FLAGS?=-x
endif
LIB_INSTALL_FLAGS?=

LIB_PERM_OWNER=root
LIB_PERM_GROUP=bin
LIB_PERM_MODE=755

# The next two conditionals are for backward compatability only
ifeq ($(strip ${LIB_STRIP}),-s)
LIB_STRIP=1
endif
ifeq ($(strip ${LIB_STRIP}),--strip)
LIB_STRIP=1
endif

ifeq (${LIB_STRIP},1)
DO_LIB_STRIP?=${STRIP} ${LIB_STRIP_FLAGS} ${FULL_INSTALL_DIR}/${LIB_TARGETS}
else
DO_LIB_STRIP=:
endif

ifndef INSTALL_FAKE
LIB_INSTALL?=${INSTALL} -o ${LIB_PERM_OWNER} \
		-g ${LIB_PERM_GROUP} -m ${LIB_PERM_MODE} \
		${LIB_INSTALL_FLAGS} ${FULL_LIB_TARGETS} \
		${FULL_INSTALL_DIR}
else
LIB_INSTALL?=${INSTALL} -m ${LIB_PERM_MODE} \
		${LIB_INSTALL_FLAGS} ${FULL_LIB_TARGETS} \
		${FULL_INSTALL_DIR}
endif


install: ensure_install_dirs install-recursive lib_${LIB}
	@if test ! "${NO_INSTALL}" -eq 1; then \
		${LIB_INSTALL}; \
		${DO_LIB_STRIP}; \
	fi

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
