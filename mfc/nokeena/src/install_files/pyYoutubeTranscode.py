#####################################################################################
#
# Python script for Youtube SSP Transcode
#
#####################################################################################
		
import time
import sys
import os
#import pipes
import string
import urlparse
from urllib2 import Request, urlopen, URLError, HTTPError

#####################################################################################
#
# Check the available disk size
#
#####################################################################################
def publish_dir_size(path):

    disk = os.statvfs("/nkn")
    capacity = disk.f_bsize * disk.f_blocks
    available = disk.f_bsize * disk.f_bavail
    used = disk.f_bsize * (disk.f_blocks - disk.f_bavail)
    return  available,capacity,used;
#####################################################################################

#####################################################################################
#
# Parse the video file to fetch width, height and video bitrate information
#                    
#####################################################################################
def parse_video_information(inputpath,inputname,fileName):

    ffmpeg_parse_cmd='/opt/nkn/bin/ffmpeg -i ' +inputpath+inputname+' 2>'+inputpath+inputname+'.txt'
    print "ffmpeg_parse_cmd=",ffmpeg_parse_cmd,"\n"
    parsevalue=os.system(ffmpeg_parse_cmd)

    logfile_name = inputpath+inputname+'.txt'
#    print"logfile_name=",logfile_name,"\n"
    if os.path.isfile(logfile_name):
        logfile_size=os.path.getsize(logfile_name)
        if logfile_size>0:
            logfile_object = open(logfile_name, "r")
            logfile =logfile_object.readlines()
            for line in logfile:
#                print "line:",line,"\n"
                streamTagPos=line.find('Stream #')
                VideoTagPos = line.find('Video:')
                if streamTagPos != -1 and VideoTagPos != -1:
                    tempLine = line[VideoTagPos+6 : ]
                    word = tempLine.split(',')
                    bpsPar = word[3]
                    tempbpsPar =bpsPar.split(' ')
                    videobitrate =int (tempbpsPar[1])
                    widthheight = word[2]
                    resizeParPos=widthheight.find('[')
                    if resizeParPos != -1:
                        tempwidthheight  = widthheight.split('[')
                        temp2widthheight = tempwidthheight[0].split('x')
                        width  = temp2widthheight[0]
                        height = temp2widthheight[1]
                    else :
                        tempwidthheight = widthheight.split('x')
                        width  = tempwidthheight[0]
                        height = tempwidthheight[1]
                        #print("videoparameter=",videoparameter, "\n")
                        #    print("videobitrate_=",videobitrate_,"\n")
                        #    print("videobitrate=",videobitrate,"\n")
                        #    print("width=",width,"\n")
                        #    print("height=",height,"\n")
                    width_int=int(width)
                    height_int=int(height)
                    if width_int%2!=0:
                        width_int = width_int+1
                    if height_int%2!=0:
                        height_int = height_int+1
#                    print("width_int",width_int,"\n")
#                    print("height_int",height_int,"\n")
#                    print("videobitrate",videobitrate,"\n")
                    break
            logfile_object.close()
            return width_int,height_int,videobitrate
        else : #if we find the logfile is empty
            return 352,288,-200
    else :#if we cannot find the logfile
        return 352,288,-200 

#here we use return -200 for videobitrate if fail to parse the video information.
#In the transcode function, if we meet a -200 videobitrate, we will skip the transcode directly
#end of parse_video_information
##########################################################################################


##########################################################################################
#
# Generate the uri w/o seek query
# to remove the seek query from the uri
#
#########################################################################################

def generate_om_seek_uri(uriData,queryName):
#    print "\nstart of generate_om_seek_uri\n"
    queryName = "&"+queryName+"="

#    print "\nuriData=", uriData,"\n"
#    print "\nqueryName=", queryName,"\n"
    seekQueryPos=uriData.rfind(queryName)
#    print "\nseekQueryPos=", seekQueryPos,"\n"
    if seekQueryPos != -1: #uri contains the seek query
        lastQueryPos=uriData.rfind("&")
#        print "\nlastQueryPos=", lastQueryPos,"\n"
        if lastQueryPos >seekQueryPos: #seek query is not the last query
            uriData = uriData[:seekQueryPos] +uriData[lastQueryPos:]
        else :#seek query is the last query
            uriData = uriData[:seekQueryPos]

#    print "\nuriData=", uriData,"\n"
#    print "\nend of generate_om_seek_uri\n"
    return uriData
# generate_om_seek_uri end
#########################################################################################


#########################################################################################
#
# Fetch the file type and file name from the Youtube RemapUri
# This is corresponding to YoutubeSSP() in nkn_ssp_youtube.c
#
########################################################################################
def parseFileNameType(remapUriData):
#    print "\nstart of parseFileNameType\n"
#    print "\nremapUriData=", remapUriData,"\n"
    fmtPos = remapUriData.find('_fmt_')
    fileName = 'unknown'
    
    if fmtPos == -1:
        fileType = 'unknown'
    else :
        fileName = remapUriData.strip('/')

        fmt = int(remapUriData[fmtPos+5 : ])
        if fmt == 18:
            fileType = '.mp4'
        elif fmt == 22:
            fileType = '.mp4'
        elif fmt == 37:
            fileType = '.mp4'
        elif fmt == 5:
            fileType = '.flv'
        elif fmt == 34:
            fileType = '.flv'
        elif fmt == 35:
            fileType = '.flv'
        else :
            fileType = 'unknown'
            
#    print "\nfiletype=", fileType,"\n"
#    print "\nfileName=", fileName,"\n"
#    print "\nend of parseFileNameType\n"
    return fileName, fileType
#  parseFileNameType ends
#########################################################################################
        

########################################################################################
#
# Generate an Uri from RemapUri
# This Uri is used to fetch video from MFC cache to a local folder
#
#######################################################################################
def generate_uri_from_remapuri(remapUriData, video_id_uri_query, format_tag, nsuriPrefix):
    videoidPos = remapUriData.find('yt_video_id_') 
    fmtPos     = remapUriData.find('_fmt_')
    video_id   = remapUriData[videoidPos+12 : fmtPos]
    itag       = remapUriData[fmtPos+5 : ]
    print "\nvideo_id =", video_id, "\n"
    print "\nitag =", itag, "\n"
    print "\nvideo_id_uri_query=", video_id_uri_query, "\n"
    return nsuriPrefix + "?" + video_id_uri_query + "=" + video_id + "&" + format_tag + "=" + itag

#########################################################################################
#
# youtube  player  Transcode  mian routine
#
#########################################################################################

def youtubeSSPTranscode(hostData, hostLen, video_id_uri_query, video_id_uri_queryLen, format_tag, format_tagLen, remapUriData, remapUriLen, nsuriPrefix,nsuriPrefixLen, transRatio, transComplex):
# How about this uriData? It should be the newUri in the nkn_ssp_youtubeplayer.c
#    print ('python transcode plugin invoked')
	
    retval=100
    if os.path.isdir('/nkn/vpe/transcode'): 
	dirretval = 0
    else: #path is not exsit, we create the path
        dirretval=os.system('mkdir /nkn/vpe/transcode')
	
    if dirretval!=0:
        dirretval=-106
	
    permretval=os.system ('chmod 755 /nkn/vpe/transcode')
    if permretval!=0:
        permretval=-110

    #get the uri from remapped uri
    cache_fetch_uri = generate_uri_from_remapuri(remapUriData, video_id_uri_query, format_tag, nsuriPrefix)
    print "\ncache_fetch_uri =", cache_fetch_uri, "\n"
    #from the remapUriData to get the file type and file name
    fileName, fileType = parseFileNameType(remapUriData)
    
    if fileName == 'unknown':
	print "we face a video type, we do not know"
        return 4
    if fileType == 'unknown':
	print "we face a video type, we do not know"
        return 4
    
    inputpath='/nkn/vpe/transcode/' 
    outputpath='/nkn/vpe/transcode/' #the same folder

    inputname=fileName+fileType # this need to be changed for future
#    print "\ninputname =",inputname,"\n"
    
    ffmpegoutputname=fileName+'_temp.mp4' #this is the ffmpeg output file name
    #both of flv and mp4 file will transcode to mp4 file first
    #if orig file is flv, it will convert from mp4 to flv with ffmpeg
    #if orig filr is mp4, it will call MP4box to process it

    ffmpegoutputflv=fileName+'_temp.flv'#this is for flv only 
    
    outputname=fileName+'_lowrate'+fileType # this is the video file for inject
#    print "\noutputname=",outputname,"\n"
    
    #check the available spaces of the working dir
    avail,total,used=publish_dir_size('/nkn/vpe');
    checkval=(int(avail) * int(100)) / int(total);
    #print "\n Checkvalue=",checkval;
    if int(checkval) < 10:
        print "\nnot enough spaces\n"
        return 1;
    
#   timelog=open("/nkn/vpe/transcode/timelog.dat",'w')
#    if timelog:
#       print '\n'
#    else:
#        print '\nCannot open timelog file\n'
	
    Totalstime=time.time()


    videofileurl='http://127.0.0.1'+cache_fetch_uri
#    print "\nvideofileurl=",videofileurl,"\n"

    print "\nwait for 10s\n"
    time.sleep(10)
    #we will need to decide whether to trigger the entire transcoder
    if 0:#os.path.isfile(inputpath+inputname):
        #this block should be replaced by a  process pool in future with python multiprocess package
        print "\nThere is already the downrated version there( maybe finished or is transcoding)\n"
        print "\nThe transcoding task of this video is already triggerd\n"
        print "\nno more transcoding for this file\n"
#        timelog.close()
        return 1
    else:
        print "\nfetch the Youtube  video\n"
        #######################################
        # Start to fetch the Youtube video
        # The youtbe video is already in the cache.
        # Because only hot video would trigger the transcode.
        fetchstime=time.time()
        req = Request(videofileurl)
        req.add_header('Host', hostData)
	try:
	    response = urlopen(req)
	except HTTPError, e:
            return 2
	except URLError, e:
	    return 2
        else:
            content_len = response.headers["Content-Length"]
            #print response.info()
            
        fetchcmd='wget --no-http-keep-alive -q -S --header="Host:'+hostData+'" "'+videofileurl+'" -O '+inputpath+inputname
        print "fetchcmd=", fetchcmd,"\n"
	
        videofetchvalue=os.system(fetchcmd)
	print "videofetchvalue=", videofetchvalue, "\n"
        if videofetchvalue!=0:
            print "\nfail to fetch video\n"
            if os.path.isfile(inputpath+inputname):
                os.remove(inputpath+inputname)
#            timelog.close()
            return 2
        
        # check whether the readfile size is the same as content_len
	print "\ncontent_len =", content_len
        filesize_wget = os.path.getsize(inputpath+inputname)
        print "\nfilesize_wget =", filesize_wget
        if int(filesize_wget) != int(content_len):
	    print "\nfail to fetch full video\n"
	    if os.path.isfile(inputpath+inputname):
		os.remove(inputpath+inputname)
	    return 2
            
        fetchetime=time.time()
        fetchtime=fetchetime-fetchstime
#        print >>timelog,"\nfetchtime     =",fetchtime,"\n"

        ########################################
        #will call the ffmpeg to parse the resolution and bitrate of the fetched file
        #1. get the bitrate
        #2. get the width and height 
        #
        #######################################
        parsestime=time.time()
        width,height,videobitrate=parse_video_information(inputpath,inputname,fileName)
#        print "original video rate=",videobitrate,"\n"
        videobitrate = int(transRatio)*int(videobitrate)/100*1000 #from kbps to bps, and also multipe the percentage ratio
#        print "downrate video rate=",videobitrate,"\n"
        if videobitrate<0:
            print "fail to parse the video\n"
            if os.path.isfile(inputpath+inputname):
            	os.remove(inputpath+inputname)
            if os.path.isfile(inputpath+inputname+'.txt'):
                os.remove(inputpath+inputname+'.txt')
#            timelog.close()
            return 3 #skip transcoding

        videobitrate = str(videobitrate)
        width = str(width)
        height = str(height)
        parseetime=time.time()
        parsetime =parseetime-parsestime
#        print >>timelog,"\nparsetime     =",parsetime,"\n"
        
        #######################################
        # call the ffmpeg to do transcode
        # 
        # the ffpreset need use absolute path
        transcodestime=time.time()
        transComplex=int(transComplex)
        if fileType!=".MP4" and fileType!=".mp4" and fileType!=".FLV" and fileType!=".flv":
            print "This is not MP4 or FLV"
            if os.path.isfile(inputpath+inputname):
            	os.remove(inputpath+inputname)
            if os.path.isfile(inputpath+inputname+'.txt'):
            	os.remove(inputpath+inputname+'.txt')
            return 4

        if transComplex==1: # 1-pass encoding
            print "1-pass encoding\n"
            ffmpegcmd='/opt/nkn/bin/ffmpeg -i ' +inputpath+inputname+ ' -acodec copy -vcodec libx264 -fpre /opt/nkn/bin/ffpresets/libx264-fast.ffpreset -fpre /opt/nkn/bin/ffpresets/libx264-main.ffpreset -vf scale='+width+':'+height+' -b '+videobitrate+' -threads 1 '+outputpath+ffmpegoutputname
            print "ffmpegcmd=",ffmpegcmd,"\n"
            transcodevalue=os.system(ffmpegcmd)
            if transcodevalue!=0:
                print"\nfail to transcode video\n"
            	if os.path.isfile(inputpath+inputname):
                    os.remove(inputpath+inputname)
            	if os.path.isfile(inputpath+inputname+'.txt'):
                    os.remove(inputpath+inputname+'.txt')
            	if os.path.isfile(outputpath+ffmpegoutputname):
                    os.remove(outputpath+ffmpegoutputname)
#                timelog.close()
                return 5
        else: # 2-pass encoding not supported
            print "2-pass encoding"
            ffmpegcmd='/opt/nkn/bin/ffmpeg -i ' +inputpath+inputname+ ' -acodec copy -vcodec libx264 -fpre /opt/nkn/bin/ffpresets/libx264-fast.ffpreset -fpre /opt/nkn/bin/ffpresets/libx264-main.ffpreset -vf scale='+width+':'+height+' -b '+videobitrate+' -threads 1 '+outputpath+ffmpegoutputname
            print "ffmpegcmd=",ffmpegcmd,"\n"
            transcodevalue=os.system(ffmpegcmd)
            if transcodevalue!=0:
                print"\nfail to transcode video\n"
                if os.path.isfile(inputpath+inputname):
                    os.remove(inputpath+inputname)
                if os.path.isfile(inputpath+inputname+'.txt'):
                    os.remove(inputpath+inputname+'.txt')
                if os.path.isfile(outputpath+ffmpegoutputname):
                    os.remove(outputpath+ffmpegoutputname)
#                timelog.close()
                return 5

        transcodeetime=time.time()
        transcodetime=transcodeetime-transcodestime
#        print >>timelog,"\ntranscode time=",transcodetime,"\n"

        print "n\wait for 10s\n"
        time.sleep(10)

        ###################################################
        #call ffmpeg to do file contain convert
        #This routine is executed if the original file is flv
        #
        #if the original file is flv, we call ffmpeg to downrate the video and we have to change the container to MP4
        #if we do not change the container(input flv, output flv),ffmpeg will copy the metadata of the input flv direct to output flv
        #This will cause some player cannot do the seek on the output flv.
        #
        #So we need to change the contianer to MP4, then it will drop the FLV metadata.
        #Then we do another simple format convert to change MP4 back to FLV
        if fileType==".flv" or fileType==".FLV":
            Formatstime=time.time()
            Formatcmd='/opt/nkn/bin/ffmpeg -i '+outputpath+ffmpegoutputname+' -acodec copy -vcodec copy -f flv '+outputpath+ffmpegoutputflv
            print "Formatcmd=",Formatcmd,"\n"
            Formatvalue=os.system(Formatcmd)
            if Formatvalue!=0:
                print"\nfail to convert from Mp4 to FLV\n"
                if os.path.isfile(inputpath+inputname):
                	os.remove(inputpath+inputname)
                if os.path.isfile(inputpath+inputname+'.txt'):
                	os.remove(inputpath+inputname+'.txt')
                if os.path.isfile(outputpath+ffmpegoutputname):
                	os.remove(outputpath+ffmpegoutputname)
                if os.path.isfile(outputpath+ffmpegoutputflv):
                	os.remove(outputpath+ffmpegoutputflv)
#                timelog.close()
                return 6

            Formatetime=time.time()
            Formattime=Formatetime-Formatstime
#            print >>timelog,"\nFmt conv time =",Formattime,"\n"

        ##########################################################
        #call yamdi to inject metadata into onmetadata in FLV file
        #
        #the yamdi will inject several metadata into onmetadata.
        #some are necessary for youtube, some are not.
        #
        #yamdi also cannot inject some youtube metadata.
        #
        #So we will call yt_flv_meta_mod to remove some unnecessary metadata
        #and inject some necessary metadata
        #########################################################
        if fileType==".flv" or fileType==".FLV":
            yamdistime=time.time()
            yamdicmd='/opt/nkn/bin/yamdi -i '+outputpath+ffmpegoutputflv+' -o '+outputpath+outputname+'.yamdi'
            print "yamdicmd=",yamdicmd,"\n"
            yamdivalue=os.system(yamdicmd)
            if yamdivalue!=0:
                print"\nfail to inject keyframe metadata\n"
		if os.path.isfile(inputpath+inputname):
    			os.remove(inputpath+inputname)
        	if os.path.isfile(inputpath+inputname+'.txt'):
                	os.remove(inputpath+inputname+'.txt')
                if os.path.isfile(outputpath+ffmpegoutputname):
                        os.remove(outputpath+ffmpegoutputname)
                if os.path.isfile(outputpath+ffmpegoutputflv):
                	os.remove(outputpath+ffmpegoutputflv)
                if os.path.isfile(outputpath+outputname+'.yamdi'):
                	os.remove(outputpath+outputname+'.yamdi')
#                timelog.close()
                return 6
            yamdietime=time.time()
            yamditime =yamdietime -yamdistime
#            print >>timelog,"\nmetadata inject time=",yamditime,"\n"
        
            metamodstime=time.time()
            metamodcmd='/opt/nkn/bin/yt_flv_meta_mod ' + outputpath+outputname+'.yamdi' + ' ' + outputpath+outputname
            print "metamodcmd=",metamodcmd,"\n"
            metamodvalue=os.system(metamodcmd)
            if metamodvalue!=0:
        	print"\nfail to mod the youtube metadata\n"
                if os.path.isfile(inputpath+inputname):
                    	os.remove(inputpath+inputname)
                if os.path.isfile(inputpath+inputname+'.txt'):
                	os.remove(inputpath+inputname+'.txt')
                if os.path.isfile(outputpath+ffmpegoutputname):
                	os.remove(outputpath+ffmpegoutputname)
                if os.path.isfile(outputpath+ffmpegoutputflv):
                	os.remove(outputpath+ffmpegoutputflv)
                if os.path.isfile(outputpath+outputname+'.yamdi'):
                	os.remove(outputpath+outputname+'.yamdi')
                if os.path.isfile(outputpath+outputname):
                	os.remove(outputpath+outputname)
#        	timelog.close()
            	return 6
            metamodetime=time.time()
            metamodtime =metamodetime -metamodstime
#            print >>timelog,"\nyoutube metadata mod time=",metamodtime,"\n"
                                                                                                
        ##########################################################################
        #call MP4box to process the MP4 file
        #MP4 file generated by FFMPEG will put the moov box at the end of the file
        #for http streaming, we should put the moov box at the beginning of the file
        #so we need the MP4box to do this
	#
        #youtube need a udta box for seek. so we need to call yt_mp4_udta to inject 
        #this box.
        #
        #############################################################################
        if fileType==".MP4" or fileType==".mp4":
            MP4Boxstime=time.time()
            MP4Boxcmd='/opt/nkn/bin/MP4Box -add '+outputpath+ffmpegoutputname+' '+outputpath+outputname+'.mp4box'
            print "MP4Boxcmd=",MP4Boxcmd,"\n"
            MP4Boxvalue=os.system(MP4Boxcmd)
            if MP4Boxvalue!=0:
                print"\nfail to MP4Box the mp4 file\n"
                if os.path.isfile(inputpath+inputname):
                    	os.remove(inputpath+inputname)
                if os.path.isfile(inputpath+inputname+'.txt'):
                	os.remove(inputpath+inputname+'.txt')
                if os.path.isfile(outputpath+ffmpegoutputname):
                	os.remove(outputpath+ffmpegoutputname)
                if os.path.isfile(outputpath+outputname+'.mp4box'):
                	os.remove(outputpath+outputname+'.mp4box')                                                
#                timelog.close()
                return 6

            MP4Boxetime=time.time()
            MP4Boxtime=MP4Boxetime-MP4Boxstime
#            print >>timelog,"\nMp4Box time=",MP4Boxtime,"\n"

            mp4udtastime=time.time()
            mp4udtacmd='/opt/nkn/bin/yt_mp4_udta '+outputpath+outputname+'.mp4box'+' '+outputpath+outputname
            print "MP4Boxcmd=",mp4udtacmd,"\n"
            mp4udtavalue=os.system(mp4udtacmd)
            if mp4udtavalue!=0:
                print"\nfail to inject udta into mp4 file\n"
                if os.path.isfile(inputpath+inputname):
                    	os.remove(inputpath+inputname)
                if os.path.isfile(inputpath+inputname+'.txt'):
                	os.remove(inputpath+inputname+'.txt')
                if os.path.isfile(outputpath+ffmpegoutputname):
                	os.remove(outputpath+ffmpegoutputname)
                if os.path.isfile(outputpath+outputname+'.mp4box'):
                	os.remove(outputpath+outputname+'.mp4box')
                if os.path.isfile(outputpath+outputname):
                	os.remove(outputpath+outputname)                                                        
#                timelog.close()
                return 6

            mp4udtaetime=time.time()
            mp4udtatime=mp4udtaetime-mp4udtastime
#            print >>timelog,"\nMp4Box time=",mp4udtatime,"\n"

        ###########################################
        #call the cache inject script
        #we will inject the reduce rate video file into cache
        #youtube in cache
        #   v1.lscache3.c.youtube.com:80/yt_video_id_86cf090ef0f4cdc2_fmt_34
        #we want to inject
        #   v1.lscache3.c.youtube.com:80/yt_video_id_86cf090ef0f4cdc2_fmt_34_lowrate

        #remapUriData (internal cache name of the orig video)is '/yt_video_id_66d016f7e37291f8_fmt_34'
        #the lowrate to inject is                            '/video'+'_yt_id_'+'66d016f7e37291f8_fmt_34'+'_lowrate'
        injectUri = nsuriPrefix + '_yt_id_' + remapUriData[13 : ]  + '_lowrate'

        injectcmd='/opt/nkn/bin/cache_add.sh -D '+hostData+' -H '+hostData+' '+outputpath+outputname+' '+injectUri
        print "\ninjectcmd= ",injectcmd,"\n"
        injectstime=time.time()
        injectvalue=os.system(injectcmd)
        if injectvalue!=0:
            print "\nfail to inject into cache\n"
            if os.path.isfile(inputpath+inputname):
            	os.remove(inputpath+inputname)
            if os.path.isfile(inputpath+inputname+'.txt'):
                os.remove(inputpath+inputname+'.txt')
            if os.path.isfile(outputpath+ffmpegoutputname):
            	os.remove(outputpath+ffmpegoutputname)
            if os.path.isfile(outputpath+outputname+'.mp4box'):
            	os.remove(outputpath+outputname+'.mp4box')
            if os.path.isfile(outputpath+outputname):
            	os.remove(outputpath+outputname)
            if os.path.isfile(outputpath+ffmpegoutputflv):
            	os.remove(outputpath+ffmpegoutputflv)
            if os.path.isfile(outputpath+outputname+'.yamdi'):
            	os.remove(outputpath+outputname+'.yamdi')
            if os.path.isfile(outputpath+outputname):
            	os.remove(outputpath+outputname)
#            timelog.close()
            return 7

        injectetime=time.time()
        injecttime=injectetime-injectstime
#        print >>timelog,"\ninject time  =",injecttime,"\n"        
        Totaletime=time.time()
        Totaltime=Totaletime-Totalstime
#        print >>timelog,"\nTotaltime    =",Totaltime,"\n"

        #os.system(injectcmd) cannot lock for running the shell script
        #As a result,we must sleep several secs here
	time.sleep(20) 

	if os.path.isfile(inputpath+inputname):
        	os.remove(inputpath+inputname)
        if os.path.isfile(inputpath+inputname+'.txt'):
        	os.remove(inputpath+inputname+'.txt')
        if os.path.isfile(outputpath+ffmpegoutputname):
        	os.remove(outputpath+ffmpegoutputname)
        if os.path.isfile(outputpath+outputname+'.mp4box'):
        	os.remove(outputpath+outputname+'.mp4box')
        if os.path.isfile(outputpath+outputname):
        	os.remove(outputpath+outputname)
        if os.path.isfile(outputpath+ffmpegoutputflv):
        	os.remove(outputpath+ffmpegoutputflv)
        if os.path.isfile(outputpath+outputname+'.yamdi'):
        	os.remove(outputpath+outputname+'.yamdi')
        if os.path.isfile(outputpath+outputname):
        	os.remove(outputpath+outputname)
                                                                                                                                                                
#    timelog.close()
    return 8
# Transcode_main function ends
###########################################################################################



if __name__ == "__main__":
#   print "argv len=",len(sys.argv),"\n"
#   for i in range(len(sys.argv)):
#       print "argv[", i, "]=",sys.argv[i],"\n"
#   print "start process\n"
   
   hostData              = sys.argv[1]
#   print "hostData",hostData, "\n"
   hostLen               = sys.argv[2]
   video_id_uri_query    = sys.argv[3]
   video_id_uri_queryLen = sys.argv[4]
   format_tag            = sys.argv[5]
   format_tagLen         = sys.argv[6]
   remapUriData          = sys.argv[7]
   remapUriLen           = sys.argv[8]
   nsuriPrefix           = sys.argv[9]
   nsuriPrefixLen        = sys.argv[10]
   transRatio            = sys.argv[11]
   transComplex          = sys.argv[12]
   youtubeSSPTranscode(hostData, hostLen, video_id_uri_query, video_id_uri_queryLen, format_tag, format_tagLen, remapUriData, remapUriLen, nsuriPrefix,nsuriPrefixLen, transRatio, transComplex)
