#!/usr/bin/perl

#
#


sub usage()
{
    print "usage: $0 convert_all PACKAGE_DIR OUTPUT_FILE [CONFIG_FILE ...]\n";
    print "usage: $0 convert_each PACKAGE_DIR OUTPUT_DIR [CONFIG_FILE]\n";
    print "usage: $0 convert_single PACKAGE_NAME OUTPUT_DIR [CONFIG_FILE]\n";
    print "usage: $0 extract_single PACKAGE_NAME OUTPUT_DIR [CONFIG_FILE]\n";
    print "usage: $0 extract_files PACKAGE_NAME OUTPUT_DIR FILE [FILE ...]\n";
    print "\n";
    print "convert_all converts a directory of rpms to tbz's\n";
    print "convert_each converts a directory of rpms to a dir of tbz's\n";
    print "convert_single converts a single rpm to a single tbz\n";
    print "extract_single dumps a single rpm to a directory\n";
    print "extract_files dumps files from a single rpm to a directory\n";
    print "\n";
    print "Optional flags:\n";
    print "    -r : override root required.  Will result in wrong extracted file meta\n";
    print "         information.\n";
    print "    -b : ignore base excludes.  Useful if you want all files from an RPM\n";
    print "         extracted.\n";
    print "    -s STRIP_PROG : specify 'strip' program instead of /usr/bin/strip . Useful\n";
    print "         for cross-build situations.\n";
    exit(1);
}

if ($#ARGV < 2) {
    usage();
}

umask 0;

$PACKAGE_DIR="";
$PACKAGE_FILE="";
$WORK_DIR="/tmp/unrpm-wd.$$"; 
$REMOVE_WORK_DIR=0;
@CONFIG_FILES=();
$STRIP_BINARY="/usr/bin/strip";
$EXEC_COMMAND="";
$ROOT_REQUIRED = 1;
$USE_BASE_EXCLUDES = 1;

while (@ARGV[0] =~ /-/) {
    if (@ARGV[0] =~ /-r/) {
        $ROOT_REQUIRED=0;
    }
    elsif (@ARGV[0] =~ /-b/) {
        $USE_BASE_EXCLUDES=0;
    }
    elsif (@ARGV[0] =~ /-s/) {
        $STRIP_BINARY="@ARGV[1]";
        shift @ARGV;
    }
    shift @ARGV;
}
$STRIP_COMMAND="${STRIP_BINARY} -g";

$CONVERT_TYPE="@ARGV[0]";  # convert_each, convert_all, none
if ($CONVERT_TYPE eq "convert_all") {
    $PACKAGE_DIR="@ARGV[1]";
    $OUTPUT_FILE="@ARGV[2]";
    $REMOVE_WORK_DIR=1;
    if ($#ARGV >= 3) {
        @CONFIG_FILES=@ARGV[3 .. $#ARGV];
    }
}
elsif ($CONVERT_TYPE eq "convert_each") {
    $PACKAGE_DIR="@ARGV[1]";
    $OUTPUT_DIR="@ARGV[2]";
    mkdir ${OUTPUT_DIR}, 0755 || die "Could not create ${OUTPUT_DIR}";
    $REMOVE_WORK_DIR=1;
    if ($#ARGV >= 3) {
        @CONFIG_FILES=@ARGV[3 .. $#ARGV];
    }
}
elsif ($CONVERT_TYPE eq "convert_single") {
    $PACKAGE_FILE="@ARGV[1]";
    ${OUTPUT_DIR}="@ARGV[2]";
    mkdir ${OUTPUT_DIR}, 0755 || die "Could not create ${OUTPUT_DIR}";
    $REMOVE_WORK_DIR=1;
    if ($#ARGV >= 3) {
        @CONFIG_FILES=@ARGV[3 .. $#ARGV];
    }
}
elsif ($CONVERT_TYPE eq "extract_single") {
    $PACKAGE_FILE="@ARGV[1]";
    ${OUTPUT_DIR}="@ARGV[2]";
    mkdir ${OUTPUT_DIR}, 0755 || die "Could not create ${OUTPUT_DIR}";
    $REMOVE_WORK_DIR=0;
    if ($#ARGV >= 3) {
        @CONFIG_FILES=@ARGV[3 .. $#ARGV];
    }
}
elsif ($CONVERT_TYPE eq "extract_files") {
    $PACKAGE_FILE="@ARGV[1]";
    ${OUTPUT_DIR}="@ARGV[2]";
    mkdir ${OUTPUT_DIR}, 0755 || die "Could not create ${OUTPUT_DIR}";
    $REMOVE_WORK_DIR=0;
    shift @ARGV;
    shift @ARGV;
    shift @ARGV;
    @extract_includes=@ARGV;
}
else {
    usage();
}

# Check EUID if required
if ($ROOT_REQUIRED && $> != 0) {
    die "ERROR: must be run as root"
}

if ($REMOVE_WORK_DIR == 1) {
    `rm -rf ${WORK_DIR}`;
    mkdir ${WORK_DIR}, 0755 || die "Could not create ${WORK_DIR}";
}

if ($CONVERT_TYPE eq "convert_all" || $CONVERT_TYPE eq "convert_each") {
    @rpm_list=glob("${PACKAGE_DIR}/*.rpm");
}
else {
    @rpm_list=($PACKAGE_FILE);
}

# In case the user doesn't have a config file, get rid of some common things
if (${USE_BASE_EXCLUDES} != 0) {
    @base_excludes=('/usr/share/doc/', '/usr/share/man/', '/usr/share/info/',
                '/usr/share/locale/', '/usr/man/', '/usr/doc/', 
                '/usr/include/', '/etc/sysconfig/', '/etc/X11/', 
                '/usr/share/misc/', '/usr/lib/pkgconfig/');
}
else {
    @base_excludes=();
}
# Read in their includes and excludes from the config file
foreach $config_file (@CONFIG_FILES) {
    eval `cat $config_file`; die $@ if $@;
} 

foreach $urp (@rpm_list) {
    chomp $urp;

##    print "rpm: $urp\n";
    if (! -f "$urp") {
        die "ERROR: file did not exist or is not a file: $urp\n";
    }
    
    ($fbn = $urp) =~ s,^.*/([^/]*-[^/-]*-[^/-]*).rpm$,$1,;
    if (($fbn =~ /\//) || ("$fbn" eq "")) {
        die "ERROR: invalid package fullname for: $urp\n";
    }

    ($pbn = $urp) =~ s,^.*/([^/]*)-([^/-]*)-([^/-]*)\.([^/\.-]*)\.rpm$,$1,;
    $pver = $2;
    $prel = $3;
    $parch = $4;

    # Most packages follow the "name-version-release.arch.rpm" naming
    # convention (matched above).  The name may have a "-" in it, but
    # the other parts may not.  However, if we get a package name that
    # does not have an "arch", handle that too.  We still require at
    # least "name-version-release.rpm" .  This too could be relaxed if
    # needed, as currently we do not use $pver or $prel below.

    if (($pbn =~ /\//) || ("$pbn" eq "")) {
        ($pbn = $urp) =~ s,^.*/([^/]*)-([^/-]*)-([^/-]*)\.rpm$,$1,;
        $pver = $2;
        $prel = $3;
        $parch = $4;

        if (($pbn =~ /\//) || ("$pbn" eq "")) {
            die "ERROR: invalid package basename for: $urp\n";
        }
    }

    # Make the package name safe for use below
    ($pbn = $pbn) =~ s/[-+\.]/_/g;
    ($parch = $parch) =~ s/[-+\.]/_/g;

##    print "fbn: $fbn\n";
##    print "parch: $parch\n";
##    print "pbn: $pbn\n";

    @package_includes=();
    @package_excludes=();
    @package_strips=();
    @package_execs=();
    $package_nobaseexcludes=0;
    if ($CONVERT_TYPE ne "extract_files") {
        if ("${parch}" ne "") {
            @package_includes=eval "\@arch_${parch}_includes_${pbn}"; die $@ if $@;
        }
        if (scalar(@package_includes) == 0) {
            @package_includes=eval "\@includes_${pbn}"; die $@ if $@;
        }
        if ("${parch}" ne "") {
            @package_excludes=eval "\@arch_${parch}_excludes_${pbn}"; die $@ if $@;
        }
        if (scalar(@package_excludes) == 0) {
            @package_excludes=eval "\@excludes_${pbn}"; die $@ if $@;
        }
        if ("${parch}" ne "") {
            @package_strips=eval "\@arch_${parch}_strips_${pbn}"; die $@ if $@;
        }
        if (scalar(@package_strips) == 0) {
            @package_strips=eval "\@strips_${pbn}"; die $@ if $@;
        }
        if ("${parch}" ne "") {
            @package_execs=eval "\@arch_${parch}_execs_${pbn}"; die $@ if $@;
        }
        if (scalar(@package_execs) == 0) {
            @package_execs=eval "\@execs_${pbn}"; die $@ if $@;
        }
        if ("${parch}" ne "") {
            $package_nobaseexcludes=eval "\$arch_${parch}_nobaseexcludes_${pbn}"; die $@ if $@;
        }
        if ($package_nobaseexcludes == "") {
            $package_nobaseexcludes=eval "\$nobaseexcludes_${pbn}"; die $@ if $@;
        }
    }
    else {
        @package_includes=@extract_includes;
    }
    if (scalar(@package_includes) && scalar(@package_excludes)) {
        printf("WARNING: Ignoring excludes for package $fbn\n");
        @package_excludes = ();
    }

    # Get the list of files in the rpm, so we can see if they start with ./
    $file_list_command="rpm2cpio $urp | cpio --quiet -t";
    $file_list=`${file_list_command}`;
    @file_list=split(/\n/, ${file_list});

    $file_prefix="-";
    foreach $fn (@file_list) {
        ##print "fn: .a.${fn}.b.\n";
        if ($fn =~ /^\.\//) {
            $new_prefix="./";
        }
        elsif ($fn =~ /^\//) {
            $new_prefix="/";
        }
        else {
            $new_prefix="";
        }
    
        if ($new_prefix ne $file_prefix) {
            if ($file_prefix ne "-") {
                die "Invalid file prefixes found: ${new_prefix} and ${file_prefix} on file ${fn}";
            }
            $file_prefix = $new_prefix;
        }
    }
    #print "FINAL file prefix: .a.${file_prefix}.b.\n";


    $patterns="";
    if (scalar(@package_includes)) {

        foreach $excl (@package_includes) {
            # First, remove any leading ./ or / , then add cpio prefix
            $excl =~ s/^\///;
            $excl =~ s/^\.\///;
            $excl = "${file_prefix}${excl}";

            # Directory
            if ($excl =~ /\/$/) {
                $patterns="${patterns} ${excl}\*";
            }
            # File
            else {
                $patterns="${patterns} ${excl}";
            }
        }
    }
    else {
        if ("$package_nobaseexcludes" ne "1") {
            @package_base_excludes = @base_excludes;
        }
        else {
            @package_base_excludes = ();
        }
        foreach $excl (@package_base_excludes, @package_excludes) {
            # First, remove any leading ./ or / , then add cpio prefix
            $excl =~ s/^\///;
            $excl =~ s/^\.\///;
            $excl = "${file_prefix}${excl}";

            # Directory
            if ($excl =~ /\/$/) {
                $patterns="${patterns} ${excl}\*";
            }
            # File
            else {
                $patterns="${patterns} ${excl}";
            }
        }
    }

##    print "pat: ${patterns}\n";

    $cpio_cmd="cpio --extract --make-directories --no-absolute-filenames --preserve-modification-time --unconditional ";

## --verbose

    if (!scalar(@package_includes)) {
        $cpio_cmd .= "--nonmatching";
    }
    $cpio_cmd .= "${patterns}";

##    print "cc: ${cpio_cmd}\n";

    if ($CONVERT_TYPE eq "convert_all" || $CONVERT_TYPE eq "convert_each" ||
        $CONVERT_TYPE eq "convert_single") {
        $unrpm_dir=$WORK_DIR;
    }
    else {
        $unrpm_dir=$OUTPUT_DIR;
    }
    if ($CONVERT_TYPE eq "convert_each") {
        $unrpm_dir .= "/${pbn}";
    }
    mkdir($unrpm_dir, 0755);

    do_exec("rpm2cpio ".$urp." | (cd $unrpm_dir; ".$cpio_cmd.") 2>&1") || 
        die "CPIO failed";

    # On extract, cpio makes parent dirs not mentioned in the cpio with
    # mode 777 (our umask is 0 on purpose so that the file perms will be
    # correct).  We make any such mode 777 directory be mode 755, and
    # then the rpm's permissions can change this as needed.
    do_exec("find $unrpm_dir -type d -perm 777 -print0 | xargs --no-run-if-empty -0 chmod 755");

    # XXX this means user/group names on the build system have to have the right #s!

    open (GETPERMS, 'rpm --queryformat \'[%{FILEMODES} %{FILEUSERNAME} %{FILEGROUPNAME} %{FILENAMES}\n]\' -qp '.$urp.' |');
    while (<GETPERMS>) {
        chomp;
        my ($mode, $owner, $group, $file) = split(/ /, $_, 4);
        $mode = $mode & 07777; # remove filetype
        my $uid = getpwnam($owner);
        if (! defined $uid) {
            $uid=0;
        }
        my $gid = getgrnam($group);
        if (! defined $gid) {
            $gid=0;
        }

        next unless -e "$unrpm_dir/$file"; # skip broken links
        if (-l "$unrpm_dir/$file") { next; }; # skip symlinks

        chown $uid, $gid, "$unrpm_dir/$file"
            || die "failed chowning $file to $uid\:$gid\: $!";
        chmod $mode, "$unrpm_dir/$file"
            || die "failed changing mode of $file to $mode\: $!";
    }

    if (scalar(@package_strips)) {
        foreach $stripl (@package_strips) {
            if ($stripl =~ /^\//) {
                $stripl = ".${stripl}";
            }
            do_exec("${STRIP_COMMAND} ${unrpm_dir}/$stripl");
        }
    }

    if (scalar(@package_execs)) {
        foreach $execl (@package_execs) {
            do_exec("(cd ${unrpm_dir}; $execl)");
        }
    }

    if ($CONVERT_TYPE eq "convert_each" || $CONVERT_TYPE eq "convert_single") {
        $ofn="$OUTPUT_DIR/${fbn}.tar";
        do_convert($unrpm_dir, "$ofn") || 
            die "Convert failed for $unrpm_dir";
##        print "output: $ofn\n";
    }
}

if ($CONVERT_TYPE eq "convert_all") {
    $unrpm_dir=$WORK_DIR;
    $ofn="$OUTPUT_FILE";
    do_convert($unrpm_dir, "$ofn") || 
        die "Convert failed for $unrpm_dir";
##    print "output: $ofn\n";
}

if ($REMOVE_WORK_DIR == 1) {
    `rm -rf ${WORK_DIR}`
}

sub do_convert {
    my $wdir = shift;
    my $of = shift;

    do_exec("rm -f $of");
    do_exec("(cd $wdir; tar -cpf $of .)") || return 1;
}

sub do_exec {
    my @command=@_;
    my $pid=fork;

    if (!$pid) {
        open(STDOUT, ">/dev/null");
        exec(@command);
        exit 1;
    }
    else {
        return (waitpid($pid, 0) > 0);
    }
}
