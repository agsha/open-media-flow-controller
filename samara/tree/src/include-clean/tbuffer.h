/*
 *
 * tbuffer.h
 *
 *
 *
 */

#ifndef __TBUFFER_H_
#define __TBUFFER_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

/* ------------------------------------------------------------------------- */
/** \file tbuffer.h Buffer data type for holding data which may contain
 * zeros (and thus not suitable for strings).
 * \ingroup lc_ds
 */

#include "common.h"
#include "tstring.h"
#include "tstr_array.h"

typedef struct tbuf tbuf;

int tb_new(tbuf **ret_tb);
int tb_new_size(tbuf **ret_tb, uint32 initial_size);
int tb_new_buf(tbuf **ret_tb, const uint8 *buf, int32 buf_len);
int tb_new_takeover(tbuf **ret_tb, uint8 **buf_h, 
                    int32 buf_len, uint32 buf_size);
int tb_new_buf_const(tbuf **ret_tb, const uint8 *buf, int32 buf_len);
int tb_set_option_auto_shrink(tbuf *tb, tbool auto_shrink);
int tb_set_option_const_buf(tbuf *tb, tbool const_buf);
int tb_free(tbuf **inout_tb);
int tb_clear(tbuf *tb);
int tb_detach(tbuf **inout_tb, uint8 **ret_buf, int32 *ret_buf_len);
int tb_dup(const tbuf *in_tb, tbuf **ret_tb);
int tb_dup_buf(const tbuf *tb, uint8 **ret_duplicate, int32 *ret_byte_length);
int tb_reverse(tbuf *tb);
int tb_ensure_size(tbuf *tb, uint32 size);
int tb_set_length(tbuf *tb, uint32 length);
int32 tb_length(const tbuf *tb);
int32 tb_size(const tbuf *tb);
int32 tb_avail(const tbuf *tb);

int tb_delete_bytes(tbuf *tb, uint32 start_byte_offset, uint32 count);

int tb_append(tbuf *tb, const tbuf *tbuffer);
int tb_append_buf(tbuf *tb, const uint8 *buf, int32 buf_len);
int tb_append_byte(tbuf *tb, uint8 by);
int tb_append_bytes(tbuf *tb, uint8 by, uint32 count);
int tb_append_uint8(tbuf *tb, uint8 by);
int tb_append_uint16(tbuf *tb, uint16 ap_16);
int tb_append_uint32(tbuf *tb, uint32 ap_32);
int tb_append_uint64(tbuf *tb, uint64 ap_64);
int tb_append_str(tbuf *tb, tbool add_nul, tbool include_pascal_length,
                  tbool use_net_byte_order,
                  const char *str, int32 str_len);
int tb_append_tstr(tbuf *tb, tbool add_nul, 
                   tbool include_pascal_length, 
                   tbool use_net_byte_order,
                   const tstring *tstr);
int tb_append_tstr_array(tbuf *tb, 
                         tbool add_nul, tbool include_pascal_length,
                         tbool use_net_byte_order,
                         const tstr_array *tsa);
int tb_append_sprintf(tbuf *tb, tbool add_nul, tbool include_pascal_length,
                      tbool use_net_byte_order,
                      const char *format, ...)
     __attribute__ ((format (printf, 5, 6)));

int tb_insert_byte(tbuf *tb, uint32 byte_offset, uint8 by);

int tb_put(tbuf *tb, uint32 tb_byte_offset, const tbuf *tbuffer);
int tb_put_buf(tbuf *tb, uint32 tb_byte_offset, const uint8 *buf, 
               int32 buf_len);
int tb_put_byte(tbuf *tb, uint32 tb_byte_offset, uint8 by);
int tb_put_bytes(tbuf *tb, uint32 tb_byte_offset, uint8 by, uint32 count);
int tb_put_uint8(tbuf *tb, uint32 tb_byte_offset, uint8 by);
int tb_put_uint16(tbuf *tb, uint32 tb_byte_offset, uint16 ap_16);
int tb_put_uint32(tbuf *tb, uint32 tb_byte_offset, uint32 ap_32);
int tb_put_uint64(tbuf *tb, uint32 tb_byte_offset, uint64 ap_64);

/**
 * Write a string to a tbuffer starting at a specified index.
 *
 * \param tb The tbuffer to which to write.
 *
 * \param tb_byte_offset The byte offset at which to begin writing,
 * starting from 0.
 *
 * \param add_nul After writing the contents of the string, should 
 * we append a NUL ('\\0') character?
 *
 * \param include_pascal_length Before writing the contents of 
 * the string, should we write the length of the string in bytes,
 * as a uint32?
 *
 * \param use_net_byte_order Only applicable to the 'include_pascal_length'
 * option.  If including a Pascal length, should we write it in network
 * byte order?  If false, host byte order is used instead.
 *
 * \param str The string to be written to the tbuffer.
 *
 * \param str_len The length of the string to be written.  Pass -1 to
 * have the length determined automatically by the delimiter.  You would
 * only pass the length here (a) for efficiency, if you already have it;
 * or (b) if you don't want the entire string copied.
 *
 * \retval ret_tb_bytes The number of bytes written to the tbuffer.
 * In addition to the string itself, this includes any extra bytes 
 * added by the add_nul or include_pascal_length options.
 * 
 */
int tb_put_str(tbuf *tb, uint32 tb_byte_offset, 
               tbool add_nul, tbool include_pascal_length, 
               tbool use_net_byte_order,
               const char *str, int32 str_len, int32 *ret_tb_bytes);

int tb_put_tstr(tbuf *tb, uint32 tb_byte_offset, tbool add_nul, 
                tbool include_pascal_length, 
                tbool use_net_byte_order,
                const tstring *tstr, int32 *ret_tb_bytes);

int tb_put_tstr_array(tbuf *tb, uint32 tb_byte_offset, tbool add_nul, 
                      tbool include_pascal_length, tbool use_net_byte_order,
                      const tstr_array *tsa, int32 *ret_tb_bytes);

int tb_put_sprintf(tbuf *tb, uint32 tb_byte_offset, 
                   tbool add_nul, tbool include_pascal_length,
                   tbool use_net_byte_order, int32 *ret_tb_bytes,
                   const char *format, ...)
     __attribute__ ((format (printf, 7, 8)));

const uint8 *tb_buf(const tbuf *tb);
const uint8 *tb_buf_maybe(const tbuf *tb);
const uint8 *tb_buf_maybe_empty(const tbuf *tb);

/**
 * Get some bytes out of a tbuffer, and place them in a provided raw
 * buffer in memory.
 *
 * \param tb Tbuffer to read from.
 * \param byte_offset Byte offset from 0 of first byte to fetch.
 * If this is beyond the offset of the last byte in the tbuffer
 * (it is >= the tbuffer's length), an error will be returned.
 * \param num_bytes Number of bytes to fetch from tbuffer, or -1 to get
 * everything until the end of the buffer.  Note that we may fetch less
 * than requested, if (a) it is a positive number, and there are not as
 * many bytes available in the tbuffer; or (b) the number of bytes 
 * to fetch (either explicitly specified, or determined from -1) is
 * greater than the buffer size.
 * \param rbuf_size_bytes Size of return buffer.  We will never write more
 * than this number of bytes to the return buffer.
 * \param ret_rbuf Buffer in which to place the bytes retrieved.
 * Note that the bytes retrieved will NOT be NUL-terminated, as this is 
 * not a string we are working with.
 * \retval ret_rbuf_length Number of bytes actually placed in the return 
 * buffer.
 * \retval ret_avail_length Number of bytes that were available to be placed
 * in the return buffer.  This will match the value placed in ret_rbuf_length
 * unless the buffer was not big enough to hold everything (i.e. the result
 * was truncated).
 */
int tb_get_buffer(const tbuf *tb, uint32 byte_offset, int32 num_bytes,
                  uint32 rbuf_size_bytes, 
                  uint8 *ret_rbuf, 
                  int32 *ret_rbuf_length, 
                  int32 *ret_avail_length);

int tb_get_str(const tbuf *tb, uint32 byte_offset, int32 max_tb_bytes,
               tbool has_nul, tbool has_pascal_length,
               tbool use_net_byte_order,
               char **ret_str, int32 *ret_str_len, int32 *ret_tb_bytes);
int tb_get_tstr(const tbuf *tb, uint32 byte_offset, int32 max_tb_bytes,
                tbool has_nul, tbool has_pascal_length,
                tbool use_net_byte_order,
                tstring **ret_tstring, int32 *ret_tb_bytes);

/**
 * Tokenize the contents of a tbuffer into a tstr_array.  Regardless of
 * has_pascal_length (which only applies to the strings themselves),
 * assumes that the tbuffer's first 4 bytes are a uint32 containing
 * the number of strings to read.
 */
int tb_get_tstr_array(const tbuf *tb, uint32 byte_offset, int32 max_tb_bytes,
                      tbool has_nul, tbool has_pascal_length,
                      tbool use_net_byte_order, tstr_array **ret_tstr_array, 
                      int32 *ret_tb_bytes);

/**
 * Compare two tbuffers.  Return a number less than, equal to, or
 * greater than zero, if tb1 is less than, equal to, or greater than
 * tb2, respectively.
 */
int tb_cmp(const tbuf *tb1, const tbuf *tb2);

uint8 tb_get_quick_byte(const tbuf *tb, uint32 byte_offset);
uint8 tb_get_quick_uint8(const tbuf *tb, uint32 byte_offset);
uint16 tb_get_quick_uint16(const tbuf *tb, uint32 byte_offset);
uint32 tb_get_quick_uint32(const tbuf *tb, uint32 byte_offset);
uint64 tb_get_quick_uint64(const tbuf *tb, uint32 byte_offset);

int tb_get_uint8(const tbuf *tb, uint32 byte_offset, uint8 *ret_uint8);
int tb_get_uint16(const tbuf *tb, uint32 byte_offset, uint16 *ret_uint16);
int tb_get_uint32(const tbuf *tb, uint32 byte_offset, uint32 *ret_uint32);

/*
 * Like tb_get_uint32(), except does not verify the pointer arguments
 * are non-NULL.  Only use if you are pre-checking these conditions.
 */
int tb_get_uint32_nonnull(const tbuf *tb, uint32 byte_offset,
                          uint32 *ret_uint32);
/*
 * Like tb_get_uint32(), except does not verify any args, including that
 * bytes are available or that arguments are non-NULL.  Only use if you
 * are pre-checking all these conditions.
 */
void tb_get_uint32_unchecked(const tbuf *tb, uint32 byte_offset,
                             uint32 *ret_uint32);
int tb_get_uint64(const tbuf *tb, uint32 byte_offset, uint64 *ret_uint64);

/**
 * Replace all instances of a specified byte in the tbuffer with
 * another byte.
 *
 * One possible application of this would be to make a tbuffer safe to
 * convert into a tstr, by replacing the zero byte with something
 * else.  ts_replace_char() does not support this particular
 * operation.
 */
int tb_replace_byte(tbuf *tb, uint8 old_byte, uint8 new_byte);

#ifdef __cplusplus
}
#endif

#endif /* __TBUFFER_H_ */
