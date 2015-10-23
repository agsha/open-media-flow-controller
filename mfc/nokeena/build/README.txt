#
# Build opensource MFC

# 1) Build machine configuration
Your build machine must be configured in a certain special way.
Instructions for setting up a build machine is in
 setup-build-machine.txt 

# 2) Source locations

The examples here have the MFC source under /home/builduser/mfc/
and the Samara tree under /home/builduser/samara/.
These can be placed anywhere, and in step 5 below set PROD_CUSTOMER_ROOT
and PROD_TREE_ROOT to match.

In the top of the MFC source are a Makefile and the directories "dist" and "nokeena".
In the top of the Samara tree is the directory "tree".

# 3) Create build directory
mkdir /home/builduser/build

# 4) Configure build settings in scripts

In file:
  /home/builduser/mfc/nokeena/src/release/junos_packaging/host-utils/scripts/mk-mfc-pkg.sh
set
  DUMMY_SIGNING=yes

# 5) Setup location environment variables
export PROD_CUSTOMER_ROOT=/home/builduser/mfc
export PROD_TREE_ROOT=/home/builduser/samara/tree
export PROD_OUTPUT_ROOT=/home/builduser/build

# 6) Setup build environment variables
export BUILD_WARNINGS_NO_EXTRA=1
export BUILD_WARNINGS_OK=1
export PROD_BUILD_VERBOSE=1

# 7) Start build
cd /home/builduser/mfc/nokeena/build
./build-mine.sh

# At this point, PXE images are located as follows:
# a) pxe_kernel.img
# /home/builduser/build/product-nokeena-x86_64/release/bootflop/vmlinuz-*
#
# b) pxe_rootfs.img
# /home/builduser/build/product-nokeena-x86_64/release/rootflop/rootflop-*
#
# c) image.img
# /home/builduser/build/product-nokeena-x86_64/release/image/image-mfc-*
#
# d) manufacture.tgz
# /home/builduser/build/product-nokeena-x86_64/release/manufacture/manufacture-*


# 6) Create MFC .iso in
# /home/builduser/build/product-nokeena-x86_64/release/mfgcd
./build-mine.sh mfgcd


MFC Reference manuals:

  https://www.juniper.net/techpubs/en_US/media-flow12.3.8/information-products/pathway-pages/media-flow/index.html


Notes:

1) MFC DHCP support is not a product feature.
   If you need this support it can be enabled via the shell.
   Procedure is as follows:

   a) Enter shell mode from the cli (cli> _shell)
   b) mkdir -p /var/lib/dhclient
   c) Create /etc/dhcp/dhclient-eth0.conf (omit [Begin] and [EOF] tokens)
[Begin]
initial-interval 2
interface "eth0" {
  send dhcp-lease-time 86400;
  send dhcp-client-identifier <Interface MAC address>;
}
[EOF]

   d) Create /etc/sysconfig/network-scripts/ifcfg-eth0 
      (omit [Begin] and [EOF] tokens)
[Begin]
DEVICE="eth0"
BOOTPROTO="dhcp"
HWADDR="<Interface MAC address>"
NM_CONTROLLED="no"
ONBOOT="yes"
TYPE="Ethernet"
[EOF]

   e) cd /etc/sysconfig/network-scripts
     ./ifup-eth ifcfg-eth0
     May need to repeat step e) several times until success is observed.

   f) cat /var/lib/dhclient/dhclient-eth0.leases to get IP address, netmask,
      default route and nameservers

   g) shell> ifconfig eth0 <IP address> netmask <mask>

   h) shell> route add default gateway <default route>

   i) Add nameservers to /etc/resolv.conf
