#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "dns_parser.h"
#include "dns_builder.h"
#include "nkn_mem_counter_def.h"

static int32_t DNS_Handler(parsed_dns_msg_t* parsed_data, void* reg_ctx) {

    parsed_data->print_msgval_hdlr(parsed_data);
    parsed_data->delete_hdlr(parsed_data);
    return 0;
}

int nkn_mon_add(const char *name, const char *instance, void *obj,
	uint32_t size)
{
    return 0;
}

int32_t main(int32_t argc, char** argv) {

    if (argc != 2) {

	printf("Usage: ./<pgm_name> <number of queries>\n");
	return -1;
    }
    uint32_t num_q = atoi(argv[1]);
    if (num_q == 0) {

	printf("Invalid query count value.\n");
	return -2;
    }

    dns_builder_t* bldr = createDNS_Builder(1024);
    if (bldr == NULL) {

	printf("Failed: DNS Builder create.\n");
	exit(-1);
    }
    if (bldr->set_msg_hdr_hdlr(bldr, 1234, 1, 0, 1, 0, 0, 0, 0) < 0) {

	printf("Failed: DNS Builder set msg.\n");
	exit(-1);
    }
    if (bldr->set_cnt_parm_hdlr(bldr, num_q, 0, 0, 0) < 0) {

	printf("Failed: DNS Builder set cnt parm.\n");
	exit(-1);
    }

    char* query_fmt = ".test%u.com";
    uint32_t i = 0;
    for (; i < num_q; i++) {

	char query[1024];
	snprintf(query, 1024, query_fmt, i);
	if (bldr->add_query_hdlr(bldr, &query[0], 1, 1) < 0) {

	    printf("Failed: DNS Builder Add Query Parm.\n");
	    exit(-1);
	}
    }

    uint8_t const* proto_data;
    uint32_t pd_len;
    if (bldr->get_proto_data_hdlr(bldr, &proto_data, &pd_len) < 0) {

	printf("DNS Builder : Get Proto Dat.\n");
	exit(-1);
    }

    dns_pr_t* pr = createDNS_Handler(1024); 
    pr->set_parse_complete_hdlr(pr, DNS_Handler, NULL);
    if (pr->parse_hdlr(pr, proto_data, pd_len) < 0) {

	printf("DNS Parser : Failed\n");
	exit(-1);
    }
    pr->delete_hdlr(pr);
    FILE* fp = fopen("dns_pk.bin", "wb");
    if (fp == NULL) {

	perror("fopen failed: ");
	exit(-1);
    }
    if (fwrite(proto_data, pd_len, 1, fp) != 1) {

	perror("fwrite failed: ");
	exit(-1);
    }
    fclose(fp);

    bldr->delete_hdlr(bldr);
    return 0;
}

