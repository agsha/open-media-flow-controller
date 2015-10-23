DPI_ARCH          ?=x86_64
DPI_OS            ?=linux
DPI_THREADINGMODEL?=smp

uc = $(shell echo $(1) | tr 'a-z' 'A-Z' | sed 's/X86/x86/')

CFLAGS += -DDPI_OS_$(call uc,$(DPI_OS))
CFLAGS += -DDPI_ARCH_$(call uc,$(DPI_ARCH))
CFLAGS += -DDPI_THREADINGMODEL_$(call uc,$(DPI_THREADINGMODEL))

# Extracts both parts of the DPI_ARCH variable
DPI_ARCH_family =$(shell echo $(DPI_ARCH) | cut -d_ -f1)
DPI_ARCH_bitsize=$(shell echo $(DPI_ARCH) | cut -d_ -f2)

DPI_OS_HAS_LIBDL        = 1
DPI_OS_HAS_LIBPTHREAD   = 1
DPI_OS_HAS_LIBPCAP      = 1
DPI_OS_HAS_LIBRT        = 1
DPI_HAS_FLOW_MANAGER    = 0

# Defaults
CROSS=
CC=$(CROSS)gcc
AR=$(CROSS)ar
LD=$(CROSS)ld

DPI_CAPTURE ?= pcap
CFLAGS +=-DDPI_CAPTURE_$(call uc,$(DPI_CAPTURE))

# default libhwa
LIBHWA = pcap_$(DPI_THREADINGMODEL)

ifeq ($(DPI_OS),linux)

	ifeq ($(DPI_CAPTURE),netronome)
		NETRONOME_SDK ?= /opt/netronome
		CFLAGS  +=-I$(NETRONOME_SDK)/include/
		LDFLAGS +=-L$(NETRONOME_SDK)/lib -lnfm
		DPI_HAS_FLOW_MANAGER = 1
		LIBHWA = $(DPI_CAPTURE)

	else ifeq ($(DPI_CAPTURE),napatech)
		NT_SDK ?= /opt/napatech
		CFLAGS  += -DNAPATECH_HEADER_FILES_VERSION_MAJOR=4 \
			  -DNAPATECH_HEADER_FILES_VERSION_MINOR=25 \
			  -I$(NT_SDK)/include/napatech \
			  -D_NT_OS_TYPE=_NT_OS_TYPE_LINUX \
			  -D_NT_HOST_CPU_POINTER_SIZE=_NT_HOST_CPU_POINTER_SIZE_64
		LDFLAGS += -L$(NT_SDK)/lib -lntcommoninterface

		LIBHWA = $(DPI_CAPTURE)

	else ifeq ($(DPI_CAPTURE),dpdk)
		ifeq ($(RTE_SDK),)
			$(error "Please define RTE_SDK environment variable")
		endif
		RTE_TARGET ?= x86_64-default-linuxapp-gcc
		CFLAGS  += -I$(RTE_SDK)/build/include
		LDFLAGS += -Wl,-whole-archive
		LDFLAGS += $(RTE_SDK)/build/lib/libethdev.a
		LDFLAGS += $(RTE_SDK)/build/lib/librte_malloc.a
		LDFLAGS += $(RTE_SDK)/build/lib/librte_mbuf.a
		LDFLAGS += $(RTE_SDK)/build/lib/librte_eal.a
		LDFLAGS += $(RTE_SDK)/build/lib/librte_pmd_igb.a
		LDFLAGS += $(RTE_SDK)/build/lib/librte_pmd_ixgbe.a
		LDFLAGS += $(RTE_SDK)/build/lib/librte_mempool.a
		LDFLAGS += $(RTE_SDK)/build/lib/librte_ring.a
		LDFLAGS += $(RTE_SDK)/build/lib/librte_timer.a
		LDFLAGS += -Wl,-no-whole-archive

		LIBHWA = $(DPI_CAPTURE)

	endif

	ifeq ($(DPI_ARCH),x86_32)
		CFLAGS  += -m32
		LDFLAGS += -m32

	else ifeq ($(DPI_ARCH),x86_64)

	endif

endif

GCC_MAJOR_VERSION = $(shell $(CC) -dM -E - < /dev/null | grep __GNUC__ | cut -d ' ' -f3)
GCC_MINOR_VERSION = $(shell $(CC) -dM -E - < /dev/null | grep __GNUC_MINOR__ | cut -d ' ' -f3)



ifeq ($(DEBUG),y)
CFLAGS += -g -DSDK_DBG
else
ifeq ($(DEBUG_INFO),y)
CFLAGS += -g
endif
endif

CFLAGS += $(CFLAGS_USER)

#
# Recent gcc versions (4.6+) on some distributions use « -Wl,--as-needed » by
# default ..  This leads to libqmprotocols *NOT* linked against the final
# executable.  Forcing « -Wl,--no-as-needed » is almost safe on all gcc
# versions.
#
# PLEASE NOTE : this linker flag should always be put *BEFORE*
# the -l<libraries> switches to take effect.
#
ifeq ($(GCC_MAJOR_VERSION), 4)
    LDFLAGS += -Wl,--no-as-needed
endif

ifndef DPI_SDK_ROOT
ifdef DPI
DPI_SDK_ROOT=$(DPI)
else
$(error DPI_SDK_ROOT/DPI is not set !)
endif
endif

autoconf_file = $(DPI_SDK_ROOT)/include/dpi_engine/dpi/bits/uautoconf.h
FLOW_EXT = $(shell grep "\<CFG_EXTERNAL_FLOW\>" ${autoconf_file} |cut -d' ' -f3)

CFLAGS += -Wall -Wno-unused -Wno-redundant-decls
CFLAGS += -DUSTREAM_NRCPU=48
CFLAGS += -D_BSD_SOURCE -D_GNU_SOURCE=1
CFLAGS += -I$(DPI_SDK_ROOT)/include/dpi_engine 
CFLAGS += -I$(DPI_SDK_ROOT)/include/dpi_engine/platform/include
CFLAGS += -I$(DPI_SDK_ROOT)/include/dpi_engine/platform/lib
CFLAGS += -I$(DPI_SDK_ROOT)/include/dpi_engine/platform/libhwa
