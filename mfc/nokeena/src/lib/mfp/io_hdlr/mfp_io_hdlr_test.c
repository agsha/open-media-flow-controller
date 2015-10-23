#include "mfp_io_hdlr.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>

#include <aio.h>

static void PM_Completion_Hdlr(int32_t sig_num, siginfo_t* sig_info, void* ctx);

static void AIO_Read_Completion_Hdlr(int32_t sig_num, siginfo_t* sig_info, void* ctx);

static void AIO_Write_Completion_Hdlr(int32_t sig_num, siginfo_t* sig_info, void* ctx);

int32_t main() {

	// Primitive IO Test {

	io_handler_t* io_hdlr_pm = createPM_IO_Hdlr();

	uint8_t path[100];
	strcpy((char*)path, "./test_pm.dump");
	io_open_ctx_t open_ctx;
	open_ctx.path = &(path[0]);
	open_ctx.flags = O_CREAT | O_ASYNC | O_RDWR;
	open_ctx.mode = S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;

	file_handle_t* file_hdle = io_hdlr_pm->io_open(&open_ctx);
	if (file_hdle->fd < 0) {

		printf("file open failed\n");
		goto return_exit;
	}

	io_complete_hdlr_fptr c_hdlr = PM_Completion_Hdlr;

	uint32_t itr;
	for (itr = 0; itr <= 9; itr++) {

		uint8_t* buf = malloc(1024*1024);
		memset(buf, 0, 1024*1024);
		io_complete_ctx_t* c_io_ctx = calloc(1, sizeof(io_complete_ctx_t));
		c_io_ctx->app_ctx = (void*)buf;

		int32_t rc = io_hdlr_pm->io_write(file_hdle, buf, 1, 1024*1024,
				c_hdlr, c_io_ctx); 
		if (rc < 0) {
			printf("write failed\n");
			free(c_io_ctx);
			free(buf);
			goto return_exit;
		}
	}

	// seek and tell test

	io_hdlr_pm->io_seek(file_hdle, 0, SEEK_SET);
	for (itr = 0; itr <= 9; itr++) {

		io_hdlr_pm->io_seek(file_hdle, 1024 * 1024, SEEK_CUR);
		printf("PM Tell: %u : %d\n", itr, (int32_t)io_hdlr_pm->io_tell(file_hdle));
	}

	io_hdlr_pm->io_seek(file_hdle, 0, SEEK_SET);
	for (itr = 0; itr <= 10; itr++) {

		uint8_t* buf = malloc(1024*1024);
		io_complete_ctx_t* c_io_ctx = calloc(1, sizeof(io_complete_ctx_t));
		c_io_ctx->app_ctx = (void*)buf;

		int32_t rc = io_hdlr_pm->io_read(file_hdle, buf, 1, 1024*1024,
				c_hdlr, c_io_ctx); 
		if (rc < 0) {
			perror("pm read: ");
			printf("read failed: %d\n", file_hdle->fd);
			free(c_io_ctx);
			free(buf);
			goto return_exit;
		} else if (rc == 0) {
			printf("PM read complete\n");
			printf("File size: %u : %d\n", itr, (int32_t)io_hdlr_pm->io_tell(file_hdle));
		} else
			printf("PM read size: %d\n", rc);
	}


	io_hdlr_pm->io_close(file_hdle);
	io_hdlr_pm->io_hdlr_delete(io_hdlr_pm);

	//}

	// Async IO Test {

	io_handler_t* io_hdlr_as = createAsyncIO_Hdlr();

	strcpy((char*)path, "./test_as.dump");
	io_open_ctx_t open_ctx_aio;
	open_ctx_aio.path = &(path[0]);
	open_ctx_aio.flags = O_RDWR | O_CREAT | O_ASYNC; 
	open_ctx_aio.mode = S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;

	file_handle_t* file_hdle_as = io_hdlr_as->io_open(&open_ctx_aio);
	if (file_hdle_as->fd < 0) {

		printf("file open failed\n");
		goto return_exit;
	}

	io_complete_hdlr_fptr c_hdlr_as = AIO_Write_Completion_Hdlr;

	for (itr = 0; itr <= 9; itr++) {

		uint8_t* buf = malloc(1024*1024);
		memset(buf, 0, 1024*1024);
		io_complete_ctx_t* c_io_ctx = calloc(1, sizeof(io_complete_ctx_t));
		c_io_ctx->app_ctx = (void*)buf;

		int32_t rc = io_hdlr_as->io_write(file_hdle_as, buf, 1, 1024*1024,
				c_hdlr_as, c_io_ctx); 
		if (rc < 0) {
			printf("AIO write failed\n");
			free(buf);
			goto return_exit;
		}
		sleep(1);
	}

    sleep(5); // wait for AIO to complete

	c_hdlr_as = AIO_Read_Completion_Hdlr;

	io_hdlr_as->io_seek(file_hdle_as, 0, SEEK_SET);
	for (itr = 0; itr <= 9; itr++) {

		uint8_t* buf = malloc(1024*1024);
		memset(buf, 0, 1024*1024);
		io_complete_ctx_t* c_io_ctx = calloc(1, sizeof(io_complete_ctx_t));
		c_io_ctx->app_ctx = (void*)buf;

		int32_t rc = io_hdlr_as->io_read(file_hdle_as, buf, 1, 1024*1024,
				c_hdlr_as, c_io_ctx); 
		if (rc < 0) {
			printf("AIO read failed\n");
			free(buf);
			goto return_exit;
		}
		sleep(1);
	}

	sleep(10);


	io_hdlr_as->io_close(file_hdle_as);
	io_hdlr_as->io_hdlr_delete(io_hdlr_as);

	//}


return_exit:	
	return 0;
}


static void PM_Completion_Hdlr(int32_t sig_num, siginfo_t* sig_info, void* ctx) {

	io_complete_ctx_t* c_ctx = (io_complete_ctx_t*)ctx;
	uint8_t* buf = (uint8_t*)c_ctx->app_ctx;
	free(buf);
	free(c_ctx);
	printf("PM complete\n");
}


static void AIO_Read_Completion_Hdlr(int32_t sig_num, siginfo_t* sig_info,
		void* ctx) {

	io_complete_ctx_t* c_ctx = (io_complete_ctx_t*)sig_info->si_value.sival_ptr;
	uint8_t* buf = (uint8_t*)c_ctx->app_ctx;
	struct aiocb* aio_ctx = (struct aiocb*)c_ctx->io_return_ctx;
	size_t rc = aio_return(aio_ctx);
	printf("AIO Read complete: %d\n", (int32_t)rc);
	free(buf);
	free(aio_ctx);
	free(c_ctx);
}


static void AIO_Write_Completion_Hdlr(int32_t sig_num, siginfo_t* sig_info,
		void* ctx) {

	io_complete_ctx_t* c_ctx = (io_complete_ctx_t*)sig_info->si_value.sival_ptr;
	uint8_t* buf = (uint8_t*)c_ctx->app_ctx;
	struct aiocb* aio_ctx = (struct aiocb*)c_ctx->io_return_ctx;
	size_t rc = aio_return(aio_ctx);
	printf("AIO Write complete: %d\n", (int32_t)rc);
	free(buf);
	free(aio_ctx);
	free(c_ctx);
}

