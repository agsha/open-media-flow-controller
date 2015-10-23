#include "nkn_vpe_mfu_parse.h"


#ifndef MFU_PARSE_NORMAL_ALLOC
#include "nkn_memalloc.h"
#endif


#define MFU_CMN_BOX_SIZE 8

static void initMfuCont(mfu_contnr_t* mfucont);

static void deleteMfuCont(mfu_contnr_t* mfucont);

mfu_contnr_t* parseMFUCont(uint8_t const *data) {

#ifdef MFU_PARSE_NORMAL_ALLOC
	mfu_contnr_t* mfucont = calloc(1, sizeof(mfu_contnr_t));
#else
	mfu_contnr_t* mfucont = nkn_calloc_type(1,
			sizeof(mfu_contnr_t), mod_vpe_mfp_mfu_parser);
#endif
	if (mfucont == NULL)
		return NULL;
	initMfuCont(mfucont);
	uint32_t offset = 0;
	mfucont->mfubbox = parseMfuMfubBox(data);
	if (mfucont->mfubbox == NULL)
		goto clean_return;
	if ((mfucont->mfubbox->mfub->offset_vdat == 0xffffffffffffffff) ||
			(mfucont->mfubbox->mfub->offset_adat == 0xffffffffffffffff))
		return mfucont;
	offset += mfucont->mfubbox->cmnbox->len + MFU_CMN_BOX_SIZE;
	mfucont->rawbox = parseMfuRawDescBox(data + offset);
	if (mfucont->rawbox == NULL)
		goto clean_return;
	offset += mfucont->rawbox->cmnbox->len + MFU_CMN_BOX_SIZE;
#ifdef MFU_PARSE_NORMAL_ALLOC
	mfucont->datbox =
		calloc(mfucont->rawbox->desc_count, sizeof(mfu_dat_box_t*));
#else
	mfucont->datbox = nkn_calloc_type(mfucont->rawbox->desc_count,
			sizeof(mfu_dat_box_t*), mod_vpe_mfp_mfu_parser);
#endif
	if (mfucont->datbox == NULL)
		goto clean_return;
	uint32_t i = 0;
	for (; i < mfucont->rawbox->desc_count; i++) {

		mfucont->datbox[i] = parseMfuDatBox(data + offset);
		if (mfucont->datbox[i] == NULL)
			goto clean_return;
		offset += mfucont->datbox[i]->cmnbox->len + MFU_CMN_BOX_SIZE;
	}

	return mfucont;

clean_return:
	mfucont->del(mfucont);
	return NULL;
}


static void initMfuCmnBox(mfu_cmn_box_t* cmnbox);

static void deleteMfuCmnBox(mfu_cmn_box_t* cmnbox);

mfu_cmn_box_t* parseMfuCmnBox(uint8_t const* data) {

#ifdef MFU_PARSE_NORMAL_ALLOC
	mfu_cmn_box_t* cmnbox = calloc(1, sizeof(mfu_cmn_box_t));
#else
	mfu_cmn_box_t* cmnbox = nkn_calloc_type(1,
			sizeof(mfu_cmn_box_t), mod_vpe_mfp_mfu_parser);
#endif
	if (cmnbox == NULL)
		return NULL;
	initMfuCmnBox(cmnbox);
	memcpy(&(cmnbox->name[0]), data, 4);
	cmnbox->len = ntohl(*(uint32_t*)(data + 4));

	return cmnbox;
}


static void initMfuMfubBox(mfu_mfub_box_t* mfubbox);

static void deleteMfuMfubBox(mfu_mfub_box_t* mfubbox);

static mfub_t* parseMfuMfub(uint8_t const* data);

mfu_mfub_box_t* parseMfuMfubBox(uint8_t const* data) {

#ifdef MFU_PARSE_NORMAL_ALLOC
	mfu_mfub_box_t* mfubbox = calloc(1, sizeof(mfu_mfub_box_t));
#else
	mfu_mfub_box_t* mfubbox = nkn_calloc_type(1,
			sizeof(mfu_mfub_box_t), mod_vpe_mfp_mfu_parser);
#endif
	if (mfubbox == NULL)
		return NULL;
	initMfuMfubBox(mfubbox);
	uint32_t offset = 0;
	mfubbox->cmnbox = parseMfuCmnBox(data);
	if (mfubbox->cmnbox == NULL)
		goto clean_return;
	offset += MFU_CMN_BOX_SIZE;
	mfubbox->mfub = parseMfuMfub(data + offset);
	if (mfubbox->mfub == NULL)
		goto clean_return;

	return mfubbox;

clean_return:
	mfubbox->del(mfubbox);
	return NULL;
}


static void initMfuRawDescBox(mfu_raw_desc_box_t* rawbox);

static void deleteMfuRawDescBox(mfu_raw_desc_box_t* rawbox);

static uint32_t getMfuRawDescCount(uint8_t const* data);

static mfu_rw_desc_t* parseMfuRawDesc(uint8_t const* data);

mfu_raw_desc_box_t* parseMfuRawDescBox(uint8_t const* data) {

#ifdef MFU_PARSE_NORMAL_ALLOC
	mfu_raw_desc_box_t* rawbox = calloc(1, sizeof(mfu_rw_desc_t));
#else
	mfu_raw_desc_box_t* rawbox = nkn_calloc_type(1,
			sizeof(mfu_rw_desc_t), mod_vpe_mfp_mfu_parser);
#endif
	if (rawbox == NULL)
		return NULL;
	initMfuRawDescBox(rawbox);
	uint32_t offset = 0;
	rawbox->cmnbox = parseMfuCmnBox(data);
	if (rawbox->cmnbox == NULL)
		goto clean_return;

	rawbox->desc_count = getMfuRawDescCount(data);
	offset += MFU_CMN_BOX_SIZE;

	if (rawbox->desc_count <= 0)
		goto clean_return;
#ifdef MFU_PARSE_NORMAL_ALLOC
	rawbox->descs = calloc(rawbox->desc_count, sizeof(mfu_rw_desc_t*));
#else
	rawbox->descs = nkn_calloc_type(rawbox->desc_count,
			sizeof(mfu_rw_desc_t*), mod_vpe_mfp_mfu_parser);
#endif
	if (rawbox->descs == NULL)
		goto clean_return;
	uint32_t i = 0;
	for (; i < rawbox->desc_count; i++) {

		uint32_t rwdesc_len = ntohl(*(uint32_t*)(data + offset));
		offset += 4;
		rawbox->descs[i] = parseMfuRawDesc(data + offset);
		if (rawbox->descs[i] == NULL)
			goto clean_return;
		offset += rwdesc_len;
	}

	return rawbox;

clean_return:
	rawbox->del(rawbox);
	return NULL;
}


static void initMfuDatBox(mfu_dat_box_t* datbox);

static void deleteMfuDatBox(mfu_dat_box_t* datbox);

mfu_dat_box_t* parseMfuDatBox(uint8_t const* data) {

#ifdef MFU_PARSE_NORMAL_ALLOC
	mfu_dat_box_t* datbox = calloc(1, sizeof(mfu_dat_box_t));
#else
	mfu_dat_box_t* datbox = nkn_calloc_type(1,
			sizeof(mfu_dat_box_t), mod_vpe_mfp_mfu_parser);
#endif
	if (datbox == NULL)
		return NULL;
	initMfuDatBox(datbox);
	datbox->cmnbox = parseMfuCmnBox(data);
	if (datbox->cmnbox == NULL)
		goto clean_return;
	datbox->dat = data + MFU_CMN_BOX_SIZE;

	return datbox;

clean_return:
	datbox->del(datbox);
	return NULL;
}


static void printMfuCmnBox(mfu_cmn_box_t const* cmnbox);

static void printMfuMfubBox(mfu_mfub_box_t const* mfubbox);

static void printMfuRawDescBox(mfu_raw_desc_box_t const* rawbox);

static void printMfuDatBox(mfu_dat_box_t const* datbox);


void printMFU(mfu_contnr_t const* mfuc) {

	printf("------------------------------------------------------\n");
	printMfuMfubBox(mfuc->mfubbox);
	printf("\n\n");
	printMfuRawDescBox(mfuc->rawbox);
	printf("\n\n");
	uint32_t i = 0;
	for (; i < mfuc->rawbox->desc_count; i++) {
		printMfuDatBox(mfuc->datbox[i]);
		printf("\n");
	}
	printf("------------------------------------------------------\n");
}


static void initMfuCont(mfu_contnr_t* mfucont) {

	mfucont->mfubbox = NULL;
	mfucont->rawbox = NULL;
	mfucont->datbox = NULL;
	mfucont->del = deleteMfuCont;
}


static void deleteMfuCont(mfu_contnr_t* mfucont) {

	if (mfucont->mfubbox != NULL)
		mfucont->mfubbox->del(mfucont->mfubbox);
	uint32_t desc_count = 0;
	if (mfucont->rawbox != NULL) {
		desc_count = mfucont->rawbox->desc_count;
		mfucont->rawbox->del(mfucont->rawbox);
	}
	if (mfucont->datbox != NULL) {
		uint32_t i = 0;
		for (; i < desc_count; i++)
			mfucont->datbox[i]->del(mfucont->datbox[i]);
		free(mfucont->datbox);
	}
	free(mfucont);
}


static void initMfuCmnBox(mfu_cmn_box_t* cmnbox) {

	cmnbox->len = 0;
	memset(&(cmnbox->name[0]), 0, 4);
	cmnbox->del = deleteMfuCmnBox;
}


static void deleteMfuCmnBox(mfu_cmn_box_t* cmnbox) {

	free(cmnbox);
}


static void initMfuMfubBox(mfu_mfub_box_t* mfubbox) {

	mfubbox->cmnbox = NULL;
	mfubbox->mfub = NULL;
	mfubbox->del = deleteMfuMfubBox;
}


static void deleteMfuMfub(mfub_t* mfub); 

static void deleteMfuMfubBox(mfu_mfub_box_t* mfubbox) {

	if (mfubbox->cmnbox != NULL)
		mfubbox->cmnbox->del(mfubbox->cmnbox);
	if (mfubbox->mfub != NULL)
		deleteMfuMfub(mfubbox->mfub);
	free(mfubbox);
}


static void initMfuRawDescBox(mfu_raw_desc_box_t* rawbox) {

	rawbox->cmnbox = NULL;
	rawbox->desc_count = 0;
	rawbox->descs = NULL;
	rawbox->del = deleteMfuRawDescBox;
}


static void deleteMfuRawDesc(mfu_rw_desc_t* raw_desc);

static void deleteMfuRawDescBox(mfu_raw_desc_box_t* rawbox) {

	if (rawbox->cmnbox != NULL)
		rawbox->cmnbox->del(rawbox->cmnbox);
	if (rawbox->desc_count > 0) {
		uint32_t i = 0;
		for (; i < rawbox->desc_count; i++)
			deleteMfuRawDesc(rawbox->descs[i]);
		free(rawbox->descs);
	}
	free(rawbox);
}


static void initMfuDatBox(mfu_dat_box_t* datbox) {

	datbox->cmnbox = NULL;
	datbox->dat = NULL;
	datbox->del = deleteMfuDatBox;
}


static void deleteMfuDatBox(mfu_dat_box_t* datbox) {

	if (datbox->cmnbox != NULL)
		datbox->cmnbox->del(datbox->cmnbox);
	free(datbox);
}


static mfub_t* parseMfuMfub(uint8_t const* data) {

#ifdef MFU_PARSE_NORMAL_ALLOC
	mfub_t* mfub = calloc(1, sizeof(mfub_t));
#else
	mfub_t* mfub = nkn_calloc_type(1, sizeof(mfub_t), mod_vpe_mfp_mfu_parser);
#endif
	if (mfub == NULL)
		return NULL;

	mfub_t const* t_mfub = (mfub_t const*)data;
	mfub->version = ntohs(t_mfub->version);
	mfub->program_number = ntohs(t_mfub->program_number);
	mfub->stream_id = ntohs(t_mfub->stream_id);
	mfub->flags = ntohs(t_mfub->flags);
	mfub->sequence_num = ntohl(t_mfub->sequence_num);
    mfub->timescale = ntohl(t_mfub->timescale);
    mfub->duration = ntohl(t_mfub->duration);
	mfub->audio_duration = ntohl(t_mfub->audio_duration);
    mfub->video_rate = ntohs(t_mfub->video_rate);
    mfub->audio_rate = ntohs(t_mfub->audio_rate);
    mfub->mfu_rate = ntohs(t_mfub->mfu_rate);
    mfub->video_codec_type = t_mfub->video_codec_type; 
    mfub->audio_codec_type = t_mfub->audio_codec_type; 
	mfub->compliancy_indicator_mask = ntohl(t_mfub->compliancy_indicator_mask);
	mfub->offset_vdat = nkn_vpe_swap_uint64(t_mfub->offset_vdat);
	mfub->offset_adat = nkn_vpe_swap_uint64(t_mfub->offset_adat);
	mfub->mfu_size = ntohl(t_mfub->mfu_size);

	return mfub; 
}


static void deleteMfuMfub(mfub_t* mfub) {

	free(mfub);
}


static uint32_t getMfuRawDescCount(uint8_t const* data) {

	if (strncmp((char const*)data, "rwfg", 4) != 0) {
		printf("RWFG not found: %c %c %c %c\n",
				data[0], data[1], data[2], data[3]);
		return -1;
	}
	uint32_t raw_cont_size = ntohl(*(uint32_t*)(data + 4));
	int32_t raw_count = 0;
	uint32_t offset = 8;
	while ((raw_cont_size) > 0) {

		uint32_t raw_desc_size = ntohl(*(uint32_t*)(data + offset));
		raw_cont_size -= (4 + raw_desc_size);
		offset += (4 + raw_desc_size);
		raw_count++;
	}
	return raw_count;
}


static mfu_rw_desc_t* parseMfuRawDesc(uint8_t const* data) {

#ifdef MFU_PARSE_NORMAL_ALLOC
	mfu_rw_desc_t* raw_desc = calloc(1, sizeof(mfu_rw_desc_t));
#else
	mfu_rw_desc_t* raw_desc = nkn_calloc_type(1,
			sizeof(mfu_rw_desc_t), mod_vpe_mfp_mfu_parser);
#endif
	if (raw_desc == NULL)
		return NULL;
	uint32_t offset = 0;
	raw_desc->unit_count = *(uint32_t*)(data);
	raw_desc->unit_count = ntohl(raw_desc->unit_count);
	offset += 4;
	uint32_t uc_nb = raw_desc->unit_count;
	raw_desc->timescale = *(uint32_t*)(data + offset);
	raw_desc->timescale = ntohl(raw_desc->timescale);
	offset += 4;
	raw_desc->default_unit_duration = *(uint32_t*)(data + offset);
	raw_desc->default_unit_duration = ntohl(raw_desc->default_unit_duration);
	offset += 4;
	raw_desc->default_unit_size = *(uint32_t*)(data + offset);
	raw_desc->default_unit_size = ntohl(raw_desc->default_unit_size);
	offset += 4;

	raw_desc->sample_duration = NULL;
	raw_desc->composition_duration = NULL;
	raw_desc->sample_sizes = NULL;
	if (1) {//raw_desc->default_unit_duration == 0) {
#ifdef MFU_PARSE_NORMAL_ALLOC
		raw_desc->sample_duration = calloc(uc_nb, sizeof(uint32_t));
#else
		raw_desc->sample_duration = nkn_calloc_type(uc_nb,
				sizeof(uint32_t), mod_vpe_mfp_mfu_parser);
#endif
		if (raw_desc->sample_duration == NULL)
			goto clean_return;
#ifdef MFU_PARSE_NORMAL_ALLOC
		raw_desc->composition_duration = calloc(uc_nb, sizeof(uint32_t));
#else
		raw_desc->composition_duration = nkn_calloc_type(uc_nb,
				sizeof(uint32_t), mod_vpe_mfp_mfu_parser);
#endif
		if (raw_desc->composition_duration == NULL)
			goto clean_return;
		memcpy(raw_desc->sample_duration, data + offset, uc_nb * 4);
		offset += uc_nb * 4;
		memcpy(raw_desc->composition_duration, data + offset , uc_nb * 4);
		offset += uc_nb * 4;
	}
	if (1) {//raw_desc->default_unit_size == 0) {
#ifdef MFU_PARSE_NORMAL_ALLOC
		raw_desc->sample_sizes = calloc(uc_nb, sizeof(uint32_t));
#else
		raw_desc->sample_sizes = nkn_calloc_type(uc_nb,
				sizeof(uint32_t), mod_vpe_mfp_mfu_parser);
#endif
		if (raw_desc->sample_sizes == NULL)
			goto clean_return;
		memcpy(raw_desc->sample_sizes, data + offset, uc_nb * 4);
		offset += uc_nb * 4;
	}

	raw_desc->codec_info_size = *(uint32_t*)(data + offset);
	raw_desc->codec_info_size = ntohl(raw_desc->codec_info_size);
	offset += 4;
	if (raw_desc->codec_info_size > 0)
		raw_desc->codec_specific_data = (uint8_t*)(data + offset);
	else
		raw_desc->codec_specific_data = NULL;

	uint32_t i = 0;
	for (; i < uc_nb; i++) {

	  if (1) {//raw_desc->default_unit_duration == 0) {
  	                raw_desc->sample_duration[i] = ntohl(raw_desc->sample_duration[i]);
			raw_desc->composition_duration[i] =
				ntohl(raw_desc->composition_duration[i]);
		}
	    if (1) //raw_desc->default_unit_size == 0)
	    		raw_desc->sample_sizes[i] = ntohl(raw_desc->sample_sizes[i]);
	}

	return raw_desc;

clean_return:
	deleteMfuRawDesc(raw_desc);
	return NULL;
}


static void deleteMfuRawDesc(mfu_rw_desc_t* raw_desc) {

	if (raw_desc->sample_duration != NULL)
		free(raw_desc->sample_duration);
	if (raw_desc->composition_duration != NULL)
		free(raw_desc->composition_duration);
	if (raw_desc->sample_sizes != NULL)
		free(raw_desc->sample_sizes);
	free(raw_desc);
}


static void printMfuCmnBox(mfu_cmn_box_t const* cmnbox) {

	printf("Box length: %u\n", cmnbox->len);
	printf("Box name: %c%c%c%c\n", cmnbox->name[0], cmnbox->name[1],
			cmnbox->name[2], cmnbox->name[3]);
}


static void printMfuMfub(mfub_t const* mfub);

static void printMfuMfubBox(mfu_mfub_box_t const* mfubbox) {

	printMfuCmnBox(mfubbox->cmnbox);
	printMfuMfub(mfubbox->mfub);
}


static void printMfuRawDesc(mfu_rw_desc_t const* raw_desc);

static void printMfuRawDescBox(mfu_raw_desc_box_t const* rawbox) {

	printMfuCmnBox(rawbox->cmnbox);
	uint32_t i = 0;
	for (; i < rawbox->desc_count; i++)
		printMfuRawDesc(rawbox->descs[i]);
}


static void printMfuDatBox(mfu_dat_box_t const* datbox) {

	printMfuCmnBox(datbox->cmnbox);
}


static void printMfuMfub(mfub_t const* mfub) {

	printf("\tVersion: %u\n", mfub->version);
	printf("\tProgram number: %u\n", mfub->program_number);
	printf("\tStream ID: %u\n", mfub->stream_id);
	printf("\tFlags: %u\n", mfub->flags);
	printf("\tSequence number: %u\n", mfub->sequence_num);
	printf("\tTimescale: %u\n", mfub->timescale);
	printf("\tDuration: %u\n", mfub->duration);
	printf("\tAudio duration: %u\n", mfub->audio_duration);
	printf("\tVideo rate: %u\n", mfub->video_rate);
	printf("\tAudio rate: %u\n", mfub->audio_rate);
	printf("\tMfu rate: %u\n", mfub->mfu_rate);
	printf("\tVideo codec type: %02X\n", mfub->video_codec_type);
	printf("\tAudio codec type: %02X\n", mfub->audio_codec_type);
	printf("\tCompliancy indicator mask: %u\n",
			mfub->compliancy_indicator_mask);
	printf("\tVdat offset: %lu\n", mfub->offset_vdat);
	printf("\tAdat offset: %lu\n", mfub->offset_adat);
	printf("\tMfu size: %u\n", mfub->mfu_size);
}


static void printMfuRawDesc(mfu_rw_desc_t const* raw_desc) {

	printf("\tUnit count: %u\n", raw_desc->unit_count);
	printf("\tTimescale: %u\n", raw_desc->timescale);
	printf("\tDefault unit duration: %u\n", raw_desc->default_unit_duration);
	printf("\tDefault unit size: %u\n", raw_desc->default_unit_size);
	if (raw_desc->default_unit_duration == 0) {

		printf("\tUnit duration:\n\t");
		uint32_t j = 0;
		for (; j < raw_desc->unit_count; j++) {
			printf("%u ", raw_desc->sample_duration[j]);
			if ((j + 1) % 5 == 0)
				printf("\n\t");
		}
		printf("\n\tComposition duration:\n\t");
		for (j = 0; j < raw_desc->unit_count; j++) {
			printf("%u ", raw_desc->composition_duration[j]);
			if ((j + 1) % 5 == 0)
				printf("\n\t");
		}
	}
	if (raw_desc->default_unit_size == 0) {

		printf("\n\tUnit Size:\n\t");
		uint32_t j = 0;
		for (; j < raw_desc->unit_count; j++) {
			printf("%u ", raw_desc->sample_sizes[j]);
			if ((j + 1) % 5 == 0)
				printf("\n\t");
		}
	}
	printf("\n\tCodec info size: %u\n", raw_desc->codec_info_size);
	uint32_t j = 0;
	printf("\tCodec specific data:\n\t");
	for (; j < raw_desc->codec_info_size; j++) {
		printf("%02X ", raw_desc->codec_specific_data[j]);
		if ((j + 1) % 8 == 0)
			printf("\n\t");
	}
	printf("\n");
}


#ifdef MFU_PARSER_UNIT_TEST

int32_t main(int32_t argc, char const * const *const argv) {


	if (argc < 2) {
		printf("Usage: ./mfup <file name>\n");
		return 0;
	}
	FILE* fp = fopen(argv[1], "rb");
	if (fp == NULL) {
		perror("File open failed: ");
		exit(-1);
	}
	uint32_t offset = 0;
	int32_t rc = 0;
	uint8_t* file_data = NULL;
	while (1) {

		mfub_t mfub;
		rc = fseek(fp, offset + 8, SEEK_SET);
		rc = fread((void*)&mfub, sizeof(mfub_t), 1, fp);
		if (rc <= 0) {
			break;
		}
		uint32_t mfu_size = ntohl(mfub.mfu_size);
		rc = fseek(fp, offset, SEEK_SET);
		file_data = malloc(mfu_size);
		rc = fread(file_data, mfu_size, 1, fp);
		mfu_contnr_t* mfuc = parseMFUCont(file_data);
		if (mfuc != NULL) {
			printMFU(mfuc);
			deleteMfuCont(mfuc);
		}
		free(file_data);
		offset += mfu_size;
	}
	fclose(fp);
	return 0;
}

#endif

