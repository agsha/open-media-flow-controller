/*
 *
 */

#ifndef __TTIME_H_
#define __TTIME_H_

#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

/* ========================================================================= */
/** \file src/include/ttime.h Time related data types and routines.
 * \ingroup lc
 *
 * This library works with four basic types of time values: datetime, 
 * date, daytime, and duration.  Each type has three possible levels 
 * of precision: second, millisecond, and microsecond.  Datetimes and
 * daytimes are stored in one of the ::lt_time_sec, ::lt_time_ms, or
 * ::lt_time_us data types.  Dates are stored in the ::lt_date data
 * type.  Durations are stored in one of the ::lt_dur_sec, ::lt_dur_ms,
 * or ::lt_dur_us data types.
 */

#include <sys/time.h>
#include <time.h>
#include "common.h"
#include "typed_array_direct_tmpl.h"


/* ============================================================================
 * Basic type definitions, and getting the current time
 */

/* ------------------------------------------------------------------------- */
/** @name Time data types
 *
 * These data types may represent either datetimes or daytimes.
 * It is up to the code to use any given variable in a consistent
 * manner.
 *
 * A datetime represents a date and a time together.  The internal
 * representation is a number of time units (sec, ms, us) since the
 * Epoch, which is 1970/1/1 00:00:00 UTC.  This library has routines
 * to render datetimes as strings either in UTC or in localtime,
 * according to the current system time zone.
 *
 * A daytime represents just a time of day, and does not specify any
 * date.  The internal representation is a number of time units (sec,
 * ms, us) since midnight on an unspecified date in an unspecified 
 * timezone.
 *
 * Note that datetimes are sometimes referred to in the function names
 * as "times" for short.  A "time" here still refers to a datetime
 * type, and are not to be confused with "daytimes", which represent 
 * a time of day.
 */
/*@{*/
typedef int32 lt_time_sec;
typedef int64 lt_time_ms;
typedef int64 lt_time_us;
/*@}*/

#define LT_TIME_SEC_MIN INT32_MIN
#define LT_TIME_SEC_MAX INT32_MAX
#define LT_TIME_MS_MIN INT64_MIN
#define LT_TIME_MS_MAX INT64_MAX
#define LT_TIME_US_MIN INT64_MIN
#define LT_TIME_US_MAX INT64_MAX


/* ------------------------------------------------------------------------- */
/** @name Date data type
 *
 * Represents just a date, and does not specify any time on that date.
 * The internal representation is the number of seconds from the Epoch
 * to midnight of this date, UTC.  Thus any lt_date value which is not
 * a multiple of 86400 is not valid.
 *
 * In the case of dates (and daytimes as they relate to dates), we
 * make a distinction between the INTERNAL REPRESENTATION and the
 * INTERPRETATION.  Dates are internally represented in UTC, but they
 * may be interpreted as either UTC or localtime depending on how you
 * operate on them.  For example, lt_date_daytime_to_time_sec() 
 * interprets a date and daytime together as if they refer to a local
 * time, and convert them into a datetime, which is unambiguously UTC.
 * This operation takes into account the current time zone.  On the
 * other hand, lt_date_daytime_utc_to_time_sec(), interprets the date 
 * and daytime as being in UTC, so the conversion is a simple addition
 * of the two values.
 */
/*@{*/
typedef int32 lt_date;
/*@}*/


/* ------------------------------------------------------------------------- */
/** @name Duration data types
 *
 * Represents the difference between two datetimes (or possibly
 * daytimes).  The internal representation is simply a number of time
 * units (sec, ms, us).
 */
/*@{*/
typedef int32 lt_dur_sec;
typedef int64 lt_dur_ms;
typedef int64 lt_dur_us;
/*@}*/

#define LT_DUR_SEC_MIN INT32_MIN
#define LT_DUR_SEC_MAX INT32_MAX
#define LT_DUR_MS_MIN INT64_MIN
#define LT_DUR_MS_MAX INT64_MAX
#define LT_DUR_US_MIN INT64_MIN
#define LT_DUR_US_MAX INT64_MAX


/* ------------------------------------------------------------------------- */
/** @name Getting the current time
 *
 * These functions return "now" as a datetime: the current number of
 * seconds, milliseconds, or microseconds since the Epoch.
 */
/*@{*/
lt_time_sec lt_curr_time(void);
lt_time_ms lt_curr_time_ms(void);
lt_time_us lt_curr_time_us(void);
/*@}*/


/* ============================================================================
 * Conversions between different time-related types
 */

/* ------------------------------------------------------------------------- */
/** @name Conversion: datetime --> date + daytime (representing localtime)
 *
 * Convert a datetime into a date and daytime, where the date and
 * daytime are to be interpreted as localtime.  This conversion IS
 * affected by the current time zone.
 */
/*@{*/
int lt_time_to_date_daytime_sec(lt_time_sec time_sec, 
                                lt_date *ret_date,
                                lt_time_sec *ret_daytime_sec);
int lt_time_to_date_daytime_ms(lt_time_ms time_ms,
                                lt_date *ret_date,
                                lt_time_ms *ret_daytime_ms);
int lt_time_to_date_daytime_us(lt_time_us time_us,
                                lt_date *ret_date,
                                lt_time_us *ret_daytime_us);
/*@}*/

/* ------------------------------------------------------------------------- */
/** @name Conversion: datetime --> date + daytime (representing UTC)
 *
 * Convert a datetime into a date and daytime, where the date and
 * daytime are to be interpreted as UTC.  This conversion is NOT 
 * affected by the current time zone.
 */
/*@{*/
int lt_time_to_date_daytime_utc_sec(lt_time_sec time_sec, 
                                    lt_date *ret_date,
                                    lt_time_sec *ret_daytime_sec);
int lt_time_to_date_daytime_utc_ms(lt_time_ms time_ms,
                                   lt_date *ret_date,
                                   lt_time_ms *ret_daytime_ms);
int lt_time_to_date_daytime_utc_us(lt_time_us time_us,
                                   lt_date *ret_date,
                                   lt_time_us *ret_daytime_us);
/*@}*/

/* ------------------------------------------------------------------------- */
/** @name Conversion: date + daytime (representing localtime) --> datetime
 *
 * Convert a date and daytime (interpreted as localtime) into a
 * datetime.  This conversion IS affected by the current time zone.
 */
/*@{*/
int lt_date_daytime_to_time_sec(lt_date date, lt_time_sec daytime_sec,
                                lt_time_sec *ret_time_sec);
int lt_date_daytime_to_time_ms(lt_date date, lt_time_ms daytime_ms,
                                lt_time_ms *ret_time_ms);
int lt_date_daytime_to_time_us(lt_date date, lt_time_us daytime_us,
                                lt_time_us *ret_time_us);
/*@}*/

/* ------------------------------------------------------------------------- */
/** @name Conversion: date + daytime (representing UTC) --> datetime
 *
 * Convert a date and daytime (interpreted as UTC) into a datetime.
 * This conversion is NOT affected by the current time zone.
 */
/*@{*/
int lt_date_daytime_utc_to_time_sec(lt_date date, lt_time_sec daytime_sec,
                                    lt_time_sec *ret_time_sec);
int lt_date_daytime_utc_to_time_ms(lt_date date, lt_time_ms daytime_ms,
                                   lt_time_ms *ret_time_ms);
int lt_date_daytime_utc_to_time_us(lt_date date, lt_time_us daytime_us,
                                   lt_time_us *ret_time_us);
/*@}*/

/* ------------------------------------------------------------------------- */
/** @name Conversion: datetime --> struct timeval
 *
 * Convert a datetime to a struct timeval, which also expresses the
 * amount of time elapsed since the Epoch.  This conversion is NOT
 * affected by the current time zone.
 */
/*@{*/
int lt_time_sec_to_timeval(lt_time_sec time_sec, struct timeval *ret_tv);
int lt_time_ms_to_timeval(lt_time_ms time_ms, struct timeval *ret_tv);
int lt_time_us_to_timeval(lt_time_us time_us, struct timeval *ret_tv);
/*@}*/

/* ------------------------------------------------------------------------- */
/** @name Conversion: struct timeval --> datetime
 *
 * Convert a struct timeval to a datetime.  Returns -1 if tv is NULL.
 * This conversion is NOT affected by the current time zone.
 */
/*@{*/
lt_time_sec lt_timeval_to_time_sec(const struct timeval *tv);
lt_time_ms lt_timeval_to_time_ms(const struct timeval *tv);
lt_time_us lt_timeval_to_time_us(const struct timeval *tv);
/*@}*/

/* ------------------------------------------------------------------------- */
/** @name Conversion: datetime --> struct tm
 * 
 * Convert a datetime to a struct tm, which has fields for all of the
 * ways people represent datetimes: year, month, day, hour, minute,
 * second, etc.  The variants ending in "..._utc" are NOT affected
 * by the current timezone and set the fields to represent the
 * datetime in UTC.  The basic versions which don't end in "..._utc"
 * ARE affected by the current timezone, and set the fields to
 * represent the datetime in localtime.
 */
/*@{*/
int lt_time_sec_to_tm(lt_time_sec time_sec, struct tm *ret_tm);
int lt_time_sec_to_tm_utc(lt_time_sec time_sec, struct tm *ret_tm);
int lt_time_ms_to_tm(lt_time_ms time_ms, struct tm *ret_tm);
int lt_time_ms_to_tm_utc(lt_time_ms time_ms, struct tm *ret_tm);
int lt_time_us_to_tm(lt_time_us time_us, struct tm *ret_tm);
int lt_time_us_to_tm_utc(lt_time_us time_us, struct tm *ret_tm);
/*@}*/

/* ------------------------------------------------------------------------- */
/** @name Conversion: struct tm --> datetime
 * 
 * Convert a struct tm to a datetime.  The variants ending in
 * "..._utc" are NOT affected by the current timezone and assume the
 * struct tm is expressed in UTC.  The basic versions which don't end
 * in "..._utc" ARE affected by the current timezone, and assume the
 * struct tm is expressed in localtime.
 *
 * Note that the redundant fields tm_wday and tm_yday are ignored
 * during this conversion, so it's OK if you have updated the other
 * members of the structure without keeping them up to date.
 * Also, anything outside their normal interval is normalized,
 * e.g. July 35th becomes August 4th.
 *
 * Run 'man mktime' to see the contents of 'struct tm'.  Please note
 * that if you start with a zeroed out structure and don't fill in
 * tm_isdst, that will be taken as a declaration that it is NOT DST,
 * regardless of whether or not it actually was at the date and time
 * given.  This will often result in being off by one hour for cases
 * which WERE in DST.  Set tm_isdst to -1 to avoid this.
 */
/*@{*/
lt_time_sec lt_tm_to_time_sec(const struct tm *tm);
lt_time_sec lt_tm_utc_to_time_sec(const struct tm *tm);
lt_time_ms lt_tm_to_time_ms(const struct tm *tm);
lt_time_ms lt_tm_utc_to_time_ms(const struct tm *tm);
lt_time_us lt_tm_to_time_us(const struct tm *tm);
lt_time_us lt_tm_utc_to_time_us(const struct tm *tm);
/*@}*/

/* ------------------------------------------------------------------------- */
/** @name Rendering/parsing: date <--> string
 *
 * Convert a date to a string or vice-versa.  This operation is NOT
 * affected by the current timezone.
 *
 * The format generated and accepted for input is "YYYY/MM/DD".
 *
 * The remainder fields are not currently implemented.
 */
/*@{*/
int lt_date_to_buf(lt_date date, uint32 buf_len, char *ret_buf,
                   lt_time_sec *ret_remainder);
int lt_date_to_str(lt_date date, char **ret_str,
                   lt_time_sec *ret_remainder);
int lt_str_to_date(const char *str, lt_date *ret_date);
/*@}*/

/* ------------------------------------------------------------------------- */
/** @name Rendering/parsing: daytime <--> string
 *
 * Convert a daytime to a string or vice-versa.  This operation is NOT
 * affected by the current timezone.
 *
 * The format generated and accepted for input is "HH:MM:SS.nnnnnn".
 * The fractional seconds are optional for input, and only generated
 * in output if the daytime being rendered has fractional seconds.
 */
/*@{*/
int lt_daytime_sec_to_buf(lt_time_sec ts, uint32 buf_len, char *ret_buf);
int lt_daytime_ms_to_buf(lt_time_ms tm, uint32 buf_len, char *ret_buf);
int lt_daytime_us_to_buf(lt_time_us tu, uint32 buf_len, char *ret_buf);
int lt_daytime_sec_to_str(lt_time_sec ts, char **ret_str);
int lt_daytime_ms_to_str(lt_time_ms tm, char **ret_str);
int lt_daytime_us_to_str(lt_time_us tu, char **ret_str);
int lt_str_to_daytime_sec(const char *str, lt_time_sec *ret_ts);
int lt_str_to_daytime_ms(const char *str, lt_time_ms *ret_tm);
int lt_str_to_daytime_us(const char *str, lt_time_us *ret_tu);
/*@}*/

/* ------------------------------------------------------------------------- */
/** @name Rendering/parsing: datetime <--> string (localtime)
 *
 * Convert a datetime to a string in localtime or vice-versa.
 * This operation IS affected by the current timezone.
 *
 * Except for the "*_str_fmt_*" functions, the format generated and accepted
 * for input is "YYYY/MM/DD HH:MM:SS.nnnnnn".  The fractional seconds are
 * optional for input, and only generated in output if the daytime being
 * rendered has fractional seconds.
 *
 * Functions that take a fmt parameter support strptime() format specifiers,
 * allowing the caller to parse arbitrary input formats.  The caller is
 * responsible for format validity, and an illegal format would fail
 * regardless of the input.  The fmt functions do NOT provide subsecond
 * precision as strptime() deals only in seconds.
 *
 * The fmt parameter is optional.  If NULL, the function will attempt to
 * match first the default format generated by ctime(), and failing that,
 * the default format of the system 'date' command, which differs only by
 * inclusion of the time zone.  Note that the timezone, if present in the
 * input, is ignored since time is assumed to be local.
 *
 * NOTE: when converting from a string to a datetime, a timezone
 * offset at the end of the string (e.g. "-0800") is not permitted,
 * and will result in a "bad type" error.  The date and time are 
 * always assumed to be in the current timezone.
 */
/*@{*/
int lt_datetime_sec_to_buf(lt_time_sec dts, tbool incl_tz,
                           uint32 buf_len, char *ret_buf);
int lt_datetime_ms_to_buf(lt_time_ms dtm, tbool incl_tz,
                          uint32 buf_len, char *ret_buf);
int lt_datetime_us_to_buf(lt_time_us dtu, tbool incl_tz,
                          uint32 buf_len, char *ret_buf);
int lt_datetime_sec_to_str(lt_time_sec dts, tbool incl_tz, char **ret_str);
int lt_datetime_ms_to_str(lt_time_ms dtm, tbool incl_tz, char **ret_str);
int lt_datetime_us_to_str(lt_time_us dtu, tbool incl_tz, char **ret_str);
int lt_str_to_datetime_sec(const char *str, lt_time_sec *ret_dts);
int lt_str_to_datetime_ms(const char *str, lt_time_ms *ret_dtm);
int lt_str_to_datetime_us(const char *str, lt_time_us *ret_dtu);

int lt_str_fmt_to_datetime_sec(const char *str,
                               const char *fmt,
                               lt_time_sec *ret_dts);
int lt_str_fmt_to_datetime_ms(const char *str,
                              const char *fmt,
                              lt_time_ms *ret_dtm);
int lt_str_fmt_to_datetime_us(const char *str,
                              const char *fmt,
                              lt_time_us *ret_dtu);
/*@}*/

/* ------------------------------------------------------------------------- */
/** @name Rendering/parsing: datetime <--> string (UTC)
 *
 * Convert a datetime to a string in UTC or vice-versa.  
 * This operation is NOT affected by the current timezone.
 *
 * The format generated and accepted for input is
 * "YYYY/MM/DD HH:MM:SS.nnnnnn".  The fractional seconds are optional
 * for input, and only generated in output if the daytime being
 * rendered has fractional seconds.
 */
/*@{*/
int lt_datetime_sec_to_buf_utc(lt_time_sec dts, tbool incl_tz,
                               uint32 buf_len, char *ret_buf);
int lt_datetime_ms_to_buf_utc(lt_time_ms dtm, tbool incl_tz,
                              uint32 buf_len, char *ret_buf);
int lt_datetime_us_to_buf_utc(lt_time_us dtu, tbool incl_tz,
                              uint32 buf_len, char *ret_buf);
int lt_datetime_sec_to_str_utc(lt_time_sec dts, tbool incl_tz, char **ret_str);
int lt_datetime_ms_to_str_utc(lt_time_ms dtm, tbool incl_tz, char **ret_str);
int lt_datetime_us_to_str_utc(lt_time_us dtu, tbool incl_tz, char **ret_str);
int lt_str_utc_to_datetime_sec(const char *str, lt_time_sec *ret_dts);
int lt_str_utc_to_datetime_ms(const char *str, lt_time_ms *ret_dtm);
int lt_str_utc_to_datetime_us(const char *str, lt_time_us *ret_dtu);
/*@}*/

/* ------------------------------------------------------------------------- */
/** @name Rendering/parsing: duration <--> string
 *
 * Convert a duration to a string, or vice-versa.  The string produced
 * by a conversion from duration has the word "seconds" (subject to
 * localization) appended to it; a string to be converted to duration
 * type may have this string but it is not required.  This operation
 * is NOT affected by the current timezone.
 *
 * The format generated and accepted for input is "ssss.nnnnnn"
 * (floating point), with s in seconds, for durations
 */
/*@{*/
int lt_dur_sec_to_buf(lt_dur_sec ds, uint32 buf_len, char *ret_buf);
int lt_dur_ms_to_buf(lt_dur_ms dm, uint32 buf_len, char *ret_buf);
int lt_dur_us_to_buf(lt_dur_us du, uint32 buf_len, char *ret_buf);
int lt_dur_sec_to_str(lt_dur_sec ds, char **ret_str);
int lt_dur_ms_to_str(lt_dur_ms dm, char **ret_str);
int lt_dur_us_to_str(lt_dur_us du, char **ret_str);
int lt_str_to_dur_sec(const char *str, lt_dur_sec *ret_ds);
int lt_str_to_dur_ms(const char *str, lt_dur_ms *ret_dm);
int lt_str_to_dur_us(const char *str, lt_dur_us *ret_du);
/*@}*/

/* ------------------------------------------------------------------------- */
/** @name Rendering/parsing: duration --> string (fancy)
 *
 * Convert a duration in milliseconds to a string in an more
 * human-readable format.  These functions are NOT affected by
 * the current timezone.
 */
/*@{*/

/**
 * Render a duration in milliseconds into a fancy-style string.
 * 
 * \param dm Duration in milliseconds
 * \param ret_str Where the return the rendered string in 
 * dynamically-allocated memory.
 * \param fixed See lt_render_flags::lrf_fixed.
 * \param use_long_words See lt_render_flags::lrf_long_words.
 */
int lt_dur_ms_to_counter_str_ex(lt_dur_ms dm, char **ret_str, tbool fixed,
                                tbool use_long_words);

/*
 * NOTE: lc_render_flags_map (in ttime.c) must be kept in sync.
 */
typedef enum {
    lrf_none = 0,
    
    /** 
     * Specifies whether the result should have a fixed structure,
     * vs. omitting unnecessary spaces and words.  For fixed format,
     * hours, minutes, and seconds are always two digits, but days 
     * are not.  e.g. "0d 4h 18m 0s" would be fixed, and "4h 18m" 
     * would not.
     */
    lrf_fixed = 1 << 0,

    /**
     * Specifies whether we should use full words like "days",
     * "hours", etc., instead of the single-character designations.
     */
    lrf_long_words = 1 << 1,

    /**
     * Specifies to use medium-length abbreviations, in between the
     * single-character ones and the full-word ones.  These would be
     * "sec", "min", "hr", and "day[s]".
     */
    lrf_medium_words = 1 << 2,

    /**
     * Specifies not to put spaces between the different components
     * of the string, e.g. "4h18m" instead of "4h 18m".
     */
    lrf_packed = 1 << 3,

} lt_render_flags;

typedef uint32 lt_render_flags_bf;

extern const lc_enum_string_map lc_render_flags_map[];

int lt_dur_ms_to_counter_str_ex2(lt_dur_ms dm, char **ret_str,
                                 lt_render_flags_bf flags);

/**
 * Convert a string rendering of a duration into a number of
 * microseconds.  We accept any string that could have been generated
 * by lt_dur_ms_to_counter_str_ex2() using any combination of flags;
 * plus we honor any number of digits after the decimal place which 
 * can fit in a 'double'.
 */
int lt_counter_str_to_dur_us(const char *counter_str, lt_dur_us *ret_dur_us);

/**
 * Like lt_counter_str_to_dur_us(), but returns result in milliseconds.
 *
 * XXX/EMT: if the value would fit in an int64 as milliseconds, but
 * not as microseconds, we would ideally be able to handle it.  But we
 * are just a wrapper around lt_counter_str_to_dur_us() for backward
 * compatibility, so we'd still overflow in that case.
 */
int lt_counter_str_to_dur_ms(const char *counter_str, lt_dur_ms *ret_dur_ms);

/**
 * Render a duration in milliseconds into a fancy-style string.
 * This is kept for backward compatibility: it just calls
 * lt_dur_ms_to_counter_str_ex() with \c fixed and \c use_long_words
 * both as \c false.
 */
int lt_dur_ms_to_counter_str(lt_dur_ms dm, char **ret_str);

/*@}*/


/* ========================================================================= */
/** @name Miscellaneous functions
 */

/*@{*/

/* 
 * Array of lt_time_sec elements.  It currently just stores them
 * directly cast as (void *)s because they are the same size.  If
 * lt_time_sec were made a larger type, these would need to be stored
 * as pointers to dynamically-allocated lt_time_secs instead.
 */
/* \cond */
TYPED_ARRAY_DIRECT_HEADER_NEW_NONE(lt_time_sec, lt_time_sec);
/* \endcond */

/**
 * Allocate a new array with elements of type ::lt_time_sec.
 */
int lt_time_sec_array_new(lt_time_sec_array **ret_arr);

/**
 * Return the current system uptime in milliseconds.
 */
lt_dur_ms lt_sys_uptime_ms(void);

/**
 * Return the current system uptime in microseconds.
 */
lt_dur_ms lt_sys_uptime_us(void);

/**
 * Convert a system uptime (a prior return value from lt_sys_uptime_ms())
 * into a datetime, given the current system clock.
 */
lt_time_ms lt_sys_uptime_ms_to_datetime_ms(lt_dur_ms sys_uptime_ms);

/**
 * Return the current system boot's unique ID.  This is randomly
 * generated by the kernel to be different each time the system has
 * started.
 */
int lt_sys_boot_id(char **ret_bootid);

/**
 * Add a number of milliseconds to a struct timeval.
 */
int lt_timeval_add_ms(struct timeval *tv, lt_dur_ms dur_ms);

/**
 * Tell if a given year is a leap year.  This does not take into
 * account any discontinuities that may have happened in the past; it
 * just blindly applies the rules about dividing by 4, 100, and 400.
 */
int lt_is_leap_year(uint32 year, tbool *ret_is_leap_year);

/**
 * Get the number of days in a particular month.
 *
 * \param year Year to check.  This only affects the answer for February.
 * \param month Month to check, in [1..12].
 * \retval ret_num_days The number of days in this month.
 */
int lt_num_days_in_month(uint32 year, uint32 month, uint32 *ret_num_days);

/**
 * Returns the offset from UTC of the active timezone, at the
 * specified datetime (specified in UTC), in seconds.  e.g. if we are
 * 1 hour before UTC, this would return -3600.
 *
 * \param base_time The time in UTC at which we are to compute the offset,
 * or -1 to use the current time.
 *
 * \retval ret_offset The UTC offset in seconds.
 */
int lt_time_get_timezone_offset(lt_time_sec base_time, lt_dur_sec *ret_offset);

/**
 * Compare two 'struct tm' structures.  Return -1 if the first
 * parameter is earlier (less than) than the second, 0 if they match,
 * and 1 if the first parameter is later (greater than).
 *
 * Note that the tm_yday field of the structures is NOT used in this
 * comparison, since sometimes callers will only be updating the other
 * fields.
 */
int32 lt_tm_cmp(struct tm *tm1, struct tm *tm2);

/**
 * Dump the contents of a 'struct tm' to the logs.
 */
int lt_tm_dump(int log_level, const char *prefix, const struct tm *tm);

typedef enum {
    lw_sunday =    0,
    lw_monday =    1,
    lw_tuesday =   2,
    lw_wednesday = 3,
    lw_thursday =  4,
    lw_friday =    5,
    lw_saturday =  6,
} lt_weekday;

extern const lc_enum_string_map lt_weekday_abbrev_map[];

extern const lc_enum_string_map lt_weekday_full_map[];

/*@}*/

#define lt_seconds_per_day  (60*60*24)
#define lt_seconds_per_hour (60*60)
#define lt_seconds_per_min  (60)

/**
 * A safe size to use for a buffer to be passed to the ..._to_buf()
 * routines above.  The routines are guaranteed to never run over
 * this limit.  This leaves room for the following format:
 * YYYY/MM/DD HH:MM:SS.XXXXXX -xxxx
 * plus a few extra characters for padding.
 */
static const uint32 lt_max_datetime_buf_size = 36;

#ifdef __cplusplus
}
#endif

#endif /* __TTIME_H_ */
