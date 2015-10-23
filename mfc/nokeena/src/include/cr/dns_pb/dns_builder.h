#ifndef DNS_BUILDER_H
#define DNS_BUILDER_H

#include <stdint.h>


#define MAX_QNAME_CNT 20

struct dns_builder;

typedef int32_t (*set_dns_msg_hdr_fptr)(struct dns_builder* bldr,
		uint16_t query_id, uint8_t qr, uint8_t op_code, uint8_t aa, uint8_t tc,
		uint8_t rd, uint8_t ra, uint8_t rcode);

typedef int32_t (*set_dns_cnt_parm_fptr)(struct dns_builder* bldr,
		uint16_t q_cnt, uint16_t ans_cnt,uint16_t auth_cnt, uint16_t addit_cnt);

typedef int32_t (*add_dns_query_fptr)(struct dns_builder* bldr, 
		char const* qname, uint16_t type, uint16_t cl);

typedef int32_t (*add_dns_resp_fptr)(struct dns_builder* bldr, 
		char const* qname, uint16_t type, uint16_t cl, uint32_t ttl, 
		uint16_t rd_len, uint8_t const* rdata);

typedef int32_t (*get_dns_proto_data_fptr)(struct dns_builder const* bldr,
		uint8_t const**proto_data, uint32_t* pd_len);

typedef void (*delete_dns_builder_fptr)(struct dns_builder* bldr);


typedef struct dns_builder {


	set_dns_msg_hdr_fptr set_msg_hdr_hdlr;
	set_dns_cnt_parm_fptr set_cnt_parm_hdlr;
	add_dns_query_fptr add_query_hdlr;
	add_dns_resp_fptr add_ans_hdlr;
	add_dns_resp_fptr add_auth_hdlr;
	add_dns_resp_fptr add_addit_hdlr;

	get_dns_proto_data_fptr get_proto_data_hdlr;

	delete_dns_builder_fptr delete_hdlr;

	void* __internal;

} dns_builder_t;


dns_builder_t* createDNS_Builder(uint32_t max_wbuf_len);

#endif

