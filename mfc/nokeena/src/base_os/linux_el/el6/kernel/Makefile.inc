#  Makefile

# This makes the kernel compile be very verbose
EXTRA_KERNEL_MAKE_FLAGS="V=1"

# Kernel Config File Override
# USE_KERNEL_REV=2.6.32_504.16.2.SCL6_6
# USE_KERNEL_REV=2.6.32_358.2.1.el6

#
#KERNEL_CONFIG_ROOT=${PROD_CUSTOMER_ROOT}/${PROD_PRODUCT_LC}/src/base_os/${PROD_TARGET_PLATFORM_LC}/${PROD_TARGET_PLATFORM_VERSION_LC}/kernel/arch_${PROD_TARGET_ARCH_LC}
ifeq (${USE_KERNEL_REV},2.6.32_358.2.1.el6)
  KERNEL_CONFIG_ROOT=${PROD_CUSTOMER_ROOT}/${PROD_PRODUCT_LC}/src/base_os/${PROD_TARGET_PLATFORM_LC}/${PROD_TARGET_PLATFORM_VERSION_LC}/kernel_2.6.32_358.2.1.el6/conf_arch_${PROD_TARGET_ARCH_LC}
else
  KERNEL_CONFIG_ROOT=${PROD_CUSTOMER_ROOT}/${PROD_PRODUCT_LC}/src/base_os/${PROD_TARGET_PLATFORM_LC}/${PROD_TARGET_PLATFORM_VERSION_LC}/kernel_2.6.32_504.16.2.SCL6_6/conf_arch_${PROD_TARGET_ARCH_LC}
endif

# Add our patch file to the CUSTOMER_PATCHES list.

# This is included by PROD_TREE_ROOT/src/base/linux_el/el6/kernel/Makefile
# This makefile does the following:
#   PATCH_ARGS=-p2 -f
#   @for patch in ${BASE_PATCHES} ${CUSTOMER_PATCHES}; do \
#     echo "Patching sources with `basename $${patch}`"; \
#     ${PATCH} ${PATCH_ARGS} -N -s -d ${SYMTREE_IMAGE_KERNEL_SMP} -i $${patch} || exit 1; \
#   done

CUST_PATCH_SRC_DIR="${PROD_CUSTOMER_ROOT}/${PROD_PRODUCT_LC}/src/base_os/${PROD_TARGET_PLATFORM_LC}/${PROD_TARGET_PLATFORM_VERSION_LC}/kernel/patches"

#EPOLLCTL_LOCK_PATCH_C6=\
#"${CUST_PATCH_SRC_DIR}/epollctl/epollctl_lock.patch" 

INET_PATCH_C6=\
"${CUST_PATCH_SRC_DIR}/inet/inet_sock_get_indev_c6.patch" 

TP4_PATCH_C6=\
"${CUST_PATCH_SRC_DIR}/tp4/1035-linux-2.6.32-l3_fwd.patch" 

#REUSE_PORT_PATCH_C6=\
#"${CUST_PATCH_SRC_DIR}/tp4/resue_port.patch" 

SCHED_PATCH_C6=\
"${CUST_PATCH_SRC_DIR}/sched/stat.patch" 

#EHCI_USB_PATCH_C6=\
#"${CUST_PATCH_SRC_DIR}/ehciusb/ehci_usb.patch" \

I2C_PATCH_C6=\
"${CUST_PATCH_SRC_DIR}/i2c/i2c.patch" \


#CUSTOMER_PATCHES:= ${EPOLLCTL_LOCK_PATCH_C6} ${INET_PATCH_C6} ${TP4_PATCH_C6} ${REUSE_PORT_PATCH_C6} ${SCHED_PATCH_C6} ${EHCI_USB_PATCH_C6} ${I2C_PATCH_C6}

CUSTOMER_PATCHES:= ${INET_PATCH_C6} ${TP4_PATCH_C6} ${SCHED_PATCH_C6} ${I2C_PATCH_C6}
