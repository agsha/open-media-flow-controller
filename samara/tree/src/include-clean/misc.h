/*
 *
 * misc.h
 *
 *
 *
 */

#ifndef __MISC_H_
#define __MISC_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

/* ------------------------------------------------------------------------- */
/** \file src/include/misc.h Other miscellaneous common functions.
 * \ingroup lc
 */

#include "common.h"
#include "tstr_array.h"

/* ------------------------------------------------------------------------- */
/** Take a command line pattern that presumably has wildcards in it
 * and return from the 'actual' command line the values that appear
 * in the positions of the wildcards. This assumes the command and
 * the pattern have the same number of 'words'.
 */
int lc_cli_cmd_pattern_match(const char *cmd_line_pattern,
                             const char *cmd_line, tstr_array **ret_param_arr,
                             tbool *ret_match);

/* ------------------------------------------------------------------------- */
/** Given a command line and the position of a character within that line,
 * determine whether or not the character (a) was inside double quotes,
 * and (b) was directly backslash-escaped.  Generally, if either of these
 * is true, the character should be taken literally.  They are detected
 * separately because some callers may need to act differently depending
 * on which case it is, e.g. removing the escaping backslash.
 *
 * Note: to be inside double quotes, it does not matter if the quotes
 * are closed after the character in question; it just requires that
 * there be an odd number of unescaped double quotes prior to the
 * character.
 */
int lc_cli_check_char_context(const char *cmd_line,
                              uint32 cursor_pos,
                              tbool *ret_is_inside_quotes,
                              tbool *ret_is_escaped);

/* ------------------------------------------------------------------------- */
/** Map a model name (as specified at manucturing) to a friendly
 * description, according to the mapping table defined in the graft
 * point in src/lib/libcommon/misc.c.
 */
int lc_model_name_to_descr(const char *model_name, tstring **ret_model_descr);

uint16 lc_calc_ip_checksum(const uint16 *addr, int len, uint16 csum);

#ifdef __cplusplus
}
#endif

#endif /* __MISC_H_ */
