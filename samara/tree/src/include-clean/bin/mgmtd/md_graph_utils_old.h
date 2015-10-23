/*
 *
 * src/bin/mgmtd/md_graph_utils_old.h
 *
 *
 *
 */

#ifndef __MD_GRAPH_UTILS_OLD_H_
#define __MD_GRAPH_UTILS_OLD_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

/*
 * Don't define anything here if this feature isn't defined.  This
 * will cause anyone trying to use the API to fail at compile time.
 * Since mgmtd uses modules, they would otherwise NOT fail at link
 * time, and the problem would only be detected at runtime.
 */
#ifdef PROD_FEATURE_OLD_GRAPHING

#include "bnode.h"
#include "hash_table.h"
#include "md_commit.h"
#include "mdb_db.h"
#include "graph.h"

/**
 * \file src/bin/mgmtd/md_graph_utils_old.h Old rrdtool-based graphing APIs
 * for mgmtd modules.  NOTE: these are present for backward compatibility,
 * but for any new graphs, the new graphing API in src/include/graph.h
 * (accompanied by src/bin/mgmtd/md_graph_utils.h for mgmtd modules)
 * is recommended.  In order to use the old API, you must enable
 * PROD_FEATURE_OLD_GRAPHING.
 * \ingroup mgmtd
 */

/**
 * Array of data points to be graphed.  The effectively acts as a union;
 * only one of the arrays in here will be used, according to a type 
 * field set elsewhere.
 *
 * XXX/EMT: not part of public API?
 */
typedef union md_graph_data_items {
    uint32_array  *mgdi_uint32;
    uint64_array  *mgdi_uint64;
    int64_array  *mgdi_int64;
    float32_array *mgdi_float32;
} md_graph_data_items;

/**
 * XXX/EMT: not part of public API?
 */
typedef struct md_graph_data {
    /** Binding name to be source for data */
    tstring *mgd_ds_bn_name;

    /** RRD tool data source 'DS' string
     * XXX/EMT: what is this used for?
     */
    tstring *mgd_ds_def;

    /** Data type for data in array.  Selects which field in ::mgd_array
     * to use.
     */
    bn_type mgd_type;

    /** Data for graph */
    md_graph_data_items mgd_array;
} md_graph_data;


TYPED_ARRAY_HEADER_NEW_NONE(md_graph_data, struct md_graph_data *);

int md_graph_data_new(md_graph_data **ret_graph_td, bn_type data_type);
int md_graph_data_dup(const md_graph_data *graph_td,
                      md_graph_data **ret_graph_td);
int md_graph_data_free(md_graph_data **ret_graph_td);
int md_graph_data_array_new(md_graph_data_array **ret_arr);

/* XXX/EMT: why are these in the public API? */
int md_graph_data_compare_names_for_array(const void *elem1,
                                          const void *elem2);
int md_graph_data_dup_for_array(const void *src, void **ret_dest);

void md_graph_data_free_for_array(void *elem);

typedef enum {
    mgs_line = 0x01,
    mgs_area = 0x02,
} md_graph_styles;

typedef enum {
    mgf_upperlimit = 0x01,
    mgf_lowerlimit = 0x02,
    mgf_width      = 0x04,
    mgf_height     = 0x08,
} md_graph_flags;

/**
 * Stores all the extracted data from statsd that will get fed into
 * rrdtool. There can be an arbitrary number of data sets as long as
 * they all (a) were collected at the same sample times; (b) have the
 * same number of samples; and (c) have the same data type. This
 * limitation exists in part due to how RRD tool 'updates' its
 * samples. XXX should explore more.
 */
typedef struct md_graph {
    /**
     * Data type of all data sources.  They all must be the same type.
     */
    bn_type mg_type;

    /** Data samples to be graphed */
    md_graph_data_array *mg_data;

    /** Data collection times for all samples. */
    lt_time_sec_array *mg_times;

    /** Start time for all samples
     * XXX/EMT: how is this different from mg_times[0]?  Is this like
     * the Y axis for the graph, which would usually want to correspond
     * with the first data point, but not necessarily?
     */
    lt_time_sec mg_start_time;

    /**
     * Graphing strings for RRD tool
     * XXX/EMT
     */
    tstr_array *mg_graph_defs;

    tstring *mg_rrd_path;         /* Location of RRD database */
    char *mg_rrdname;             /* An optional name for RRD DB */
    char *mg_bgcolor;             /* Background color for graph */
    char *mg_gifname;             /* Name of gif file to produce */
    int32 mg_uid;                 /* uid to chown gif to, or -1 */
    int32 mg_gid;                 /* gid to chgrp gif to, or -1 */
    int32 mg_mode;                /* mode to chmod gif to, or -1 */

    md_graph_flags mg_flags;      /* Set indication for some of the below */
    
    tbool mg_do_max;
    tbool mg_no_plot;             /* Skip generation of graph gif */
    lt_dur_sec mg_step;           /* Rate of chd samples */
    uint32 mg_data_scale;         /* Scaling of samples if necessary */
    int32 mg_ds_min;              /* Possible min value of data source */
    int32 mg_upper_limit;         /* Max. y to display in graph */
    int32 mg_lower_limit;         /* Min. y to display in graph */

    char *mg_vert_label;          /* Vertical axis label */

    /* height and width are strings since we're just going to sprintf
     * them into command strings anyway.
     */
    char *mg_width;              /* width of graph in pixels */
    char *mg_height;             /* height of graph in pixels */
} md_graph;


/* 
 * XXX may look into a different way of getting the updates into rrd
 * to avoid having large argvs.
 */
static const int rrd_max_line_size = 512;

/**
 * Allocate a new graphing data type that will be used to graph
 * a number of data sources, each collected at the same time.
 *
 * \param ret_graph The newly allocated data structure
 * \param data_type The data type of the data samples
 * \param step The interval at which the samples are taken, in seconds
 * \param do_max An indication if the maximum value for each sample
 *               should be calculated
 */
int md_graph_new(md_graph **ret_graph, bn_type data_type, lt_dur_sec step,
                 tbool do_max);

/**
 * Free the given graphing data structure, releasing all
 * associated memory.
 *
 * \param ret_graph The graphing data structure to free
 */
int md_graph_free(md_graph **ret_graph);

/**
 * Set the upper limit of the vertical axis to display. If not set
 * the scale is set automatically according to the data.
 *
 * \param graph The graphing data structure
 * \param upper_limit The maximum y-axis value to graph
 */
int md_graph_set_upper_limit(md_graph *graph, int32 upper_limit);

/**
 * Set the lower limit of the vertical axis to display. If not set
 * the scale is set automatically according to the data.
 *
 * \param graph The graphing data structure
 * \param lower_limit The minimum y-axis value to graph
 */
int md_graph_set_lower_limit(md_graph *graph, int32 lower_limit);

/**
 * Set the scaling factor of the data in the samples. If not
 * set, this defaults to one. The scale divides the data.
 *
 * \param graph The graphing data structure
 * \param scale Amount to divide the data samples by
 */
int md_graph_set_data_scale(md_graph *graph, uint32 scale);

/**
 * Set the vertical label on the graph.
 *
 * \param graph The graphing data structure
 * \param label Text to display
 */
int md_graph_set_vertical_label(md_graph *graph, const char *label);

/**
 * Set the 'no plot' attribute on the graph.
 *
 * \param graph The graphing data structure
 * \param no_plot The plotting disposition
 */
int md_graph_set_no_plot(md_graph *graph, tbool no_plot);

/**
 * Set the background color for the graph
 *
 * \param graph The graphing data structure
 * \param params The binding array which contains the action arguments
 *               that has the background color
 */
int md_graph_set_bgcolor_from_param(md_graph *graph,
                                    const bn_binding_array *params);

/**
 * Set the location of the produced file for the graph
 *
 * \param graph The graphing data structure
 * \param params The binding array which contains the action arguments
 *               that has the gif name
 */
int md_graph_set_gifname_from_param(md_graph *graph, 
                                    const bn_binding_array *params);

/**
 * Set the owner (uid) on the produced file for the graph
 *
 * \param graph The graphing data structure
 * \param uid The uid, or -1 to get the default owner
 */
int md_graph_set_gif_uid(md_graph *graph, int32 uid);

/**
 * Set the group (gid) on the produced file for the graph
 *
 * \param graph The graphing data structure
 * \param gid The gid, or -1 to get the default group
 */
int md_graph_set_gif_gid(md_graph *graph, int32 gid);

/**
 * Set the mode (permissions) on the produced file for the graph
 *
 * \param graph The graphing data structure
 * \param mode The mode, or -1 to get the default permissions
 */
int md_graph_set_gif_mode(md_graph *graph, int32 mode);

/**
 * Set the file permissions based on the commit uid and gid.
 *
 * \param graph The graphing data structure
 * \param commit The current mgmtd commit state
 */
int md_graph_set_gif_file_attribs_from_commit(md_graph *graph,
                                              const md_commit *commit);

/**
 * Set the height of the graph
 *
 * \param graph The graphing data structure
 * \param params The binding array which contains the action arguments
 *               that has the gif name
 */
int md_graph_set_height_from_param(md_graph *graph, bn_binding_array *params);

/**
 * Set the width of the graph
 *
 * \param graph The graphing data structure
 * \param params The binding array which contains the action arguments
 *               that has the gif name
 */
int md_graph_set_width_from_param(md_graph *graph, bn_binding_array *params);

/**
 * Set the name of the desired location of the RRD DB to save
 *
 * \param graph The graphing data structure
 * \param params The binding array which contains the action arguments
 *               that has the rrd name
 * rrdname param is the suffix for the standard graph generation directory
 * of a RRD DB file to keep after generating the data from statsd
 */
int md_graph_set_rrdname_from_param(md_graph *graph, 
                                    const bn_binding_array *params);

/**
 * A graph may have numerous data sources, which is permissible as
 * long as each of the sample for the given graph were measured
 * at the same time (this is not to be too onerous since the related
 * samples will normally come from a statd chd with multiple binding
 * nodes). This routine creates many of the necessary format strings
 * that are used by RRD tool.
 *
 * \param graph The graphing data structure
 * \param ds_name A unique name for the data source
 * \param bn_name The binding name to gather the data from
 * \param max_label A format string to be displayed in the graph
 *                  footer to display the maximum value if calculated
 */
int md_graph_add_data_source(md_graph *graph, const char *ds_name,
                             const char *bn_name, const char *max_label);

/**
 * Specify data sources that should be graphed together in
 * a related way. Includes providing the legel labels and colors.
 *
 * \param graph The graphing data structure
 * \param do_invert Specifies if data samples should be negated
 * \param style The visual style for each data source
 * \param num_elems Number of 3-tuples of variable arguments
 * \param ... A number of 3-tuples of (data source name, label, color)
 */
int md_graph_config_graph_va(md_graph *graph, tbool do_invert,
                             md_graph_styles style,
                             uint32 num_elems, ...);

/**
 * Analog of 'md_graph_config_graph_va' except in tstr_array form.
 *
 * \param graph The graphing data structure
 * \param do_invert Specifies if data samples should be negated
 * \param style The visual style for each data source
 * \param ds_names Data source names
 * \param labels Legend label array for each data source
 * \param colors Color on graph for each data source
 */
int md_graph_config_graph(md_graph *graph, tbool do_invert, 
                          md_graph_styles style, const tstr_array *ds_names,
                          const tstr_array *labels, const tstr_array *colors);

/**
 * Query the data from the graph's associated bindings and store the
 * time and data in the various typed arrays.
 *
 * \param graph The graphing data structure
 * \param commit Mgmtd data structure
 * \param db Mgmtd data base containing data
 */
int md_graph_get_binding_data(md_graph *graph, md_commit *commit,
                              const mdb_db *db);

/**
 * Should be called after the data samples have been collected.
 * A wrapper function to the individual steps of RRD tool which
 * result in the produced image.
 *
 * \param graph The graphing data structure
 * \param ret_made_graph Indication if a graph was created
 */
int md_graph_generate(md_graph *graph, tbool *ret_made_graph);

/**
 * A convenience function to create the required data sources
 * and graph components for related stats data from a single
 * name array. In other words, the name array could represent
 * a set of interfaces each of which would graph a particular
 * stat.
 *
 * \param graph The graphing data structure
 * \param do_invert
 * \param style
 * \param names
 * \param ds_name_fmt
 * \param stat_binding_fmt
 * \param label_fmt
 * \param color_choice
 * \param num_colors
 * \param is_fs_name
 */
int
md_graph_stats_name_array(md_graph *graph, tbool do_invert, 
                          md_graph_styles style,
                          const tstr_array *names,
                          const char *ds_name_fmt,
                          const char *stat_binding_fmt,
                          const char *label_fmt, const char *color_choice[],
                          uint32 num_colors, tbool is_fs_name);

#endif /* PROD_FEATURE_OLD_GRAPHING */

#ifdef __cplusplus
}
#endif

#endif /* __MD_GRAPH_UTILS_OLD_H_ */
