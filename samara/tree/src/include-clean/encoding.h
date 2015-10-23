/*
 *
 * encoding.h
 *
 *
 *
 */

#ifndef __ENCODING_H_
#define __ENCODING_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

/* ------------------------------------------------------------------------- */
/** \file encoding.h Routines to encode and decode data
 * \ingroup lc
 */

#include "common.h"
#include "tstring.h"
#include "tbuffer.h"

#ifndef bits_per_b32_char
#define bits_per_b32_char   5
#endif

#ifndef bits_per_byte
#define bits_per_byte       8
#endif

/**
 * Masks to clear out all but the n lowest bits of a byte.
 */
static const uint8 lce_bit_masks[] = {0x00, 0x01, 0x03, 0x07,
                                      0x0f, 0x1f, 0x3f, 0x7f, 0xff};


/* ------------------------------------------------------------------------- */
/** @name Base16 encoding
 *
 * Our base16 alphabet is hexadecimal using capital letters.
 */
/*@{*/

/**
 * Encode a series of bytes in base16.
 *
 * \param bytes The buffer to be encoded
 *
 * \param size The number of bytes to be encoded
 *
 * \retval ret_b16_str The base16 string that encodes the provided bytes
 */
int lce_base16_encode(const uint8 *bytes, uint32 size, tstring **ret_b16_str);

/**
 * Decode a base16 string into a series of bytes.
 *
 * \param b16_str The base16 string to be decoded
 *
 * \param num_b16_chars The number of characters from the string to
 * decode, or -1 to decode the entire string.  We never decode beyond a 
 * terminating NUL character in any case.
 *
 * \retval ret_tb A newly-allocated tbuffer containing the decoded bytes.
 */
int lce_base16_decode(const char *b16_str, int num_b16_chars, tbuf **ret_tb);

/*@}*/


/* ------------------------------------------------------------------------- */
/** @name Base32 encoding
 *
 * Our base32 alphabet is the numeric digits, followed by the capital
 * alphabet, excluding four characters.  The characters to exclude
 * were chosen mainly to facilitate unambiguous reading or copying of
 * these strings by humans.  'I', 'O', and 'S' were excluded to avoid
 * visual confusion with 1, 0, and 5; and 'Z' was excluded because it
 * comes at the end (and to avoid aural confusion with 'C').
 */
/*@{*/

/**
 * Encode a series of bytes in base32.
 *
 * \param bytes The buffer to be encoded
 *
 * \param num_bytes The number of bytes to be encoded
 *
 * \param truncate_leading_zeros If 'false', we encode all of the
 * bytes, representing everything explicitly.  If 'true', we only
 * encode starting from the first nonzero bit.  This can allow a
 * smaller encoding in cases where the size of the data will be known
 * through some other means at decode time (e.g. if we know the data
 * type, and it is a fixed-size data type).  NOT YET IMPLEMENTED; THIS
 * FUNCTION ALWAYS ACTS AS IF 'FALSE' WERE SPECIFIED.
 *
 * \retval ret_b32_str The base32 string that encodes the provided bytes
 */
int lce_encode_b32(const uint8 *bytes, uint32 num_bytes,
                   tbool truncate_leading_zeros, tstring **ret_b32_str);

/**
 * Convert a single number in [0..31] into a base32 character.
 *
 * \param num A number to convert into base32.
 *
 * \return A base32 character representing the number, or the NUL character
 * if the number was not in [0..31].
 */
char lce_encode_b32_single(int num);

/**
 * Decode a base32 string into a series of bytes.
 *
 * \param b32_str The beginning of a base32 string to be decoded.
 *
 * \param num_b32_chars The number of base32 characters to decode,
 * or -1 to decode the entire string.  We never decode beyond a 
 * terminating NUL character in any case.
 *
 * \retval ret_tb A newly-allocated tbuffer containing the decoded bytes.
 */
int lce_decode_b32(const char *b32_str, int num_b32_chars, tbuf **ret_tb);

/**
 * Convert a single base32 character into a number in [0..31].
 *
 * \param b32_char A base32 character to decode.
 *
 * \return The number in [0..31] represented by the provided character,
 * or -1 if the character was not valid base32.
 */
int lce_decode_b32_single(char b32_char);

#ifdef __cplusplus
}
#endif

#endif /* __ENCODING_H_ */
