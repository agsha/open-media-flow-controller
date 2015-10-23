/*
 *
 * src/bin/web/cgi_launcher/lau_internal.h
 *
 *
 *
 */

#ifndef __LAU_INTERNAL_H_
#define __LAU_INTERNAL_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif


#include "common.h"
#include "cgi.h"
#include "proc_utils.h"

/**
 * Attempt to authenticate the user.
 * \param ctx the cgi context.
 * \param params the launch params to be updated.
 * \param update_activity a flag to be passed on to wsmd in the 
 * authentication request, specifying whether it should update its idea
 * of the last activity time for this user.  Some requests explicitly ask
 * for the last activity time to not be updated, e.g. if you're sitting on
 * an auto-refresh page.
 * \retval ret_username The username of the user as whom we are
 * authenticated.  If this was a remote authentication and the original
 * username was mapped to a local one, this is the original one.
 * Otherwise it should be the same as the local one.
 * \retval ret_local_username The local username of the user as whom we
 * are authenticated.
 * \return 0 on success, error code on failure.
 */
int lau_authenticate_user(lcgi_context *ctx, lc_launch_params *params,
                          tbool update_activity, char **ret_username,
                          char **ret_local_username);


#ifdef __cplusplus
}
#endif

#endif /* __LAU_INTERNAL_H_ */
