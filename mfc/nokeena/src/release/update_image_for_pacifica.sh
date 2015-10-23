#!/bin/sh
#
#	File : update_image_for_pacifica.sh
#
#	Purpose : This script takes a regularly built MFC image and using it, 
#	builds the mx-mfc.img that is required for Pacifica that has the root 
#	file system, elilo.efi and other files needed to boot MXC to MFC. 
#
#	Created by : Rmananad Narayanan (ramanandn@juniper.net)
#	Created on : 25th March, 2012
#	

# MACROS used
MX_MFC_IMG=mx-mfc.img
MFCROOTFS_IMG=mfcrootfs.img
MD5SUMS=md5sums
TPKG_MANIFEST=tpkg-manifest

#
#
#
usage ()
{
	echo "Usage : $0 [-d <temp dir>] <MFC image file name>"
	echo 
	exit 1;
} # end of usage ()

#
#
#
cleanup ()
{
  cd ${TMP_DIR}
  unmount_mfcroot

# Remove all the files and directories that we created in this script
	rm *.tbz 2> /dev/null;
	rm *.tar 2> /dev/null;
	rm ${MX_MFC_IMG} 2> /dev/null;
	rm ${MD5SUMS} 2> /dev/null;
	rm ${TPKG_MANIFEST} 2> /dev/null;
	rm lfi 2> /dev/null;
	rm -rf LiveOS 2> /dev/null;
	rm -rf mx-mfc 2> /dev/null;
  if [ "${REMOVE_TMP_DIR}" = "yes" ] ; then
    cd "${MY_DIR}"
    rm -rf ${TMP_DIR}
  fi
} # end of cleanup ()

unmount_mfcroot ()
{
  cd ${TMP_DIR}
  [ ! -d  mfcroot ] && return
  umount mfcroot || echo "error: failed to unmount mfcroot (should not happen) "
  rm -rf mfcroot 2> /dev/null
}

#
#
#
check_error ()
{
	if [ $1 != 0 ]; then
		echo "error: $2";
		echo;
		cleanup;
		exit 1;
	fi
} # end of check_error ()

#
#
#
create_pacifica_rootfs ()
{
	local mfc_root_tar=$1;

	echo "create_pacifica_rootfs()"
	# Create the ext3fs 
	rm -rf LiveOS
	mkdir  LiveOS
	check_error $? "failed to created directory LiveOS, exiting ...";

	cd LiveOS;
	echo dd if=/dev/zero of=ext3fs.img bs=1M count=1300
	dd if=/dev/zero of=ext3fs.img bs=1M count=1300 > /dev/null;
	check_error $? "failed to create ext3fs.img using dd, exiting ...";

	echo /sbin/mke2fs -T ext3 -F ext3fs.img
	/sbin/mke2fs -T ext3 -F ext3fs.img
	check_error $? "failed to created ext3 filesystem on ext3fs.img, exiting ...";

	cd ..
	rm -rf mfcroot
	mkdir  mfcroot
	check_error $? "failed to create directory mfcroot, exiting ...";

	echo mount -o loop LiveOS/ext3fs.img mfcroot/
	mount -o loop LiveOS/ext3fs.img mfcroot/
	check_error $? "failed to mount LiveOS/ext3fs.img on mfcroot, exiting ...";

	cd mfcroot
	echo tar -xf ../$mfc_root_tar;
	tar -xf ../$mfc_root_tar;
	RV=${?}
	pwd
	ls -la
	df -h .
	check_error ${RV} "failed to untar $mfc_root_tar in mfcroot, exiting ...";

	# untar done, hence we get remove the tar file
	rm -f ../$mfc_root_tar;

	# Now that the file system is extracted, need to do the changes in it 
	cp opt/nkn/pacifica/rc.sysinit etc/rc.d/rc.sysinit;
	cp opt/nkn/pacifica/dhclient.conf etc/dhcp/dhclient.conf;
	cp opt/nkn/pacifica/dhclient-exit-hooks etc/dhclient-exit-hooks;
	mv etc/rc.d/init.d/rename_ifs etc/rc.d/init.d/original.rename_ifs

	# Create /etc/fstab with only basic entries
	cat  > etc/fstab <<EOF
none            /           tmpfs   defaults          0 0
none            /proc       proc    defaults          0 0
none            /sys        sysfs   defaults          0 0
none            /dev/pts    devpts  gid=5,mode=620    0 0
none            /dev/shm    tmpfs   defaults          0 0
EOF

	# Create image_layout.sh specific to Pacifica
	cat  > etc/image_layout.sh <<EOF
IL_LAYOUT=STD
export IL_LAYOUT
IL_LO_STD_TARGET_DISK1_DEV=/dev/sda
export IL_LO_STD_TARGET_DISK1_DEV
EOF

	# Create the file for running the firstboot.sh
	touch etc/.firstboot

	# Update grub.conf with a dummy one
	cat > bootmgr/boot/grub/grub.conf <<EOF
serial --unit=0 --speed=9600 --word=8 --parity=no --stop=1
terminal --timeout=3 --dumb serial console
default=0
timeout=5
fallback=1
hiddenmenu

title mfc mfc-pacifica
      root (hd0,1)
      kernel /vmlinuz ro root=/dev/sda5   img_id=1 quiet loglevel=4 panic=10 console=tty0 console=ttyS0,9600n8


title mfc mfc-pacifica
      root (hd0,2)
      kernel /vmlinuz ro root=/dev/sda6   img_id=2 quiet loglevel=4 panic=10 console=tty0 console=ttyS0,9600n8
EOF

	# Now copy pacifica files out and unmount the root filesystem
	cd ${TMP_DIR}

	rm -rf mx-mfc
	mkdir  mx-mfc
	check_error $? "failed to create directory mx-mfc, exiting ...";

	for i in `ls -d mfcroot/opt/nkn/pacifica/*`
	do
		cp $i mx-mfc
	done
	check_error $? "failed to copy out pacifica files from rootfs, exiting ...";

	cp mfcroot/boot/vmlinuz-smp mx-mfc/vmlinuz0
	check_error $? "failed to copy out vmlinuz-smp from rootfs, exiting ...";

	cp mfcroot/bin/lfi .
	check_error $? "failed to copy out lfi from rootfs, exiting ...";

} # end of create_pacifica_rootfs ()

#
#
#
create_squashfs ()
{
	cd ${TMP_DIR}

	# Run mksquashfs to create the squashfs
	# For Samara Fir based systems we install a special verison into /usr/local/bin:
	#    /usr/local/bin/mksquashfs LiveOS  ${MFCROOTFS_IMG} -keep-as-directory -info;
	# For Samara Ginkgo (CentOS 6.3, use the native version:
	/sbin/mksquashfs LiveOS  ${MFCROOTFS_IMG} -keep-as-directory -info;
        
	check_error $? "failed to create squashfs file, exiting ...";

} # end of create_squashfs ()

#
#
#
create_mx_mfc_img ()
{
	cd ${TMP_DIR}

	FULLNAME=`echo $BASENAME | cut -c 7-`;
	# Examples: mfc-12.3.6-4_29592_371  mfc-12.3.7-qa-6_29661_371
        # Junos:cleanup-pkgs.sh: cleanup_package() uses pkgmatch="$package-[0-9]"
        # to decide whether a mounted package is an old version...
        # For no-multi and multi to coexist, PKG_FILE_NAME must not be the same.
        # PKG_FILE_NAME with "--" instead of just one dash is a workaround for this issue.
        # Since the tgz is name using PKGFILE, the PKGNAME can have the single dash.
	PKG_BASE_NAME=`echo ${FULLNAME} | cut -f1 -d-`
	PKG_VER_NAME=`echo ${FULLNAME} | cut -f2- -d-`
        PKG_FILE_NAME="${PKG_BASE_NAME}--${PKG_VER_NAME}"
	# Examples: mfc--12.3.6-4_29592_371  mfc--12.3.7-qa-6_29661_371


	# Create files that host the product version names.
	echo ${FULLNAME} > mx-mfc/version.txt;
	check_error $? "failed to create version.txt file, exiting ...";
	echo ${PKG_FILE_NAME} > mx-mfc/pkgfilename.txt;
	check_error $? "failed to create pkgfilename.txt file, exiting ...";
	
	# Update elilo.conf with the image name
	cat mx-mfc/elilo.conf.template \
	  | sed -e "s#PREFIX_PATH#/var/db/ext/juniper/mfc/images#" \
	  | sed -e "s/IMAGE_VERSION/${FULLNAME}/g" \
	  > mx-mfc/elilo.conf;
	check_error $? "failed to create elilo.conf file, exiting ...";
	rm -f mx-mfc/elilo.conf.template;

	# Copy the mfcrootfs.img
	mv ${MFCROOTFS_IMG} mx-mfc;
	# Create the mx-mfc.img file using zip
	cd mx-mfc;
	
	# Zipping the image contents to create mx-mfc.img
	zip ../${MX_MFC_IMG} *;
	check_error $? "zip failed to create file, exiting ...";

	echo "Contents of the Pacifica Image:";
	ls -l;
} # end of create_mx_mfc_img ()

#
#
#
update_initrd ()
{
	cd ${TMP_DIR}

	# The initrd from the image will be under mx-mfc directory
	cd mx-mfc;

	# First gunzip the existing initrd
	mv initrd.img initrd.img.gz
	gunzip initrd.img.gz
	check_error $? "gunzip failed to uncompress initrd.img.gz, exiting ...";

	# Create a temp directory to extract and update the initrd
	rm -rf  initrd
	mkdir   initrd
	cd initrd;
	cpio -i --make-directories < ../initrd.img;
	check_error $? "failed to extract initrd.img, exiting ...";

	# Now update init and other files with the latest version
	cp -fR ${TMP_DIR}/mfcroot/opt/nkn/pacifica/initrd/* .

	# Find the image kernel version and check if modules directory exists
	KERNEL_VER=`grep "Linux kernel version" ${TMP_DIR}/mfcroot/boot/config-smp | cut -f2 -d":"`
	KERNEL_VER=`echo $KERNEL_VER` # Stripping the leading space
	OLD_KERNEL_VER=`ls lib/modules`;

	# If the kernel version not there rename the old one to the new one
	if [ ! -d lib/modules/${KERNEL_VER} ]; then
		echo "  missing lib/modules/${KERNEL_VER}, hence creating ...";
		mv lib/modules/${OLD_KERNEL_VER} lib/modules/${KERNEL_VER};
		check_error $? "failed to move ${OLD_KERNEL_VER} to ${KERNEL_VER}, exiting ...";
	fi

	# Copy the latest kernel modules (igb.ko, dca.ko ...)to match the kernel
	for KO in `find . -name "*.ko"`
	do
		KO_NAME=`basename ${KO}`;
		NEW_KO=`find ${TMP_DIR}/mfcroot/lib/modules -name ${KO_NAME}`
		INITRD_KO=`find . -name ${KO_NAME}`;

		if [ ! -z $NEW_KO ]; then
			echo "  copying new version of ${KO_NAME}"
			cp -R ${NEW_KO} ${INITRD_KO}
			check_error $? "failed to copy ${NEW_KO}, exiting ...";
		else
			# if new ko now found remove the old one
			echo "  removing ${KO_NAME} as new version not found"
		fi
	done

	# Recreate the initrd
	find . | cpio --create --format='newc' > ../initrd.img;
	cd ..
	gzip initrd.img;
	mv initrd.img.gz initrd.img;
	cp initrd.img  ${TMP_DIR}/mfcroot/boot/
	cp initrd.img  ${TMP_DIR}/mfcroot/opt/nkn/pacifica/
	# Remove the initrd directory
	rm -rf initrd;

	cd ${TMP_DIR}/mfcroot/

	# Now that all changes to the rootfs is done  let us sync and pause
	sync
	sleep 5

} # end of update_initrd

#
#
#
add_mx_mfc_img_to_mfc_image ()
{
	cd ${TMP_DIR}

	# Leave the original img file alone and make the changes to a copy.
	MFC_NAME=`echo ${BASENAME} | cut -f2- -d-`;
	PAC_IMG=${IMG_DIR}/as-mlc-${MFC_NAME}.img
	cp ${IMG_NAME} ${PAC_IMG}
	# Replace md5sums file with an updated version that has mx-mfc.img 
	rm -f ${MD5SUMS}
	unzip ${PAC_IMG} ${MD5SUMS};
	check_error $? "unzip failed to extract ${MD5SUMS} from ${PAC_IMG}, exiting ...";

	grep -v ${MX_MFC_IMG} ${MD5SUMS} > ${MD5SUMS}.tmp;
	md5sum ./${MX_MFC_IMG} >> ${MD5SUMS}.tmp;
	check_error $? "md5sum for ${MX_MFC_IMG} failed, exiting ...";
	mv ${MD5SUMS}.tmp ${MD5SUMS}

	zip -r ${PAC_IMG} ${MD5SUMS};

	# Replace tpkg-manifest with updated md5sums and mx-mfc.img
	rm -f ${TPKG_MANIFEST}
	unzip ${PAC_IMG} ${TPKG_MANIFEST};
	check_error $? "unzip failed to extract ${TPKG_MANIFEST} from ${PAC_IMG}, exiting ...";

	grep -v ${MD5SUMS} ${TPKG_MANIFEST} | grep -v ${MX_MFC_IMG}  > ${TPKG_MANIFEST}.tmp;
	./lfi -tgunm ./${MD5SUMS} >> ${TPKG_MANIFEST}.tmp;
	./lfi -tgunm -a sha256 ./${MD5SUMS} >> ${TPKG_MANIFEST}.tmp;
	./lfi -tgunm ./${MX_MFC_IMG} >> ${TPKG_MANIFEST}.tmp;
	./lfi -tgunm -a sha256 ./${MX_MFC_IMG} >> ${TPKG_MANIFEST}.tmp;
	mv ${TPKG_MANIFEST}.tmp ${TPKG_MANIFEST};

	zip -r ${PAC_IMG} ${TPKG_MANIFEST};

	# Easy step is to add to the zip
	zip -g ${PAC_IMG} ${MX_MFC_IMG}
	check_error $? "zip failed to add ${MX_MFC_IMG} to ${PAC_IMG}, exiting ...";

	# Everything looks fine, remove mx-mfc.img
	rm -f ${MX_MFC_IMG};

} # end of add_mx_mfc_img_to_mfc_image()


#
#---------------------------------------------------------------------------
#	MAIN LOGIC BEGINS HERE 
#---------------------------------------------------------------------------
#

MY_DIR=`pwd`

# Check if image name is provided
if [ "_${1}" = "_-d" ] ; then
  TMP_DIR="${2}"
  [ -z "${TMP_DIR}" ] && usage $*
  case ${TMP_DIR} in
  .*)     echo Error, cannot specify . or .. at the start of the temp dir name. ${TMP_DIR}
          exit 1 ;;
  /tmp/*) break
          ;;
  /*/*/*) break
          ;;
  /|/*)   echo Error, temp dir name too close to root. ${TMP_DIR}
          exit 1 ;;
  esac
  shift; shift
else
  TMP_DIR="${MY_DIR}"/tmp.$$.tmp
fi
if [ $# != 1 ]; then
	usage $*;
fi
IMG_NAME="${1}"

REMOVE_TMP_DIR=no
if [ ! -d "${TMP_DIR}" ] ; then
  mkdir -p "${TMP_DIR}"
  if [ ! -d "${TMP_DIR}" ] ; then
    echo "Failed to create tmp dir ${TMP_DIR}"
    exit 1
  fi
  REMOVE_TMP_DIR=yes
else
  cleanup
fi

# Check if image exists
test -f "${IMG_NAME}"
check_error $? "$IMG_NAME file not found";

# Check if the file is a Zip file
file "${IMG_NAME}" | grep "Zip archive data" > /dev/null;
check_error $? "$IMG_NAME not a zip file";

BASENAME=`basename "${IMG_NAME}" .img`
IMG_DIR=`dirname "${IMG_NAME}"`
case "${IMG_DIR}" in
.*) IMG_DIR=`pwd`/"${IMG_DIR}" ;;
esac

cd "${TMP_DIR}"

# Extract the .tbz file from the .img file
echo "step 1: extracting ${BASENAME}.tbz";
rm -f ${BASENAME}.tbz 2> /dev/null;
echo unzip "${IMG_NAME}" ${BASENAME}.tbz
unzip "${IMG_NAME}" ${BASENAME}.tbz 2> /dev/null
check_error ${?} "${BASENAME}.tbz file missing in $IMG_NAME, unable to proceed"

# Bunzip the .tbz file
echo
echo "step 2: unzipping ${BASENAME}.tbz";
bunzip2 ${BASENAME}.tbz 2> /dev/null;
check_error $? "${BASENAME}.tbz not a valid bzip2 file, unable to proceed";

# Make sure we have the root filesystem tar file now
test -f ${BASENAME}.tar
check_error $? "${BASENAME}.tar not found, maybe unzip failed hence unable to proceed";

# Create the root filesystem for Pacifica
echo
echo "step 3: creating root filesystem from ${BASENAME}.tar";
create_pacifica_rootfs ${BASENAME}.tar

# Update the initrd with the latest init script
echo
echo "step 4: updating the pacifica initrd image file";
update_initrd

# Create the squashfs file from the rootfs
echo 
echo "step 5: creating squashfs filesystem from ${BASENAME}.tar";
create_squashfs 


# Create elilo.conf and version.txt file
echo
echo "step 6: creating the pacifica specific image file ${MX_MFC_IMG}";
create_mx_mfc_img

# Add the pacifica image mx-mfc.img to the original MFC image
add_mx_mfc_img_to_mfc_image

# All is well !!!
echo "DONE ${0}"
cleanup

exit 0

#
# End of update_image_for_pacifica.sh
#
