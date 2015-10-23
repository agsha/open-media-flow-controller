#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include "nkn_sched_task.h"
#include "nkn_hash_map.h"

/*
        This API can be used to insert a new list (can have just one element).
        This function will append the list to the already existing list for
        that object id.
        NOTE: obj_id --> (Vn, Sn)
*/
GList *nkn_task_push_sleepers(char *obj_id, GList *in_list)
{
        GList *ret_list = NULL;
	GList *tmp_list = NULL;

	assert(obj_id);
	assert(in_list);

	tmp_list = (GList *) c_sleeper_q_tbl_get(obj_id);
	
	/* Concatenate any list that was already there with the new list */
        ret_list = g_list_concat(tmp_list, in_list);
	assert(ret_list);

	/* Insert replaces the original list */
	c_sleeper_q_tbl_insert(obj_id, ret_list);

        return ret_list;
}

/*
        This API can be used to get all the tasks that are sleeping on a
        particular object.
        The list is deleted from the hashmap. The caller needs to repopulate
        with a list of any tasks that are still waiting for this obj_id.

        NOTE: The caller function needs to send in a ptr variable to GList
        so that this function can assign the pointer to the head of the GList.
        After the pointer is assigned the array location will be set to NULL.
*/
void
nkn_task_pop_sleepers(char *obj_id, GList **a_in_ptr)
{
	GList *tmp_list;

	tmp_list = (GList *) c_sleeper_q_tbl_get (obj_id);

        *a_in_ptr = tmp_list;
	
	/* 
	* This function just erase the list pointer. Does not delete memory.
	*/
	c_sleeper_q_tbl_delete(obj_id);
}

/*
* This function puts a pointer of any type (void *) into the sleeper queue
* with the obj_id as the key to the queue. 
*/
int
nkn_task_helper_push_task_on_list(char *obj_id, void *in_ptr)
{
	GList *tmp_list = NULL;
	int ret_val = -1;
	int new_list = 0;

	assert(in_ptr);
	assert(obj_id);

	tmp_list = (GList *) c_sleeper_q_tbl_get(obj_id);
	if(tmp_list == NULL) {
		new_list = 1;
	}
	else {
	}

	/* Optimize this. g_list_prepend() and reverse*/
        tmp_list = g_list_append(tmp_list, in_ptr);
	if (new_list == 1) {
		ret_val = c_sleeper_q_tbl_insert(obj_id, tmp_list);
		if(ret_val < 0) {
			return -1;
		}
	}
	
	assert(tmp_list);
	return 0;
}

