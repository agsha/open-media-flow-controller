This directory is "tree/src/base_os/linux_el/el6/arch_x86_64/".

We do not keep the standard binary rpms in the samara source tree as with
previous versions of Samara.

The loction of the CentOS/SCL binary and source rpms are hardwired in the two Makefiles:
image_packages/Makefile 
  DIST_SCL_6=/volume/ssd-linux-storage01/repo/ssdlinux/scl6/release/6.6.R1.0
rootflop_packages/Makefile
  DIST_SCL_6=/volume/ssd-linux-storage01/repo/ssdlinux/scl6/release/6.6.R1.0

The location is also in the tool that updates these Makefiles when a new
update or version needs to be integrated:
update-rpms/FIND_RPM_UPDATES.sh:
  DIST_SCL_6=/volume/ssd-linux-storage01/repo/ssdlinux/scl6/release/6.6.R1.0

It is also hardwired in the customer tree in
nokeena/src/manufacture/Makefile

Note that the ssdlinux dir is NFS mounted this way in /etc/fstab:
svl-eng010-cf2.jnpr.net:/vol/ssd_linux_storage01/ssd-linux-storage01/repo/ssdlinux /volume/ssd-linux-storage01/repo/ssdlinux  nfs  ro,soft,bg  0 0

When updating the CentOS/SCL package versions used you also need to update:

  packages/pkg_bin_check.txt     -- Add the lfiv1 checksum line.
                                    Command to generate the line: lfi -tgunm <filename>
  packages/source_pkgs.inc       -- List the name and location of the src.rpm file.
  build_required.inc             -- Add the rpm package name if the rpm is required to be installed on the build machine.
  rootflop_packages/Makefile     (If the rpm is to go into the install-time image)
  image_packages/Makefile        (If the rpm is to go into the run-time product)
  image_packages/pkg_config.pl   (If the default excludes exclude too much from the rpm, or more files need to be excluded)

E.g.
packages/pkg_bin_check.txt:lfiv1 010 - 1 - - - 585552 - - - md5 5082c8b38aea1166a20293513dec5c80 ./e2fsprogs-1.41.12-21.SCLC6_6.R1.0.1.x86_64.rpm
packages/source_pkgs.inc:	${DIST_SCL_6_SRPMS}/e2fsprogs-1.41.12-21.SCLC6_6.R1.0.1.src.rpm \
build_required.inc:		e2fsprogs-1.41.12-21.SCLC6_6.R1.0.1.x86_64 \
rootflop_packages/Makefile:	${DIST_SCL_6_X86_64}/e2fsprogs-1.41.12-21.SCLC6_6.R1.0.1.x86_64.rpm \
image_packages/Makefile		${DIST_SCL_6_X86_64}/e2fsprogs-1.41.12-21.SCLC6_6.R1.0.1.x86_64.rpm \


To generate the lfiv1 line, in the directory where the rpm file is, use this
command line:
  lfi -tgunm ./<rpmfilename>
e.g.
  lfi -tgunm ./i2c-tools-3.1.0-1.el6.x86_64.rpm 
prints out
lfiv1 010 - - - - - 68168 - - - md5 d95e4fc9c47227f987921df9f211e518 ./i2c-tools-3.1.0-1.el6.x86_64.rpm
