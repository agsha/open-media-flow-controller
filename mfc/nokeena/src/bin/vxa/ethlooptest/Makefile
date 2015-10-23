include ${PROD_TREE_ROOT}/src/mk/common.mk
# This sets the following
# export PROD_OUTPUT_DIR=${PROD_OUTPUT_ROOT}/product-${PROD_PRODUCT_LC}-${PROD_TARGET_ID_NAME}
# export PROD_BUILD_ROOT=${PROD_OUTPUT_DIR}/build
# export PROD_BUILD_OUTPUT_DIR=${PROD_BUILD_ROOT}
# export PROD_INSTALL_ROOT=${PROD_OUTPUT_DIR}/install
# export PROD_INSTALL_OUTPUT_DIR=${PROD_INSTALL_ROOT}
# export PROD_RELEASE_ROOT=${PROD_OUTPUT_DIR}/release
# export PROD_RELEASE_OUTPUT_DIR=${PROD_RELEASE_ROOT}

BD=${PROD_BUILD_OUTPUT_DIR}/image/bin32/ethlooptest
ID=${PROD_INSTALL_OUTPUT_DIR}/image/opt/nkn/bin

CC=gcc -g

CFLAGS= -g -m32 -static -std=c99 -Wall -W -Wshadow -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wredundant-decls -Wpointer-arith -Wnested-externs -Wredundant-decls -Wundef -Wreturn-type -Wwrite-strings -Wbad-function-cast -Wno-unused -Wformat-security -Wmissing-format-attribute -Winline -Werror

LDFLAGS= -m32 -static

LIBS = -lpthread


#
# gcc -g -m32 -static -std=c99 -Wall -W -Wshadow -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wredundant-decls -Wpointer-arith -Wnested-externs -Wredundant-decls -Wundef -Wreturn-type -Wwrite-strings -Wbad-function-cast -Wno-unused  -Wformat-security -Wmissing-format-attribute -Winline  -Werror   -c -o ethlooptest.o ethlooptest.c
#
# gcc -g -m32 -static -std=c99 -Wall -W -Wshadow -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wredundant-decls -Wpointer-arith -Wnested-externs -Wredundant-decls -Wundef -Wreturn-type -Wwrite-strings -Wbad-function-cast -Wno-unused  -Wformat-security -Wmissing-format-attribute -Winline  -Werror ethlooptest.o -lpthread -o ethlooptest
#
compile: $(BD)/ethlooptest

install: $(ID)/ethlooptest

all: install

$(BD)/ethlooptest: $(BD)/ethlooptest.o
	@echo Link ethlooptest
	$(CC) $(LDFLAGS) $(BD)/ethlooptest.o $(LIBS) -o $(BD)/ethlooptest

$(BD)/ethlooptest.o: $(BD) ethlooptest.c
	@echo Compile ethlooptest
	$(CC) $(CFLAGS) ethlooptest.c -c -o $(BD)/ethlooptest.o

$(ID)/ethlooptest: $(ID) $(BD)/ethlooptest
	@echo Install ethlooptest
	cp $(BD)/ethlooptest $(ID)/ethlooptest

release:
	Nothing to do for release

clean:
	rm -f $(ID)/ethlooptest
	rm -f $(BD)/ethlooptest
	rm -f $(BD)/ethlooptest.o

$(ID):
	@echo mkdir -p $(ID)
	mkdir -p $(ID)

$(BD):
	@echo mkdir -p $(BD)
	mkdir -p $(BD)


print:
	@echo PROD_BUILD_OUTPUT_DIR=$(PROD_BUILD_OUTPUT_DIR)
	@echo PROD_INSTALL_OUTPUT_DIR=$(PROD_INSTALL_OUTPUT_DIR)
	@echo BD=$(BD)
	@ls -l $(BD)
	@echo ID=$(ID)
	@ls -l $(ID)

