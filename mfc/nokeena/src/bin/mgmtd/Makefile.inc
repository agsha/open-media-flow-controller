# This file is included by $PROD_TREE_ROOT/src/bin/mgmtd/Makefile.
# It causes mgmtd to link with libraries that currently only this product needs it to link with.

# PR 942054 -Removed "-lcrypt ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libtcrypt/libtcrypt.a"
# PR 873333 -Added "LDFLAGS += -lcprops" (trunk rev 29022)
# PR 795336 -Added "${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/libjmdb/libjmdb.a" (trunk rev 27725)
# Bugs 9479,9503 -Added "-lcrypt ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libtcrypt/libtcrypt.a" (trunk rev 20499)
# Bugs 8783,8796 -Added "LDADD += ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/libmgmtlog/libmgmtlog.so.1.0.0"  (trunk rev 18494)

LDFLAGS += -lcprops

LDADD += ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/libmgmtlog/libmgmtlog.so.1.0.0 \
	 ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/libjmdb/libjmdb.a	\

