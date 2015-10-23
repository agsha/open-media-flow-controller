/*
 * (C) Copyright 2009 Ankeena Networks, Inc
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

#include "nkn_defs.h"
#include "nkn_util.h"
#include "nkn_assert.h"
#include "nkn_cod.h"
#include "nkn_sched_api.h"
#include "nkn_diskmgr_intf.h"

#include "nkn_diskmgr2_local.h"
#include "diskmgr2_locking.h"
#include "diskmgr2_util.h"
#include "nkn_diskmgr2_disk_api.h"
#include "nkn_locmgr2_uri.h"
#include "nkn_locmgr2_extent.h"
#include "nkn_locmgr2_container.h"
#include "nkn_locmgr2_physid.h"
#include "nkn_locmgr2_attr.h"
#include "diskmgr2_common.h"

/* counter for sub error code NKN_SERVER_BUSY  10003 */
uint64_t glob_dm2_attr_read_server_busy_err;


static int
dm2_is_attr_good(char *attr_buf,
		 char *end)
{
    DM2_disk_attr_desc_t *dattrd = (DM2_disk_attr_desc_t *) attr_buf;
    nkn_attr_t		 *attr = (nkn_attr_t *)(attr_buf + DEV_BSIZE);

    if (attr_buf + DM2_ATTR_TOT_DISK_SIZE > end)
	return -4;

    if ((dattrd->dat_magic == DM2_ATTR_MAGIC) &&
	(dattrd->dat_version == DM2_ATTR_VERSION)) {
	/* Header is good, lets check the attributes.  Because we no longer
	 * support V1 attributes, we don't need to worry about the old v1
	 * magic value */
	if ((attr->na_magic == NKN_ATTR_MAGIC) &&
	    (attr->na_version == NKN_ATTR_VERSION)) {
	    /* Attribute is also good */
	    return 0;
        } else {
	    dattrd = (DM2_disk_attr_desc_t *)(attr_buf + DEV_BSIZE);
	    if ((dattrd->dat_magic == DM2_ATTR_MAGIC) &&
		(dattrd->dat_version == DM2_ATTR_VERSION)) {
		/*! Two headers in succession */
		return -2;
	    } else {
		return -1; /* Corrupted File ?? */
	    }
	}
    } else if (dattrd->dat_magic == DM2_ATTR_MAGIC_GONE) {
	return -5;  /* Deleted Attribute */
    } else {
	attr = (nkn_attr_t *) attr_buf;
	if ((attr->na_magic == NKN_ATTR_MAGIC) &&
	    (attr->na_version == NKN_ATTR_VERSION)) {
	    /* This first pointer is the attribute,
	     * case to be skipped */
	    return -3;
	} else {
	    return -1; /* Corrupted File ?? */
	}
    }
}	/* dm2_is_attr_good */


int
dm2_read_single_attr(MM_get_resp_t		*get,
		     const nkn_provider_type_t	ptype,
		     DM2_uri_t			*uri_head,
		     GList                      *ext_list_head,
		     char			*ext_uri,
		     int			read_single)
{
    struct stat		 sb;
    nkn_uol_t		 uol;
    nkn_attr_t		 *attr;
    dm2_container_t	 *cont = uri_head->uri_container;
    dm2_cache_info_t	 *ci = cont->c_dev_ci;
    dm2_cache_type_t	 *ct = ci->ci_cttype;
    GList		 *ext_obj;
    nkn_buffer_t	 *abuf = NULL;
    DM2_extent_t	 *ext;
    DM2_disk_attr_desc_t *dad;
    char		 *seek_uri_basename = 0, *uri_basename = 0;
    char		 *attr_buf = NULL, *pbuf = NULL;
    char		 *attr_pathname, *bp, *pbp;
    char		 *ext_uri_base_addr = ext_uri + strlen(ext_uri);
    off_t		 first_off = 0, last_off = 0, seek_off = 0,
			 attr_off = 0, start = 0, end = 0, act_end = 0;
    size_t		 rdsz, read_len = 0;
    int			 afd = -1, ret = 0, attr_size = 0;
    int			 num_attr = 0, found = 0, err = 0;

    DM2_GET_ATTRPATH(cont, attr_pathname);
    if (uri_head->uri_at_off == DM2_ATTR_INIT_OFF) {
	/* Never initialized - not found */
	DBG_DM2S("[file=%s] No attributes for URI=%s",
		 attr_pathname, uri_head->uri_name);
	uri_head->uri_chksum_err = 1;
	ret = -ENOENT;
	goto free_buf;
    }

    ret = nkn_cod_test_and_set_version(get->in_uol.cod, uri_head->uri_version, 0);
    NKN_ASSERT(ret != NKN_COD_INVALID_COD);
    if (ret != 0) {
	DBG_DM2E("[cache_type=%d] COD mismatch: cod=%ld",
		 ptype, get->in_uol.cod);
	/* XXXmiken: Don't have counter here to increment */
	ret = -ret;
	goto free_buf;
    }
    seek_uri_basename =
	dm2_uri_basename((char *)nkn_cod_get_cnp(get->in_uol.cod));

    if ((ret = dm2_open_attrpath(attr_pathname)) < 0) {
	DBG_DM2S("Failed to open attribute file (%s): %d",
		 attr_pathname, errno);
	goto free_buf;
    }
    afd = ret;
    ret = 0;

    bp = nkn_buffer_getcontent(get->in_attr);
    /* Point to attribute offset, not header offset in file */
    seek_off = (uri_head->uri_at_off + 1) * DEV_BSIZE;

    if (read_single) {
	/* Just need to read only one attribute */
	start = seek_off - DEV_BSIZE;
	read_len = DM2_ATTR_TOT_DISK_SIZE;
	num_attr = 1;
    } else {
	/* first_off & last_off always points to the attribute
	 * offset in the file */
	first_off = last_off = DM2_ATTR_INIT_OFF;
	for (ext_obj = ext_list_head; ext_obj; ext_obj = ext_obj->next) {
	    ext = (DM2_extent_t *) ext_obj->data;
	    /* This object has no attribute */
	    if (ext->ext_uri_head->uri_at_off == DM2_ATTR_INIT_OFF)
		continue;
	    attr_off = (ext->ext_uri_head->uri_at_off + 1) * DEV_BSIZE;

	    /* first_off gets the offset of the lowest valid offset */
	    if (first_off == DM2_ATTR_INIT_OFF)
		first_off = attr_off;
	    else if (attr_off < first_off)
		first_off = attr_off;

	    /* last_off get the offset of the highest valid offset */
	    if (last_off == DM2_ATTR_INIT_OFF)
		last_off = attr_off;
	    else if (attr_off > last_off)
		last_off = attr_off;
	}
	start = first_off - DEV_BSIZE;	// point to header
	end = act_end = last_off + DM2_MAX_ATTR_DISK_SIZE;//point to next header
	read_len = end - start;
	num_attr = read_len / DM2_ATTR_TOT_DISK_SIZE;
    }
    NKN_ASSERT((read_len % DM2_ATTR_TOT_DISK_SIZE) == 0);

    /*
     * Try to read a max of 64 attributes alone
     * Make sure that the distance between the first attr and the requested
     * is not more than 16. The assumption is attributes after the requested
     * one will be more useful than one at the front
     */
    if (num_attr > 64) {
	if (first_off == seek_off) {
	    start = first_off - DEV_BSIZE;
	} else if (first_off < seek_off) {
	    if (seek_off - first_off > 16 * DM2_ATTR_TOT_DISK_SIZE)
		start = (seek_off - DEV_BSIZE) - (16 * DM2_ATTR_TOT_DISK_SIZE);
	} else {
	    /* Only a bug should lead us here */
	    DBG_DM2S("offset=%ld fsize=%ld file=%s num_attr=%d first_off=%ld "
		     "last_off=%ld", seek_off, sb.st_size, attr_pathname,
		     num_attr, first_off, last_off);
	    assert(0);
	}

	end = start + (64 * DM2_ATTR_TOT_DISK_SIZE);
	if (act_end < end)
	    end = act_end;
	read_len = end - start;
	num_attr = read_len / DM2_ATTR_TOT_DISK_SIZE;
    }

    /* Allocate temp buffer to read in a bunch of attributes at the same time*/
    if (dm2_posix_memalign((void **)&attr_buf, DEV_BSIZE, read_len,
			   mod_dm2_attr_read_buf_t)) {
	DBG_DM2S("posix_memalign failed: %lu", sizeof(dm2_container_t));
	ret = -ENOMEM;
	goto free_buf;
    }

    rdsz = pread(afd, attr_buf, read_len, start);
    if (rdsz != read_len) {
	ret = errno;
	DBG_DM2S("IO ERROR:[URI=%s] Read from attribute file (%s) is not "
                 "complete: %ld/%ld first_off=%ld last_off=%ld, "
                 "seek_off=%ld, errno=%d", uri_head->uri_name,
                 attr_pathname, rdsz, read_len, first_off, last_off,
                 seek_off, ret);
	NKN_ASSERT(rdsz != (size_t)-1 || ret != EBADF);
	glob_dm2_attr_read_err++;
	if ((ret = fstat(afd, &sb)) == -1) {
	    NKN_ASSERT(errno != EBADF);
	    DBG_DM2S("fstat failed file=%s URI=%s: %d", attr_pathname,
		     uri_head->uri_name, errno);
	} else {
	    DBG_DM2S("fsize=%ld", sb.st_size);
	}
	ret = -EIO;
	goto free_buf;
    }
    dm2_inc_attribute_read_bytes(ct, ci, rdsz);

    for (ext_obj = ext_list_head; ext_obj; ext_obj = ext_obj->next) {
	ext = (DM2_extent_t *) ext_obj->data;

	if ((uri_basename = dm2_uh_uri_basename(ext->ext_uri_head)) == NULL) {
	    //BUG ??
	    continue;
	}
	/* Let's find a match for this extent in the attribute buffer */
	for (pbuf = attr_buf; pbuf < (attr_buf + read_len);
	     pbuf += DM2_ATTR_TOT_DISK_SIZE) {
	    err = dm2_is_attr_good(pbuf, (attr_buf + read_len));
	    if (err != 0) {
		if (err == -1) {
		    DBG_DM2S("Attribute file %s possibly corrupt at sector"
			     " offset %ld", attr_pathname,
			     ((pbuf - attr_buf)+ start)/DEV_BSIZE);
		} else if (err == -2) {
		    DBG_DM2M2("Attribute file %s has headers in "
			      "succession", attr_pathname);
		} else if (err == -3) {
		    DBG_DM2M2("Attribute file %s - skipping 1 sector at %ld",
			      attr_pathname,
			      ((pbuf - attr_buf)+ start)/DEV_BSIZE);
		}
		continue;
	    }
	    dad = (DM2_disk_attr_desc_t *) pbuf;

	    if (!strncmp(uri_basename, dad->dat_basename, NAME_MAX)) {
	        if (!strncmp(seek_uri_basename, dad->dat_basename, NAME_MAX)) {
		    if (!found) {
			/* Take care of duplicate URIs */
			uol.cod = get->in_uol.cod;
			attr = (nkn_attr_t *) (pbuf + DEV_BSIZE);
			memcpy(bp, (pbuf + DEV_BSIZE), attr->blob_attrsize);
			/* Update the latest hotness to the attribute incase
			 * the disk hotness was not synced yet */
			attr->hotval = ext->ext_uri_head->uri_hotval;
			/* Set the bytes delivered from cache to 0 */
			attr->cached_bytes_delivered = 0;
			nkn_buffer_setid(get->in_attr, &uol, ptype, 0);
		        found = 1;
			uol.cod = NKN_COD_NULL;
			/* if we found the required attr, we can exit now */
			if (read_single)
			    goto free_buf;
		    }
		} else {
		    /* Allocate extra buffers to put the extra attributes */
		    strcpy(ext_uri_base_addr, dad->dat_basename);
		    uol.cod = nkn_cod_open(ext_uri, strlen(ext_uri),
					   NKN_COD_DISK_MGR);
		    if (uol.cod == NKN_COD_NULL) {
			DBG_DM2W("[cache_type=%d] COD allocation failed "
				 "(ext URI=%s) (req URI=%s)", ptype, ext_uri,
				 dad->dat_basename);
			glob_dm2_cod_null_cnt++;
			glob_dm2_attr_read_err++;
			continue;
		    }
		    ret = nkn_cod_test_and_set_version(uol.cod,
					ext->ext_uri_head->uri_version, 0);
		    NKN_ASSERT(ret != NKN_COD_INVALID_COD);
		    if (ret != 0) {
			/* XXXmiken: no ct ptr to increment warning */
			nkn_cod_close(uol.cod, NKN_COD_DISK_MGR);
			uol.cod = NKN_COD_NULL;
			continue;
		    }
		    attr = (nkn_attr_t *) (pbuf + DEV_BSIZE);
		    attr_size = attr->blob_attrsize;
		    abuf = nkn_buffer_alloc(NKN_BUFFER_ATTR, attr_size, 0);
		    if (abuf == NULL) {
			DBG_DM2S("[cache_type=%d] Attribute buffer allocation "
				 "failed (URI=%s): %d", ptype,
				 ext->ext_uri_head->uri_name, ENOMEM);
			glob_dm2_attr_read_err++;
			ret = -ENOMEM;
			goto free_buf;
		    }
		    pbp = nkn_buffer_getcontent(abuf);
		    memcpy(pbp, (pbuf + DEV_BSIZE), attr_size);
		    /* Update the latest hotness to the attribute */
		    attr->hotval = ext->ext_uri_head->uri_hotval;
		    /* Set the bytes delivered from cache to 0 */
		    attr->cached_bytes_delivered = 0;
		    nkn_buffer_setid(abuf, &uol, ptype, 0);
		    nkn_buffer_release(abuf);
		    abuf = NULL;
		    nkn_cod_close(uol.cod, NKN_COD_DISK_MGR);
		    uol.cod = NKN_COD_NULL;
		}
	    } else {
		/* No match. Skip this attribute */
	    }
	}
    }

 free_buf:
    if (uol.cod != NKN_COD_NULL)
	nkn_cod_close(uol.cod, NKN_COD_DISK_MGR);
    if (attr_buf != NULL)
	dm2_free(attr_buf, read_len, DEV_BSIZE);
    if (afd != -1) {
	glob_dm2_attr_close_cnt++;
	nkn_close_fd(afd, DM2_FD);
    }
    return ret;
}	/* dm2_read_single_attr */


static int
dm2_check_attr_head(DM2_disk_attr_desc_t *dattrd,
		    const char		 *attrpath,
		    const size_t	 secnum,
		    const char		 *pbuf,
		    const char		 *rbuf)
{
    int		skip = 0;
    nkn_attr_t	*dattr = (nkn_attr_t *)dattrd;

    if (dattrd->dat_magic == DM2_ATTR_MAGIC_GONE) {
	/*
	 * Do nothing; Can skip this sector.
	 * It's possible to store these sector offsets in a list and
	 * reuse the sectors.  However, this is more work than I want
	 * to do at this point.
	 */
	DBG_DM2M2("attr file (%s) name=%s offset=%ld secnum=%ld GONE magic",
		  attrpath, dattrd->dat_basename, pbuf-rbuf, secnum);
	skip = 8;
    } else if (dattrd->dat_magic != DM2_ATTR_MAGIC) {
	DBG_DM2S("Illegal magic number in attribute file (%s) @ sec=%ld: "
		 "name=%16s expected=0x%x|0x%x read=0x%x offset=%ld",
		 attrpath, secnum, dattrd->dat_basename, DM2_ATTR_MAGIC,
		 DM2_ATTR_MAGIC_GONE, dattrd->dat_magic, pbuf-rbuf);
	skip = 1;
	if (dattr->na_magic == NKN_ATTR_MAGIC) {
	    DBG_DM2S("Expected disk descriptor in attr file (%s) looks like "
		     "disk attr at offset=%ld secnum=%ld", attrpath, pbuf-rbuf,
		     secnum);
	}
	NKN_ASSERT(dm2_assert_for_disk_inconsistency);
    }
    if (skip == 0 && dattrd->dat_version != DM2_ATTR_VERSION) {
	DBG_DM2S("Illegal version in attribute file (%s) @ sec=%ld: "
		 "name=%s expected=%d read=%d offset=%ld",
		 attrpath, secnum, dattrd->dat_basename, DM2_ATTR_VERSION,
		 dattrd->dat_version, pbuf-rbuf);
	skip = 1;
	NKN_ASSERT(dm2_assert_for_disk_inconsistency);
    }
    return skip;
}	/* dm2_check_attr_head */


static int
dm2_check_attr(nkn_attr_t	*dattr,
	       const size_t	secnum,
	       dm2_cache_type_t *ct,
	       const char	*attrpath)
{
    uint64_t disk_checksum, calc_checksum;

    if (dattr->na_magic != NKN_ATTR_MAGIC) {
	DBG_DM2S("[cache_type=%s] Bad magic number (0x%x) at sector (%ld) "
		 "(file=%s)", ct->ct_name, dattr->na_magic, secnum, attrpath);
	return 1;
    }

    disk_checksum = dattr->na_checksum;
    dattr->na_checksum = 0;
    calc_checksum = do_csum64_iterate_aligned((uint8_t *)dattr,
					      dattr->total_attrsize, 0);
    if (disk_checksum != 0 && calc_checksum != disk_checksum) {
	DBG_DM2S("[cache_type=%s] Checksum mismatch: expected=0x%lx got=0x%lx "
		 "at sector=%ld file=%s",
		 ct->ct_name, disk_checksum, calc_checksum, secnum, attrpath);
	return 1;
    }
    return 0;
}	/* dm2_check_attr */


static int
dm2_truncate_attrfile(int afd,
		      dm2_cache_type_t *ct,
		      char *attrpath,
		      size_t secnum)
{
    int ret;

    ret = ftruncate(afd, secnum*DEV_BSIZE);
    if (ret != 0)
	DBG_DM2S("[cache_type=%s] Ftruncate to %ld failed for %s: %d",
		 ct->ct_name, secnum*DEV_BSIZE, attrpath, ret);
    return ret;
}	/* dm2_truncate_attrfile */


/* dm2_read_attr_file() is invoked after rlocking the corresponding 'ci'.
 * So we need not lock the ci again inside this function
 */
static int
dm2_read_attr_file(dm2_container_t	*cont,
		   dm2_container_head_t *cont_head,
		   int			*ch_lock,
		   uint32_t		preread,
		   dm2_cache_type_t	*ct)
{
#define RD_ATTR_BUFSZ	(2*1024*1024)	// must be multiple of 4KiB
    char		 ns_uuid[NKN_MAX_UID_LENGTH];
    DM2_disk_attr_desc_t *dattrd;
    char		 *uri, *attrpath;
    char		 *alloc_buf = NULL;
    nkn_uol_t		 uol;
    nkn_attr_t		 *dattr;
    nkn_buffer_t	 *abuf = NULL;
    DM2_uri_t		 *uh = NULL;
    char		 *slash, *bp, *pbuf, *rbuf = 0;
    off_t		 at_off, loff, secnum;
    size_t		 at_len, rdsz;
    nkn_cod_t		 attr_cod = NKN_COD_NULL;
    ns_stat_token_t	 ns_stat_token;
    ns_stat_category_t	 cp_stype;
    int			 ret = 0, skip, afd = -1;
    int			 uri_rlocked = 0;

    dm2_attr_mutex_lock(cont);
    if (NKN_CONT_ATTRS_READ(cont))
	goto free_buf;

    uri = alloca(strlen(cont->c_uri_dir)		// directory
		 + 1					// End of String
		 + 1					// slash
		 + NAME_MAX);				// 255
    strcpy(uri, cont->c_uri_dir);
    strcat(uri, "/");
    slash = &uri[strlen(uri)-1];

    if ((ret = dm2_uridir_to_nsuuid(cont->c_uri_dir, ns_uuid)) != 0)
	goto free_buf;
    ns_stat_token = ns_stat_get_stat_token(ns_uuid);
    if (ns_is_stat_token_null(ns_stat_token)) {
	DBG_DM2S("Can not find namespace: %s", ns_uuid);
	goto free_buf;
    }
    //cache_enabled = ns_stat_get_cache_enable(ns_uuid);	// boolean
    cp_stype = NS_CACHE_PIN_SPACE_D1 - 1 +
	atoi(&cont->c_dev_ci->mgmt_name[strlen(NKN_DM_DISK_CACHE_PREFIX)]);

    memset(&uol, 0, sizeof(uol));
    if (preread) {
	/* Allocate memory from stack, preread stacks are 2MB */
	alloc_buf = alloca(RD_ATTR_BUFSZ+DEV_BSIZE);
	rbuf = (char *)roundup((off_t)alloc_buf, DEV_BSIZE);
    } else {
	if (dm2_posix_memalign((void *)&rbuf, DEV_BSIZE, RD_ATTR_BUFSZ,
			       mod_dm2_attr_read_buf_t)) {
	    DBG_DM2S("posix_memalign failed: %d %d", RD_ATTR_BUFSZ, errno);
	    ret = -ENOMEM;
	    goto free_buf;
	}
    }
    DM2_GET_ATTRPATH(cont, attrpath);
    ct->ct_dm2_attrfile_open_cnt++;
    if ((ret = dm2_open_attrpath(attrpath)) < 0) {
	DBG_DM2S("[cache_type=%s] Failed to open attribute file (%s): %d",
		 ct->ct_name, attrpath, ret);
	ct->ct_dm2_attrfile_open_err++;
	goto free_buf;
    }
    afd = ret;

    secnum = 0;
    rdsz = pread(afd, rbuf, RD_ATTR_BUFSZ, secnum*DEV_BSIZE);
    while (rdsz != 0) {
	if (cont->c_dev_ci->ci_disabling)
	    break;
	glob_dm2_attr_read_cnt++;
	ct->ct_dm2_attrfile_read_cnt++;
	if (rdsz == (size_t)-1) {
	    ret = -errno;
	    DBG_DM2S("IO ERROR:[cache_type=%s] Read of attribute file (%s) "
		     "failed read(%zu) from (0x%lx) errno(%d)",
		     ct->ct_name, attrpath, RD_ATTR_BUFSZ,
		     secnum*DEV_BSIZE, -ret);
	    glob_dm2_attr_read_err++;
	    ct->ct_dm2_attrfile_read_err++;
	    NKN_ASSERT(ret != -EBADF);
	    goto free_buf;
	}
	if (rdsz % DEV_BSIZE != 0) {
	    DBG_DM2S("IO ERROR:[cache_type=%s] Read from attribute file "
		     "(%s) is not an integral sector amount: %ld failed"
		     " read(%zu) from (0x%lx) errno(%d)",ct->ct_name,
		     attrpath, rdsz, RD_ATTR_BUFSZ, secnum*DEV_BSIZE,
                     errno);
	    DBG_DM2S("Will attempt to cache as much as possible");
	}
	if (rdsz < DM2_ATTR_TOT_DISK_SIZE) {
	    DBG_DM2S("IO ERROR:[cache_type=%s] Short read from attribute "
		     "file (%s): %ld read(%zu) from (0x%lx) errno(%d)",
		     ct->ct_name, attrpath, rdsz, RD_ATTR_BUFSZ,
		     secnum*DEV_BSIZE, errno);
	    DBG_DM2S("  Truncating file to %ld", secnum*DEV_BSIZE);
	    /* We are finished reading this file */
	    glob_dm2_attr_read_err++;
	    ct->ct_dm2_attrfile_read_err++;
	    cont->c_rflags |= DM2_CONT_RFLAG_ATTRS_READ;
	    ret = dm2_truncate_attrfile(afd, ct, attrpath, secnum);
	    goto free_buf;
	}
	ct->ct_dm2_attrfile_read_bytes += rdsz;
	glob_dm2_attr_read_bytes += rdsz;
	/*
	 * If we don't have enough data for an entire attr, we don't go back
	 * and perform another read w/o incrementing any counters
	 */
	for (pbuf = rbuf, dattrd = (DM2_disk_attr_desc_t *)rbuf;
	     rdsz >= DM2_ATTR_TOT_DISK_SIZE;
		 dattrd = (DM2_disk_attr_desc_t *)pbuf) {
	    if (cont->c_dev_ci->ci_disabling)
		break;

	    skip = dm2_check_attr_head(dattrd, attrpath, secnum, pbuf, rbuf);
	    if (skip == 1) {
		/* Unknown magic or version => corruption? */
		glob_dm2_attr_read_err++;
		ct->ct_dm2_attrfile_read_err++;
		goto next_attr;
	    } else if (skip == 8) {	// GONE magic
		dm2_attr_slot_push_tail(cont, secnum*DEV_BSIZE);
		goto next_attr;
	    }
	    strcat(uri, dattrd->dat_basename);
	    uh = dm2_uri_head_get_by_ci_uri_rlocked(uri, cont_head, ch_lock,
						    cont, cont->c_dev_ci, ct);
	    if (uh == NULL) {
		/*
		 * While this doesn't mean corruption has occurred, we
		 * definitely have a bug or oversight in the code.  If this
		 * uri is not present, then there are no extents for this
		 * uri.  That's not good.
		 */
		if (!cont->c_dev_ci->ci_disabling) {
		    DBG_DM2S("[cache_type=%s] URI=%s not found for attr."
			     "Deleting attr at offset=%ld (%s)", ct->ct_name,
			     uri, pbuf-rbuf, attrpath);
		    loff = secnum * DEV_BSIZE;
		    ret = dm2_do_stampout_attr(afd, dattrd, cont, loff,
					       dattrd->dat_basename, 0,
					       DM2_ATTR_IS_OK);
		    NKN_ASSERT(ret == 0);
		    /* Should we re-use the slot now? */
		    glob_dm2_attr_read_err++;
		    ct->ct_dm2_attrfile_read_err++;
		}
		goto next_attr;
	    }
	    uri_rlocked = 1;
	    dm2_uri_log_state(uh, DM2_URI_OP_PRE, 0, 0, 0, nkn_cur_ts);
	    dattr = (nkn_attr_t *)(pbuf + DEV_BSIZE);
	    if (dm2_check_attr(dattr, secnum, ct, attrpath)) {
		ret = dm2_do_stampout_attr(afd, dattrd, cont, secnum*DEV_BSIZE,
					   dattrd->dat_basename, 0,
					   DM2_ATTR_IS_CORRUPT);
		NKN_ASSERT(ret == 0);
		/* Should we re-use the slot now? */
		glob_dm2_attr_read_err++;
		ct->ct_dm2_attrfile_read_err++;
		goto next_attr;
	    }
	    at_off = uh->uri_at_off;
	    at_len = uh->uri_at_len;
	    if ((at_off != 0 && at_off != DM2_ATTR_INIT_OFF) ||
		(at_len != 0 && at_len != DM2_ATTR_INIT_LEN)) {
		DBG_DM2S("[cache_type=%s] Warning: URI (%s) already has "
			 "attributes set: old_off=%ld old_len=%d "
			 "new_secoff=%ld file=%s", ct->ct_name,
			 uri, uh->uri_at_off,
			 uh->uri_at_len, secnum, attrpath);
		/* Throw away duplicate attributes */
		goto next_attr;
	    }
	    uh->uri_at_off = secnum;	// sector of header
	    uh->uri_at_len = dattr->total_attrsize;
	    uh->uri_blob_at_len = dattr->blob_attrsize;
	    uh->uri_expiry = dattr->cache_expiry;
	    uh->uri_invalid = (dattr->na_flags2 & NKN_OBJ_INVALID);
	    uh->uri_time0 = dattr->cache_time0;
	    uh->uri_cache_create = dattr->origin_cache_create +
				   dattr->cache_correctedage;
	    uh->uri_content_len = dattr->content_length;
	    uh->uri_hotval = dattr->hotval;
	    uh->uri_orig_hotness = am_decode_hotness(& uh->uri_hotval);
	    if (ct->ct_ptype == glob_dm2_lowest_tier_ptype)
		uh->uri_cache_pinned = nkn_attr_is_cache_pin(dattr);
	    if (nkn_attr_is_streaming(dattr)) {
		/*
		 * This would be the case when a streaming object was not
		 * fully ingested or the end put never came. The content
		 * length in the attribute is not usable; and hence, mark
		 * the URI for deletion.
		 */
		uh->uri_chksum_err = 1;
	    }
	    /*
	     * If the URI is partial, uri_resv_len would be useful
	     * for reserving blocks when PUTs continue to come. At
	     * this point uri_resv_len would be -ve and reflect the data
	     * already in the disk. Add the content length to get the amount
	     * of data yet to be written to disk.
	     */
	    uh->uri_resv_len += uh->uri_content_len;

	    memcpy(&uh->uri_version, &dattr->obj_version,
		   sizeof(uh->uri_version));
	    if (NKN_OBJV_EMPTY(&uh->uri_version))
		DBG_DM2S("NULL URI VERSION: %s", uri);

	    dm2_ns_calc_cache_pin_usage(ns_stat_token, cp_stype,
					ct->ct_ptype, dattr);
	    dm2_ns_update_total_objects(ns_stat_token,
					cont->c_dev_ci->mgmt_name,
					 ct, 1);
	    /* All the data in the disk are to be accounted as
	     * ingested bytes */
	    ct->ct_dm2_ingest_bytes += uh->uri_content_len;

	    if (uh->uri_chksum_err)
		goto next_attr;

	    attr_cod = nkn_cod_open(uri, strlen(uri),NKN_COD_DISK_MGR);
	    if (attr_cod == NKN_COD_NULL) {
		DBG_DM2W("[cache_type=%s] COD allocation failed (URI=%s)",
			 ct->ct_name, uri);
		glob_dm2_attr_read_cod_null_err++;
		ct->ct_dm2_attrfile_cod_null_err++;
		glob_dm2_attr_read_server_busy_err++;
		goto next_attr;
	    }
	    ret = nkn_cod_test_and_set_version(attr_cod, uh->uri_version, 0);
	    NKN_ASSERT(ret != NKN_COD_INVALID_COD);
	    if (ret != 0) {
		ct->ct_dm2_attrfile_read_cod_mismatch_warn++;
		goto next_attr;
	    }
	    abuf = nkn_buffer_alloc(NKN_BUFFER_ATTR, dattr->blob_attrsize,
				    (preread)? BM_PREREAD_ALLOC : 0);
	    if (abuf == NULL) {
		DBG_DM2M("[cache_type=%s] Attribute buffer allocation "
			 "failed (URI=%s): %d", ct->ct_name, uri, ENOMEM);
		glob_dm2_attr_read_alloc_failed_err++;
		ct->ct_dm2_attrfile_alloc_fail_err++;
		goto next_attr;
	    }
	    bp = nkn_buffer_getcontent(abuf);
	    /* Bytes delivered from cache needs to be set to 0 */
	    dattr->cached_bytes_delivered = 0;
	    memcpy(bp, dattr, dattr->blob_attrsize);
	    uol.cod = attr_cod;
	    glob_dm2_attr_read_setid_cnt++;
	    nkn_buffer_setid(abuf, &uol, ct->ct_ptype, 0);
	    nkn_buffer_release(abuf);
	    abuf = NULL;

	next_attr:
	    *(slash+1) = '\0';
	    if (uri_rlocked) {
		dm2_evict_add_uri(uh, am_decode_hotness(&uh->uri_hotval));
		dm2_uri_head_rwlock_runlock(uh, ct, DM2_NOLOCK_CI);
		uri_rlocked = 0;
	    }
	    if (attr_cod != NKN_COD_NULL) {
		nkn_cod_close(attr_cod, NKN_COD_DISK_MGR);
		attr_cod = NKN_COD_NULL;
	    }
	    secnum += DM2_ATTR_TOT_DISK_SECS;
	    rdsz -= DM2_ATTR_TOT_DISK_SECS * DEV_BSIZE;
	    pbuf += DM2_ATTR_TOT_DISK_SECS * DEV_BSIZE;
	}
    }
    cont->c_rflags |= DM2_CONT_RFLAG_ATTRS_READ;
    ret = 0;

 free_buf:
    if (preread)
	rbuf = NULL;
    if (attr_cod)
	nkn_cod_close(attr_cod, NKN_COD_DISK_MGR);
    if (uri_rlocked && uh)
	dm2_uri_head_rwlock_runlock(uh, ct, DM2_NOLOCK_CI);
    if (rbuf)
	dm2_free(rbuf, RD_ATTR_BUFSZ, DEV_BSIZE);
    if (afd != -1) {
	glob_dm2_attr_close_cnt++;
	nkn_close_fd(afd, DM2_FD);
    }
    dm2_attr_mutex_unlock(cont);
    return ret;
}	/* dm2_read_attr_file */


/*
 * cont_head is read locked.
 */
int
dm2_read_container_attrs(dm2_container_head_t	*cont_head,
			 int		        *ch_lock,
			 uint32_t		preread,
			 dm2_cache_type_t	*ct)
{
    dm2_container_t *cont;
    GList	    *cptr;
    int		    ret;

    for (cptr = cont_head->ch_cont_list; cptr; cptr = cptr->next) {
	cont = (dm2_container_t *)cptr->data;
	if (cont->c_dev_ci->ci_disabling)
	    continue;
	dm2_ci_dev_rwlock_rlock(cont->c_dev_ci);
	ret = dm2_read_attr_file(cont, cont_head, ch_lock, preread, ct);
	if (ret < 0) {
	    if (ret == -ENOMEM) {
		DBG_DM2S("Ran out of memory: [cache:%s] %s",
			 cont->c_dev_ci->mgmt_name, cont->c_uri_dir);
		dm2_ci_dev_rwlock_runlock(cont->c_dev_ci);
		return ret;
	    }
	}
	dm2_ci_dev_rwlock_runlock(cont->c_dev_ci);
    }
    return 0;
}	/* dm2_read_container_attrs */


