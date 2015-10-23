# Makefile

include ${PROD_TREE_ROOT}/src/mk/common.mk

SUBDIRS=              \
	base_os       \
	include       \
	lib           \
	thirdparty    \
	bin           \
	install_files \
	config        \
	manufacture   \
	mfl           \
	external      \

include ${PROD_TREE_ROOT}/src/mk/subdir.mk

release:
	$(MAKE) -C thirdparty release
	$(MAKE) -C manufacture release
	$(MAKE) -C mfl release
	$(MAKE) -C external release
	$(MAKE) -C release release
