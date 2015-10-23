#include "kms/mfp_kms_client.h"
#include "kms/mfp_kms_lib_cmn.h"
#include "mfp_err.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <assert.h>
#include <sys/time.h>
#include <unistd.h>
#include <pthread.h>

static int32_t initVmxKMS(kms_client_t* kms_client_ctx);

static kms_key_resp_t* VmxKMS_GetNextKey(kms_client_t* kms_client_ctx);

static void VmxKMS_Delete(kms_client_t* kms_client_ctx);

static struct in_addr* convertToSockAddr(char const* kms_addr);


typedef struct crlf_resp {
	uint8_t* resp;
	uint32_t resp_len;
	uint8_t* content;
	uint32_t content_len;
} crlf_resp_t;

static crlf_resp_t* recvCRLF_Resp(int32_t sd);

static int32_t getCRLF_RespCode(crlf_resp_t const* resp);

static int32_t crlfGetContentLength(uint8_t* buf);

static uint8_t* getCRLF_Location(crlf_resp_t const* resp);

static int32_t connectTimeout(int32_t sd, struct sockaddr_in const* addr_ctx,
		uint32_t seconds);

kms_client_t* createVerimatrixKMSClient(char const* kms_addr,
		uint16_t kms_port, uint32_t channel_id,
		uint32_t key_chain_length, kms_sess_type_t sess_type,
		uint8_t const* key_store_loc,
		uint8_t const* key_delivery_base_uri) {

	if ((key_store_loc == NULL) && (key_delivery_base_uri != NULL))
		return NULL;
	if ((key_store_loc != NULL) && (key_delivery_base_uri == NULL))
		return NULL;

	kms_client_t* kms_client_ctx = kms_calloc(1, sizeof(kms_client_t));
	if (kms_client_ctx == NULL)
		return NULL;
	kms_client_ctx->errcode = 0;

	kms_client_ctx->get_next_key = VmxKMS_GetNextKey;
	kms_client_ctx->delete_client = VmxKMS_Delete;

	kms_client_ctx->init_flag = 0;
	kms_client_ctx->channel_id = channel_id;
	kms_client_ctx->key_chain_length = key_chain_length;
	kms_client_ctx->sess_type = sess_type;
	if (sess_type == KMS_VOD)
		kms_client_ctx->sess_pos.pos = 0;
	else
		kms_client_ctx->sess_pos.time = 0;
	kms_client_ctx->key_store_loc = NULL;
	kms_client_ctx->key_delivery_base_uri = NULL;
	if (key_store_loc != NULL) {
		kms_client_ctx->key_delivery_type = LOCAL;
		uint32_t key_store_loc_len = strlen((char*)key_store_loc);
		uint32_t key_delivery_base_uri_len =
			strlen((char*)key_delivery_base_uri);
		kms_client_ctx->key_store_loc = kms_calloc(key_store_loc_len + 1, 1);
		kms_client_ctx->key_delivery_base_uri = kms_calloc(
				key_delivery_base_uri_len + 1, 1);
		if ((kms_client_ctx->key_store_loc == NULL) ||
				(kms_client_ctx->key_delivery_base_uri == NULL)) {
			kms_client_ctx->delete_client(kms_client_ctx);
			return NULL;
		}
		strncpy((char*)kms_client_ctx->key_store_loc, (char*)key_store_loc,
				key_store_loc_len + 1);
		strncpy((char*)kms_client_ctx->key_delivery_base_uri,
				(char*)key_delivery_base_uri,
				key_delivery_base_uri_len + 1);
	} else
		kms_client_ctx->key_delivery_type = USE_KMS;

	kms_client_ctx->conn_ctx.sd = -1;
	struct in_addr* addr = convertToSockAddr(kms_addr);
	if (addr == NULL) {
		kms_client_ctx->delete_client(kms_client_ctx);
		return NULL;
	}
	memset(&kms_client_ctx->key_src.addr_ctx, 0, sizeof(struct sockaddr_in));
	kms_client_ctx->key_src.addr_ctx.sin_family = AF_INET;
	kms_client_ctx->key_src.addr_ctx.sin_port = htons(kms_port);
	kms_client_ctx->key_src.addr_ctx.sin_addr.s_addr = addr[0].s_addr;
	free(addr);
	return kms_client_ctx;
}


static int32_t initVmxKMS(kms_client_t* kms_client_ctx) {

	kms_client_ctx->conn_ctx.sd = socket(AF_INET, SOCK_STREAM, 0);
	if (kms_client_ctx->conn_ctx.sd < 0) {
		perror("socket init: kms vmx client: ");
		kms_client_ctx->errcode = E_MFP_KMS_VMX_SOCK_FAIL;
		return -1; 
	}

	int32_t rc = connectTimeout(kms_client_ctx->conn_ctx.sd, 
			&kms_client_ctx->key_src.addr_ctx, 8);
	if (rc < 0) {
		kms_client_ctx->errcode = rc; 
		return -1; 
	}

	// prepare init HTTP content
	char req_buf[1024];
	char const* fmt =
		"POST /CAB/keyfile?r=%u&t=%s&c=%u HTTP/1.1\r\nHost: %s\r\n\r\n";
	char type[4] = "VOD";
	if (kms_client_ctx->sess_type == KMS_DTV) {
		type[0] = 'D'; type[1] = 'T'; type[2] = 'V';
	}
	type[3] = '\0';

	char host_str[INET_ADDRSTRLEN + 1];
	if (inet_ntop(AF_INET, &(kms_client_ctx->key_src.addr_ctx.sin_addr), 
				&host_str[0], INET_ADDRSTRLEN) ==  NULL) {
                kms_client_ctx->errcode = E_MFP_KMS_VMX_ADDR_FAIL;
		return -1;
	}

	int32_t len = snprintf(&req_buf[0], 1024, fmt, kms_client_ctx->channel_id,
			&type[0], kms_client_ctx->key_chain_length, &host_str[0]);

	// is a non blocking socket
	rc = send(kms_client_ctx->conn_ctx.sd, &req_buf[0], len, 0);
	if (rc < 0) {
		perror("send: kms vmx client: ");
		kms_client_ctx->errcode = E_MFP_KMS_VMX_SEND_FAIL;
		return -1;
	}
	assert(rc == len);
	//printf("Request:\n%s\n", &req_buf[0]);

	crlf_resp_t* ret = recvCRLF_Resp(kms_client_ctx->conn_ctx.sd);
	if (ret == NULL) {
	        kms_client_ctx->errcode = E_MFP_KMS_VMX_RCV_FAIL;
		return -1;
	}
	//printf("Response:\n%s\n", ret->resp);
	int32_t resp_code = getCRLF_RespCode(ret);
	if (resp_code != 200) {
    	        kms_client_ctx->errcode = E_MFP_KMS_VMX_INVALID_RESPCODE;
		rc = -1;
	} else {
		rc = 1;
	}

	if (ret->resp != NULL)
		free(ret->resp);
	if (ret->content != NULL)
		free(ret->content);
	free(ret);
	close(kms_client_ctx->conn_ctx.sd);

	return rc;
}


static kms_key_resp_t* VmxKMS_GetNextKey(kms_client_t* kms_client_ctx) {

	kms_key_resp_t* k_resp = NULL;
	if (kms_client_ctx->init_flag == 0) {
		if (initVmxKMS(kms_client_ctx) < 0)
			goto clean_return; 
		kms_client_ctx->init_flag = 1;
	}

	kms_client_ctx->conn_ctx.sd = socket(AF_INET, SOCK_STREAM, 0);
	if (kms_client_ctx->conn_ctx.sd < 0) {
		perror("socket init: kms vmx client: ");
		kms_client_ctx->errcode = E_MFP_KMS_VMX_SOCK_FAIL;
		return NULL; 
	}
	int32_t rc = connectTimeout(kms_client_ctx->conn_ctx.sd,
			&kms_client_ctx->key_src.addr_ctx, 8);
	if (rc < 0) {
		kms_client_ctx->errcode = rc; 
		return NULL; 
	}

	char req_buf[1024];
	char const* fmt = "GET /CAB/keyfile?r=%u&t=%s&p=%lu HTTP/1.1\r\n"
		"Host: %s\r\n\r\n";
	char type[4];
	uint64_t kpos = 0;
	if (kms_client_ctx->sess_type == KMS_VOD) {
		type[0] = 'V'; type[1] = 'O'; type[2] = 'D';
		kpos = kms_client_ctx->sess_pos.pos % kms_client_ctx->key_chain_length;
		kms_client_ctx->sess_pos.pos++;
	} else {
		type[0] = 'D'; type[1] = 'T'; type[2] = 'V';
		kpos = kms_client_ctx->sess_pos.time;
		struct timeval now;
		gettimeofday(&now, NULL);
		kms_client_ctx->sess_pos.time = now.tv_sec;
	}
	type[3] = '\0';

	char host_str[INET_ADDRSTRLEN + 1];
	if (inet_ntop(AF_INET, &(kms_client_ctx->key_src.addr_ctx.sin_addr),
				&host_str[0], INET_ADDRSTRLEN) == NULL) {
    	        kms_client_ctx->errcode = E_MFP_KMS_VMX_ADDR_FAIL;
		goto clean_return;
	}

	int32_t len = snprintf(&req_buf[0], 1024, fmt, kms_client_ctx->channel_id,
			&type[0], kpos, &host_str[0]);

	//printf("Request:\n%s\n", &req_buf[0]);
	// It is a non blocking socket
	rc = send(kms_client_ctx->conn_ctx.sd, &req_buf[0], len, 0);
	if (rc < 0) {
		perror("send : kms vmx client : ");
		kms_client_ctx->errcode = E_MFP_KMS_VMX_SEND_FAIL;
		goto clean_return;
	}
	crlf_resp_t* ret = recvCRLF_Resp(kms_client_ctx->conn_ctx.sd);
	if (ret == NULL) {
		kms_client_ctx->errcode = E_MFP_KMS_VMX_RCV_FAIL;
		goto clean_return;
	}
	//printf("Response:\n%s\n", ret->resp); 
	int32_t resp_code = getCRLF_RespCode(ret);
	if (resp_code == 200) {

		uint8_t* key_base_uri = getCRLF_Location(ret);
		uint8_t key_delivery_uri[512];
		if (kms_client_ctx->key_delivery_type == USE_KMS) {

			char const* key_apnd_fmt = "%s?r=%u&t=%s&p=%lu";
			uint32_t max_key_uri = strlen((char*)key_base_uri) + 128;
			if (max_key_uri >= 512)
				assert(0);
			snprintf((char*)key_delivery_uri, max_key_uri, key_apnd_fmt,
					key_base_uri, kms_client_ctx->channel_id, &type[0], kpos);
		} else {
			char const* key_apnd_fmt = "%s/key_%u_t_%s_p_%lu";
			uint32_t max_key_uri = strlen(
					(char*)kms_client_ctx->key_delivery_base_uri) + 128;
			if (max_key_uri >= 512)
				assert(0);
			snprintf((char*)key_delivery_uri, 512, key_apnd_fmt,
					kms_client_ctx->key_delivery_base_uri,
					kms_client_ctx->channel_id, &type[0], kpos);

			uint8_t key_store_loc[512];
			max_key_uri = strlen((char*)kms_client_ctx->key_store_loc) + 128;
			if (max_key_uri >= 512)
				assert(0);
			snprintf((char*)key_store_loc, 512, key_apnd_fmt,
					kms_client_ctx->key_store_loc,
					kms_client_ctx->channel_id, &type[0], kpos);

			FILE* fp = fopen((char*)key_store_loc, "wb");
			fwrite(ret->content, ret->content_len, 1, fp);
			fclose(fp);
		}
		free(key_base_uri);

		k_resp = createKmsKeyResponse(ret->content, ret->content_len,
					      &key_delivery_uri[0], 1,
					      0);
	} else {
    	        kms_client_ctx->errcode = E_MFP_KMS_VMX_INVALID_RESPCODE;
	}
	if (ret != NULL) {
		free(ret->resp);
		if (ret->content != NULL)
			free(ret->content);
		free(ret);
	}
clean_return:
	close(kms_client_ctx->conn_ctx.sd);
	return k_resp;
}


static void VmxKMS_Delete(kms_client_t* kms_client_ctx) {

    goto skip_key_chain_del;
	kms_client_ctx->conn_ctx.sd = socket(AF_INET, SOCK_STREAM, 0);
	if (kms_client_ctx->conn_ctx.sd < 0) {
		perror("socket init: kms vmx client: ");
		return;
	}
	int32_t rc = connectTimeout(kms_client_ctx->conn_ctx.sd,
			&kms_client_ctx->key_src.addr_ctx, 8);
	if (rc < 0) {
		return; 
	}

	char req_buf[1024];
	char const* fmt = "DELETE /CAB/keyfile?r=%u&t=%s HTTP/1.1\r\n"
		"Host: %s\r\n\r\n";
	char type[4] = "VOD";
	if (kms_client_ctx->sess_type == KMS_DTV) {
		type[0] = 'D'; type[1] = 'T'; type[2] = 'V';
	}
	type[3] = '\0';

	char host_str[INET_ADDRSTRLEN + 1];
	if (inet_ntop(AF_INET, &(kms_client_ctx->key_src.addr_ctx.sin_addr),
				&host_str[0], INET_ADDRSTRLEN) == NULL) {
		printf("inet_ntop failed\n");
		return;
	}

	int32_t len = snprintf(&req_buf[0], 1024, fmt, kms_client_ctx->channel_id,
			&type[0], &host_str[0]);

	//printf("Request:\n%s\n", &req_buf[0]);
	// It is a non blocking socket
	rc = send(kms_client_ctx->conn_ctx.sd, &req_buf[0], len, 0);
	if (rc < 0) {

		perror("send : kms vmx client: ");
		printf("send failed\n");
		return;
	}
	crlf_resp_t* ret = recvCRLF_Resp(kms_client_ctx->conn_ctx.sd);
	if (ret == NULL) {
		printf("recvCRLF_Resp failed\n");
		return;
	}
	//printf("Response:\n%s\n", ret->resp);
	int32_t resp_code = getCRLF_RespCode(ret);
	if (resp_code != 200)
		printf("getCRLF_RespCode not 200: %d\n", resp_code);

	if (ret->resp != NULL)
		free(ret->resp);
	if (ret->content != NULL)
		free(ret->content);
	free(ret);

	if (kms_client_ctx->conn_ctx.sd >= 0)
		close(kms_client_ctx->conn_ctx.sd);

 skip_key_chain_del:
	if (kms_client_ctx->key_store_loc != NULL)
		free(kms_client_ctx->key_store_loc);
	if (kms_client_ctx->key_delivery_base_uri != NULL)
		free(kms_client_ctx->key_delivery_base_uri);
	free(kms_client_ctx);
}


static struct in_addr* convertToSockAddr(char const* kms_addr) {

	struct in_addr *addr = NULL, tmp;

	if (inet_aton(kms_addr, &tmp) == 0) {

		addr = kms_calloc(1, sizeof(struct in_addr));
		if (addr == NULL)
			return NULL;	
		addr->s_addr = tmp.s_addr;
		return addr;
	}

	char* buf = NULL;
	struct hostent result_buf, *result = NULL;
	uint32_t len = 1024;
	int32_t h_err;
	buf = kms_malloc(1024);
	if (buf == NULL)
		goto clean_return;
	int32_t rc;
	while ((rc = gethostbyname2_r(kms_addr, AF_INET, &result_buf, buf, len,
					&result, &h_err)) == ERANGE) {
		len *= 2;
		buf = realloc(buf, len);
		if (buf == NULL)
			goto clean_return;
	}
	if (rc != 0) {
		printf("Host res failed : kms vmx client\n");
		goto clean_return;
	}
	uint32_t res_cnt = 0;
	while (result_buf.h_addr_list[res_cnt++] != NULL);
	res_cnt--;
	addr = kms_calloc(res_cnt, sizeof(struct in_addr));
	if (addr == NULL)
		goto clean_return;
	uint32_t i = 0;
	for (i = 0; i < res_cnt; i++)
		addr[i].s_addr = ((struct in_addr*)(result_buf.h_addr_list[i]))->s_addr;

clean_return:
	if (buf != NULL)
		free(buf);
	return addr;
}


#define CRLF_MAX_RESP_SIZE 2048

static crlf_resp_t* recvCRLF_Resp(int32_t sd) {

	crlf_resp_t* ret = kms_calloc(1, sizeof(crlf_resp_t));
	if (ret == NULL)
		return NULL;
	ret->resp = kms_malloc(CRLF_MAX_RESP_SIZE);
	if (ret->resp == NULL) {
		free(ret);
		return NULL;
	}
	
	uint8_t* buf = ret->resp;
	uint32_t buf_total_size = CRLF_MAX_RESP_SIZE; 
	int32_t err = 0;
	uint32_t buf_read_pos = 0;
	uint32_t buf_write_pos = 0;
	int8_t resp_hdr_flag = 0;

	int32_t rc = 0;
	while (1) {

		rc = recv(sd, buf + buf_write_pos,
				buf_total_size - buf_write_pos, 0);
		if (rc <= 0) {
			perror("recv()1: kms vmx client");
			err = -1;
			//assert(0);
			break;
		}
		buf_write_pos += rc;
		uint32_t count = rc;
		while (count > 3) {

			if ((buf[buf_read_pos] == '\r') &&
					(buf[buf_read_pos + 1] == '\n') &&
					(buf[buf_read_pos + 2] == '\r') &&
					(buf[buf_read_pos + 3] == '\n')) {
				buf_read_pos += 4;
				resp_hdr_flag = 1;
				break;
			}
			buf_read_pos++;
			count--;
		}
		if (resp_hdr_flag > 0) {

			ret->resp_len = buf_read_pos;
			int32_t cont_len = crlfGetContentLength(buf);
			if (cont_len < 0) {
				ret->content = NULL;
				ret->content_len = 0;
				break;
			}
			ret->content = kms_calloc(cont_len, sizeof(char));
			if (ret->content == NULL) {
				err = -6;
				break;
			}
			ret->content_len = cont_len;
			uint32_t len_to_read = cont_len -
				(buf_write_pos - buf_read_pos);
			memcpy(ret->content, buf + buf_read_pos,
					buf_write_pos - buf_read_pos);
			buf[buf_read_pos] = '\0';
			uint32_t content_pos = buf_write_pos - buf_read_pos;
			if (len_to_read == 0)
				break;
			while (len_to_read > 0) {
				rc = recv(sd, ret->content + content_pos, len_to_read, 0);
				if (rc <= 0) {
					perror("recv()2: kms vmx client");
					err = -2;
					//assert(0);
					break;
				}
				len_to_read -= rc;
				content_pos += rc;
			}
			break;
		} else {
			buf_read_pos += count;
			continue;
		}
	}
	if (err < 0)
		return NULL;
	return ret;
}


static int32_t crlfGetContentLength(uint8_t* buf) {

	char* t_str = NULL;
	if ((t_str = strcasestr((char*)buf, "\r\nContent-Length:")) != NULL) {

		t_str += 17;
		char* tailptr;
		int64_t rv = strtol(t_str, &tailptr, 0);
		if (tailptr[0] == '\r')
			return rv;
	}
	return -1;
}


static int32_t getCRLF_RespCode(crlf_resp_t const* ret) {

	char* t_str = NULL;
	if (strncmp((char*)ret->resp, "HTTP/1.1 ", 9) == 0) {

		t_str = (char*)ret->resp + 9;
		char* tailptr;
		int64_t rv = strtol(t_str, &tailptr, 0);
		if ((tailptr[0] == ' ') && (tailptr[1] == 'O') && (tailptr[2] == 'K'))
			return rv;
	}
	return 0;
}


static uint8_t* getCRLF_Location(crlf_resp_t const* ret) {

	char* t_str1 = NULL, *t_str2 = NULL;
	if ((t_str1 = strcasestr((char*)ret->resp, "\r\nLocation: ")) != NULL) {

		t_str1 += 12;
		if ((t_str2 = strstr(t_str1, "\r\n")) != NULL) {

			uint8_t* loc = kms_malloc(t_str2 - t_str1 + 1);
			memcpy(loc, t_str1, (t_str2 - t_str1));
			loc[t_str2 - t_str1] = '\0';
			return loc;
		}
	}
	return NULL;
}


static int32_t connectTimeout(int32_t sd, struct sockaddr_in const* addr_ctx,
		uint32_t seconds) {

	struct timeval curr_tmout_val, new_tmout_val;
	socklen_t tmout_cont_len = sizeof(struct timeval);
	new_tmout_val.tv_sec = seconds;
	new_tmout_val.tv_usec = 0;
	int32_t rc = getsockopt(sd, SOL_SOCKET, SO_SNDTIMEO, &curr_tmout_val,
			&tmout_cont_len);
	if (rc < 0) {
		perror("VMX KMS getsockopt failed: ");
		return -E_MFP_KMS_VMX_SOCK_FAIL;
	}
	rc = setsockopt(sd, SOL_SOCKET, SO_SNDTIMEO, &new_tmout_val,
			tmout_cont_len);
	if (rc < 0) {
		perror("VMX KMS setsockopt failed: ");
		return -E_MFP_KMS_VMX_SOCK_FAIL;
	}
	rc= connect(sd, (struct sockaddr*)addr_ctx,
			sizeof(struct sockaddr_in));
	if (rc < 0) {
		perror("connect: kms vmx client: ");
		return -E_MFP_KMS_VMX_CONNECT_FAIL;
	}
	rc = setsockopt(sd, SOL_SOCKET, SO_SNDTIMEO, &curr_tmout_val, 
			tmout_cont_len);
	if (rc < 0) {
		perror("VMX KMS setsockopt 2 failed: ");
		close(sd);
		return -E_MFP_KMS_VMX_SOCK_FAIL;
	}
	return rc;
}

