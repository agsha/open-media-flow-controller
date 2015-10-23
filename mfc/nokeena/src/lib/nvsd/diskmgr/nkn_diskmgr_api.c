/*
	COPYRIGHT: NOKEENA NETWORKS
	AUTHOR: Vikram Venkataraghavan
*/
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#ifdef HAVE_FLOCK
#  include <sys/file.h>
#endif
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/uio.h>
#define USE_SPRINTF
#include "nkn_diskmgr_api.h"
#include "nkn_mediamgr_api.h"
#include <glib.h>
#include "nkn_hash_map.h"
#include "nkn_locmgr_extent.h"
#include "nkn_locmgr_uri.h"
#include "nkn_locmgr_physid.h"
#include "nkn_defs.h"
#include "cache_mgr.h"
#include "nkn_trace.h"
#if 0
#include "nkn_assert.h"
#else
/* TBD:NKN_ASSERT */
/* The NKN_ASSERT macro definition is not working properly in this file */
#define NKN_ASSERT(_e)
#include <assert.h>
#endif

extern void nkn_locmgr_file_tbl_cleanup(void);

#define NO_FILE_STR "no_file_name_assigned\0"

int dm_api_debug = 0;

struct dm_cache_info {
	char dirname[NKN_MAX_DIR_NAME_SZ];
	uint64_t cur_cache_bytes_filled; /* Needs to hold values of Gigabytes*/
	uint64_t max_cache_size;
	int disk_space_exceeded;
};


static int s_input_cur_write_dir_idx;
static struct dm_cache_info g_cache_info[NKN_DM_MAX_CACHES];
int glob_dm_num_map_entries = 0;
int glob_dm_num_locmgr_entries = 0;
int glob_num_caches = 0;
int glob_dm_tot_map_read = 0;
int s_disk_space_exceeded = 0;
int glob_dm_file_open_err = 0;
int glob_dm_file_lseek_err = 0;
int glob_dm_file_read_err = 0;

int glob_dm_stat_err = 0;
int glob_dm_stat_cnt = 0;
int glob_dm_stat_not_found = 0;
int glob_dm_get_cnt = 0;
int glob_dm_get_err = 0;
int glob_dm_put_cnt;
int glob_dm_put_err;

int glob_dm_num_files_open = 0;
int glob_dm_num_files_closed = 0;
int glob_dm_num_open_err = 0;
int glob_dm_num_seek_err = 0;
int glob_dm_num_read_err = 0;

int glob_dm_num_move_files = 0;
int glob_dm_num_other_files = 0;

static struct LM_extent *
	s_find_extent_by_offset(GList *in_ext_list, uint64_t uri_offset,
				uint64_t len, uint64_t *tot_len);

static int s_rr_disk_caches(void);

int DM_delete(MM_delete_resp_t *delete);
int DM_discover(struct mm_provider_priv *p);
int DM_evict(void);
int DM_provider_stat (MM_provider_stat_t *tmp);
int DM_get(MM_get_resp_t *in_ptr_resp);
int DM_shutdown(void);

void s_print_clip_struct(struct DM_clip_location *clip, FILE *file);

void
dm_api_dump_counters()
{
	printf("\nglob_dm_num_map_entries = %d", glob_dm_num_map_entries);
	printf("\nglob_dm_num_locmgr_entries = %d", glob_dm_num_locmgr_entries);
	printf("\nglob_num_caches = %d", glob_num_caches);
	printf("\nglob_dm_tot_map_read = %d", glob_dm_tot_map_read);
	printf("\ns_disk_space_exceeded = %d", s_disk_space_exceeded);
	printf("\nglob_dm_num_files_open = %d", glob_dm_num_files_open);
	printf("\nglob_dm_num_files_closed = %d", glob_dm_num_files_closed);
	printf("\n glob_dm_file_open_err = %d", glob_dm_file_open_err);
	printf("\n glob_dm_file_lseek_err = %d", glob_dm_file_lseek_err);
	printf("\n glob_dm_file_read_err = %d", glob_dm_file_read_err);

	printf("\nglob_dm_stat_err = %d", glob_dm_stat_err);
	printf("\nglob_dm_stat_cnt = %d", glob_dm_stat_cnt);
	printf("\nglob_dm_get_cnt = %d", glob_dm_get_cnt);
	printf("\nglob_dm_get_err = %d", glob_dm_get_err);
}

static void
s_init_cache_helper(char     *dirname,
		    int      index,
		    uint64_t size)
{
	int i = index;

	sprintf(g_cache_info[i].dirname, "%s", dirname);
	g_cache_info[i].cur_cache_bytes_filled = 0;
	g_cache_info[i].max_cache_size = size;
	g_cache_info[i].disk_space_exceeded = 0;
	printf("\ndirname: %s", g_cache_info[i].dirname);
}

static int
s_init_caches(const char *a_conf_file,
	      const char *needed_manager)
{
    FILE *file = NULL;
    struct stat sb;
    int i = 0;
    char dirname[NKN_MAX_FILE_NAME_SZ];
    char devname[80];
    int size = 0;
    char  manager[20];
    int ret;
    float fsize;

    file = fopen(a_conf_file, "r");
    if (file == NULL) {
	printf("\n%s(): ERROR: Cannot open file", __FUNCTION__);
	return -1;
    }
    else {
	printf("\n%s(): File opened successfully", __FUNCTION__);
    }

    while(!feof(file)) {
	ret = fscanf(file, "%s %s %f %s\n",
		     dirname, devname, &fsize, manager);
	if (ret == EOF)
	    break;
	if (ret != 4) {
	    printf("\n\n%s(): Illegal format for config file: %s\n",
		   __FUNCTION__, a_conf_file);
	    NKN_ASSERT(ret == 4);
	    return 0;
	}
	if (strcmp(manager, NKN_DM_MGR) != 0 &&		// no match
	    strcmp(manager, NKN_DM2_MGR) != 0) {
	    printf("\n\n%s(): Illegal format for config file: %s\n",
		   __FUNCTION__, a_conf_file);
	    NKN_ASSERT(0);
	    return 0;
	}
	if (strcmp(manager, needed_manager) != 0)	// no match
	    continue;
	if (stat(devname, &sb) < 0) {
	    printf("Stat on devname (%s) failed: %d\n", devname, errno);
	    continue;
	}
	if (! S_ISBLK(sb.st_mode)) {
	    printf("Device name is not a block device: 0x%x\n", sb.st_mode);
	    continue;
	}
	if (stat(dirname, &sb) < 0) {
	    printf("Failed to find mount point: %s\n", dirname);
	    continue;
	}
	printf("Mounting %s on %s\n", devname, dirname);
#if 0
	if ((ret = mount(devname, dirname, "ext3", MS_NOATIME, NULL)) < 0) {
	    if (errno != EBUSY) {
		printf("Mount-1 failed for (%s)/(%s): %d\n",
		       devname, dirname, errno);
		continue;
	    }
	    /* We don't know what is mounted, so we need to unmount */
	    printf("Unmounting %s\n", dirname);
	    if ((ret = umount(dirname)) < 0) {
		printf("umount failed for %s: %d\n", dirname, errno);
		continue;
	    }
	    printf("Mounting %s on %s\n", devname, dirname);
	    ret = mount(devname, dirname, "ext3", MS_NOATIME, NULL);
	    if (ret < 0) {
		printf("Mount-2 failed for (%s)/(%s): %d\n",
		       devname, dirname, errno);
		continue;
	    }
	}
#endif
	size = fsize;
	s_init_cache_helper(dirname, i, size);
	i++;

	if(i >= NKN_DM_MAX_CACHES) {
	    printf("\n%s(): Exceeded MAX number of caches",
		   __FUNCTION__);
	    break;
	}
    }

    printf("\n%s(): Number of filenames fetched: %d", __FUNCTION__, i);
    glob_num_caches = i;

    fclose(file);

    return i;
}

static uint32_t
s_get_file_size(char *filename)
{
	struct stat file_stat;

	if (!stat(filename, &file_stat)) {
		return file_stat.st_size;
	}
	return 0;
}

static int
s_find_file_in_cache_dir(struct MM_put_data *map)
{
	char tmp_filename[NKN_MAX_FILE_NAME_SZ];
	int  i;

	for (i = 0; i < glob_num_caches; i++) {
		sprintf(tmp_filename, "%s/%s",
			g_cache_info[s_input_cur_write_dir_idx].dirname,
			map->objname);

		if (s_get_file_size(tmp_filename) > 0) {
			memcpy(map->clip_filename, tmp_filename,
			       strlen(tmp_filename));
			return 0;
		}
	}
	return -1;
}

static int
s_create_new_map_file(char *filename)
{
	FILE *file;

	file = fopen(filename, "a+");
	if (file == NULL) {
		printf("\n%s(): ERROR: Could not create file. Create file",
		       __FUNCTION__);
		return -2;
	}

	fclose(file);
	return 0;
}

static FILE *
s_open_file_for_read(char *filename)
{
	FILE *file;
	file = fopen(filename, "r");
	if (file == NULL) {
		return NULL;
	}
	return file;
}

static FILE *
s_open_file_for_append(char *filename)
{
	FILE *file;
	file = fopen(filename, "a");
	if (file == NULL) {
		return NULL;
	}
	return file;
}

static void
s_close_file(FILE *file)
{
	fclose(file);
}

void
s_print_clip_struct(struct DM_clip_location *clip,
		    FILE		    *file)
{
	fprintf(file, "\n--------- Printing Clip structure");
	fprintf(file, "\n storage_file: %s", clip->storage_file);
	fprintf(file, "\n offset: %lld length: %lld",
		clip->offlen.offset, clip->offlen.length);
	fprintf(file, "\n uri_file: %s", clip->uri_file);
	fprintf(file, "\n--------- END Printing Clip structure \n");
}

static uint64_t
s_lm_get_file_offset(char *filename)
{
	struct LM_file *file = NULL;

	file = (struct LM_file *) nkn_locmgr_file_tbl_get(filename);

	if (file == NULL) {
		return 0;
	}

	return (file->cur_offset);
}

static uint64_t
s_populate_next_file_offset(char     *filename,
			    uint64_t in_length,
			    uint64_t in_offset)
{
        struct LM_file *file = NULL;
        uint64_t offset = 0;

        file = (struct LM_file *) nkn_locmgr_file_tbl_get(filename);

        if (file == NULL) {
                file = (struct LM_file *) nkn_calloc_type(1,
					sizeof(struct LM_file),
					mod_dm_LM_file_t);
                assert(file);
                file->cur_offset = 0;
                file->cur_block = 0;
                nkn_locmgr_file_tbl_insert(filename, file);
        }
	/* This may happen if data is recd out of order in a file */
	else if (file->cur_offset != in_offset)
		file->cur_offset = in_offset;

	file->cur_offset += in_length;

	if ((file->cur_offset & CM_MEM_PAGE_MASK) == 0) {
		/* Page is aligned. Nothing to do*/
		offset = file->cur_offset;
	}
	else {
		/* Align to PAGE_SIZE specified by CM */
		offset = ((file->cur_offset + CM_MEM_PAGE_SIZE) &
			  ~CM_MEM_PAGE_MASK);
		file->cur_offset = offset;
	}

	file->cur_block = offset / (NKN_DM_BLK_SIZE_KB * 1024);

	return offset;
}

/* TBD: Revisit this */
static void
s_populate_construct_physid(char     *filename,
			    uint32_t block,
			    char     **out_physid)
{
	char temp[4096];
	int len;

	sprintf(temp, "%s_%d", filename, block);
	len = strlen(temp);

	*out_physid = (char *) nkn_malloc_type(len + 1, mod_dm_strbuf);

	memcpy(*out_physid, temp, len);
	(*out_physid)[len] = '\0';
}

static void
s_populate_extent_by_physid(char	*uriname,
			    uint64_t	in_file_offset,
			    uint64_t	in_uri_offset,
			    uint64_t	in_length,
			    char	*in_filename,
			    uint32_t	in_blk_id,
			    uint64_t	in_total_len)
{
	struct LM_extent *ptr_ext = NULL;
	char *physid;

	ptr_ext = (struct LM_extent *) nkn_calloc_type(1,
					sizeof(struct LM_extent),
					mod_dm_LM_extent_t);
	assert(ptr_ext);

	ptr_ext->inUse = 1;
	s_populate_construct_physid(in_filename, in_blk_id, &physid);

	memcpy(ptr_ext->physid, physid, strlen(physid));
	free(physid);
	ptr_ext->uol.length = in_length;
	ptr_ext->file_offset = in_file_offset;
	ptr_ext->tot_content_len = in_total_len;

	ptr_ext->uol.uri = (char *) nkn_calloc_type(1, NKN_MAX_FILE_NAME_SZ, 
						mod_dm_uri_t);
	assert(ptr_ext->uol.uri);
	memcpy(ptr_ext->uol.uri, uriname, strlen(uriname));

	memcpy(&ptr_ext->filename[0], in_filename, strlen(in_filename));

	/* Set the uri offset */
	nkn_lm_set_extent_offset(ptr_ext->uol.uri, ptr_ext, in_uri_offset);
	nkn_lm_extent_insert_by_uri(ptr_ext->uol.uri, ptr_ext);
	nkn_lm_extent_insert_by_physid(ptr_ext->physid, ptr_ext);
}


/*
	- This function constructs a physid given the uriname, filename,
	  offset, and length.
*/
static uint64_t
s_populate_new_uri(char		*uriname,
		   char		*filename,
		   uint64_t	in_file_offset,
		   uint64_t	*in_uri_offset,
		   int		*partial_len,
		   uint64_t	total_len,
		   uint32_t	*out_write_len)
{
        uint64_t file_offset = 0;
	uint64_t cur_block = 0;
	uint64_t new_offset = 0;
	uint32_t blk_boundary;
	uint32_t write_len = 0;

	file_offset = in_file_offset;
	cur_block = in_file_offset / (NKN_DM_BLK_SIZE_KB * 1024);

	if (*partial_len > 0) {

		blk_boundary = ((cur_block * NKN_DM_BLK_SIZE_KB * 1024) +
                        (NKN_DM_BLK_SIZE_KB * 1024));

		if ((file_offset + *partial_len) > (uint64_t)blk_boundary) {

			write_len = (blk_boundary - file_offset);

			s_populate_extent_by_physid(uriname,
				file_offset, *in_uri_offset,
				write_len, filename,
				cur_block, total_len);

			*in_uri_offset += write_len;
			*out_write_len = write_len;
			*partial_len -= write_len;
		}
		else {
			/* handle less than 1 page */
			s_populate_extent_by_physid(uriname, file_offset,
				*in_uri_offset, *partial_len, filename,
				cur_block, total_len);

			/* Uri offset does not change in this case.*/
			*in_uri_offset += *partial_len;
			*out_write_len = *partial_len;
			*partial_len = 0;
		}

		/* Find the next page offset */
		new_offset = s_populate_next_file_offset(filename,
							 *out_write_len,
						    	 file_offset);
	}
	return new_offset;
}

static int
s_populate_loc_mgr(char *dirname,
		   char *map_filename,
		   int  *num_entries)
{
	MM_put_data_t	map;
	char		clip_filename[NKN_MAX_FILE_NAME_SZ];
	uint64_t	file_offset;
	uint64_t	uri_offset;
	int32_t		partial_len;
	uint32_t	dummy;
	FILE		*file = NULL;
	int		count = 0;

	*num_entries = 0;

	file = s_open_file_for_read(map_filename);
	if(file == NULL) {
		printf("\n%s(): Cannot open file", __FUNCTION__);
		return -1;
	}

	while(!feof(file)) {
		glob_dm_tot_map_read ++;

		fscanf(file, "%s %s %ld %ld %lu\n", map.objname, map.uriname,
		       &map.clip_offset, &map.content_length, &file_offset);

		g_cache_info[s_input_cur_write_dir_idx].cur_cache_bytes_filled
			+= map.content_length;

		sprintf(&clip_filename[0], "%s/%s", dirname, map.objname);
		memcpy(map.clip_filename, clip_filename, strlen(clip_filename));
		map.clip_filename[strlen(clip_filename)] = '\0';

		//s_print_map(&map);

		partial_len = map.content_length;

		/* The offset in the file for this URI */

		/* Usually this number is 0 but OM could get URIs in a
		   fragmented way. */
		uri_offset = map.clip_offset;

		while(partial_len > 0) {
			file_offset =
				s_populate_new_uri(map.uriname,
					clip_filename, file_offset,
					&uri_offset, &partial_len,
					map.content_length, &dummy);
		}
		(*num_entries)++;

		glob_dm_num_locmgr_entries ++;
		count ++;
	}
	s_close_file(file);
	nkn_locmgr_uri_tbl_print();
	nkn_locmgr_physid_tbl_print();
	printf("\n ------- Nokeena DM file import finished -----------");

	return 0;
}

/**
   This function does the following actions:
   1. Takes as input an array of strings:
	- /Cache1
	- /Cache2
	- etc...
      	- This array of string are the names of the formatted and mounted
	  files which represent all the disks in the system (after mkfs
	  and mount commands).
   2. This function will try to open a file called nkn_loc_map in each
      in each of these directories.
   3. If the file does not exist, this function will create the file.
   4. nkn_loc_map: this file will contain the map of the following:
      - <Vn, Pn, Sn> --> <File, offset, length>
      - one entry in each line.
   5. If the file exists, this function will read the file and populate
      the Location Manager hash map with the above information.
 */
int
DM_init(void)
{
	char mapfilename[NKN_MAX_FILE_NAME_SZ];
	char dirname[NKN_MAX_FILE_NAME_SZ];
	int num_map_entries;
	int num_caches;
	uint32_t file_size;
	int ret_val = -1;
	int i;

return 0;
	nkn_locmgr_file_tbl_init();
	nkn_locmgr_uri_tbl_init();
	nkn_locmgr_physid_tbl_init();

	num_caches = s_init_caches("/config/nkn/diskcache.info", NKN_DM_MGR);

	for (i = 0; i < num_caches; i++) {
		memcpy(dirname, g_cache_info[i].dirname,
		       strlen(g_cache_info[i].dirname));
		dirname[strlen(g_cache_info[i].dirname)] = '\0';
		printf("\n %s(): filename: %s",  __FUNCTION__, dirname);
		sprintf(&mapfilename[0], "%s/nkn_map_file.txt", dirname);
		printf("\n %s(): mapfilename: %s len = %d",
		       __FUNCTION__, mapfilename, (int)strlen(mapfilename));

		file_size = s_get_file_size(mapfilename);

		num_map_entries = 0;
		if (file_size != 0) {
			s_populate_loc_mgr(dirname, mapfilename,
					   &num_map_entries);
			printf("\n%s(): Number of map entries: %d\n",
			       __FUNCTION__, num_map_entries);
		} else {
			printf("\n%s(): CREATING NEW FILE.", __FUNCTION__);
			s_create_new_map_file(mapfilename);
		}
	}

#if 0
	ret_val = MM_register_provider(SASDiskMgr_provider, 0,
				       DM_put, DM_stat, DM_get,
				       DM_delete, DM_shutdown, DM_discover, 
				       DM_evict, DM_provider_stat, 
				       NULL, NULL, 10);
#endif

	return ret_val;
}	/* DM_init */

static int
s_input_check_cache_space(struct MM_put_data *map)
{
	uint64_t cur_cache_sz = 0;
	uint64_t max_cache_sz = 0; /* convert GB to Bytes */
	uint64_t tmp_cur_size = 0;

	cur_cache_sz =
		g_cache_info[s_input_cur_write_dir_idx].cur_cache_bytes_filled;
	max_cache_sz =
		g_cache_info[s_input_cur_write_dir_idx].max_cache_size *
		(1024 * 1024 * 1024);

	tmp_cur_size = cur_cache_sz + map->content_length;

	if ((cur_cache_sz + map->content_length) < max_cache_sz) {
		return 0;
	}
	else {
		return -1;
	}
}

static void
s_update_cache_sz(struct MM_put_data *map)
{
	g_cache_info[s_input_cur_write_dir_idx].cur_cache_bytes_filled +=
		map->content_length;
}

/**
	This function will get the next free block number and segment
	number in a particular file in a cache.
	At the moment, there is no limit to seg_num. When we do, raw disk,
	there will be a limit.
*/
static int
s_input_get_cache_space(struct MM_put_data *map)
{
	uint64_t cur_cache_sz = 0;
	int ret_val = -1;
	int i, found = 0;

	for(i = 0; i < glob_num_caches; i++) {
        	ret_val = s_rr_disk_caches();
        	if(ret_val < 0) {
			s_disk_space_exceeded = 1;
        		return -1;
        	}

		cur_cache_sz =
			g_cache_info[s_input_cur_write_dir_idx].cur_cache_bytes_filled;

		ret_val = s_input_check_cache_space(map);
		if(ret_val == 0) {
			cur_cache_sz += map->content_length;

			sprintf(map->clip_filename, "%s/%s",
				g_cache_info[s_input_cur_write_dir_idx].dirname,
				map->objname);

			found = 1;
			break;
		}
		else {
			/* TBD: Can probably squeeze more data in.*/
			g_cache_info[s_input_cur_write_dir_idx].disk_space_exceeded = 1;
		}
	}

	if(found == 1)
		return 0;
	else
		return -1;
}

static void
s_input_write_map_data(struct MM_put_data *map,
		       uint64_t		  file_offset)
{
	FILE *file;
	char mapfilename[NKN_MAX_FILE_NAME_SZ];

	sprintf(&mapfilename[0], "%s/nkn_map_file.txt",
		g_cache_info[s_input_cur_write_dir_idx].dirname);

	file = s_open_file_for_append(mapfilename);

	glob_dm_num_map_entries++;
	fprintf(file, "%s %s %ld %ld %lu\n", map->objname,
		map->uriname, map->clip_offset,
		map->content_length, file_offset);

	s_close_file(file);
}

static uint64_t
s_input_get_file_offset(struct MM_put_data *map,
			video_types_t	   video_type,
			int		   *found)
{
	struct stat tmp_stat;
	uint64_t    old_file_offset = 0;
	int	    move_case = 0;
	int	    ret_stat = 0;

	if (video_type == NKN_VT_MOVE) {
		glob_dm_num_move_files++;
		move_case = 1;
	}
	else if (video_type == NKN_VT_OTHER){
		glob_dm_num_other_files++;
		move_case = 0;
	}
	else {
		assert(0);
	}

	ret_stat = stat(map->clip_filename, &tmp_stat);
	if (move_case == 1) {
		/* File offset could be the end of the file in case of MOVE */
		if (ret_stat < 0) {
			old_file_offset = 0;
			*found = 0;
		}
		else {
			old_file_offset =
				s_lm_get_file_offset(map->clip_filename);
			*found = 1;
		}
	}
	else {
		/* File offset is equal to the uri offset in the
		 * non-Move case because it is one big file
		 * to be written */
		old_file_offset = map->clip_offset;
		if (ret_stat < 0)
			*found = 0;
		else
			*found = 1;
	}
	return old_file_offset;
}

/* This function stores the content in the next available location */
static int
s_input_store_content_in_free_block(struct MM_put_data	*in_map,
				    video_types_t	video_type)
{
	MM_put_data_t	*map;
	uint64_t	uri_offset;
	uint64_t	new_file_offset;
	int64_t		old_file_offset;
	int64_t		retf;
	char		*write_data_ptr;
	int32_t		partial_len;
	int		open_fd;
	uint32_t	write_len = 0;
	int		found = 0;

	assert(in_map);
	map = in_map;

	assert(strcmp(map->clip_filename, NO_FILE_STR) != 0);

	partial_len = map->content_length;

	old_file_offset = s_input_get_file_offset(map, video_type, &found);

	/* URI offset is always set by OM */
	uri_offset = map->clip_offset;
	write_data_ptr = map->content_ptr;

	s_update_cache_sz(map);

	while (partial_len > 0) {
		if (s_disk_space_exceeded != 0) {
			printf("\n NKN ERROR: Disk Space Exceeded. ");
			return (-NKN_DM_DISK_SPACE_EXCEEDED);
		}

		open_fd = open(map->clip_filename, O_RDWR | O_CREAT, 0644);
		if (open_fd < 0) {
			return -1;
		}

		new_file_offset = s_populate_new_uri(
			map->uriname,
			map->clip_filename,
			old_file_offset, &uri_offset,
			&partial_len,
			map->content_length,
			&write_len);

		map->content_length = write_len;

		/* Seek to the offset from the start of the file */
		retf = lseek(open_fd, old_file_offset, SEEK_SET);
		assert(retf == old_file_offset);

		retf = write(open_fd, write_data_ptr, write_len);

		s_input_write_map_data(map, old_file_offset);

		old_file_offset = new_file_offset;

		if (partial_len != 0) {
			/* More data to write; skipped in the last round due
			 * to alignment. */
			map->content_length = partial_len;
			write_data_ptr += write_len;
			map->clip_offset = uri_offset;
		}
		close(open_fd);
	}
	return NKN_DM_OK;
}

void
DM_print_stats(void)
{
	int i;

	for(i = 0; i < glob_num_caches; i++) {
		printf("\nCur Cache Size: %ld",
		       g_cache_info[i].cur_cache_bytes_filled);
		printf("\nMax Cache Size: %ld",
		       g_cache_info[i].max_cache_size * 1024 * 1024 * 1024);
		printf("\ndirname: %s", g_cache_info[i].dirname);
	}
}

static int
s_rr_disk_caches(void)
{
	int i = 0, found = 0;

	if (glob_num_caches == 1) {
		if (g_cache_info[i].disk_space_exceeded != 1) {
			found = 1;
		}
	}
	else {
		for (i = 0; i < glob_num_caches; i++) {
			s_input_cur_write_dir_idx =
			    (s_input_cur_write_dir_idx + 1) % glob_num_caches;

			if (g_cache_info[i].disk_space_exceeded != 1) {
				found = 1;
				break;
			}
		}
	}

	if (found != 1) {
		s_disk_space_exceeded = 1;
		return -1;
	}
	else {
		s_disk_space_exceeded = 0;
		return 0;
	}
}

/**
	This function takes a data pointer and information about a file as
	input and populates persistent storage (disk) with that information.
	Also, it will update a map file that stores meta-data about that
	content.
*/
int
DM_put(struct MM_put_data *map,
       video_types_t	  video_type)
{
	GList		 *extent_list;
	struct LM_extent *ptr_ext = NULL;
	uint64_t	 tot_len = 0;
	int		 ret_val = -1;

	return -1;

	glob_dm_put_cnt++;
	if (s_disk_space_exceeded != 0) {
		if (dm_api_debug > 2)
			printf("\n NKN ERROR: Disk Space Exceeded. ");

                //DM_print_stats();

		ret_val = -NKN_DM_DISK_SPACE_EXCEEDED;
		goto error;
	}

	memcpy(map->clip_filename, NO_FILE_STR, strlen(NO_FILE_STR));

	extent_list = nkn_lm_get_extents_by_uri(map->uriname);

	if (s_find_file_in_cache_dir(map) == 0) {
		/* File exists in cache */
		if (extent_list != NULL) {
			ptr_ext = s_find_extent_by_offset(extent_list,
							  map->clip_offset,
							  map->content_length,
							  &tot_len);
			if (ptr_ext != NULL) {
				return 0;
			}
		}
	}
	else {
		/* New file: Round-robin the disk caches */
		ret_val = s_input_get_cache_space(map);
		if (ret_val < 0) {
		    goto error;
		}
	}

	ret_val = s_input_store_content_in_free_block(map, video_type);

	return ret_val;

 error:
	glob_dm_put_err++;
	return ret_val;
}

#if 0
static void
nkn_diskmgr_handle_read_errno()
{
	printf("\n%s(): DISK READ ERROR: "\
	       "Errno returned: %d", __FUNCTION__, errno);
}

static void
nkn_diskmgr_handle_lseek_errno(void)
{
	printf("\n%s(): DISK LSEEK ERROR: "\
	       "Errno returned: %d", __FUNCTION__, errno);
}
#endif

/*
	- This function searches an extent for an offset and length.
	- Assumes that the extent list is ordered.
*/
static struct LM_extent *
s_find_extent_by_offset(GList	 *in_ext_list,
			uint64_t uri_offset,
			uint64_t len,
			uint64_t *tot_len)
{
	struct LM_extent *ptr_ext = NULL;
	GList *ptr_list = NULL;
	GList *ptr_tgt = NULL;
	GList *ptr_prev = NULL;
	struct LM_extent *ptr_tgt_ext = NULL;

	ptr_list = in_ext_list;

	/* Extent list is ordered for efficiency */
	/* TBD: we can make this search more efficient in the future
	   Right now O(n) but the length of the list is so small that
	   it is insignificant  */
	ptr_prev = ptr_list;
	while(ptr_list != NULL) {
		ptr_ext = (struct LM_extent *) ptr_list->data;
		//*tot_len += ptr_ext->tot_content_len;
		*tot_len += ptr_ext->uol.length;

		if ((ptr_ext->uol.offset <= (int64_t)uri_offset) &&
		    (ptr_ext->uol.offset + ptr_ext->uol.length > (int64_t)uri_offset)) {
			ptr_tgt = ptr_list;
			break;
		}
		ptr_prev = ptr_list;
		ptr_list = ptr_list->next;
	}

	/* Go to the end of the list to find the total length */
	if(ptr_list)
		ptr_list = ptr_list->next;
	while(ptr_list != NULL) {
		ptr_ext = (struct LM_extent *) ptr_list->data;
		*tot_len += ptr_ext->uol.length;

		ptr_prev = ptr_list;
		ptr_list = ptr_list->next;
		continue;
	}

	if(ptr_tgt != NULL) {
		ptr_tgt_ext = (struct LM_extent *)ptr_tgt->data;
		return (ptr_tgt_ext);
	}
	else {
		return NULL;
	}
}

/*
	- This function returns a 'block' of memory that contains the offset, 
	  length of a 'content-name'.
	- It gets a list of extents from the location manager and 
	  searches this list for an offset.
	- Once it gets an extent, it will check how much of the data 
	  it can deliver will return only that amount of data. 
	- The caller will re-call this function for the rest of the data, 
	  which is on other block-ids. 
*/
int
DM_stat(nkn_uol_t	uol,
	MM_stat_resp_t	*in_ptr_resp)
{
	GList		*extent_list = NULL;
	LM_extent_t	*ptr_ext;
	uint64_t	tot_len = 0;
	int		str_size = 0;

	glob_dm_stat_cnt++;
	NKN_ASSERT(uol.uri);
	if (uol.uri == NULL)
		goto dm_stat_error;

	//nkn_lm_get_extents_by_uri(uol.uri, &extent_list);
	extent_list = nkn_lm_get_extents_by_uri(uol.uri);

	if(extent_list == NULL)
		goto dm_stat_error;

	ptr_ext = s_find_extent_by_offset(extent_list, uol.offset,
					  uol.length, &tot_len);
	if(ptr_ext == NULL)
		goto dm_stat_not_found;

	assert(ptr_ext->inUse == TRUE);
	in_ptr_resp->tot_content_len = tot_len;
	in_ptr_resp->media_blk_len = NKN_DM_BLK_SIZE_KB * 1024;

	str_size = strlen(ptr_ext->physid);
	//assert(str_size < NKN_MAX_FILE_NAME_SZ); can't occur
	memcpy(&in_ptr_resp->physid[0], ptr_ext->physid, str_size + 1);
	in_ptr_resp->physid[str_size] = '\0';
	in_ptr_resp->ptype = SASDiskMgr_provider;

	return 0;

dm_stat_not_found:
	glob_dm_stat_not_found++;
	in_ptr_resp->tot_content_len = 0;
	in_ptr_resp->media_blk_len = 0;
	return -1;

dm_stat_error:
	glob_dm_stat_err++;
	in_ptr_resp->tot_content_len = 0;
	in_ptr_resp->media_blk_len = 0;

	return  -1;
}

static int
s_get_content(const char     *in_filename,
	      const uint64_t in_offset,
	      struct iovec   *in_iov,
	      const int32_t  in_iovcnt)
{
	int fd, retf = -1;
	int out_errno = 0;

	fd = open(in_filename, O_RDONLY | O_DIRECT);
	if (fd < 0) {
		out_errno = errno;
		glob_dm_num_open_err ++;
		if (dm_api_debug > 2)
			printf("\n%s(): FILESYSTEM OPEN FAILED %s \n",
			       __FUNCTION__, in_filename);
		return out_errno;
	}
	glob_dm_num_files_open ++;

	retf = lseek(fd, in_offset, SEEK_SET);
	if (retf >= 0) {
		retf = readv(fd, in_iov, in_iovcnt);
		if (retf <= 0) {
		    out_errno = errno;
			glob_dm_num_read_err ++;
			if (dm_api_debug > 2)
				printf("\n%s(): FILESYSTEM READ ERROR: %s %d",
					__FUNCTION__, in_filename, retf);
		}
	}
	else {
		out_errno = errno;
		glob_dm_num_seek_err ++;
		if (dm_api_debug > 2)
			printf("\n%s(): FILESYSTEM SEEK ERROR: %d",
			       __FUNCTION__, retf);
	}

	close(fd);
	glob_dm_num_files_closed ++;
	return out_errno;
}

static int
s_dm_get_read_helper(MM_get_resp_t *in_ptr_resp,
		     char	   *in_filename,
		     uint64_t	   in_file_offset)
{
#define STACK_ALLOC (1024*1024/CM_MEM_PAGE_SIZE)
	struct iovec	iovp_mem[STACK_ALLOC];
	struct iovec	*iov = iovp_mem;
	void		*ptr_vobj;
	void		*content_ptr = NULL;
	int		iov_alloced = 0;
	uint64_t	i;
	int		in_num_obj = in_ptr_resp->in_num_content_objects;
	int		ret_val = -1;

	if(in_num_obj > STACK_ALLOC) {
		iov = nkn_calloc_type(sizeof(struct iovec), in_num_obj, mod_dm_iovec_t);
		if(iov == NULL) {
			goto dm_get_error;
		}
		iov_alloced = 1;
	}

	for(i = 0; i < in_ptr_resp->in_num_content_objects; i++) {
		ptr_vobj = in_ptr_resp->in_content_array[i];
		assert(ptr_vobj);

		content_ptr = nkn_buffer_getcontent(ptr_vobj);
		assert(content_ptr);

		iov[i].iov_base = content_ptr;
		iov[i].iov_len = CM_MEM_PAGE_SIZE;
	}

	ret_val = s_get_content(in_filename, in_file_offset, iov, in_num_obj);

	if(iov_alloced)
		free(iov);
	return ret_val;

dm_get_error:
	if(iov_alloced)
		free(iov);
	return ret_val;
}

/*
	- This function reads a block of data.
	- For each PAGE (PAGE_SIZE = 64 KB), it will call API functions
	  to fill the video_obj_t pointer passed in an array.
	- This way it reports all the contents that are obtained 
	  in a block of data. 
	- The pointer to video_obj_t is an array of pointers to 
	  video_obj_t.
	- The physid = inode#_filename_blockid
*/
int
DM_get(MM_get_resp_t *in_ptr_resp)
{
#define STACK_ALLOC (1024*1024/CM_MEM_PAGE_SIZE)
	int		uol_len[STACK_ALLOC];
	nkn_uol_t	uol;
	GList		*extent_list = NULL, *ptr_list = NULL;
	LM_extent_t	*ptr_ext;
	void		*ptr_vobj;
	uint64_t	read_offset;
	int		ret_val;
	int		num_pages;
	int		partial_pages;
	int		cont_idx;
	int		j, str_size;
	uint32_t	read_length;
	int		err_code = 0;

	// TBD: Search by UOL to get the PHYSID and then search by PHYSID
	uol.uri = NULL;
	glob_dm_get_cnt++;
	NKN_ASSERT(in_ptr_resp);
	NKN_ASSERT(in_ptr_resp->physid);
	//assert(in_ptr_resp->uol.length > 0);
	if (in_ptr_resp == NULL || in_ptr_resp->physid == NULL) {
		err_code = -EINVAL;
		goto dm_get_error;
	}

	nkn_lm_get_extents_by_physid(in_ptr_resp->physid, &extent_list);
	if (extent_list == NULL) {
		err_code = -1;
	        goto dm_get_error;
	}

	in_ptr_resp->out_num_content_objects = 0;

	ptr_list = g_list_first(extent_list);
	ptr_ext = (LM_extent_t *) ptr_list->data;
	assert(ptr_ext);

	/* Execute the readv system call */
	ret_val = s_dm_get_read_helper(in_ptr_resp, ptr_ext->filename, 
				       ptr_ext->file_offset);
	if (ret_val < 0) { 
		err_code = ret_val;
		goto dm_get_error;
	}

	/* Remove this when generic uri alloc mechanism is there */
	uol.uri = (char *) nkn_calloc_type(1,
				NKN_MAX_FILE_NAME_SZ * sizeof(char),
				mod_dm_uri_t);
	assert(uol.uri);

	cont_idx = 0;
	memset(uol_len, 0, sizeof(uol_len));
	for (; ptr_list != NULL; ptr_list = ptr_list->next) {
		ptr_ext = (LM_extent_t *) ptr_list->data;

		read_offset = ptr_ext->uol.offset;
		read_length = ptr_ext->uol.length;
		assert(read_length > 0);

		num_pages = read_length / CM_MEM_PAGE_SIZE;
		partial_pages = read_length % CM_MEM_PAGE_SIZE;
		if(partial_pages > 0)
			num_pages ++;

		assert(ptr_ext->uol.uri);
		assert(ptr_ext->uol.length > 0);

		str_size = strlen(ptr_ext->uol.uri);
		memcpy(uol.uri, ptr_ext->uol.uri, str_size);
		uol.uri[str_size] = '\0';

		if ((in_ptr_resp->out_num_content_objects == 
		     in_ptr_resp->in_num_content_objects)) {
			break;
		}

		for (j = 1; j <= num_pages; j++) {
			assert(read_length != 0);
			if(in_ptr_resp->out_num_content_objects ==
			   in_ptr_resp->in_num_content_objects) {
				break;
			}

			ptr_vobj = in_ptr_resp->in_content_array[cont_idx];
			assert(ptr_vobj);

			uol.offset = read_offset;
			if(read_length >= CM_MEM_PAGE_SIZE) {
				uol.length = CM_MEM_PAGE_SIZE;
				read_offset += CM_MEM_PAGE_SIZE;
				read_length -= CM_MEM_PAGE_SIZE;
			}
			else {
				uol.length = read_length;
				read_offset += uol.length;
				read_length = 0;
			}

			//uol_len[j-1] = uol.length;
			nkn_buffer_setid(ptr_vobj, &uol, SASDiskMgr_provider, 0);
			in_ptr_resp->out_num_content_objects++;
			cont_idx++;
		}

		if(in_ptr_resp->out_num_content_objects == 
			in_ptr_resp->in_num_content_objects) {
			break;
		}
	}

	if(uol.uri) {
		free(uol.uri);
	}
	return 0;

dm_get_error:
	if (uol.uri) {
		free(uol.uri);
	}
	glob_dm_get_err++;
	if (in_ptr_resp) {
		in_ptr_resp->out_num_content_objects = 0;
		in_ptr_resp->err_code = err_code;
	}
	return -1;
}

void
DM_cleanup(void)
{
        nkn_locmgr_uri_tbl_cleanup();
        nkn_locmgr_physid_tbl_cleanup();
        nkn_locmgr_file_tbl_cleanup();
}

int
DM_delete(MM_delete_resp_t *delete)
{
	return 0;
}

int
DM_shutdown(void)
{
	return 0;
}

int
DM_discover(struct mm_provider_priv *p)
{
	return 0;
}

int
DM_evict(void)
{
	return 0;
}

int
DM_provider_stat (MM_provider_stat_t *tmp)
{
	return -1;
}
