#Python Publishing Module '''
		
import time
import sys
import os
#import pipes
import string
import urlparse

#print 'publish Enter';

################################################################

# Publish Main

################################################################

def publish_init(hostData, hostLen, uriData, uriLen, stateQuery, stateQueryLen):
    
    #print ('python plugin invoked');
    
    retval=100;
    
    dirretval=os.system('mkdir /nkn/vpe');

    if dirretval!=0:
	dirretval=-106;

    permretval=os.system('chmod 755 /nkn/vpe/');

    if permretval!=0:
	permretval=-110;
   
    #print "\nState=",stateQuery;
    
    queryparam=stateQuery.strip(' \n\r\t');

    timelog=open("/nkn/vpe/timelog.dat",'w');
    
    if timelog:
	print '\n';
    else:
	print '\nCannot open a Timelog file\n';

    Totalstime=time.time();
	
    #print('\n\n\n');
    #print "\nUri orig Data ", uriData;

    uriPortion = uriData[:uriData.rfind('?')];
    metauriPortion = uriPortion[:uriPortion.rfind('.')];
    metauriPortion=metauriPortion+'.dat';

    #print "\nHost Data ", hostData;
    #print "\nUri  Data ", metauriPortion;
    #print "\nHost length =",hostLen;
    #print "\nUri Length =",uriLen;

    dirretval=os.system('rm -rf /nkn/vpe/in');

    if dirretval!=0:
	dirretval=-106;

    retval,sizewarflag=publish_start(hostData,hostLen,uriData,uriLen,metauriPortion,queryparam,timelog);

    #os.system('rm -rf /nkn/vpe/in');
    #os.system('rm -rf /nkn/vpe/out'); 

    Totaletime=time.time();
   
# Time taken by publishing module
 
    Totaltime=Totaletime-Totalstime;
    
    print >>timelog,"\nTotal Time taken in seconds= ",Totaltime;	

    retval=publish_errorcode(retval,dirretval,permretval,sizewarflag);
    
    return retval;    

################################################################

def publish_errorcode(retval,dirretval,permretval,sizewarflag):

    if dirretval!=0:
        if retval==100:
                retval=-1;
        else:
                retval=dirretval;
	return retval;

    if permretval!=0:
        if retval==100:
                retval=int(retval)+int(permretval);
        else:
                retval=permretval;
    	return retval;	

    if sizewarflag==1:
	if retval!=100:
		retval=-103;	
	else:
		retval=-112;
	return retval;

    return retval;	

################################################################

def publish_dir_size(path):

	disk = os.statvfs("/nkn");
	
#	diskSpace=disk;

	capacity = disk.f_bsize * disk.f_blocks;
	
	available = disk.f_bsize * disk.f_bavail;
	
	used = disk.f_bsize * (disk.f_blocks - disk.f_bavail);

	#print"\nAvailable free=",available;

	#print"\nTotal size =",capacity;

	#print"\nDisk usage size=",used;
	
	return	available,capacity,used;

################################################################


def publish_start(hdata,hlen,udata,ulen,uriportion,queryparam,timelog):

	timemetast=time.time();
	
#	Create directory 

	dirretval=os.system('mkdir /nkn/vpe');
	
	if dirretval!=0:
		retval=-106;
	
	dirretval=os.system('mkdir /nkn/vpe/in');
	
	if dirretval!=0:
		retval=-106;
	
	inputpath='/nkn/vpe/in';
	
	#availsize=os.path.getsize(inputpath);

	sizewarflag=0;

	avail,total,used=publish_dir_size('/nkn/vpe');


	checkval=(int(avail) * int(100)) / int(total);

	#print "\n Checkvalue=",checkval;

	if int(checkval) < 10:
		sizewarflag=1;

	#Get Publisher meta file

	metaportion=uriportion;
	tmp=[];
	uriportion=uriportion.rstrip('\r\n\t ');
	metaportion=uriportion.split('.');

	#print "\nuriportion=",uriportion;

	#if tmp.endswith('.flv'):
	#	metaportion=tmp.strip('.');
	#	metapotion=metaportion+'_meta.dat';
	#	print "\nmetaportion=",metaportion;	
	#else:
	#	metaportion=uriportion;

	metadat=metaportion[0];#+'_meta.dat';
	metadataurl='http://127.0.0.1'+uriportion+'?no-cache=1&'+queryparam+'=5';
	metafilename=hdata+uriportion;
	tmp=uriportion[uriportion.rfind('/'):];
	tmp2=tmp.split('.');
	metadataname=tmp2[0];#+'_meta.dat';        
	filenameout=tmp2[0];
	metadataname=tmp2[0]+'.dat';        
	#print "\nmetadataurl=",metadataurl;
	cmd='wget --no-http-keep-alive -S --header="Host:'+hdata+'" "'+metadataurl+'" -O '+inputpath+metadataname;
	#print "cmd=",cmd,"\n";

	metaget=os.system(cmd);

	if metaget!=0:
		#print "\nArgument Error: Get meta file\n";
		return -101,0;
	
	metafileargname='/nkn/vpe/in'+metadataname;

	#print "\nmetaname=",metafileargname;
	
	metahand = open (metafileargname,'r');

	if metahand==0:
		#print "Meta File open successfull\n";
		return -102,0
	#else:	
		#print "Error Opening publisher meta file or path \n";
		return -102,0;
	pathfile='temp/';
	metadata=metahand.readlines();
	linecnt=len(metadata);

	if int(linecnt)<=int(1):
		return -107,0;

	timemetaend=time.time();
	int_read_flag=1;
	int_tbl_data=' ';
	int_tbl_len=0;

	try:
		int_data = open ("/nkn/vpe/int_data.dat",'r+');
	except IOError:
		#print "\n\nInput Output error\n\n"
		int_data=open("/nkn/vpe/int_data.dat",'w');
		int_read_flag=0;
	file_write_flag=0;
	if int_read_flag:
		#print "Internal dat name content file open successfull\n";
		int_tbl_data=int_data.readlines()
		int_tbl_len=len(int_tbl_data);
		tbl_ran_st=(500*10)/100;
		tbl_ran_end=500-tbl_ran_st;
		if int_tbl_len>tbl_ran_end:
			file_write_flag=1;
		else:
			file_write_flag=0;
		#print "\n\n test was done = ",int_tbl_len;
	#else:	
		#print "Error Opening Internal dat name file or path \n";
		#int_data = open ("/nkn/vpe/int_data.dat",'w');
		#return -102,0;

	pathfile='temp/';
	

	#metadata=metahand.readlines();

	# Time taken to fetch meta file

	timemeta=timemetaend-timemetast;

	print>>timelog,"\nTime for Meta file get from Publisher and open in seconds =",timemeta;

# Parse meta data file function call

	timeparsest=time.time();

	retval,queueuri,filename,bitrate,baseuri,seqdur,seqdurflag,uricntc,streamcnt,framerateflag,fps,afrflag,afrval,afrthflag,afrthval,bitratehigh,viddur,viddurflag=Publish_parse(metadata,linecnt);

	#print 'MetaFile Parsing Returned with ', retval;

	if retval < 0:
        	return retval,sizewarflag;

	timeparseend=time.time();

	timeparse=timeparseend-timeparsest;

	print>>timelog,"\nTime for Parsing meta file in seconds = ",timeparse;

	# Size available check

	#print "\nbitratehigh=",bitratehigh;

	if viddurflag:
		#print "\n Video duration=",viddur;
		profilereqsize=(int(viddur)*int(bitratehigh))/int(8);
		#print"\nMax Profile required size",profilereqsize;
		if int(profilereqsize)>int(avail):
			return 	-111,0;
	
# Get video files and prepare parameters need for preprocessing

	timevidgetst=time.time();

	#if videonameflag==1:
	#	filenameout=videonamepath;
	#else:

	filenameout=metadataname;
	
	retval,bitratetimelog,bitrates,inFile,outFile,outpath,outpathorig,fileprocess,filenameout,int_data=Publish_scratch_vidfetch(int_data,int_tbl_data,int_tbl_len,filename,bitrate,baseuri,seqdur,seqdurflag,uricntc,hdata,filenameout,queryparam,timelog);

	#print "TESTVALUE = ",retval;	

	#if proc_flag==0:
	#	proc_data=file_proc.readlines();
	#	proc_len=len(proc_data)
	#       cnt=0
	#	os.remove("/nkn/vpe/on_proc_data.dat");
	#	file_proc=open("/nkn/vpe/on_proc_data.dat",'w')
	#	if proc_len>0:
	#		proc_data_len=proc_len-1
 	#	        for cnt in range(0,proc_data_len):
	#			print>>file_hdl,proc_data[cnt]

	if retval < 0:
		if file_write_flag:
			os.remove("/nkn/vpe/int_data.dat");
			int_dat=open("/nkn/vpe/int_data.dat",'w+');
			cnt=tbl_ran_st;
			file_write_flag=0
			for cnt in range(tbl_ran_st,500):	
				val_strip=int_tbl_data[cnt].strip(' \r\n\t')
				filewttime=int(time.time())
				print>>int_dat,val_strip,filewttime;
				#print>>int_dat
		return retval,sizewarflag;
		
# Preprocessing and Add Chunks to FileManager queue

	timevidgetend=time.time();

	timevidget=timevidgetend-timevidgetst;
	
	print>>timelog,"\nTime for Video files get from origin and file format conversion ( if any )in seconds = ",timevidget;

	timeprest=time.time();
	
	retval=Publish_preproc_fmgr(int_data,filenameout,uricntc,framerateflag,fps,afrflag,afrval,afrthflag,afrthval,seqdurflag,baseuri,seqdur,bitrates,inFile,outFile,outpathorig,outpath,queueuri,metahand,fileprocess,streamcnt,timelog,bitratetimelog,hdata);	

	timepreend=time.time();

	timepreproc=timepreend-timeprest;

	print>>timelog,"\nTime for Chunking and ingesting into File Manager Queue in seconds = ",timepreproc;

	return retval,sizewarflag;

######################################################################

#       Parsing of Meta File

######################################################################

def Publish_parse(metadata,linecnt):
	cnt=0;
	wc=0;
	bitrate={};
	filename={};
	uribase={};
	uri={};
	baseuri={};
	prof_cnt=0;
	framerateflag=0;
	uricnt=0;
	rtcnt=0;
	afrflag=0;
	afrthflag=0;
	scratchflag=0;
	seqdur = -1;
	seqdurflag=0;
	viddur=0;
	viddurflag=0;
	videonameflag=0;
	queueuri={};
	fps=0;
	afrvalue=str(1);
	afrthval=str(112000);#900kbps
	trickplay=0;
	linedata=[];
	bitratehigh=0;
	streamcnt = 0;
        retval=100;
	while (linecnt>0):
		#print "\nlineecnt==",linecnt;
		metadataline=metadata[cnt].strip(' \r\n');
	       	if metadataline != '':
                	linedata=metadataline.split(':');
        	        value=linedata[1].strip(' \r\n\t');
	        linecnt=linecnt-int(1);
        	datalen=len(linedata);
	        profchk='profile_' + str(rtcnt + 1);
		stripval=linedata[0].lower();
		#print "\nlinedata[0]=",stripval.strip(' ');
        	if linedata[0].lower() == 'version':
                	vers=value.split('.');
	                major = vers[0].strip(' \r\n\t');
        	        minor = vers[1].strip(' \t\r\n');
	        elif linedata[0].lower() == 'profiles':
        	        streamcnt=value.strip(' \t\r\n');
                	#streamcnt=streams[0];
	        elif linedata[0].lower() == 'trickplay':
			trickplay=value.strip(' \t\r\n');
		elif linedata[0].lower() == 'keyframeinterval':
           	        seqdur=value.strip(' \t\r\n');
			seqdurflag=1;
	        elif linedata[0].lower() == 'frame rate':
                	fps=value.strip(' \t\r\n');
			#print "\n FPS=",fps;
			framerateflag=1;
        	elif linedata[0].lower() == 'sequence duration':
	                viddur=value.strip(' \t\r\n');
			viddurflag=1;
	        elif linedata[0].lower() == 'sequenceduration':
	                viddur=value.strip(' \t\r\n');
			viddurflag=1;
		elif (linedata[0].lower()).strip(' ') == 'video encoding format':
                	vidformat=value.strip(' \t\r\n');
	        elif linedata[0].lower() == 'smoothflow period':
        	        smoothflowperiod=value.strip(' \t\r\n');
	        elif linedata[0].lower() == 'audio encoding format':
        	        audformat=value.strip(' \t\r\n');
	        elif linedata[0].lower() == 'container format':
        	        cntformat=value.strip(' \t\r\n');
	        elif linedata[0].lower() == profchk:
        	        bitrate[rtcnt]=value.strip(' \t\r\n');
			#if bitrate[rtcnt]>bitratehigh:
			#	bitratehigh=bitrate[rtcnt];
			bitratehigh=int(bitratehigh)+int(bitrate[rtcnt]);
	                rtcnt = rtcnt + 1;
        	        ratecntc=rtcnt+1;
	        #elif linedata[0].lower() == 'path':
        	#       scratchpath = value.strip(' \t\r\n');
		#	scratchflag=1;
	        #elif linedata[0].lower() == 'video name':
        	#       videonamepath = value.strip(' \t\r\n');
		# 	videonameflagflag=1;
		elif  linedata[0].lower() == 'assured flow rate':
			afrflag=1;
			afrvalue=value.strip(' \t\r\n');
		elif linedata[0].lower() == 'afr threshold':
			afrthflag=1;
			afrthval=value.strip(' \t\r\n');
			afrthcomp=int(afrthval)
			afrthcomp=(afrthcomp/8)*1000
			afrthval=str(afrthcomp)
	        elif linedata[0].lower() == 'uri':
                	uriname=linedata[datalen-1];
	                uriarray=uriname.split('/');
        	        lenuri=len(uriarray);
			lencnt=0;
			total=lenuri-1;
			queueuri='';
			for lencnt in range(3,total):
		 		queueuri=queueuri+uriarray[lencnt]+'/';
			filena=uriarray[lenuri-1].split('\n');
	                filename[uricnt]=filena[0].rstrip(' \r\n\t');
        	        namelength=len(uriname);
        	        baseuri[uricnt]=linedata[1]+':'+linedata[2];
                	urib=linedata[2].split('/');
	                uribase[uricnt]=urib[2];
                	uricnt = uricnt + int(1);
	                uricntc=uricnt;
			basequeueuri=uriarray[2];
	        else:
	                if int(trickplay) > int(0):
        		        if linedata[0].lower() == 'direction':
		                        direction=value.strip(' \t\r\n');
                                if linedata[0].lower() == 'speed':
                                	speed=value.strip(' \t\r\n');
			elsecnt=0;
			#vidformat='flv';
			#audformat='mp3';
			smoothflowperiod=2;
			#scratchpath='NULL';	
			#scratchflag=0;
        	cnt=cnt+1;

	#print 'streamcnt, uricntc, ratecntc, rtcnt, fps, seqdur', streamcnt, uricntc, fps, seqdur, ratecntc, rtcnt;

	try:
		ss = int(streamcnt);
	except:
		retval = -7;
		#print 'Invalid character in Stream Count';
        try:
                ss = int(seqdur);
        except:
                retval = -7;
                #print 'Invalid character in Sequence Duration';

        try:
                ss = int(fps);
        except:
                retval = -7;
                #print 'Invalid character in Frames Per Second';

	if ratecntc == 0:
                retval = -7;
                #print 'Number of Profile Indicators cant be zero';
	

	if streamcnt == 0:
	       retval = -7;
               #print 'Number of Streams cannot be zero';
	#print 'TESTRETVAL=',retval;

	if int(rtcnt) != int(streamcnt):
		retval = -7;
		#print 'Number of Profile Indicators not same as the number of URIs';

	if int(uricntc) != int(streamcnt):
               retval = -7;
               #print 'Stream Count not the same as the number of URIs'

	#print 'TESTRETVALA=',retval;

	if int(fps) == 0:
	        retval = -7;
		#print 'fps cant be zero';
	#print 'TESTRETVALB=',retval;

	if int(seqdur) == -1:
		retval = -7;
	        #print 'Invalid value for Sequence Durations';
	#print 'TESTRETVALEND=',retval;


        return retval,queueuri,filename,bitrate,baseuri,seqdur,seqdurflag,uricntc,streamcnt,framerateflag,fps,afrflag,afrvalue,afrthflag,afrthval,bitratehigh,viddur,viddurflag;

###############################################################

# Publish_scratch_vidfetch:scratch path creation 

###############################################################

def Publish_scratch_vidfetch(int_dat_hdl,int_tbl_read,int_dat_len,filename,bitrate,baseuri,seqdur,seqdurflag,uricntc,hdata,filenameout,queryparam,timelog):

	retval=100;
	uriinputpath='/nkn/vpe/in/';
	#if scratchflag == 1:
	#	pathfile=scratchpath.split('/');
	#else:
		#print "\nScratch Path not present in meta file\n"
		#return	-3,bitrates,inFile,outFile,outpath;
		#return -1;
	#string.check '?';
	scratchpath='/nkn/vpe/out';
	pathfile=scratchpath.split('/');
	outpathorig=scratchpath.rstrip('\n');
	outpath=scratchpath.split('\n');
	tmp=outpath[0].rstrip('/out');
	filenum=len(pathfile);
	filecnt=0;
	direct=' ';
	togcount=1;

	print "\n Files download in Progress\n"
	for filecnt in range(0,filenum):
        	if pathfile[filecnt]!='':
                	if togcount:
                        	removepath=pathfile[filecnt];
	                        togcount=0;
        	        dircreat=direct+pathfile[filecnt];
                	#print "dircreat=",dircreat;
			dirretval=os.system('mkdir '+dircreat);
			#print "gfy=",dirretval;
			#if dirretval!=0:
			#retval=-108;
        	        direct=dircreat+'/';
	        else:
        	        direct='/';
	basecnt=0;
	bitrates='';
	inFile='';
	bitratetimelog='';

	# Video file Download
	# print "path=",scratchpath;
	# print "baseuri",baseuri[0];
	fetch_flag=1
    	outFile = '';
	write_data=[0];
#       outPath = '';
#       outpathorig = '';
#       fileprocess = '';
	httppath='';	
	returnvalue=0;
	convtime=0;
	parallel_val=uricntc-1;

#       fname_temp = filename[basecnt];
#       fname_tempo = fname_temp[:fname_temp.rfind('.')];
#       fname_temp=fname_tempo.rstrip(' \t\r\n');
        fileproc=filename[basecnt];
        fileprocessm=fileproc[:fileproc.rfind('.')+1];
        #print "\nfileprocessm=",fileprocessm;
        fileprocess=fileprocessm[:fileprocessm.rfind('_')];
	dat_cmp=fileprocess+'.dat'
	write_flg=0;
	proc_flag=1;
	file_data_proc=0;
	try:
		file_hdl_proc=open("/nkn/vpe/on_proc_data.dat",'r+');
	except:
		#print"\nError opening processeing data file";
		#retval=-11;
		write_flg=1;
		file_hdl_proc=open("/nkn/vpe/on_proc_data.dat",'w+');
		proc_data_len=0
	if write_flg==0:
		print"\nFile read success";
		file_data_proc=file_hdl_proc.readlines();
		proc_data_len=len(file_data_proc);
		#os.remove("\nkn\vpe\on_proc_data.dat");
		#file_hdl_proc=open("\nkn\vpe\on_proc_data.dat",'w+');
	cnt=0
	chk_flag=1
 
	int_dat_hdl.close()
        os.remove('/nkn/vpe/int_data.dat')
        int_dat_hdl=open('/nkn/vpe/int_data.dat','w+')
        for cnt in range(0,int_dat_len):
                #print "\n\nData table on process\n\n";
                #print "\n\nFile name mod is ",filenamemod;
         	value=int_tbl_read[cnt].strip(' \r\t\n');
		val_match=value.split(' ')	
                #print "\n\nTable read value match is ",val_match;
	        if dat_cmp==val_match[0]:
			diff=0
                	new_ar_time=time.time()
			print "\n value time=",float(val_match[1])
        	        fetch_flag=0;
			diff=new_ar_time-float(val_match[1])
			diff=int(diff)
			print "\ndifference=",diff
			diff=abs(diff)
			print "\nAbsolute difference=",diff
			if diff>120:
				#write_data[cnt]=value	
				#print "\nFiles are matching\n";
	                       	#break;
				chk_flag=0
        	        	fetch_flag=1;
#				int_dat_hdl.close()
				print ("\nFile removed")
#				os.remove('/nkn/vpe/int_data.dat')
#				int_dat_hdl=open('/nkn/vpe/int_data.dat','w')
				#print >>int_dat_hdl,dat_cmp,new_ar_time
			else:
				int_dat_hdl.write(int_tbl_read[cnt])
		else:
#			write_data[cnt]=value
#			print "write data=",write_data[cnt]
			int_dat_hdl.write(int_tbl_read[cnt])
		#print "\nwrite data cnt	=",write_data[cnt]
	
#	print "\n Files download in Progress\n"

	if fetch_flag==0:
		print "\nVideo file already present in cache: Feasibility support added\n";
        	retval=-10;
	        outFile = '';
        	outPath = '';
	        outpathorig = '';
        	fileprocess = '';
	        filename='';
        	return  retval,bitratetimelog,bitrates,inFile,outFile,outpath,outpathorig,fileprocess,filename,int_dat_hdl;
        cntl=0
	#print "\n processed data = ",file_data_proc[0];
        for cntl in range(0,proc_data_len):
		print "\n processed data = ",file_data_proc[0];
                val_match=file_data_proc[cntl].strip(' \r\t\n');
                if dat_cmp==val_match:
                        fetch_flag=0;
                        break;
        if fetch_flag==0:
                print "\nVideo file download already started:\n";
                retval=-12;
                outFile = '';
                outPath = '';
                outpathorig = '';
                fileprocess = '';
                filename='';
                return  retval,bitratetimelog,bitrates,inFile,outFile,outpath,outpathorig,fileprocess,filename,int_dat_hdl;

	#print>>file_hdl_proc,dat_cmp;
	#print>>int_dat_hdl,write_data
	#int_dat_hdl.close()
	for basecnt in range (0,uricntc):
		baseurimod=baseuri[basecnt].strip('\r\n');
		urimod=baseurimod.split('//');
		nameuri=urimod[1];
		urifilename=nameuri.split('/');
		baselen=len(urifilename);
		#print"urimod=",urimod;
		filenamemod=urifilename[baselen-1].rstrip(' \t\r\n');
		#print "filen=",filenamemod;
#		intdatfilename[basecnt]=filenamemod;
#		fetch_flag=1;
#		cnt=0;
#		for cnt in range(0,int_dat_len):
#			#print "\n\nData table on process\n\n";
#			#print "\n\nFile name mod is ",filenamemod;
#			val_match=int_tbl_read[cnt].strip(' \r\t\n');
#			#print "\n\nTable read value match is ",val_match;
#			if filenamemod==val_match:		
#				fetch_flag=0;
#				#print "\nFiles are matching\n";
#				break;
#		cnt=0
#               for cnt in range(0,proc_data_len):
#                       #print "\n\nData table on process\n\n";
#                       #print "\n\nFile name mod is ",filenamemod;
#                        val_match=file_data_proc[cnt].strip(' \r\t\n');
#                        #print "\n\nTable read value match is ",val_match;
#                        if filenamemod==val_match:
#                                fetch_flag=1;
#                                #print "\nFiles are matching\n";
#                                break;

		#if fetch_flag!=0:		
			#possible fseek to end and rewind come here
			#print>>int_dat_hdl,filenamemod;
			#print>>int_dat_hdl,"\n";
		findstr=urimod[1];
		findstro=findstr[findstr.find('/'):];
		#print"\nuridat=",findstr;
		findstr=findstro.rstrip(' \t\r\n');
		fileuri=' --no-http-keep-alive -q -S --header="Host:'+hdata+'" "http://127.0.0.1'+findstr;
		#print "baseuri",fileuri;
		#if fetch_flag!=0:
		httppath=httppath+'wget'+fileuri+'?'+queryparam+'=5" -O '+uriinputpath+filenamemod;
		#print "httppath=",httppath;
		if basecnt<parallel_val:
			httppath=httppath+' & ';
			#print>>file_hdl_proc,filenamemod;
		#else:
		#	print "\nVideo file already present in cache: Feasibility support added\n";
		#	retval=-10;
		#	outFile = '';
		#	outPath = '';
		#	#proc_flag=0;
		#	outpathorig = '';
		#	fileprocess = '';
		#	filename='';
 		#	return  retval,bitratetimelog,bitrates,inFile,outFile,outpath,outpathorig,fileprocess,filename;
		

	#rint "\n Files download in Progress\n"
	returnvalue=os.system(httppath);
	if returnvalue != 0:
		print "\nError Getting video file \n";
		retval=-109;
                proc_data=file_hdl_proc.readlines();
                proc_len=len(proc_data)
                os.remove("/nkn/vpe/on_proc_data.dat");
                file_hdl_proc=open("/nkn/vpe/on_proc_data.dat",'w+')
                if proc_len>0:
                        proc_data_len=proc_len-1
                        for cnt in range(0,proc_data_len):
                                print>>file_hdl_proc,proc_data[cnt]
		outFile = '';
		outPath = '';
		outpathorig = '';
		fileprocess = '';
		filename='';
 		return  retval,bitratetimelog,bitrates,inFile,outFile,outpath,outpathorig,fileprocess,filename,int_dat_hdl;
#	else:
#		print "\n Files download in Progress\n"

	print "\n Files download success"
	basecnt=0;
	filecntchk=uricntc+1;
#	print"\n File check for uri is",filecntchk;
#	while	1:
#		retfilecnt=os.system('ls /nkn/vpe/in |wc -l >>/nkn/vpe/chk.txt')
#		chkhdl=open("/nkn/vpe/chk.txt",'r')
#		chkread=chkhdl.readlines();
#		retfileval=chkread[0];
#		print"\nFilechk cnt is",retfileval
#		if retfilecnt>=filecntchk:
#			break;	
#		os.remove('/nkn/vpe/chk.txt');
	for basecnt in range (0,uricntc):
		#if fetch_flag!=0:
		bitrates=' -b '+bitrate[basecnt]+bitrates;
		bitratetimelog=bitratetimelog+bitrate[basecnt]+'   ';
		fname_temp = filename[basecnt];
		fname_tempo = fname_temp[:fname_temp.rfind('.')];
		fname_temp=fname_tempo.rstrip(' \t\r\n');	
		ffmpegcmd='/opt/nkn/bin/ffmpeg -i '+uriinputpath+filename[basecnt]+' -f flv -vcodec copy -acodec copy '+uriinputpath+fname_tempo+'.flv';
		#print "\nffmpegcmd=",ffmpegcmd,"\n";
		convreturn=0;
		convtimest=time.time();
		if filename[basecnt].endswith('mp4'):
			convreturn=os.system( '/opt/nkn/bin/ffmpeg -i '+uriinputpath+filename[basecnt]+' -f flv -vcodec copy -acodec copy '+uriinputpath+fname_tempo+'.flv');
			if convreturn!=0:
				#print "\nError using ffmpeg utility\n";
				retval=-105;
				proc_flag=0;
		convtimeend=time.time();
		convtime=convtime+(convtimeend-convtimest);
		filename[basecnt]=fname_temp+'.flv';
		fileproc=filename[basecnt];
		fileprocessm=fileproc[:fileproc.rfind('.')+1];
		#print "\nfileprocessm=",fileprocessm;		
		fileprocess=fileprocessm[:fileprocessm.rfind('_')];
		#print "\nfileprocess=",fileprocess;		
		ipfile=uriinputpath+filename[basecnt];
		inFile=' -add '+uriinputpath+filename[basecnt]+inFile;
		#print "\nFle Process in fetch =",fileprocess;
		outFile=' -new '+fileprocess;
	print >>timelog,"\nMP4 to FLV conversion (If MP4 files)	:",convtime;
	return 	retval,bitratetimelog,bitrates,inFile,outFile,outpath,outpathorig,fileprocess,filename,int_dat_hdl;


####################################################################

#    Publish_preproc_fmgr:Preprocessing

####################################################################


def Publish_preproc_fmgr(int_dat_hdl,filenameout,filecnt,framerateflag,fps,afrflag,afrval,afrthflag,afrthval,seqdurflag,baseuri,seqdur,bitrates,inFile,outFile,outpathorig,outpath,queueuri,metahand,fileprocess,streamcnt,timelog,bitratetimelog,hdata):

	memory_file_io_flag=1; #0-Memory 1-File

	exe='/opt/nkn/sbin/vpe_pre_proc';

	if seqdurflag==0:
		seqdur=str(2);

	print>>timelog,"\n\t File Pre-processed is    	     	 :", fileprocess;

	print>>timelog,"\n\t Number of Profiles      	     	 :", streamcnt;
	
	print>>timelog,"\n\t Bitrates of each profile in kbps	 :",bitratetimelog;

	print>>timelog,"\n\t Profile duration is      	     	 :", seqdur;	
	
	#print"output path is ",outpathorig,"\n";
	
	arg= ' ';

	if framerateflag==1:
		arg=arg+'-fps '+fps+' ';
	
	#if afrflag==1:
	arg=arg+'-afr '+afrval+' ';

	#if afrthflag==1:
	arg=arg+'-qt '+afrthval+' ';

	cmd= exe+' -np '+str(streamcnt)+' -duration '+seqdur+bitrates+' -i '+str(memory_file_io_flag)+inFile+outFile+' -s '+outpathorig+'/'+arg;

	print "\n pre-proc cmd line ", cmd;
	
	timechunkst=time.time();

	retvalue=os.system(cmd);
	
	filear=fileprocess.split('.');
	file=filear[0];
	datname=file+'.dat'
	if retvalue!=0:
		#print "\nError in preprocessing arguments\n";
	        cnt=0
        	file_hdl_proc=open("/nkn/vpe/on_proc_data.dat",'r+')
	        file_read=file_hdl_proc.readlines()
        	read_len=len(file_read)
	        os.remove("/nkn/vpe/on_proc_data.dat");
        	file_hdl_proc=open("/nkn/vpe/on_proc_data.dat","w+")
	        for cnt in range(0,read_len):
			filechk=file_read[cnt].rstrip(' \r\n\t')
        	        if filechk!=datname:
                	        print>>file_hdl_proc,file_read[cnt]
		return -104;

	timechunkend=time.time();

	chunktime=timechunkend-timechunkst;

	print>>timelog,"\n\t Time taken for chunking in seconds :",chunktime;
	#print("\n\n\nOutput File is %s",file)
        logf="/nkn/vpe/out/"+file+".log";
        logopen=open(logf,"r");
        logread=logopen.readlines();
	loglinecnt=len(logread);
	#print("\nLoglinecnt=%d",loglinecnt);
	cnt=0;
	chunkcnt=0;
	mystr="%.10d"%cnt;
	basename=filenameout[0].split('.');	
        chkname=basename[0]+'_c'+str(mystr)+'.flv';
        #print("\n\n Chunkname=%s",chkname);

	while int(loglinecnt)>0:
		linerd=logread[cnt].split(':');	
		if linerd[0].lower()=='error code':
			linerd_sp=linerd[1].split(' ');
			if int(linerd_sp[1].lower())!=int(0):
				#print ("Error in Pre-Processing of type %s",linerd_sp[1].lower());
				logopen.close();
				return -104;	
		elif linerd[0].lower()=='no of output chunks':
			linerd_sp=linerd[1].split(' ');
			chunkcnt=int(linerd_sp[1]);		
			#print ("\n\nChunk count = %d",chunkcnt)
		#else:
			#print("\nLog read on process\n\n");
		cnt=cnt+int(1);
		loglinecnt=loglinecnt-int(1);

	cnt=0;
	ch_cnt=1;
	filec=0;
	for filec in range(0,filecnt):
		basename=filenameout[filec].split('.');	
		for cnt in range(0,chunkcnt):
			mystr="%.10d"%ch_cnt;
			chkname=basename[0]+'_c'+str(mystr)+'.flv';
			#print("\nchunk name = %s",chkname);
			queueret=os.system('/opt/nkn/bin/cache_add.sh -r -D ' +hdata+ ' /nkn/vpe/out/'+chkname + ' /' + queueuri + chkname);
			#if queueret!=0:
			#	print('\n Error while queue insertion \n')
			ch_cnt=ch_cnt+int(1);
		cnt=0;
		ch_cnt=1;
	xmlname="/nkn/vpe/out/"+file+'.xml';
	queueret=os.system('/opt/nkn/bin/cache_add.sh -r -D ' +hdata+ ' '+xmlname + ' /' + queueuri+file+'.xml');
	#if queueret==0:
	datname="/nkn/vpe/out/"+file+'.dat';
	queueret=os.system('/opt/nkn/bin/cache_add.sh -r -D ' +hdata+ ' '+datname + ' /' + queueuri+file+'.dat');
	#	print('\n Error while queue insertion \n')
	
	datname=file+'.dat';

	
	cnt=0
	file_hdl_proc=open("/nkn/vpe/on_proc_data.dat",'r+')
	file_read=file_hdl_proc.readlines()
	read_len=len(file_read)
	os.remove("/nkn/vpe/on_proc_data.dat");
	file_hdl_proc=open("/nkn/vpe/on_proc_data.dat","w+")
	for cnt in range(0,read_len):
		filechk=file_read[cnt].rstrip(' \r\n\t')
		if filechk!=datname:
			print"\n File read count is",file_read[cnt]
			print>>file_hdl_proc,file_read[cnt]

	file_time=time.time()
	#print>>int_dat_hdl,datname
	print>>int_dat_hdl,datname,file_time

        metahand.close();
	return 100;

# Publish_preproc_fmgr function ends



################################################################

# Parsing log file from preprocessing engine 

################################################################

def dir_list2(dir_name): 
    fileList = [] 
    for file in os.listdir(dir_name): 
        dirfile = os.path.join(dir_name, file) 
        if os.path.isfile(dirfile): 
                fileList.append(dirfile) 
    return fileList 


################################################################


if __name__ == "__main__":

   hostData     = sys.argv[1]
   hostLen      = sys.argv[2]
   uriData      = sys.argv[3]
   uriLen       = sys.argv[4]
   stateQuery   = sys.argv[5]
   stateQueryLen= sys.argv[6]
   publish_init(hostData, hostLen, uriData, uriLen, stateQuery, stateQueryLen)
