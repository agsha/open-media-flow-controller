#ifndef __CONMSG__H__
#define __CONMSG__H__

/* 
 * COmmon leading structure
 */
#define MSG_MAGIC  "MSG conmsg_ver_1.0"

#define MSG_DISPLAY_SYMBOL	1
#define MSG_WRITE_SYMBOL	2
#define MSG_CALL_FUNC		3

typedef struct msg_req {
	char magic[32];

	uint16_t type;			// include this structure itself
	uint16_t size;			// include this structure itself
	
} msg_req_t;
#define msg_req_s sizeof(struct msg_req)


/*
 * Display Symbol
 */
#define MSG_TYPE_STRING	1
#define MSG_TYPE_HEX	2

typedef struct msg_display_symbol {
	int type;
	uint64_t address;
	char name[0];
} msg_display_symbol_t;
#define msg_display_symbol_s sizeof(struct msg_display_symbol)

/*
 * Write Symbol
 */
typedef struct msg_write_symbol {
	uint64_t value;
	char name[0];
} msg_write_symbol_t;
#define msg_write_symbol_s sizeof(struct msg_write_symbol)

/*
 * Call function
 */
typedef struct msg_call_func {
	int argc;
	char name[80];
	char arg1[80];
} msg_call_func_t;
#define msg_call_func_s sizeof(struct msg_call_func)

#endif // __CONMSG__H__

