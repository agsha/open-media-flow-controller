#ifndef VFS_DEFS__H
#define VFS_DEFS__H

#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "nkn_defs.h"
#include "rtsp_session.h"
#include "virt_cache_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct vfs_aio {
        size_t size;
        size_t nmemb;
	/* need to be allocated by caller.
	   TODO: make this as direct BM iovec pointers
	*/
        void *data_ptr;
        int  (*aio_callback)(void *vfs_aio);
        uint32_t retval;
} nkn_vfs_aio_t;

struct vfs_funcs_t {
        FILE * (* pnkn_fopen)(const char *path, const char *mode);
        size_t (* pnkn_fread)(void *ptr, size_t size, size_t nmemb, FILE *stream);
        size_t (* pnkn_fwrite)(const void *ptr, size_t size, size_t nmemb, FILE *stream);
        long   (* pnkn_ftell)(FILE *stream);
        off_t  (* pnkn_ftello)(FILE *stream);
        void  (* pnkn_clearerr)(FILE *stream);
        int   (* pnkn_ferror)(FILE *stream);
        int   (* pnkn_feof)(FILE *stream);
        int   (* pnkn_fileno)(FILE *stream);
        int   (* pnkn_fseek)(FILE *stream, long offset, int whence);
        void  (* pnkn_rewind)(FILE *stream);
        int   (* pnkn_fflush)(FILE *stream);
        int   (* pnkn_fgetpos)(FILE *stream, fpos_t *pos);
        int   (* pnkn_fsetpos)(FILE *stream, fpos_t *pos);
        int   (* pnkn_fseeko)(FILE *stream, off_t offset, int whence);
        int   (* pnkn_fputc)(int c, FILE *stream);
        int   (* pnkn_fgetc)(FILE *stream);
        int   (* pnkn_fputs)(const char *s, FILE *stream);
        char * (* pnkn_fgets)(char *s, int size, FILE *stream);
        int   (* pnkn_stat)(const char *path, struct stat *buf);
        int   (* pnkn_fstat)(int filedes, struct stat *buf);
        int   (* pnkn_lstat)(const char *path, struct stat *buf);
        int   (* pnkn_fclose)(FILE *fp);
        nkn_provider_type_t (* pnkn_vfs_get_last_provider)(FILE *stream);

        // NB: virt_cache server specific methods (return vccmp_stat_data).
        int    (* pnkn_vcstat)(const char *path, vccmp_stat_data_t *stat_data);
        FILE * (* pnkn_vcopen)(const char *path, vccmp_stat_data_t *stat_data);
        size_t (* pnkn_vcread)(FILE *stream, void *ptr, size_t size, 
                               off_t offset, uint8_t direct,
                               uint32_t streaming_read_size);

        //size_t (*pnkn_vfs_get_file_size)(FILE *stream);
        //size_t (*pnkn_vfs_set_file_size)(FILE *stream, size_t size);
	/*size_t (* pnkn_fread_aio)(FILE *stream, nkn_vfs_aio_t *in_aio); */

	/*
	 *
	 */
	TaskToken (* pnkn_scheduleDelayedTask)(int64_t microseconds, 
						TaskFunc* proc, 
						void* clientData);
	void (* pnkn_unscheduleDelayedTask)(TaskToken prevTask);
	void (* pnkn_updateRTSPCounters)(uint64_t bytesSent);
        size_t (* pnkn_fread_opt)(void *ptr, size_t size, size_t nmemb, FILE *stream,
				  void **temp_ptr, int *tofree);


	void * pnkn_callback;
	int  * prtsp_dm2_enable;
};

#ifdef __cplusplus
}
#endif

#endif // VFS_DEFS__H
