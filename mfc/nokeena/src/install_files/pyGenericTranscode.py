#####################################################################################
#
# Python script for Generic SSP Transcode
#
#####################################################################################
		
import time
import sys
import os
#import pipes
import string
import urlparse

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
# Parse the video file, Fetch width, height and video bitrate information
#
#####################################################################################
def parse_video_information(inputpath,inputname,fileName):

    ffmpeg_parse_cmd='/opt/nkn/bin/ffmpeg -i ' +inputpath+inputname+' 2>'+inputpath+inputname+'.txt'
#    print "ffmpeg_parse_cmd=",ffmpeg_parse_cmd,"\n"
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
###############################################################################################



###############################################################################################
#
# Generic Player  Transcode  main routine
#
###############################################################################################

def genericSSPTranscode(hostData, hostLen, uriData, uriLen, containerType, transRatio,transComplex):
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
#If the uriData is /dnet/media/2011/1/25/b9c2fa18-736b-485e-b058-212ab6f58a71_2.mp4?xxxxx
#with uriWoQuery=uriData[:uriData.rfind('?')]
#we can have uriWoQuery /dnet/media/2011/1/25/b9c2fa18-736b-485e-b058-212ab6f58a71_2.mp4
#but if uriData="/dnet/media/2011/1/25/b9c2fa18-736b-485e-b058-212ab6f58a71_2.mp4"
#with uriWoQuery=uriData[:uriData.rfind('?')]
#we will have uriWoQuery /dnet/media/2011/1/25/b9c2fa18-736b-485e-b058-212ab6f58a71_2.mp
#to avoid this, we will add '_' after uriData, then both situation, we will have the corret
#uriWoQuery

    #transcode path and transcoder input file name
#    print "\nuriData=", uriData,"\n"
    uriData=uriData+'_'

    uriWoQuery=uriData[:uriData.rfind('?')] #uri without query parameters
#    print "\nuriWoQuery     =",uriWoQuery,"\n"
    
    uriWoQuery=uriWoQuery.rstrip('\r\n\t ')
#    print "\nstripuriWoQuery=",uriWoQuery,"\n"

    splitUriWoQuery=uriWoQuery.split('/')
#    print "\nspliUriWoQuery=",splitUriWoQuery,"\n"
    
    lensofsplitUriWoQuery=len(splitUriWoQuery)
#    print "\nlensofsplitUriWoQuery=",lensofsplitUriWoQuery,"\n"
    
    fileUri=splitUriWoQuery[lensofsplitUriWoQuery-1]
#    print "\nfileUri=",fileUri,"\n"
    
    fileName = fileUri[:fileUri.rfind('.')] #the file name
#    print "\nfielName=",fileName,"\n"
    
#    fileType = fileUri[fileUri.rfind('.'):]  #file type
    fileType ='.' + containerType #fix the irregular file extension bug
    
    print "\nfielType=",fileType,"\n"
    #need modify hter
    
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
#    timelog=open("/nkn/vpe/transcode/timelog.dat",'w')
#    if timelog:
#        print '\n'
#    else:
#        print '\nCannot open timelog file\n'
	
    Totalstime=time.time()


    videofileurl='http://127.0.0.1'+uriWoQuery
#    print "\nvideofileurl=",videofileurl,"\n"
#The youtube video name in cache is like this
#v12.lscache4.c.youtube.com:80/yt_video_id_e0446bbc62297df9_fmt_34
#                             /this part is the uriData
#    print "\nwait for 10s\n"
    time.sleep(10)
    #we will need to decide whether to trigger the entire transcoder
    if 0:#os.path.isfile(inputpath+inputname):
        print "\nThere is already the downrated version there( maybe finished or is transcoding)\n"
        print "\nThe transcoding task of this video is already triggerd\n"
        print "\nno more transcoding for this file\n"
#        timelog.close()
        return 1
    else:
        print "\nfetch the break  video\n"
        #######################################
        # Start to fetch the break video
        # The youtbe video is already in the cache.
        # Because only hot video would trigger the transcode.
        fetchstime=time.time()
        fetchcmd='wget --no-http-keep-alive -q -S --header="Host:'+hostData+'" "'+videofileurl+'" -O '+inputpath+inputname
#        print "fetchcmd=", fetchcmd,"\n"
	
        videofetchvalue=os.system(fetchcmd)
        if videofetchvalue!=0:
            print "\nfail to fetch video\n"
            if os.path.isfile(inputpath+inputname):
               os.remove(inputpath+inputname)
#            timelog.close()
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
        # just for demo, so the bitrate is fixed at 150000bp
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
#            print "ffmpegcmd=",ffmpegcmd,"\n"
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
        else: # 2-pass encoding
            print "2-pass encoding"
            ffmpegcmd='/opt/nkn/bin/ffmpeg -i ' +inputpath+inputname+ ' -acodec copy -vcodec libx264 -fpre /opt/nkn/bin/ffpresets/libx264-fast.ffpreset -fpre /opt/nkn/bin/ffpresets/libx264-main.ffpreset -vf scale='+width+':'+height+' -b '+videobitrate+' -threads 1 '+outputpath+ffmpegoutputname
#            print "ffmpegcmd=",ffmpegcmd,"\n"
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
#            print "Formatcmd=",Formatcmd,"\n"
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
        #call yamdi to inject the keyframe metadata into onmetadata in FLV file
        #
        #it is manditary as FFMPEG do not inject keyframe metadata
        #Keyframe metadata is necessary for seek
        if fileType==".flv" or fileType==".FLV":
            yamdistime=time.time()
            yamdicmd='/opt/nkn/bin/yamdi -i '+outputpath+ffmpegoutputflv+' -o '+outputpath+outputname
#            print "yamdicmd=",yamdicmd,"\n"
            yamdivalue=os.system(yamdicmd)
            if yamdivalue!=0:
                print"\nfail to inject keyframe metadata\n"
#                timelog.close()
                if os.path.isfile(inputpath+inputname):
                   os.remove(inputpath+inputname)
                if os.path.isfile(inputpath+inputname+'.txt'):
                   os.remove(inputpath+inputname+'.txt')
                if os.path.isfile(outputpath+ffmpegoutputname):
                   os.remove(outputpath+ffmpegoutputname)
                if os.path.isfile(outputpath+ffmpegoutputflv):
                   os.remove(outputpath+ffmpegoutputflv)
                if os.path.isfile(outputpath+outputname):
                   os.remove(outputpath+outputname)
                return 6
            yamdietime=time.time()
            yamditime =yamdietime -yamdistime
#            print >>timelog,"\nmetadata inject time=",yamditime,"\n"
        
            
        ###############################################
        #call MP4box to process the MP4 file
        #MP4 file generated by FFMPEG will put the moov box at the end of the file
        #for http streaming, we should put the moov box at the beginning of the file
        #so we need the MP4box to do this
        if fileType==".MP4" or fileType==".mp4":
            MP4Boxstime=time.time()
            MP4Boxcmd='/opt/nkn/bin/MP4Box -add '+outputpath+ffmpegoutputname+' '+outputpath+outputname
#            print "MP4Boxcmd=",MP4Boxcmd,"\n"
            MP4Boxvalue=os.system(MP4Boxcmd)
            if MP4Boxvalue!=0:
                print"\nfail to MP4Box the mp4 file\n"
#                timelog.close()
                if os.path.isfile(inputpath+inputname):
                   os.remove(inputpath+inputname)
                if os.path.isfile(inputpath+inputname+'.txt'):
                   os.remove(inputpath+inputname+'.txt')
                if os.path.isfile(outputpath+ffmpegoutputname):
                   os.remove(outputpath+ffmpegoutputname)
                if os.path.isfile(outputpath+outputname):
                   os.remove(outputpath+outputname)
                return 6

            MP4Boxetime=time.time()
            MP4Boxtime=MP4Boxetime-MP4Boxstime
#            print >>timelog,"\nMp4Box time=",MP4Boxtime,"\n"

        ###########################################
        #call the cache inject script
        #we will inject the reducedrate video file into cache
        injectUri=uriWoQuery[:uriWoQuery.rfind('.')]+'_lowrate'+fileType

        injectcmd='/opt/nkn/bin/cache_add.sh -D '+hostData+' -H '+hostData+' '+outputpath+outputname+' '+injectUri
#        print "\ninjectcmd= ",injectcmd,"\n"
        injectstime=time.time()
        injectvalue=os.system(injectcmd)
        if injectvalue!=0:
            print "\nfail to inject into cache\n"
#            timelog.close()
            if os.path.isfile(inputpath+inputname):
               os.remove(inputpath+inputname)
            if os.path.isfile(inputpath+inputname+'.txt'):
               os.remove(inputpath+inputname+'.txt')
            if os.path.isfile(outputpath+ffmpegoutputname):
               os.remove(outputpath+ffmpegoutputname)
            if os.path.isfile(outputpath+ffmpegoutputflv):
               os.remove(outputpath+ffmpegoutputflv)
            if os.path.isfile(outputpath+outputname):
               os.remove(outputpath+outputname)
            return 7

        injectetime=time.time()
        injecttime=injectetime-injectstime
#        print >>timelog,"\ninject time  =",injecttime,"\n"
        
        Totaletime=time.time()
        Totaltime=Totaletime-Totalstime

        time.sleep(20) # Must sleep for several secs to wait for the injection finished
        #It looks like python GLI cannot lock the shell script
        if os.path.isfile(inputpath+inputname):
           os.remove(inputpath+inputname)
        if os.path.isfile(inputpath+inputname+'.txt'):
           os.remove(inputpath+inputname+'.txt')
        if os.path.isfile(outputpath+ffmpegoutputname):
           os.remove(outputpath+ffmpegoutputname)
        if os.path.isfile(outputpath+ffmpegoutputflv):
           os.remove(outputpath+ffmpegoutputflv)
        if os.path.isfile(outputpath+outputname):
           os.remove(outputpath+outputname)

#        print >>timelog,"\nTotaltime    =",Totaltime,"\n"
        
#    timelog.close()
    return 8
# Transcode_main function ends
########################################################################################################





if __name__ == "__main__":

   hostData     =  sys.argv[1]
   hostLen      =  sys.argv[2]
   uriData      =  sys.argv[3]
   uriLen       =  sys.argv[4]
   containerType = sys.argv[5]
   transRatio   =  sys.argv[6]
   transComplex =  sys.argv[7]
   genericSSPTranscode(hostData, hostLen, uriData, uriLen, containerType, transRatio,transComplex)



