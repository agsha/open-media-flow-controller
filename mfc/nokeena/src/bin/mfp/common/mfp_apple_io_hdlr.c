#include <stdlib.h>
#include <string.h>
#include <aio.h>

#include "mfp_apple_io_hdlr.h"

#include "nkn_memalloc.h"
#include "nkn_debug.h"

void Apple_WriteNewFile(io_handler_t const* io_hdlr,
		io_complete_hdlr_fptr io_end_hdlr, file_handle_t const* fh,
		char* data, uint32_t len, uint32_t offset, ref_count_mem_t* ref_cont) {

	io_complete_ctx_t* io_ctx = nkn_calloc_type(1,
			sizeof(io_complete_ctx_t), mod_mfp_apple_io);
	io_ref_ctx_t* io_ref = nkn_calloc_type(1, sizeof(io_ref_ctx_t),
			mod_mfp_apple_io);
	io_ref->ctx = data;
	io_ref->ref_cont = ref_cont;
	io_ctx->app_ctx = io_ref;

	ref_cont->hold_ref_cont(ref_cont);
	if (io_hdlr->io_write(fh, data, len, 1, offset,
				io_end_hdlr, io_ctx) < 0) {
		io_ref_ctx_t* io_ref_ctx = (io_ref_ctx_t*)io_ctx->app_ctx;
		free(io_ref_ctx->ctx);
		io_ref_ctx->ref_cont->release_ref_cont(io_ref_ctx->ref_cont);
		free(io_ref_ctx);
		if (io_ctx->io_return_ctx != NULL)
			free(io_ctx->io_return_ctx);
		free(io_ctx);
	}
}


void AppleIO_CompleteHdlr(sigval_t sig_val) {

	io_complete_ctx_t* c_ctx = (io_complete_ctx_t*)sig_val.sival_ptr;
	io_ref_ctx_t* io_ref = (io_ref_ctx_t*)c_ctx->app_ctx;
	ref_count_mem_t* ref_cont = (ref_count_mem_t*)io_ref->ref_cont;

	free(io_ref->ctx);
	free(io_ref);
	free(c_ctx);
	if (ref_cont != NULL)
		ref_cont->release_ref_cont(ref_cont);
}


void AppleAIO_CompleteHdlr(sigval_t sig_val) {

	io_complete_ctx_t* c_ctx = (io_complete_ctx_t*)sig_val.sival_ptr;
	io_ref_ctx_t* io_ref = (io_ref_ctx_t*)c_ctx->app_ctx;
	ref_count_mem_t* ref_cont = (ref_count_mem_t*)io_ref->ref_cont;
	struct aiocb* aio_ctx = (struct aiocb*)c_ctx->io_return_ctx;

	int32_t err = aio_error(aio_ctx);
	if (err != 0) {
		perror("aio error: ");
		DBG_MFPLOG("AIO",SEVERE, MOD_MFPLIVE, "IO ERROR : %d", err);
		if (err == 28)
			DBG_MFPLOG("AIO",SEVERE, MOD_MFPLIVE, "IO ERROR: NO DISK SPACE");
	}
	ssize_t rc = aio_return(aio_ctx);
	free(aio_ctx);
	free(io_ref->ctx);
	free(io_ref);
	free(c_ctx);
	if (ref_cont != NULL)
		ref_cont->release_ref_cont(ref_cont);
}


void deleteAIO_RefCtx(void* ctx) {

	file_handle_t* fh = (file_handle_t*)ctx;
	io_handler_t* io_hdlr = createAIO_Hdlr();
	io_hdlr->io_flush(fh);
	io_hdlr->io_advise(fh, 0, 0, POSIX_FADV_DONTNEED);
	io_hdlr->io_close(fh);
	io_hdlr->io_hdlr_delete(io_hdlr);
}


void deleteIO_RefCtx(void* ctx) {

	file_handle_t* fh = (file_handle_t*)ctx;
	io_handler_t* io_hdlr = createIO_Hdlr();
	io_hdlr->io_flush(fh);
	io_hdlr->io_advise(fh, 0, 0, POSIX_FADV_DONTNEED);
	io_hdlr->io_close(fh);
	io_hdlr->io_hdlr_delete(io_hdlr);
}

