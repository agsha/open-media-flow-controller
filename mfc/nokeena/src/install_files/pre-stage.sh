#!/bin/bash 

NKNFQUEUE="/opt/nkn/bin/nknfqueue "
INGEST_DIR="/tmp/ftp"
INGEST_FILE="$INGEST_DIR/ingest.list"
TMP_MEDIA_DIR="/tmp/ftp/content"
FMGR="/tmp/FMGR.queue"
LOG="/var/log/nkn/ingest.log"

RM="rm -f"
MV=mv
CP=cp
MKDIR=mkdir
TOUCH=touch


[ ! -d $INGEST_DIR ] && $MKDIR $INGEST_DIR; 
[ ! -f $INGEST_FILE ] && $TOUCH $INGEST_FILE;
[ ! -d $TMP_MEDIA_DIR ] && $MKDIR $TMP_MEDIA_DIR;
[ ! -f $LOG ] && $TOUCH $LOG;


####
log()
{
    [ $debug ] && echo $1 
    return 1;
}

ingest_log()
{
    echo "[`date +\"%d/%b/%Y:%H:%m:%S %z\"`] [$USER] [$1] [$2]" >> $LOG;
    return 1;
}

get_tmp_file()
{
    suffix=`date +%y%m%d_%H%M%S%N`;
    fname="/tmp/ftp/ftp_"$USER"_"$suffix
}

get_media_files()
{
    new_files=`ls $1/*.mp4 $1/*.flv --hide=dead* --hide=.* > $INGEST_DIR/.tmp_list`;

    log $new_files
    ## Uniquify out from whatever has been  ingested
    ingested=`cat $INGEST_FILE`;

    diffs=`diff  $INGEST_DIR/.tmp_list $INGEST_FILE | grep "^<" | awk '//{print $2 "\n";}'`;
    new_files=`cat $INGEST_DIR/.tmp_list`;
    log $diffs ;

    #remove the tmp file
    $RM $INGEST_DIR/.tmp_list;

    for file in $diffs
    do
        echo $file >> $INGEST_FILE

        #echo ${file%.*}.meta
        if [ -f  ${file%.*}.meta ] 
        then
            log "Meta file found for $file"
            slurp ${file%.*}.meta
        else
            slurp_with_default $file
        fi
    done
}


slurp()
{
    [ ! -f $1 ] && return -1;
    media=${1%.*}
    exts="mp4 flv qss"
    found=0
    # check if any of the valid extensions exist
    for i in $exts
    do
        if [ -f $media.$i ]
        then
            found=1;
            break;
        fi
    done

    [ $found ] && log "File to slurp -- $1";
    [ ! $found ] && return -1;

    $CP $media.$i $TMP_MEDIA_DIR;
    META=$1
    #CONTENT=${META%meta}.$i
    local METADATA=`cat $META`
    for j in $METADATA ; do
        #echo $i;
        local header=`echo | awk '
        {
            split("'"${j}"'", a, "=")
            print a[1]
        }
        '`
        local data=`echo | awk '
        {
            split("'"${j}"'", a, "=")
            print a[2]
        }
        '`

        if [ $header == "URI" ] ; then
            local _uri=$data
        else
            local attr_args+=" -H $header:$data"
        fi
    done

    # get the filename - this needs to be appended to the uri
    local file=${media#$HOME\/*}.$i

    local size=`wc -m $TMP_MEDIA_DIR/$file | awk '{print $1;}'`;
    attr_args+=" -H Content-Length:$size"
    log $file
    get_tmp_file;
    local outfile=$fname

    ret=1;
    while [ $ret -ne 0 ]
    do
        $NKNFQUEUE -i $outfile -p $TMP_MEDIA_DIR/$file -u $_uri/$file $attr_args
        ret=$?
        if [ $ret -ne 0 ]; then
            sleep 1
        fi
    done

    ret=1
    while [ $ret -ne 0 ]
    do
        $NKNFQUEUE -e $outfile -q $FMGR
        ret=$?
        if [ $ret -ne 0 ]; then
            sleep 1
        fi
    done
}

slurp_with_default()
{
    [ ! -f $1 ] && return -1;

    media=${1%.*};
    exts="mp4 flv qss"
    found=0
    # check if any of the valid extensions exist
    for i in $exts
    do
        if [ -f $media.$i ]
        then
            found=1;
            break;
        fi
    done

    [ ! $found ] && return -1;
    # move content to tmp location so that we dont pollute the 
    # user dir
    [ $found ] && log "File to slurp -- $media.$i" 
    
    $CP $media.$i $TMP_MEDIA_DIR;

    local _uri=`mdreq -v query get - /nkn/nvsd/pre_stage/ftpd/default/url`;
    local template="/nkn/nvsd/pre_stage/ftpd/default/attribute";
    local hdrs=`mdreq -v query iterate enter-pass $template`;

    for header in $hdrs
    do
        value=`mdreq -v query iterate enter-pass $template/$header `;
        local attr_args+=" -H $header:$value"
    done

    # get the filename - this needs to be appended to the uri
    local file=${media#$HOME\/*}.$i
    local size=`wc -m $TMP_MEDIA_DIR/$file | awk '{print $1;}'`;
    attr_args+=" -H Content-Length:$size"
    get_tmp_file;
    local outfile=$fname
    #echo "$NKNFQUEUE -i $outfile $attr_args -p $TMP_MEDIA_DIR/$file -u $_uri/$file";


    ret=1;
    while [ $ret -ne 0 ]
    do
        $NKNFQUEUE -i $outfile -p $TMP_MEDIA_DIR/$file -u $_uri/$file $attr_args
        ret=$?
        if [ $ret -ne 0 ]; then
            sleep 1
        fi
    done
    
    ret=1
    while [ $ret -ne 0 ]
    do
        $NKNFQUEUE -e $outfile -q $FMGR
        ret=$?
        if [ $ret -ne 0 ]; then
            sleep 1
        fi
    done

    ingest_log "ingest" "$_uri/$file" ;
    #echo "$NKNFQUEUE -i $outfile $attr_args -p $TMP_MEDIA_DIR/$file -u $_uri/$file"
    #echo "$NKNFQUEUE -e $outfile -q $FMGR"

}

####





    
get_media_files $HOME

