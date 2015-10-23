/*
 *	COPYRIGHT: NOKEENA NETWORKS
 *
 * This file contains code which implements the Disk Manager
 *
 * Author: Michael Nishimoto
 *
 * Non-obvious coding conventions:
 *	- All functions should begin with dm2_
 *	- All functions have a name comment at the end of the function.
 *	- All log messages use special logging macros
 *
 */
#ifndef DISKMGR2_COMMON_H
#define DISKMGR2_COMMON_H

#define DM2_NO_ALIGN 0

char *dm2_uri_basename(char *uri_name);
char *dm2_uh_uri_basename(const DM2_uri_t *uri_head);
char *dm2_uri_dirname(char *uri_name, int include_slash);
char *dm2_uh_uri_dirname(const DM2_uri_t *uri_head,
			 char *uri_name, int include_slash);

void *dm2_calloc(size_t nmemb, size_t size, nkn_obj_type_t type);
void dm2_free(void *ptr, size_t size, size_t align);
int dm2_posix_memalign(void **memptr, size_t alignment, size_t size,
		       nkn_obj_type_t type);
int dm2_convert_disk(const char *mountpt);
int dm2_stat_get_tierstat_cats(nkn_provider_type_t ptype,
			       ns_stat_category_t *ti_ptype,
			       ns_stat_category_t *td_ptype);
int dm2_stat_get_diskstat_cats(const char *full_uri_dir,
			       ns_stat_category_t *cp_type,
			       ns_stat_category_t *us_type,
			       ns_stat_category_t *to_stype);
int dm2_uridir_to_nsuuid(const char *uri_dir, char *ns_uuid);
int dm2_mount_fs(char *fsdev, char *mountpt);
int dm2_open(const char *pathname, int flags, mode_t mode);
int dm2_umount_fs(const char *mountpt);
#endif /* DISKMGR2_COMMON_H */
