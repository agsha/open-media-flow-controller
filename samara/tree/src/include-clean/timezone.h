/*
 *
 * timezone.h
 *
 *
 *
 */

#ifndef __TIMEZONE_H_
#define __TIMEZONE_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

/* ------------------------------------------------------------------------- */
/** \file timezone.h Timezone routines
 * \ingroup lc
 */

#include "tree.h"

typedef struct lt_tz_opt {
    /**
     * Path to time zone information file, relative to /usr/share/zoneinfo.
     */
    const char *lto_zonefile;

    /**
     * User-friendly single string name for this time zone, used in
     * the Web UI.  If this field is the empty string, this entry
     * should not be shown in the Web UI.
     *
     * Often this will be the same as the lto_zonefile, except with
     * underscores replaced with spaces.
     */
    const char *lto_ui_str;

    /**
     * User-friendly hierarchical name for this time zone.  Each level
     * of the hierarchy is delimited by a space character.
     */
    const char *lto_name;

    /**
     * Should this option be hidden in the CLI?  This would normally
     * be used for options that were available before, and we don't
     * want them anymore, but we have to keep them available for
     * backward compatibility.
     */
    tbool lto_hidden;

} lt_tz_opt;

extern const lt_tz_opt lt_tz_opts[];

/**
 * The array lt_tz_opts is loaded into a tree.  The path to each node
 * is the string in lto_name, broken into words by spaces.  The value
 * of each node is the corresponding lto_zonefile.
 */
extern tree *lt_timezones;

/**
 * The array lt_tz_opts is also loaded into another tree.  It node
 * names and structure are identical to lt_timezones, but its values
 * are the lt_tz_opt pointers, rather than just the lto_zonefiles.
 */
extern tree *lt_timezone_structs;

uint32 lt_get_num_timezones(void);

/**
 * Call this to populate lt_timezones, which will be NULL before
 * initialization.
 */
int lt_timezone_init(void);

/**
 * Call this to free lt_timezones when you're done with it.
 */
int lt_timezone_deinit(void);

/**
 * Call this if you think the timezone may have changed.  This is required
 * because of a deficiency in many libc's that if /etc/localtime is changed,
 * running processes do not use the new timezone in evaluating localtime().
 */
int lt_timezone_handle_changed(void);

#ifdef __cplusplus
}
#endif

#endif /* __TIMEZONE_H_ */
