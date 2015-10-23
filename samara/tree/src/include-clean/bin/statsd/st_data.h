/*
 *
 * src/bin/statsd/st_data.h
 *
 *
 *
 */

#ifndef __ST_DATA_H_
#define __ST_DATA_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

#include "ttime.h"
#include "bnode.h"

/* ------------------------------------------------------------------------- */
/* An st_dataset structure represents the entire set of data stored
 * for a single sample or CHD record.
 *
 * The data is a two-dimensional matrix.  One axis is a sequence
 * number, also called the instance ID, which is a monotonically
 * increasing number representing when the sample was taken, and is
 * associated with a timestamp.  The other axis is the name of the
 * node on which the sample was based.  The matrix may be sparse in
 * some areas, but not sparse enough to justify fancy space-saving
 * measures at the expense of random access.
 *
 * The interface allows data to be added in rows but retrieved in
 * columns:
 *
 *   - A row of data is a set of nodes with all the same instance ID,
 *     and this is how the data is gathered.  This is also called an
 *     "instance".  There is no separate data structure for an
 *     instance, since the data is just passed to the dataset in a
 *     bn_binding_array and is thereafter stored in a different form.
 *
 *   - A column of data is a set of instances all based on the same
 *     node, and this is how the gathered data is exposed to clients.
 *     This is also called a "series" and is represented by st_series.
 *     There are no public API functions for modifying st_series 
 *     structures; they are exposed as read-only and managed internally
 *     by the data set.
 *
 * Internally, some of the data is stored in disk and some in memory.
 * However, this is not exposed in the API, except to allow the data
 * cache to be flushed to disk on demand.
 */

typedef struct st_dataset st_dataset;
typedef struct st_series st_series;

TYPED_ARRAY_PRE_DECL(st_series);

/* ------------------------------------------------------------------------- */
/* st_dataset: tracks a set of series of data for a single sample or
 * CHD record.
 */
struct st_dataset {
    char *    sd_owner_descr;

    /* ........................................................................
     * Information about the dataset in aggregate, counting both what
     * is stored on disk, and what is held in memory.
     *
     * Note that sd_num_instances can be greater than
     * sd_max_instances.  sd_max_instances limits the amount of data
     * that can be stored on disk, but more data will accumulate in
     * memory until it is flushed to disk.  At equilibrium, after a
     * flush sd_num_instances will equal sd_max_instances, and it
     * will grow beyond it until the next flush.
     *
     * Note that sd_newest_inst_id and sd_num_instances do not reflect
     * partial instances.
     */
    int32     sd_oldest_inst_id;
    int32     sd_newest_inst_id;
    uint32    sd_num_instances;
    uint32    sd_max_instances;

    /* ........................................................................
     * This simply reflects current configuration.  It is used to make
     * decisions when working with this dataset.
     */
    uint32    sd_num_to_cache;

    /* ........................................................................
     * Information about the file on disk
     *
     * The chunk offsets are effectively indices into the array of
     * instances within each series, as well as the array in the file
     * header that maps instance IDs to sample times.  At equilibrium
     * the newest chunk offset should be one less than the oldest
     * chunk offset.
     *
     * The next series byte offset is the byte offset at which the 
     * next new series should be added, should a new one arrive.
     * This information could also be derived from looking at the
     * ssr_data_byte_offset from all of the series, taking the
     * highest one, and adding ssr_series_byte_size.
     *
     * Note that partial instances are never written to disk,
     * so none of these values are affected by their existence.
     */
    tstring * sd_disk_file_path;
    int32     sd_disk_oldest_inst_id;
    int32     sd_disk_oldest_chunk_offset;
    int32     sd_disk_newest_inst_id;
    int32     sd_disk_newest_chunk_offset;
    uint32    sd_disk_num_instances;
    uint64    sd_disk_series_begin_offset;
    uint64    sd_disk_series_next_offset;

    /* ........................................................................
     * Information about the cache of data points in memory.
     * Note that sd_mem_newest_inst_id and sd_mem_num_instances
     * do not reflect partial instances.
     */
    int32     sd_mem_oldest_inst_id;
    int32     sd_mem_newest_inst_id;
    uint32    sd_mem_num_instances;

    /*
     * Records for each of the series in this dataset.
     */
    st_series_array *   sd_series;

    /*
     * Array of times for each instance ID.  This array is parallel to
     * the attribute arrays in each entry in sd_series: the instance ID
     * that each entry refers to is its index plus sd_mem_oldest_inst_id.
     */
    lt_time_sec_array * sd_inst_times;

    /*
     * Keeps track of whether or not a partial instance is present for
     * this dataset, and if so, what time it was added.  If this is
     * false, the ssr_partial_instance field of all of its series'
     * should be NULL.  If this is true, the field may have a value,
     * if a partial instance was successfully computed.
     */
    tbool               sd_has_partial_instance;
    lt_time_sec         sd_partial_time;
};


/* ------------------------------------------------------------------------- */
/* st_series: tracks a series of data for a single node
 */
struct st_series {
    st_dataset *       ssr_dataset;
    tstring *          ssr_node_name;
    tstr_array *       ssr_node_name_parts;

    /*
     * NOTE: in the future, we may store a 0 here to mean that a
     * series has been decided to be "obsolete", meaning it is not
     * expected to have any more data, and may be removed.
     */
    bn_type            ssr_node_type;

    /*
     * Cache of instances that have not yet been written to disk.
     * Note that these instances are all complete; partial instances
     * are stored separately in ssr_partial_instance.
     */
    bn_attrib_array *  ssr_elem_cache;

    /*
     * The byte offset in the file where the first data element starts
     * (equivalent to the file header length), and the size of each
     * data element.  These numbers combined with a chunk offset can
     * locate the beginning of any chunk in the file.
     */
    uint32             ssr_data_elem_size;
    uint64             ssr_series_byte_offset;
    uint32             ssr_series_byte_size;

    /*
     * A partial value for the next instance in this series.
     * Only complete instances are stored in ssr_elem_cache;
     * if there is a partially-calculated instance, it is stored
     * here.  If there isn't, this is NULL.
     *
     * The partial instance, if there is one, has an instance ID of
     * one higher than the highest instance ID currently assigned to
     * a complete instance.
     */
    bn_attrib *        ssr_partial_instance;

    /*
     * The number of consecutive empty instances in this series that
     * have been written to the data file on disk since the last
     * non-empty instance.  This number is maintained as statsd runs
     * and saved in the data file whenever the series is flushed,
     * so it is always up to date.   
     * 
     * This helps us with bug 12175: once we have written at least 
     * sd_max_instances (from our dataset), we can stop writing zeroes,
     * and save some disk I/O.
     */
    uint32             ssr_num_empty_elems;
};

TYPED_ARRAY_HEADER_BODY_NEW_NONE(st_series, st_series *);


/* ------------------------------------------------------------------------- */
/* Initialize a dataset.  If a file exists with data for this dataset,
 * load the relevant information into memory to set us up to use that
 * file.  Otherwise, create an empty file.
 */
int st_dataset_new(st_dataset **ret_dataset, uint32 max_instances,
                   int32 num_to_cache, char **inout_owner_descr,
                   const char *filename_prefix, const char *record_id);


/* ------------------------------------------------------------------------- */
/* Deallocate a dataset structure.  If it had a file on disk, the file
 * is left, of course.
 */
int st_dataset_free(st_dataset **inout_dataset);


/* ------------------------------------------------------------------------- */
/* Clear the contents of an entire dataset, including all of the series.
 * It will start fresh, as if it was just created.
 *
 * XXX/EMT: not sure if this should reset the instance IDs, but it does
 * currently.
 */
int st_dataset_clear(st_dataset *dataset);


/* ----------------------------------------------------------------------------
 * Add a new instance to the dataset.
 *
 * If 'partial' is true, this instance is saved as a partial instance
 * instead of being added to the memory cache of complete instances.
 * If 'partial' is false, the instance is added normally, and if a
 * partial instance is outstanding, it is deleted.
 *
 * Note: the instance ID and timestamp are not provided because they
 * are computed automatically.
 *
 * Note: the instance is not const because we sort it to aid performance,
 * and then remove elements from it as they are added to the dataset.
 * The caller should not assume that anything will be left in it when
 * we return, though the array itself will not be freed.
 *
 * ret_clear will be set to true if the dataset should be cleared.
 * This would be as a result of detecting a change of data type.
 */
int st_dataset_add_instance(st_dataset *dataset,
                            bn_binding_array *instance, tbool partial,
                            tbool *ret_clear_dataset);


/* ----------------------------------------------------------------------------
 * Iterate over all of the series in the dataset.
 */

typedef int (*st_dataset_series_iter_func)(st_dataset *dataset,
                                           st_series *series,
                                           void *data);

int st_dataset_iterate_series(st_dataset *dataset, 
                              st_dataset_series_iter_func iter_func,
                              void *data);


/* ------------------------------------------------------------------------- */
/* Look up a specified series by name.
 */
int st_dataset_lookup_series(const st_dataset *dataset, const char *node_name,
                             st_series **ret_series);


/* ----------------------------------------------------------------------------
 * Create a new series record.  This does not initialize the
 * ssr_data_byte_offset, ssr_data_chunk_size, and
 * ssr_series_byte_size.  It is expected that the caller will
 * afterwards call st_file_add_series() to add a new series to an
 * existing file, or if the series is being read from a file, fill in
 * the fields himself.
 */
int st_series_new(st_series **ret_series, st_dataset *dataset,
                  const char *node_name, bn_type node_type);


/* ----------------------------------------------------------------------------
 * Iterate over all of the instances in a series.
 *
 * first_inst_id can be 0 to start at the beginning (0 doesn't have to
 * still be present; it will just start at first actual instance at or
 * following the ID given).  last_inst_id can be -1 to go to the end.
 */

typedef int (*st_series_elem_iter_func)(st_series *series,
                                        int32 inst_id, tbool partial,
                                        const bn_attrib *elem, void *data);

int st_series_iterate_elems(st_series *series, 
                            int32 first_inst_id, int32 last_inst_id, 
                            st_series_elem_iter_func iter_func, void *data);


/* ----------------------------------------------------------------------------
 * Look up an element from a series by instance ID.
 */
int st_series_lookup_elem(st_series *series, int32 inst_id,
                          bn_attrib **ret_elem, tbool *ret_partial);


/* ----------------------------------------------------------------------------
 * A shortcut to just getting the last element from a series.
 * Note that this does not return the last non-NULL element;
 * it returns whatever is in the dataset for that element for that
 * last instance ID, which could be NULL if that node was missing
 * last time.
 */
int st_series_get_last_elem(st_series *series, tbool *ret_ok,
                            int32 *ret_inst_id, bn_attrib **ret_elem,
                            tbool *ret_partial);


/* ----------------------------------------------------------------------------
 * Make sure a range of instance IDs is in range.  Note that if a partial
 * instance is present, the highest permissible instance ID is one higher
 * than sd_newest_inst_id.
 */
int
st_dataset_clamp_inst_id(st_dataset *dataset, int32 *inout_first_inst_id,
                         int32 *inout_last_inst_id);


/* ----------------------------------------------------------------------------
 * Get information about a particular instance ID: when it was taken, and
 * whether or not it is a partial instance.
 */
int st_dataset_get_instance_time(const st_dataset *dataset, int32 inst_id,
                                 lt_time_sec *ret_time, tbool *ret_partial);


/* ----------------------------------------------------------------------------
 * Get the instance ID and time of the last instance in the dataset.
 */
int st_dataset_get_last_instance(const st_dataset *dataset, tbool *ret_ok,
                                 int32 *ret_inst_id, lt_time_sec *ret_time,
                                 tbool *ret_partial);

/* ----------------------------------------------------------------------------
 * Get the range of instance IDs that fall within the range of times
 * provided.  We set ret_got_some_instances to true iff we found at
 * least one instance in the range.
 */
int st_dataset_get_instance_range(const st_dataset *dataset,
                                  lt_time_sec tmin, lt_time_sec tmax,
                                  int32 *ret_inst_min, int32 *ret_inst_max,
                                  tbool *ret_got_some_instances);


#ifdef __cplusplus
}
#endif

#endif /* __ST_DATA_H_ */
