DPI_TARGET ?= $(notdir $(realpath .))
SRC ?= $(wildcard *.c)

PREREQUISITES = $(DPI_SDK_ROOT)/examples/lib/libflow/flow_config.h

#
# Common for all OS-es
#

deps install-deps clean-deps:
	$(foreach DIR, $(filter-out libhwa,$(DEPS)), \
		$(MAKE) -C $(DPI_SDK_ROOT)/examples/lib/$(DIR) $(filter-out deps,$(patsubst %-deps,%,$@)); )
ifeq ($(filter libhwa,$(DEPS)),libhwa)
	$(MAKE) -C $(DPI_SDK_ROOT)/examples/platform/libhwa/$(LIBHWA) $(filter-out deps,$(patsubst %-deps,%,$@))
endif

clean:: clean-deps
	rm -f $(DPI_TARGET) $(OBJS)


.PHONY: deps all clean distclean

