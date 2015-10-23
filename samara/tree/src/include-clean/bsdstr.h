/*
 *
 * bsdstr.h
 *
 *
 *
 */

#ifndef __BSDSTR_H_
#define __BSDSTR_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

#include <sys/types.h>

/* ------------------------------------------------------------------------- */
/** \file src/include/bsdstr.h Safer string copy functions, like strlcpy()
 * \ingroup lc
 */


#if !defined(PROD_TARGET_OS_SUNOS) && !defined(PROD_TARGET_OS_FREEBSD) && !defined(TMS_NO_STRLCPY)

/* ------------------------------------------------------------------------- */
/** Copy src to string dst of size siz.  At most siz-1 characters
 * will be copied.  Always NUL terminates (unless siz == 0).
 * Returns strlen(src); if retval >= siz, truncation occurred.
 * \retval ret_dst Destination
 * \param src Source string
 * \param siz How much to copy
 */
size_t strlcpy(char *ret_dst, const char *src, size_t siz);

/* ------------------------------------------------------------------------- */
/** Append src to string dst of size siz (unlike strncat, siz
 * is the full size of dst, not space left).  At most siz-1 characters
 * will be copied.  Always NUL terminates (unless siz <= strlen(dst)).
 * Returns strlen(src) + MIN(siz, strlen(initial dst)).  If retval >=
 * siz, truncation occurred.
 * \retval ret_dst Destination
 * \param src Source string
 * \param siz How much to append
 */
size_t strlcat(char *ret_dst, const char *src, size_t siz);

#else /* !defined(PROD_TARGET_OS_SUNOS) && !defined(PROD_TARGET_OS_FREEBSD) && !defined(TMS_NO_STRLCPY) */

#ifndef TMS_NO_STRLCPY
#include <string.h>
#endif

#endif

/* ------------------------------------------------------------------------- */
/** Allocate a buffer of size siz, then use strlcpy() to copy src into it.
 * Note that because the string has to be NUL-terminated, only a maximum
 * of siz-1 bytes of the string's contents will be copied.
 *
 * \return Copied string
 * \param src Source string
 * \param siz How much to append
 */
char *strldup(const char *src, size_t siz);

#ifdef __cplusplus
}
#endif

#endif /* __BSDSTR_H_ */
