#ifndef NKN_REPLACE_HH
#define NKN_REPLACE_HH

#include "stdio.h"
#include "stdlib.h"

#ifdef __cplusplus 
extern "C" {
#endif

 void nkn_vfs_clearerr(FILE *stream);
 int nkn_vfs_feof(FILE *stream);
 int nkn_vfs_ferror(FILE *stream);
 int nkn_vfs_fileno(FILE *stream);
 int nkn_vfs_fseek(FILE *stream, long offset, int whence);
 long nkn_vfs_ftell(FILE *stream);
 void nkn_vfs_rewind(FILE *stream);
 int nkn_vfs_fgetpos(FILE *stream, fpos_t *pos);
 int nkn_vfs_fsetpos(FILE *stream, fpos_t *pos);
 size_t nkn_vfs_fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
 size_t nkn_vfs_fread_opt(void *ptr, size_t size, size_t nmemb, FILE *stream,
			  void *tmp_ptr, int *free_buf);
 size_t nkn_vfs_fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
 off_t nkn_vfs_ftello(FILE *stream);
 int nkn_vfs_fseeko(FILE *stream, off_t offset, int whence);
 int nkn_vfs_fputc(int c, FILE *stream);
 int nkn_vfs_fputs(const char *s, FILE *stream);
 int nkn_vfs_fgetc(FILE *stream);
 char *nkn_vfs_fgets(char *s, int size, FILE *stream);
 int nkn_vfs_stat(const char *path, struct stat *buf);
 int nkn_vfs_fstat(int filedes, struct stat *buf);
 int nkn_vfs_lstat(const char *path, struct stat *buf);
 int nkn_vfs_fclose(FILE *fp);
 FILE *nkn_vfs_fopen(const char *path, const char *mode);
 int nkn_vfs_fflush(FILE *stream);
 nkn_provider_type_t nkn_vfs_get_last_provider(FILE *stream);
 size_t nkn_vfs_get_file_size (FILE *stream);
    size_t nkn_vfs_set_file_size (FILE *stream, size_t size);

 TaskToken scheduleDelayedTask(int64_t microseconds,
                                TaskFunc* proc,
                                void* clientData);

 void unscheduleDelayedTask(TaskToken prevTask);
 void updateRTSPCounters(uint64_t bytesSent);

#ifdef __cplusplus
} //cpluplus externs
#endif

#endif // NKN_REPLACE_HH
