/*
 *
 * src/bin/mgmtd/md_graph_utils.h
 *
 *
 *
 */

#ifndef __MD_GRAPH_UTILS_H_
#define __MD_GRAPH_UTILS_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

#include "graph.h"

/**
 * \file src/bin/mgmtd/md_graph_utils.h Graphing APIs for mgmtd modules
 * \ingroup mgmtd
 */

static const lg_color md_graph_color_major_ticks = 0xe8e8e8;
static const lg_color md_graph_color_value_labels = 0xf5caca;

/* ------------------------------------------------------------------------- */
/** Fetch data from statsd and add it to a newly-created series group
 * in the specified graph.
 *
 * \param graph The graph to add the data to.
 *
 * \param colors Colors to use for the series we're adding.  The
 * length of the array is not specified explicitly; it is expected to
 * have as many members as 'subst_strs', but can be terminated early
 * by a member with the value lg_color_choose.  If we need more colors
 * than are available in this list, we recycle the old colors, starting
 * again from the beginning of the list.
 *
 * \param commit Mgmtd commit structure.
 *
 * \param db Mgmtd database structure.
 *
 * \param subst_strs Strings to be substituted into node_name_fmt and
 * legend_name_fmt.
 *
 * \param node_name_fmt A printf-style format string which will be used to
 * form the names of stats nodes to fetch data from.  This should have 
 * exactly one "%s", into which we will substitute each member of subst_strs
 * to form each series.  (XXX/EMT: should support if subst_strs is NULL, 
 * we just use it as is)
 *
 * \param legend_name_fmt Same as node_name_fmt, except used to form name
 * of series as it will be displayed in the graph's legend.  Use this 
 * if your legend names can be thus formed; otherwise, use legend_names
 * parameter.
 *
 * \param legend_names Array of names for the series, as an alternative to
 * legend_name_fmt in cases where the names cannot be formed by simple
 * substitution.
 *
 * \param time_min Lower bound for timestamps of data points to be graphed,
 * or -1 for no bound.
 *
 * \param time_max Upper bound for timestamps of data points to be graphed,
 * or -1 for no bound.
 *
 * \param step Distance between each instance number to be graphed.
 * e.g. 1 means every instance, 2 means every other instance, etc.
 *
 * \param divisor Number by which to divide the value of every point
 * plotted.
 *
 * \param dsg_flags Bit fild of ::lg_data_series_group_flags for the 
 * data series group that will be created to contain the data fetched.
 *
 * \param snap_spacing If dsg_flags contains ldgf_align_snap, this
 * value will be converted to a bn_attrib of the same data type found
 * on the X axis, and used to fill in ldsg_snap_distance.  Otherwise,
 * it is ignored.
 *
 * \retval inout_max_num_points Pass in the maximum number of data
 * points seen in any series so far.  Returns the maximum number of
 * data points seen in any series, including the ones just fetched.
 * If you just want the number from the series just fetched, just pass
 * in a zero.  The caller may use this to determine if there was
 * enough data to draw an interesting graph, for example.
 */
int md_graph_get_stats_data(lg_graph *graph, const lg_color colors[],
                            md_commit *commit, const mdb_db *db, 
                            const tstr_array *subst_strs, 
                            const char *node_name_fmt,
                            const char *legend_name_fmt, 
                            const tstr_array *legend_names,
                            lt_time_sec time_min,
                            lt_time_sec time_max, int64 step, float64 divisor,
                            lg_data_series_group_flags_bf dsg_flags,
                            float64 snap_spacing,
                            int *inout_max_num_points);


/* ------------------------------------------------------------------------- */
/** Flags to modify the behavior of md_graph_render_graph().
 */
typedef enum {

    /**
     * Set dimensions manually, i.e. use whatever was found in the
     * lg_graph structure passed in, rather than extracting it from
     * the bindings in 'params'.
     */
    mgrf_manual_dims = 1 << 0,

} md_graph_render_flags;

/** Bit field of ::md_graph_render_flags ORed together */
typedef uint32 md_graph_render_flags_bf;


/* ------------------------------------------------------------------------- */
/** Render a graph and save the result as a file in the filesystem, in
 * PNG format.  The graph to be rendered is as specified in the
 * 'graph' parameter, but is further modified by:
 *
 *   - The width and height of the image to be generated is extracted
 *     from the "width" and "height" bindings in 'params'.  These 
 *     should be of type uint32 (though type string is also allowed
 *     for backward compatibility).
 *
 *   - The background color for the graph image overall is extracted
 *     from the "bgcolor" binding in 'params'.  It should be a string
 *     containing a hexadecimal representation of a 24-bit RGB value.
 *     It may optionally be preceded by a '#' character (for backward 
 *     compatibility).
 *
 *   - The filename to save the graph file to is extracted from the
 *     "filename" binding in 'params'.  (Specifying it in the
 *     "gif_name" binding is also supported for backward compatibility,
 *     though the file generated will still be in PNG format.)
 *
 * \param graph A graph structure containing the data to be graphed,
 * and other settings desired.
 *
 * \param params Bindings contained in the action request that
 * triggered us to generate the graph.  The width, height, and
 * filename of the graph can be extracted from here.
 *
 * \param commit Mgmtd commit structure.  If provided, the graph file
 * generated will be chown()ed to be owned by the uid and gid of the
 * user making the request.  Otherwise, the graph file generated will
 * have a uid and gid owner of 0 (root).
 *
 * \param mode Mode to assign to the file generated, or -1 to use our
 * default mode (defined as graph_file_mode in tpaths.h).
 *
 * \param path Full path to the file to generate, or NULL to use our
 * default (the destination directory defined as web_graphs_path in
 * tpaths.h, and the filename found in 'params' as described above).
 *
 * \param flags A bit field of ::md_graph_render_flags ORed together.
 */
int md_graph_render_and_save(lg_graph *graph, const bn_binding_array *params,
                             md_commit *commit, int64 mode, const char *path,
                             md_graph_render_flags_bf flags);

/* ------------------------------------------------------------------------- */
/** Sort of the first half of md_graph_render_and_save(): render the
 * graph into an image, but then return the image pointer so the caller
 * can make final modifications before the file is saved.
 */
int md_graph_render(lg_graph *graph, const bn_binding_array *params,
                    md_graph_render_flags_bf flags, gdImagePtr *ret_image);

/* ------------------------------------------------------------------------- */
/** Sort of the second half of md_graph_render_and_save(): given an
 * image, presumably containing an already-rendered graph, save it to
 * disk and fix up the owner and mode as appropriate.
 */
int md_graph_save(gdImagePtr image, const bn_binding_array *params,
                  md_commit *commit, int64 mode, const char *path,
                  md_graph_render_flags_bf flags);


/* ------------------------------------------------------------------------- */
/** Flags to md_graph_draw_markup().
 */
typedef enum {
    mgm_NONE = 0,
    
    /** 
     * Align markup to the left edge of the image.  By default, it is
     * aligned to the right.
     */
    mgm_left_align = 1 << 0,

    /**
     * Align markup to the right edge of the image.  This flag has no
     * effect, as right alignment is the default; it exists solely to
     * allow code to be more expressive.
     */
    mgm_right_align = 1 << 1,

} md_graph_markup_flags;

/** Bit field of ::md_graph_markup_flags ORed together */
typedef uint32 md_graph_markup_flags_bf;


/* ------------------------------------------------------------------------- */
/** Draw some text in an image generated from the specified graph.
 * The text is drawn in black, using the same font as specified in the
 * graph structure.  Each string in the array is drawn on a separate line.
 *
 * By default, all strings are right justified, and the block is
 * justified to the bottom of the screen.  The flags here leave the
 * door open to specifying other alignments in the future.
 */
int md_graph_draw_markup(lg_graph *graph, gdImagePtr image, 
                         const tstr_array *strings, 
                         md_graph_markup_flags_bf flags,
                         int horiz_margin, int vert_margin);

int md_graph_standard_params(lg_graph *graph);

int md_graph_format_x_axis(lg_graph *graph, lg_axis *xax);

#ifdef __cplusplus
}
#endif

#endif /* __MD_GRAPH_UTILS_H_ */
