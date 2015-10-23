#include "crst_prot_parser.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


int32_t msgHandler(crst_prot_msg_t* msg, void* ctx) {

    printf("Received Message: %u\n", *(uint32_t*)ctx);
    msg->print_hdlr(msg);
    msg->delete_hdlr(msg);
    free(ctx);
    return 0;
}

int32_t main() {


    // TEST 1
    uint32_t* msg_ctx1 = malloc(sizeof(uint32_t));
    *msg_ctx1 = 1;
    crst_prot_parser_t* pr1 = createCRST_Prot_Parser(1024, msgHandler,msg_ctx1);
    if (pr1 == NULL) {

	printf("Protocol parser create failed.\n");
	return -1;
    }
    char* msg1 = "GET /DN/example.com\n";
    uint32_t msg_len1 = strlen(msg1);
    pr1->parse_hdlr(pr1, (uint8_t*)msg1, msg_len1);
    pr1->delete_hdlr(pr1);


    // TEST 2
    uint32_t* msg_ctx2 = malloc(sizeof(uint32_t));
    *msg_ctx2 = 2;
    crst_prot_parser_t* pr2 = createCRST_Prot_Parser(1024, msgHandler,msg_ctx2);
    if (pr2 == NULL) {

	printf("Protocol parser create failed.\n");
	return -1;
    }
    char* msg2 = "GET /DN/example.com\n";
    uint32_t msg_len2 = strlen(msg2);
    uint32_t j = 0;
    for (; j < msg_len2; j++)
	pr2->parse_hdlr(pr2, (uint8_t*)(msg2 + j), 1);
    pr2->delete_hdlr(pr2);


     // TEST 3
    uint32_t* msg_ctx3 = malloc(sizeof(uint32_t));
    *msg_ctx3 = 3;
    crst_prot_parser_t* pr3 = createCRST_Prot_Parser(1024, msgHandler,msg_ctx3);
    if (pr3 == NULL) {

	printf("Protocol parser create failed.\n");
	return -1;
    }
    char* msg3 = "GET /DN/example.com 5\nHello";
    uint32_t msg_len3 = strlen(msg3);
    pr3->parse_hdlr(pr3, (uint8_t*)(msg3), msg_len3);
    pr3->delete_hdlr(pr3);


    // TEST 4
    uint32_t* msg_ctx4 = malloc(sizeof(uint32_t));
    *msg_ctx4 = 4;
    crst_prot_parser_t* pr4 = createCRST_Prot_Parser(1024, msgHandler,msg_ctx4);
    if (pr4 == NULL) {

	printf("Protocol parser create failed.\n");
	return -1;
    }
    char* msg4 = "GET /DN/example.com 5\nHello";
    uint32_t msg_len4 = strlen(msg4);
    for (j = 0; j < msg_len4; j++)
	pr4->parse_hdlr(pr4, (uint8_t*)(msg4 + j), 1);
    pr4->delete_hdlr(pr4);

    return 0;
}

