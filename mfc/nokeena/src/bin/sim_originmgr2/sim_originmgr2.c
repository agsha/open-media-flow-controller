/*****************************************************************************
*
*	File : originmgr_main.c
*
*	Description : The main c logic of origin manager
*
*	Created By : Ramanand Narayanan (ramanand@nokeena.com)
*
*	Created On : 1st July, 2008
*
*	Modified On : 
*
*	Copyright (c) Nokeena Inc., 2008
*
*****************************************************************************/
#include <stdio.h>
#include <stdlib.h>

#define NEW_DM 1

#include "nkn_common_config.h"
#include "nkn_diskmgr_api.h"
#include "nkn_diskmgr2_api.h"
#include "nkn_attr.h"

/* Local Macros */
#define MAX_WGET_BUFFER_SIZE	1048576		/* 1MB = 1024*1024bytes */
#define MAX_LINE_SIZE		160
#define	WGET_LOG_FILE	"/tmp/.nkn.wget.log"
#define	WGET_DATA_FILE	"/tmp/.nkn.wget.data"
#define WGET_200_OK_STR	"200 OK"

/* Global Variables */
time_t	nkn_cur_ts;
int am_cache_ingest_hits_threshold = 1;
int am_cache_ingest_last_eviction_time_diff = 60;

/* Definition of origin server map as provided by Move Networks */
struct move_originsrv_map
{
	char *cpKey ;
	char *cpOriginSrv ;
} stOriginMap[] = {
{(char *)"/stream/", (char *)"espnmovelive25.espn.com.edgesuite.net"},
{(char *)"/foxvod/", (char *)"qmplivefox.movenetworks.com"},
{(char *)"/abcvod/", (char *)"abc.move.l3.cdn.go.com"},
{(char *)"/discovery/", (char *)"media2.discovery.com"},
{(char *)"/vod/", (char *)"stream.qcg2.qcn3e.movenetworks.com"}
} ;
#define	MAP_COUNT	5
/* PROTOTYPES */
//void 	nvsd_mgmt_syslog(char *fmt, ...) __attribute__((format(printf, 1, 2)));
void print_usage (char *cpProgName) ;
int sanity_test_n_init (int argc, char *argv[]) ;
int get_origin_server (char *cpURL) ;
boolean wget_200_ok (void) ;
static int nkn_sim_get_file_sz(int prof) ;
char * fill_memory(char *url, char *ptr, uint64_t in_length) ;
void get_videos (char *cpOriginServer, char *cpVideoPath) ;
void fetch_video_clips (char *cpURL) ;
void process_url_list_file (void);

/* Local Variables */
static 	FILE	*fpUrlList ; /* file pointer to the url list file */
static	char	*cpWgetBuffer ; /* buffer used to store wget output */
static	int	verbose;

#if 0
/* ------------------------------------------------------------------------- */
/*
 *	funtion : nvsd_mgmt_syslog ()
 *	purpose : API/Wrapper for logging into syslog
 */
void
nvsd_mgmt_syslog (char *fmt, ...)
{
	char 	log_str [1024];
	va_list	tAp;

	va_start(tAp, fmt);
	vprintf (fmt, tAp);
	va_end(tAp);

//	lc_log_basic(LOG_ALERT, "%s", log_str);

} /* end of nvsd_mgmt_syslog() */

/* ------------------------------------------------------------------------- */
/*
 *	funtion : nvsd_mgmt_timestamp ()
 *	purpose : API/Wrapper for returning timestamp
 */
char*
nvsd_mgmt_timestamp(char *sz_ts)
{
	struct timeval	tval;
	time_t	t_timep;
	int		retval;

	/* Sanity test */
	if (!sz_ts) return ("<error>");

	/* Now do the needful */
	strcpy (sz_ts, "<Time Stamp>");

	/* Get the timeval value */
	retval = gettimeofday (&tval, NULL);
	if (0 == retval)
	{
		/* Now create the timestamp string */
		t_timep = tval.tv_sec;
		memset (sz_ts, 0, MAX_TIMESTAMP_LEN);
		if (NULL != asctime_r (localtime (&t_timep), sz_ts))
		{
			char *cp;
			char szMillisecond [8];

			/* Replace the \n with \0 */
			cp = strchr (sz_ts, '\n');
			if (cp) *cp = '\0';

			/* Get the milli second */
			snprintf (szMillisecond, 7, " %3dms", (int)tval.tv_usec/1000); 
			strcat (sz_ts, szMillisecond);
		}
		else
			strcpy (sz_ts, "<gettime failed>");
	}
	else
	{
		/* gettimeofday has failed */
		strcpy (sz_ts, "<gettime failed>");
	}

	return (sz_ts);

} /* end of nvsd_mgmt_timestamp() */
#endif // 0

/*****************************************************************************
*	Function : print_usage ()
*	Purpose : print the usage of this utility namely command line options
*	Parameters : cpProgName - the argv[0] value
*	Return Value : void
*****************************************************************************/
void
print_usage (char *cpProgName)
{
	fprintf (stderr, "usage : %s <URL list filename>\n", cpProgName) ;

	return ;
} /* end of print_usage () */

/*****************************************************************************
*	Function : sanity_test_n_init ()
*	Purpose : perform basic checks to see if the app can run
*	Parameters : argc, argv - passed as is from main 
*	Return Value : 0 on success and non-zero on failure
*****************************************************************************/
int
sanity_test_n_init (int argc, char *argv[])
{
	char **argument = &argv[1];

	/* Test to see if the filename is passed */
	if (argc ==1)
	{
		fprintf (stderr, "%s : missing parameter URL list filename\n", argv [0]) ;
		print_usage (argv[0]) ;
		return (1) ;
	}

	if (!strcmp(argv[1], "-v")) {
	    verbose = 1;
	    argument++;
	}
	/* Try to open the file */
	fpUrlList = fopen (*argument, "r") ;
	if (NULL == fpUrlList)
	{
		fprintf (stderr, "%s : unable to open URL list <%s>\n", argv[0], argv[1] ) ;
		print_usage (argv[0]) ;
		return (1) ;
	}

	/* Allocate the buffer memory to reuse thru all the URIs */
#ifndef NEW_DM
	cpWgetBuffer = (char*) calloc (MAX_WGET_BUFFER_SIZE, sizeof (char)) ;
	if (NULL == cpWgetBuffer)
	{
		fprintf (stderr, "Memory allocation failure, check system memory\n") ;
		return (2) ;
	}
#else
	if (posix_memalign((void *)&cpWgetBuffer, 64*1024, MAX_WGET_BUFFER_SIZE)) {
	    fprintf(stderr, "Memory allocation failure, check system memory\n");
	    return(2);
	}
#endif
	return (0) ;
} /* end of sanity_test_n_init () */


/*****************************************************************************
*	Function : get_origin_server ()
*	Purpose : This function parsers the URL and based on the hostname 
*		in the URL figures if it has to change based on the mapping
*		table by Move Networks
*	Parameters : cpURL
*	Return Value : int - index in the mapping table that matches
*****************************************************************************/
int
get_origin_server (char *cpURL)
{
	int	nIndex ;
	char	*cpKey ;

	/* Now search for the substrings provided by Move Networks */
	nIndex = 0 ;
	for (nIndex = 0; nIndex < MAP_COUNT; nIndex++)
	{
		cpKey = strstr (cpURL, stOriginMap [nIndex].cpKey) ;
		if (NULL != cpKey)
			break ; /* Match found hence jump out of loop */
	}
	if (NULL == cpKey)
		return -1 ; /* No match found hence no mapping */

	/* Return the index to the mapped origin server */
	return (nIndex) ;

} /* end of get_origin_server () */

/*****************************************************************************
*	Function : wget_200_ok ()
*	Purpose : This function reads the wget.log file in /tmp to see 
*		if the wget call was successful. If not 200 OK then 
*		video fetched failed
*	Parameters : void
*	Return Value : boolean - TRUE if 200 ok else FALSE
*****************************************************************************/
boolean
wget_200_ok (void)
{
	FILE	*fpWgetLog ;
	char	*cp200ok ;
	char	szLine [MAX_LINE_SIZE + 1] ;

	fpWgetLog = fopen (WGET_LOG_FILE, "r") ;
	if (NULL == fpWgetLog)
		return FALSE ;

	while (NULL != fgets (szLine, MAX_LINE_SIZE, fpWgetLog))
	{
		cp200ok = strstr (szLine, WGET_200_OK_STR) ;
		if (NULL != cp200ok)
		{
			fclose (fpWgetLog) ;
			return TRUE ;
		}
	}

	/* No 200 Ok in the log hence wget failed */
	fclose (fpWgetLog) ;
	fpWgetLog = fopen (WGET_LOG_FILE, "r") ;
	while (NULL != fgets (szLine, MAX_LINE_SIZE, fpWgetLog))
		puts (szLine) ;
	fclose (fpWgetLog) ;
	return FALSE ;

} /* end of wget_200_ok () */

static int
nkn_sim_get_file_sz(int prof)
{

	switch (prof) {
		case 1:
		    return 600;
		case 2:
			return 40*1024;
		case 3:
			return 60*1024;
		case 4:
			return 80*1024;
		case 5:
			return 100*1024;
		case 6:
			return 120*1024;
		case 7:
			return 140*1024;
		case 8:
			return 160*1024;
		case 9:
			return 180*1024;
		case 10:
			return 200*1024;
		case 11:
			return 210*1024;
		case 12:
			return 230*1024;
		case 13:
			return 250*1024;
		default:
			return 250*1024;
	}
}

char *
fill_memory(char *url, char *ptr, uint64_t in_length)
{
        int len = strlen(url);
        char *str_ptr = ptr;
	uint64_t length = 0;
#ifdef NEW_DM
	int sec = 1;
#endif

#ifndef NEW_DM
        while ((length+len) < in_length)
#else
	while ((length+len) < in_length && length < 1024)
#endif
	{
                strncpy(ptr, url, len);
                ptr += len;
                length += len;
        }
#ifdef NEW_DM
	ptr = (char *)((uint64_t)ptr & ~0x1ff);
	length = 1024;
	while (length < in_length) {
	    if (length+512 < in_length) {
		memset(ptr, sec, 512);
		ptr += 512;
		length += 512;
	    } else {
		memset(ptr, sec, in_length-length);
		ptr += (in_length-length);
		length += (in_length-length);
	    }
	    sec++;
	}
#endif
        return str_ptr;
}

/*****************************************************************************
*	Function : get_videos ()
*	Purpose : This function uses the origin server and video path to form
*		the URL to fetch using wget. It appends the profile and clip
*		part to the URL, increasing them till we hit a HTTP 404. 
*	Parameters : cpOriginServer, cpVideoPath 
*	Return Value : void
*****************************************************************************/
void
get_videos (char *cpOriginServer, char *cpVideoPath)
{
	struct MM_put_data stA_info ;
	int	nProfile, nClip ;
	int	nReadSize, fdWgetData ;
	boolean	bMoreProfiles, bMoreClips ;
	char	szURL [MAX_LINE_SIZE + 1] ;
	char	*cpTemp ;
	int	nCount = 0;
	int	ret_val = -1;
#ifdef NEW_DM
	nkn_iovec_t iov[1];
	nkn_attr_t *attr;
#endif

	posix_memalign((void **)&attr, 512, 512);

	bMoreProfiles = bMoreClips = TRUE ;
	nCount = 0 ;
	for (nProfile = 1; nProfile < 14; nProfile++)
	{
		bMoreClips = TRUE ;
		//for (nClip = 1; bMoreClips; nClip++)
		for (nClip = 1; nClip < 100; nClip++)
		{
			snprintf (szURL, MAX_LINE_SIZE + 1, "http://%s/%s_%02X%08X.qss",
				cpOriginServer, cpVideoPath, nProfile, nClip) ;

			if (1)
			{
				/* Now read the data from wget.data file */
				memset (cpWgetBuffer, 1, MAX_WGET_BUFFER_SIZE + 1) ;
				//fdWgetData = open (WGET_DATA_FILE, O_RDONLY) ;
				fdWgetData = 1;
				if (-1 != fdWgetData)
				{
					//nReadSize = read (fdWgetData, cpWgetBuffer,  MAX_WGET_BUFFER_SIZE) ;
					nReadSize = nkn_sim_get_file_sz(nProfile);

					//close (fdWgetData) ;

					/* Fill the structure to store */
					/* Clean the struct memory */
					memset (&stA_info, 0, sizeof(struct MM_put_data)) ;
					/* objname will have Vn_Pn */
					cpTemp = strrchr (cpVideoPath, '/') ;
					cpTemp++ ; /* skip the forward slash */
					snprintf (stA_info.objname, NKN_MAX_FILE_NAME_SZ, "%s_%02X", cpTemp, nProfile) ;
					strcpy (stA_info.uriname, szURL) ;
					snprintf (stA_info.uriname, NKN_MAX_FILE_NAME_SZ, "%s_%02X%08X.qss", cpVideoPath, nProfile, nClip) ;
					stA_info.content_ptr = cpWgetBuffer ;
					stA_info.content_length = nReadSize ;
					fill_memory(stA_info.uriname, stA_info.content_ptr, nReadSize);

#if 0
					printf ("MM_put_data.objname = %s\n", stA_info.objname) ;
					printf ("MM_put_data.uriname = %s\n", stA_info.uriname) ;
					printf ("MM_put_data.content_length = %d\n", stA_info.content_length) ;
#endif

#ifdef NEW_DM
					/* Call DM_put to store the video */
					stA_info.uol.uri = &stA_info.uriname[0];
					stA_info.uol.offset = 0;
					stA_info.uol.length = nReadSize;
					stA_info.iov_data_len = 1;
					stA_info.attr = attr;
					nkn_attr_init(attr);
					attr->attrsize += sizeof("ZYXWVU");
					strcpy((char *)attr->blob, "ZYXWVU");
					strcpy(cpWgetBuffer, "abcdefghijk");
					iov[0].iov_base = cpWgetBuffer;
					iov[0].iov_len = nReadSize;
					stA_info.iov_data = iov;
					stA_info.ptype = SAS2DiskMgr_provider;
					ret_val = DM2_put(&stA_info, NKN_VT_MOVE);
#else
					ret_val = DM_put(&stA_info, NKN_VT_MOVE);
#endif

					if(ret_val < 0) {
						return;
					}

#if 1
					/* Just for testing ... */
					nCount++ ;
					if (nCount == 1)
					{
						bMoreProfiles = FALSE ;
						bMoreClips = FALSE ;
					}
#endif /* TEST */
					continue ;
				}
			}
			else
			{
				printf ("\n\nwget FAILED : %s\n", szURL) ;
			}
		} /* for loop */
	} /* for loop */


	//printf("\n ------------------- FINISHED POPULATION ---------------------");

} /* end of get_videos () */

/*****************************************************************************
*	Function : fetch_video_clips ()
*	Purpose : This function gets the origin server from the URL using the
*		static mapping and then starts doing wgets to the various clips
*	Parameters : cpURL 
*	Return Value : void
*****************************************************************************/
void
fetch_video_clips (char *cpURL)
{
	int	nMapIndex ; /* Index of the matching map in the mapping list */
	char	*cpOriginServer ; /* Actual origin server to go get video */
	char	*cpVideoPath, *cpTemp ;

	/* Get the origin server from URL */

	nMapIndex = get_origin_server (cpURL) ;
	if (-1 == nMapIndex) {
		//printf("\n***********SKIPPING");
		return  ; /* No match hence skip */
	}
	cpOriginServer = stOriginMap [nMapIndex].cpOriginSrv ;

	/* Now get the video path from the URL */
	cpVideoPath = strstr (cpURL, stOriginMap [nMapIndex].cpKey) ;

#if 0
	printf ("\nOrigin Server : %s\n", cpOriginServer) ;
	printf ("\nVideo Path : %s\n", cpVideoPath) ;
#endif
	/* Parse the tail part that specifies the profile # and the clip # */
	cpTemp = cpVideoPath + strlen(cpVideoPath) ;
	while ('_' != *cpTemp)
	{
		cpTemp-- ;
		if (cpTemp == cpVideoPath)
			break ;
	}
	if (cpTemp != cpVideoPath)
		*cpTemp = '\0' ;

	//printf ("\nVideo Name : %s\n", cpVideoPath) ;

	/* Now call wget with increasing profile number and clip number */
	//printf("\n%s(): VIKRAM: ENDING ORIGIN MGR HERE", __FUNCTION__);
	get_videos (cpOriginServer, cpVideoPath) ;

	return ;

} /* end of fetch_video_clips () */


/*****************************************************************************
*	Function : process_url_list_file ()
*	Purpose : This function read thru the URL list file and for 
*		each line parses the URI and makes a wget call for 
*		the number of 2-sec clips
*	Parameters : none
*	Return Value : void
*****************************************************************************/
void
process_url_list_file (void)
{
	char	szLine [MAX_LINE_SIZE + 1] ;
	char	*cpURL ;

	/* Read thru the file line by line */
	while (NULL != fgets (szLine, MAX_LINE_SIZE, fpUrlList))
	{
		//printf ("Line : %s", szLine) ;

		/* Parse line to get the URL */
		cpURL = strtok (szLine, " \n") ;
		if (NULL == cpURL) continue ;

		/* Check the parsed values */
		//printf ("URL = %s\n", cpURL) ;

		/* Now call the function to get the server and do a wget */
		fetch_video_clips (cpURL) ;

	} /* end of while () */

	return ;
} /* end of process_url_list_file () */

/*****************************************************************************
*	Function : main ()
*	Purpose : main logic 
*	Parameters : argc, argv 
*	Return Value : int
*****************************************************************************/
int
main (int argc, char *argv[])
{
	/*extern int DM2_init(void);*/

	/* Test to see if the filename is passed */
	if (sanity_test_n_init (argc,argv))
		exit (1) ; /* sanity failed so exit */

	/* Call dm_init to initialize the disk manager interface */
	g_thread_init (NULL);
	DM2_init () ;

	/* Process the URL list file */
	process_url_list_file () ;

	/* All done so close the file pointer */
	fclose (fpUrlList) ;

	exit (0) ;
} /* end of main () */
