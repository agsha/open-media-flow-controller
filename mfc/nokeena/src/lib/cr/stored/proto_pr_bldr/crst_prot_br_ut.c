#include "crst_prot_bldr.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


int32_t main() {


    char prot_data[1024];
    int32_t rc = 0;
    // TEST 1
    crst_prot_bldr_t* bldr1 = createCRST_ProtocolBuilder(1024);
    bldr1->set_resp_code_hdlr(bldr1, 200);
    bldr1->set_resp_desc_hdlr(bldr1, "OK");
    rc = bldr1->get_prot_data_hdlr(bldr1, (uint8_t*)&prot_data[0], 1024);
    prot_data[rc] = '\0';
    printf("Protocol data length: %d\n", rc);
    printf("Protocol data: %s\n", &prot_data[0]);
    bldr1->delete_hdlr(bldr1);

    // TEST 2
    crst_prot_bldr_t* bldr2 = createCRST_ProtocolBuilder(1024);
    bldr2->set_resp_code_hdlr(bldr2, 200);
    bldr2->set_resp_desc_hdlr(bldr2, "OK");
    char* content2 = "Hello";
    bldr2->add_content_hdlr(bldr2, (uint8_t*)content2, strlen(content2)); 
    rc = bldr2->get_prot_data_hdlr(bldr2, (uint8_t*)&prot_data[0], 1024);
    prot_data[rc] = '\0';
    printf("Protocol data length: %d\n", rc);
    printf("Protocol data: %s\n", &prot_data[0]);
    bldr2->delete_hdlr(bldr2);


    // TEST 3
    crst_prot_bldr_t* bldr3 = createCRST_ProtocolBuilder(1);
    bldr3->set_resp_code_hdlr(bldr3, 200);
    bldr3->set_resp_desc_hdlr(bldr3, "OK");
    rc = bldr3->get_prot_data_hdlr(bldr3, (uint8_t*)&prot_data[0], 1024);
    prot_data[rc] = '\0';
    printf("Protocol data length: %d\n", rc);
    printf("Protocol data: %s\n", &prot_data[0]);
    bldr3->delete_hdlr(bldr3);


    // TEST 4
    crst_prot_bldr_t* bldr4 = createCRST_ProtocolBuilder(1);
    bldr4->set_resp_code_hdlr(bldr4, 200);
    bldr4->set_resp_desc_hdlr(bldr4, "OK");
    char* content4 = "Hello";
    bldr4->add_content_hdlr(bldr4, (uint8_t*)content4, strlen(content4)); 
    rc = bldr4->get_prot_data_hdlr(bldr4, (uint8_t*)&prot_data[0], 1024);
    prot_data[rc] = '\0';
    printf("Protocol data length: %d\n", rc);
    printf("Protocol data: %s\n", &prot_data[0]);
    bldr4->delete_hdlr(bldr4);


    // TEST 5
    crst_prot_bldr_t* bldr5 = createCRST_ProtocolBuilder(1);
    bldr5->set_resp_code_hdlr(bldr5, 200);
    bldr5->set_resp_desc_hdlr(bldr5, "OK");
    char* content5 = "Hello";
    bldr5->add_content_hdlr(bldr5, (uint8_t*)content5, strlen(content5)); 
    uint32_t tot_len = 0;
    while (1) {
        rc = bldr5->get_prot_data_hdlr(bldr5, (uint8_t*)&prot_data[tot_len],
		1);
	if (rc <= 0)
	    break;
	tot_len += rc;
    }
    prot_data[tot_len] = '\0';
    printf("Protocol data length: %d\n", tot_len);
    printf("Protocol data: %s\n", &prot_data[0]);
    bldr5->delete_hdlr(bldr5);

    return 0;
}

