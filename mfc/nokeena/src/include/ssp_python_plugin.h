#ifndef NKN_PYTHONLIB_H
#define NKN_PYTHONLIB_H

int py_checkMD5Hash(const char *hostData, int hostLen, const char *uriData, const char *queryHash);

/*
 * BZ 8382
 * Transcode script call
 * Due to Python GLI
 * The C2Python interface for transcode is not used in multi-thread
 */

#if  0
int pyVpePublish(const char *hostData, int hostLen, const char *uriData, int uriLen, const char *stateQuery, int stateQueryLen);
#endif /* 0 */


//Rtsp ingest management
int pyRtspIngest(const char *hostData, int hostLen, const char *uriData, int uriLen);

/*
 * BZ 8382
 * Transcode script call
 * Due to Python GLI
 * The C2Python interface for transcode is not used in multi-thread
 */
#if 0
int pyVpeGenericTranscode(const char *hostData, int hostLen,
			  const char *uriData, int uriLen,
			  int transRatio,int transComplex);

int pyVpeYoutubeTranscode(const char *hostData, int hostLen,
			  const char *uriData, int uriLen,
			  const char *queryName, int queryLen,
	      		  const char *remapUriData, int remapUriLen,
			  const char *nsuriPrefix, int nsuriPrefixLen,
			  int transRatio, int transComplex);
#endif /* 0 */ 


#endif
