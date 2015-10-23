/*
 *
 * i18n.h
 *
 *
 *
 */

#ifndef __I18N_H_
#define __I18N_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

/**
 * \file src/include/i18n.h Internationalization APIs and definitions
 * \ingroup lc
 */

/* As we do not include common.h, check if this include is prohibited */
#if defined(NON_REDIST_HEADERS_PROHIBITED)
#error Non-redistributable header file included when prohibited
#endif

#include "tinttypes.h"
#include "ttypes.h"

#if defined(PROD_FEATURE_I18N_SUPPORT)
#include <locale.h>
#include <libintl.h>

/**
 * Read the file in which we store the current locale and set the
 * locale accordingly for this program.
 *
 * If log_all_errors is true, we log any errors we encounter.  If it
 * is false, we do not log errors trying to read the locale file, but
 * still log any other errors.  Everyone except mgmtd should pass
 * true to this function.
 */
int lc_set_locale_from_config(tbool log_all_errors);

#define I18N_LOCALE_DEFAULT "en_US.utf8"

#endif /* defined(PROD_FEATURE_I18N_SUPPORT) */

#define lc_charset_plain "ISO-8859-1"
#define lc_charset_utf8  "utf-8"


/* ------------------------------------------------------------------------- */
/** @name Definitions for I18N using gettext.
 *
 * This section defines some
 * macros that can be put around string literals.  Each has two
 * characteristics: (a) whether xgettext is told to scan for this
 * macro and copy its parameter into the string catalog at build time;
 * and (b) whether it expands to a call to gettext() to look up the
 * string in the string catalog at runtime.
 *
 *   _("s") puts "s" into the string catalog, and invokes gettext
 *   with whatever domain GETTEXT_DOMAIN is defined to be.
 *
 *   I_("s") does neither.  It is for "internal" messages which we may
 *   want to localize later, but not right now.  It can be changed later
 *   to behave the same as _("s").
 *
 *   N_("s") puts "s" into the string catalog, but does not invoke gettext.
 *
 *   GT_(s, d) does not put its parameter (which will generally be a
 *   variable rather than a string literal) into the string catalog,
 *   but does invoke gettext.  It also requires a gettext domain to be
 *   passed to it.
 *
 *   D_(s, d) puts "s" into the string catalog, and invokes gettext
 *   with a specified domain.
 *
 * Here are some guidelines for determining which macro, if any, to use
 * in conjunction with a string:
 *
 *   - If it is a string to be displayed immediately to the end user,
 *     or a user-visible message to be returned in a mgmtd response,
 *     use _("s").
 *
 *   - If the string is in a place where function calls are not legal,
 *     e.g. the initialization of a variable, use N_("s").  Later,
 *     when the variable the string was put into is used, use GT_()
 *     on it before using it.
 *
 *   - If the string is assigned at a point where the locale may change
 *     between now and when the string is actually displayed, use
 *     N_("s") now and GT_() later.  For example, CLI modules
 *     register their help messages at initialization, but the CLI
 *     displays them later.  The CLI infrastructure knows that 
 *     strings it is given at init time are not literal strings, but
 *     are keys to be fed to gettext for translation at the
 *     appropriate time.
 *
 *     Note that this does NOT apply to help strings returned by your
 *     help callback.  Those strings are displayed immediately, and
 *     thus the CLI expects you to call _() yourself.
 *
 *     It is because GT_() is called later that it requires a gettext
 *     domain parmaeter.  It is expected that often the domain the
 *     string is in is different from the domain of the code looking
 *     it up in the string catalog.  In the example of CLI modules 
 *     above, the libcli domain is active when GT_() is called,
 *     though the string may be in the cli_mods domain, or in the 
 *     domain of some separate customer CLI modules.
 *
 *     As a convention to help keep track of which variables hold strings
 *     which still need to be translated, we suggest suffixing the 
 *     variable name with _gtk (for "gettext key").
 *
 *   - If the string is defined in a header file where other code from
 *     potentially different gettext domains will call it, use
 *     D_().  Cause the header file to be scanned by xgettext by
 *     mentioning it in one of your Makefile.i18n files (only .c files
 *     are picked up by default), and define the strings to be calls
 *     to D_() with the correct domain.
 *
 *   - If the string is a log message, there are three cases:
 *
 *       1. The log message is not expected to be seen by the 
 *          average user, so it does not need to be localized.
 *          Do not use any macros for these messages.  This applies
 *          to all messages logged at level INFO or DEBUG.
 *
 *       2. The log message might not be seen by the average user,
 *          so it might not need to be localized.  Use I_("s") for
 *          these messages.  We can easily flip a switch controlling
 *          whether these messages are localizable or not.  This
 *          generally applies to errors which the average user should
 *          never see because they indicate internal errors with the
 *          TMS software or the modules or daemons written to integrate
 *          with it.  e.g. error messages logged by the sanity checks
 *          during CLI module command registration.
 *
 *       3. The log message will definitely be seen by the average
 *          user, so it must be localized.  Use _("s").  This applies
 *          to all other log messages: those at level NOTICE or above
 *          which do not refer to error conditions, or which refer
 *          to errors which may arise in a "normally bug-free" system
 *          due to conditions external to the software generating
 *          the logs.
 *
 *   - If the string is not to be viewed by a user under any
 *     circumstances -- e.g. a binding name, filename, etc. --
 *     then don't use any of these macros.
 *
 */

/*@{*/

#if defined(PROD_FEATURE_I18N_SUPPORT) && \
    defined(GETTEXT_DOMAIN)
    #define _(a) dgettext(GETTEXT_DOMAIN,a)
#else 
    #define _(a) (a)
#endif /* PROD_FEATURE_I18N_SUPPORT && GETTEXT_DOMAIN */

#if defined(PROD_FEATURE_I18N_SUPPORT) && \
    defined(PROD_FEATURE_I18N_EXTRAS) && \
    defined(GETTEXT_DOMAIN)
    #define I_(a) dgettext(GETTEXT_DOMAIN,a)
#else
    #define I_(a) (a)
#endif

#if defined(PROD_FEATURE_I18N_SUPPORT)
    #define GT_(a,d) dgettext(d,a)
    #define D_(a,d) dgettext(d,a)
#else
    #define GT_(a,d) (a)
    #define D_(a,d) (a)
#endif /* PROD_FEATURE_I18N_SUPPORT */

#define N_(a) (a)

/*@}*/

#ifdef __cplusplus
}
#endif

#endif /* __I18N_H_ */
