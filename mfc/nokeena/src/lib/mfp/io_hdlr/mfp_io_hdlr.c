#include "mfp_io_hdlr.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <aio.h>

#include "nkn_assert.h"
#include "nkn_debug.h"
#include "nkn_memalloc.h"
// Init the lib
static io_alloc_hdlr_fptr io_alloc_hdlr = NULL;

void initIO_Handlers(io_alloc_hdlr_fptr io_alloc_ptr) {

	io_alloc_hdlr = io_alloc_ptr;
}

// primitive IO functions
static  file_handle_t* pm_io_open(io_open_ctx_t const* open_ctx);

static ssize_t pm_io_read(file_handle_t const* desc_ctx, void* buf,
		uint32_t count, uint32_t size, uint32_t offset,
		io_complete_hdlr_fptr completion_hdlr,
		io_complete_ctx_t* completion_ctx);

static ssize_t pm_io_write(file_handle_t const* desc_ctx, void* buf,
		uint32_t count, uint32_t size, uint32_t offset,
		io_complete_hdlr_fptr completion_hdlr,
		io_complete_ctx_t* completion_ctx);

static ssize_t pm_io_seek(file_handle_t const* desc_ctx, ssize_t seek,
		ssize_t whence);

static ssize_t pm_io_tell(file_handle_t const* desc_ctx);

static int32_t pm_io_flush(file_handle_t const* desc_ctx);

static int32_t pm_io_advise(file_handle_t const* desc_ctx, 
		int32_t offset, int32_t len, int32_t advise);

static void pm_io_close(file_handle_t* desc_ctx);


// Async IO functions
static ssize_t async_io_read(file_handle_t const* desc_ctx, void* buf,
		uint32_t count, uint32_t size, uint32_t offset,
		io_complete_hdlr_fptr completion_hdlr,
		io_complete_ctx_t* completion_ctx);

static ssize_t async_io_write(file_handle_t const* desc_ctx, void* buf,
		uint32_t count, uint32_t size, uint32_t offset,
		io_complete_hdlr_fptr completion_hdlr,
		io_complete_ctx_t* completion_ctx);

static void ioHdlrDelete(io_handler_t* io_hdlr) {

	free(io_hdlr);
}

io_handler_t* createAIO_Hdlr(void) {

	io_handler_t* io_hdlr;
	if (io_alloc_hdlr != NULL)
		io_hdlr = io_alloc_hdlr(1, sizeof(io_handler_t));
	else
		io_hdlr = nkn_calloc_type(1, sizeof(io_handler_t), mod_vpe_io_hdlr_t);
	if (io_hdlr == NULL) {
		perror("ioh alloc: ");
		return NULL;
	}
	io_hdlr->io_open = pm_io_open;
	io_hdlr->io_read = async_io_read;
	io_hdlr->io_write = async_io_write;
	io_hdlr->io_seek = pm_io_seek;
	io_hdlr->io_tell = pm_io_tell;
	io_hdlr->io_flush = pm_io_flush; 
	io_hdlr->io_advise = pm_io_advise;
	io_hdlr->io_close = pm_io_close;

	io_hdlr->io_hdlr_delete = ioHdlrDelete;

	return io_hdlr;
}


io_handler_t* createIO_Hdlr(void) {

	io_handler_t* io_hdlr;
	if (io_alloc_hdlr != NULL)
		io_hdlr = io_alloc_hdlr(1, sizeof(io_handler_t));
	else
		io_hdlr = nkn_calloc_type(1, sizeof(io_handler_t), mod_vpe_io_hdlr_t);
	if (io_hdlr == NULL) {
		perror("ioh alloc: ");
		return NULL;
	}

	io_hdlr->io_open = pm_io_open;
	io_hdlr->io_read = pm_io_read;
	io_hdlr->io_write = pm_io_write;
	io_hdlr->io_seek = pm_io_seek;
	io_hdlr->io_tell = pm_io_tell;
	io_hdlr->io_flush = pm_io_flush; 
	io_hdlr->io_advise = pm_io_advise;
	io_hdlr->io_close = pm_io_close;

	io_hdlr->io_hdlr_delete = ioHdlrDelete;

	return io_hdlr;
}


// Primitive IO
static file_handle_t* pm_io_open(io_open_ctx_t const* open_ctx) {

	int32_t fd = open((char*)open_ctx->path, open_ctx->flags, open_ctx->mode);
	int32_t err = 0;
	
	if (fd < 0) {
	    err = errno;
	    NKN_ASSERT(0);
		DBG_MFPLOG("IOHDLR", SEVERE, MOD_MFPLIVE,
			   "Error opening file (errno=%d) with path"
			   " %s", err, open_ctx->path);
		return NULL;
	}
	file_handle_t* fh = NULL;
	if (io_alloc_hdlr != NULL)
		fh = io_alloc_hdlr(1, sizeof(file_handle_t));
	else
		fh = nkn_calloc_type(1, sizeof(file_handle_t), mod_vpe_io_hdlr_t);
	fh->fd = fd;
	return fh;
}

static ssize_t pm_io_read(file_handle_t const* desc_ctx, void* buf,
		uint32_t count, uint32_t size, uint32_t offset,
		io_complete_hdlr_fptr completion_hdlr,
		io_complete_ctx_t* completion_ctx) {

	int32_t fd = desc_ctx->fd;
	ssize_t rc = pread(fd, buf, count * size, offset);
	//Completion handler called only on success
	if (rc >=0)
		if (completion_hdlr != NULL) {
			sigval_t sig_val;
			sig_val.sival_ptr = completion_ctx;
			completion_hdlr(sig_val);
		}
	return rc;
}

static ssize_t pm_io_write(file_handle_t const* desc_ctx, void* buf,
		uint32_t count, uint32_t size, uint32_t offset,
		io_complete_hdlr_fptr completion_hdlr,
		io_complete_ctx_t* completion_ctx) {

	int32_t fd = desc_ctx->fd;
	ssize_t rc = pwrite(fd, buf, count * size, offset);
	//Completion handler called only on success if (rc >=0)
	if(rc >=0)
		if (completion_hdlr != NULL) {
			sigval_t sig_val;
			sig_val.sival_ptr = completion_ctx;
			completion_hdlr(sig_val);
		}
	return rc;
}

static ssize_t pm_io_seek(file_handle_t const* desc_ctx, ssize_t seek,
		ssize_t whence) {

	int32_t fd = desc_ctx->fd;
	ssize_t rc = lseek(fd, seek, whence);
	return rc;

}

static ssize_t pm_io_tell(file_handle_t const* desc_ctx) {

	ssize_t rc = pm_io_seek(desc_ctx, 0, SEEK_CUR);
	return rc;

}

static void pm_io_close(file_handle_t* desc_ctx) {

	int32_t fd = desc_ctx->fd;
	close(fd);
	free(desc_ctx);
}

static int32_t pm_io_flush(file_handle_t const* desc_ctx) {

	return fdatasync(desc_ctx->fd);
}

static int32_t pm_io_advise(file_handle_t const* desc_ctx, 
		int32_t offset, int32_t len, int32_t advise) {

	return posix_fadvise(desc_ctx->fd, offset, len, advise);
}


// Async IO
static ssize_t async_io_read(file_handle_t const* desc_ctx, void* buf,
		uint32_t count, uint32_t size, uint32_t offset,
		io_complete_hdlr_fptr completion_hdlr,
		io_complete_ctx_t* completion_ctx) {

	int32_t rc;
	int32_t fd = desc_ctx->fd;

	struct aiocb* aio_ctx = NULL;
	if (io_alloc_hdlr != NULL)
		aio_ctx = io_alloc_hdlr(1, sizeof(struct aiocb));
	else
		aio_ctx = nkn_calloc_type(1, sizeof(struct aiocb), mod_vpe_io_hdlr_t);

	if (aio_ctx == NULL) {
		perror("aiocb calloc");
		rc = -1;
		goto clean_return;
	}

	// setting the content
	memset(aio_ctx, 0, sizeof(struct aiocb));
	aio_ctx->aio_fildes = fd;
	aio_ctx->aio_offset = offset;
	aio_ctx->aio_buf = buf;
	aio_ctx->aio_nbytes = count * size;

	// configure the aio signal gen
	aio_ctx->aio_sigevent.sigev_notify = SIGEV_THREAD;
	aio_ctx->aio_sigevent.sigev_notify_function = completion_hdlr;
	aio_ctx->aio_sigevent.sigev_notify_attributes = NULL;
	aio_ctx->aio_sigevent.sigev_value.sival_ptr = (void*)completion_ctx;

	completion_ctx->io_return_ctx = (void*)aio_ctx;

	if (aio_read(aio_ctx) < 0) {

		perror("aio_read: ");
		rc = -1;
		completion_ctx->io_return_ctx = NULL;
		free(aio_ctx);
		goto clean_return;
	}
	rc = 1;

clean_return:
	return rc;
}


static ssize_t async_io_write(file_handle_t const* desc_ctx, void* buf,
		uint32_t count, uint32_t size, uint32_t offset,
		io_complete_hdlr_fptr completion_hdlr,
		io_complete_ctx_t* completion_ctx) {

	int32_t rc;
	int32_t fd = desc_ctx->fd;

	struct aiocb* aio_ctx = NULL;
	if (io_alloc_hdlr != NULL)
		aio_ctx = io_alloc_hdlr(1, sizeof(struct aiocb));
	else
		aio_ctx = nkn_calloc_type(1, sizeof(struct aiocb), mod_vpe_io_hdlr_t);
	if (aio_ctx == NULL) {
		perror("aiocb calloc");
		rc = -1;
		goto clean_return;
	}

	// setting the content
	memset(aio_ctx, 0, sizeof(struct aiocb));
	aio_ctx->aio_fildes = fd;
	aio_ctx->aio_offset = offset;
	aio_ctx->aio_buf = buf;
	aio_ctx->aio_nbytes = count * size;

	// configure the aio signal gen
	aio_ctx->aio_sigevent.sigev_notify = SIGEV_THREAD;
	aio_ctx->aio_sigevent.sigev_notify_function = completion_hdlr;
	aio_ctx->aio_sigevent.sigev_notify_attributes = NULL;
	aio_ctx->aio_sigevent.sigev_value.sival_ptr = (void*)completion_ctx;

	completion_ctx->io_return_ctx = (void*)aio_ctx;

	if (aio_write(aio_ctx) < 0) {

		perror("aio_write: ");
		rc = -1;
		completion_ctx->io_return_ctx = NULL;
		free(aio_ctx);
		goto clean_return;
	}
	rc = 1;

clean_return:
	return rc;
}


