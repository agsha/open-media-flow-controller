/*
 *
 * ftrans_curl.h
 *
 *
 *
 */

#ifndef __FTRANS_CURL_H_
#define __FTRANS_CURL_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

/* ------------------------------------------------------------------------- */
/** \file ftrans_curl.h Utilities for transfering files via curl
 * \ingroup lc
 */

#include "common.h"
#include "tstring.h"
#include "ftrans_utils.h"

/**
 * Upload/download a file using curl via HTTP/FTP.
 *
 * \param pull_file Direction of the operation
 * \param overwrite Whether the destination file should overwrite if it exists
 * \param local_path The file to upload from or location to download to
 * \param dest_dir What directory to use for the operation
 * \param remote_url URL on remote system
 * \param progress_oper_id Operation ID to use to track progress, or NULL
 * to not track progress.
 * \param ret_output The output (or error text) from the executable
 */
int lft_curl_transfer_file(tbool pull_file, tbool overwrite, 
                           const char *local_path, const char *dest_dir,
                           const char *remote_url, 
                           const char *progress_oper_id,
                           tstring **ret_output);


int lft_curl_transfer_file_non_blocking(tbool pull_file, tbool overwrite, 
                                        const char *local_path, 
                                        const char *dest_dir,
                                        const char *remote_url,
                                        const char *progress_oper_id,
                                        lc_launch_result **ret_launch_result,
                                        lft_state **ret_ft_state);



int lft_curl_transfer_file_non_blocking_complete(lc_launch_result **
                                                 inout_launch_result,
                                                 lft_state **
                                                 inout_ft_state,
                                                 int *ret_return_code,
                                                 tstring **ret_output);


#ifdef __cplusplus
}
#endif

#endif /* __FTRANS_CURL_H_ */
