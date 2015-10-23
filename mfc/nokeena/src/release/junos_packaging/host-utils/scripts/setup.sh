:
# RCSid:
#	$Id: setup.sh,v 1.6.306.1 2009-08-07 16:10:09 ssiano Exp $

#	Common setup for host-utils/scripts/*
#	We handle command line settings here so that the various
#	scripts can have a more common style, and be easier to augment
#	as new needs arrise.
#

. $PKGVARTOPDIR/pkgvars

Mydir=${Mydir:-`dirname $0`}
Myname=${Myname:-`basename $0 .sh`}

source_rc() {
    for r in $*
    do
	[ -s $r ] && . $r
    done
}

error() {
        echo "ERROR: $Myname: $@" >&2
        Exit 1
}

warn() {
        echo "WARNING: $Myname: $@" >&2
}

inform() {
        echo "NOTICE: $Myname: $@" >&2
}

Exit() {
    ExitStatus=$1
    exit $1
}

Which() {
    case "$1" in
    -?) _t="$1"; shift;;
    *) _t=-x;;
    esac
    case "$1" in
    /*) test $_t $1 && echo $1;;
    *)
        for _d in `IFS=:; echo ${2:-$PATH}`
        do
            test $_t $_d/$1 && { echo $_d/$1; break; }
        done
        ;;
    esac
}

find_manifest() {
    m=${1:-${MANIFEST:-$PKG.manifest}}
    [ -s $m ] && { echo $m; return; }
    Which -s $m ${2:-$MANIFEST_PATH}
}
    
# some defaults
MAKE=/volume/buildtools/bin/mk
TAR=tar
MAKEISO=$Mydir/makeiso
MAKEMAN=$Mydir/makeman
MAKETAR=$Mydir/maketar
ADD_OPTION_BLOCK=$Mydir/add-option-block
MAKE_INSTALL_FLOPPY=$Mydir/make-install-floppy
STRIP_FILES=$Mydir/strip-files
SIGNATURE_BEGIN="-----BEGIN JUNOS SIGNATURE-----"
SIGNATURE_END="-----END JUNOS SIGNATURE-----"
# clear the vars we are going to default below, so they cannot be
# comming from the environment, we can't set defauls for them here
# because we don't yet know the value of HOST_OBJTOP
GZIPVN=
MKISOFS=
GENPART=
HOST_OBJTOP=
OBJTOPS=
OBJTOP=
SRCTOP=
PKGSCRIPTS=
PKGSCRIPTSOBJ=
MFORMAT=
MPARTITION=

# pickup the basic settings from release/Makefile
source_rc ${HOST_UTILS_ENV_FILE:-./.host-utils.env}

# This allows us to handle vars containing spaces.
eval_override() {
    local var val

    eval `IFS==; set -- $*; unset IFS; var=$1; shift; val="$@"; echo "var=$var val='$val' $var='$val'"`
    OVERRIDES="$OVERRIDES $var='$val'"
}

# we need to eval $OVERRIDES for them to be passed on correctly
run_overrides() {
    local cmd

    cmd=$1; shift
    eval $cmd $OVERRIDES "$@"
}

require_vars() {
    local var val
    
    for var in $*
    do
        eval val="\$$var"
        case "$val" in
	"") inform "need values for each of: $*"
	    error "no value for $var";;
	esac
    done
}

case ",$DEBUG_SH," in
*,$Myname,*) set -x;;
esac

# now allow VAR=val on the command line to override
while :
do
    case "$1" in
    -env) source_rc $2; shift 2;;
    *=*) eval_override "$1"; shift;;
    *)   break;;
    esac
done

HASH=${HASH:-${PKG_HASH:-sha256}}
eval HASH_CMD=${HASH_CMD:-$`echo $HASH | tr 'a-z' 'A-Z'`}

hash_file() {
    if [ $# = 0 ]; then
        $HASH_CMD
    elif [ -s $1.$HASH ]; then
	cat $1.$HASH
    else
        $HASH_CMD < $1
    fi
}

HOST_OBJTOP=${HOST_OBJTOP:-$OBJTOP}
PKGSCRIPTS_PATH=${PKGSCRIPTS_PATH:-$CURDIR/../package-scripts:$SRCTOP_JUNOS/package-scripts}
PKGSCRIPTSOBJ=${PKGSCRIPTSOBJ:-$_OBJDIR/../package-scripts}
MANIFEST_PATH=${MANIFEST_PATH:-$_OBJDIR:$CURDIR:$SRCTOP_JUNOS/release}
HOST_OBJTOP_JUNOS=${HOST_OBJTOP_JUNOS:-$HOST_OBJTOP/junos}

# some commonly used utils
GZIPVN=${GZIPVN:-$HOST_OBJTOP_JUNOS/host-utils/gzipvn/gzipvn}
MKISOFS=${MKISOFS:-$HOST_OBJTOP_JUNOS/host-utils/mkisofs/mkisofs}
GENPART=${GENPART:-$HOST_OBJTOP_JUNOS/host-utils/genpart/genpart}
PKG_CREATE=${PKG_CREATE:-$HOST_OBJTOP_JUNOS/host-utils/pkg_create/pkg_create}

OBJTOPS=${OBJTOPS:-${OBJTOP}:${_OBJTOP}}

case ./$0 in
*/setup.sh) # unit test
    echo $0 $OVERRIDES "$@"
    echo "MAKE='$MAKE' FOO='$FOO' OVERRIDES=<$OVERRIDES>"
    require_vars MAKE FOO
    $SKIP run_overrides $0 SKIP=: "$@"
    exit 0
    ;;
esac
