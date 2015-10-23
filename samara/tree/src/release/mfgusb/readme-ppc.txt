#
#

==================================================

The files in this ZIP are to allow for manufacturing a PowerPC-based
appliance solely using a bootable USB storage device, like a USB flash
drive.  The appliance must already be running a recent, TMS-integrated
version of U-boot.  

The ZIP file can be put onto the USB storage device from Windows or
Linux.

==================================================
-- Windows instructions:

0) Power off the appliance.

----> On your Windows machine:

1) Plug in your USB flash drive.  See that it shows up in 'My computer'.

2) Note the drive letter that Windows has given your USB drive.  It will
   be something like 'd:' 'e:' or 'f:' .  

   Verify that you are working with the first partition on the USB
   drive.  The drive must be partitioned, and you must use the first
   partition or the manufacture files will not be found.

   Verify that the partition on the USB drive is formatted with FAT16,
   not with NTFS or FAT32.  Further, the partition must be less than
   1900MB .

   Verify that the drive has enough free space.  Typically, 50MB more
   than twice the size of the ZIP file will be sufficient.

3) Unzip the mfgusb.zip file into the top folder (root directory) of the
   USB drive.  You want to do 'extract all'.  A new folder (directory)
   will be created with the HWNAME of the target (in lower-case), and
   will contain all the required files for manufacturing the appliance.

4) Unmount or "eject" the USB drive.  This may be called 'Safely remove
   hardware'.  If you do not do this, you may corrupt the images on the
   drive.

5) Unplug the USB drive.

----> On the appliance:

6) Plug the USB drive in to the appliance.  Power on the appliance.

7) Hit a key to stop U-boot auto boot.

8) Choose 'c' to enter the command prompt.  Depending on if and how the
   system was previously imaged, you may already be in the command
   prompt, denoted by "=> " .

9) Type "run mfg_usb" and hit enter.  The system will spend a while
   loading the required files off USB, and then boot.  

   If you get an error like:

       ## Error: "mfg_usb" not defined

   You must first update the U-boot of the appliance.  How to do the
   U-boot update is outside the scope of this document.

10) Run manufacture.sh as usual.  The system image file is in the root
    directory, and is called "image.img" .  An example manufacture
    command is:

    ----------
       manufacture.sh -a -m echo2020d -f /image.img
    ----------

    NOTE: if the build that produced this does NOT have the image as
        part of the root filesystem (customer.mk will have
        MKMFGUSB_ARGS=-e), you will have to first mount the first
        partition of the USB drive, typically /dev/sda1.  This approach
        may be quicker, as loading files off USB from u-boot is slower
        than it is in Linux.

11) Remove the USB drive, and reboot the appliance.

==================================================

-- Linux notes

# Boot the mfg environment, or any other linux distro on an x86 or
# PowerPC system

# Note this destroys everything on the drive you specify, so make sure
# you don't mistakenly choose your desktop's drive!

# Check /proc/partitions or dmesg, should be like /dev/sdb or /dev/sda
DISK=/dev/sdX
# Partition to 1900MB FAT16
echo "0,1900 e" | sfdisk -uM ${DISK}
# Put a FAT16 filesystem on it
mkfs.vfat -F 16 ${DISK}1

# Mount our new partition
mkdir -p /mnt
mount -t vfat ${DISK}1 /mnt

# Get the ZIP (or copy it into place if it is already on the system)
cd /mnt
wget http://SERVER/mfgusb.zip

# Extract
unzip mfgusb.zip 

# Unmount
cd /
sync
umount /mnt

- Unplug the USB drive.

---> Follow the above Windows instructions from step 6

==================================================
