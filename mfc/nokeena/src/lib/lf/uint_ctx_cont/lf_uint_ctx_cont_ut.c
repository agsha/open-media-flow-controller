#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "lf_uint_ctx_cont.h"


static void deleteCtx(void* ctx) {

	uint32_t* uint = (uint32_t*)ctx;
	printf("Delete: %u\n", *uint);
	free(ctx);
}

int32_t main(int32_t argc, char** argv) {

	uint_ctx_cont_t* uint_ctx = createUintCtxCont(100);

	uint32_t i = 0;
	for (; i <= 100; i++) {

		uint32_t* ctx = malloc(sizeof(uint32_t));
		*ctx = i;
		int32_t rc = uint_ctx->add_element_hdlr(uint_ctx, i, ctx, deleteCtx);
		if (rc < 0) {
			printf("Add failed: %d\n", i);
			free(ctx);
		}
	}
	for (i = 0; i <= 100; i++) {

		void* ctx = uint_ctx->get_element_hdlr(uint_ctx, i);
		if (ctx == NULL)
			printf("Get failed: %d\n", i);
	}
	for (i = 0; i <= 100; i++) {

		int32_t rc = uint_ctx->remove_element_hdlr(uint_ctx, i);
		if (rc < 0)
			printf("Remove failed: %d\n", i);
	}
	uint_ctx->delete_hdlr(uint_ctx);

	return 0;
}
