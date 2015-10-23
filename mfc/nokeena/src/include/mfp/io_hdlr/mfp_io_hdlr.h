#ifndef MFP_IO_HDLR_H
#define MFP_IO_HDLR_H

#include <stdint.h>
#include <unistd.h>
#include <signal.h>

#ifdef __cplusplus
extern "C" {
#endif

	// Union to hold different types of IO Handlers
	typedef union {
		// ... Add FILE, Safe IO
		int32_t fd;
	} file_handle_t;


	typedef void (*io_complete_hdlr_fptr)(sigval_t sig_val);


	typedef struct io_complete_ctx {

		file_handle_t* fh;
		void* app_ctx;
		void* io_return_ctx;// required only by aio
	} io_complete_ctx_t;

	typedef struct io_open_ctx {

		uint8_t const* path;
		uint32_t flags;
		uint32_t mode;
	} io_open_ctx_t;


	// IO Handlers interface definition
	typedef file_handle_t* (*io_open_fptr)(io_open_ctx_t const* open_ctx);

	typedef ssize_t (*io_read_fptr)(file_handle_t const* desc_ctx, void* buf,
			uint32_t count, uint32_t size, uint32_t offset, 
			io_complete_hdlr_fptr io_complete_hdlr, 
			io_complete_ctx_t* completion_ctx);

	typedef ssize_t (*io_write_fptr)(file_handle_t const* desc_ctx, void* buf,
			uint32_t count, uint32_t size, uint32_t offset, 
			io_complete_hdlr_fptr io_complete_hdlr, 
			io_complete_ctx_t* completion_ctx);

	typedef ssize_t (*io_seek_fptr)(file_handle_t const* desc_ctx, ssize_t seek,
			ssize_t whence);

	typedef ssize_t (*io_tell_fptr)(file_handle_t const* desc_ctx);

	typedef int32_t (*io_flush_fptr)(file_handle_t const* desc_ctx);

	typedef int32_t (*io_advise_fptr)(file_handle_t const* desc_ctx,
			int32_t offset, int32_t len, int32_t flags);

	typedef void (*io_close_fptr)(file_handle_t* desc_ctx);


	struct io_handler;

	typedef void (*io_hdlr_delete_fptr)(struct io_handler*);

	// Container to collect the Handlers of IO operations
	typedef struct io_handler {

		io_open_fptr io_open;
		io_read_fptr io_read;
		io_write_fptr io_write;
		io_seek_fptr io_seek;
		io_tell_fptr io_tell;
		io_flush_fptr io_flush;
		io_advise_fptr io_advise;
		io_close_fptr io_close;

		io_hdlr_delete_fptr io_hdlr_delete;
	    
	} io_handler_t;


	// Alloc function to be used
	typedef void* (*io_alloc_hdlr_fptr)(uint32_t count, uint32_t size);

	// Init the IO Handler Library : configuring the alloc to use
	void initIO_Handlers(io_alloc_hdlr_fptr io_alloc_hdlr);


	io_handler_t* createAIO_Hdlr(void);

	io_handler_t* createIO_Hdlr(void);

#ifdef __cplusplus
}
#endif
#endif

