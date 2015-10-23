#ifndef _NKN_VPE_MFU_PARSE_H_
#define _NKN_VPE_MFU_PARSE_H_

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include "nkn_vpe_mfu_defs.h"
#include "nkn_vpe_types.h"

#ifdef __cplusplus
extern "C" {
#endif

	//MFU parser utility declaration and MFU container definition

	struct mfu_contnr;
	struct mfu_cmn_box;
	struct mfu_mfub_box;
	struct mfu_raw_desc_box;
	struct mfu_dat_box;

	typedef void (*deleteMfuParser_fptr) (struct mfu_contnr* mfu_contnr);

	typedef void (*deleteMfuCmnBox_fptr)(struct mfu_cmn_box* cmnbox);
	
	typedef void (*deleteMfuMfubBox_fptr)(struct mfu_mfub_box* mfubbox);
	
	typedef void (*deleteMfuRawDescBox_fptr)(struct mfu_raw_desc_box* rawbox);
	
	typedef void (*deleteMfuDatBox_fptr)(struct mfu_dat_box* datbox);


	typedef struct mfu_cmn_box {

		uint32_t len;
		char name[4];

		deleteMfuCmnBox_fptr del;
	} mfu_cmn_box_t;

	typedef struct mfu_mfub_box {

		mfu_cmn_box_t* cmnbox;
		mfub_t* mfub;

		deleteMfuMfubBox_fptr del;
	} mfu_mfub_box_t;

	typedef struct mfu_raw_desc_box {

		mfu_cmn_box_t* cmnbox;
		uint32_t desc_count;
		mfu_rw_desc_t** descs;

		deleteMfuRawDescBox_fptr del;
	} mfu_raw_desc_box_t;

	typedef struct mfu_dat_box {

		mfu_cmn_box_t* cmnbox;
		uint8_t const* dat;

		deleteMfuDatBox_fptr del;
	} mfu_dat_box_t;

	typedef struct mfu_contnr {

		mfu_mfub_box_t* mfubbox;
		mfu_raw_desc_box_t* rawbox;
		mfu_dat_box_t** datbox;

		deleteMfuParser_fptr del;
	} mfu_contnr_t;


	mfu_contnr_t* parseMFUCont(uint8_t const* mfu_data);

	mfu_cmn_box_t* parseMfuCmnBox(uint8_t const* data);

	mfu_mfub_box_t* parseMfuMfubBox(uint8_t const* data);

	mfu_raw_desc_box_t* parseMfuRawDescBox(uint8_t const* data);

	mfu_dat_box_t* parseMfuDatBox(uint8_t const * data);

	void printMFU(mfu_contnr_t const* mfuc); 

#ifdef __cplusplus
}
#endif

#endif

