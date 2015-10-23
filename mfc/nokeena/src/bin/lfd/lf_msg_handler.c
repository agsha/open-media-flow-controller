#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>

// libxml
#include <libxml/xmlwriter.h>

// lf includes
#include "lf_msg_handler.h" 
#include "lf_http_parser.h"
#include "lf_http_builder.h"
#include "lf_uint_ctx_cont.h"
#include "lf_sock_msg_buffer.h"
#include "lf_nw_event_handler.h"
#include "lf_common_defs.h"
#include "lf_metrics_monitor.h"
#include "lf_metrics2xml_api.h"
#include "lf_err.h"
#include "lf_dbg.h"

#include "dispatcher_manager.h"
#include "entity_context.h"

#define LF_URI_BASE "/lf/"

#define SKIP_WHITE_SPACE(str) { \
	while (*(str)++ == ' '); \
	(str)--; \
}

#define SKIP_CONSEC_CHR(str, chr, n) { \
	while (((n)--) && (*(str)++ == (chr))); \
	(str)--; \
}

#define SKIP_CONSEC_CHR_REV(str, chr, n) { \
	while (((n)--) && (*(str)-- == (chr))); \
	(str)++; \
}


extern uint_ctx_cont_t* uint_ctx_cont;
extern lf_metrics_ctx_t *lfm;

static int32_t lfMatchURI(char const* uri);

int32_t lfMsgHandler(uint8_t* buf, uint32_t len, void const* ctx) {

	int32_t rc = 0;
	void *metrics[3] = {NULL};
	void *ref_id[3] = {NULL};
	char *resp_content = NULL;
	uint32_t resp_content_len = 0;
	xmlBufferPtr xml_out = NULL;

	http_msg_builder_t* http_bldr = NULL;
	parsed_http_msg_t* parsed_out = parseHTTP_Message(buf);
	if (parsed_out == NULL) {
		LF_LOG(LOG_ERR, LFD_MSG_HANDLER, "Error parsing HTTP message\n");
		rc = -1;
		goto error_return;
	}
	LF_LOG(LOG_NOTICE, LFD_MSG_HANDLER, "Received Message: %s\n",
			&parsed_out->msg_ctx.request.path[0]);

	if (strcmp(&parsed_out->msg_ctx.request.method[0], "GET") == 0) {

		if (lfMatchURI(&parsed_out->msg_ctx.request.path[0]) >= 0) {

			rc = lf_metrics_get_snapshot_ref(lfm, metrics, ref_id);
			if (rc) {
				LF_LOG(LOG_ERR, LFD_MSG_HANDLER,
						"unable to retrieve data from the stats monitor"
						"errcode: %d\n", rc);
				goto error_return;
			}
			writeXMLFromMetrics(NULL, lfm, metrics, (void*)&xml_out);
			lf_metrics_release_snapshot_ref(lfm, ref_id);

			//	char const* resp_content = "<html><body><h1>LF
			//	Test</h1></body></html>";
			resp_content = (char *)xml_out->content;
			resp_content_len = xml_out->use;//(uint32_t)strlen(resp_content);
			///

			http_bldr = createHTTP_Response(
					&parsed_out->msg_ctx.request.version[0], 200, "OK", 1024);
			if (http_bldr == NULL) {
				LF_LOG(LOG_ERR, LFD_MSG_HANDLER,
						"No memory : Error creating HTTP builder\n");
				rc = -2;
				goto error_return;
			}
		} else {
			http_bldr = createHTTP_Response(
					&parsed_out->msg_ctx.request.version[0], 404,
					"Not Found", 1024);
		}
	} else {
		http_bldr = createHTTP_Response(&parsed_out->msg_ctx.request.version[0],
				405, "Method Not Allowed", 1024);
	}
	char time_str[100];
	struct tm curr_time;
	time_t curr_ctime;
	curr_ctime = time(NULL);
	if (gmtime_r(&curr_ctime, &curr_time) != NULL) {
		if (asctime_r(&curr_time, &time_str[0]) != NULL) {
			uint32_t time_str_len = strlen(time_str);
			if (time_str[time_str_len - 1] == '\n')
				time_str[time_str_len - 1] = '\0';
			if (http_bldr->add_hdr_hdlr(http_bldr, "Date", &time_str[0]) < 0) {
				LF_LOG(LOG_ERR, LFD_MSG_HANDLER, "Error Adding Date header\n");
				rc = -6;
				goto error_return;
			}
		}
	}
	if (http_bldr->add_hdr_hdlr(http_bldr, "Server", "LF (MFC)") < 0) {
		LF_LOG(LOG_ERR, LFD_MSG_HANDLER, "Error Adding Server header\n");
		rc = -7;
		goto error_return;
	}

	if (http_bldr->add_hdr_hdlr(http_bldr, "Connection", "close") < 0) {
		LF_LOG(LOG_ERR, LFD_MSG_HANDLER, "Error Adding Server header\n");
		rc = -7;
		goto error_return;
	}

	if (resp_content_len > 0) {

		char tmp_len_buf[20];
		snprintf(&tmp_len_buf[0], 20, "%u",resp_content_len); 
		if (http_bldr->add_hdr_hdlr(http_bldr, "Content-Length",
					&tmp_len_buf[0]) < 0) {
			LF_LOG(LOG_ERR, LFD_MSG_HANDLER,
					"Error Adding Content-Length header\n");
			rc = -3;
			goto error_return;
		}

		if (http_bldr->add_hdr_hdlr(http_bldr, "Content-Type",
					"text/xml") < 0) {
			LF_LOG(LOG_ERR, LFD_MSG_HANDLER,
					"Error Adding Content-Type header\n");
			rc = -4;
			goto error_return;
		}

		if (http_bldr->add_content_hdlr(http_bldr, (uint8_t*)resp_content,
					resp_content_len) < 0) {
			LF_LOG(LOG_ERR, LFD_MSG_HANDLER, "Error Adding content\n");
			rc = -5;
			goto error_return;
		}
	}

	uint8_t const* reply_buf = NULL;
	uint32_t reply_len = 0;
	http_bldr->get_msg_hdlr(http_bldr, &reply_buf, &reply_len);
	if ((reply_buf == NULL) || (reply_len == 0)) {
		 LF_LOG(LOG_ERR, LFD_MSG_HANDLER, "Error preparing HTTP response\n");
		rc = -8;
		goto error_return;
	}

	fd_context_t* fd_ctx = (fd_context_t*)ctx;
	uint32_t uint_key = fd_ctx->fd; 
	msg_buffer_t* msg_buf = uint_ctx_cont->get_element_hdlr(
			uint_ctx_cont, uint_key);
	if (msg_buf == NULL) {
		 LF_LOG(LOG_ERR, LFD_MSG_HANDLER,
				 "Error getting message sock context\n");
		rc = -9;
		goto error_return;
	}
	if (msg_buf->add_write_msg_hdlr(msg_buf, reply_buf, reply_len) < 0) {

		LF_LOG(LOG_ERR, LFD_MSG_HANDLER,
				"Error writing response message to buffer\n"); 
		rc = -10;
		goto error_return;
	}

	entity_context_t* entity_ctx = fd_ctx->entity_ctx;
	entity_ctx->disp_mngr->self_set_write(entity_ctx);

error_return:
	free(buf);
	if (xml_out != NULL) {
		xmlBufferFree(xml_out);
	}
	if (parsed_out != NULL)
		parsed_out->delete_hdlr(parsed_out);
	if (http_bldr != NULL)
		http_bldr->delete_hdlr(http_bldr);
	return rc;
}


static int32_t lfMatchURI(char const* uri) {

	/* For phase-1, we do a very simple decoding. With more URI support, a detailed URI
	   decoding shall be developed */
	SKIP_WHITE_SPACE(uri);
	char const* start_uri_pos = uri;
	if (strncasecmp(uri, "HTTP://", 7) == 0) {

		char const* tmp_uri = strchr(uri + 7, '/');
		if (tmp_uri != NULL) 
			start_uri_pos = tmp_uri;
		else
			return -1;
	}

	if (start_uri_pos[0] != '/')
		return -2;
	char const* tmp_start_uri_pos1 = start_uri_pos;
	uint32_t len = strlen(start_uri_pos) - 1;
	uint32_t n = len; 
	char const* tmp_start_uri_pos2 = start_uri_pos + len;
	SKIP_CONSEC_CHR(tmp_start_uri_pos1, '/', len);
	len = n;
	start_uri_pos += n;
	SKIP_CONSEC_CHR_REV(tmp_start_uri_pos2, '/', len);
	uint32_t norm_len = tmp_start_uri_pos2 - tmp_start_uri_pos1 + 1;

	if ((norm_len == 2) && (strncmp(tmp_start_uri_pos1, "lf", 2) == 0))
		return 0;
	return -2;
}

