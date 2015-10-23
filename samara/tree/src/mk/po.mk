#
#
#
#

# Define related variables, if needed
#export REQ_GETTEXT_VERSION=0.14.1-12

# Set some variables if the Makefile that called us didn't
SRCS_BASE?=$(shell ${MAKE} -s -r -R --no-print-directory -C ./ print_make_var-SRCS | awk '/SRCS/' | sed 's/^SRCS=//')
EXCLUDE_SRCS?=
EXTRA_SRCS?=
GETTEXT_PACKAGE?=$(shell ${MAKE} -s -r -R --no-print-directory -C ./ print_make_var-GETTEXT_PACKAGE | awk '/GETTEXT_PACKAGE/' | sed 's/^GETTEXT_PACKAGE=//')
PO_OUTPUT_DIR?=.

ifneq ("$(strip ${EXCLUDE_SRCS})","")
	SRCS=${EXTRA_SRCS} $(filter-out ${EXCLUDE_SRCS}, ${SRCS_BASE})
else
	SRCS=${EXTRA_SRCS} ${SRCS_BASE}
endif

GETTEXT_KEYWORDS=--keyword=_ --keyword=N_ --keyword=D_
ifneq (${PROD_FEATURE_I18N_EXTRAS}, 0)
	GETTEXT_KEYWORDS+=--keyword=I_
endif

CATALOGS_BUILD_DIR=${MO_OUTPUT_DIR}

INSTALL_TREE?=image
PROD_INSTALL_PREFIX=
PROG_INSTALL_DIR=/usr/share/locale
FULL_BUILD_DIR=${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CATALOGS_BUILD_DIR}
FULL_INSTALL_DIR=${PROD_INSTALL_OUTPUT_DIR}/${INSTALL_TREE}/${PROD_INSTALL_PREFIX}/${PROG_INSTALL_DIR}

# Set some internal source settings based on SRCS
C_SRCS=$(filter %.c %.h,${SRCS})

# Set of all the PO files in the directory
PO_NAMES=$(addsuffix .${GETTEXT_PACKAGE}.po,${PROD_LOCALES})
PO_FILES=$(addprefix ${PO_OUTPUT_DIR}/,${PO_NAMES})

# Set of all the MO files in the directory
MO_FILES=$(addsuffix .${GETTEXT_PACKAGE}.mo,${PROD_LOCALES})

CATALOGS=$(addprefix ${FULL_BUILD_DIR}/,${MO_FILES})
BUILD_DIRS=$(sort ${FULL_BUILD_DIR}/ $(dir ${CATALOGS}))

INSTALL_DIR_CATALOGS=$(addprefix ${FULL_INSTALL_DIR}/,${MO_FILES})
DIRS_TMP=$(basename $(basename ${INSTALL_DIR_CATALOGS}))
INSTALL_DIRS=$(sort $(addsuffix /LC_MESSAGES/,${DIRS_TMP}))
INSTALL_MO_FILES=$(addsuffix ${GETTEXT_PACKAGE}.mo,${INSTALL_DIRS})

TEMPLATE_PO_TARGET=${PO_OUTPUT_DIR}/${GETTEXT_PACKAGE}.pot

.PHONY: catalogs update

# This makefile template is not "-j" safe.  Do not remove the following line
.NOTPARALLEL:

# ============================== ALL ==============================
all: startup ensure_build_dirs ensure_language_translations catalogs shutdown

catalogs: ${CATALOGS}
	@echo "${GETTEXT_PACKAGE} catalogs completed"

# Compile .po files to .mo (aka generating catalogs)
${FULL_BUILD_DIR}/%.mo: ${PO_OUTPUT_DIR}/%.po
	@cmd="${MSGFMT} $< -o $@" ; \
	if test ! "${PROD_BUILD_VERBOSE}" -eq 1; then \
		echo -e "Compiling catalog file: $@" ;\
	else \
		echo "$${cmd}"; \
	fi; \
	$${cmd} || exit 1;

# XXX/EMT: the invocation of awk below should respect the VERBOSE flag.
# I couldn't figure out the right escaping to accomplish this...

${TEMPLATE_PO_TARGET}: ${C_SRCS}
	@if test ! -f ${TEMPLATE_PO_TARGET}; then \
		action="Creating" ; \
	else \
		action="Updating" ; \
	fi; \
	if test ! -d ${PO_OUTPUT_DIR}; then \
		mkdir -p ${PO_OUTPUT_DIR} ; \
	fi; \
	cmd="${XGETTEXT} --output=${TEMPLATE_PO_TARGET}.tmp \
		--directory=./ ${GETTEXT_KEYWORDS} \
		--force-po ${C_SRCS}" ; \
	if test ! "${PROD_BUILD_VERBOSE}" -eq 1; then \
		echo "$${action} template translation file ${TEMPLATE_PO_TARGET}"; \
	else \
		echo "$${cmd}"; \
	fi; \
	$${cmd} || exit 1; \
	awk '! /^"Content-Type:.* charset=CHARSET/ { print }; \
	/^"Content-Type:.* charset=CHARSET/ { sub(/charset=CHARSET/,"charset=UTF-8",$$0); print }' \
	${TEMPLATE_PO_TARGET}.tmp > ${TEMPLATE_PO_TARGET}; \
	${RM} ${TEMPLATE_PO_TARGET}.tmp; \

i18n-update-catalogs: startup ensure_build_dirs ensure_language_translations update shutdown

update: ${TEMPLATE_PO_TARGET}
	@all_po_files='${PO_FILES}'; \
	for po_file in $$all_po_files; do \
		if test ! -d ${PO_OUTPUT_DIR}; then \
			mkdir -p ${PO_OUTPUT_DIR} ; \
		fi; \
		cmd="${MSGMERGE} --quiet --update $$po_file ${TEMPLATE_PO_TARGET}" ; \
		if test ! "${PROD_BUILD_VERBOSE}" -eq 1; then \
			echo -e "Updating $$po_file translation file"; \
		else \
			echo "$${cmd}"; \
		fi; \
		$${cmd} || exit 1; \
	done;

# XXX/EMT: I18N: msginit thinks it's so smart, it adds a localized
# line to most of its non-English files, which reads "Translations to
# <language> for the package PACKAGE" in whatever language you told it
# (derived from the -locale option).  Sometimes this includes special
# characters which are encoded somehow other than UTF-8.  Sometimes it
# is ISO-8859-1, as in the Spanish version with an 0xF1 (n with a
# tilde), although the Japanese version has some characters which are
# clearly something else.  So some of them are invalid UTF-8, which is
# not too useful since we intend for the file to contain UTF-8
# messages.  There are no command line parameters that can prevent it
# from doing this.  Setting your current locale to a UTF-8 locale
# (e.g. en_US.utf8) does not help.  Nor does having the .pot file
# you feed it as input call out UTF-8 as its charset.
#
# It is not trivial to automatically remove this line because it has
# no common characteristics across languages (aside from having
# "PACKAGE" in it, which matches several other lines as well), and
# although it is always on the 2nd line of the file, sometimes
# (e.g. for hi_IN) the line is omitted entirely.  For now the 
# developer will just have to remove or fix it manually.

# Note that we do not echo "Creating $$po_file" below in the
# non-verbose case because msginit prints its own message of this
# nature to stderr, so we cannot silence it without also silencing
# legitimate errors.

i18n-create-catalogs: ${TEMPLATE_PO_TARGET}
	@all_locales='${PROD_LOCALES}'; \
	for locale in $$all_locales; do \
		po_file=`echo ${PO_OUTPUT_DIR}/$$locale.${GETTEXT_PACKAGE}.po`; \
		if test ! -d ${PO_OUTPUT_DIR}; then \
			mkdir -p ${PO_OUTPUT_DIR} ; \
		fi; \
		if test ! -f $$po_file; then \
			cmd="${MSGINIT} --input=${TEMPLATE_PO_TARGET} --output-file=$$po_file --locale=$$locale --no-translator" ; \
			if test "${PROD_BUILD_VERBOSE}" -eq 1; then \
				echo "$${cmd}"; \
			fi; \
			$${cmd} || exit 1; \
		fi; \
	done;

ensure_language_translations:
	@for po_file in ${PO_FILES}; do \
		if test ! -f $${po_file}; then \
			 ${ECHO} "Supported language translation file $${po_file} does not exist"; \
			 ${ECHO} "Run 'make i18n-create-catalogs' to create language translation files"; \
			 exit 1; \
	    	fi; \
	done;

CLEANFILES+=${CATALOGS}

# Check environment for validity of xgettext also
startup:
	@if test -z "${GETTEXT_PACKAGE}"; then \
		echo "Variable GETTEXT_PACKAGE not defined"; \
		exit 1; \
	fi; \

	@if test -z "${MO_OUTPUT_DIR}"; then \
		echo "Variable MO_OUTPUT_DIR not defined"; \
		exit 1; \
	fi; \

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
PROG_STRIP?=0
PROG_INSTALL_FLAGS?=

PROG_PERM_OWNER?=root
PROG_PERM_GROUP?=bin
PROG_PERM_MODE?=755

#ifndef INSTALL_FAKE
#DO_PROG_INSTALL?=${INSTALL} -o ${PROG_PERM_OWNER} \
#		-g ${PROG_PERM_GROUP} -m ${PROG_PERM_MODE} \
#		${PROG_INSTALL_FLAGS} ${dir} \
#		${FULL_INSTALL_DIR}
#else
#DO_PROG_INSTALL?=${INSTALL} -m ${PROG_PERM_MODE} \
#		${PROG_INSTALL_FLAGS} ${dir} \
#		${FULL_INSTALL_DIR}
#endif

ifndef INSTALL_FAKE
DO_PROG_INSTALL?=${INSTALL} -o ${PROG_PERM_OWNER} \
		-g ${PROG_PERM_GROUP} -m ${PROG_PERM_MODE} \
		${PROG_INSTALL_FLAGS} 
else
DO_PROG_INSTALL?=${INSTALL} -m ${PROG_PERM_MODE} \
		${PROG_INSTALL_FLAGS} 
endif

DO_PROG_POST_INSTALL?=

install: ensure_install_dirs install-recursive ${CATALOGS}
	@all_dirs='${CATALOGS}'; \
	for dir in $$all_dirs; do \
		lang_locale=`echo $$dir | sed 's/^.*\/\(.*\)\..*\.mo$$/\1/'`; \
		echo "Installing $$lang_locale"; \
		echo "${DO_PROG_INSTALL} $$dir ${FULL_INSTALL_DIR}/$$lang_locale/LC_MESSAGES/${GETTEXT_PACKAGE}.mo"; \
		${DO_PROG_INSTALL} $$dir ${FULL_INSTALL_DIR}/$$lang_locale/LC_MESSAGES/${GETTEXT_PACKAGE}.mo; \
	done;

# XXX make all the install directories we need
ensure_install_dirs:
	@all_dirs='${INSTALL_DIRS}'; \
	for dir in $$all_dirs; do \
		if test ! -d $$dir/; then \
			mkdir -m 755 -p $$dir; \
			if test ! -d $$dir/; then \
				${ECHO} "Unable to create $$dir."; \
				exit 1; \
			fi; \
			${ECHO} "Created directory $$dir"; \
		fi; \
	done;

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
