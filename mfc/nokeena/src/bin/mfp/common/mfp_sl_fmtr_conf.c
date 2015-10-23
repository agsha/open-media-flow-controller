#include "mfp_sl_fmtr_conf.h"
#include "mfp_safe_io.h"
#include "nkn_vpe_types.h"

static io_handlers_t sio = {
	.ioh_open = sio_fopen,
	.ioh_read = sio_fread,
	.ioh_tell = sio_ftell,
	.ioh_seek = sio_fseek,
	.ioh_close = sio_fclose,
	.ioh_write = sio_fwrite
};

int32_t SL_IOW_Handler(void* ctx, uint8_t const* file_path,
		uint8_t const* buf, uint32_t len) {

	if (ctx == NULL)
		return -1;
	void* fid = NULL;
	io_handlers_t *ioh = &sio;
	mfp_safe_io_ctx_t *sioc = (mfp_safe_io_ctx_t*)ctx; 
	fid = ioh->ioh_open((char*)file_path, "wb", len);
	if (fid == NULL)
		return -1;
	mfp_safe_io_attach((mfp_safe_io_desc_t *)fid,
			(mfp_safe_io_ctx_t *)sioc);
	int32_t rc = mfp_safe_io_reserve_size((mfp_safe_io_desc_t *)fid, len);
	if (rc >= 0)
		ioh->ioh_write((char*)buf, len, 1, fid);
	ioh->ioh_close(fid);
	return rc;
}

int32_t SL_IOD_Handler(void* ctx, uint8_t const* file_path) {

	if (ctx == NULL)
		return -1;

	io_handlers_t *ioh = &sio;
	mfp_safe_io_ctx_t *sioc = (mfp_safe_io_ctx_t*)ctx; 
	void* fid = ioh->ioh_open((char*)file_path, "rb", 0);
	if (fid == NULL)
		return -1;
	uint32_t size = 0;
	int32_t rc = -1;
	if (ioh->ioh_seek(fid, 0, SEEK_END) != 0) {
		goto close_exit;
	}
	size = ioh->ioh_tell(fid);
	mfp_safe_io_attach((mfp_safe_io_desc_t*)fid, sioc);
	if (remove((char const*)file_path) == 0)
		rc = mfp_safe_io_return_size((mfp_safe_io_desc_t*)fid, size);
close_exit:
	ioh->ioh_close(fid);
	return rc;
}
