#
# 
#
#

INSTALL_TREE=image
JAVA_BUILD_DIR=java
JAVA_INSTALL_DIR=java
FULL_BUILD_DIR=${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${JAVA_BUILD_DIR}
FULL_INSTALL_DIR=${PROD_INSTALL_OUTPUT_DIR}/${INSTALL_TREE}/${PROD_INSTALL_PREFIX}/${JAVA_INSTALL_DIR}

JAVA_CLASS_OUTPUT_DIR=${FULL_BUILD_DIR}/classes
JAVA_SOURCE_OUTPUT_DIR=${FULL_BUILD_DIR}/sources
JAVA_JNI_DIR=${JAVA_SOURCE_OUTPUT_DIR}/jniinclude


-include ${PROD_TREE_ROOT}/src/mk/common.mk


# Set some internal source settings based on JAVA_SRCS

JAVA_CLASSES=$(addprefix ${JAVA_CLASS_OUTPUT_DIR}/,$(patsubst %.java,%.class,${JAVA_SRCS}))
JAVA_JNI_HEADERS=$(addprefix ${JAVA_JNI_DIR}/,$(patsubst %.java,%.h,$(subst /,_,${JAVA_JNI_SRCS})))
JAVA_JNI_CLASSES=$(addprefix ${JAVA_CLASS_OUTPUT_DIR}/,$(patsubst %.java,%.class,${JAVA_JNI_SRCS}))

BUILD_DIRS=$(sort ${FULL_BUILD_DIR}/ $(dir ${JAVA_CLASSES} ${JAVA_JNI_HEADERS} ${JAVA_JNI_CLASSES}))

# This makefile template is not "-j" safe.  Do not remove the following line
.NOTPARALLEL:

.PHONY: java_classes java_jni_headers

PRE_JAVAC_FLAGS=-g
ifneq (${WARNINGS_OVERRIDE},1)
    PRE_JAVAC_FLAGS+=-Xlint:all
endif
ifneq (${WARNINGS_ARE_ERRORS},0)
    PRE_JAVAC_FLAGS+=-Werror
endif

PRE_JAVAH_FLAGS=-jni
JAVAC_FLAGS?=
JAVAH_FLAGS?=
POST_JAVAC_FLAGS=
POST_JAVAH_FLAGS=

ALL_JAVAC_FLAGS=${PRE_JAVAC_FLAGS} ${JAVAC_FLAGS} ${POST_JAVAC_FLAGS}
ALL_JAVAH_FLAGS=${PRE_JAVAH_FLAGS} ${JAVAH_FLAGS} ${POST_JAVAH_FLAGS}


PRE_JAVA_SOURCEPATH=${CURDIR}
POST_JAVA_SOURCEPATH=
JAVA_SOURCEPATH_COLONS=$(subst : ,:,$(patsubst %,%:,$(filter-out :,${JAVA_SOURCEPATH})))$(filter :,${JAVA_SOURCEPATH})
JAVA_SOURCEPATH_SPACES=$(strip $(subst :, ,${JAVA_SOURCEPATH_COLONS}))
ALL_JAVA_SOURCEPATH=${PRE_JAVA_SOURCEPATH}:${JAVA_SOURCEPATH_COLONS}:${POST_JAVA_SOURCEPATH}

PRE_JAVA_CLASSPATH=${JAVA_CLASS_OUTPUT_DIR}
POST_JAVA_CLASSPATH=
ALL_JAVA_CLASSPATH=${PRE_JAVA_CLASSPATH}:${JAVA_CLASSPATH}:${POST_JAVA_CLASSPATH}


# ============================== ALL ==============================
all: startup ensure_build_dirs classdeps all-recursive java_classes java_jni_headers shutdown

java_classes: ${JAVA_CLASSES}

java_jni_headers: ${JAVA_JNI_HEADERS}

CLEANFILES+=${JAVA_CLASSES} ${JAVA_JNI_HEADERS} ${JAVA_JNI_CLASSES}

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

# XXXX Right now we are just installing every file in the class output dir!
install: ensure_install_dirs install-recursive java_classes java_jni_headers
	@echo "Installing all .class files from output dir to ${FULL_INSTALL_DIR}/classes"
	@(cd ${JAVA_CLASS_OUTPUT_DIR}; tar cpf - `find . -type f -name '*.class' -print`) | (cd ${FULL_INSTALL_DIR}/classes; tar -xf -)


# XXX make all the install directories we need
ensure_install_dirs:
	@for subdir in ${FULL_INSTALL_DIR} ${FULL_INSTALL_DIR}/classes ; do \
		if test ! -d $${subdir}/; then \
			mkdir -m 755 -p $${subdir}; \
			if test ! -d $${subdir}/; then \
				${ECHO} "Unable to create $${subdir}."; \
				 exit 1; \
			 fi; \
			 ${ECHO} "Created directory $${subdir}"; \
		 fi; \
	done

# ============================== CLEAN ==============================
clean: clean-recursive
	${RM} ${CLEANFILES}


# ============================== DEPEND ==============================
depend: depend-recursive ${ALL_DEPS}

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

# ============================== RULES =========================

# WARNING: the cmd_text below MUST be kept in sync with the actual
# command!  It is done this way only to avoid potential quoting issues.
${JAVA_CLASS_OUTPUT_DIR}/%.class: %.java
	@mkdir -p ${JAVA_CLASS_OUTPUT_DIR}
	@${RM} $@
	@cmd_text='${JAVAC} -d ${JAVA_CLASS_OUTPUT_DIR} -classpath ${ALL_JAVA_CLASSPATH} -sourcepath ${ALL_JAVA_SOURCEPATH} ${ALL_JAVAC_FLAGS} $<' ; \
	if test ! "${PROD_BUILD_VERBOSE}" -eq 1; then \
		echo "Compiling Java $<"; \
	else \
		echo "$${cmd_text}"; \
	fi ; \
	${JAVAC} -d ${JAVA_CLASS_OUTPUT_DIR} -classpath ${ALL_JAVA_CLASSPATH} -sourcepath "${ALL_JAVA_SOURCEPATH}" ${ALL_JAVAC_FLAGS} $< || jfail=1 ; \
	if [ "$${jfail}" = "1" ]; then \
		${RM} $@ ; \
		exit 1 ; \
	fi

${JAVA_JNI_HEADERS}: ${JAVA_JNI_SRCS}
	@mkdir -p ${JAVA_JNI_DIR} $(@D)
	@CNL=$$(echo $^ | tr ' ' '\n' | sed 's/^\(.*\).java$$/\1/' | sed 's/[\/]/./g' | tr '\n' ' ') ;\
	CNLP=$$(echo $${CNL} | sed 's/ /\n\t/g') ;\
	echo "Making Java JNI headers for: $${CNLP}" ;\
	${JAVAH} -force -d ${JAVA_JNI_DIR} -classpath ${JAVA_CLASS_OUTPUT_DIR} ${ALL_JAVAH_FLAGS} $${CNL}

classdeps:
	@if [ ! -z "${JAVA_SOURCEPATH_SPACES}" ]; then \
		echo "Starting class deps for ${JAVA_SOURCEPATH_SPACES}" ; \
	fi; \
        failcom='exit 1'; \
        for f in x $$MAKEFLAGS; do \
          case $$f in \
            *=* | --[!k]*);; \
            *k*) failcom='fail=yes';; \
          esac; \
        done; \
	for subdir in ${JAVA_SOURCEPATH_SPACES}; do \
		(cd $$subdir && ${MAKE} all) || eval $$failcom; \
	done; \
	test -z "$$fail"
