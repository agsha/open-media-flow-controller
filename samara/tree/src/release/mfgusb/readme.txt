#
#

==================================================
The files in this zip are to allow for manufacturing an appliance solely
using a bootable USB storage device, like a jumpdrive, pendisk, etc.  The
zip file can be used from Windows or Linux.

==================================================
-- Windows instructions:

1) Plug in your USB drive.  See that it shows up in 'My computer'.

2) Note the drive letter that Windows has given your USB drive.  It will be
   something like 'd:' 'e:' or 'f:' .   Make absolutely certain you know
   which drive letter is your USB drive, or you may seriously damage your
   computer. 

3) Unzip the mfgusb.zip file into the top folder (root directory) of your
   USB drive.  You want to do 'extract all'.

4) Double click on the relevant install '(inst_X.bat') file.  For drive
   'd:', double click on 'inst_d.bat' .  A window will flash up on your
   screen and disappear.  You have now made your USB drive bootable.

5) Power off (not just reboot) your computer.

(Continue on with step 6 in the 'Common instructions' below.

==================================================
-- Linux instructions:

1) Plug in your USB drive.  Become 'root'.  The rest of these instructions are
   done as 'root'.

2) Mount the USB drive somewhere of your choosing.  In this instructions,
   we'll assume it is called '/dev/sda1', and that you mounted it on
   '/mnt/flash'.  You may need to look at '/etc/fstab', 'dmesg' or the
   '/var/log/messages' file to discover the partition name.  Depending on how
   you USB drive is set up, the drive may be all one partition with no
   partition table (like '/dev/sda'), or may be a partition inside a
   partition table (like '/dev/sda1').

----------
        mount /dev/sda1 /mnt/flash
----------

3) Unzip the mfgusb.zip file into the root directory of your USB drive, and
   then unmount it.

----------
        cd /mnt/flash
        unzip ~/Work/tree/output/product-demo-i386/release/mfgusb/mfgusb-demo-i386-20050710-090757.zip
        cd ~
        umount /mnt/flash
----------

4) Install syslinux on the drive to make it bootable.  Remember to
   substitute your device name here, is it may be different than '/dev/sda1'.
   If you use the wrong device name you could seriously damage your
   computer.

----------
        syslinux /dev/sda1
----------

5) Power off (not just reboot) your computer

----------
        shutdown -h now
----------


-- Common steps:

==================================================
6) Make sure your BIOS is set up to allow booting from USB drives.  Not all
   BIOS's allow this.  Sometimes there are multiple BIOS settings for USB
   booting.  If there are, you may have to try multiple of them before
   getting it to work.   Try 'USB HDD' first.  Make sure to power off (not
   just reboot) between attempts.

7) Either set the boot order in your BIOS to have USB storage before your
   hard disk, or (only in some BIOS's) hit the key that brings up the boot
   menu, and select your USB device.

8) You will see the 'syslinux' prompt, and eventually the manufacturing
   environment will start.  If you see some output from 'syslinux', but no
   output from the linux kernel, your BIOS may not be compatible with either
   the version of syslinux, or something about the layout of your USB drive.
   The default version of syslinux included is 3.09 (as syslinux.exe), and
   an older version 2.11 is also included (as sysln211.exe).  To try to
   older version of syslinux, just rename sysln211.exe to syslinux.exe and
   try these instructions again.

9) Run manufacture.sh as usual.  The system image file is in the root
   directory, and called image.img .  An example manufacture command is:

----------
       manufacture.sh -a -m echo100 -f /image.img
----------

10) Remove the USB drive, and reboot your system.


11) Repeat: Later, to install a new image onto the USB, you need only do the
  'unzip' step, step 3) above, and then power off and restart your computer.

==================================================


-- Problems: if your USB seems messed up, or won't boot, you may want to try
   these steps, which will destroy everything on the USB.

==================================================
-- From Linux:

P-1) Figure out which raw device is yours, WITHOUT the partition number.
   We'll assume it's /dev/sda .  If you think it's /dev/sda1, use /dev/sda .

P-2) As root do the following:  (Note that 'mbr.bin' is included in the zip
   file, so the instructions assume you start out as root in the mfgusb zip
   unzip'd on your hard disk.)

----------
dd if=/dev/zero of=/dev/sda bs=512 count=100
sfdisk -R /dev/sda
dd if=mbr.bin of=/dev/sda bs=1
echo ',,6,*' | sfdisk /dev/sda
mkdosfs /dev/sda1
syslinux /dev/sda1
----------

P-3) Unzip the image, as in the instructions above.  Your device name is
   /dev/sda1 , not /dev/sda . 

----------
mount /dev/sda1 /mnt/flash
cd /mnt/flash
unzip ~/Work/tree/output/product-demo-i386/release/mfgusb/mfgusb-demo-i386-20050710-090757.zip
cd ~
umount /mnt/flash
----------

P-4) Continue on with step 5) above

==================================================
