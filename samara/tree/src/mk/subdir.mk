#
# 
#
#

-include ${PROD_TREE_ROOT}/src/mk/common.mk

# This makefile template is not "-j" safe.  Do not remove the following line
.NOTPARALLEL:

# ============================== ALL ==============================
all: startup ${SUBDIR_PRE_BUILD_TARGET} all-recursive ${SUBDIR_POST_BUILD_TARGET} shutdown

startup:
	$(standard-startup)

shutdown:
	$(standard-shutdown)

# ============================== INSTALL ==============================
install: ${SUBDIR_PRE_INSTALL_TARGET} install-recursive ${SUBDIR_POST_INSTALL_TARGET}

# ============================== CLEAN ==============================
clean: clean-recursive

# ============================== DEPEND ==============================
depend: depend-recursive

# ============================== REALCLEAN ==============================
realclean: realclean-recursive

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
