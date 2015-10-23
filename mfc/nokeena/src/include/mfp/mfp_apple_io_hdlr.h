#ifndef MFP_APPLE_IO_HDLR_H
#define MFP_APPLE_IO_HDLR_H

#include "mfp_ref_count_mem.h"
#include "io_hdlr/mfp_io_hdlr.h"

typedef struct io_ref_ctx {

	char* ctx;
	ref_count_mem_t* ref_cont;
} io_ref_ctx_t;

void Apple_WriteNewFile(io_handler_t const* io_hdlr,
		io_complete_hdlr_fptr io_end_hdlr, file_handle_t const* fh,
		char* data, uint32_t len, uint32_t offset, ref_count_mem_t* ref_cont); 

void AppleIO_CompleteHdlr(sigval_t sig_val);

void AppleAIO_CompleteHdlr(sigval_t sig_val);

void deleteAIO_RefCtx(void* ctx);

void deleteIO_RefCtx(void* ctx);


#endif
