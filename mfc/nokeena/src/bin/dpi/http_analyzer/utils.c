/*
 * File: utils.c
 *   
 * Author: Sivakumar Gurumurthy 
 *
 *
 * (C) Copyright 2014 Juniper Networks, Inc.
 * All rights reserved.
 */

#ifdef _STUB_

/* Include Files */
#include <stdio.h>
#include <stdlib.h>
#include "event_handler.h"
#include "thread.h"

#include "atomic_ops.h"
#include "nkn_stat.h"
#include "nkn_debug.h"

/* Function Prototypes */
void* my_calloc(size_t nmemb, size_t size);
void my_free(void *ptr);


/* Functions ... */
void*
my_calloc(size_t nmemb, size_t size)
{
	void *buffer;
	int thread_id;

	/* Call the default calloc */
	buffer = calloc(nmemb, size);

	/* Get the thread id to reference into the global array */
//	thread_id = afc_get_cpuid();
//	printf ("[%d]Allocated : %ld @ %p\n", thread_id, size, buffer);

	return (buffer);

} /* end my_calloc() */

void
my_free(void *ptr)
{
	int thread_id;

	/* Call the default free ()  */
	free (ptr);

	/* Get the thread id to reference into the global array */
//	thread_id = afc_get_cpuid();
//	printf ("[%d]Freeing : %p\n", thread_id, ptr);

	return ;

} /* end of my_free() */


/*
 * Add header name to header list
 */
void
httphdr_list_add_name(http_hdr ** hdr, char *name, int len)
{
	http_hdr *dataNode = NULL,*curNode = NULL;
	if (*hdr == NULL)
	{
		dataNode = (http_hdr *) my_calloc (1,sizeof(http_hdr));
		dataNode->header_name = (char *)my_calloc((len+1),sizeof(char));
		if (dataNode->header_name == NULL) {
			DBG_LOG(ERROR, MOD_DPI_TPROXY,"Out of Memory");
			exit(1);
		}
		strncpy(dataNode->header_name, name,len);
		dataNode->header_name[len]='\0';
		dataNode->next = NULL;
		*hdr = dataNode;
	}
	else {
		curNode = *hdr;
		while(curNode->next)
			curNode = curNode->next;
		dataNode = (http_hdr *) my_calloc (1,sizeof(http_hdr));
		dataNode->header_name = (char *)my_calloc((len+1),sizeof(char));
		if (dataNode->header_name == NULL) {
			DBG_LOG(ERROR, MOD_DPI_TPROXY,"Out of Memory");
			exit(1);
		}
		strncpy(dataNode->header_name, name,len);
		dataNode->header_name[len]='\0';
		dataNode->next = NULL;
		curNode->next = dataNode;
	}
}

/*
 * Get the current active htt_hdr node which will always be the last nodein the list
 */
http_hdr*
get_cur_httphdr_node(http_hdr **dlist)
{
	http_hdr *curNode= *dlist;
	while(curNode->next)
		curNode = curNode->next;
	return curNode;
	
}

/*
 *Return the length of http_hdr list
 */
unsigned int
httphdr_list_len(http_hdr * dlist)
{
        int n = 0;
        for ( ; dlist != NULL; dlist = dlist->next)
                n++;
        return n;
}

//Free the complete complete http_hdr list
void httphdr_list_free(http_hdr ** head)
{
	http_hdr *deleteMe;
	while ((deleteMe = *head)) {
		*head = deleteMe->next;
		my_free(deleteMe->header_value);
		my_free(deleteMe->header_name);
		my_free(deleteMe);
    	}
	*head = NULL;
}

void
datalist_add (datalist ** dlist, void *data)
{
        datalist *dataNode = NULL, *tmpNode = NULL, *prevNode = NULL;

        if (*dlist == NULL) {
                dataNode = (datalist *) my_calloc (1,sizeof(datalist));
                dataNode->data = data;
                dataNode->next = NULL;
                *dlist = dataNode;
        }
        else {
                for (tmpNode = *dlist; tmpNode !=NULL; tmpNode = tmpNode->next)
                        prevNode = tmpNode;
                dataNode = (datalist *) my_calloc (1, sizeof(datalist));
                dataNode->data = data;
                dataNode->next = NULL;
                prevNode->next = dataNode;
        }
}

void
datalist_remove_node(datalist **dlist, int p_index)
{
	datalist *tmpNode= NULL, *curNode = NULL, *head = NULL;
	head = *dlist;
	while (head && ((http_pkt *)head->data)->index == p_index)
	{
		tmpNode = head;
		head = head->next;
		my_free(((http_pkt *)tmpNode->data)->reqline);
		httphdr_list_free(&(((http_pkt *)tmpNode->data)->httphdr_list));
		my_free(tmpNode->data);
		my_free(tmpNode);
	}
	if (head == NULL) { return; }
	*dlist = head;
	curNode = head;
	while(curNode->next)
	{
		if(((http_pkt *)curNode->next->data)->index == p_index)
		{
			tmpNode = curNode->next;
			curNode->next = tmpNode->next;
			httphdr_list_free(&(((http_pkt *)tmpNode->data)->httphdr_list));
			my_free(((http_pkt *)tmpNode->data)->reqline);
			my_free(tmpNode->data);
			my_free(tmpNode);
		}
		else curNode= curNode->next;
	}
}

/*
 * Delete the current node from the list
 */
void
delete_curNode(datalist **dlist)
{
	datalist *curNode = NULL, *head = NULL;
	head = curNode = *dlist;
	//If there is only one node in the list free that node
	if (head && head->next == NULL)
	{
		if (((http_pkt *)head->data)->reqline)
			my_free(((http_pkt *)head->data)->reqline);
		httphdr_list_free(&(((http_pkt *)head->data)->httphdr_list));
		my_free(head->data);
		my_free(head);
		*dlist = NULL;
		return;
	}
	//Delete the last node from the list which will be the current node
	else {
		//Loop till you reach the last node
		while(curNode->next)
			curNode = curNode->next;
		//Now free the last node which will be the currentNode
		if (((http_pkt *)head->data)->reqline)
		my_free(((http_pkt *)curNode->data)->reqline);
		httphdr_list_free(&(((http_pkt *)curNode->data)->httphdr_list));
		my_free(curNode->data);
		my_free(curNode);
	}

}

void*
get_curNode(datalist **dlist)
{
	datalist *lastNode= NULL, *tmpNode=NULL;
       for (tmpNode = *dlist; tmpNode !=NULL; tmpNode = tmpNode->next)
		lastNode = tmpNode;
	return lastNode;
	
}

void
datalist_free(datalist ** head)
{
	datalist *deleteMe;

	while ((deleteMe = *head)) {
		httphdr_list_free(&(((http_pkt *)deleteMe->data)->httphdr_list));
		my_free(((http_pkt *)deleteMe->data)->reqline);
		my_free(deleteMe->data);
		*head = deleteMe->next;
		my_free(deleteMe);
    	}
	*head = NULL;
}

unsigned int
datalist_len(datalist * dlist) 
{
        int n = 0;
        for ( ; dlist != NULL; dlist = dlist->next)
                n++;
        return n;
}

void *
getMatchingNode(datalist **dlist, uapp_cnx_t *uapp_cnx)
{
	datalist *tmpNode=NULL;
       for (tmpNode = *dlist; tmpNode !=NULL; tmpNode = tmpNode->next)
	{
		if(((http_pkt *)tmpNode->data)->uapp_cnx == uapp_cnx)
			return tmpNode->data;
	}
	return NULL;
}

#endif /* _STUB_ */

/* End of utils.c */
