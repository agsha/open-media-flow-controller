/*
 *
 * tdefaults.h
 *
 *
 *
 */

#ifndef __TDEFAULTS_H_
#define __TDEFAULTS_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

/* ------------------------------------------------------------------------- */
/** \file tdefaults.h Default values for constants which may be defined
 * in customer.h.  If a definition is omitted from customer.h, the value
 * is taken from here instead.
 * \ingroup lc
 */

/* ----------------------------------------------------------------------------
 * NOTES FOR THE DEVELOPER:
 *
 *   - Do NOT add any #include directives to this file, and do NOT #include
 *     it from anywhere besides customer.h!  This can cause problems due to
 *     circular dependencies.  It must be #include'd at the bottom of
 *     customer.h.
 *
 *   - All items in this file should correspond to something in customer.h,
 *     and should be listed in the same order as the items in customer.h.
 *     (Using either the 'demo' or 'generic' product as a reference, as 
 *     they both should be in the same order.)
 *
 *   - Always define the default value separately first, using "_DEF" as
 *     a suffix to the original constant name.  Then define the main 
 *     constant to the default if it's not already defined.
 *
 *   - Every #define'd constant should be in all caps, and should have
 *     a corresponding lowercase variable, extern-declared here, and
 *     declared in tdefaults.c.  This will be linked into libcommon,
 *     mainly to help out with gdb, since those can be inspected in
 *     gdb, while preprocessor constants cannot.
 *
 *   - This cannot be #include'd from common.h due to possible circular
 *     dependency problems.
 */


/******************************************************************************
 ******************************************************************************
 *** Licensing
 ******************************************************************************
 ******************************************************************************
 ***/

#define RESTRICTED_CMDS_LICENSED_FEATURE_NAME_DEF "RESTRICTED_CMDS"
#ifndef RESTRICTED_CMDS_LICENSED_FEATURE_NAME
#define RESTRICTED_CMDS_LICENSED_FEATURE_NAME \
        RESTRICTED_CMDS_LICENSED_FEATURE_NAME_DEF
#endif

extern const char *restricted_cmds_licensed_feature_name_def;
extern const char *restricted_cmds_licensed_feature_name;


/******************************************************************************
 ******************************************************************************
 *** AAA general options
 ******************************************************************************
 ******************************************************************************
 ***/

/*
 * We do not define a default for AAA_TRUSTED_AUTH_INFO_SHARED_SECRET
 * because we want to force customers to define their own.
 */

#define AAA_EXTRA_USER_PARAM_ROLE_DEF "role"
#ifndef AAA_EXTRA_USER_PARAM_ROLE
#define AAA_EXTRA_USER_PARAM_ROLE \
        AAA_EXTRA_USER_PARAM_ROLE_DEF
#endif


/******************************************************************************
 ******************************************************************************
 *** Authentication logging options
 ******************************************************************************
 ******************************************************************************
 ***/

#ifdef aaa_log_unknown_usernames
#define aaa_log_unknown_usernames_flag 1
#else
#define aaa_log_unknown_usernames_flag 0
#endif


/******************************************************************************
 ******************************************************************************
 *** Capabilities
 ******************************************************************************
 ******************************************************************************
 ***/

#define md_passwd_default_gid_def mgt_admin
#ifndef md_passwd_default_gid
#define md_passwd_default_gid md_passwd_default_gid_def
#endif


#ifdef __cplusplus
}
#endif

#endif /* __TDEFAULTS_H_ */
