/*
 *
 * src/bin/statsd/st_file.h
 *
 *
 *
 */

#ifndef __ST_FILE_H_
#define __ST_FILE_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

#include "st_data.h"

/* ------------------------------------------------------------------------- */
int st_file_init(void);


/* ------------------------------------------------------------------------- */
int st_file_deinit(void);


/* ------------------------------------------------------------------------- */
/* If a file already exists for the given prefix and record ID, open
 * it and load its contents into a dataset.  If not, create such a
 * file.
 */
int st_file_open(st_dataset *dataset);


/* ------------------------------------------------------------------------- */
/* Empty the contents of a dataset file.  Note that the old data is not
 * necessarily overwritten with zeros; the counters and pointers that 
 * tell us where the data is are simply reset to reflect that there is
 * no valid data.
 */
int st_file_empty(st_dataset *dataset);


/* ------------------------------------------------------------------------- */
/* Add a new, empty series (node record) to the file.
 *
 * Note: this routine fills out the remaining fields in the
 * structure: ssr_data_byte_offset, ssr_data_chunk_size, and
 * ssr_series_byte_size.
 */
int st_file_add_series(st_dataset *dataset, st_series *series);


/* ------------------------------------------------------------------------- */
/* Flush any data stored in the dataset's memory cache to its file.
 */
int st_file_flush(st_dataset *dataset);


/* ------------------------------------------------------------------------- */
/* Flush all datasets
 */
int st_file_flush_all(void);


/* ------------------------------------------------------------------------- */
/* For all of the elements in the specified range of instance IDs, for
 * the specified node, call a provided callback with the element
 * value.  If first_inst_id or last_inst_id are out of range, they are
 * clamped to the first and last valid instance ID, respectively.
 */
int st_file_iterate_elems(st_dataset *dataset, st_series *series,
                          int32 first_inst_id, int32 last_inst_id,
                          st_series_elem_iter_func iterate_func,
                          void *data);


/* ------------------------------------------------------------------------- */
/* Find the time a specified instance was taken, if it falls within the
 * range of the instances stored on disk.
 */
int st_file_lookup_instance(const st_dataset *dataset, int32 inst_id,
                            lt_time_sec *ret_inst_time);


/* ------------------------------------------------------------------------- */
/* Constants
 */

/*
 * Number of bytes allocated to hold node name of a series, including
 * the terminating NUL.
 */
#define st_node_name_size      496

#define ST_FILE_MAGIC "STATFILE"
extern const char st_file_magic[];
static const uint32 st_file_format_version =   1;

/*
 * Stats file absolute byte offsets for the header.
 */

static const uint32 sfao_header_begin =         0;
static const uint32 sfao_magic =                0;
static const uint32 sfao_format_version =       8;
static const uint32 sfao_num_series =          12;
static const uint32 sfao_max_instances =       16;
static const uint32 sfao_num_instances =       20;
static const uint32 sfao_oldest_inst_id =      24;
static const uint32 sfao_oldest_chunk_offset = 28;
static const uint32 sfao_newest_inst_id =      32;
static const uint32 sfao_newest_chunk_offset = 36;
static const uint32 sfao_inst_times =          40;

/*
 * Length of header without instance times
 */
#define st_header_fixed_len sfao_inst_times

/*
 * Stats file relative byte offsets for the series records,
 * relative to the beginning of the series.
 */
static const uint32 sfro_record_size =         0;
static const uint32 sfro_elem_size =           4;
static const uint32 sfro_node_type =           8;
static const uint32 sfro_node_name =          12;
static const uint32 sfro_reserved1 =          12 + st_node_name_size;
static const uint32 sfro_reserved2 =          12 + st_node_name_size + 4;
static const uint32 sfro_reserved3 =          12 + st_node_name_size + 8;
static const uint32 sfro_num_empty =          12 + st_node_name_size + 12;
static const uint32 sfro_data =               12 + st_node_name_size + 16;

/*
 * Length of series header
 */
#define st_series_header_len sfro_data


#ifdef __cplusplus
}
#endif

#endif /* __ST_FILE_H_ */
