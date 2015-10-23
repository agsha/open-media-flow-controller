/*
 *      COPYRIGHT: NOKEENA NETWORKS
 *
 *	Author: Michael Nishimoto
 */

#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/mount.h>
#include <strings.h>
#include <dirent.h>

#include "nkn_diskmgr2_api.h"
#include "nkn_diskmgr2_local.h"
#include "nkn_assert.h"
#include "diskmgr2_common.h"
#include "nkn_nknexecd.h"

extern int dm2_ram_drive_only;

int64_t glob_dm2_convert_success,
    glob_dm2_convert_failed,
    glob_dm2_disk_already_converted;

uint64_t glob_dm2_high_dict_mem_bytes;
/*
 * /			  => /
 * /namespace/		  => /
 * /namespace/dir1/file1/ => "file1/"
 * /namespace/dir1/file2  => file2
 *
 */
char *
dm2_uri_basename(char *uri_name)
{
    int  len = strlen(uri_name);
    char *p_basename = NULL;
    int idx = len;

    if (uri_name[0] != '/')
	return NULL;

    if (uri_name[len-1] == '/')
	idx--;

    if (idx == 0)
	/* Only single slash */
	p_basename = &uri_name[len-1];
    else {
	while (idx > 0) {
	    if (uri_name[idx-1] == '/') {
		p_basename = &uri_name[idx-1];
		if (idx == 1)
		    p_basename = &uri_name[len-1];
		else
		    p_basename++;
		break;
	    } else {
		idx--;
	    }
	}
    }

    return p_basename;
}	/* dm2_uri_basename */


/*
 * /namespace/		  => /namespace
 * /namespace/dir1/file1/ => /namespace/dir1
 * /namespace/dir1/file2  => /namespace/dir1
 *
 * Assume that uri_name can be modified
 */
char *
dm2_uri_dirname(char *uri_name,
		int  include_slash)
{
    int len = strlen(uri_name);
    char *slash;

    if (uri_name[0] != '/')
	return NULL;

    if (uri_name[len-1] == '/')
	uri_name[len-1] = '\0';

    slash = strrchr(uri_name, '/');
    NKN_ASSERT(slash != NULL);
    if (slash == NULL)
	return NULL;

    if (slash == uri_name) {
	if (include_slash)
	    uri_name[len-1] = '/';
    } else {
	if (include_slash)
	    *(slash+1) = '\0';
	else
	    *slash = '\0';
    }
    return uri_name;
}	/* dm2_uri_dirname */

char *
dm2_uh_uri_dirname(const DM2_uri_t *uh,
		   char		   *uri_dir,
		   int		   include_slash)
{
    strcpy(uri_dir, uh->uri_container->c_uri_dir);
    if (include_slash)
	strcat(uri_dir, "/");
    return uri_dir;
}
/*
 * 012345/	      => /
 * 012345/dir1/file1/ => file1/
 * 012345/dir1/file2  => file2
 *
 */

char *
dm2_uh_uri_basename(const DM2_uri_t *uh)
{
    char* p_basename = NULL;
    char* uri_name;
    int idx, len, last_slash = 0;

    if (!uh || !uh->uri_name)
	return NULL;

    uri_name = uh->uri_name;
    idx = len = strlen(uri_name);

    if (len == 0)
	assert(0);

    if (uri_name[0] == '/')
	assert(0);

    if (uri_name[len-1] == '/') {
        idx--;
        last_slash = idx;
    }

    if (idx == 0)
        /* Only single slash */
        p_basename = &uri_name[len-1];
    else {
        while (idx > 0) {
            if (uri_name[idx-1] == '/') {
                p_basename = &uri_name[idx-1];
                if (idx == 1)
                    p_basename = &uri_name[len-1];
                else
                    p_basename++;
                break;
            } else {
                idx--;
            }
        }
    }

    if (p_basename == NULL)
        p_basename = uri_name + last_slash;

    return p_basename;

}	/* dm2_uh_uri_basename */


int
dm2_posix_memalign(void **memptr,
		   size_t alignment,
		   size_t size,
		   nkn_obj_type_t type)
{
    int ret, alloc_size;
    uint64_t total_bytes;

    ret = nkn_posix_memalign_type(memptr, alignment, size, type);
    if (ret == 0) {
	alloc_size = ALLOC_SIZE(size, alignment);
	total_bytes = AO_fetch_and_add(&glob_dm2_alloc_total_bytes, alloc_size);
	if (dm2_print_mem)
	    DBG_DM2S("posix_memalign %d bytes, Total - %ld", alloc_size,
		      glob_dm2_alloc_total_bytes);

	/* Remember the highest dictionary usage */
	total_bytes += alloc_size;
	if (glob_dm2_high_dict_mem_bytes < total_bytes)
	    glob_dm2_high_dict_mem_bytes = total_bytes;
    }

    return ret;
}	/* dm2_posix_memalign */


void *
dm2_calloc(size_t nmemb,
	   size_t size,
	   nkn_obj_type_t type)
{
    void *ptr;
    int alloc_size;
    uint64_t total_bytes;

    ptr = nkn_calloc_type(nmemb, size, type);
    if (ptr != NULL) {
	alloc_size = NKN_SIZE(nmemb*size);
	total_bytes = AO_fetch_and_add(&glob_dm2_alloc_total_bytes, alloc_size);
	if (dm2_print_mem)
	    DBG_DM2S("dm2_calloc %d bytes, Total - %ld", alloc_size,
		     glob_dm2_alloc_total_bytes);
	/* Remember the highest dictionary usage */
	total_bytes += alloc_size;
	if (glob_dm2_high_dict_mem_bytes < total_bytes)
	    glob_dm2_high_dict_mem_bytes = total_bytes;
    }
    return ptr;
}	/* dm2_calloc */


void
dm2_free(void *ptr,
	 size_t size,
	 size_t align)
{
    int alloc_size;

    if (ptr != NULL) {
	if (align)
	    alloc_size = ALLOC_SIZE(size, align);
	else
	    alloc_size = NKN_SIZE(size);
	AO_fetch_and_add(&glob_dm2_alloc_total_bytes, -alloc_size);
	if (dm2_print_mem)
	    DBG_DM2S("Free %d bytes, Total - %ld", alloc_size,
		      glob_dm2_alloc_total_bytes);
    }
    free(ptr);
}	/* dm2_free */


int
dm2_convert_disk(const char *mountpt)
{
    char cmd[1024];
    struct stat sb;
    dm2_bitmap_header_t *bmh = NULL;
    char fname[PATH_MAX];
    int ret = 0, fd = -1, nbytes;

    if (stat(mountpt, &sb) < 0) {
	ret = -errno;
	DBG_DM2S("Unable to convert disk at %s", mountpt);
	glob_dm2_convert_failed++;
	return ret;
    }

    /*
     * Check the version in the bitmap file to see that the attributes are
     * converted yet to V2.
     */
    ret = dm2_posix_memalign((void **)&bmh, NBPG, sizeof(dm2_bitmap_header_t),
			     mod_dm2_cache_buf_t);
    if (ret != 0) {
	DBG_DM2S("ERROR: memory allocation failed: %d", ret);
	return -ret;
    }
    memset(bmh, 0, sizeof(dm2_bitmap_header_t));
    ret = snprintf(fname, PATH_MAX, "%s/%s", mountpt,
		   DM2_BITMAP_FNAME);

    glob_dm2_bitmap_open_cnt++;
    if ((fd = dm2_open(fname, O_RDONLY, 0644)) == -1) {
	ret = -errno;
	NKN_ASSERT(ret != -EMFILE);
	DBG_DM2S("ERROR: Open %s failed: %d", fname, ret);
	glob_dm2_bitmap_open_err++;
	return ret;
    }
    nkn_mark_fd(fd, DM2_FD);
    if ((nbytes = read(fd, bmh, sizeof(dm2_bitmap_header_t))) == -1) {
	ret = -errno;
	DBG_DM2S("ERROR: read error (%s)[%d,0x%lx]: %d",
		 fname, fd, (uint64_t)bmh, -ret);
	NKN_ASSERT(ret != -EBADF);
	ret = -EIO;
	glob_dm2_bitmap_read_err++;
	goto dm2_convert_disk_exit;
    }
    if (nbytes != sizeof(dm2_bitmap_header_t)) {
	DBG_DM2S("ERROR: Short read for %s: expected %lu bytes,read %d bytes",
		 fname, sizeof(dm2_bitmap_header_t), nbytes);
	ret = -EIO;
	glob_dm2_bitmap_read_err++;
	goto dm2_convert_disk_exit;
    }

    if (bmh->u.bmh_magic != DM2_BITMAP_MAGIC) {
	DBG_DM2S("ERROR: [dev=%s] Wrong magic number: expected 0x%x, got 0x%x",
		 mountpt, DM2_BITMAP_MAGIC, bmh->u.bmh_magic);
	ret = -EIO;
	glob_dm2_bitmap_read_err++;
	goto dm2_convert_disk_exit;
    }
    if (bmh->u.bmh_version == DM2_BITMAP_VERSION) {
        DBG_DM2M("INFO: [dev=%s] Disk already converted to "
		 "V2 attributes", mountpt);
	glob_dm2_disk_already_converted++;
	/* Is normal exit */
	goto dm2_convert_disk_exit;
    }

    if (stat(DM2_CONVERSION_SCRIPT, &sb) < 0) {
	DBG_DM2S("Can not find cache conversion script: %s",
		 DM2_CONVERSION_SCRIPT);
	glob_dm2_convert_failed++;
	ret = -ENOENT;
	goto dm2_convert_disk_exit;
    }
    snprintf(cmd, 1020, "%s %s", DM2_CONVERSION_SCRIPT, mountpt);
    if ((ret = system(cmd)) < 0) {
	ret = -errno;
	DBG_DM2S("[mountpt=%s] Conversion script failed: errno=%d",
		 mountpt, errno);
	glob_dm2_convert_failed++;
	goto dm2_convert_disk_exit;
    }
    if (ret != 0) {
	DBG_DM2S("[mountpt=%s] Conversion script failed: %d", mountpt, ret);
	glob_dm2_convert_failed++;
	ret = -EINVAL;
	goto dm2_convert_disk_exit;
    }
    glob_dm2_convert_success++;

dm2_convert_disk_exit:
    if (bmh)
	dm2_free(bmh, sizeof(dm2_bitmap_header_t), NBPG);
    if (fd != -1) {
	nkn_close_fd(fd, DM2_FD);
	glob_dm2_bitmap_close_cnt++;
    }
    return ret;
}	/* dm2_convert_disk */


int
dm2_stat_get_tierstat_cats(nkn_provider_type_t ptype,
			   ns_stat_category_t *ti_ptype,
			   ns_stat_category_t *td_ptype)
{
    switch (ptype) {
        case SolidStateMgr_provider:
	    if (ti_ptype)
		*ti_ptype = NS_INGEST_OBJECTS_SSD;
	    if (td_ptype)
		*td_ptype = NS_DELETE_OBJECTS_SSD;
	    break;
        case SAS2DiskMgr_provider:
	    if (ti_ptype)
		*ti_ptype = NS_INGEST_OBJECTS_SAS;
	    if (td_ptype)
		*td_ptype = NS_DELETE_OBJECTS_SAS;
	    break;
        case SATADiskMgr_provider:
	    if (ti_ptype)
		*ti_ptype = NS_INGEST_OBJECTS_SATA;
	    if (td_ptype)
		*td_ptype = NS_DELETE_OBJECTS_SATA;
	    break;
        default:
	    return -EINVAL;
    }
    return 0;
}	/* dm2_stat_get_tierstat_cats */

int
dm2_stat_get_diskstat_cats(const char *mgmt_name,
			   ns_stat_category_t *cp_stype,
			   ns_stat_category_t *us_stype,
			   ns_stat_category_t *to_stype)
{
    int strindex, disk_num;

    strindex = strlen(NKN_DM_DISK_CACHE_PREFIX);
    disk_num = atoi(&mgmt_name[strindex]);
    if (disk_num == 0) {
	/*
	 * Cannot check for values > max # of drives because we can currently
	 * go higher than this from drive swaps.  If by chance we end up with
	 * dc_0, we won't collect stats for it.  dc_0 can now be found when
	 * a caching device is put on the drive in slot 0.  Normally, this is
	 * not possible, but this happened on a VXA2100 system before we
	 * could properly understand which drives are in which slots.
	 */
	DBG_DM2S("Can not find disk number: %s", mgmt_name);
	return -EINVAL;
    }

    if (cp_stype)
	*cp_stype = NS_CACHE_PIN_SPACE_D1 + disk_num - 1;
    if (us_stype)
	*us_stype = NS_USED_SPACE_D1 + disk_num - 1;
    if (to_stype)
	*to_stype = NS_TOTAL_OBJECTS_D1 + disk_num - 1;
    return 0;
}	/* dm2_stat_get_diskstat_cats */


int
dm2_uridir_to_nsuuid(const char *uri_dir,
		     char *ns_uuid)
{
    const char *ns, *uid;
    int ret, ns_len, uid_len;

    ret = decompose_ns_uri_dir(uri_dir, &ns, &ns_len,
			       &uid, &uid_len,
			       NULL /* host */, NULL /* host_len */,
			       NULL /* port */, NULL /* port_len */);
    if (ret != 0) {
	DBG_DM2S("[URI_DIR=%s] Decompose of cache name directory failed: %d",
		 uri_dir, ret);
	return -EINVAL;
    }
    if (ns_len + 1 + uid_len + 1 > (int)NKN_MAX_UID_LENGTH) {
	assert(0);
	return -EINVAL;
    }

    strncpy(ns_uuid, ns, ns_len+1+uid_len);
    ns_uuid[ns_len+1+uid_len] = '\0';
    return 0;
}	/* dm2_uridir_to_nsuuid */


int
dm2_mount_fs(char *fsdev,
	     char *mountpt)
{
    char		cmd[NKNEXECD_MAXCMD_LEN];
    char		basename_buf[NKNEXECD_MAXBASENAME_LEN];
    nknexecd_retval_t	retval;

    DBG_DM2M("Mounting %s on %s", fsdev, mountpt);
    snprintf(cmd, NKNEXECD_MAXCMD_LEN, "/opt/nkn/bin/mount_cache.sh %s %s",
	     fsdev, mountpt);
    memset(&retval, 0, sizeof(retval));
    strcpy(basename_buf, "mount_cache");
    nknexecd_run(cmd, basename_buf, &retval);
    if (retval.retval_reply_code != 0) {
	DBG_DM2S("Cache mount failed for (%s)/(%s): %d",
		 fsdev, mountpt, errno);
	/* No of lines to move from stderr to syslog file */
	nkn_print_file_to_syslog(retval.retval_stderrfile, 10);
	return -EINVAL;
    }
    return 0;
}	/* dm2_mount_fs */

int
dm2_open(const char *filename, int flags, mode_t mode)
{
    int ret;

    if (dm2_ram_drive_only == 0)
	flags |= O_DIRECT;

    ret = open(filename, flags, mode);
    return ret;
}	/* dm2_open */

int
dm2_umount_fs(const char *mountpt)
{
    char		cmd[NKNEXECD_MAXCMD_LEN];
    char		basename_buf[NKNEXECD_MAXBASENAME_LEN];
    nknexecd_retval_t	retval;

    DBG_DM2M("Unmounting %s", mountpt);
    snprintf(cmd, NKNEXECD_MAXCMD_LEN, "/opt/nkn/bin/umount_cache.sh %s",
	     mountpt);
    memset(&retval, 0, sizeof(retval));
    strcpy(basename_buf, "umount_cache");
    nknexecd_run(cmd, basename_buf, &retval);
    if (retval.retval_reply_code != 0) {
	DBG_DM2S("Cache unmount failed for (%s): %d",
		 mountpt, errno);
	/* No of lines to move from stderr to syslog file */
	nkn_print_file_to_syslog(retval.retval_stderrfile, 10);
	return -EINVAL;
    }
    return 0;
}	/* dm2_umount_fs */
