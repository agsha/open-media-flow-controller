#  Makefile - Ankeena

include ${PROD_TREE_ROOT}/src/mk/common.mk

SUBDIRS=        	\
	libketama  	\

INSTALL_DIRECTIVES= \
	 ${SYS_CREATE_SYMLINK_libketama} \

CLEANFILES= \
	${SYS_CREATE_SYMLINK_libketama}


SYS_CREATE_SYMLINK_libketama="symlink /lib/nkn /lib/nkn/libketama.so.1.0.0 libketama.so.1"

include ${PROD_TREE_ROOT}/src/mk/files_adv.mk

