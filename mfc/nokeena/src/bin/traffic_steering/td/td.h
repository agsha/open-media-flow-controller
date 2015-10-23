/*
 * (C) Copyright 2011 Juniper Networks
 */
#ifndef __TD_H__
#define __TD_H__

#define MAX_CMD_LEN 100
#define MAX_LINE_LEN 100 // need to change fscanf string len if this is changed 
#define BGP_AS_NUM_OFFSET 11
#define BGP_NETWORK_OFFSET 8

#define EXTRA 5   /* data= */
#define MAXINPUT 100000

#define TD_CONFIG "/config/nkn/td.conf"

static GHashTable *bgp_hash_table;
static char bgp_as_number[MAX_LINE_LEN + 1];

int td_authenticate_tm_ip(void);
void td_hash_dump(gpointer key, gpointer value, gpointer user_data);
void td_bgp_remove(gpointer key, gpointer value, gpointer user_data);
void td_hash_key_destroy(gpointer key);
int td_read_bgp_table(void);
int td_read_http_post(char *input);
void td_update_bgp_table(char *input);

#endif /* __TD_H__ */
