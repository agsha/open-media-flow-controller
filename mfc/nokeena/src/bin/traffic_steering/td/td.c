/*
 * (C) Copyright 2011 Juniper Networks
 */ 
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <glib.h>
#include "syslog.h"
#include "td.h"

/* TODO: add statistics to TD/TM */

/* 
 * Authenticate TM IP address, which is stored in the config file 
 *
 * Return value: TRUE if success, otherwise FALSE
 */
int
td_authenticate_tm_ip (void)
{
    FILE    *fp;
    char    tm_ip[MAX_LINE_LEN + 1];
    char    *remote_addr = NULL;

    if ( !(fp = (FILE *)fopen(TD_CONFIG, "r")) )  {
        syslog(LOG_ERR, "Error in opening TD config file!");
        return FALSE;
    }

    /* str len should be less than tm_ip len */
    if(fscanf(fp, "%100s", tm_ip) != 1) {
        syslog(LOG_ERR, "Error in reading TM IP address!");
        fclose(fp);
        return FALSE;    
    }

    fclose(fp);

    if ((remote_addr = getenv("REMOTE_ADDR")) == NULL) {
        syslog(LOG_ERR, "Error in invocation - wrong remote IP address.");
        return FALSE;
    }

    if (strcmp(remote_addr, tm_ip)) {
        syslog(LOG_WARNING, "remote addr %s in HTTP POST doesn't match configured TM address %s!",
                remote_addr, tm_ip);
        return FALSE;
    } 

    return TRUE;
}

/* 
 * For debug only. dump a hash entry
 *
 * Arguments:
 *      key: hash key
 *      value: hash value
 *      user_data: user data supplied by the caller  
 */
void 
td_hash_dump (gpointer key, gpointer value, gpointer user_data) 
{
    (void)value;
    (void)user_data;
 
    syslog(LOG_DEBUG, "%s", (char *)key);
}

/* 
 * Remove one BGP network specified by a hash entry
 *
 * Arguments:
 *      key: hash key
 *      value: hash value
 *      user_data: user data supplied by the caller  
 */
void
td_bgp_remove (gpointer key, gpointer value, gpointer user_data)
{
    char cmd_str[MAX_CMD_LEN + 1];

    (void)value;
    (void)user_data;

    syslog(LOG_DEBUG, "removing %s", (char *)key);

    cmd_str[MAX_CMD_LEN] = 0;
    snprintf(cmd_str, MAX_CMD_LEN, "/usr/local/bin/vtysh -c \"conf t\" -c \"router bgp %s\" -c \"no network %s\"", bgp_as_number, (char *)key);
    system(cmd_str);
}

/* 
 * Hash destroy function for a glib hash table
 *
 * Arguments:
 *      key: hash key
 */
void
td_hash_key_destroy (gpointer key)
{
    free(key);
}

/* 
 * Read current BGP networks into a hash table 
 *
 * Return value: TRUE if success, otherwise FALSE
 */
int 
td_read_bgp_table (void)
{
    FILE    *fpipe;
    char    line[MAX_LINE_LEN + 1]; 
    char    *start, *ip_network;
    char    cmd_str[MAX_CMD_LEN + 1];

    memset(bgp_as_number, 0, sizeof(bgp_as_number));

    cmd_str[MAX_CMD_LEN] = 0;
    snprintf(cmd_str, MAX_CMD_LEN, "/usr/local/bin/vtysh -c \"show run\"");

    if ( !(fpipe = (FILE *)popen(cmd_str, "r")) )  {  
        syslog(LOG_ERR, "Problems with pipe!");
        return FALSE; 
    }  

    line[MAX_LINE_LEN] = 0;
    while (fgets(line, MAX_LINE_LEN, fpipe)) {  
    
        if (bgp_as_number[0] == 0) {
            if ((start = strstr(line, "router bgp ")) != NULL) {
    
                /* get BGP AS Number */
                sscanf(start + BGP_AS_NUM_OFFSET, "%s", bgp_as_number);
                syslog(LOG_DEBUG, "BGP AS number %s", bgp_as_number);
            }
        } else if ((start = strstr(line, "network")) != NULL) {

            /* get BGP networks */
            if ((ip_network = (char *)malloc(MAX_LINE_LEN + 1)) == NULL ||
                sscanf(start + BGP_NETWORK_OFFSET, "%s", ip_network) != 1) {
                syslog(LOG_ERR, "Cannot get ip_network!");
                fclose(fpipe);
                return FALSE;
            }
                
            syslog(LOG_DEBUG, "network %s", ip_network);
         
            g_hash_table_insert(bgp_hash_table, (gpointer)ip_network, (gpointer)ip_network);               
        }
    }

    fclose(fpipe);
   
    /* BGP is not configured or enabled, no need to proceed further */
    if (bgp_as_number[0] == 0) {
        syslog(LOG_ERR, "BGP is not ready!");

        return FALSE;     
    }

    syslog(LOG_DEBUG, "now dump the hash table"); 
    g_hash_table_foreach(bgp_hash_table, td_hash_dump, NULL);

    return TRUE;
}

/* 
 * Read HTTP POST content into a buffer
 *
 * Arguments:
 *      input: pointer to the buffer to store the content
 *
 * Return value: TRUE if success, otherwise FALSE
 */
int 
td_read_http_post (char *input)
{
    char *lenstr = NULL;
    long len;

    lenstr = getenv("CONTENT_LENGTH");
    if(lenstr == NULL || sscanf(lenstr, "%ld", &len) != 1 || len > MAXINPUT) {
        syslog(LOG_ERR, "Error in invocation - wrong FORM probably.");
        return FALSE;
    } 

    /* read HTTP POST content. IP networks are seperated by '+' */
    fgets(input, MAXINPUT + 1, stdin);
    input[MAXINPUT] = 0;
    syslog(LOG_DEBUG, "HTTP POST content len %ld, string %s", len, input);

    return TRUE;
}

/* 
 * Update BGP networks. 
 *
 * Arguments:
 *      input: pointer to the buffer where HTTP POST content is stored.
 */
void 
td_update_bgp_table (char *input)
{
    char cmd_str[MAX_CMD_LEN];
    char *ip_network, *str, *ip_lookup;

    /* The hash table will keep networks not in HTTP POST */ 
    for (str = input + EXTRA + 1; ; str= NULL) {     
        ip_network = strtok(str, "+");
        if (ip_network == NULL) {
            break;
        }

        ip_lookup = (char *)g_hash_table_lookup(bgp_hash_table, (gpointer)ip_network);            
        syslog(LOG_DEBUG, "look up %s, found %s", ip_network, ip_lookup ? ip_lookup : "NULL");        
 
        if (ip_lookup) {
            
            g_hash_table_remove(bgp_hash_table, (gpointer)ip_network);
        } else {
            syslog(LOG_DEBUG, "adding network %s to BGP", ip_network);
    
            /* New network. Need to configure to BGP */
            snprintf(cmd_str, MAX_CMD_LEN, "/usr/local/bin/vtysh -c \"conf t\" -c \"router bgp %s\" -c \"network %s\"", bgp_as_number, ip_network);            
            system(cmd_str);
        }
    }

    syslog(LOG_DEBUG, "now remove unused network from BGP");

    /* remove all unused networks from BGP */
    g_hash_table_foreach(bgp_hash_table, td_bgp_remove, NULL);
}

int 
main (void)
{
    char input[MAXINPUT + 1];

    syslog(LOG_DEBUG, "td script is triggered");

    printf("%s%c%c\n", "Content-Type:text/html;charset=iso-8859-1",13,10);
    printf("<TITLE>Response</TITLE>\n");

    setuid(0);
    if (td_authenticate_tm_ip() == FALSE) {
        return 1;
    }

    if (td_read_http_post(input) == FALSE) {
        return 1;
    }

    g_thread_init(NULL);
       
    if ((bgp_hash_table = g_hash_table_new_full(g_str_hash, g_str_equal, td_hash_key_destroy, NULL)) == NULL) {
        syslog(LOG_ERR, "Problems with g_hash_table_new_full");
        return 1;
    }

    if (td_read_bgp_table() == FALSE) {
        g_hash_table_destroy(bgp_hash_table);
        return 1;
    }

    td_update_bgp_table(input);

    g_hash_table_destroy(bgp_hash_table);
	return 0;
}
