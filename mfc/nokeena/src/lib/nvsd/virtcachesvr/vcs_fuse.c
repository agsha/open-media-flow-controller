/*============================================================================
 *
 *
 * Purpose: This file implements vcs_fuse functions.
 *
 * Copyright (c) 2002-2012 Juniper Networks. All rights reserved.
 *
 *
 *============================================================================*/
#define FUSE_USE_VERSION 26
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 500
#endif // _XOPEN_SOURCE

#include "fuse.h"
#include "fuse_opt.h"
#include "fuse_lowlevel.h"
#include "fuse_common_compat.h"
#include <errno.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/prctl.h>
#include <sys/mount.h>

#include "vcs_fuse.h"
#include "vcs_private.h"

#define VCS_FUSE_MAX_RETRIES 3

static int vcs_fuse_getattr(
    const char *path,
    struct stat *stbuf)
{
    memset(stbuf, 0, sizeof(struct stat));
    if (strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
	return 0;
    }
    if (vcs_channel_find_by_name(path+1) == NULL) {
	return -ENOENT;
    }

    stbuf->st_mode = S_IFREG | 0444;
    stbuf->st_nlink = 1;
    stbuf->st_size = VCS_CHANNEL_SIZE;
    return 0;
}

static int vcs_fuse_readdir(
    const char *path,
    void *buf,
    fuse_fill_dir_t filler,
    off_t offset,
    struct fuse_file_info *fi)
{
    int i;
    UNUSED_ARGUMENT(offset);
    UNUSED_ARGUMENT(fi);

    if (strcmp(path, "/") != 0)
        return -ENOENT;

    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);

    for (i = 0; i < VCS_N_CHANNELS; i++) {
	filler(buf, vc->vcs_vc_channels[i].name, NULL, 0);
    }

    return 0;
}

static int vcs_fuse_open(
    const char *path,
    struct fuse_file_info *fi)
{
    nkn_vc_channel_t *ch = vcs_channel_find_by_name(path+1);

    if (ch == NULL) {
	DBG_VCE("(OPEN) no such channel: %s\n", path);
	return -ENOENT;
    }

    if ((fi->flags & 3) != O_RDONLY) {
	DBG_VCE("(OPEN) invalid flags: 0x%x\n", fi->flags);
        return -EACCES;
    }

    DBG_VCM("OPEN %s\n", path);
    fi->fh = (uint64_t)ch;
    AO_fetch_and_add1(&ch->total_open_virt_files_cnt);
    AO_fetch_and_add1(&ch->current_open_virt_files_cnt);

    return 0;
}

static int vcs_fuse_release(
    const char *path,
    struct fuse_file_info *fi)
{
    nkn_vc_channel_t *ch = (nkn_vc_channel_t *)fi->fh;
    UNUSED_ARGUMENT(path);
    if (ch)
        AO_fetch_and_sub1(&ch->current_open_virt_files_cnt);
    fi->fh = 0;
    return 0;
}

static int vcs_fuse_os_read(
    nkn_vc_channel_t *ch,
    const char *path,
    char *buf,
    size_t size,
    off_t offset)
{
    int rc = size;
    vccmp_virtual_drive_boot_sector_t bs;  /* cause it is :) */

    if (offset < 512) {
	int len = (512-offset);
	char *p = (char *)&bs + offset;
	vccmp_setup_virtual_drive_boot_sector(&bs, ch->id, ch->vm_name);
	memcpy(buf, p, len);
	size -= len;
	buf += len;
    }
    memset(buf, 0, size);

    DBG_VCM("READ[OS] %s\n"
	     "  size:         %u\n"
	     "  offset:       %"PRIu64"\n",
	     path, 
	     rc,
	     offset);

    return rc;
}

static int vcs_fuse_slot_read(
    nkn_vc_channel_t *ch,
    vccmp_slot_read_address_t addr,
    const char *path,
    char *buf,
    size_t size,
    off_t offset)
{
    uint32_t slot_index = (uint32_t)(addr.fields.index);
    off_t read_offset = (off_t)(addr.fields.offset);
    uint8_t nonce = (uint8_t)(addr.fields.nonce);
    nkn_vc_slot_t *slot = NULL;
    size_t length, extra = 0;
    int rc = 0;

    if (slot_index >= VCCMP_SLOTS_PER_CHANNEL) {
	DBG_VCE("(READ) invalid slot index: %u\n", slot_index);
	/* FUSE likely prevents this from happening via filesize. */
	return -EINVAL;
    }

    slot = vcs_slot_get(ch, slot_index, nonce);

    if (slot != NULL) {
	length = size;
	if (read_offset + length > slot->file_length) {
	    extra = (read_offset + length) - slot->file_length;
	    if (extra <= length) {
		length -= extra;
	    } else {
		length = 0;
		extra = size;
	    }
	}
    } else {
	length = 0;
	extra = size;
    }

    if (length > 0) {
	uint8_t direct = 0;
	size_t srs = (size_t)slot->streaming_read_size;
	if (srs && (length != srs) && (read_offset + length < slot->file_length))
	    direct = 1;
	rc = vfs_funs.pnkn_vcread(slot->vfp, buf, length, read_offset, direct,
				  slot->streaming_read_size);
	if (rc < 0) {
	    DBG_VCE("(READ) pnkn_vcread(%s) failed: %s\n", slot->uri, strerror(errno));
	    vcs_slot_put(ch, slot, slot_index);
	    AO_fetch_and_add1(&vc->total_read_errors_cnt);
	    AO_fetch_and_add1(&ch->total_read_errors_cnt);
	    return -errno;
	}
	AO_fetch_and_add(&vc->total_bytes_read_cnt, length);
	AO_fetch_and_add(&ch->total_bytes_read_cnt, length);
	buf += length;
    }

    if (extra > 0) {
	memset(buf, 0, extra);
    }

    DBG_VCM("READ[SLOT] %s\n"
	     "  %s\n"
	     "  slot index:   %u\n"
	     "  close flag:   %ld\n"
	     "  size:         %u\n"
	     "  offset:       %"PRIu64"\n"
	     "  read offset:  %"PRIu64"\n"
	     "  extra:        %u\n",
	     path, 
	     (slot != NULL) ? slot->uri : "<empty slot>",
	     slot_index,
	     (slot != NULL) ? AO_load(&slot->close_flag) : 0,
	     (uint32_t)size, 
	     (uint64_t)offset, 
	     (uint64_t)read_offset, 
	     (uint32_t)extra);

    vcs_slot_put(ch, slot, slot_index);

    return size;
}

static int vcs_fuse_read(
    const char *path,
    char *buf,
    size_t size,
    off_t offset,
    struct fuse_file_info *fi)
{
    vccmp_slot_read_address_t addr = { .value = (uint64_t)offset };
    nkn_vc_channel_t *ch;
    if (fi->fh)
	ch = (nkn_vc_channel_t *)fi->fh;
    else {
	DBG_VCM("fi->fh is not set!");
	ch = vcs_channel_find_by_name(path+1);
    }

    if (ch == NULL) {
	DBG_VCE("(READ) invalid channel: %s\n", path);
	return -ENOENT;
    }

    if (addr.fields.slot_read) {
	return vcs_fuse_slot_read(ch, addr, path, buf, size, offset);
    } else {
	return vcs_fuse_os_read(ch, path, buf, size, offset);
    }
    return -1;
}

static int vcs_fuse_chown(const char *path, uid_t uid, gid_t gid)
{
    DBG_VCM("CHOWN %s\n"
	     "  uid: %d\n"
	     "  gid: %d\n",
	     path, uid, gid);
    return 0;
}

static int vcs_fuse_flush(const char *path, struct fuse_file_info *fi)
{
    UNUSED_ARGUMENT(path);
    UNUSED_ARGUMENT(fi);
    return 0;
}

static void *vcs_fuse_init(struct fuse_conn_info *conn)
{
    DBG_VCM("INIT major:%u minor:%u async_read:%u max_write:%u max_readahead:%u\n",
	conn->proto_major, conn->proto_minor, conn->async_read, 
	conn->max_write, conn->max_readahead);

    // NB: Signal the acceptor thread that it can start accepting connections.
    pthread_cond_signal(&vc->acceptor.cond);
    return NULL;
}


static struct fuse_operations vcs_fuse_operations = {
    .getattr	= vcs_fuse_getattr,
    .readdir	= vcs_fuse_readdir,
    .open	= vcs_fuse_open,
    .release	= vcs_fuse_release,
    .read	= vcs_fuse_read,
    .chown	= vcs_fuse_chown,
    .flush	= vcs_fuse_flush,
    .init	= vcs_fuse_init,
};


int vcs_fuse_main(void)
{
    int retry_cnt, ret;
    struct fuse *fh;
    struct fuse_session *se;
    struct fuse_chan *fc;
    uint32_t i;
    struct fuse_args args = FUSE_ARGS_INIT(0, NULL);
    static const char* vcs_fuse_args[] = {
	"/opt/nkn/sbin/nvsd",                       // binary path and name
	"-o",                                       // option flag
	"nonempty,direct_io,ro,fsname=virtcache",   // options
    };

    for (i = 0; i < sizeof(vcs_fuse_args)/sizeof(vcs_fuse_args[0]); i++) {
	ret = fuse_opt_add_arg(&args, vcs_fuse_args[i]);
	if (ret == -1) {
	    DBG_VCE("vcs_fuse_main(): failed to add arg %s", vcs_fuse_args[i]);
	    fuse_opt_free_args(&args);
	    return ret;
	}
    }

    prctl(PR_SET_NAME, "virt-cache-fs", 0,0,0);

retry:
    fc = fuse_mount(VCS_MOUNT_POINT, &args);
    if (!fc) {
	if (retry_cnt < VCS_FUSE_MAX_RETRIES) {
	    //do a lazy unmount and try again
	    umount2(VCS_MOUNT_POINT, 0);
	    retry_cnt++;
	    goto retry;
	} else {
	    fuse_opt_free_args(&args);
	    return -1;
	}
    }

    fh = fuse_new(fc, &args, &vcs_fuse_operations, sizeof(vcs_fuse_operations), NULL);
    if (!fh) {
	goto unmount;
    }

    se = fuse_get_session(fh);
    if (!se) {
	goto cleanup;
    }

    if (fuse_set_signal_handlers(se)) {
	goto cleanup;
    }

    fuse_loop_mt(fh);

    fuse_remove_signal_handlers(se);
    fc = fuse_session_next_chan(se, NULL);
    fuse_unmount(VCS_MOUNT_POINT, fc);
    fuse_chan_destroy(fc);
    fuse_exit(fh);
    fuse_opt_free_args(&args);
    return 0;

cleanup:
    fuse_destroy(fh);
    fh = NULL;
unmount:
    fuse_unmount(VCS_MOUNT_POINT, fc);
    fuse_chan_destroy(fc);
    fuse_opt_free_args(&args);
    return -1;
}


/*
int vcs_fuse_main(void)
{
    int ret;
    uint32_t i;
    struct fuse_args args = FUSE_ARGS_INIT(0, NULL);
    static const char* vcs_fuse_args[] = {
	"/opt/nkn/sbin/nvsd",                       // binary path and name
	"-f",                                       // foreground
//	"-s",                                       // single-threaded (debugging)
	"-o",                                       // option flag
	"nonempty,direct_io,ro,fsname=virtcache",   // options
	VCS_MOUNT_POINT
    };

    for (i = 0; i < sizeof(vcs_fuse_args)/sizeof(vcs_fuse_args[0]); i++) {
	ret = fuse_opt_add_arg(&args, vcs_fuse_args[i]);
	if (ret == -1) {
	    DBG_VCE("vcs_fuse_main(): failed to add arg %s", vcs_fuse_args[i]);
	    fuse_opt_free_args(&args);
	    return ret;
	}
    }

    ret = fuse_main(args.argc, args.argv, &vcs_fuse_operations, NULL);
    fuse_opt_free_args(&args);
    return ret;
}
*/
