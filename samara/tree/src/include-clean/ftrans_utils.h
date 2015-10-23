/*
 *
 * ftrans_utils.h
 *
 *
 *
 */

#ifndef __FTRANS_UTILS_H_
#define __FTRANS_UTILS_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

#include "common.h"
#include "tstring.h"
#include "proc_utils.h"

/* ------------------------------------------------------------------------- */
/** \file ftrans_utils.h Utilities for transfering files
 * \ingroup lc
 */

/*
 * XXX/EMT: the other ftrans_... header files should be merged
 * into this one.
 */

/* These definitions may be used by various protocol enum definitions */
#define LFT_PROTOCOL_NONE   0
#define LFT_PROTOCOL_SCP    1
#define LFT_PROTOCOL_SFTP   2
#define LFT_PROTOCOL_HTTP   3
#define LFT_PROTOCOL_HTTPS  4
#define LFT_PROTOCOL_FTP    5
#define LFT_PROTOCOL_TFTP   6

/**
 * Download a file.
 *
 * \param remote_url A URL indicating which file to download.  It may
 * specify any of the following protocols: scp, ftp or http.  For scp,
 * please see the format described in lu_parse_scp_url().
 *
 * \param password An optional password to use with an scp URL.  Note
 * that if a password (including the empty string) is contained in
 * remote_url, it will take precedence and this parameter will be
 * ignored.
 *
 * \param local_dir
 * \param local_filename
 * \param local_path
 *
 * \param mode Specifies the mode that should be assigned to the 
 * downloaded file.  If -1 is specified, the mode is not set, and
 * we take whatever happens naturally (probably according to the
 * umask of the current process).  Otherwise, it must be in [0..07777].
 *
 * \param uid Specifies the uid that should be assigned to the
 * downloaded file.  If -1 is specified for this and 'gid', the uid is
 * not set.  Note that if either of these is specified, both must be;
 * specifying -1 for only one of them is an error.
 *
 * \param gid Specifies the gid that should be assigned to the 
 * downloaded file.  If -1 is specified for this and 'uid', the gid is
 * not set.  Note that if either of these is specified, both must be;
 * specifying -1 for only one of them is an error.
 *
 * \param overwrite
 *
 * \param progress_oper_id Operation ID to use for progress tracking,
 * if desired.  Assumes that the operation and step have already been
 * begun, and only step updates need to be sent.  To skip progress
 * tracking, pass NULL.
 *
 * \retval ret_errors
 */
int lf_download_file(const char *remote_url, 
                     const char *password,
                     const char *local_dir,
                     const char *local_filename, 
                     const char *local_path,
                     int32 mode, int32 uid, int32 gid,
                     tbool overwrite, 
                     const char *progress_oper_id,
                     tstring **ret_errors);


/**
 * Upload a file.
 *
 * \param local_path Location of file to upload
 * \param remote_url Remote URL to upload to
 * \param password Password to use for scp transfers.
 * \retval ret_errors Error indication
 */
int lf_upload_file(const char *local_path, 
                   const char *remote_url,
                   const char *password,
                   tstring **ret_errors);

/** 
 * This is used to differentiate the transfer types.
 */ 
/* XXX/SML:  We should deprecate use of this enum in favor of lft_url_protocol */
typedef enum {
    lfts_scp = 0,
    lfts_sftp = 1,
    lfts_curl = 2,
    lfts_tftp = 3,
} lfts_types;


/* Structure used to store running state of a non-blocking transfer */
typedef struct lft_state {
    /* Target file path, used if we need to remove on failure */
    char lfts_local_path[1024];  /* XXX magic number */
    tbool lfts_error_remove_local_path; /* If there's an error, do we remove
                                         * lfts_local_path 
                                         */
    /* Tracks the type of transfer which was used. */
    lfts_types lfts_type;
    int32 lfts_mode;
    int32 lfts_uid;
    int32 lfts_gid;
} lft_state;


int lf_download_file_non_blocking(const char *remote_url, 
                                  const char *password,
                                  const char *local_dir,
                                  const char *local_filename,
                                  const char *local_path,
                                  int32 mode, int32 uid, int32 gid,
                                  tbool overwrite, 
                                  const char *progress_oper_id,
                                  lc_launch_result **ret_launch_result,
                                  lft_state **ret_ft_state);


int lf_download_file_non_blocking_complete(lc_launch_result **
                                           inout_launch_result,
                                           lft_state **
                                           inout_curl_state,
                                           int *ret_return_code,
                                           tstring **ret_output);


int lf_upload_file_non_blocking(const char *local_path, 
                                const char *remote_url, 
                                const char *password,
                                lc_launch_result **ret_launch_result,
                                lft_state **ret_ft_state);

int lf_upload_file_non_blocking_complete(lc_launch_result **
                                         inout_launch_result,
                                         lft_state **
                                         inout_ft_state,
                                         int *ret_return_code,
                                         tstring **ret_output);


#ifdef __cplusplus
}
#endif

#endif /* __FTRANS_UTILS_H_ */
