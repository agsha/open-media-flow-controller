/*
 *
 * ftrans_ssh.h
 *
 *
 *
 */

#ifndef __FTRANS_SSH_H_
#define __FTRANS_SSH_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

/* ------------------------------------------------------------------------- */
/** \file ftrans_ssh.h Utilities for transfering files via ssh (scp, sftp)
 * \ingroup lc
 */

#include "common.h"
#include "tstring.h"
#include "ftrans_utils.h"

/*
 * NOTE: lft_ssh_protocol_name_map, lft_ssh_protocol_url_prefix_map, and
 * lft_ssh_protocol_err_prefix_map (in ftrans_ssh.c) must be kept in sync.
 *
 * XXX/SML: Also, the lft_ssh_protocol enum values below are made to match
 * those in lft_url_protocol to ensure the same protocol value in the event
 * that a caller inadvertantly passes a lft_url_protocol to one of these
 * functions.  In the future, maybe deprecate this list in favor of the latter.
 */
typedef enum {
    lftsp_none = LFT_PROTOCOL_NONE,
    lftsp_scp = LFT_PROTOCOL_SCP,  /* SSH Secure Copy Protocol   */
    lftsp_sftp = LFT_PROTOCOL_SFTP, /* SSH File Transfer Protocol */
} lft_ssh_protocol;

extern const lc_enum_string_map lft_ssh_protocol_name_map[];

extern const lc_enum_string_map lft_ssh_protocol_url_prefix_map[];

extern const lc_enum_string_map lft_ssh_protocol_err_prefix_map[];


/**
 * Transfer a file via the specified ssh-bassed protocol (blocking wait).
 * If pull_file == true, copy from remote to local, else local to remote.
 *
 * If the transfer does not complete successfully, a string containing
 * a description of the error is returned in ret_output.
 *
 * \param protocol The protocol to use for file transfer
 * \param pull_file Direction of the operation (set to true for download)
 * \param password_fd Set to true if providing a remote_password.  The
 * password will be passed to the the ssh client via a secure pipe, else
 * it is ignored.  Note that if remote_password is NULL, nothing will be
 * sent on the pipe, and the protocol will assume an empty password.
 * \param remote_user Username to use for remote authentication
 * \param remote_password Password for user.  Note: 1) ignored unless
 * password_fd is true; 2) NULL is equivalent to an empty password.
 * \param remote_host Name or IP address of the remote system
 * \param remote_port The sshd service port number, or 0 for the default
 * \param remote_path Path for the file on the remote system
 * \param local_path The file to upload from or directory to download to
 * \param progress_oper_id Operation ID to use to track progress, or NULL
 * \retval ret_output Error text from ssh
 */
int lft_ssh_transfer_file(lft_ssh_protocol protocol,
                          tbool pull_file, tbool password_fd,
                          const char *remote_user,
                          const char *remote_password,
                          const char *remote_host,
                          uint16 remote_port,
                          const char *remote_path,
                          const char *local_path,
                          const char *progress_oper_id,
                          tstring **ret_output);


/**
 * Transfer a file via the specified ssh-bassed protocol without blocking.
 * If pull_file == true, copy from remote to local, else local to remote.
 * This function should be followed with a call to
 * lft_ssh_transfer_file_non_blocking_complete() when the child transfer
 * process exists.
 *
 * \param protocol The protocol to use for file transfer
 * \param pull_file Direction of the operation (set to true for download)
 * \param password_fd Set to true if providing a remote_password.  The
 * password will be passed to the the ssh client via a secure pipe, else
 * it is ignored.  Note that if remote_password is NULL, nothing will be
 * sent on the pipe, and the protocol will assume an empty password.
 * \param remote_user Username to use for remote authentication
 * \param remote_password Password for user.  Note: 1) ignored unless
 * password_fd is true; 2) NULL is equivalent to an empty password.
 * \param remote_host Name or IP address of the remote system
 * \param remote_port The sshd service port number, or 0 for the default
 * \param remote_path Path for the file on the remote system
 * \param local_path The file to upload from or directory to download to
 * \param progress_oper_id Operation ID to use to track progress, or NULL
 * \retval ret_launch_result Return variable for the transfer process
 * results.  The caller must free the contents and the structure by
 * invoking lc_launch_result_free_contents() and safe_free().
 * \retval ret_ft_state Return variable for contextual information about
 * the non-blocking transfer required by the transfer completion call
 * function.  Must be freed by the caller.
 */
int lft_ssh_transfer_file_non_blocking(lft_ssh_protocol protocol,
                                       tbool pull_file, tbool password_fd,
                                       const char *remote_user,
                                       const char *remote_password,
                                       const char *remote_host,
                                       uint16 remote_port,
                                       const char *remote_path,
                                       const char *local_path,
                                       const char *progress_oper_id,
                                       lc_launch_result **ret_launch_result,
                                       lft_state **ret_ft_state);


/**
 * Completion function companion to lft_ssh_transfer_file_non_blocking() to
 * be called when the child transfer process exits.
 *
 * If the transfer did not complete successfully, a non-zero lc error code is
 * is returned in ret_return_code and a string containing a description of
 * the error is returned in ret_output.
 *
 * \param protocol The protocol to use for file transfer
 * \param inout_launch_result A copy of the launch result returned by the
 * transfer function.  Prior to passing this, set its lr_child_pid to -1,
 * and it's lr_exit_status to the status returned by waitpid().  Lanch
 * results are stored in lr_captured_output after some tailoring, and file
 * cleanup is performed if an error occurs.  The caller must free the
 * contents and the structure by invoking lc_launch_result_free_contents()
 * and safe_free().
 * \param inout_ft_state Contextual information about the transfer
 * indicating what file (if any) should be removed on error.  Presently
 * this parameter structure is not altered by the call.
 * \retval ret_return_code Error code from the transfer process itself.
 * \retval ret_output Error text from the transfer process (must be freed
 * by the caller).
 */
int lft_ssh_transfer_file_non_blocking_complete(lft_ssh_protocol protocol,
                                                lc_launch_result **
                                                    inout_launch_result,
                                                lft_state **
                                                    inout_ft_state,
                                                int *ret_return_code,
                                                tstring **ret_output);


#ifdef __cplusplus
}
#endif

#endif /* __FTRANS_SSH_H_ */
