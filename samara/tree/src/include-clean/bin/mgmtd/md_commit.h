/*
 *
 * src/bin/mgmtd/md_commit.h
 *
 *
 *
 */

#ifndef __MD_COMMIT_H_
#define __MD_COMMIT_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

#include "common.h"
#include "mdb_db.h"
#include "md_mod_commit.h"

static const uint32 mdm_commit_max_iterations = 11;
static const uint32 mdm_commit_max_iterations_prewarn = 4;

extern uint32 md_commit_num_commits_in_progress;

/* Must match GETTEXT_PACKAGE as defined in the mgmtd Makefile */
#define MD_GETTEXT_DOMAIN "mgmtd"

/*
 * It would be better for these to be static const char * variables,
 * so they can be queried from GDB.  But for I18N, we need them to
 * be #defines so they can handle the string lookup for us.
 * 
 * I18N note: we use D_() instead of _() here because these may be
 * used from a variety of contexts outside mgmtd, so the
 * GETTEXT_DOMAIN preprocessor constant that _() depends on may be
 * wrong.
 */
#define MDM_COMMIT_INTERNAL_ERROR \
    D_("Internal error, code 1001 (see logs for details)", MD_GETTEXT_DOMAIN)
#define MDM_APPLY_INTERNAL_ERROR \
    D_("Internal error, code 1002 (see logs for details)", MD_GETTEXT_DOMAIN)
#define MDM_ACTION_INTERNAL_ERROR \
    D_("Internal error, code 1003 (see logs for details)", MD_GETTEXT_DOMAIN)
#define MDM_EVENT_INTERNAL_ERROR \
    D_("Internal error, code 1004 (see logs for details)", MD_GETTEXT_DOMAIN)
#define MDM_COMMIT_CONVERGE_INTERNAL_ERROR \
    D_("Internal error, code 1005 (see logs for details)", MD_GETTEXT_DOMAIN)
#define MDM_QUERY_INTERNAL_ERROR \
    D_("Internal error, code 1006 (see logs for details)", MD_GETTEXT_DOMAIN)

#define mdm_type_general_error \
    D_("Invalid value.", MD_GETTEXT_DOMAIN)

int md_commit_commit_state_free(md_commit **ret_commit);

#ifdef __cplusplus
}
#endif

#endif /* __MD_COMMIT_H_ */
