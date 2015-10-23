#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "lf_uint_ctx_cont.h"


static void delete(uint_ctx_cont_t* cont);
static int32_t addElement(uint_ctx_cont_t* cont, uint32_t uint_key, void* ctx, 
		uint_ctx_element_delete_fptr ctx_delete_hdlr);
static void* acquireElement(struct uint_ctx_cont* cont, uint32_t uint_key);
static void* getElement(struct uint_ctx_cont* cont, uint32_t uint_key);
static int32_t releaseElement(uint_ctx_cont_t* cont, uint32_t uint_key);
static int32_t removeElement(uint_ctx_cont_t* cont, uint32_t uint_key);
static int32_t lockElement(uint_ctx_cont_t* cont, uint32_t uint_key);
static int32_t unlockElement(uint_ctx_cont_t* cont, uint32_t uint_key);


uint_ctx_cont_t* createUintCtxCont(uint32_t max_count) {

	if (max_count == 0)
		return NULL;
	uint_ctx_cont_t* cont = calloc(1, sizeof(uint_ctx_cont_t));
	if (cont == NULL)
		return NULL;
	cont->ctx_arr = calloc(max_count, sizeof(void**));
	if (cont->ctx_arr == NULL) {

		free(cont);
		return NULL;
	}
	cont->ctx_delete_hdlr_arr = calloc(max_count,
			sizeof(uint_ctx_element_delete_fptr));
	if (cont->ctx_delete_hdlr_arr == NULL) {

		free(cont->ctx_arr);
		free(cont);
		return NULL;
	}
	cont->element_locks = calloc(max_count, sizeof(pthread_mutex_t));
	if (cont->element_locks == NULL) {

		free(cont->ctx_delete_hdlr_arr);
		free(cont->ctx_arr);
		free(cont);
		return NULL;
	}

	cont->max_count = max_count;
	uint32_t i = 0;
	for (; i < max_count; i++) {
		cont->ctx_arr[i] = NULL;
		pthread_mutex_init(&cont->element_locks[i], NULL);
	}

	cont->get_element_hdlr = getElement;
	cont->add_element_hdlr = addElement;
	cont->acquire_element_hdlr = acquireElement;
	cont->release_element_hdlr = releaseElement;
	cont->remove_element_hdlr = removeElement;
	cont->delete_hdlr = delete; 

	return cont;
}


static void delete(uint_ctx_cont_t* cont) {

	uint32_t i = 0;
	for (; i < cont->max_count; i++)
		if (cont->ctx_arr[i] != NULL)
			cont->ctx_delete_hdlr_arr[i](cont->ctx_arr[i]);
	free(cont->ctx_arr);
	free(cont->ctx_delete_hdlr_arr);
	free(cont);
}


static int32_t addElement(uint_ctx_cont_t* cont, uint32_t uint_key, void* ctx, 
		uint_ctx_element_delete_fptr ctx_delete_hdlr) {

	if (uint_key >= cont->max_count)
		return -1;
	if (lockElement(cont, uint_key) != 0)
		return -2;
	if (cont->ctx_arr[uint_key] != NULL) {
		unlockElement(cont, uint_key);
		return -2;
	}
	cont->ctx_arr[uint_key] = ctx;
	cont->ctx_delete_hdlr_arr[uint_key] = ctx_delete_hdlr;
	unlockElement(cont, uint_key);
	return 0;
}


static void* getElement(struct uint_ctx_cont* cont, uint32_t uint_key) {

	if (uint_key >= cont->max_count)
		return NULL;
	return cont->ctx_arr[uint_key];
}


static void* acquireElement(struct uint_ctx_cont* cont, uint32_t uint_key) {

	if (uint_key >= cont->max_count)
		return NULL;
	if (lockElement(cont, uint_key) != 0)
		return NULL;
	return cont->ctx_arr[uint_key];
}


static int32_t releaseElement(struct uint_ctx_cont* cont, uint32_t uint_key) {

	if (uint_key >= cont->max_count)
		return -1;
	if (unlockElement(cont, uint_key) != 0)
		return -2;
	return 0; 
}


static int32_t removeElement(uint_ctx_cont_t* cont, uint32_t uint_key) {

	if (uint_key >= cont->max_count)
		return -1;
	if (lockElement(cont, uint_key) != 0)
		return -2;
	if (cont->ctx_arr[uint_key] == NULL) {
		unlockElement(cont, uint_key);
		return -3;
	}
	if (cont->ctx_delete_hdlr_arr[uint_key] != NULL)
		cont->ctx_delete_hdlr_arr[uint_key](cont->ctx_arr[uint_key]);
	cont->ctx_arr[uint_key] = NULL;
	cont->ctx_delete_hdlr_arr[uint_key] = NULL;
	unlockElement(cont, uint_key);
	return 0;
}


static int32_t lockElement(uint_ctx_cont_t* cont, uint32_t uint_key) {

	if (uint_key >= cont->max_count)
		return -1;
	return pthread_mutex_lock(&cont->element_locks[uint_key]);
}


static int32_t unlockElement(uint_ctx_cont_t* cont, uint32_t uint_key) {

	if (uint_key >= cont->max_count)
		return -1;
	return pthread_mutex_unlock(&cont->element_locks[uint_key]);
}

