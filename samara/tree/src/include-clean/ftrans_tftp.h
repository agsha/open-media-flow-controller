/*
 *
 * ftrans_tftp.h
 *
 *
 *
 */

#ifndef __FTRANS_TFTP_H_
#define __FTRANS_TFTP_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

/* ------------------------------------------------------------------------- */

#include "common.h"
#include "tstring.h"
#include "ftrans_utils.h"

/**
 * Transfer a file via tftp.  If pull_file == true, copy from remote to
 * local (get), else from local to remote (put). 
 *
 * \param pull_file Direction of the operation
 * \param overwrite Overwrite the destination file
 * \param local_path Local file to send or save to
 * \param dest_dir Directory to save files in
 * \param remote_url URL passed in from the command line
 * \param progress_oper_id Operation ID to use to track progress, or NULL
 * to not track progress.
 * \param ret_output Error text from tftp
 */
int lft_tftp_transfer_file(tbool pull_file, tbool overwrite,
                           const char *local_path, const char *dest_dir,
                           const char *remote_url,
                           const char *progress_oper_id, 
                           tstring **ret_output);

int lft_tftp_transfer_file_non_blocking(tbool pull_file, tbool overwrite,
                                        const char *local_path,
                                        const char *dest_dir,
                                        const char *remote_url,
                                        const char *progress_oper_id, 
                                        lc_launch_result **ret_launch_result,
                                        lft_state **ret_ft_state);

int lft_tftp_transfer_file_non_blocking_complete(lc_launch_result **
                                                 inout_launch_result,
                                                 lft_state **inout_ft_state,
                                                 int *return_code,
                                                 tstring **ret_output);
#ifdef __cplusplus
}
#endif

#endif /* __FTRANS_TFTP_H_ */
