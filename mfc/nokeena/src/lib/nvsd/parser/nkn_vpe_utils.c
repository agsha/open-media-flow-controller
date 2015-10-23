#include "nkn_vpe_utils.h"

#include <sys/stat.h>
#include <dirent.h>


int32_t recursive_mkdir(const char *const path, int32_t mode)
{
	char *p, *tmp, *rem = NULL;
	struct stat st;
	int32_t rv;

	/* test if there is a file with the name of this directory already
	 * existing 
	 */
	if (!stat(path, &st)) {
		if (!S_ISDIR(st.st_mode)) {
			// Given path's existing type is not DIR
			return -1;
		}
	}
	p = tmp = strdup(path);
	/* skip '/' at the start */
	while(*p == '/') {
		p++;
	}

	rem = p;
	while((p = strchr(p, '/'))) {
		*p = '\0';
		if (!stat(tmp, &st)) {
			if (!S_ISDIR(st.st_mode)) {
				free(tmp);
				return -1;
			}
		} else {
			rv = mkdir(tmp, mode);
			chmod(tmp, mode);
			if (rv) {
				free(tmp);
				return -1;
			}
		}	    
		/* restore the slash */
		*p = '/';

		/* skip trailing slash */
		while (*p == '/') {
			p++;
		}
		rem = p;
	}

	if (*rem != '\0') { // if last dir name is not followed by "/"
		rv = mkdir(tmp, mode);
		if ((rv < 0) && (errno != EEXIST)) {
			free(tmp);
			return -1;
		}
		chmod(tmp, mode);
	}

	free(tmp);
	return 0;
}


int32_t traverse_tree_dir(char const* dir_path, int32_t level,
		traverse_hdlr_fptr traverse_hdlr, void* traverse_ctx) {

	if (level < 0)
		return 0;
	DIR *dp;
	struct dirent *dir_ent;
	char file_uri[FILENAME_MAX];
	dp = opendir(dir_path);

	if (dp != NULL) {
		while ((dir_ent = readdir(dp)) != NULL) {
			struct stat file_info;
			snprintf(file_uri,FILENAME_MAX, "%s/%s", dir_path, dir_ent->d_name);
			if (lstat(file_uri, &file_info) < 0) {
				perror("lstat failed: ");
				return -1;
			}
			if (S_ISDIR(file_info.st_mode)) {
				if (strcmp(dir_ent->d_name, ".") &&
						strcmp(dir_ent->d_name, "..")) {
					if (traverse_tree_dir(file_uri, level-1, traverse_hdlr,
								traverse_ctx) < 0)
						return -1;
				}
			} else {
				traverse_hdlr(traverse_ctx, dir_path, dir_ent->d_name);
			}
		}
		closedir(dp);
	} else {
		perror("opendir failed: ");
		return -1;
	}

	traverse_hdlr(traverse_ctx, dir_path, NULL);
	return 0;
}


void *
nkn_vpe_fopen(char *p_data, const char *mode, size_t size)
{
    size = size;
    return (void *)fopen(p_data, mode);
}

size_t
nkn_vpe_fseek(void *desc, size_t seek, size_t whence)
{
    FILE *f = (FILE*)desc;
    return fseek(f, seek, whence);
}

size_t
nkn_vpe_ftell(void *desc)
{
    FILE *f = (FILE*)desc;
    return ftell(f);
}

void
nkn_vpe_fclose(void *desc)
{
    FILE *f = (FILE*)desc;
    fclose(f);

    return;
}

size_t
nkn_vpe_fwrite(void *buf, size_t n, size_t size, void *desc)
{
    FILE *f = (FILE*)desc;
    return fwrite(buf, n, size, f);
}

size_t
nkn_vpe_fread(void *buf, size_t n, size_t size, void *desc)
{
    FILE *f = (FILE*)desc;

    return fread(buf, n, size, f);
}


///

void * pio_open(char *p_data, const char *mode, size_t size) {

	/* NOTE: Currently this API does not use mode and size parm. It uses a
	   fixed set of flags and mode values.

	   Next step: Parameters passed need to be made compatible for all forms
	   of IO Handlers (fopen, primitive open and safe io)
	   */
	int32_t fid = open((char*)p_data, O_WRONLY | O_RDONLY |
			O_CREAT | O_ASYNC | O_TRUNC,
			S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
	int64_t* fd = malloc(sizeof(int64_t));
	*fd = fid;
	return (void*)fd;
}

size_t pio_seek(void *desc, size_t seek, size_t whence) {

    int64_t *fd = (int64_t*)desc;
	int32_t fid = *fd;
    size_t rc = lseek(fid, seek, whence);
	return rc;
}

size_t pio_tell(void *desc) {

	size_t rc = pio_seek(desc, 0, SEEK_CUR);
	return rc;
}

void pio_close(void *desc) {

    int64_t *fd = (int64_t*)desc;
	int32_t fid = *fd;
    close(fid);
	free(fd);
    return;
}

size_t pio_write (void *buf, size_t n, size_t size, void *desc) {

    int64_t *fd = (int64_t*)desc;
	int32_t fid = *fd;
	size_t rc = write(fid, buf, n*size);
	return rc;
}

size_t pio_read(void *buf, size_t n, size_t size, void *desc) {

    int64_t *fd = (int64_t*)desc;
	int32_t fid = *fd;
    size_t rc = read(fid, buf, n*size);
	return rc;
}

uint64_t nkn_memstat_type(nkn_obj_type_t type){
	return (*obj_type_data[type].cnt);
}

