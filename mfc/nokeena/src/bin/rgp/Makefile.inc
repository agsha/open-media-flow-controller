# Makefile

# This file is included by src/bin/cli/Makefile.
# It causes the CLI shell to link with libraries that currently only DEMO's
# product needs it to link with.

LDADD+= \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/libcli/libnkncli.a

ifdef STATIC_MODULES

endif
