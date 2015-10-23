/*
 *
 * ftrans_wget.h
 *
 *
 *
 */

#ifndef __FTRANS_WGET_H_
#define __FTRANS_WGET_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

#include "common.h"
#include "tstring.h"

#ifndef PROD_FEATURE_WGET
#error Cannot include ftrans_wget.h without PROD_FEATURE_WGET
#endif

/* ------------------------------------------------------------------------- */
/** \file ftrans_wget.h Utilities for transfering files via wget
 * \ingroup lc
 */

/**
 * Upload/download a file using wget via HTTP/FTP.
 *
 * \param remote_url URL on remote system
 * \param overwrite Whether the destination file should overwrite if it exists
 * \param had_filename
 * \param dest_dir What directory to use for the operation
 * \param dest_filename
 * \param dest_path
 */
int lft_wget_transfer_file(const char *remote_url, tbool overwrite,
                           tbool had_filename, tstring *dest_dir,
                           tstring *dest_filename, tstring *dest_path);

#ifdef __cplusplus
}
#endif

#endif /* __FTRANS_WGET_H_ */
