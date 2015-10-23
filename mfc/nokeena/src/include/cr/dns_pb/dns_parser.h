#ifndef DNS_PARSER_H
#define DNS_PARSER_H

#include <stdint.h>
#include <netinet/in.h>

#define MAX_DNS_QNAME_LEN 256
#define MAX_DNS_RESP_LEN MAX_DNS_QNAME_LEN

#define MAX_QUERY_COUNT 20
#define MAX_RESP_COUNT MAX_QUERY_COUNT

enum dns_type {

	DNS_A = 1,
	DNS_NS = 2,
	DNS_CNAME = 5,
	DNS_SOA = 6,
	DNS_WKS = 11,
	DNS_PTR = 12,
	DNS_HINFO = 13,
	DNS_MINFO = 14,
	DNS_MX = 15,
	DNS_TXT = 16,
	DNS_AAAA = 28,
	DNS_AXFR = 252,
	DNS_MAILB = 253,
	DNS_ALL = 255
} dns_type_t;

#define DNS_CLASS_INT 0x0001 


struct parsed_dns_msg;
struct dns_pr;

typedef int32_t (*registered_dns_msg_hdlr_fptr)
	(struct parsed_dns_msg* parsed_data, void* reg_ctx);

typedef int32_t (*parse_dns_msg_fptr)(struct dns_pr* parser,
		uint8_t const* data, uint32_t len);

typedef int32_t (*delete_dns_pr_fptr)(struct dns_pr* parser);
typedef int32_t (*set_transport_type_fptr)(struct dns_pr *parser,
					   uint32_t over_tcp_flag);
typedef void (*set_parse_complete_hdlr_fptr)(struct dns_pr *pr,
			    registered_dns_msg_hdlr_fptr reg_msg_hdlr,
			    void *reg_ctx);

typedef struct dns_pr {

	parse_dns_msg_fptr parse_hdlr;
	delete_dns_pr_fptr delete_hdlr;
        set_transport_type_fptr set_transport_hdlr;
        set_parse_complete_hdlr_fptr set_parse_complete_hdlr;
	void* __internal;
} dns_pr_t;

dns_pr_t* createDNS_Handler(uint32_t max_rbuf_len);

//////////////

typedef uint32_t (*get_dns_msg_value_fptr)(struct parsed_dns_msg const*
		parsed_data);

typedef int32_t (*get_dns_name_idx_fptr)
	(struct parsed_dns_msg const* parsed_data, uint32_t idx, 
	 char const** qname); 

typedef int32_t (*get_dns_rdata_idx_fptr)
	(struct parsed_dns_msg const* parsed_data, uint32_t idx,
	 uint8_t const** qname, uint32_t*rlen); 

typedef int32_t (*get_dns_i32_idx_fptr)
	(struct parsed_dns_msg const* parsed_data, uint32_t idx);

typedef int64_t (*get_dns_i64_idx_fptr)
	(struct parsed_dns_msg const* parsed_data, uint32_t idx);

typedef void (*print_dns_msgval_fptr)(struct parsed_dns_msg const* parsed_data);

typedef void (*delete_dns_parser_fptr)(struct parsed_dns_msg* parsed_data);

typedef struct parsed_dns_msg {

	get_dns_msg_value_fptr get_query_id_hdlr;
	get_dns_msg_value_fptr get_qr_hdlr;
	get_dns_msg_value_fptr get_opcode_hdlr;
	get_dns_msg_value_fptr get_aa_hdlr;
	get_dns_msg_value_fptr get_tc_hdlr;
	get_dns_msg_value_fptr get_rd_hdlr;
	get_dns_msg_value_fptr get_ra_hdlr;
	get_dns_msg_value_fptr get_rcode_hdlr;
	get_dns_msg_value_fptr get_question_cnt_hdlr;
	get_dns_msg_value_fptr get_answer_cnt_hdlr;
	get_dns_msg_value_fptr get_authority_cnt_hdlr;
	get_dns_msg_value_fptr get_addit_rc_cnt_hdlr;

	get_dns_name_idx_fptr get_qname_hdlr;
	get_dns_i32_idx_fptr get_qtype_hdlr;
	get_dns_i32_idx_fptr get_qclass_hdlr;

	get_dns_name_idx_fptr get_ans_qname_hdlr;
	get_dns_i32_idx_fptr get_ans_type_hdlr;
	get_dns_i32_idx_fptr get_ans_class_hdlr;
	get_dns_i64_idx_fptr get_ans_ttl_hdlr;
	get_dns_i32_idx_fptr get_ans_rdlen_hdlr;
	get_dns_rdata_idx_fptr get_ans_rdata_hdlr;

	get_dns_name_idx_fptr get_auth_qname_hdlr;
	get_dns_i32_idx_fptr get_auth_type_hdlr;
	get_dns_i32_idx_fptr get_auth_class_hdlr;
	get_dns_i64_idx_fptr get_auth_ttl_hdlr;
	get_dns_i32_idx_fptr get_auth_rdlen_hdlr;
	get_dns_rdata_idx_fptr get_auth_rdata_hdlr;

	get_dns_name_idx_fptr get_addit_qname_hdlr;
	get_dns_i32_idx_fptr get_addit_type_hdlr;
	get_dns_i32_idx_fptr get_addit_class_hdlr;
	get_dns_i64_idx_fptr get_addit_ttl_hdlr;
	get_dns_i32_idx_fptr get_addit_rdlen_hdlr;
	get_dns_rdata_idx_fptr get_addit_rdata_hdlr;

	print_dns_msgval_fptr print_msgval_hdlr;

	delete_dns_parser_fptr delete_hdlr;

	void* __internal;
} parsed_dns_msg_t;


#endif
