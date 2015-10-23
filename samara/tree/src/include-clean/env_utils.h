/*
 *
 * env_utils.h
 *
 *
 *
 */

#ifndef __ENV_UTILS_H_
#define __ENV_UTILS_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

#include "common.h"

/* ------------------------------------------------------------------------- */
/** \file env_utils.h Cross-platform utilities for manipulating environment
 * variables.
 *
 * See docs/design/multithreading.txt for thread safety notes.
 * \ingroup lc
 */

/* ------------------------------------------------------------------------- */
/** Just like getenv().  The string returned is part of the environment,
 * so should not be modified or freed by the caller.  If no variable
 * matches 'name', or if 'name' is NULL, returns NULL.
 *
 * THIS FUNCTION IS NOT THREAD SAFE.
 *
 * \param name Environment variable name
 * \return Vaule if found, NULL otherwise
 */
const char *lc_getenv(const char *name);

/* ------------------------------------------------------------------------- */
/** Threadsafe version of lc_getenv().  Caller assumes ownership of 
 * returned value.
 */
int lc_getenv_ts(const char *name, char **ret_value);

/* ------------------------------------------------------------------------- */
/** Just like setenv(), except that it's available on all platforms.
 * Ownership of the parameters passed is not assumed.  If there was
 * already a variable with the name specified and 'overwrite' was
 * 'false', the environment is not changed and success is returned.
 *
 * \param name Environment variable name.  May not have the '=' character
 * in it; if it does, an error will be returned.
 * \param value Value for environment variable.
 * \param overwrite Should we overwrite an existing environment variable 
 * with the same name?
 */
int lc_setenv(const char *name, const char *value, tbool overwrite);

/* ------------------------------------------------------------------------- */
/** A cheap imitation of unsetenv().  Instead of removing a variable,
 * it just sets it to the empty string if it exists.  If the variable
 * does not exist, it is not created, and lc_err_not_found is
 * returned.
 *
 * Note that unlike unsetenv(), this does not change the indices at
 * which other environment variables can be retrieved using
 * lc_getenv_nth(), and it does not prevent this name from being read
 * from the environment in the future.
 *
 * \param name Environment variable name
 */
int lc_setenv_empty(const char *name);

/* ------------------------------------------------------------------------- */
/** A cheap imitation of clearenv().  Instead of removing all variables,
 * it just sets them all to the empty string.
 *
 * This has the same differences vs. clearenv() as lc_setenv_empty()
 * has vs. unsetenv().
 */
int lc_setenv_empty_all(void);

/* ------------------------------------------------------------------------- */
/** Just like putenv(), except that it takes an extra level of
 * indirection on the pointer so it can NULL out the caller's copy of
 * the pointer to follow the Tall Maple memory ownership convention:
 * we do take ownership of the string passed.
 *
 * \param inout_string Name/value pair in the form "name=value".  The name
 * may not include an '=' character.  Will be set to NULL.
 */
int lc_putenv(char **inout_string);

/* ------------------------------------------------------------------------- */
/** Retreive the nth environment variable.  The strings returned are
 * copies and must be freed by the caller.  'n' is not bounds-checked,
 * except that lc_err_not_found will be returned for the element
 * immediately past the end of the array.  Thus callers should only
 * access the array in sequence to avoid going off the end of it.
 *
 * \param n Nth environment variable
 * \retval ret_name Name of the Nth environment variable
 * \retval ret_value Value of the Nth environment variable
 */
int lc_getenv_nth(uint32 n, char **ret_name, char **ret_value);

/* ------------------------------------------------------------------------- */
/** Dump all environment variables to the logs, for debugging purposes.
 */
int lc_dump_env(void);

#ifdef __cplusplus
}
#endif

#endif /* __ENV_UTILS_H_ */
