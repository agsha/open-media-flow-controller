/* This is a generated file, don't edit */

#define NUM_APPLETS 243

const char applet_names[] ALIGN1 = ""
"[" "\0"
"[[" "\0"
"arp" "\0"
"arping" "\0"
"ash" "\0"
"awk" "\0"
"base64" "\0"
"basename" "\0"
"bbconfig" "\0"
"blkid" "\0"
"blockdev" "\0"
"brctl" "\0"
"cal" "\0"
"cat" "\0"
"chgrp" "\0"
"chmod" "\0"
"chown" "\0"
"chroot" "\0"
"chrt" "\0"
"chvt" "\0"
"cksum" "\0"
"clear" "\0"
"cmp" "\0"
"comm" "\0"
"cp" "\0"
"cttyhack" "\0"
"cut" "\0"
"date" "\0"
"dc" "\0"
"dd" "\0"
"deallocvt" "\0"
"depmod" "\0"
"devmem" "\0"
"df" "\0"
"diff" "\0"
"dirname" "\0"
"dmesg" "\0"
"dnsdomainname" "\0"
"dos2unix" "\0"
"du" "\0"
"dumpkmap" "\0"
"echo" "\0"
"ed" "\0"
"egrep" "\0"
"eject" "\0"
"env" "\0"
"expand" "\0"
"expr" "\0"
"false" "\0"
"fbsplash" "\0"
"fdformat" "\0"
"fdisk" "\0"
"fgconsole" "\0"
"fgrep" "\0"
"find" "\0"
"findfs" "\0"
"flock" "\0"
"fold" "\0"
"free" "\0"
"freeramdisk" "\0"
"fsync" "\0"
"fuser" "\0"
"getopt" "\0"
"getty" "\0"
"grep" "\0"
"groups" "\0"
"gunzip" "\0"
"gzip" "\0"
"halt" "\0"
"hd" "\0"
"hdparm" "\0"
"head" "\0"
"hexdump" "\0"
"hostid" "\0"
"hostname" "\0"
"hwclock" "\0"
"id" "\0"
"ifconfig" "\0"
"ifdown" "\0"
"ifenslave" "\0"
"ifup" "\0"
"init" "\0"
"insmod" "\0"
"install" "\0"
"ionice" "\0"
"iostat" "\0"
"ip" "\0"
"ipaddr" "\0"
"ipcalc" "\0"
"ipcrm" "\0"
"ipcs" "\0"
"iplink" "\0"
"iproute" "\0"
"iprule" "\0"
"kbd_mode" "\0"
"kill" "\0"
"killall" "\0"
"killall5" "\0"
"klogd" "\0"
"last" "\0"
"less" "\0"
"linux32" "\0"
"linux64" "\0"
"linuxrc" "\0"
"ln" "\0"
"loadfont" "\0"
"loadkmap" "\0"
"logger" "\0"
"login" "\0"
"logname" "\0"
"logread" "\0"
"losetup" "\0"
"ls" "\0"
"lsmod" "\0"
"lsof" "\0"
"lspci" "\0"
"lsusb" "\0"
"makedevs" "\0"
"md5sum" "\0"
"mdev" "\0"
"mesg" "\0"
"mkdir" "\0"
"mkfifo" "\0"
"mknod" "\0"
"mkswap" "\0"
"mktemp" "\0"
"modinfo" "\0"
"modprobe" "\0"
"more" "\0"
"mount" "\0"
"mountpoint" "\0"
"mpstat" "\0"
"mv" "\0"
"nameif" "\0"
"netstat" "\0"
"nice" "\0"
"nohup" "\0"
"nslookup" "\0"
"ntpd" "\0"
"od" "\0"
"openvt" "\0"
"patch" "\0"
"pgrep" "\0"
"pidof" "\0"
"ping" "\0"
"ping6" "\0"
"pivot_root" "\0"
"pkill" "\0"
"pmap" "\0"
"poweroff" "\0"
"printenv" "\0"
"printf" "\0"
"ps" "\0"
"pstree" "\0"
"pwd" "\0"
"pwdx" "\0"
"rdate" "\0"
"rdev" "\0"
"readahead" "\0"
"readlink" "\0"
"realpath" "\0"
"reboot" "\0"
"renice" "\0"
"reset" "\0"
"resize" "\0"
"rev" "\0"
"rm" "\0"
"rmdir" "\0"
"rmmod" "\0"
"route" "\0"
"run-parts" "\0"
"runlevel" "\0"
"script" "\0"
"sed" "\0"
"seq" "\0"
"setarch" "\0"
"setconsole" "\0"
"setfont" "\0"
"setkeycodes" "\0"
"setlogcons" "\0"
"setserial" "\0"
"setsid" "\0"
"sh" "\0"
"sha1sum" "\0"
"sha256sum" "\0"
"sha512sum" "\0"
"showkey" "\0"
"sleep" "\0"
"sort" "\0"
"split" "\0"
"stat" "\0"
"strings" "\0"
"stty" "\0"
"su" "\0"
"sulogin" "\0"
"sum" "\0"
"swapoff" "\0"
"swapon" "\0"
"switch_root" "\0"
"sync" "\0"
"sysctl" "\0"
"syslogd" "\0"
"tac" "\0"
"tail" "\0"
"tee" "\0"
"telnet" "\0"
"test" "\0"
"tftp" "\0"
"time" "\0"
"timeout" "\0"
"top" "\0"
"touch" "\0"
"tr" "\0"
"traceroute" "\0"
"traceroute6" "\0"
"true" "\0"
"tty" "\0"
"udhcpc" "\0"
"udhcpc6" "\0"
"umount" "\0"
"uname" "\0"
"unexpand" "\0"
"uniq" "\0"
"unix2dos" "\0"
"uptime" "\0"
"users" "\0"
"usleep" "\0"
"uudecode" "\0"
"uuencode" "\0"
"vconfig" "\0"
"vi" "\0"
"volname" "\0"
"wall" "\0"
"watch" "\0"
"wc" "\0"
"wget" "\0"
"which" "\0"
"who" "\0"
"whoami" "\0"
"xargs" "\0"
"yes" "\0"
"zcat" "\0"
"zcip" "\0"
;

#ifndef SKIP_applet_main
int (*const applet_main[])(int argc, char **argv) = {
test_main,
test_main,
arp_main,
arping_main,
ash_main,
awk_main,
base64_main,
basename_main,
bbconfig_main,
blkid_main,
blockdev_main,
brctl_main,
cal_main,
cat_main,
chgrp_main,
chmod_main,
chown_main,
chroot_main,
chrt_main,
chvt_main,
cksum_main,
clear_main,
cmp_main,
comm_main,
cp_main,
cttyhack_main,
cut_main,
date_main,
dc_main,
dd_main,
deallocvt_main,
depmod_main,
devmem_main,
df_main,
diff_main,
dirname_main,
dmesg_main,
hostname_main,
dos2unix_main,
du_main,
dumpkmap_main,
echo_main,
ed_main,
grep_main,
eject_main,
env_main,
expand_main,
expr_main,
false_main,
fbsplash_main,
fdformat_main,
fdisk_main,
fgconsole_main,
grep_main,
find_main,
findfs_main,
flock_main,
fold_main,
free_main,
freeramdisk_main,
fsync_main,
fuser_main,
getopt_main,
getty_main,
grep_main,
id_main,
gunzip_main,
gzip_main,
halt_main,
hexdump_main,
hdparm_main,
head_main,
hexdump_main,
hostid_main,
hostname_main,
hwclock_main,
id_main,
ifconfig_main,
ifupdown_main,
ifenslave_main,
ifupdown_main,
init_main,
insmod_main,
install_main,
ionice_main,
iostat_main,
ip_main,
ipaddr_main,
ipcalc_main,
ipcrm_main,
ipcs_main,
iplink_main,
iproute_main,
iprule_main,
kbd_mode_main,
kill_main,
kill_main,
kill_main,
klogd_main,
last_main,
less_main,
setarch_main,
setarch_main,
init_main,
ln_main,
loadfont_main,
loadkmap_main,
logger_main,
login_main,
logname_main,
logread_main,
losetup_main,
ls_main,
lsmod_main,
lsof_main,
lspci_main,
lsusb_main,
makedevs_main,
md5_sha1_sum_main,
mdev_main,
mesg_main,
mkdir_main,
mkfifo_main,
mknod_main,
mkswap_main,
mktemp_main,
modinfo_main,
modprobe_main,
more_main,
mount_main,
mountpoint_main,
mpstat_main,
mv_main,
nameif_main,
netstat_main,
nice_main,
nohup_main,
nslookup_main,
ntpd_main,
od_main,
openvt_main,
patch_main,
pgrep_main,
pidof_main,
ping_main,
ping6_main,
pivot_root_main,
pgrep_main,
pmap_main,
halt_main,
printenv_main,
printf_main,
ps_main,
pstree_main,
pwd_main,
pwdx_main,
rdate_main,
rdev_main,
readahead_main,
readlink_main,
realpath_main,
halt_main,
renice_main,
reset_main,
resize_main,
rev_main,
rm_main,
rmdir_main,
rmmod_main,
route_main,
run_parts_main,
runlevel_main,
script_main,
sed_main,
seq_main,
setarch_main,
setconsole_main,
setfont_main,
setkeycodes_main,
setlogcons_main,
setserial_main,
setsid_main,
ash_main,
md5_sha1_sum_main,
md5_sha1_sum_main,
md5_sha1_sum_main,
showkey_main,
sleep_main,
sort_main,
split_main,
stat_main,
strings_main,
stty_main,
su_main,
sulogin_main,
sum_main,
swap_on_off_main,
swap_on_off_main,
switch_root_main,
sync_main,
sysctl_main,
syslogd_main,
tac_main,
tail_main,
tee_main,
telnet_main,
test_main,
tftp_main,
time_main,
timeout_main,
top_main,
touch_main,
tr_main,
traceroute_main,
traceroute6_main,
true_main,
tty_main,
udhcpc_main,
udhcpc6_main,
umount_main,
uname_main,
expand_main,
uniq_main,
dos2unix_main,
uptime_main,
who_main,
usleep_main,
uudecode_main,
uuencode_main,
vconfig_main,
vi_main,
volname_main,
wall_main,
watch_main,
wc_main,
wget_main,
which_main,
who_main,
whoami_main,
xargs_main,
yes_main,
gunzip_main,
zcip_main,
};
#endif

const uint16_t applet_nameofs[] ALIGN2 = {
0x0000,
0x0002,
0x0005,
0x0009,
0x0010,
0x0014,
0x0018,
0x001f,
0x0028,
0x0031,
0x0037,
0x0040,
0x0046,
0x004a,
0x004e,
0x0054,
0x005a,
0x0060,
0x0067,
0x006c,
0x0071,
0x0077,
0x007d,
0x0081,
0x0086,
0x0089,
0x0092,
0x0096,
0x009b,
0x009e,
0x00a1,
0x00ab,
0x00b2,
0x00b9,
0x00bc,
0x00c1,
0x00c9,
0x00cf,
0x00dd,
0x00e6,
0x00e9,
0x00f2,
0x00f7,
0x00fa,
0x0100,
0x0106,
0x010a,
0x0111,
0x0116,
0x011c,
0x0125,
0x012e,
0x0134,
0x013e,
0x0144,
0x4149,
0x0150,
0x0156,
0x015b,
0x0160,
0x016c,
0x0172,
0x0178,
0x017f,
0x0185,
0x018a,
0x0191,
0x0198,
0x019d,
0x01a2,
0x01a5,
0x01ac,
0x01b1,
0x01b9,
0x01c0,
0x01c9,
0x01d1,
0x01d4,
0x01dd,
0x01e4,
0x01ee,
0x01f3,
0x01f8,
0x01ff,
0x0207,
0x020e,
0x0215,
0x0218,
0x021f,
0x0226,
0x022c,
0x0231,
0x0238,
0x0240,
0x0247,
0x0250,
0x0255,
0x025d,
0x0266,
0x026c,
0x0271,
0x0276,
0x027e,
0x0286,
0x028e,
0x0291,
0x029a,
0x02a3,
0x82aa,
0x02b0,
0x02b8,
0x02c0,
0x02c8,
0x02cb,
0x02d1,
0x02d6,
0x02dc,
0x02e2,
0x02eb,
0x02f2,
0x02f7,
0x02fc,
0x0302,
0x0309,
0x030f,
0x0316,
0x031d,
0x0325,
0x032e,
0x4333,
0x0339,
0x0344,
0x034b,
0x034e,
0x0355,
0x035d,
0x0362,
0x0368,
0x0371,
0x0376,
0x0379,
0x0380,
0x0386,
0x038c,
0x4392,
0x4397,
0x039d,
0x03a8,
0x03ae,
0x03b3,
0x03bc,
0x03c5,
0x03cc,
0x03cf,
0x03d6,
0x03da,
0x03df,
0x03e5,
0x03ea,
0x03f4,
0x03fd,
0x0406,
0x040d,
0x0414,
0x041a,
0x0421,
0x0425,
0x0428,
0x042e,
0x0434,
0x043a,
0x0444,
0x044d,
0x0454,
0x0458,
0x045c,
0x0464,
0x046f,
0x0477,
0x0483,
0x048e,
0x0498,
0x049f,
0x04a2,
0x04aa,
0x04b4,
0x04be,
0x04c6,
0x04cc,
0x04d1,
0x04d7,
0x04dc,
0x04e4,
0x84e9,
0x04ec,
0x04f4,
0x04f8,
0x0500,
0x0507,
0x0513,
0x0518,
0x051f,
0x0527,
0x052b,
0x0530,
0x0534,
0x053b,
0x0540,
0x0545,
0x054a,
0x0552,
0x0556,
0x055c,
0x455f,
0x456a,
0x0576,
0x057b,
0x057f,
0x0586,
0x058e,
0x0595,
0x059b,
0x05a4,
0x05a9,
0x05b2,
0x05b9,
0x05bf,
0x05c6,
0x05cf,
0x05d8,
0x05e0,
0x05e3,
0x85eb,
0x05f0,
0x05f6,
0x05f9,
0x05fe,
0x0604,
0x0608,
0x060f,
0x0615,
0x0619,
0x061e,
};

const uint8_t applet_install_loc[] ALIGN1 = {
0x33,
0x32,
0x31,
0x31,
0x21,
0x42,
0x13,
0x11,
0x41,
0x33,
0x33,
0x33,
0x11,
0x13,
0x13,
0x23,
0x12,
0x33,
0x11,
0x33,
0x11,
0x11,
0x33,
0x33,
0x21,
0x23,
0x13,
0x23,
0x33,
0x23,
0x31,
0x21,
0x31,
0x11,
0x32,
0x32,
0x33,
0x21,
0x23,
0x22,
0x22,
0x32,
0x11,
0x11,
0x31,
0x13,
0x11,
0x13,
0x33,
0x32,
0x13,
0x01,
0x41,
0x32,
0x31,
0x22,
0x21,
0x33,
0x23,
0x23,
0x13,
0x13,
0x12,
0x22,
0x11,
0x11,
0x21,
0x11,
0x33,
0x34,
0x33,
0x13,
0x11,
0x32,
0x23,
0x31,
0x31,
0x31,
0x44,
0x33,
0x23,
0x33,
0x13,
0x11,
0x22,
0x21,
0x13,
0x13,
0x42,
0x43,
0x31,
0x31,
0x33,
0x13,
0x33,
0x31,
0x11,
0x32,
0x22,
0x12,
0x22,
0x33,
0x33,
0x33,
0x33,
0x13,
0x33,
0x13,
0x23,
0x13,
0x31,
0x33,
0x33,
0x31,
0x23,
0x31,
0x13,
0x33,
0x33,
0x33,
0x13,
0x02,
};

#define MAX_APPLET_NAME_LEN 13
