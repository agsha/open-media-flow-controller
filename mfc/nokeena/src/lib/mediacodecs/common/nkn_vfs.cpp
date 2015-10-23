#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <netdb.h>
#include <time.h>
#include <assert.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <pthread.h>
#include <stdint.h>
#include <atomic_ops.h>
#include "rtsp_func.h"

//#define RTSP_PRINTOUT
#ifdef RTSP_PRINTOUT
#define RTSP_DEBUG(fmt, ...)  \
	printf("%s:%d: "fmt"\n", __FUNCTION__, __LINE__, ##__VA_ARGS__);
#else
#define RTSP_DEBUG(fmt, ...) 
#endif // 0

#define MAX_STREAM	10000

struct rtsp_stream {
	int used;
	FILE * pfs;
} g_streams[MAX_STREAM];

#define IS_CACHED_FS(x) \
	(((char *)x>=(char *)&g_streams[0]) && ((char *)x<=(char *)&g_streams[MAX_STREAM]))
#define STREAM_TO_FILE(x) ((struct rtsp_stream *)x)->pfs

pthread_mutex_t rtsp_mutex = PTHREAD_MUTEX_INITIALIZER;

struct rtsp_nkn_callback_t * prtsp_nkn_callback = NULL;

/* *****************************************************
 * RTSP virtual file system
 * virtual file system APIs
 ******************************************************* */
struct vfs_funcs_t * pfunc;

extern "C" void nkn_vfs_vfs_clearerr(FILE *stream);
extern "C" int nkn_vfs_feof(FILE *stream);
extern "C" int nkn_vfs_ferror(FILE *stream);
extern "C" int nkn_vfs_fileno(FILE *stream);
extern "C" int nkn_vfs_fseek(FILE *stream, long offset, int whence);
extern "C" long nkn_vfs_ftell(FILE *stream);
extern "C" void nkn_vfs_vfs_rewind(FILE *stream);
extern "C" int nkn_vfs_fgetpos(FILE *stream, fpos_t *pos);
extern "C" int nkn_vfs_fsetpos(FILE *stream, fpos_t *pos);
extern "C" size_t nkn_vfs_fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
extern "C" size_t nkn_vfs_fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
extern "C" off_t nkn_vfs_ftello(FILE *stream);
extern "C" int nkn_vfs_fseeko(FILE *stream, off_t offset, int whence);
extern "C" int nkn_vfs_fputc(int c, FILE *stream);
extern "C" int nkn_vfs_fputs(const char *s, FILE *stream);
extern "C" int nkn_vfs_fgetc(FILE *stream);
extern "C" int nkn_vfs_stat(const char *path, struct stat *buf);
extern "C" int nkn_vfs_fstat(int filedes, struct stat *buf);
extern "C" int nkn_vfs_lstat(const char *path, struct stat *buf);
extern "C" char *nkn_vfs_fgets(char *s, int size, FILE *stream);
extern "C" int nkn_vfs_fclose(FILE *fp);
extern "C" FILE *nkn_vfs_fopen(const char *path, const char *mode);
extern "C" int nkn_vfs_fflush(FILE *stream);
extern "C" size_t nkn_vfs_fread_opt(void *ptr, size_t size, size_t nmemb, FILE *stream,
				    void **temp_ptr, int *free_buf);
extern "C" void nkn_vfs_clearerr(FILE *stream)
{
	if(IS_CACHED_FS(stream)) {
		pfunc->pnkn_clearerr(STREAM_TO_FILE(stream));
		return;
	}
	RTSP_DEBUG("stream=%p", stream);
	clearerr(stream);
}

extern "C" int nkn_vfs_feof(FILE *stream)
{
	int ret;

	if(IS_CACHED_FS(stream)) {
		return pfunc->pnkn_feof(STREAM_TO_FILE(stream));
	}
	ret = feof(stream);
	RTSP_DEBUG("stream=%p ret=%d", stream, ret);
	return ret;
}

extern "C" int nkn_vfs_ferror(FILE *stream)
{
	int ret;

	if(IS_CACHED_FS(stream)) {
		return pfunc->pnkn_ferror(STREAM_TO_FILE(stream));
	}
	ret = ferror(stream);
	RTSP_DEBUG("stream=%p ret=%d", stream, ret);
	return ret;
}

extern "C" int nkn_vfs_fileno(FILE *stream)
{
	int ret;

	if(IS_CACHED_FS(stream)) {
		return pfunc->pnkn_fileno(STREAM_TO_FILE(stream));
	}
	RTSP_DEBUG("stream=%p", stream);
        ret = fileno(stream);
        RTSP_DEBUG("stream=%p ret=%d", stream, ret);
        return ret;
}

extern "C" int nkn_vfs_fseek(FILE *stream, long offset, int whence)
{
	if(IS_CACHED_FS(stream)) {
		return pfunc->pnkn_fseek(
			((struct rtsp_stream *)stream)->pfs,
			offset, whence);
	}
	RTSP_DEBUG("stream=%p whence=%d offset=%ld", 
			stream, whence, offset);
	return fseek(stream, offset, whence);
}

extern "C" long nkn_vfs_ftell(FILE *stream)
{
	long ret;
	if(IS_CACHED_FS(stream)) {
		return pfunc->pnkn_ftell(STREAM_TO_FILE(stream));
	}
	ret = ftell(stream);
	RTSP_DEBUG("stream=%p ret=%ld", stream, ret);
	return ret;
}

extern "C" void nkn_vfs_rewind(FILE *stream)
{
	if(IS_CACHED_FS(stream)) {
		return pfunc->pnkn_rewind(STREAM_TO_FILE(stream));
	}
	RTSP_DEBUG("stream=%p", stream);
	return rewind(stream);
}

extern "C" int nkn_vfs_fgetpos(FILE *stream, fpos_t *pos)
{
	if(IS_CACHED_FS(stream)) {
		return pfunc->pnkn_fgetpos(STREAM_TO_FILE(stream), pos);
	}
	RTSP_DEBUG("stream=%p", stream);
	return fgetpos(stream, pos);
}

extern "C" int nkn_vfs_fsetpos(FILE *stream, fpos_t *pos)
{
	if(IS_CACHED_FS(stream)) {
		return pfunc->pnkn_fsetpos(STREAM_TO_FILE(stream), pos);
	}
	RTSP_DEBUG("stream=%p", stream);
	return fsetpos(stream, pos);
}

extern "C" size_t nkn_vfs_fread(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
	size_t ret;
	struct timespec selfTimeStart, selfTimeEnd;

	if(IS_CACHED_FS(stream)) {
		ret = pfunc->pnkn_fread( ptr, size, nmemb, STREAM_TO_FILE(stream));
		RTSP_DEBUG("stream=%p size=%ld nmemb=%ld ret=%ld ...", 
			stream, size, nmemb, ret);
		return ret;
	}
	ret = fread(ptr, size, nmemb, stream);
	RTSP_DEBUG("stream=%p size=%ld nmemb=%ld ret=%ld", 
		stream, size, nmemb, ret);
	return ret;
}

extern "C" size_t nkn_vfs_fread_opt(void *ptr, size_t size, size_t nmemb, FILE *stream,
				    void **temp_ptr, int *free_buf)
{
	size_t ret;
	struct timespec selfTimeStart, selfTimeEnd;

	if(IS_CACHED_FS(stream)) {
		ret = pfunc->pnkn_fread_opt( ptr, size, nmemb, STREAM_TO_FILE(stream),
					     temp_ptr, free_buf);
		RTSP_DEBUG("stream=%p size=%ld nmemb=%ld ret=%ld ...", 
			stream, size, nmemb, ret);
		return ret;
	}
	ret = fread(ptr, size, nmemb, stream);
	RTSP_DEBUG("stream=%p size=%ld nmemb=%ld ret=%ld", 
		stream, size, nmemb, ret);
	return ret;
}


extern "C" size_t nkn_vfs_fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream)
{
	if(IS_CACHED_FS(stream)) {
		return pfunc->pnkn_fwrite(ptr, size, nmemb, STREAM_TO_FILE(stream));
	}
	RTSP_DEBUG("%s stream=%p\n", __FUNCTION__, stream);
	return fwrite(ptr, size, nmemb, stream);
}

extern "C" off_t nkn_vfs_ftello(FILE *stream)
{
	off_t ret;

	if(IS_CACHED_FS(stream)) {
		ret = pfunc->pnkn_ftello(STREAM_TO_FILE(stream));
		RTSP_DEBUG("%s stream=%p ret =%ld ...\n", __FUNCTION__, stream, ret);
		return ret;
	}
	ret = ftello(stream);
	RTSP_DEBUG("%s stream=%p ret =%ld\n", __FUNCTION__, stream, ret);
	return ret;
}

extern "C" int nkn_vfs_fseeko(FILE *stream, off_t offset, int whence)
{
	int ret;

	if(IS_CACHED_FS(stream)) {
		return pfunc->pnkn_fseeko(STREAM_TO_FILE(stream), offset, whence);
	}
	ret = fseeko(stream, offset, whence);
	RTSP_DEBUG("stream=%p whence=%d offset=%ld ret=%d", 
			stream, whence, offset, ret);
	return ret;
}

extern "C" int nkn_vfs_fputc(int c, FILE *stream)
{
	if(IS_CACHED_FS(stream)) {
		return pfunc->pnkn_fputc(c, STREAM_TO_FILE(stream));
	}
	RTSP_DEBUG("stream=%p", stream);
	return fputc(c, stream);
}

extern "C" int nkn_vfs_fputs(const char *s, FILE *stream)
{
	if(IS_CACHED_FS(stream)) {
		return pfunc->pnkn_fputs(s, STREAM_TO_FILE(stream));
	}
	RTSP_DEBUG("stream=%p", stream);
	return fputs(s, stream);
}

extern "C" int nkn_vfs_fgetc(FILE *stream)
{
	if(IS_CACHED_FS(stream)) {
		return pfunc->pnkn_fgetc(STREAM_TO_FILE(stream));
	}
	RTSP_DEBUG("stream=%p", stream);
	return fgetc(stream);
}

extern "C" int nkn_vfs_stat(const char *path, struct stat *buf)
{
	int ret;

	if(*pfunc->prtsp_dm2_enable) {
		ret = pfunc->pnkn_stat(path, buf);
		if(ret == 0) return ret; // Success
	}

	ret = stat(path, buf);
	RTSP_DEBUG("size=%ld", buf->st_size);
	return ret;
}

extern "C" int nkn_vfs_fstat(int filedes, struct stat *buf)
{
	return fstat(filedes, buf);
}

extern "C" int nkn_vfs_lstat(const char *path, struct stat *buf)
{
	return lstat(path, buf);
}

extern "C" char *nkn_vfs_fgets(char *s, int size, FILE *stream)
{
	if(IS_CACHED_FS(stream)) {
		return pfunc->pnkn_fgets(s, size, STREAM_TO_FILE(stream));
	}
	RTSP_DEBUG("stream=%p", stream);
	return fgets(s, size, stream);
}

extern "C" int nkn_vfs_fclose(FILE *fp)
{
	int ret;
	if(IS_CACHED_FS(fp)) {
		struct rtsp_stream * p = (struct rtsp_stream *)fp;
		FILE * pfs = p->pfs;

		RTSP_DEBUG("stream=%p ret=0 ....", fp);
		pthread_mutex_lock(&rtsp_mutex);
		pfunc->pnkn_fclose(pfs);
		p->used = 0;
		p->pfs = NULL;
		pthread_mutex_unlock(&rtsp_mutex);
		return 0;
	}
	ret = fclose(fp);
	RTSP_DEBUG("stream=%p ret=%d", fp, ret);
	return ret;
}

extern "C" FILE *nkn_vfs_fopen(const char *path, const char *mode)
{
	FILE * pfs;
	int i;

	pthread_mutex_lock(&rtsp_mutex);
	if(*pfunc->prtsp_dm2_enable) {
		pfs = pfunc->pnkn_fopen(path, mode);
	}
	else {
		pfs = NULL;
	}
	if(pfs == NULL) {
		RTSP_DEBUG("path=%s", path);
		pthread_mutex_unlock(&rtsp_mutex);
		return fopen(path, mode);
	}

	for(i=0; i<MAX_STREAM; i++) {
		if(g_streams[i].used == 1) continue;
		g_streams[i].used = 1;
		g_streams[i].pfs = pfs;
		pthread_mutex_unlock(&rtsp_mutex);

		return (FILE *)&g_streams[i];
	}
	if(i == MAX_STREAM) {
    	        pthread_mutex_unlock(&rtsp_mutex);
		return NULL;
	}
	pfunc->pnkn_fclose(pfs);
	pthread_mutex_unlock(&rtsp_mutex);
	return fopen(path, mode);
}

extern "C" int nkn_vfs_fflush(FILE *stream)
{
	int ret;
	if(IS_CACHED_FS(stream)) {
		return pfunc->pnkn_fflush(STREAM_TO_FILE(stream));
	}
	ret = fflush(stream);
	RTSP_DEBUG("stream=%p ret=%d", stream, ret);
	return ret;
}

void init_vfs_init(struct vfs_funcs_t * func);
void init_vfs_init(struct vfs_funcs_t * func)
{
	memset((char *)&g_streams[0], 0, MAX_STREAM * sizeof(struct rtsp_stream));
	pfunc = func;
	prtsp_nkn_callback = (struct rtsp_nkn_callback_t *)(func->pnkn_callback);
}


/*
 * The next two functions are for real time scheduler.
 */

extern "C" nkn_provider_type_t nkn_vfs_get_last_provider(FILE *stream)
{
    if(IS_CACHED_FS(stream)) {
	return pfunc->pnkn_vfs_get_last_provider(STREAM_TO_FILE(stream));
    } else {
	return (nkn_provider_type_t)0;
    }

}


#if 0
extern "C" size_t nkn_vfs_get_file_size (FILE *stream)
{

    if(IS_CACHED_FS(stream)) {
        return pfunc->pnkn_vfs_get_file_size(STREAM_TO_FILE(stream));
    } else {
        return (nkn_provider_type_t)0;
    }
}

extern "C" size_t nkn_vfs_set_file_size (FILE *stream, size_t size)
{

    if(IS_CACHED_FS(stream)) {
        return pfunc->pnkn_vfs_set_file_size(STREAM_TO_FILE(stream), size);
    } else {
        return (nkn_provider_type_t)0;
    }
}
#endif

extern "C" TaskToken scheduleDelayedTask(int64_t microseconds, 
				TaskFunc* proc,
				void* clientData)
{
        return pfunc->pnkn_scheduleDelayedTask(microseconds, proc, clientData);
}

extern "C" void unscheduleDelayedTask(TaskToken prevTask)
{
	pfunc->pnkn_unscheduleDelayedTask(prevTask);
}

extern "C" void updateRTSPCounters(uint64_t bytesSent)
{
	pfunc->pnkn_updateRTSPCounters(bytesSent);
}

