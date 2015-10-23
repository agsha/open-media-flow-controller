--------------------------------------------------------------------------------
mfc-dmi-adapter-11A.1-R4
2011-10-13
Changes done on Oct 13th 2011 by Ramanand (ramanandn@juniper.net)
The VJX image has been updated with a newer version that has a 11A.1 version of
RE-SDK app preinstalled. The format of the file name has been changed as we do
not want to expose the words Junos or VJX at this time. Since the purpose of
the VJX image at this time for MFA connectivity and at a high level something
like the DMI adapter, we are calling it DMI adapter for now. 

New version of VJX image:
-r--r--r-- 1 dpeet jrs 2047868928 Oct 19 13:45 mfc-dmi-adapter-11A.1-R4.img
-r--r--r-- 1 dpeet jrs  190858884 Oct 19 13:50 mfc-dmi-adapter-11A.1-R4.img.zip
Any question please contact ramanandn@juniper.net
--------------------------------------------------------------------------------
mfc-dmi-adapter-11.B.3
2011-12-28
The VJX image was updated by Sivakumar Gurumurthy <sgurumurthy@juniper.net>
with the latest RE-SDK App for use with mfc-11B.3.
The RE-SDK App version is "media-flow-10.3R-3.07R-11.B.3".
The image file was compressed with this command:
   zip mfc-dmi-adapter-11.B.3.img.zip mfc-dmi-adapter-11.B.3.img
The original and zipped image files:
-r--r--r-- 1 dpeet jrs 2047868928 Dec 28 14:23 mfc-dmi-adapter-11.B.3.img
-r--r--r-- 1 dpeet jrs  201627695 Dec 28 14:55 mfc-dmi-adapter-11.B.3.img.zip
Sums:
10775 1999872 mfc-dmi-adapter-11.B.3.img
54990 196903 mfc-dmi-adapter-11.B.3.img.zip
--------------------------------------------------------------------------------
mfc-dmi-adapter-11.B.3-R1
2012-01-05 updated 2012-02-08
The VJX image was updated by Sivakumar Gurumurthy <sgurumurthy@juniper.net>
with the latest RE-SDK App for use with mfc-11.B.3-1.  This was from:
rev 187 svn://cmbu-svn01.juniper.net/junos-media-flow/branches/2011/pacifica/mfc/src
The RE-SDK App version is "media-flow-10.3R-3.07R-11.B.3-R1"
The image file was compressed with this command:
  zip mfc-dmi-adapter-11.B.3-R1.img.zip mfc-dmi-adapter-11.B.3-R1.img
The original and zipped image files:
-r--r--r-- 1 dpeet jrs 2047868928 Jan  5 11:00 mfc-dmi-adapter-11.B.3-R1.img
-r--r--r-- 1 dpeet jrs  204256149 Jan  5 11:00 mfc-dmi-adapter-11.B.3-R1.img.zip
Sums:
53679 1999872 mfc-dmi-adapter-11.B.3-R1.img
52336 199469 mfc-dmi-adapter-11.B.3-R1.img.zip
The released MFC builds that has this are:
+ mfc-11.B.3-1_22606_306 2012-01-05 (everett branch)
+ mfc-11.B.3-2_23254_306 2012-02-08 (everett branch)
--------------------------------------------------------------------------------
mfc-dmi-adapter-11.B.3-R2
2012-02-27
The VJX image was updated by Dheeraj Gautam<dgautam@juniper.net>
with the latest RE-SDK App, and is used in mfc-11.B.3 (everett rev 23611 and later),
and mfc-12.1 (flathead rev 23612 and later).
This was from: rev 284 of
  svn://cmbu-svn01.juniper.net/junos-media-flow/trunk/junos-sdk-apps/mfc
The RE-SDK App version is "media-flow-10.3R-3.07R-11.B.3R2.tgz"
The image file was compressed with this command:
  zip mfc-dmi-adapter-11.B.3-R2.img.zip mfc-dmi-adapter-11.B.3-R2.img 
The original and zipped image files:
-r--r--r-- 1 dpeet jrs 2047868928 Feb 27 10:22 mfc-dmi-adapter-11.B.3-R2.img
-r--r--r-- 1 dpeet jrs  209400438 Feb 27 12:05 mfc-dmi-adapter-11.B.3-R2.img.zip
Sums:
19711 1999872 mfc-dmi-adapter-11.B.3-R2.img
48510 204493 mfc-dmi-adapter-11.B.3-R2.img.zip
--------------------------------------------------------------------------------
mfc-dmi-adapter-11.B.3-R2-SINGLE-IP
2012-02-28
The VJX image was updated by Dheeraj Gautam<dgautam@juniper.net>
with the latest RE-SDK App for use with mfc-11.B.3 (everett rev 23611 and later).
This was from: rev 284 of
  svn://cmbu-svn01.juniper.net/junos-media-flow/trunk/junos-sdk-apps/mfc
The RE-SDK App version is "media-flow-10.3R-3.07R-11.B.3R2.tgz"
This image contains default configuration of single IP address.
This is only for internal use (private build) and is NOT supposed to be
integrated with any RC/QA builds. This is needed for Apple POC.
The image file was compressed with this command:
  zip mfc-dmi-adapter-11.B.3-R2-SINGLE-IP.img.zip mfc-dmi-adapter-11.B.3-R2-SINGLE-IP.img 
The original and zipped image files:
-r--r--r-- 1 dpeet jrs 2047868928 Feb 27 23:10 mfc-dmi-adapter-11.B.3-R2-SINGLE-IP.img
-r--r--r-- 1 dpeet jrs  209376105 Feb 27 23:10 mfc-dmi-adapter-11.B.3-R2-SINGLE-IP.img.zip
Sums:
24820 1999872 mfc-dmi-adapter-11.B.3-R2-SINGLE-IP.img
36087 204469 mfc-dmi-adapter-11.B.3-R2-SINGLE-IP.img.zip
--------------------------------------------------------------------------------
mfc-dmi-adapter-11.B.3-R3
2012-03-05
The VJX image was updated by Dheeraj Gautam<dgautam@juniper.net>
with the latest RE-SDK App, and is used in mfc-11.B.3-RC3
(which is mfc-11.B.3-3_23828_314) and mfc-12.1 (trunk rev 23879 and later)
This was from: rev 287 of
  svn://cmbu-svn01.juniper.net/junos-media-flow/trunk/junos-sdk-apps/mfc
The RE-SDK App version is "media-flow-10.3R-3.07R-11.B.3R3.tgz"
The image file was compressed with this command:
  zip mfc-dmi-adapter-11.B.3-R3.img.zip mfc-dmi-adapter-11.B.3-R3.img 
The original and zipped image files:
-r--r--r-- 1 dpeet jrs 2047868928 Mar  5 17:36 mfc-dmi-adapter-11.B.3-R3.img
-r--r--r-- 1 dpeet jrs  209605554 Mar  5 17:44 mfc-dmi-adapter-11.B.3-R3.img.zip
Sums:
43496 1999872 mfc-dmi-adapter-11.B.3-R3.img
54515 204693 mfc-dmi-adapter-11.B.3-R3.img.zip
978c06c5ef0a29cc9623aea44b3d22fb  mfc-dmi-adapter-11.B.3-R3.img
5be1b66dc71637a4d9a1a1de0fcdc9a9  mfc-dmi-adapter-11.B.3-R3.img.zip
--------------------------------------------------------------------------------
mfc-dmi-adapter-12.1-QA
2012-03-15
The VJX image was updated by Dheeraj Gautam<dgautam@juniper.net>
with the latest RE APP, and is for use in in mfc-12.1.
This DMI image contains Single IP configuration.  Also:
1: Removed unwanted configuration from DMI configuration (domain-name was
set to jnpr.net, removed SNMP configuration, web-management).
2: Removed unwanted files from VJX.
These were done the following way: (by Dheeraj Gautam <dgautam@juniper.net>)
  On mfc-dmi-adapter-11.B.3-R3.img, issued the following commands
  to change the configuration of JunOS (in config mode) and renamed
  image as mfc-dmi-adapter-12.1-QA.img:
 
    edit
    delete interfaces em0
    rename interfaces em1 to em0
    set system syslog file messages any notice
    set system syslog file default-log-messages any notice
    delete system services web-management
    delete snmp
    delete system domain-name jnpr.net
    delete routing-options
    commit
 
  Following command in Operational mode (following by yes) :
    request system storage cleanup
 
This contains RE-SDK App generated from revision 287 of
  svn://cmbu-svn01.juniper.net/junos-media-flow/trunk/junos-sdk-apps/mfc
The RE-SDK App version contained is  "media-flow-10.3R-3.07R-11.B.3R3.tgz"
The image file was compressed with this command:
  zip mfc-dmi-adapter-12.1-QA.img.zip mfc-dmi-adapter-12.1-QA.img
The original and zipped image files:
-r--r--r-- 1 dpeet jrs 2047868928 Mar 15 10:40 mfc-dmi-adapter-12.1-QA.img
-r--r--r-- 1 dpeet jrs  209010897 Mar 15 10:45 mfc-dmi-adapter-12.1-QA.img.zip
Sums:
31004 1999872 mfc-dmi-adapter-12.1-QA.img
60752 204113 mfc-dmi-adapter-12.1-QA.img.zip
5a2872d2c2657cd327b2a14f652455ac  mfc-dmi-adapter-12.1-QA.img
195eeae30c91be528de9507eea88103b  mfc-dmi-adapter-12.1-QA.img.zip
--------------------------------------------------------------------------------
mfc-dmi-adapter-12.1.0-R1.img
2012-03-21
The VJX image was updated by Dheeraj Gautam<dgautam@juniper.net>
with the latest RE-SDK App for use with mfc-12.1.0 (flathead rev 24219 and
later).
This was from: rev 290 of
  svn://cmbu-svn01.juniper.net/junos-media-flow/trunk/junos-sdk-apps/mfc
The RE-SDK App version is "media-flow-10.3R-3.07R-12.1.0-R1.tgz".
Base VJX used is mfc-dmi-adapter-12.1-QA.img.
 
The image file was compressed with this command:
  zip mfc-dmi-adapter-12.1.0-R1.img.zip mfc-dmi-adapter-12.1.0-R1.img
The original and zipped image files:
-r--r--r-- 1 dpeet jrs 2047868928 Mar 22 01:57 mfc-dmi-adapter-12.1.0-R1.img
-r--r--r-- 1 dpeet jrs  206022656 Mar 22 02:14 mfc-dmi-adapter-12.1.0-R1.img.zip
Sums:
42415 1999872 mfc-dmi-adapter-12.1.0-R1.img
16266 201194 mfc-dmi-adapter-12.1.0-R1.img.zip
2cd7328a193f8299efa93ad2d34baefa  mfc-dmi-adapter-12.1.0-R1.img
af970c6f5e989633241df9bd4c17cc81  mfc-dmi-adapter-12.1.0-R1.img.zip
--------------------------------------------------------------------------------
mfc-dmi-adapter-12.2.0-R1.img
2012-03-21
The VJX image was updated by Dheeraj Gautam<dgautam@juniper.net>
(TBD - need info about how it was created)

The image file was compressed with this command:
  zip mfc-dmi-adapter-12.2.0-R1.img.zip mfc-dmi-adapter-12.2.0-R1.img
The original and zipped image files:
-r--r--r-- 1 dpeet jrs 2047868928 Apr  2 02:46 mfc-dmi-adapter-12.2.0-R1.img
-r--r--r-- 1 dpeet jrs  171823387 Apr  2 02:46 mfc-dmi-adapter-12.2.0-R1.img.zip
Sums:
15296 1999872 mfc-dmi-adapter-12.2.0-R1.img
17883 167797 mfc-dmi-adapter-12.2.0-R1.img.zip
a689803644612e798729195d2a55c9c5  mfc-dmi-adapter-12.2.0-R1.img
10366ccc67e686c0834846cdf2ac9c51  mfc-dmi-adapter-12.2.0-R1.img.zip
--------------------------------------------------------------------------------
mfc-dmi-adapter-12.2.0-R2.img
2012-06-18
The VJX image was updated by Dheeraj Gautam<dgautam@juniper.net>
with the latest RE-SDK App for use with mfc-12.2.0 (greenhorn) (PR 786283).
This was from: rev 317 of
  svn://cmbu-svn01.juniper.net/junos-media-flow/trunk/junos-sdk-apps/mfc
The RE-SDK App version is media-flow-10.3R-3.07R-12.2.0-R1.tgz
Base VJX used is mfc-dmi-adapter-12.1.0-R1.img.
 
The image file was compressed with this command:
    zip mfc-dmi-adapter-12.2.0-R2.img.zip mfc-dmi-adapter-12.2.0-R2.img
The original and zipped image files:
-r--r--r-- 1 dpeet jrs 2047868928 Jun 18 10:57 mfc-dmi-adapter-12.2.0-R2.img
-r--r--r-- 1 dpeet jrs  175808526 Jun 18 10:57 mfc-dmi-adapter-12.2.0-R2.img.zip
Sums:
05784 1999872 mfc-dmi-adapter-12.2.0-R2.img
57519 171689 mfc-dmi-adapter-12.2.0-R2.img.zip
2bff49ecef6900d4d0967a37fda071f9  mfc-dmi-adapter-12.2.0-R2.img
5afd5acc31d3ec64fd0c791a8f56d079  mfc-dmi-adapter-12.2.0-R2.img.zip
--------------------------------------------------------------------------------
mfc-dmi-adapter-12.2.1-R1.img
2012-07-13
The VJX image was updated by Dheeraj Gautam<dgautam@juniper.net>
with the latest RE-SDK App for use with mfc-12.2.1 (flathead) (PR ???)
This was from: rev 333 of
  svn://cmbu-svn01.juniper.net/junos-media-flow/branches/2012/mfa-3.2/mfc
The RE-SDK App version is "media-flow-10.3R-3.07R-12.2.1-R1.tgz"
Base VJX used is mfc-dmi-adapter-12.2.0-R2.img.
 
Following commands have been executed after installation of RE-SDK App
inside VJX.  This is reduce the number of logs generated by VJX and it
helps to reduced CPU consumption in MFC:
    set system syslog file messages match "(mfcd)|(mgd)"

The image file was compressed with this command:
    zip mfc-dmi-adapter-12.2.1-R1.img.zip mfc-dmi-adapter-12.2.1-R1.img
The original and zipped image files:
-r--r--r-- 1 dpeet jrs 2047868928 Jul 13 01:07 mfc-dmi-adapter-12.2.1-R1.img
-r--r--r-- 1 dpeet jrs  176249870 Jul 13 01:07 mfc-dmi-adapter-12.2.1-R1.img.zip
Sums:
25425 1999872 mfc-dmi-adapter-12.2.1-R1.img
04823 172120 mfc-dmi-adapter-12.2.1-R1.img.zip
7a56f0ce221e172b9075fca6df131b5a  mfc-dmi-adapter-12.2.1-R1.img
604ef25d0f2d50e83a395829d7720780  mfc-dmi-adapter-12.2.1-R1.img.zip
--------------------------------------------------------------------------------
mfc-dmi-adapter-12.2.1-R2.img
2012-07-23
The VJX image was updated by Dheeraj Gautam<dgautam@juniper.net>
with the latest RE-SDK App for use with mfc-12.2.1 (garfield) (PR 776424-1)
This was from: rev 333 of
  svn://cmbu-svn01.juniper.net/junos-media-flow/branches/2012/mfa-3.2/mfc
The RE-SDK App version is "media-flow-10.3R-3.07R-12.2.1-R1.tgz"
Base VJX used is mfc-dmi-adapter-12.2.1-R1.img.
 
Following changes has been made to VJX:
Added following lines to /boot/loader.conf. These are done to increase the
number of maxproc and maxprocperuid.
This helps in avoiding "unable to open SSH channe" error via MFA.
    kern.maxproc="500"
    kern.maxprocperuid="500"
    kern.hz="100"
 
The image file was compressed with this command:
    zip mfc-dmi-adapter-12.2.1-R2.img.zip mfc-dmi-adapter-12.2.1-R2.img
The original and zipped image files: 
-r--r--r-- 1 dpeet jrs 2047868928 Jul 23 23:13 mfc-dmi-adapter-12.2.1-R2.img
-r--r--r-- 1 dpeet jrs  176366722 Jul 23 23:13 mfc-dmi-adapter-12.2.1-R2.img.zip
sum:
51509 1999872 mfc-dmi-adapter-12.2.1-R2.img
17130 172234 mfc-dmi-adapter-12.2.1-R2.img.zip
md5sum:
36fe79a5217d20731c2e9ee7a45c6c15  mfc-dmi-adapter-12.2.1-R2.img
6f91bcaadb35d10a90490e4360991a6e  mfc-dmi-adapter-12.2.1-R2.img.zip
--------------------------------------------------------------------------------
mfc-dmi-adapter-12.2.3-R1.img
2012-09-21
The VJX image was generated by Sivakumar Gurumurthy <sgurumurthy@juniper.net>
with the latest RE-SDK App for use with mfc-12.2.3 (garfield) (PR 816958)
This was from: rev 342 of
  svn://cmbu-svn01.juniper.net/junos-media-flow/branches/2012/mfa-3.2/mfc/src
 
The RE-SDK App version built using 12.1 SDK and version 342, Steps at
http://cmbu-svn01.juniper.net/mediawiki/index.php/Setting_up_12.1_based_RE-SDK_BSB
http://cmbu-svn01.juniper.net/mediawiki/index.php/Packaging_12.1_based_RE-SDK_DSB
 
Step to re-generate 12.1 VJX image has put up at
http://cmbu-svn01.juniper.net/mediawiki/index.php/12.1_VJX_-_All_you_should_know
 
The image file was compressed with this command:
     zip mfc-dmi-adapter-12.2.3-R1.img.zip mfc-dmi-adapter-12.2.3-R1.img
The original and zipped image files:
-r--r--r-- 1 dpeet jrs 2147483648 Sep 21 12:30 mfc-dmi-adapter-12.2.3-R1.img
-r--r--r-- 1 dpeet jrs  202985528 Sep 21 12:30 mfc-dmi-adapter-12.2.3-R1.img.zip
sum:
17917 2097152 mfc-dmi-adapter-12.2.3-R1.img
54174 198229 mfc-dmi-adapter-12.2.3-R1.img.zip
md5sum:
1168b7071b27d7786ed82e5c935de88e  mfc-dmi-adapter-12.2.3-R1.img
6f91501d0e2d883ee9e74442b69d1a63  mfc-dmi-adapter-12.2.3-R1.img.zip
sha1sum:
505a377137e98e61736cc1fa3669f1f67bd5de2a  mfc-dmi-adapter-12.2.3-R1.img
e056235b1a24855cf38520479956513fc2ace302  mfc-dmi-adapter-12.2.3-R1.img.zip
--------------------------------------------------------------------------------
mfc-dmi-adapter-12.2.3-R2.img
2012-10-02
The VJX image was generated by Sivakumar Gurumurthy <sgurumurthy@juniper.net>
with the latest RE-SDK App for use with mfc-12.2.3 (garfield) (PR 816958)
This was from: rev 40 of
  svn://cmbu-svn01.juniper.net/re-sdk-mf/12.x/trunk/src
  
The RE-SDK App version built using 12.1 SDK and version 40 Steps at
  http://cmbu-svn01.juniper.net/mediawiki/index.php/Setting_up_12.1_based_RE-SDK_BSB
  http://cmbu-svn01.juniper.net/mediawiki/index.php/Packaging_12.1_based_RE-SDK_DSB
  
Step to re-generate 12.1 VJX image has put up at
  http://cmbu-svn01.juniper.net/mediawiki/index.php/12.1_VJX_-_All_you_should_know
 
The image file was compressed with this command:
  zip mfc-dmi-adapter-12.2.3-R2.img.zip mfc-dmi-adapter-12.2.3-R2.img
The original and zipped image files:
-r--r--r-- 1 dpeet jrs 2147483648 Oct  2 13:28 mfc-dmi-adapter-12.2.3-R2.img
-rw-r--r-- 1 dpeet jrs  208349311 Oct  2 13:28 mfc-dmi-adapter-12.2.3-R2.img.zip
sum:
21044 2097152 mfc-dmi-adapter-12.2.3-R2.img
34237 203467 mfc-dmi-adapter-12.2.3-R2.img.zip
md5sum:
c0669e7dafd686ddd44aa04df54b91ea  mfc-dmi-adapter-12.2.3-R2.img
bfb24a3dd40264e0d9bd35f923cf9ab3  mfc-dmi-adapter-12.2.3-R2.img.zip
sha1sum:
77061c89d3d64bbbda44812eafeac641f1dd4131  mfc-dmi-adapter-12.2.3-R2.img
26f387e3504257f4607c1272ec15e6c7b84fccf5  mfc-dmi-adapter-12.2.3-R2.img.zip
--------------------------------------------------------------------------------
mfc-dmi-adapter-12.2.4-R1.img
2012-10-23
The VJX image was generated by Sivakumar Gurumurthy <sgurumurthy@juniper.net>
with the latest RE-SDK App for use with mfc-12.2.4 (garfield) (PR 816958)
This was from: rev 53 of svn://cmbu-svn01.juniper.net/re-sdk-mf/12.x/trunk/src
 
The RE-SDK App version built using 12.1 SDK and version 342, Steps at
http://cmbu-svn01.juniper.net/mediawiki/index.php/Setting_up_12.1_based_RE-SDK_BSB
http://cmbu-svn01.juniper.net/mediawiki/index.php/Packaging_12.1_based_RE-SDK_DSB
 
Step to re-generate 12.1 VJX image has put up at
http://cmbu-svn01.juniper.net/mediawiki/index.php/12.1_VJX_-_All_you_should_know

The image file was compressed with this command:
  zip mfc-dmi-adapter-12.2.4-R1.img.zip mfc-dmi-adapter-12.2.4-R1.img
The original and zipped image files:
-r--r--r-- 1 dpeet jrs 2147483648 Oct 23 14:29 mfc-dmi-adapter-12.2.4-R1.img
-r--r--r-- 1 dpeet jrs  236073713 Oct 23 14:29 mfc-dmi-adapter-12.2.4-R1.img.zip
sum:
45786 2097152 mfc-dmi-adapter-12.2.4-R1.img
13432 230541 mfc-dmi-adapter-12.2.4-R1.img.zip
md5sum:
ba67b4ee5c17bccf91ee7b061eb63964  mfc-dmi-adapter-12.2.4-R1.img
bff8967cfd8682369722f289caee4bc2  mfc-dmi-adapter-12.2.4-R1.img.zip
sha1sum:
3efad21fa8661be3d1cfee7828e64debb4b103ee  mfc-dmi-adapter-12.2.4-R1.img
940f66790fa67b589e514a06245afd3ceba98c68  mfc-dmi-adapter-12.2.4-R1.img.zip
--------------------------------------------------------------------------------
mfc-dmi-adapter-12.2.4-R2.img
1012-11-13
The VJX image was generated by Sivakumar Gurumurthy <sgurumurthy@juniper.net>
with the latest RE-SDK App for use with mfc-12.2.4
This was from: rev 65 of svn://cmbu-svn01.juniper.net/re-sdk-mf/12.x/trunk/src
     
The RE-SDK App version built using 12.1 SDK and version 342, Steps at
http://cmbu-svn01.juniper.net/mediawiki/index.php/Setting_up_12.1_based_RE-SDK_BSB
http://cmbu-svn01.juniper.net/mediawiki/index.php/Packaging_12.1_based_RE-SDK_DSB
 
Step to re-generate 12.1 VJX image has put up at
http://cmbu-svn01.juniper.net/mediawiki/index.php/12.1_VJX_-_All_you_should_know

The image file was compressed with this command:
  zip mfc-dmi-adapter-12.2.4-R2.img.zip mfc-dmi-adapter-12.2.4-R2.img
The original and zipped image files:
-r--r--r-- 1 dpeet jrs 2147483648 Nov 13 16:16 mfc-dmi-adapter-12.2.4-R2.img
-r--r--r-- 1 dpeet jrs  213515650 Nov 13 16:16 mfc-dmi-adapter-12.2.4-R2.img.zip
sum:
53329 2097152 mfc-dmi-adapter-12.2.4-R2.img
24020 208512 mfc-dmi-adapter-12.2.4-R2.img.zip
md5sum:
0dd675217b4f96ef5a183a2042a69739  mfc-dmi-adapter-12.2.4-R2.img
11a814c61e750fc354c3dc246ef776f9  mfc-dmi-adapter-12.2.4-R2.img.zip
sha1sum:
cb97e3db23661c7ce730ba7de952fd7ae970ae57  mfc-dmi-adapter-12.2.4-R2.img
510ffeb6a6ed63351fd76c62fd33ef70f392bc1f  mfc-dmi-adapter-12.2.4-R2.img.zip
--------------------------------------------------------------------------------
mfc-dmi-adapter-12.2.4-R3.img
2012-11-27
The VJX image was generated by Sivakumar Gurumurthy <sgurumurthy@juniper.net>
with the latest RE-SDK App for use with mfc-12.2.4
This was from: rev 77 of svn://cmbu-svn01.juniper.net/re-sdk-mf/12.x/trunk/src
     
The RE-SDK App version built using 12.1 SDK and version 342, Steps at
http://cmbu-svn01.juniper.net/mediawiki/index.php/Setting_up_12.1_based_RE-SDK_BSB
http://cmbu-svn01.juniper.net/mediawiki/index.php/Packaging_12.1_based_RE-SDK_DSB
 
Step to re-generate 12.1 VJX image has put up at
http://cmbu-svn01.juniper.net/mediawiki/index.php/12.1_VJX_-_All_you_should_know

The image file was compressed with this command:
  zip mfc-dmi-adapter-12.2.4-R3.img.zip mfc-dmi-adapter-12.2.4-R3.img
The original and zipped image files:
-r--r--r-- 1 dpeet jrs 2147483648 Nov 27 14:07 mfc-dmi-adapter-12.2.4-R3.img
-r--r--r-- 1 dpeet jrs  215832630 Nov 27 14:23 mfc-dmi-adapter-12.2.4-R3.img.zip
sum:
23079 2097152 mfc-dmi-adapter-12.2.4-R3.img
43365 210775 mfc-dmi-adapter-12.2.4-R3.img.zip
md5sum:
34388f0d9b03c38e17480b951a81b659  mfc-dmi-adapter-12.2.4-R3.img
fac83b5ce8f395ab555b70b0e2f0b315  mfc-dmi-adapter-12.2.4-R3.img.zip
sha1sum:
cee19f0f57b0d6bd858469b18b419455a0b6db49  mfc-dmi-adapter-12.2.4-R3.img
ce78ea81aebc505659cb1dc890877a9eee955d23  mfc-dmi-adapter-12.2.4-R3.img.zip
--------------------------------------------------------------------------------
mfc-dmi-adapter-12.2.4-R4.img
2012-12-10
The VJX image was generated by Sivakumar Gurumurthy <sgurumurthy@juniper.net>
with the latest RE-SDK App for use with mfc-12.2.4
This was from: rev 81 of svn://cmbu-svn01.juniper.net/re-sdk-mf/12.x/trunk/src
     
The RE-SDK App version built using 12.1 SDK and version 342, Steps at
http://cmbu-svn01.juniper.net/mediawiki/index.php/Setting_up_12.1_based_RE-SDK_BSB
http://cmbu-svn01.juniper.net/mediawiki/index.php/Packaging_12.1_based_RE-SDK_DSB
 
Step to re-generate 12.1 VJX image has put up at
http://cmbu-svn01.juniper.net/mediawiki/index.php/12.1_VJX_-_All_you_should_know

The image file was compressed with this command:
  zip mfc-dmi-adapter-12.2.4-R4.img.zip mfc-dmi-adapter-12.2.4-R4.img
The original and zipped image files:
-r--r--r-- 1 dpeet jrs 2147483648 Dec 10 16:40 mfc-dmi-adapter-12.2.4-R4.img
-r--r--r-- 1 dpeet jrs  222667209 Dec 10 18:09 mfc-dmi-adapter-12.2.4-R4.img.zip
sum:
47121 2097152 mfc-dmi-adapter-12.2.4-R4.img
54046 217449 mfc-dmi-adapter-12.2.4-R4.img.zip
md5sum:
170dc165a6c974fe2546f386bd83211c  mfc-dmi-adapter-12.2.4-R4.img
d28e0576c2e25ece1dff70bf9d87a568  mfc-dmi-adapter-12.2.4-R4.img.zip
sha1sum:
d063c97a0c89cc93cda99ef5f32d063d9a1ac86c  mfc-dmi-adapter-12.2.4-R4.img
3ec3432ff6325163eb5551962c06e5de5bdaa2c0  mfc-dmi-adapter-12.2.4-R4.img.zip
--------------------------------------------------------------------------------
mfc-dmi-adapter-12.3.2-R1.img
2013-05-06
This MFC-DMI Adapter VJX image was generated by starting with the
mfc-dmi-adapter-12.2.4-R4.img VJX image and upgrading the RE-SDK App to
the latest version and then creating a new image file of the changed VJX.
Done by Dheeraj Gautam<dgautam@juniper.net>
Used the latest RE-SDK App for use with mfc-12.3.2 (halifax) (PR 776424-1)
RE-SDK App source:
  rev 88 of svn://cmbu-svn01.juniper.net/re-sdk-mf/12.x/trunk/src
RE-SDK App Build Steps:
  http://cmbu-svn01.juniper.net/mediawiki/index.php/Setting_up_12.1_based_RE-SDK_BSB
The RE-SDK App package used:
  media-flow-i386-12.1-12.3.2-3.4-88.tgz
The changes:
* New namespace properties added
* Namespace precedence changes
* Support for multiple FQDN support added
* Config version, schema version RPC added
* Backup config RPC added 

The MFC-DMI Adapter VJX image file was compressed with this command:
  zip mfc-dmi-adapter-12.3.2-R1.img.zip mfc-dmi-adapter-12.3.2-R1.img
The original and zipped image files:
-r--r--r-- 1 dpeet jrs 2147483648 May  6 10:43 mfc-dmi-adapter-12.3.2-R1.img
-rw-r--r-- 1 dpeet jrs  230303057 May  6 11:36 mfc-dmi-adapter-12.3.2-R1.img.zip
sum:
47650 2097152 mfc-dmi-adapter-12.3.2-R1.img
03510 224906 mfc-dmi-adapter-12.3.2-R1.img.zip
md5sum:
194ca67550966cd23a632a20b997b455  mfc-dmi-adapter-12.3.2-R1.img
f3397d5904d096e437b55c69cb6c73d7  mfc-dmi-adapter-12.3.2-R1.img.zip
sha1sum:
2f48050f4c065bd624224647f628764d40bbfbff  mfc-dmi-adapter-12.3.2-R1.img
8200c48db9aed0223ff1b0308d1b962f87902e42  mfc-dmi-adapter-12.3.2-R1.img.zip
--------------------------------------------------------------------------------
mfc-dmi-adapter-12.3.3-R1.img
2013-07-09
This MFC-DMI Adapter VJX image was generated by starting with the
mfc-dmi-adapter-12.2.4-R4.img VJX image and upgrading the RE-SDK App to
the latest version and then creating a new image file of the changed VJX.
Done by Dheeraj Gautam<dgautam@juniper.net>
Used the latest RE-SDK App for use with mfc-12.3.3 (halifax) (PR 878289-2)
RE-SDK App source:
  rev 93 of svn://cmbu-svn01.juniper.net/re-sdk-mf/12.x/trunk/src
RE-SDK App Build Steps:
  http://cmbu-svn01.juniper.net/mediawiki/index.php/Setting_up_12.1_based_RE-SDK_BSB
The RE-SDK App package used:
  media-flow-i386-12.1-12.3.2-3.4-93.tgz
The changes:
* Added MFA dashboard stats RPC
* Added support for 1024 namespaces
* Added correct check for precedence
 
The MFC-DMI Adapter VJX image file was compressed with this command:
  zip mfc-dmi-adapter-12.3.3-R1.img.zip mfc-dmi-adapter-12.3.3-R1.img
The original and zipped image files:
-r--r--r-- 1 dpeet jrs 2147483648 Jul  8 23:40 mfc-dmi-adapter-12.3.3-R1.img
-r--r--r-- 1 dpeet jrs  230026269 Jul  8 23:40 mfc-dmi-adapter-12.3.3-R1.img.zip
sum:
23159 2097152 mfc-dmi-adapter-12.3.3-R1.img
41353 224636 mfc-dmi-adapter-12.3.3-R1.img.zip
md5sum:
aca1ef619681a354ab5567171208ff88  mfc-dmi-adapter-12.3.3-R1.img
fe1c4e6179ef7cf8c5b0ba950e8019c2  mfc-dmi-adapter-12.3.3-R1.img.zip
sha1sum:
636ba25c8017796a60a99f5344e4f366e51ac993  mfc-dmi-adapter-12.3.3-R1.img
2d465b25d00ef1e8c6ecdc8ceea2befaf9a447b5  mfc-dmi-adapter-12.3.3-R1.img.zip
--------------------------------------------------------------------------------
