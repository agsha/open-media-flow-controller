/*
 *
 * file_utils.h
 *
 *
 *
 */

#ifndef __FILE_UTILS_H_
#define __FILE_UTILS_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

#include "common.h"
#include "tstring.h"
#include "tstr_array.h"
#include "ttime.h"
#include "tbuffer.h"
#include "typed_arrays.h"

/* ------------------------------------------------------------------------- */
/** \file file_utils.h Utilities for working with files
 * \ingroup lc
 */

/* ------------------------------------------------------------------------- */
/** Options for copying files or directories.  These are flags which may be
 * ORed together.
 */
typedef enum {
    /**
     * No file copy options specified
     */
    fco_none = 0,

    /**
     * Should the destination be given the same access/mod/change
     * times as the source?
     */
    fco_preserve_times = 1 << 0, 
    
    /** 
     * If copying a directory, should subdirectories be copied
     * recursively?  If not set, only the directory specified, and its
     * immediate children which are files, are copied.
     */
    fco_recursive = 1 << 1, 
    
    /** 
     * If copying a directory, should files whose names begin with the
     * '.' character be skipped?
     */
    fco_skip_dotfiles = 1 << 2, 
    
    /** 
     * If set, and if something already exists with the name of the target,
     * it is deleted first.
     */
    fco_overwrite_target = 1 << 3, 
    
    /** 
     * Only applies to copying directories, and is mutually exclusive
     * with fco_overwrite_target; if set, and if there is already a
     * directory with the name given to the destination, the contents
     * of the source directory are added to the existing destination
     * directory.
     */
    fco_overlay_target = 1 << 4,

    /**
     * If the destination directory specified does not exist, should it
     * be created automatically?
     */
    fco_ensure_dest_dir = 1 << 5,

    /**
     * Sync the file and its parent directory before closing it, to
     * make absolutely sure the changes are written safely.
     */
    fco_sync = 1 << 6,

} file_copy_options;

/* Bit field of ::file_copy_options ORed together */
typedef uint32 file_copy_options_bf;

/* ------------------------------------------------------------------------- */
/** Copy a single file.
 *
 * \param from_path Full path to the source file to be copied.
 *
 * \param to_path Full path to the destination file.  NOTE: you must
 * specify the filename, even if you want to preserve the existing
 * filename.)
 *
 * \param options Options for how to copy the file.  If any options
 * which only apply to directories are are specified, an error is
 * raised.
 */
int lf_copy_file(const char *from_path, const char *to_path, 
                 file_copy_options_bf options);

/* ------------------------------------------------------------------------- */
/** Copy the contents of an entire directory.
 *
 * \param from_path Full path to the source directory to be copied.
 *
 * \param to_path Full path to the destination directory.  Note that
 * this is not the directory the copy will be placed in -- it is the
 * name the copy will be given.
 *
 * \param options Options for how to copy the directory.
 */
int lf_copy_dir(const char *from_path, const char *to_path,
                file_copy_options_bf options);

/* ------------------------------------------------------------------------- */
/** Make a full copy of whatever target is specified, whether a file
 * or a directory.
 *
 * \param from_path Full path to the target to be copied.
 *
 * \param to_path Full path to the destination to which the target
 * should be copied.  Note that this is not the directory the copy
 * will be placed in -- it is the name the copy will be given.
 *
 * \param options Options for how to copy the target.
 * If the target turns out to be a file, the options which only apply
 * to directories will be silently ignored.
 */
int lf_copy_anything(const char *from_path, const char *to_path,
                     file_copy_options_bf options);

/**
 * seek to specified offset in file, either absolute or relative.
 * Currently just passes through to lseek().  'whence' can be
 * SEEK_SET (offset is relative to beginning of file), SEEK_CUR
 * (offset is relative to current file pointer), or SEEK_END
 * (offset is relative to end of file).
 */
int lf_seek(int fd, uint64 offset, int whence, uint64 *ret_abs_offset);

/* See lf_write_file_flags below */
typedef uint32 lf_write_file_flags_bf;

/**
 * Compare the contents of two files, and tell if they are an exact
 * match.  One of the files is specified by an absolute filesystem
 * path; the other file must already be open, and is specified by an
 * fd which is opened for read (i.e. not write-only).
 *
 * If the file specified by file1_path does not exist, 
 * lc_err_file_not_found is returned.
 *
 * Note that this does not compare any file metadata, like owner/group
 * or mode.
 */
int lf_compare_file_fd(const char *file1_path, int file2_fd, tbool *ret_match);

/**
 * Compare the contents of a file to a buffer, and tell if they are an
 * exact match.  
 *
 * The only write flag honored by this function is
 * lwo_verify_ignore_file_extra .  All others are silently ignored.
 *
 * If the file specified does not exist,
 * lc_err_file_not_found is returned.  
 *
 * Note that this does not compare any file metadata, like owner/group
 * or mode.
 */

int lf_compare_buffer_fd(int fd, 
                         lf_write_file_flags_bf write_flags,
                         const void *buf, uint32 buf_len,
                         tbool *ret_match);


/** @name Reading and writing entire files */
/*@{*/

/** reads until eof.  */
int lf_read_file(const char *path, char **ret_buf, uint32 *ret_buf_len);

/* ------------------------------------------------------------------------- */
/** Options for reading files.  These are flags which may be ORed together.
 */
typedef enum {
    /**
     * No read options specified
     */
    lro_none = 0,

    /**
     * Should the target file be allowed to be a block special file?
     */
    lro_filefmt_block_allow = 1 << 1,

    /**
     * Should the target file be allowed to be a character special file?
     */
    lro_filefmt_char_allow = 1 << 2,
    
} lf_read_file_flags;

typedef uint32 lf_read_file_flags_bf;


/** (NYI) reads until eof or buf_size bytes */
int lf_read_file_buf(const char *path, uint32 buf_size,
                 char *ret_buf, uint32 *ret_buf_len);

/* XXX add function / param like lf_read_file that goes until NUL */

/** get a bytes from a file. if max_length == -1, whole file */
int lf_read_file_rbuf(const char *path, int32 max_length, 
                      char **ret_buf, uint32 *ret_buf_len);

/** 
 * Reads from a file into an allocated buffer, with options about which
 * types of source files to allow.
 */
int lf_read_file_rbuf_full(const char *path,
                           lf_read_file_flags_bf read_flags,
                           int32 max_length,
                           char **ret_buf, 
                           uint32 *ret_buf_len);

/** get a bytes from a file, until a NUL.  if max_length == -1, whole file */
int lf_read_file_str(const char *path, int32 max_length, 
                     char **ret_str, uint32 *ret_str_len);

/** get a bytes from a file, until a NUL.  if max_length == -1, whole file */
int lf_read_file_tstr(const char *path, int32 max_length, tstring **ret_tstr);

int lf_read_file_tbuf(const char *path, int32 max_length, 
                      tbuf **ret_tbuf);

/**
 * Read an entire file, splitting it into separate strings at newlines.
 * The newlines themselves are stripped.  Only '\\n' is looked for;
 * '\\r' is not treated specially, so this may have trouble with DOS or
 * Mac file formats.
 *
 * XXXX/EMT: NOTE: if the file does not have a trailing newline, the
 * last line of the file is NOT returned.  This could be considered a bug.
 */
int lf_read_file_lines(const char *path, tstr_array **ret_lines);

/* ------------------------------------------------------------------------- */
/** Options for writing files.  These are flags which may be ORed together.
 */
typedef enum {
    /**
     * No write options specified
     */
    lwo_none = 0,

    /**
     * Should the target file be overwritten?
     */
    lwo_overwrite = 1 << 0,

    /**
     * Should the target file be allowed to be a block special file?
     */
    lwo_filefmt_block_allow = 1 << 1,

    /**
     * Should the target file be allowed to be a character special file?
     */
    lwo_filefmt_char_allow = 1 << 2,

    /**
     * If set, read in what we wrote, comparing to the buffer we have.
     */
    lwo_verify_contents = 1 << 3,

    /**
     * If set, and lwo_verify_contents is set, ignore any trailing
     * (extra) bytes in the file, as compared to the buffer.  This does
     * not ignore extra characters in the buffer compared to the file.
     * This may be useful if writing a file to a raw device.
     */
    lwo_verify_ignore_file_extra = 1 << 4,
    
} lf_write_file_flags;

/* lf_write_file_flags_bf is declared above */

/**
 * Writes a given buffer to a file, with flags to specify options about
 * overwrite, allowed file types, and content verification.
 */
int lf_write_file_full(const char *path, mode_t file_mode, 
                       lf_write_file_flags_bf write_flags,
                       const char *buf, int32 buf_len);

/* 
 * Note: the file is always set to the specified mode, and the owner and
 *    group to the euid and egid.
 */
int lf_write_file(const char *path, mode_t file_mode, 
                  const char *buf, tbool overwrite, int32 buf_len);
int lf_write_file_tbuf(const char *path, mode_t file_mode, 
                       const tbuf *tb, tbool overwrite);

/* Note: fails returning -1 / ENOENT if path does not exist */
int lf_write_file_append(const char *path, const char *buf, int32 buf_len);

/*@}*/

/**
 * @name Reading and write bytes to files
 *
 * These functions deal with EINTR. 
 *
 * If the socket is non-blocking, they return whatever bytes they've handled
 * so far when EAGAIN is returned.  
 *
 * If the socket is blocking, writes continue until all bytes are written or
 * a fatal error occurs.  Reads continue until filling the buffer, or there
 * are no more bytes to read (read returns 0).
 *
 * Note that fewer bytes than requested may be read/written.  The actual number
 * is returned, or -1 on error, with errno set.
 */

/* XXX should rename to lf_read(), and make lf_read_bytes() return int */

/*@{*/

int32 lf_read_bytes(int fd, char *ret_buf, uint32 buf_size);

int32 lf_read_bytes_closed(int fd, char *ret_buf, uint32 buf_size,
                           tbool *ret_is_closed);

/** max_length == -1 means read until eof */
int lf_read_bytes_rbuf(int fd, int32 max_length, 
                       char **ret_buf, uint32 *ret_buf_len);

/** bytes beyond any NUL in the file are read from the fd, but lost */
int lf_read_bytes_str(int fd, int32 max_length, 
                      char **ret_str, uint32 *ret_str_len);

/** bytes beyond any NUL in the file are read from the fd, but lost */
int lf_read_bytes_tstr(int fd, int32 max_length, tstring **ret_tstr);

int lf_read_bytes_tbuf(int fd, int32 max_length, tbuf **ret_tbuf);

/**
 * Opens a file carefully for output, potentially overwritting an
 * existing file.
 */
int lf_empty_file(const char *path, mode_t file_mode, tbool overwrite, 
                  int *ret_fd);

/**
 * Opens a file carefully for output, potentially overwritting an
 * existing file.  Similar to lf_empty_file(), but takes flags
 * to further specify the behavior.  See lf_write_file_flags above.
 */
int lf_empty_file_full(const char *path, 
                       mode_t file_mode,
                       lf_write_file_flags_bf write_flags,
                       int *ret_fd);

/** 
 * Writes until all written, or if fd would block.  In both cases, return the
 * number of bytes written, or -1 on any error not related to blocking.
 * 
 * buf_len == -1 means write until first NUL of buf 
 */
int32 lf_write(int fd, const char *buf, int32 buf_len);

/** Returns error if not all bytes are written */
int lf_write_bytes(int fd, const char *buf, int32 buf_len,
                   int32 *ret_bytes_written);

int lf_write_bytes_tstr(int fd, const tstring *tstr, int32 *ret_bytes_written);

int lf_write_bytes_tbuf(int fd, const tbuf *tb, int32 *ret_bytes_written);

int lf_write_bytes_printf(int fd, int32 *ret_bytes_written,
                          const char *format, ...)
     __attribute__ ((format (printf, 3, 4)));

/*@}*/


/** Sees if we could write to fd without blocking. */
tbool lf_is_writable(int fd);

/**
 * Sees if we could write to fd without blocking within the time
 * period specified (in milliseconds)
 */
tbool lf_is_writable_timed(int fd, lt_dur_ms timeout);

/** Sees if we could read from fd without blocking. */
tbool lf_is_readable(int fd);

/**
 * Sees if we could read from fd without blocking withing the time
 * period specified (in milliseconds).
 */
tbool lf_is_readable_timed(int fd, lt_dur_ms timeout);

int lf_set_nonblocking(int fd, tbool is_non_blocking);
int lf_is_nonblocking(int fd, tbool *ret_is_non_blocking);

int lf_set_cloexec(int fd, tbool cloexec_on);

/**
 * @name Move, remove and create files and directories
 */

/*@{*/

/**
 * Rename a file and/or move it to a different directory.  The from
 * and to paths must be on the same filesystem: this will fail if they
 * are not.  Will overwrite the destination if it exists, subject to
 * the restrictions described in "man 2 rename".
 */
int lf_rename_file(const char *from_path, const char *to_path);

/**
 * Same as lf_rename_file() if from and to paths are on the same
 * filesystem.  If they are on different filesystems, does a copy and 
 * delete.
 */
int lf_move_file(const char *from_path, const char *to_path);

int lf_touch_file(const char *path, mode_t mode);
int lf_touch_command(const char *file_path);
int lf_touch_file_owner(const char *path, mode_t mode, uid_t uid, gid_t gid);
int lf_chown_file(const char *file_path, uid_t uid, gid_t gid);
int lf_remove_file(const char *path);
int lf_remove_dir(const char *dir_path);   /* only works if dir is empty */
int lf_remove_tree(const char *dir_path); 
int lf_remove_dir_contents(const char *dir_path);
int lf_ensure_dir(const char *dir_name, mode_t mode);
/** Only set uid & gid on newly created directories */
int lf_ensure_dir_owner(const char *dir_name, mode_t mode, uid_t uid,
                        gid_t gid);
int lf_ensure_dir_of_file(const char *file_name, mode_t mode);

/*@}*/

int lf_chdir(const char *dir_path);



/* ------------------------------------------------------------------------- */
/** @name File cleanup routines
 * Functions to delete older or somehow less desirable files to make
 * room for newer and/or better files.
 */
/*@{*/

/**
 * Cleanup context.  Caches information related to cleaning up a
 * particular group of files (usually one directory), so the directory
 * doesn't have to be rescanned every time we come back to delete more
 * files.
 */
typedef struct lf_cleanup_context lf_cleanup_context;

/**
 * Function which we can call that will tell us whether or not the 
 * specified file meets the cleanup criteria.  The 'filename' is just
 * the filename without the directory; this may be enough information
 * for a callback only called in the context of a single directory.
 * The 'dirname' is the directory in which the file was found;
 * the 'pathname' is the full absolute path to the file.
 *
 * Return true to include the file in the group, and false to exclude it.
 */
typedef tbool (*lf_cleanup_compare_func)(void *data, const char *filename,
                                         const char *dirname,
                                         const char *pathname);

/**
 * Scan a specified directory and create a context that can be used to
 * clean up files in that directory.
 *
 * \param dir Path to directory to scan
 *
 * \retval ret_context A context pointer which should be passed back
 * to lf_cleanup_files() zero or more times, and then to
 * lf_cleanup_free_context() .
 */
int lf_cleanup_get_context(const char *dir, lf_cleanup_context **ret_context);

/**
 * Scan another directory and add it to the provided context.
 */
int lf_cleanup_context_add_dir(lf_cleanup_context *context, const char *dir);

/**
 * Clean up files in a directory previously scanned by
 * lf_cleanup_get_context().  A certain number of the oldest (least
 * recently modified) files in the directory are deleted.  Note that
 * subdirectories, and the files in any subdirectories, are ignored.
 *
 * \param context The cleanup context returned from
 * lf_cleanup_get_context().
 *
 * \param prefix If specified, only files whose names are prefixed
 * with this string are considered.  If NULL or the empty string, all
 * files are considered.
 *
 * \param max_num_files If negative, the single oldest file meeting
 * the prefix criteria is deleted.  If non-negative, as many files as
 * necessary are deleted to bring the number of matching files down to
 * the number specified, starting with the oldest.
 *
 * \retval ret_bytes_deleted The sum of the sizes of all files
 * deleted.
 */
int lf_cleanup_leave_n_files(lf_cleanup_context *context, const char *prefix,
                             int32 max_num_files, uint64 *ret_bytes_deleted);

/**
 * Same as lf_cleanup_leave_n_files() except that instead of a prefix
 * to match files against, you provide a function to do customized
 * matching, and a void * to be passed to it.
 */
int lf_cleanup_leave_n_files_custom(lf_cleanup_context *context,
                                    lf_cleanup_compare_func compare_func,
                                    void *compare_data,
                                    int32 max_num_files,
                                    uint64 *ret_bytes_deleted);

/**
 * Same as lf_cleanup_leave_n_files() except it takes the number of
 * files to be deleted, not the number of files to leave.  If there
 * are not as many files to be deleted as requested, all of the
 * matching files are deleted and success is returned.
 */
int lf_cleanup_delete_n_files(lf_cleanup_context *context, const char *prefix,
                              uint32 num_files, uint64 *ret_bytes_deleted);

/**
 * Same as lf_cleanup_delete_n_files() except that instead of a prefix
 * to match files against, you provide a function to do customized
 * matching, and a void * to be passed to it.
 */
int lf_cleanup_delete_n_files_custom(lf_cleanup_context *context,
                                     lf_cleanup_compare_func compare_func,
                                     void *compare_data,
                                     uint32 num_files,
                                     uint64 *ret_bytes_deleted);


/**
 * Deallocate a context which was returned from lf_cleanup_get_context().
 *
 * \param inout_context The context pointer to free.  The memory is
 * deallocated and the pointer is set to NULL.
 */
int lf_cleanup_free_context(lf_cleanup_context **inout_context);
/*@}*/


/** @name File type and existence */
/*@{*/
int lf_test_exists(const char *path, tbool *ret_test);
int lf_test_is_regular(const char *path, tbool *ret_test);
int lf_test_is_regular_not_symlink(const char *path, tbool *ret_test);
int lf_test_is_dir(const char *path, tbool *ret_test);
int lf_test_is_dir_not_symlink(const char *path, tbool *ret_test);
int lf_test_is_symlink(const char *path, tbool *ret_test);
int lf_test_is_socket(const char *path, tbool *ret_test);
int lf_test_dir_of_file(const char *file_name, tbool *ret_test);
/*@}*/

/* File types */
typedef enum {
    lff_none = 0,
    lff_regular,
    lff_dir,
    lff_symlink,
    lff_socket,
    lff_last
} lf_ftype;

int lf_get_type(const char *path, lf_ftype *ret_ftype);

/**
 * Search an ordered list of directories for a file.  Searches for a
 * regular file whose name matches 'filename' in each of the
 * directories in 'dirs', starting from index 0, until it is found.
 * Returns the name of the directory in which the file was found and
 * the full path to that file, or NULL for each if the file was not
 * found in any directories.
 *
 * If there is an error reading any of the specified directories
 * (e.g. one does not exist, or is not a directory), they are silently
 * ignored.  This does not cause an error or abort the search.
 *
 * Wildcards in the filename and directory name are not supported.
 */
int lf_search_dirs_for_file(const char *filename, const tstr_array *dirs,
                            tstring **ret_found_dir, tstring **ret_found_path);

/* Directory walk and contents */

/* ------------------------------------------------------------------------- */
/** Retrieve a list of all files in a specified directory.  May set
 * errno on error.
 */
int lf_dir_list_names(const char *dir_path, tstr_array **ret_filenames);
int lf_dir_list_paths(const char *dir_path, tstr_array **ret_filepaths);

/* ------------------------------------------------------------------------- */
/** Retrieve a list of all files in a specified directory.  Can be filtered
 * by the provided function.  May set errno on error.
 */

typedef tbool (*lf_dir_matching_func)(const char *name, void *mf_arg);

int lf_dir_list_names_matching(const char *dir_path,
                               lf_dir_matching_func mf,
                               void *mf_arg,
                               tstr_array **ret_filenames);

int lf_dir_list_paths_matching(const char *dir_path,
                               lf_dir_matching_func mf,
                               void *mf_arg,
                               tstr_array **ret_filepaths);

/**
 * Count the number of entries in the directory matching the provided
 * pattern.
 */
int lf_dir_count_names_matching(const char *dir_path,
                                lf_dir_matching_func mf,
                                void *mf_arg, uint32 *ret_count);

/* 
 * A matching function for lf_dir_list_names_matching() to not return
 * names that start with '.' (like '.' and '..') 
 */
tbool lf_dir_matching_nodots_names_func(const char *name, void *mf_arg);


typedef int (*lf_dir_walk_func)(const char *full_path, const char *filename,
                                void *wf_arg);

int lf_dir_walk(const char *dir_path, tbool recursive, lf_dir_walk_func wf,
                void *wf_arg);

int lf_dir_list_subdirs(const char *dir, tbool recursive, 
                        tstr_array **ret_subdirs);

/* 
 * Same as lf_dir_list_subdirs() but will not recurse, and will return
 * symlinks to directories as well.  Use with caution, to avoid
 * potential security issues.
 */
int lf_dir_list_immediate_subdirs_symlinks(const char *dir, 
                                           tstr_array **ret_subdirs);


/** @name File properties: mode, size, times, ownership */
/*@{*/

/*
 * Returns lc_err_file_not_found if 'path' does not exist, and
 * lc_err_io_error on all other errors.
 */
int lf_get_file_mode(const char *path, mode_t *ret_file_mode);
/* Note this works only on actual files or directories, not on symlinks */
int lf_set_file_mode(const char *path, mode_t file_mode);
int lf_get_file_size(const char *path, uint64 *ret_file_size);
int lf_get_dir_size(const char *dir, tbool recursive, uint64 *ret_dir_bytes);

int lf_get_file_times(const char *path, lt_time_sec *ret_access_time,
                      lt_time_sec *ret_mod_time,
                      lt_time_sec *ret_change_time);
int lf_get_file_owner(const char *path, uid_t *ret_uid, gid_t *ret_gid);
int lf_set_file_owner(const char *path, uid_t uid, gid_t gid);


int lf_get_fd_mode(int fd, mode_t *ret_file_mode);
int lf_set_fd_mode(int fd, mode_t file_mode);
int lf_get_fd_size(int fd, uint64 *ret_file_size);
int lf_get_fd_times(int fd, lt_time_sec *ret_access_time,
                      lt_time_sec *ret_mod_time,
                      lt_time_sec *ret_change_time);
int lf_get_fd_owner(int fd, uid_t *ret_uid, gid_t *ret_gid);
int lf_set_fd_owner(int fd, uid_t uid, gid_t gid);
/*@}*/

#define lf_curr_dir            "."
#define lf_parent_dir          ".."


/** @name File name manipulation functions */
/*@{*/

/** Remove any multiple /'s or ..'s , append trailing / if it's not a file */
int lf_path_canonical(const tstring *path, tbool is_file, 
                          tstring **ret_path);
int lf_path_str_canonical(const char *path, tbool is_file,
                              tstring **ret_path);

int lf_path_is_absolute(const tstring *path, tbool *ret_abs);
int lf_path_is_absolute_str(const char *path, tbool *ret_abs);
int lf_path_make_absolute(const tstring *path, const tstring *rel_root,
                          tstring **ret_abs_path);
int lf_path_component_is_simple(const char *path_part, tbool *ret_is_simple);

/**
 * Make a path component simple (pass lf_path_component_is_simple()) by
 * replacing problematic characters with underscore ('_').
 */
int lf_path_component_simplify(const char *path_part, 
                               tstring **ret_simple_part);

/**
 * Make a path component safe to display, without losing any
 * information.  Nonprintable characters (anything less than 32 or
 * greater than 126), and the backslash character (92), are transformed
 * into "\x<nn>" where &lt;nn&gt; is a two-digit hex
 * representation of the character code.
 *
 * This differs from lf_path_component_simplify(), which loses
 * information by replacing a broader selection of characters all with
 * a single character, the underscore.  Exception: any multi-byte
 * UTF-8 characters we encounter will still be replaced with '_'.
 */
int lf_path_component_sanitize(const char *path_part, 
                               tstring **ret_sanitized_part);

char lf_path_get_delimiter(void);
char lf_path_get_extension_delim(void);

/**
 * Given a path or filename (we ignore the directories, and only look at
 * anything after the last delimeter), extract the filename extension:
 * everything after the last '.', if any.  If the filename ends in '.',
 * the empty string is returned.  If there is no '.', NULL is returned.
 */
int lf_path_get_extension(const char *path, tstring **ret_extension);

/**
 * Tells if the specified filename or path name specifies the current
 * or parent directories ("." or "..").  Set full_path to true if
 * file is a pathname with delimiter characters in it, to false if
 * it's just a filename.
 */
tbool lf_is_curr_or_parent_dir(const char *file, tbool full_path);

/** Just the last full component */
int lf_path_last(const char *path, tstring **ret_tail_name);

/** Everything before the last path component */
int lf_path_parent(const char *path, tstring **ret_dir_path);

/**
 * Break a path down into its components, i.e. one component per part
 * between delimiters.
 *
 * \param path Path to be broken down.
 * \retval ret_path_components The path's components, one string per
 * portion between delimiters.
 * \retval ret_is_absolute Tells if the path was absolute, i.e. if
 * it started with the delimiter.
 * \retval ret_ends_in_delimiter Tells if the path ended in the 
 * delimiter.  In some contexts this can indicate that the path
 * represents a directory name instead of a filename, though that
 * is not always the case.  We return this simply because the
 * information would otherwise be lost in the conversion to components.
 */
int lf_path_get_components(const char *path, 
                           tstr_array **ret_path_components, 
                           tbool *ret_is_absolute,
                           tbool *ret_ends_in_delimiter);


/** Construct paths */
int lf_path_from_tstr_array(tstring **ret_path, tbool last_is_file,
                            const tstr_array *path_components);
#if 0
int lf_path_from_str_array(tstring **ret_path, tbool last_is_file,
                           str_array *path_components);
#endif

/**
 * Construct a path given a set of parts to combine together.
 * Each part may specify one or more levels of the directory
 * hierarchy.  All but the last part must be directory names.  Up to
 * one delimiter is removed from or added to the end of the parts as
 * required, to allow for canonicalized or non-canonicalized directory
 * names, but the beginning is not touched.  The last part may be
 * either a directory or a file, which controls whether or not the
 * result ends with the delimiter.
 *
 * e.g. you could combine "/usr/local", "bin/", and "emacs", to get
 * "/usr/local/bin/emacs".
 */
int lf_path_from_va_str(tstring **ret_path, tbool last_is_file,
                        uint32 num_parts, const char *first_part, ...);

/** Like lf_path_from_va_str() except it takes tstrings */
int lf_path_from_va_tstr(tstring **ret_path, tbool last_is_file,
                         uint32 num_parts, const tstring *first_part, ...);

int lf_path_append_dir(tstring *inout_path, const tstring *dir);
int lf_path_append_file(tstring *inout_path, const tstring *file);
int lf_path_append_path(tstring *inout_path, tbool last_is_file, 
                        const tstring *path);

/**
 * Construct a path given a directory name and a filename within that
 * directory.  The dir_name may or may not be canonicalized (end with 
 * a path delimiter); one will be added only if needed.
 */
int lf_path_from_dir_file(const char *dir_name, const char *file_name,
                          tstring **ret_path_name);

/**
 * Given the full path to a file, and the name of a different directory,
 * returns the full path that file would have if it were in the other
 * directory.  Useful for moving or copying files.
 */
int lf_path_from_srcpath_destdir(const char *src_path,
                                 const char *dest_dir,
                                 tstring **ret_path_name);

/**
 * Looks at file system, determines the true, absolute name of a file,
 * eliminating any symlinks .  If path doesn't exist, returns an error.
 */
int lf_path_real_name(const tstring *path, tstring **ret_true_name);


/**
 * Looks at file system, and finds a given build system output directory,
 * for example a module directory.
 *
 * On a production system, this should always return in_path.
 * 
 */
int lf_lookup_dir_path(const char *in_path, char **ret_path);

/**
 * Must specify exactly one of: 1) local_dir (only, no other local_*) 
 * 2) local_dir and local_filename only, 3) local_path only.
 *
 * The idea of this is to support the model where end-users are downloading
 * from a url into a fixed directory location, and might (case 2) or might
 * not (case 1) have specified the name the file should have once
 * downloaded.  Case 3 is to support unconstrained (full path specified)
 * downloads, which presumably are never based directly on user input.
 *
 * On return we set, in case 1: ret_local_dir only. case 2: ret_local_dir,
 * ret_local_filename, and ret_local_path .  case 3 we set: ret_local_dir
 * ret_local_filename, and ret_local_path.
 *
 */
int lf_path_synth(const char *local_dir, const char *local_filename,
                  const char *local_path, tstring **ret_local_dir,
                  tstring **ret_local_filename, tstring **ret_local_path,
                  tbool *ret_success);

/**
 * Tells if the specified path could refer to any file outside the
 * subtree rooted at the directory from which the path is interpreted.
 * That is, returns true if (a) the path is absolute, or (b) the 
 * path's components contain ::lf_parent_dir in such a position as
 * to go above the current level.  e.g. "a/../.." escapes, while
 * "a/b/../c" does not.
 */
int lf_path_escapes_curr_subtree(const char *path, tbool *ret_escapes);

/*@}*/

/* ------------------------------------------------------------------------- */
/** Options for activating temporary files.  These are flags which may be
 * ORed together.
 */
typedef enum {
    /**
     * No activation options specified
     */
    lao_none = 0,

    /**
     * Should the original file be backed up to orig_filename.bak ?
     */
    lao_backup_orig = 1 << 0, 
    
    /** 
     * If we should disable sync'ing out the data of the temporary file
     * before moving it into place.  This may increase the chance of
     * data loss, but avoids potentially costly synchronization calls.
     */
    lao_sync_disable = 1 << 1, 

    /**
     * Before synching and closing the file, read its contents and
     * compare them to the destination file.  If they match, forget
     * the sync; just close and delete the temporary file instead.
     *
     * This is intended for the case where the caller does not know
     * ahead of time if the file it writes is going to be different
     * from what's already there.  We're willing to pay the price of
     * reading the file back in (it's probably already in cache anyway)
     * for the possibility of avoiding an fsync().
     */
    lao_compare_orig = 1 << 2,
    
} lf_activate_flags;

typedef uint32 lf_activate_flags_bf;

/**
 * Called to get an fd and name for a temporary file, which may eventually
 * be renamed to the original file.
 */
int lf_temp_file_get_fd(const char *orig_filename, 
                        char **ret_temp_filename,
                        int *ret_fd);

/**
 * After a temp file has been created (as above), this (as atomically as
 * possible) makes the temporary file be the original file.  It also
 * (optionally) syncs and closes the open fd.  Optionally saves the
 * original file to "orig_filename.bak"
 */
int lf_temp_file_activate_fd(const char *orig_filename,
                             const char *temp_filename,
                             int *inout_fd,
                             uid_t new_owner, gid_t new_group,
                             mode_t new_mode, 
                             lf_activate_flags_bf flags);

/**
 * After a temp file has been created (as above) and close()'d, this (as
 * atomically as possible) makes the temporary file be the original file.
 * Optionally saves the original file to "orig_filename.bak" .  
 *
 * In general, use lf_temp_file_activate_fd() instead, as it saves an extra
 * open() / close() pair.
 */
int lf_temp_file_activate(const char *orig_filename, 
                          const char *temp_filename,
                          uid_t new_owner, gid_t new_group,
                          mode_t new_mode, 
                          lf_activate_flags_bf flags);

/* Functions to call fsync() on various objects */
int lf_sync_fd(int fd);
int lf_sync_file_simple(const char *path);
int lf_sync_parent_dir(const char *path);
int lf_sync_file(const char *path);


/**
 * Atomically make a symlink that points at 'existing_path' in 'new_path'
 */
int lf_symlink(const char *existing_path, const char *new_path,
                tbool overwrite_existing);

/**
 * Find out what an existing symlink points to.
 */
int lf_read_symlink(const char *path, tstring **ret_link_contents);


/**
 * Fork a gzip process using lc_launch() to compress the specified
 * file in place.  This will create a compressed version of the file
 * with the same filename plus ".gz" at the end, and delete the
 * original file.
 */
int lf_gzip_file(const char *path);


/* File system info */

/**
 * ret_free_bytes is the non-superuser available bytes.  Allows
 * NULLs for any values you don't care about.
 */
int lf_get_fsys_space(const char *path, uint64 *ret_avail_bytes,
                      uint64 *ret_free_bytes, uint64 *ret_used_bytes,
                      uint64 *ret_total_bytes, uint64 *ret_total_inodes,
                      uint64 *ret_free_inodes);

/**
 * Given an absolute path, return an ID that uniquely identifies the
 * device that a file (or directory) lives on.  Between two files,
 * this number will be different iff they live on separate
 * filesystems.
 */
int lf_get_file_device_id(const char *path, tbool follow_symlinks,
                          uint64 *ret_device_id);

/**
 * Return array(s) describing the system's mounted filesystems.
 *
 * \param ret_mountdirs For each mount, the directory name which is
 * the target of the mount
 *
 * \param ret_dev_names For each mount, the block device or filesystem
 * which is the source of the mount.  Can be NULL.
 *
 * \param ret_fstypes For each mount, the file system name, like "ext3".
 * Can be NULL.
 *
 * \param ret_is_remote For each mount, if the file system name, is
 * local (like "ext3") or remote (like "nfs").  Can be NULL.
 */
int lf_get_mount_points(tstr_array **ret_mountdirs,
                        tstr_array **ret_dev_names,
                        tstr_array **ret_fstypes,
                        tbool_array **ret_is_remote);

/**
 * Return the device name for a given mount point
 */
int lf_get_mount_dev_name(const char *mount_name, tstring **ret_dev_name);
    
/**
 * Return the filesystem type for a given mount point
 */
int lf_get_mount_fstype(const char *mount_name, tstring **ret_fstype);

/* ------------------------------------------------------------------------- */
/**
 * Return true if both paths refer to the same exact file inode identity.
 * Paths may be full, or relative to the caller's current working directory.
 *
 * If the file specified by either file1_path or file_path2 does not exist, 
 * lc_err_file_not_found is returned.
 * 
 * \param file_path1 Path of file1
 *
 * \param file_path2 Path of file2
 *
 * \param ret_is_same_file Boolean return variable to indicate whether the files
 * are identical.  False is returned in case of an error.
 */
int lf_is_same_file_identity(const char *file_path1, const char *file_path2,
                             tbool *ret_is_same_file);

#ifdef __cplusplus
}
#endif

#endif /* __FILE_UTILS_H_ */
