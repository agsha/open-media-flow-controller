#ifndef _MFP_FILE_SESS_MGR_
#define _MFP_FILE_SESS_MGR_

#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif
    /**
     * start the file processing pipeline by doing the following
     * a. adds to a thread pool for processing.
     * @param path - path to the PMF file
     * @return retunrs 0 on success and a negative integer on error 
     */
     int32_t mfp_file_start_listener_session(char *path); 

#ifdef __cplusplus
}
#endif

#endif //_MFP_FILE_SESS_MGR_
