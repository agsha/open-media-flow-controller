/*
 *
 * graph.h
 *
 *
 *
 */

#ifndef __GRAPH_H_
#define __GRAPH_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

#include "gd.h"
#if defined(GD_H) && !defined(TMS_GD_H)
#error The incorrect "gd.h" is included.  Please add -I${PROD_TREE_ROOT}/src/lib/libgd to INCLUDES
#endif
#include "common.h"
#include "bnode.h"
#include "md_client.h"
#include "mdb_db.h"

/** \defgroup libgraph LibGraph: APIs for generating graphs of data sets */

/* ========================================================================= */
/** \file graph.h Graphing API
 * \ingroup libgraph
 *
 * This API allows the caller to create graphs from raw data supplied
 * (a) directly as bn_attribs using this API; (b) from sample and CHD
 * records in statsd; or (c) from any mgmtd nodes whose structure
 * matches that of statsd's state nodes.  Line graphs, area graphs,
 * bar graphs, stacked bar graphs, and pie charts are supported.  The
 * graph can be generated to a PNG file in the filesystem; or can be
 * returned to the caller in the form of a gdImagePtr.  The latter
 * option allows caller to use other libgd APIs to modify the graph
 * before saving, and/or to save in a different format.
 *
 * Because of this capability to post-operate on the graph using
 * libgd, there are certain desirable options we do not attempt to
 * support, since the caller can just as easily add them himself with
 * greater flexibility than we could provide., e.g. adding text to the
 * graph image with arbitrary locations, colors, fonts, etc.
 */


/* ========================================================================= */
/* Enums, constants, and atomic data types
 */

/* ------------------------------------------------------------------------- */
/** General type of graph.
 */
typedef enum {
    lgt_none = 0,

    /**
     * Line graph: plot points from one or more data series, and
     * connect points from each series with lines.  An "area graph" is
     * a special case of this, where regions bounded by the lines are
     * filled with color (when the ::ldgf_stack flag is used on the
     * data series group to be used).
     */
    lgt_line,

    /** 
     * Bar graph: for each data point, a bar will extend from the
     * discrete axis (the X axis by default) to the corresponding data
     * value.  If more than one series is involved, the bars for each
     * series may be either placed side by side, or stacked on top of
     * each other.
     *
     * NOTE: we currently require that the first series in a bar graph
     * have a data point for every X coordinate (or Y coordinate if
     * the axes are reversed) that will be used, i.e. for each bar.
     */
    lgt_bar,

    /**
     * Pie chart: draw a pie chart, with one segment per data value,
     * sized to show each value's percentage of the whole.
     */
    lgt_pie,

} lg_graph_type;


/* ------------------------------------------------------------------------- */
/** Flags that can be specified on a graph.  Note that some of these
 * are only applicable to certain kinds of graphs, as described in 
 * comments below.
 *
 * Attempts to use inappropriate flags should be caught at runtime.
 */
typedef enum {
    lgf_none = 0,

    /**
     * (Line graphs only) Draw a square dot at each data point.
     */
    lgf_dot_data_points = 1 << 0,

    /**
     * (Line graphs only) Skip drawing the lines.  Usually used in
     * conjunction with lgf_dot_data_points, to get a scatter plot.
     */
    lgf_skip_lines = 1 << 1,

    /**
     * (Area graphs and bar graphs only) Should we outline in black
     * any areas filled with color?  For area graphs, this means each
     * different region representing a data series; for bar graphs,
     * this means each bar (or bar segment if stacked).
     *
     * NOTE: this flag does not currently support pie charts.
     */
    lgf_outline = 1 << 2,

    /**
     * (Bar graphs only) Should we put a tick mark between each
     * element along the discrete axis; i.e. between each clump of
     * bars?
     */
    lgf_discrete_axis_ticks = 1 << 3,

    /**
     * (Bar graphs only) Instead of bars rising vertically from the
     * X axis, make the bars extend horizontally from the Y axis.
     * This makes the Y axis the "discrete" axis instead of the
     * X axis.  This is somewhat like rotating the graph 90 degrees
     * clockwise, while keeping text oriented properly.
     *
     * Note however that the fields which refer to X and Y
     * (lsg_x_axis, lsg_y_axis, lds_x_values, lds_y_values) do not
     * change their meaning, i.e. X is still the horizontal dimension,
     * and Y is still the vertical dimension.
     *
     * NOTE: this flag is not currently supported with line graphs
     * or area graphs.
     *
     * NOTE: this flag impacts the behavior of lg_axis_set_auto(),
     * so make sure it is set on your graph first, if you are going
     * to call that function.
     */
    lgf_reverse_axes = 1 << 4,

    /**
     * (Bar graphs without stacked values only) Print the value of
     * each bar beyond the end of it.  Normally this means printing
     * the value represented by the height above the bar; but if the
     * axes are swapped with lgf_reverse_axes, we print the value
     * represented by the length to the right of the bar.
     */
    lgf_bar_label_value = 1 << 5,

    /**
     * (Pie charts only) Render in 3D.  This just makes the graph look
     * flashier; it does not cause it to represent more or less data.
     *
     * (Could also support this with bar graphs, but that is not 
     * currently a requirement.)
     */
    lgf_3d = 1 << 6,

    /**
     * (Bar graphs and pie charts only) For each filled area that
     * represents a subset of the data being graphed, calculate a
     * polygon that expresses its perimeter.  This includes every
     * slice in a pie chart, every bar or bar segment in a bar graph,
     * and every entry in the legend for either.  The coordinates of
     * the vertices of this polygon are returned in the lgr_regions
     * field of the lg_graph_result.  Each entry is bound to a tag
     * that you specified with the data, to help you associate each
     * polygon with the data it corresponds to.  This can be used to
     * piece the polygons together into a client-side image map
     * allowing click-through on each portion of the graph.
     *
     * NOTE: see lg_image_map_render() and the lg_image_map_context
     * structure for an optional mechanism that uses this tagging
     * facility to generate a full HTML image map definition for you.
     *
     * NOTE: this is not supported for line/area graphs.
     */
    lgf_compute_regions = 1 << 7,

    /**
     * (Pie charts only) Should we antialias the drawing of the pie
     * itself?  Note that text is always antialiased if you specify
     * a TrueType font, but never otherwise.
     */
    lgf_antialiased = 1 << 8,

    /**
     * (Line or bar graphs with "stacked" data only) Should we treat
     * the data in the lsg_data_stacked field as absolute, rather than
     * relative?  That is, should we forego adding each data point to
     * the sum of all corresponding data points in previous series?
     *
     * NOTE: if you use this with an area graph (::ldgf_stack), you MUST 
     * add the data series to the data series array in ascending order.
     * Otherwise, the colors used to fill the regions in the graph will
     * be wrong.
     *
     * NOTE: this flag impacts the behavior of lg_axis_set_auto(),
     * so make sure it is set on your graph first, if you are going
     * to call that function.
     */
    lgf_stacked_abs = 1 << 9,

    /**
     * Do not declare a color as transparent for this graph.
     * By default, all background colors (lgg_margin_color,
     * lgg_graph_bgcolor, and lgg_plotting_area_bgcolor) are
     * transparent.  You can change these to non-transparent colors,
     * but the designated transparent color (0x00fefefe) will still be
     * declared transparent.  So set this flag if (a) you wanted to
     * use 0x00fefefe, or (b) as a shortcut to make all of the
     * backgrounds white without setting all of them individually.
     */
    lgf_no_transparency = 1 << 10,

    /**
     * (Area graphs with "snap-aligned" data: ldgf_align_snap flag on
     * the data series group) By default, an area graph with
     * snap-aligned data is drawn as a sort of "packed bar graph".
     * Each data point is a rectangular bar, but it is otherwise more
     * like an area graph, as there is no space between the bars, and
     * the X axis remains non-discrete.  But with the
     * lgf_smart_sloping flag, we instead slope the tops of each bar
     * towards the points on either side of it, provided there is a 
     * point to slope towards, and that ALL series have a data point
     * in the adjacent slot.
     */
    lgf_smart_sloping = 1 << 11,

    /**
     * Do not log a warning if we are forced to "recycle" colors
     * because we are asked to automatically choose a color more often
     * than we have predetermined colors.
     */
    lgf_color_recycling_nocomplain = 1 << 12,

} lg_graph_flags;


/* ------------------------------------------------------------------------- */
/** Specify how to orient text to be drawn.
 */
typedef enum {
    ltot_none = 0,

    /** 
     * Render text horizontally, to be read from left to right.
     */
    ltot_horizontal,

    /**
     * Render text vertically, to be read from bottom to top.
     */
    ltot_vertical,

    /**
     * Render text at an arbitrary angle (to be specified in a
     * separate field).
     */
    ltot_angle,

} lg_text_orientation_type;


/* ------------------------------------------------------------------------- */
/** Specify the content of a pie slice label.
 */
typedef enum {
    lplt_none = 0,

    /**
     * Labels showing the absolute value of the slice, from the
     * original data set.
     */
    lplt_absolute,

    /**
     * Labels showing the percentage of the pie taken up by the slice.
     */
    lplt_percentage,

    lplt_last = lplt_percentage

} lg_pie_label_type;


/* ------------------------------------------------------------------------- */
/** Flags to specify where a value label for a pie chart is to be
 * positioned.
 */
typedef enum {
    lpvlp_none = 0,

    /** Position the label inside the pie slice it goes with */
    lpvlp_inside = 1 << 0,

    /** Position the label outside the pie, next to the slice it goes with */
    lpvlp_outside = 1 << 1,

    /** Position the label in the legend, in parentheses after the name */
    lpvlp_legend = 1 << 2,

} lg_pie_value_label_pos;


/* ------------------------------------------------------------------------- */
/** A color to be used in drawing the graph or text on it.  This is a
 * 24-bit RGB value: the higher-order bits are reserved for internal 
 * use.  Of the lowest 24-bits, the highest 8 bits are for red, then
 * 8 bits for green, then 8 bits for blue.
 *
 * The special color lg_color_transparent (only appropriate for the
 * background color) means to make it transparent.
 */
typedef uint32 lg_color;

/**
 * Specify a fully transparent color.  Only legal to use for the
 * background color of a graph: in the lgg_margin_color,
 * lgg_graph_bgcolor, and lgg_plotting_area_bgcolor fields.  This will
 * be rejected from any other color field.
 */
#define lg_color_transparent (UINT32_MAX)

/**
 * Request that the library choose a color for you, as described in
 * lg_color_choose_auto().  Note that this value may be passed to the
 * functions for creating objects with a color (data series, pie
 * slice, etc.), and they will choose the color for you immediately.
 * It may not be assigned to those fields directly, though; this
 * will produce an error.
 */
#define lg_color_choose      (UINT32_MAX - 1)

#define LG_COL_RED(x)    ((((uint32) (x)) >> 16) & 0xff)
#define LG_COL_GREEN(x)  ((((uint32) (x)) >>  8) & 0xff)
#define LG_COL_BLUE(x)   (((uint32) (x))         & 0xff)

/**
 * The maximum width and height of an image the library will create.
 * Images will be clamped to this size.
 */

/*
 * Warning: do not set either of these bigger than 32767 , to avoid 
 *          triggering potential overflow bugs in libgd.
 */
enum {
    lg_image_width_max = 8191,
    lg_image_height_max = 8191
};


/* ------------------------------------------------------------------------- */
/* Names of fonts we have available.
 */

/** Luxi monospaced regular */
static const char lg_font_luxi_mr[] = "luximr.ttf";

/** Luxi sans-serif regular */
static const char lg_font_luxi_sr[] = "luxisr.ttf";

/** Luxi sans-serif bold */
static const char lg_font_luxi_sb[] = "luxisb.ttf";

/** Luxi narrow sans-serif regular */
static const char lg_font_luxi_rr[] = "luxirr.ttf";

/** Vera sans-serif */
static const char lg_font_vera[] = "Vera.ttf";

/** Vera with serifs */
static const char lg_font_vera_se[] = "VeraSe.ttf";


/* ------------------------------------------------------------------------- */
/** A file format into which a graph can be rendered.
 */
typedef enum {
    lgff_none = 0,

    /** Use the PNG file format */
    lgff_png,

} lg_file_format;


/* ------------------------------------------------------------------------- */
/** Options for fetching data to be graphed from statsd (or from nodes
 * whose structure matches statsd's).
 */
typedef enum {
    lsf_none = 0,

    /**
     * Fill gaps in the series by interpolating from adjacent values.
     * We detect gaps by watching instance numbers, which are present
     * in the bindings fetched even though they are not represented in
     * the resultant data series; thus, the interpolation must be done
     * while translating the bindings into a data series, and not
     * after.  If we are fetching multiple series at once, we also pad
     * out the ends of each series to the min and max timestamp across
     * the whole set, to ensure that they are fully aligned.
     *
     * NOTE: this option requires that any given instance number
     * always pairs with the same X value; and that the instances are
     * evenly spaced, i.e. the distance in X values between any two
     * adjacent instance numbers is the same.
     *
     * NOTE: this option is potentially made obsolete by the general
     * data series group option ::ldgf_interpolate.  That
     * interpolation option works on any data set, not requiring it to
     * come from statsd nodes or to meet the two criteria about
     * instance numbers mentioned in the above note.  This option
     * may be removed in the future if it is determined to add no
     * value.  (It might be faster...?)
     */
    lsf_interpolate = 1 << 0,

    /**
     * Convert the data points fetched into the float64 data type.
     * This may result in a small performance penalty, but allows the
     * data to maintain more precision if any calculations are done on
     * it after it has been fetched.  e.g. if the 'divisor' parameter
     * to lg_graph_fetch_stats_data() is specified, or if the data
     * is placed in a series group with the ldgf_interpolate flag.
     */
    lsf_convert_to_float = 1 << 1,

} lg_stats_flags;

/** Bit field of ::lg_stats_flags ORed together */
typedef uint32 lg_stats_flags_bf;


/* ------------------------------------------------------------------------- */
/** Flags to lg_axis_set_auto()
 */
typedef enum {
    laaf_none = 0,

    /**
     * By default, if the min of all the data is >= zero, we choose
     * zero as the bottom of the axis range.  This flag specifies to 
     * not do that, and to use our normal heuristics to choose a 
     * bottom endpoint according to the actual min of the data.
     * You do not need this flag if any of your data is less than
     * zero, as in that case we choose an endpoint based on the
     * data as you would expect.
     *
     * You'll commonly want to use this on the X axis of a line or
     * area graph, though we do not try to detect that case and supply
     * the flag for you automatically.
     *
     * NOTE: it may be tempting to do this on the Y axis to avoid
     * "wasting" the space between zero and the min of your data.  But
     * keep in mind that this in some cases this would be considered a
     * misleading representation of data, as it makes the variation in
     * the data look greater than it really is.  e.g. if your data
     * varies from 4127.2 to 4127.3, and you use this flag, the Y
     * values will appear to fluctuate dramatically, when in fact they
     * are confined to a fairly narrow range.  And with stacked data
     * (an area graph or stacked bar graph), since we will choose the
     * min based on the first (lowest) data series, the area filled in
     * to represent the first data series will be artifically less
     * than its "real" area, while all of the other series will be
     * correct, making the first look smaller relative to them than it
     * really is.  On the other hand, there are some cases where this
     * would be appropriate, such as when the data is somehow
     * inherently bound to a range well above zero, and you want to
     * show the detail of its movements within this range.
     */
    laaf_no_zero_min = 1 << 0,

    /**
     * By default, we round the min and max of an axis to the nearest
     * multiple of the value label, major tick or minor tick spacing, as
     * specified by the laaf_round_endpoints_level_* flags below.
     *
     * Specify this flag to skip that and just use the exact min and max
     * of the data found in the graph.
     */
    laaf_no_round_endpoints = 1 << 1,

    /**
     * We require each indicator's spacing to be a multiple of the 
     * less-major spacings, so everything lines up nicely.  By default
     * we choose spacing in order from least major (minor ticks) to 
     * most major (value labels).  This means that the major ticks and
     * value labels may get less than optimal spacings, i.e. they might
     * have to skip the best spacing because it is not a multiple of
     * the ones already chosen.  This flag specifies to choose the
     * spacings in reverse order, starting from value labels.  Use it
     * if having the optimal spacing for value labels is more important
     * than having the optimal spacing for minor ticks.
     */
    laaf_optimize_value_labels = 1 << 2,

    /**
     * Do not cycle through powers of 10 with numbers from laao_mults:
     * just use them raw, and consider that there are no options 
     * above or below the number range it offers.
     *
     * This is most commonly expected to be used to scale an axis with
     * a time-related data type, where traditional powers-of-10-based
     * scaling would not produce good results.  In this case, you could
     * use lg_axis_auto_mult_time for laao_mults, or provide your own
     * list of options.
     */
    laaf_no_pow10 = 1 << 3,

} lg_axis_auto_flags;

/* ------------------------------------------------------------------------- */
/** @name Pre-set arrays for ::laao_mults field of ::lg_axis_auto_options.
 */
/*@{*/
static const float64 lg_axis_auto_mult_1_25_5[] = {5, 2.5, 1, 0};
static const float64 lg_axis_auto_mult_1_2_25_5[] = {5, 2.5, 2, 1, 0};
static const float64 lg_axis_auto_mult_1_2_5[] = {5, 2, 1, 0};
static const float64 lg_axis_auto_mult_1_5[] = {5, 1, 0};

#define lg_sec_per_min   60
#define lg_min_per_hour  60
#define lg_hour_per_day  24
#define lg_day_per_week  7
#define lg_sec_per_hour (lg_sec_per_min * lg_min_per_hour)
#define lg_sec_per_day (lg_sec_per_hour * lg_hour_per_day)
#define lg_sec_per_week (lg_sec_per_day * lg_day_per_week)

/**
 * This is a special series intended to be used to autoscaling of axes
 * based on a time or datetime data type.  These numbers are not used
 * with powers of 10: they are used raw (in other words, only with
 * 10^0).  Note that they are intended as numbers of seconds: if you
 * are working with a millisecond or microsecond-based data type,
 * you should make up your own list.
 *
 * NOTE: we do not currently deal with autoscaling axes that would
 * want the time quantum to be <1 second, or that wants it to be
 * something other than a fixed multiple.  1 week is the largest time
 * quantum with a fixed size: we can use multiples of that (or of
 * days), but we do not support non-constant spacing such as in terms
 * of months or years.
 */
static const float64 lg_axis_auto_mult_time[] = 
    {52 * lg_sec_per_week, 26 * lg_sec_per_week, 13 * lg_sec_per_week,
     4 * lg_sec_per_week, 2 * lg_sec_per_week, lg_sec_per_week,
     3 * lg_sec_per_day, 2 * lg_sec_per_day, lg_sec_per_day,
     12 * lg_sec_per_hour, 6 * lg_sec_per_hour, 4 * lg_sec_per_hour,
     2 * lg_sec_per_hour, lg_sec_per_hour,
     30 * lg_sec_per_min, 15 * lg_sec_per_min, 10 * lg_sec_per_min,
     5 * lg_sec_per_min, 2 * lg_sec_per_min, lg_sec_per_min,
     30, 15, 10, 5, 2, 1, 0};
/*@}*/


/* ========================================================================= */
/* Structures
 */

/* ------------------------------------------------------------------------- */
/** Specifies an orientation to use for rendering text.
 */
typedef struct lg_text_orientation {
    /**
     * General type of text orientation.
     */
    lg_text_orientation_type lto_type;

    /**
     * Only heeded if lto_type is ltot_angle.  Angle is expressed in
     * degrees counterclockwise from horizontal (3 o'clock).  
     * Must be between 0.0 (same as ltot_horizontal) and 90.0
     * (same as ltot_vertical).
     */
    double lto_angle;
} lg_text_orientation;


/* ------------------------------------------------------------------------- */
/** Definition of a "special line" to draw on a graph.  This is a line
 * to be drawn on either a line or bar graph, parallel to one of the
 * axes, extending from the other axis to the far end of the graph.
 * It is used to indicate a special level of some sort. e.g. it might
 * indicate a maximum safe value, to make it easier to see when values
 * were going out of the desirable range.  Each axis on a graph may
 * have any number of these, except that the discrete axis of a bar
 * graph may not have any.
 */
typedef struct lg_special_line {
    /**
     * Value at which the line should be placed.  This must have the
     * same data type as the values of the data points being graphed
     * along the same axis, and that type must be numeric.
     */
    bn_attrib *lsl_value;

    /**
     * Color to be used to draw this line.  Note: this MAY NOT be 
     * lg_color_transparent or lg_color_choose.
     */
    lg_color lsl_color;

#if 0
    /**
     * Should we add a label to indicate the value of this line?
     * The label will go opposite the axis from which the line is
     * drawn, mainly to ensure that it does not overlap with any
     * of the value labels on the axis itself.
     *
     * NOT YET IMPLEMENTED.
     */
    tbool lsl_label_opposite;
#endif

    /**
     * Descriptive name for this line, to be used in the legend.
     * If none is provided, one like "Line 1" will be automatically
     * generated.
     */
    tstring *lsl_legend_name;

    /**
     * Should this special line be drawn in the foreground, in front
     * of the main graph?
     */
    tbool lsl_fg;

} lg_special_line;

TYPED_ARRAY_HEADER_NEW_NONE(lg_special_line, lg_special_line *);

typedef enum {
    lgait_none = 0,
    lgait_minor_ticks,
    lgait_major_ticks,
    lgait_value_labels,
} lg_axis_indicator_type;

typedef enum {
    laia_none = 0,

    /**
     * Align the axis indicators with the origin.  i.e. start them at
     * lga_range_low for the axis, going up by the spacing from there.
     */
    laia_origin,

    /**
     * Align the axis indicators on integer multiples of the spacing.
     * i.e. start at the first multiple of the spacing past the origin,
     * and maintain spacing from there.
     */
    laia_multiples,

} lg_axis_indicator_alignment;


/* ------------------------------------------------------------------------- */
/** This structure holds options for "indicators" that can be posted
 * at regular intervals along an axis.  We use "indicator" as a
 * generic term that covers both tick marks and value labels.  Every
 * instance of this structure will pertain to a specific type of
 * indicator, so there is no field in the structure itself that
 * specifies which type of indicator we are referring to.
 */
typedef struct lg_axis_indicator {
    /**
     * Distance between indicators on the axis.  Specify NULL to
     * skip these indicators altogether.
     */
    bn_attrib *lai_spacing;

    /**
     * Specifies whether or not we will extend a line from this axis
     * to the other end of the graph at every point where this
     * indicator falls.  This is most appropriate for minor and major
     * ticks, though it will also be honored for value labels if
     * specified.
     */
    tbool lai_extend_line;

    /**
     * If lai_extend_line is true, specifies whether this line should
     * be in the foreground of the image, in front of the graph.
     * By default, it is in the background.
     */
    tbool lai_extend_line_fg;

    /**
     * If lai_extend_line is true, what color should we draw the line
     * in?  The default is a light blue for minor ticks, and a light red 
     * for major ticks or value labels.
     */
    lg_color lai_extend_line_color;

    /**
     * Skip drawing this indicator itself, but still draw the line
     * extensions (gridlines) if lai_extend_line is true.
     */
    tbool lai_skip_indicator;
   
} lg_axis_indicator;


/* ------------------------------------------------------------------------- */
/** Indicates a method of specifying how to format data values.
 * Used in conjunction with other fields in the lg_data_format
 * structure, which will specify more detail on the formatting.
 */
typedef enum {
    ldft_none = 0,

    /**
     * Render the data as a string in the most unformatted way
     * possible, that is, using only bn_attrib_get_tstr().  This
     * option does not pair with any other fields in lg_data_format.
     */
    ldft_raw,

    /**
     * Use default formatting for this type.  This will be the same as
     * ldft_raw in many cases, but in some cases may apply some
     * discretion, such as rendering datetimes as 'YYYY/MM/DD HH:MM:SS'
     * instead of as the number of seconds since the epoch.  This option
     * does not pair with any other fields in lg_data_format.
     */
    ldft_default,

    /**
     * Specify formatting using a printf format string.  Applicable only
     * to floating point types.
     */
    ldft_printf,

    /**
     * Specify formatting using a format string to be passed to
     * strftime().  Only applicable to time-related types:
     * date, time_sec, time_ms, time_us, datetime_sec, datetime_ms,
     * datetime_us, duration_sec, duration_ms, and bt_duration_us.
     */
    ldft_strftime,

    /**
     * Specify formatting for a duration type (duration_sec,
     * duration_ms, or duration_us) using a set of flags to be passed
     * to our own custom duration rendering function.  This allows you
     * to get around two shortcomings of rendering durations with
     * strftime: it always left-pads everything with zeros out to a
     * constant width, and it can't leave out components that are zero
     * if you want.  e.g. strftime() would only product "00h 50m 07s",
     * while this could product "50m 7s".
     */
    ldft_duration,

    /**
     * Provide a callback function that will render the value for us.
     * This provides ultimate flexibility for the discriminating
     * caller.
     */ 
    ldft_custom,

} lg_data_format_type;


/* ------------------------------------------------------------------------- */
/** Flags modifying how to render a duration value.  These all map onto
 * flags to be passed to lt_dur_ms_to_counter_str_ex().
 */
typedef enum {
    ldff_none = 0,
    ldff_fixed  =      1 << 0,
    ldff_long_words =  1 << 1,
} lg_duration_format_flags;

/**
 * Callback to render an attribute into a string, to be drawn as a
 * data label on a graph.
 *
 * \param attrib The data value to be rendered
 *
 * \param data The ::ldf_callback_data field from the ::lg_data_format
 * structure where this callback was specified.
 *
 * \retval ret_str The rendering of the data, in string form.  Note
 * that it may contain '\n' characters to make the rendering on the
 * graph span multiple lines.
 */
typedef int (*lg_data_render_callback)(const bn_attrib *attrib,
                                       void *data, tstring **ret_str);


/* ------------------------------------------------------------------------- */
/** Options to specify how to format data values.
 */
typedef struct lg_data_format {
    /**
     * What method are we going to use to specify the data format?
     * The option selected here controls which of the other fields
     * in this structure are heeded.  Defaults to ldft_default.
     */
    lg_data_format_type ldf_type;

    /**
     * Printf format string to use to format the data value.  Heeded
     * only if ldf_type is ldft_printf.  Supported only for floating
     * point values; i.e. if the type along this axis is float32 or
     * float64.
     *
     * The format strings considered legal here are very restricted.
     * The string must begin with '%' and end with the "conversion
     * specifier" corresponding to it.  The conversion specified must
     * be 'f', 'e', or 'g'.  Do not use any length modifiers like 'l'
     * to specify the correct data size: the library will fix this for
     * you.  Otherwise, you may use any characters you want in between
     * the '%' and the conversion specifier: e.g. to specify field
     * width, precision, and other flags.
     */
    tstring *ldf_printf_format;

    /**
     * Format string to be passed to strftime() to render the data
     * value.  Heeded only if ldf_type is ldft_strftime.  The library
     * will convert the value being rendered into a struct tm, and
     * then pass this along with your format string to strftime(), to
     * generate the final string to be displayed.
     */
    tstring *ldf_strftime_format;

    /**
     * Bit field of ::lg_duration_format_flags specifying how to render 
     * a duration value.  Heeded only if ldf_type is ldft_duration.
     */
    uint32 ldf_duration_format_flags;

    /**
     * Callback function to be called to render data values.
     * Heeded only if ldf_type is ldft_custom.
     */
    lg_data_render_callback ldf_callback;

    /**
     * Data to be passed to ldf_callback whenever we call it.
     */
    void *ldf_callback_data;

} lg_data_format;


/* ------------------------------------------------------------------------- */
/** Options to lg_axis_set_auto().
 *
 * NOTE: instead of specifying the maximum number of each kind of
 * indicator, we would have liked to specify the minimum distance
 * between these indicators in pixels, since that would allow us to
 * choose more reasonable defaults that were independent of the
 * image dimensions specified.  But this is difficult to do in the 
 * general case due to a circular dependency:
 *   1. Spacing indicators by number of pixels requires knowing the
 *      total number of pixels allocated for the length of this axis.
 *   2. Knowing that requires calculating the full layout of the
 *      image, including the "thickness" of the opposite axis (i.e. the
 *      size in the dimension perpendicular to its length).
 *   3. Calculating the thickness of the opposite axis requires 
 *      knowing the range and spacing of its value labels, so we can
 *      do a "dry run" of rendering that axis, to see how many 
 *      pixels are required to draw the value labels determined by
 *      the range and spacing.
 *   4. So the spacing of each axis depends on the spacing of the 
 *      other axis.
 *
 * In most cases, you could probably guess about the thickness of the
 * other axis by doing a pre-"dry run" that just renders the max and 
 * min (with nothing in the middle, since we don't know the spacing
 * yet), and takes the max size of those two.  The size is unlikely to
 * be far off that in most cases.  But you never know: e.g. if the
 * axis is durations, and it's ranging from zero to 9 hours, and we
 * have not selected ldff_fixed, then the endpoints would be "0s"
 * and "9h", but depending on what spacing is chosen, the longest item
 * in the middle might be either "5h" (1-hour spacing), or "5h 57m 30s"
 * (30-second spacing).
 *
 * So instead, we toss the burden of guessing to the caller.
 * We really only need the maximum number of indicators anyway
 * (we were just going to calculate that based on the minimum pixel
 * distance and the graph dimensions), and that's what they have to pass.
 * Our defaults will be reasonable for graphs of a certain size, with
 * a certain size font for value labels; but anyone deviating far from
 * these may need to increase or decrease them to prevent the ticks and
 * labels from being too sparse or too crowded.
 */
typedef struct lg_axis_auto_options {
    /** 
     * Bit field of ::lg_axis_auto_flags.
     */
    uint32 laao_flags;

    /**
     * Array of multiples of powers of 10 to to use as "round" numbers
     * while choosing spacing of axis indicators.  This list must 
     * (a) be terminated with a zero.
     * (b) contain only numbers in [1..10), except for the terminator;
     * (c) be in descending order;
     * You can use one of the standard lists defined by the library
     * (::lg_axis_auto_mult_1_25_5 et al.), or specify your own.
     * The default is ::lg_axis_auto_mult_1_25_5.
     *
     * The library guarantees that:
     *
     *   - Each indicator spacing is a multiple of the next
     *     closer-spaced one.  e.g. if the mults are {5, 2, 1}, and 2
     *     gets chosen for minor ticks, we will not choose 5 (with the
     *     same exponent) for major ticks.
     *
     *   - Only integer spacings are chosen for integer data types.
     *     e.g. if we're using {5, 2.5, 1}, we skip 2.5 when the
     *     exponent is zero.
     *
     * NOTE: if the laaf_no_pow10 flag is used, the only power of 10
     * we multiply this by is 10^0.
     */
    const float64 *laao_mults;

    /**
     * Maximum allowable number of minor ticks on this axis.  We choose
     * the smallest spacing that does not violate this constraint, and
     * which is also a "round" number.  Specify zero to not have any
     * minor ticks.
     */
    uint32 laao_max_minor_ticks;

    /**
     * Maximum allowable number of major ticks on this axis.
     * Specify zero to not have any major ticks.
     */
    uint32 laao_max_major_ticks;

    /**
     * Maximum allowable number of value labels on this axis.
     * Specify zero to not have any value labels.
     */
    uint32 laao_max_value_labels;

    /**
     * If non-NULL, forces the lga_range_low to be this value, rather
     * than being automatically determined by examining the data.
     */
    bn_attrib *laao_range_low;

    /**
     * If non-NULL, force the lga_range_high to be this value, rather
     * than being automatically determined by examining the data.
     */
    bn_attrib *laao_range_high;

    /**
     * By default, we round the min and max of an axis to the nearest
     * multiple of the value label spacing.  Use this field to specify
     * the range rounding granularity explicitly.
     *
     * This setting has no effect if the laaf_no_round_endpoints flag is set.
     *
     * If the rounding level cannot be honored, for example if rounding
     * to minor ticks is selected, but laao_max_minor_ticks is set to 0,
     * then the rounding target will be changed to the next higher level
     * of granularity available, e.g. major ticks, then value labels.
     *
     * Note that while the default is currently value lable rounding
     * (for compatibility with historical behavior) this is subject to
     * change in the future, so all new code should be written with an
     * explicit choice of rounding level.  If you want to guarantee
     * the current default behavior, set this field to lgait_value_labels.
     */

    lg_axis_indicator_type laao_endpoint_rounding_level;

} lg_axis_auto_options;


/* ------------------------------------------------------------------------- */
/** Options for a graph axis (line and bar graphs only).
 */
typedef struct lg_axis {
    /**
     * Text of a label describing the axis, e.g. the nature of the
     * data being plotted and optionally the units it is expressed in.
     * This will be rendered vertically for the Y axis, and
     * horizontally for the X axis.
     */
    tstring *lga_label;

    /**
     * How should we orient the text of the value labels along this
     * axis?  Note that this does not affect the overall label for the
     * axis.
     */ 
    lg_text_orientation lga_value_label_orientation;

    /**
     * Array of "special lines" to extend from this axis across the
     * graph.  May not be specified with the discrete axis of a bar
     * graph.
     */
    lg_special_line_array *lga_special_lines;

    /* ..................................................................... */
    /** Value formatting options
     *
     * This field specifies how to format values along this axis.
     * This apples to the labels along the axis, as well as any other
     * places the values are rendered, such as actual values above bar
     * graphs.  This structure comes preallocated, so you only need
     * to fill out the values in it.
     */
    lg_data_format *lga_data_format;

    /**
     * Override value formatting options just for the axis itself.
     * This structure comes pre-allocated, but with ldf_type set to
     * ldft_none, meaning to ignore this structure and use
     * lga_data_format for all formatting on this axis.  If the
     * type is set to something else, these formatting options are
     * used for data value labels on the axis, while lga_data_format
     * is used in other places where values from this axis are
     * rendered, such as labels above bars in a bar graph.
     */
    lg_data_format *lga_data_format_axis;

    /* ..................................................................... */
    /** @name Value range and labels
     *
     * These values specify the range of values for this axis, and how
     * we should space the minor ticks, major ticks, and value labels.
     * These will be ignored if the this is the discrete axis of a bar
     * graph, since they will be derived from the data set being
     * plotted.
     *
     * If they are specified, they must all have the same data type,
     * and that data type must be numeric.
     */
    /*@{*/

    /**
     * Low end of the range of data values for this axis.  This will
     * be the bottom for the Y axis, or the left for the X axis.
     */
    bn_attrib *lga_range_low;

     /**
     * High end of the range of data values for this axis.  This will
     * be the top for the Y axis, or the right for the X axis.
     */
    bn_attrib *lga_range_high;

    /**
     * Settings for minor ticks on this axis.
     */
    lg_axis_indicator *lga_minor_ticks;

    /**
     * Settings for major ticks on this axis.
     */
    lg_axis_indicator *lga_major_ticks;

    /**
     * Settings for value labels on this axis.
     */
    lg_axis_indicator *lga_value_labels;

    /**
     * Specifies how the indicators should be aligned relative to their
     * spacing, i.e. where we should start the indicators.  Defaults to
     * laia_origin.
     */
    lg_axis_indicator_alignment lga_align;

    /**
     * Options to be used when choosing the range and indicator
     * spacing automatically.  This is only heeded when
     * lg_axis_set_auto() is called.
     */
    lg_axis_auto_options *lga_auto_opts;

    /*@}*/

} lg_axis;


/* ------------------------------------------------------------------------- */
/** One series of data to be graphed.  Applicable to line and bar graphs.
 */
typedef struct lg_data_series {
    /**
     * Color to use to plot this series.  Note: this MAY NOT be 
     * lg_color_transparent or lg_color_choose.
     */
    lg_color lds_color;

    /**
     * Descriptive name for this series, for use in the legend.
     * If none is provided, one like "Series 1" will be automatically
     * generated.
     */
    tstring *lds_legend_name;

    /**
     * Opaque tag for this data series.  This is used in conjunction
     * with the lgf_compute_regions flag on the graph, to help you
     * correlate each polygonal region we return with a data series.
     *
     * If you want to use lg_image_map_render() to create a full 
     * HTML image map ready to include into a document, create a 
     * lg_image_map_context object and assign a pointer to it to this
     * field.  However, we have made it a 'void *' for maximum 
     * flexibility, in case you want to use your own structure and
     * render it in your own manner.
     *
     * NOTE: the graph structure does not take ownership of the tag
     * you pass it, so if your tag involved any dynamic allocation
     * (e.g. lg_image_map_context_new()), you must keep track of all
     * the tags you create and assign, and free them when you're all
     * done with graphing.
     */
    void *lds_series_tag;

    /**
     * X coordinates of all data points.  Note that this array is
     * parallel to lds_y_values: the element at position x corresponds
     * the the element at the same position in the other array, and
     * together they form a single data point.
     *
     * The data types of all attributes in this array must match each
     * other.
     */
    bn_attrib_array *lds_x_values;
    
    /**
     * Y coordinates of all data points.  Note that this array is
     * parallel to lds_x_values.
     *
     * The data types of all attributes in this array must match each
     * other.
     */
    bn_attrib_array *lds_y_values;

    /**
     * (Bar graphs only) Tags for each individual data point, for use
     * with lgf_compute_regions when a bar graph is being rendered.
     * Adding tags here is optional.  If you do not add tags here, or
     * if a given data point lacks a tag, the tag for the entire
     * series will be used instead.  But any data point in a bar graph
     * that does have an entry here will use this tag instead.
     *
     * Note: this array is created with no free function registered.
     * If you add dynamically-allocated items to this array, you
     * should keep track of them and free them yourself after all
     * the dust has settled.
     */
    array *lds_bar_tags;
    
} lg_data_series;

TYPED_ARRAY_HEADER_NEW_NONE(lg_data_series, lg_data_series *);


/* ------------------------------------------------------------------------- */
/** Flags that can be specified on a data series group.
 *
 * Note: we refer to the "base" axis and the "stackable" axis below.
 * Normally, the "base" axis means the X axis and the "stackable" axis
 * means the Y axis; but if ::lgf_reverse_axes is specified, the
 * meanings are revered.
 */
typedef enum {

    /**
     * Interpolate data: insert new values into the data sets as
     * necessary, such that for any given series, within the bounds of
     * its min and max value on the base axis (X axis by default), it
     * has a data point for every value on that axis found in any
     * other series in this data series group.  Do this by
     * interpolation between the surrounding concrete points.
     *
     * This option also implies "external padding" (inserting NULL
     * values before the beginning and/or after the end of each
     * series) to align all the series, such that across all series,
     * all values at any given index share the same value on the base
     * axis.
     *
     * For example, if you had two series:
     *   {10, 200}  {20, 600}  {30, 700}
     *   {15,  50}  {25, 100}
     *
     * and the X axis was the base axis, the result of interpolation
     * and external padding would be:
     *   {10, 200}  {15, 400}  {20, 600}  {25, 650}  {30, 700}
     *     NULL     {15,  50}  {20,  75}  {25, 100}    NULL
     *
     * This is mainly intended for line graphs or area graphs.
     * With bar graphs, there is somewhat more of an expectation
     * that each bar represents a real data point rather than an
     * interpolated one, so ::ldgf_align may be more appropriate.
     */
    ldgf_interpolate = 1 << 0,

    /**
     * Align data by insertion.  Insert NULLs into the data sets as
     * necessary, such that across all series, all values at any given
     * index either share the same value on the base axis, or are
     * NULL.  This is like ::ldgf_interpolate, except that wherever
     * that flag would have inserted an interpolated value, we insert
     * a NULL.
     *
     * For example, if you had two series:
     *   {10, 200}  {20, 600}  {30, 700}
     *   {15,  50}  {25, 100}
     *
     * and the X axis was the base axis, the result of alignment would be:
     *   {10, 200}    NULL     {20, 600}    NULL     {30, 700}
     *     NULL     {15,  50}    NULL     {25, 100}    NULL
     *
     * This is mainly intended for bar graphs.  With line and area
     * graphs, better results may be produced with ::ldgf_interpolate.
     *
     * (CAUTION: this flag is NOT fully tested, and won't really help
     * in the general case since we still require that the first 
     * bar graph series mention all discrete axis coordinates.)
     */
    ldgf_align_insert = 1 << 1,

    /*
     * Snap data to nearest valid value.  Determine a set of valid
     * coordinates on the base axis (X axis by default) for points on
     * this graph, based on the minimum base axis coordinate found in
     * the raw dataset, and the ldsg_snap_distance field in the
     * lg_data_series_group structure.  Then ensure that for every
     * valid value in the range spanned by all data series in the
     * group, we have a point in every series at that point, by (a)
     * snapping nearby values to match, or (b) inserting a NULL, as
     * necessary.
     *
     * For example, if you had two series:
     *   {10, 200}  {21, 600}   {49, 700}
     *   {11, 50}   {19, 140}   {41, 773}
     *
     * and the snap distance was set to 10, then the result of the
     * alignment would be:
     *
     *   {10, 200}  {20, 600}   NULL        NULL     {50, 700}
     *   {10, 50}   {20, 140}   NULL     {40, 773}      NULL
     *
     * Note that when used with an area graph (a line graph with
     * ldgf_stack set), this produces a different style of drawing
     * than ldgf_interpolate.  Interpolation is different in that it
     * produces a dataset with no gaps, because it does not have the
     * expected distance between data points and is not able to detect
     * gaps.  Interpolation always connects two adjacent data points
     * in a series with a straight line...
     *
     * When the data is rendered using ldgf_align_snap however, we do
     * what you might call a "packed bar graph".  It's packed because
     * there is no space between bars: each bar takes up the entire
     * width of the slot assigned to it.  But the base axis is still
     * labeled like an area graph, not like a bar graph.  Note that 
     * this will look pretty blocky, since every data point will be
     * represented by a rectangular bar.  Use ::lgf_smart_sloping to
     * smooth out the edges as much as possible.
     */
    ldgf_align_snap = 1 << 2,

    /**
     * Treat data as relative.  Each data point's coordinate on the
     * stackable axis (Y axis by default) is relative to the stackable
     * axis coordinate of the corresponding point in the series
     * preceding.
     *
     * NOTE: "Corresponding" means the one with the same index, but in
     * order for this to work that must also mean the one with the
     * same value on the base axis.  This can be ensured by using
     * either ::ldgf_interpolate or ::ldgf_align.  Or if you are
     * certain your data set already meets these requirements, you can
     * skip those flags for better performance.
     */
    ldgf_relative = 1 << 3,

    /**
     * Negate data.  The Y coordinates of all data must be unsigned
     * integers, and will be negated before being plotted.
     */
    ldgf_negative = 1 << 4,

    /**
     * Stack data.  This has a somewhat different meaning depending on 
     * the type of graph you are drawing.
     *
     * In the case of line graphs, this means to draw an area graph,
     * filling the area "beneath" each series, between it and whatever
     * series is below it.  (This would generally be the series 
     * preceding it, but could be an even earlier one if the series
     * immediately preceding it did not have values in this range.)
     * Here "beneath" means between the series values and the base
     * axis (because in the default case where the base axis is the
     * X axis it really is drawn beneath).
     *
     * In the case of bar graphs, this means to draw a stacked bar
     * graph, putting each data point as a bar perched atop the
     * corresponding point for previous series, rather than beside
     * them.
     * 
     * This flag alone does not imply ::ldgf_relative, though it will
     * commonly be used in conjunction with that flag.
     *
     * NOTE: in order to use this, your data must be aligned such that
     * at any given index, all points have the same value on the base
     * axis.  This can be ensured by using either ::ldgf_interpolate
     * or ::ldgf_align.  Or if you are certain your data set already
     * meets these requirements, you can skip those flags for better
     * performance.
     */
    ldgf_stack = 1 << 5,

    ldgf_area_interpolated = ldgf_interpolate | ldgf_relative | ldgf_stack,

    ldgf_area_snapped = ldgf_align_snap | ldgf_relative | ldgf_stack,

} lg_data_series_group_flags;

/** Bit field of ::lg_data_series_group_flags ORed together */
typedef uint32 lg_data_series_group_flags_bf;


/* ------------------------------------------------------------------------- */
typedef struct lg_data_series_group {
    /**
     * The actual data in this data series group.
     */
    lg_data_series_array *ldsg_data;

    /**
     * Bit field of ::lg_data_series_group_flags ORed together, electing 
     * options for this data series group.
     */
    lg_data_series_group_flags_bf ldsg_flags;

    /**
     * Heeded only if the ldgf_align_snap flag is specified in
     * ldsg_flags.  This specifies the distance between valid X
     * coordinates for points to be graphed.  The first valid X
     * coordinate is considered to be the minimum encountered across
     * all of the data, and the subsequent ones are spaced from there
     * at intervals of ldsg_snap_distance.
     */
    bn_attrib *ldsg_snap_distance;

    /** This field is for internal use only -- do not use */
    void *ldsg_opaque;

} lg_data_series_group;

TYPED_ARRAY_HEADER_NEW_NONE(lg_data_series_group, lg_data_series_group *);


/* ------------------------------------------------------------------------- */
/** Options for any graph based on a set of data series: line or bar
 * graphs.
 */
typedef struct lg_series_graph {
    /**
     * Options for the X axis.
     */
    lg_axis *lsg_x_axis;

    /**
     * Options for the Y axis.
     */
    lg_axis *lsg_y_axis;

    /**
     * Thickness of any special lines we draw on this graph.
     */
    uint32 lsg_special_line_thickness;

    /**
     * Series groups to be plotted.  Call lg_data_series_group_new()
     * to create one, and lg_data_series_group_array_append_takeover()
     * to add it to this array when you are done adding data series
     * to it.
     */
    lg_data_series_group_array *lsg_series_groups;

    /**
     * Data to be plotted normally.  This is just like the data series
     * array you'd get by creating a series group with no flags.  It
     * is redundant with lsg_series_groups, but is maintained for
     * backward compatibility, as well as for convenience in common
     * cases.
     */
    lg_data_series_array *lsg_data;

    /**
     * Data to be plotted "stacked".  Like lsg_data, this is redundant
     * with lsg_series_groups, but is maintained for backward
     * compatibility and convenience.  The flags it implies depend on
     * a broader context:
     *
     *   - If you are drawing an area graph and have not specified
     *     ::lgf_stacked_abs on the graph, it implies
     *     (::ldgf_interpolate | ::ldgf_relative | ::ldgf_stack)
     *
     *   - If you are drawing an area graph have have specified
     *     ::lgf_stacked_abs on the graph, it implies
     *     (::ldgf_interpolate | ::ldgf_stack)
     *
     *   - If you are drawing a bar graph, it implies
     *     (::ldgf_align | ::ldgf_stack)
     */
    lg_data_series_array *lsg_data_stacked;

} lg_series_graph;


/* ------------------------------------------------------------------------- */
/** Context structure for image maps.  This is an optional structure,
 * whose intended use is to have a pointer to one instance of it
 * assigned to the tag of each pie slice (lps_tag field) or data
 * series (lds_series_tag or lds_bar_tags field) in your graph.  When
 * you get the lg_graph_result back from rendering the graph, then
 * call lg_image_map_render() on lgr_regions to convert it into a full
 * string of HTML that you can insert into your document.
 */
typedef struct lg_image_map_context {
    /**
     * URL to which this portion of the graph image should link.
     * This is emitted as the 'href' attribute in the 'area' tag
     * in the image map.
     */
    tstring *limc_href;

    /**
     * Emitted as the 'alt' attribute in the 'area' tag
     * in the image map.
     *
     * (XXX/EMT: explain what this is for)
     */
    tstring *limc_alt;

    /**
     * Emitted as the 'title' attribute in the 'area' tag
     * in the image map. 
     *
     * (XXX/EMT: explain what this is for)
     */
    tstring *limc_title;

} lg_image_map_context;


/* ------------------------------------------------------------------------- */
/** One data point (slice) for a pie chart.
 */
typedef struct lg_pie_slice {
    /**
     * Color to use for this pie slice.  Note: this MAY NOT be 
     * lg_color_transparent or lg_color_choose.
     */
    lg_color lps_color;

    /**
     * Descriptive name for this slice in the legend.
     * If none is provided, one like "Slice 1" will be automatically
     * generated.
     */
    tstring *lps_legend_name;

    /**
     * Opaque tag for this slice.  This is used in conjunction with
     * the lgf_compute_regions flag on the graph, to help you
     * correlate each polygonal region we return with a pie slice.
     *
     * If you want to use lg_image_map_render() to create a full 
     * HTML image map ready to include into a document, create a 
     * lg_image_map_context object and assign a pointer to it to this
     * field.  However, we have made it a 'void *' for maximum 
     * flexibility, in case you want to use your own structure and
     * render it in your own manner.
     *
     * NOTE: the graph structure does not take ownership of the tag
     * you pass it, so if your tag involved any dynamic allocation
     * (e.g. lg_image_map_context_new()), you must keep track of all
     * the tags you create and assign, and free them when you're all
     * done with graphing.
     */
    void *lps_tag;

    /**
     * Size of this slice.  This must be the same data type as all
     * other slices in this pie, and the data type must be numeric.
     * They need not all add up to anything in particular; their sum
     * will be computed, and each one's percentage will be computed
     * relative to that sum.
     */
    bn_attrib *lps_size;

} lg_pie_slice;


TYPED_ARRAY_HEADER_NEW_NONE(lg_pie_slice, lg_pie_slice *);


/* ------------------------------------------------------------------------- */
/** Options for a line graph.  Data we have in common with bar graphs
 * is held separately in an lg_series_graph structure.
 */
typedef struct lg_line_graph {
    /**
     * Thickness of each series line we draw, in pixels.
     */
    uint32 llg_line_thickness;

} lg_line_graph;


/* ------------------------------------------------------------------------- */
/** Options for a bar graph.  Data we have in common with line graphs
 * is held separately in an lg_series_graph structure.
 */
typedef struct lg_bar_graph {
#if 0
    /**
     * Width of each bar we draw, in pixels.  Defaults to -1, meaning
     * to automatically choose based on width of image, number of
     * series, number of data points, etc.
     *
     * NOT YET IMPLEMENTED.
     */
    int32 lbg_bar_width;
#endif

    /* NOTE: Remove this when at least one member is added to this struct */
    int lbg_DUMMY;

} lg_bar_graph;


/* ------------------------------------------------------------------------- */
/** Options pertaining to a group of labels to go on a pie chart.
 *
 * NOTE: though the current functionality could be achieved with a
 * uint32_array whose indices corresponded with the lg_pie_label_type,
 * we structure the data this way to give more flexibility for future
 * expansion.  e.g. each label could be in a different font, color, etc.
 */
typedef struct lg_pie_label_options {
    /**
     * Specifies what type of label these will be.
     */
    lg_pie_label_type lplo_label_type;

    /**
     * Bit field of ::lg_pie_value_label_pos indicating where to place
     * these labels.  Any number of locations may be specified.
     */
    uint32 lplo_positions;

} lg_pie_label_options;


/* ------------------------------------------------------------------------- */
/** Options for a pie chart, including the data to be graphed.
 */
typedef struct lg_pie_chart {
    /**
     * Slices to be included in this pie chart.
     */
    lg_pie_slice_array *lpc_slices;
    
    /**
     * Specifies the minimum percentage of the pie a given data value
     * must have if it is to be labeled inside or outside the pie
     * chart.  This should be in [0..100], not [0..1].  This applies
     * to both absolute value and percentage labels.  A segment will
     * be labeled according to those flags if and only if the
     * percentage of the pie it takes up is greater than or equal to
     * this value.
     *
     * Note that this does not apply to labels for the legend; all
     * entries in the legend are labeled with their corresponding
     * values as specified by lpc_abs_val_label and lpc_pct_val_label.
     */
    float32 lpc_label_pct_threshold;

    /** 
     * This field specifies how to format data values that represent
     * the absolute value of pie slices.  This will be used anywhere
     * we render these values: inside the pie, outside the pie, or 
     * in the legend.
     */
    lg_data_format *lpc_data_format;

    /**
     * Settings for main labels to go with each pie slice that specify
     * the sizes of the slice.  Each pie chart may have up to two sets
     * of labels.  This is for the main one.
     */
    lg_pie_label_options *lpc_labels_main;

    /**
     * Settings for secondary labels to go with each pie slice that
     * specify the sizes of the slice.  If this is specified,
     * lpc_labels_main must also be specified.  If both are specified
     * and occur in the same place, the main one will come first, and
     * the secondary one will follow in parentheses.
     *
     * (Note: we currently only support secondary labels in the
     * legend, not in the graph itself.)
     */
    lg_pie_label_options *lpc_labels_secondary;

    /**
     * If the pie is 3D (lgf_3d is set), this is the number of pixels
     * "high" the pie chart will be.  Generally, the larger the pie,
     * the larger this should be, to look most realistic.
     */
    uint32 lpc_3d_dropdown_height;

} lg_pie_chart;


/* ------------------------------------------------------------------------- */
/** Flags that can be set on a graph's legend.
 *
 * Note that some of these are mutually exclusive, such as
 * llf_align_left and llf_align_right.  Just use common sense.
 */
typedef enum {
    /**
     * Omit the legend entirely.
     */
    llf_omit = 1 << 0,

    /**
     * Draw a background under the legend.
     */
    llf_background = 1 << 1,

    /**
     * Draw a border around the legend.
     */
    llf_border = 1 << 2,

    /**
     * Do not include "special lines" in the legend.
     */
    llf_no_special_lines = 1 << 3,

    /**
     * Align the legend along the lefthand side of the area allotted
     * to it.  Applies only if the legend is along the bottom part of
     * the screen.
     */
    llf_align_left = 1 << 4,

    /**
     * Align the legend along the righthand side of the area allotted
     * to it.  Applies only if the legend is along the bottom part of
     * the screen.
     */
    llf_align_right = 1 << 5,

    /**
     * Align the legend along the top edge of the area allotted to it.
     * Applies only if the legend is along the righthand side of the
     * screen.
     */
    llf_align_top = 1 << 6,

    /**
     * Align the legend along the bottom edge of the area allotted to
     * it.  Applies only if the legend is along the righthand side of
     * the screen.
     */
    llf_align_bottom = 1 << 7,

    /**
     * Align the legend entries in columns of equal width.  In the
     * lg_legend structure, you can specify the number of columns to
     * use with the ll_num_columns field.
     *
     * If this is not specified, we'll plot legend entries in rows
     * where each item takes up exactly as much space as it needs.
     */
    llf_use_columns = 1 << 8,

    /**
     * Request that columns not be overlapped.  Only relevant if
     * llf_use_columns is set.
     *
     * By default, the column width chosen is the min of (a) the
     * widest column, and (b) the maximum column width given the space
     * available.  If the maximum column width is the smaller of the
     * two, that means the columns will overlap, and thus some of the
     * legend text may overlap with the color square of the entry on
     * the right.
     *
     * If the columns overlap, and this flag is set, columns will be
     * automatically disabled, i.e. it will be as if llf_use_columns
     * was not set.  This will cause the legend to fit in as small a 
     * space is possible, smaller than would be possible using columns.
     * Note that if the columns would not have overlapped, this flag
     * has no effect.
     */
    llf_no_column_overlap = 1 << 9,

} lg_legend_flags;


/* ------------------------------------------------------------------------- */
/** Specify where the legend should be placed relative to the graph
 * itself, in the overall layout of the image being rendered.
 */
typedef enum {
    lll_none = 0,

    /**
     * Place the legend to the right of the graph.
     * This is the default for pie charts, and is not currently
     * available for any other graph type.
     */
    lll_right,

    /**
     * Place the legend below the graph.
     * This is the default for series graphs (line and bar graphs),
     * but pie charts can be changed to use this layout.
     */
    lll_bottom,

} lg_legend_location;

/* ------------------------------------------------------------------------- */
/** Options for the legend on a graph.
 */
typedef struct lg_legend {
    /**
     * Bit field of ::lg_legend_flags.
     */
    uint32 ll_flags;

    /**
     * Where to place the legend in the graph.
     * Defaults to lll_right for pie charts, lll_bottom for others.
     */
    lg_legend_location ll_location;

    /**
     * Number of columns to use to draw legend.  Applies only if the
     * llf_use_columns flag is set in ll_flags.  Specify 0 to
     * automatically choose the appropriate number of columns based on
     * what will comfortably fit.
     *
     * Defaults to 1 for pie charts (since the legend is generally
     * given a tall, thin space to fill); and 0 for all other graphs
     * (since the legend is generally given a short, wide space to fill).
     *
     * Note that all columns will have the same width, that being
     * the width necessary to accomodate the widest item in the
     * whole legend.
     */
    uint16 ll_num_columns;

} lg_legend;


/* ------------------------------------------------------------------------- */
/* Information about fonts to use in the graph.
 *
 * We default to using the built-in fonts, for minimum dependencies
 * and maximum speed.  The caller can override and use a TrueType
 * font.
 */
typedef struct lg_font_info {
    /** 
     * Filename of TrueType font to use, or NULL to use built-in fonts.
     * The font must be found in the FONTS_PATH directory (see tpaths.h)
     *
     * See above for definitions of fonts we know about (starting with
     * "lg_font_..."), but you can names of your own fonts if you
     * prefer.
     */
    const char *lfi_font;

    /**
     * Text size to use for all text in this graph.  Only applicable
     * if a TrueType font is supplied in lgg_font; otherwise, all
     * text will use the "small" built-in libgd font.
     */
    double lfi_size;

    /**
     * Specifies whether the font size for the title of the graph is 
     * relative to lfi_size (false), or absolute (true).
     */
    tbool lfi_title_size_abs;

    /**
     * Font size for title of graph.  This can be either relative to
     * lfi_size, or absolute, depending on the value of
     * lfi_title_size_abs.
     */
    double lfi_title_size;

} lg_font_info;


/* ------------------------------------------------------------------------- */
/** Overall structure specifying a graph to be rendered.
 */
typedef struct lg_graph {
    /** Label to print at the top of the entire graph */
    tstring *lgg_title;

    /** Bit field of ::lg_graph_flags */
    uint32 lgg_flags;

    /** Which type of graph are we plotting? */
    lg_graph_type lgg_graph_type;

    /** Settings for a line graph, only heeded if lgg_graph_type is lgt_line */
    lg_line_graph *lgg_line_graph;

    /** Settings for a bar graph, only heeded if lgg_graph_type is lgt_bar */
    lg_bar_graph *lgg_bar_graph;

    /**
     * Data for a line or bar graph, only heeded if lgg_graph_type is
     * lgt_line or lgt_bar.
     */
    lg_series_graph *lgg_series_graph;

    /**
     * Settings and data for a pie chart, only heeded if
     * lgg_graph_type is lgt_pie
     */
    lg_pie_chart *lgg_pie_chart;

    /** Settings for the graph's legend */
    lg_legend *lgg_legend;

    /** Fonts and sizes to use in graph */
    lg_font_info *lgg_font_info;

    /* ..................................................................... */
    /** @name Image dimensions
     *
     * You can control the overall size of the image file generated,
     * and how much empty space is left on each side of it, presumably 
     * either for use as an actual margin, or for the caller to draw
     * into later.
     *
     * The image size minus the margin is the area we will draw into.
     * This includes the graph area, the axes, the axis labels and
     * overall graph labels, and the legend.  There is currently no
     * way to control the size of the graphing region separately.
     */
    /*@{*/

    /** Width of entire image to generate, in pixels */
    uint32 lgg_width;

    /** Height of entire image to generate, in pixels */
    uint32 lgg_height;

    /** Left margin to leave blank in image */
    uint32 lgg_left_margin;

    /**
     * Right margin to leave blank in image.
     *
     * (NOTE: the graph and X axis will end here, but the rightmost
     * value label on the X axis will be centered on this edge, so
     * half of it will spill into the margin.)
     */
    uint32 lgg_right_margin;

    /** Top margin to leave blank in image */
    uint32 lgg_top_margin;

    /** Bottom margin to leave blank in image */
    uint32 lgg_bottom_margin;

    /** 
     * Adjustment to padding between legend and rest of graph.
     * Defaults to zero, which is default padding.  A negative number
     * will make whatever borders the legend encroach into its space,
     * though they won't immediately overlap because there is some
     * default padding.
     */
    int32 lgg_legend_pad_delta;

    /**
     * Amount of padding to use between different components of the
     * graph.  This is used in a lot of different places to prevent
     * different components from being too close together.
     */
    int32 lgg_inside_padding;

    /*@}*/

    /** 
     * Color of margin around image.  Defaults to lg_color_transparent;
     * lg_color_choose is not legal.
     */
    lg_color lgg_margin_color;
    
    /** 
     * Background color of non-margin portion of image.  Defaults to
     * lg_color_transparent; lg_color_choose is not legal.
     */
    lg_color lgg_graph_bgcolor;

    /**
     * Background color of actual graph plotting area.  This is the
     * area encompassed by lgg_graph_bgcolor, minus the area used for
     * axis labels, title, and legend.  Defaults to lg_color_transparent;
     * lg_color_choose is not legal.
     */
    lg_color lgg_plotting_area_bgcolor;

    /** This field is for internal use only -- do not use */
    void *lgg_opaque;

} lg_graph;


/* ------------------------------------------------------------------------- */
/** One element in a set of regions to be returned from a graphing
 * operation.  If the lgf_compute_regions flag is specified, one of
 * these will be produced per "area" in the graph, where the nature of
 * the area is determined by the graph type.  In the case of a pie
 * chart, an "area" is one pie slice; in a bar chart, an "area" is
 * one bar or bar segment.  One region is also defined for every 
 * legend entry corresponding to a data element (but not for those
 * representing special lines).
 */
typedef struct lg_region {
    /**
     * The tag provided by the caller on the area to which this region
     * pertains.  Note that when the region structure is freed, the
     * library will not attempt to free this item, so you must retain
     * ownership of it and free it when it is no longer being
     * referenced by anything.
     */
    void *lr_tag;

    /**
     * Array of X coordinates of vertices surrounding the polygonal
     * region this structure describes.  This array is parallel to
     * lr_y_coords in that the numbers at a given index in both
     * together describe a point.
     */
    uint32_array *lr_x_coords;

    /**
     * Array of Y coordinates of vertices surrounding the polygonal
     * region this structure describes.  This array is parallel to
     * lr_x_coords in that the numbers at a given index in both
     * together describe a point.
     */
    uint32_array *lr_y_coords;

} lg_region;

TYPED_ARRAY_HEADER_NEW_NONE(lg_region, lg_region *);


/* ------------------------------------------------------------------------- */
/** Structure containing results of a graphing operation.  Different
 * fields in this structure may be populated or not during a graphing
 * operation, depending on the graph type, and on the options
 * specified by the caller.
 */
typedef struct lg_graph_result {
    /**
     * Region data representing the perimeter of a set of areas in
     * the graph.  Only populated if the lgf_compute_regions flag is
     * set on the graph.
     */
    lg_region_array *lgr_regions;

    /**
     * Indicates whether or not we had to recycle colors because we
     * were asked to automatically choose colors more times than we
     * had predetermined colors.
     */
    tbool lgr_recycled_colors;

} lg_graph_result;


/* ========================================================================= */
/* Functions
 */

/* ------------------------------------------------------------------------- */
/** Create a new graph structure.  This structure can be filled out and
 * then graphed with one of the lg_graph_draw_...() variants.  This
 * call allocates not only the main lg_graph structure, but all of the
 * container substructures inside it, except where otherwise noted.
 *
 * \param graph_type Type of graph to create.
 *
 * \retval ret_graph Returns the newly-created graph structure.
 */
int lg_graph_new(lg_graph_type graph_type, lg_graph **ret_graph);


/* ------------------------------------------------------------------------- */
/** Choose a color from a series of predetermined colors, and return
 * it to the caller.  The graph structure is used to keep track of
 * which colors have been automatically chosen so far, so we can
 * choose a unique one in the context of that graph.  Note that the
 * automatic choice of colors does NOT consider any manually chosen
 * colors of objects already in the graph, but rather blindly iterates
 * down the list of predetermined colors.
 *
 * NOTE: if we run out of unique predetermined colors to return, we
 * log a warning and recycle them from the beginning.  We do not
 * return an error, on the theory that it's better to provide a graph
 * with recycled colors than to risk erroring out if the callers don't
 * check return values.  Call lg_graph_recycled_colors() to find out 
 * if this recycling has occurred yet for a given graph.
 * 
 * \param graph Graph to use as context for choosing the color.
 * \retval ret_color Color value chosen.
 */
int lg_color_choose_auto(lg_graph *graph, lg_color *ret_color);


/* ------------------------------------------------------------------------- */
/** Tell if we have had to recycle predetermined colors in this graph
 * yet, because the caller has requested us to choose colors more
 * often than we had colors to offer.  This can be called before or
 * after plotting a graph, but its answer can also be found in the 
 * lg_graph_result structure.
 */
int lg_graph_recycled_colors(const lg_graph *graph, tbool *ret_recycled);


/* ------------------------------------------------------------------------- */
/** Create a new empty data series group with the specified flags, and
 * return it.  The caller must then add any data series desired to its
 * data series array, and add the group to the series graph structure.
 * For example:
 *
 *    lg_data_series_group *dsg = NULL;
 *    lg_data_series_array *dsa = NULL;
 *
 *    err = lg_data_series_group_new(flags, true, &dsg);
 *    bail_error(err);
 *    dsa = dsg->ldsg_data;
 *    err = lg_data_series_group_array_append_takeover
 *        (graph->lgg_series_graph->lsg_series_groups);
 *    bail_error(err);
 *    //
 *    // Now add data series to dsa, just as you would to lsg_data[_stacked].
 *    // You do not own dsa, so do not free it or add it anywhere when done.
 *    //
 *
 * \param flags A bit field of ::lg_data_series_group_flags ORed together.
 * \param create_dsa Should we create an empty data series array to go
 * in this group?  If not, you must provide your own.
 * \retval ret_dsg Returns the newly-created data series group, which you
 * must then add to the graph.
 */
int lg_data_series_group_new(lg_data_series_group_flags_bf flags,
                             tbool create_dsa,
                             lg_data_series_group **ret_dsg);


/* ------------------------------------------------------------------------- */
/** Create a new data series group, given a data series array, and add
 * it to the specified graph.  Ownership of the array provided is
 * taken over by the graph, and the caller's pointer NULLed out to
 * avoid confusion.  A pointer to the data series group is not
 * returned: it is assumed that when this is called, the data series
 * array is already complete.
 *
 * Note that this does not give you the flexibility to set other
 * fields in the data series group, such as ldsg_snap_distance.
 */
int lg_data_series_group_create_takeover(lg_graph *graph, 
                                         lg_data_series_group_flags_bf flags,
                                         lg_data_series_array **inout_dsa);


/* ------------------------------------------------------------------------- */
/** Fetch data for a graph, either from statsd, or from a set of nodes
 * that have the same structure as statsd's.  This will query the
 * nodes specified using the mdb_db.h API, which is appropriate for
 * calling from a mgmtd module.  It uses the resulting data to create
 * a new data series.  You can then add this series to your graph
 * structure as desired.
 *
 * If a subset of data is to be graphed, usually the constraints are
 * specified as either instance numbers or timestamps, though if both
 * types of constraints are specified, they will both be honored and
 * only instances meeting all constraints will be graphed.
 *
 * NOTE: if you are going to be plotting multiple data series together 
 * on the same graph, consider using lg_graph_fetch_stats_data_mult()
 * to fetch it all with a single call.  Its comment explains why.
 *
 * NOTE: if you are not generating the graph from inside mgmtd, fetch
 * the data yourself (probably using the md_client.h API), and call
 * lg_graph_translate_stats_data().  This is expected to be rare,
 * except if perhaps we start generating graphs from within statsd to
 * escape the GCL overhead, in which case the method of data fetching
 * is completely nonstandard anyway.
 *
 * \param graph The graph to which this series will be added.
 * Note that this function will not add the series to the graph for you.
 *
 * \param color The color that should be used to represent this series
 * in the graph, or lg_color_choose to allow it to be chosen
 * automatically.
 *
 * \param commit The commit structure to use for the context of this
 * query.  This imparts permissions, etc.  This does not represent a
 * choice for the caller; rather, the caller will have been passed an
 * md_commit pointer by the infrastructure, and should just pass it
 * on to us.
 *
 * \param db The database to query from.  As the the 'commit' parameter,
 * this does not represent a choice for the caller.
 *
 * \param node_name_root Name of root node of data series to fetch.
 * If you're actually fetching data from statsd, this will match either
 * "/stats/state/sample/ * /node/ *" or "/stats/state/chd/ * /node/ *".
 * But the only real requirement is that the data to be graphed all be
 * underneath this node in nodes named "instance/ * /value" and 
 * "instance/ * /name".  The value will be used as the Y coordinate,
 * and the time will be used as the X coordinate of each point.
 *
 * \param legend_name The string to assign to the lds_legend_name field
 * of the data series to be created.
 *
 * \param instance_min Minimum instance number to graph, or -1 for
 * no lower bound.
 *
 * \param instance_max Maximum instance number to graph, or -1 for
 * no upper bound.
 *
 * \param time_min Minimum timestamp of instances to graph, or -1
 * for no lower bound.
 *
 * \param time_max Maximum timestamp of instances to graph, or -1
 * for no upper bound.
 *
 * \param step Indicates the distance between each instance number to
 * be graphed.  -1, 0, and 1 all mean the same thing: graph every
 * instance in the range specified.  2 would mean skip every other
 * one, and so forth.
 *
 * \param divisor A number to divide all of the stats data by.  Of course,
 * you can effectively multiply it by something by passing 1/n here.
 *
 * \param flags A bit field of ::lg_stats_flags.
 *
 * \retval ret_data_series The newly-created data series.
 *
 * \retval ret_instance_min The instance number of the first data
 * point in the series, which is the lowest instance number since they
 * are sorted.  Note that if lsf_no_interpolate was specified, no
 * assumptions can be made about instance numbers of data points after
 * the first.
 *
 * \retval ret_instance_max The instance number of the last data point
 * in the series, which is also the highest instance number.
 */
int lg_graph_fetch_stats_data(lg_graph *graph, lg_color color,
                              md_commit *commit, const mdb_db *db,
                              const char *node_name_root, 
                              const char *legend_name,
                              int64 instance_min, int64 instance_max,
                              lt_time_sec time_min, lt_time_sec time_max,
                              int64 step, float64 divisor,
                              lg_stats_flags_bf flags,
                              lg_data_series **ret_data_series,
                              uint32 *ret_instance_min,
                              uint32 *ret_instance_max);
                              

/* ------------------------------------------------------------------------- */
/** Fetch multiple series of data from statsd in a single request, and
 * optionally align them all by instance number to ensure that an element
 * at a given index in any data series always corresponds to the same
 * stats instance.  For this latter part to work, all of the series must
 * come from the same stats object (sample or CHD).  If you are going to
 * plot all of the series you fetch here on the same graph, this function
 * has the following advantages vs. calling lg_graph_fetch_stats_data()
 * multiple times:
 *
 *    - Efficiency, by reducing the number of GCL round trips to statsd.
 * 
 *    - Avoids the case where statsd generates more data in the time
 *      interval between querying the first data set and querying
 *      the last, which is bad because then to fully align all of the
 *      data you have to fill a hole at the end of the earlier data
 *      sets, and possibly at the beginning of the later data sets,
 *      if old data was overwritten by the new.
 *
 *    - Allows us to properly align all of the data sets together,
 *      such that a given index into each always refers to the same
 *      instance number.  While we fill gaps in data sets fetched
 *      individually, without considering them all together we cannot
 *      detect holes at the beginning or end of individual ones,
 *      which could still occur if they are all queried together in
 *      the case that statsd simply never got a value for a given 
 *      instances of a given series.  Note that we only do this 
 *      alignment if the 'interpolate' boolean is true (though 
 *      technically it is not interpolation if we only have a value
 *      on one side: if values are missing from the ends, we just
 *      replicate the closest one we have to fill the gaps).
 *
 * We also do not support specifying individual colors for all of the
 * series to be created.  If 'choose_colors' to true, we will
 * automatically choose colors for all of them; otherwise, you can
 * iterate over the series setting the colors yourself.
 */
int lg_graph_fetch_stats_data_mult(lg_graph *graph, tbool choose_colors,
                                   md_commit *commit, const mdb_db *db, 
                                   const tstr_array *node_name_roots,
                                   const tstr_array *legend_names,
                                   int64 instance_min, int64 instance_max,
                                   lt_time_sec time_min, lt_time_sec time_max,
                                   int64 step, float64 divisor,
                                   lg_stats_flags_bf flags,
                                   lg_data_series_array **ret_data_series,
                                   uint32_array **ret_instance_mins,
                                   uint32_array **ret_instance_maxs);


/* ------------------------------------------------------------------------- */
/** Same as lg_graph_fetch_stats_data_mult(), except that the node
 * name roots and legend names are passed as arrays of 'const char *'.
 * The node name root array is terminated by the first NULL.
 * The legend names array, if non-NULL, is expected to be the same 
 * length as the node name root array, allowing it to contain NULLs
 * for some of the names without being considered terminated.
 */
int lg_graph_fetch_stats_data_mult_str(lg_graph *graph, tbool choose_colors,
                                       md_commit *commit, const mdb_db *db, 
                                       const char *node_name_roots[],
                                       const char *legend_names[],
                                       int64 instance_min, int64 instance_max,
                                       lt_time_sec time_min,
                                       lt_time_sec time_max,
                                       int64 step, float64 divisor, 
                                       uint32 flags,
                                       lg_data_series_array **ret_data_series,
                                       uint32_array **ret_instance_mins,
                                       uint32_array **ret_instance_maxs);
                                   

/* ------------------------------------------------------------------------- */
/** Extract data for a data series from an array of bindings,
 * presumably originated from statsd.  We will ignore any bindings
 * that do not start with the specified series node name root.  All
 * bindings that do, whether or not they fall within the instance or
 * time ranges specified, will be removed from the array.
 *
 * The removing of bindings from this array was done to speed up our
 * own operation (so we can move the appropriate attributes into the
 * data series instead of copying them), and to speed up subsequent
 * extractions of data from the remaining bindings.
 *
 * The series node name root will presumably be of the form:
 * "/stats/state/sample/ * /node/ *" or
 * "/stats/state/chd/ * /node/ *".  We expect the nodes underneath
 * this root to have the structure described in stats-design.txt.
 *
 * Most of the parameters to this function work the same as for
 * lg_graph_fetch_stats_data().
 */
int lg_graph_translate_stats_data(lg_graph *graph, lg_color color,
                                  bn_binding_array *inout_bindings,
                                  const char *node_name_root,
                                  const char *legend_name,
                                  int64 instance_min, int64 instance_max,
                                  lt_time_sec time_min, lt_time_sec time_max,
                                  int64 step, float64 divisor,
                                  lg_stats_flags_bf flags,
                                  lg_data_series **ret_data_series,
                                  uint32 *ret_instance_min,
                                  uint32 *ret_instance_max);
                                  

/* ------------------------------------------------------------------------- */
/** Create a data structure to hold a new series of data to be graphed.
 * Assign it the specified color, or if lg_color_choose is passed,
 * use a color chosen by lg_color_choose_auto().
 *
 * NOTE: this call does not add the data series to the graph.  You'll
 * still need to do this, when you're done filling out the structure,
 * by calling lg_data_series_array_append_takeover().
 */
int lg_data_series_new(lg_graph *graph, lg_color color,
                       lg_data_series **ret_data_series);


/* ------------------------------------------------------------------------- */
int lg_data_series_free(lg_data_series **inout_data_series);


/* ------------------------------------------------------------------------- */
/** Create a new pie slice.  Assign it the specified color, or if
 * lg_color_choose is passed, use a color chosen by
 * lg_color_choose_auto().
 *
 * NOTE: this call does not add the pie slice to the graph.  You'll
 * still need to do this, when you're done filling out the structure,
 * by calling lg_pie_slice_array_append_takeover
 * (graph->lgg_pie_chart->lpc_slices, &slice).
 */
int lg_pie_slice_new(lg_graph *graph, lg_color color, 
                     lg_pie_slice **ret_pie_slice);


/* ------------------------------------------------------------------------- */
int lg_pie_slice_free(lg_pie_slice **inout_pie_slice);


/* ------------------------------------------------------------------------- */
/** Create a new image map context structure, to be assigned to the
 * lps_tag field of an lg_pie_slice structure; or the lds_series_tag
 * of an lg_data_series structure; or to be appended to the
 * lds_bar_tags field of the same structure.
 */
int lg_image_map_context_new(lg_graph *graph, lg_image_map_context **ret_ctxt);


/* ------------------------------------------------------------------------- */
int lg_image_map_context_free(lg_image_map_context **inout_ctxt);


/* ------------------------------------------------------------------------- */
/** Create a new special line.  Assign it the specified color, or if
 * lg_color_choose is passed, use a color chosen by
 * lg_color_choose_auto().
 *
 * NOTE: this call does not add the special line slice to the graph.
 * You'll still need to do this, when you're done filling out the
 * structure, by calling lg_special_line_array_append_takeover
 * (axis->lga_special_lines, &special_line).
 */
int lg_special_line_new(lg_graph *graph, lg_color color,
                        lg_special_line **ret_special_line);


/* ------------------------------------------------------------------------- */
/** Free all memory associated with a specified graph structure.
 *
 * \param inout_graph Graph data structure to be freed.  The pointer 
 * passed in will be set to NULL after it is freed to avoid dangling
 * references.
 */
int lg_graph_free(lg_graph **inout_graph);


/* ------------------------------------------------------------------------- */
/** Draw a graph, and return a gdImagePtr to allow further
 * manipulation of the image using libGD.
 *
 * \param graph Structure containing all options and data for the
 * graph to draw.
 *
 * \retval ret_image LibGD image containing drawn graph.
 *
 * \retval ret_result Other results of the graphing operation, if any.
 */
int lg_graph_draw_to_memory(const lg_graph *graph, gdImagePtr *ret_image,
                            lg_graph_result **ret_result);


/* ------------------------------------------------------------------------- */
/** Draw a graph, and save it to a file with the specified path.
 *
 * \param graph Structure containing all options and data for the
 * graph to draw.
 *
 * \param format Image format to write in.  If there are
 * format-specific options to be specified (e.g. quality level for a
 * JPEG), those will be in the lg_graph structure.
 *
 * \param path Absolute path to file in which graph should be saved.
 *
 * \param overwrite If a file already exists at the specified path,
 * should we overwrite it?  If false is passed and the file exists,
 * lc_err_exists is returned.
 *
 * \retval ret_result Other results of the graphing operation, if any.
 */
int lg_graph_draw_to_file(const lg_graph *graph, lg_file_format format,
                          const char *path, tbool overwrite,
                          lg_graph_result **ret_result);


/* ------------------------------------------------------------------------- */
/** Render the contents of the lgr_regions field of the provided graph
 * result into a complete image map definition suitable for inclusion 
 * in an HTML document.  The returned string will be a MAP tag definition,
 * containing one AREA tag definition for each pie slice referenced in 
 * the graph result.
 *
 * NOTE: the lps_tag field of the pie slices in the graph that produced
 * this result MUST have been pointers to lg_image_map_context objects.
 * 
 * \param gr Graph result structure returned from rendering the graph.
 *
 * \param map_name String to be used as the NAME attribute on the MAP
 * tag in the HTML emitted.  You will then use this string in your own
 * IMG tag to reference the map defined in the returned HTML.
 *
 * \retval ret_image_map String containing the HTML described above.
 */
int lg_image_map_render(lg_graph_result *gr, const char *map_name,
                        tstring **ret_image_map);


/* ------------------------------------------------------------------------- */
/** Free all memory associated with a specified graph result structure.
 *
 * \param inout_graph_result Graph result data structure to be freed.
 * The pointer passed in will be set to NULL after it is freed to
 * avoid dangling references.
 */
int lg_graph_result_free(lg_graph_result **inout_graph_result);


/* ------------------------------------------------------------------------- */
/** Helper function for filling out the scale portions of the lg_axis
 * structure.  This function is only a convenient shortcut, as it does
 * only things that can also be accomplished by filling out fields in
 * the lg_axis structures.
 *
 * \param axis The axis structure to fill out.
 *
 * \param label The string label to use for this axis, if any.
 *
 * \param data_type The data type being plotted on this axis.  It must
 * be an integer numeric type.  The rest of the parameters are all
 * int64, because that type allows the broadest range of integers to
 * be represented.  If the caller is using floating point, or if
 * there are uint64 numbers greater than (2^63)-1, this function
 * cannot be used; the caller can always fill out the fields directly.
 *
 * \param range_low The low end of the range of data to be represented
 * on this axis.
 *
 * \param range_high The high end of the range of data to be
 * represented on this axis.
 *
 * \param minor_tick_spacing The space to put between minor ticks on
 * this axis: the value to use for lga_minor_ticks->lai_spacing.
 * Since spacing must be positive, a zero or any negative number means
 * not to have minor ticks.
 *
 * \param major_tick_spacing The space to put between major ticks on
 * this axis: the value to use for lga_major_ticks->lai_spacing.
 * Since spacing must be positive, a zero or any negative number means
 * not to have major ticks.
 *
 * \param value_label_spacing The space to put between value labels on
 * this axis: the value to use for lga_value_labels->lai_spacing.
 * Since spacing must be positive, a zero or any negative number means
 * not to have value_labels.
 */
int lg_axis_set_int(lg_axis *axis, const char *label,
                    bn_type data_type, int64 range_low, int64 range_high,
                    int64 minor_tick_spacing, int64 major_tick_spacing,
                    int64 value_label_spacing);


/* ------------------------------------------------------------------------- */
/** Floating point counterpart to lg_axis_set_int().
 */
int lg_axis_set_float(lg_axis *axis, const char *label, 
                      bn_type data_type, float64 range_low, float64 range_high,
                      float64 minor_tick_spacing, float64 major_tick_spacing,
                      float64 value_label_spacing);


/* ------------------------------------------------------------------------- */
/** Automatically choose and fill out certain fields in an axis on a
 * series graph yet to be drawn.  The fields set automatically are
 * lga_range_low, lga_range_high, lga_minor_ticks.lai_spacing,
 * lga_major_ticks.lai_spacing, and lga_value_labels.lai_spacing.
 * Note that this function is not appropriate to be called on the 
 * discrete axis of a bar graph, and will fail if called thusly.
 *
 * NOTE: this function MUST only be called after all of the data has
 * been added to the graph, and all of the flags have been set on
 * the lgg_flags field of the graph.
 *
 * The inputs to the automatic process are the data in the graph, and
 * the options specified in the lg_axis::lga_auto_opts field of the axis
 * specified.
 *
 * Note that there are some factors which we might have taken into account,
 * but currently do NOT:
 *   - Actual size of the value labels that will be rendered, to ensure 
 *     that they don't overlap.
 *   - Any of the five fields that are already filled out, as constraints
 *     we should work within.
 *
 * \param graph The graph containing the axis in question.
 * \param axis The axis whose fields to fill out automatically.
 * \param label Label to assign to this axis.  Pass NULL to leave it alone.
 * This is just a convenience, since you can set it yourself by setting 
 * lga_label directly.
 */
int lg_axis_set_auto(const lg_graph *graph, lg_axis *axis, 
                     const char *label);

/* ------------------------------------------------------------------------- */
/** Deinitialize global state in the graphing library.  This is not
 * really necessary to call, as it just does cleanup of some
 * non-recurring memory allocation, but can help make a Valgrind run
 * cleaner since otherwise some of that memory will show up as leaks.
 */
int lg_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* __GRAPH_H_ */
