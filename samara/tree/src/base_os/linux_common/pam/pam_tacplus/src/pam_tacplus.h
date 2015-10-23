/* pam_tacplus command line options */
#define PAM_TAC_DEBUG		01
#define PAM_TAC_FIRSTHIT	02
#define PAM_TAC_ACCT		04 /* account on all specified servers */
#define PAM_TAC_USE_FIRST_PASS  10
#define PAM_TAC_TRY_FIRST_PASS  20

/* pam_tacplus major, minor and patchlevel version numbers */
#define PAM_TAC_VMAJ		1
#define PAM_TAC_VMIN		2
#define PAM_TAC_VPAT		9


#define MAXPWNAM 253    /* maximum user name length. Server dependent,
                         * this is the default value
                         */

/* Set iff pam_sm_authenticate() is successful in authenticating the user */
#define TMS_PAM_TACPLUS_AUTHENTICATE_SUCCESS "TMS-PAM-TACPLUS-AUTHENTICATE-SUCCESS"

/* Holds the global config started by pam_sm_authenticate() */
#define TMS_PAM_TACPLUS_GLOBAL_CONFIG "TMS-PAM-TACPLUS-GLOBAL-CONFIG"
