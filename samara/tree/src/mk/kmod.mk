#
# 
#
#

-include ${PROD_TREE_ROOT}/src/mk/common.mk

# This makefile template is not "-j" safe.  Do not remove the following line
.NOTPARALLEL:

ifeq (${PROD_TARGET_ARCH_LC},ppc)
    export ARCH=powerpc
else
    export ARCH=${PROD_TARGET_ARCH_LC}
endif

ifneq ($(strip ${CROSS_PATH_PREADD}),)
    export PATH:=${CROSS_PATH_PREADD}:${PATH}
endif

ifneq ($(strip ${CROSS_DEPMOD}),)
    export DEPMOD:=${CROSS_DEPMOD}
endif

##export KBUILD_VERBOSE=1

ifeq ($(strip ${KMOD_KERNEL_TARGETS}),)
KMOD_KERNEL_TARGETS= \

ifneq (${PROD_FEATURE_KERNEL_IMAGE_UNI},0)
KMOD_KERNEL_TARGETS+= \
	image_kernel_uni \

endif

ifneq (${PROD_FEATURE_KERNEL_IMAGE_SMP},0)
KMOD_KERNEL_TARGETS+= \
	image_kernel_smp \

endif

ifneq (${PROD_FEATURE_KERNEL_BOOTFLOPPY_UNI},0)
KMOD_KERNEL_TARGETS+= \
	bootfloppy_kernel_uni \

endif

endif # !KMOD_KERNEL_TARGETS

KMOD_KERNEL_TREE_ROOT?= \
	${PROD_TREE_ROOT}/src/base_os/${PROD_TARGET_PLATFORM_LC}/${PROD_TARGET_PLATFORM_VERSION_LC}/kernel/

KMOD_KERNEL_OUTPUT_ROOT?= \
	${PROD_BUILD_OUTPUT_DIR}/base_os/${PROD_TARGET_PLATFORM_LC}/${PROD_TARGET_PLATFORM_VERSION_LC}/kernel/

KMOD_KERNEL_OUTPUT_DIRS=$(addprefix ${KMOD_KERNEL_OUTPUT_ROOT},$(strip ${KMOD_KERNEL_TARGETS}))

KMOD_ID?=unknown
KMOD_DIR?=extra/${KMOD_ID}

KMOD_KMOD_OUTPUT_DIRS=$(addsuffix /${KMOD_DIR},${KMOD_KERNEL_OUTPUT_DIRS})


# NOTE: KMOD_BASEVERSION does not seem to be used.
KMOD_BASEVERSION=$(shell ${MAKE} -s --no-print-directory -C ${KMOD_KERNEL_TREE_ROOT} print_make_var_value-BASE_KERNEL_VERSION)
export KMOD_BASEVERSION

KMOD_EXTRAVERSION_image_kernel_uni=$(shell ${MAKE} -s --no-print-directory -C ${KMOD_KERNEL_TREE_ROOT} print_make_var_value-EXTRAVERSION_IMAGE_KERNEL_UNI)
export KMOD_EXTRAVERSION_image_kernel_uni
KMOD_EXTRAVERSION_image_kernel_smp=$(shell ${MAKE} -s --no-print-directory -C ${KMOD_KERNEL_TREE_ROOT} print_make_var_value-EXTRAVERSION_IMAGE_KERNEL_SMP)
export KMOD_EXTRAVERSION_image_kernel_smp
KMOD_EXTRAVERSION_bootfloppy_kernel_uni=$(shell ${MAKE} -s --no-print-directory -C ${KMOD_KERNEL_TREE_ROOT} print_make_var_value-EXTRAVERSION_BOOTFLOPPY_KERNEL_UNI)
export KMOD_EXTRAVERSION_bootfloppy_kernel_uni

# Can be set in makefile to pass more options to the 'make modules'
KMOD_EXTRA_ARGS?=

# Can be set in makefile to pass more options to the 'make modules_install'
KMOD_EXTRA_INSTALL_ARGS?=


# ============================== ALL ==============================
all: startup kmods all-recursive shutdown

startup:
	$(standard-startup)

shutdown:
	$(standard-shutdown)

# We currently only support build kernel modules on linux
ifeq (${PROD_TARGET_OS},LINUX)

kmod_symtrees:
	echo RULE kmod_symntrees in kmod.mk
	for kmsd in ${KMOD_KMOD_OUTPUT_DIRS}; do \
		if [ ! -f $${kmsd}/.done_links ]; then \
			echo "Making symlink tree for kmod ${KMOD_ID} in $${kmsd} from ${PWD}"  ; \
			${PROD_TREE_ROOT}/src/release/symtree.sh ${PWD} $${kmsd} || exit 1 ; \
			touch $${kmsd}/.done_links ; \
		fi ; \
	done			 
	echo RULE Done kmod_symntrees in kmod.mk

kmod_dirs:
	echo RULE kmod_dirs in kmod.mk
	${MAKE} -C ${KMOD_KERNEL_TREE_ROOT} startup ;\
	${MAKE} -C ${KMOD_KERNEL_TREE_ROOT} ensure_build_dirs ;\
	${MAKE} -C ${KMOD_KERNEL_TREE_ROOT} extract_sources ;\

	@unset MAKEFLAGS MFLAGS ;\
	for kmd in ${KMOD_KERNEL_TARGETS}; do \
		${MAKE} -C ${KMOD_KERNEL_TREE_ROOT} config_$${kmd} ;\
	done

	for dir in ${KMOD_KMOD_OUTPUT_DIRS}; do \
		mkdir -p $${dir} ; \
	done
	echo RULE Done kmod_dirs in kmod.mk

kmods: kmod_dirs kmod_symtrees
	echo RULE kmods in kmod.mk
	unset MAKEFLAGS MFLAGS ;\
	for km in ${KMOD_KERNEL_TARGETS}; do \
		eval 'EXTRAVERSION="$${KMOD_EXTRAVERSION_'$${km}'}"' ; \
		export EXTRAVERSION ; \
		${MAKE} -C ${KMOD_KERNEL_OUTPUT_ROOT}/$${km} M=${KMOD_DIR} ${KMOD_EXTRA_ARGS} modules ; \
	done
	echo RULE Done kmods in kmod.mk


endif # !LINUX

# ============================== INSTALL ==============================
install: install-recursive
	echo RULE install in kmod.mk
	@unset MAKEFLAGS MFLAGS ;\
	for km in ${KMOD_KERNEL_TARGETS}; do \
		eval 'EXTRAVERSION="$${KMOD_EXTRAVERSION_'$${km}'}"' ; \
		export EXTRAVERSION ; \
		if [ $${km} = image_kernel_uni -o $${km} = image_kernel_smp ]; then \
			${MAKE} -C ${KMOD_KERNEL_OUTPUT_ROOT}/$${km} M=${KMOD_DIR} INSTALL_MOD_PATH=${PROD_INSTALL_OUTPUT_DIR}/image INSTALL_MOD_DIR=${KMOD_DIR} modules_install ; \
		else \
			${MAKE} -C ${KMOD_KERNEL_OUTPUT_ROOT}/$${km} M=${KMOD_DIR} INSTALL_MOD_PATH=${PROD_INSTALL_OUTPUT_DIR}/rootflop INSTALL_MOD_DIR=${KMOD_DIR} ${KMOD_EXTRA_INSTALL_ARGS} modules_install ; \
		fi ; \
	done


# ============================== CLEAN ==============================
clean: clean-recursive
	echo RULE install in kmod.mk
	echo EXTRAVERSION=${EXTRAVERSION}
	@unset MAKEFLAGS MFLAGS ;\
	for km in ${KMOD_KERNEL_TARGETS}; do \
		eval 'EXTRAVERSION="$${KMOD_EXTRAVERSION_'$${km}'}"' ; \
		export EXTRAVERSION ; \
		if test -d ${KMOD_KERNEL_OUTPUT_ROOT}/$${km} -a -d ${KMOD_KERNEL_OUTPUT_ROOT}/$${km}/${KMOD_DIR} ; then \
			${MAKE} -C ${KMOD_KERNEL_OUTPUT_ROOT}/$${km} M=${KMOD_DIR} clean ; \
		fi ; \
		rm -f ${KMOD_KERNEL_OUTPUT_ROOT}/$${km}/${KMOD_DIR}/.done_links ; \
	done

# ============================== DEPEND ==============================
depend: depend-recursive

# ============================== REALCLEAN ==============================
realclean: realclean-recursive

# Recursion
all-recursive install-recursive depend-recursive \
  clean-recursive realclean-recursive:
	$(recurse-subdirs)
	$(recurse-submakefiles)
