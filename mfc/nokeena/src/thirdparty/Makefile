# Makefile

include ${PROD_TREE_ROOT}/src/mk/common.mk

SUBDIRS=        \
	bin     \
	binary  \
	install-from-buildmachine \
	lib  	\

include ${PROD_TREE_ROOT}/src/mk/subdir.mk

release:
	$(MAKE) -C bin release
