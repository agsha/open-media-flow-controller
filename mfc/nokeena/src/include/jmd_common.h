#ifndef _JMD_COMMON_H_
#define _JMD_COMMON_H_

#include <ctype.h>
#include "common.h"
#include "mdb_db.h"
#include "md_utils.h"
#include "nkn_mgmt_defs.h"

int 
md_rfc952_domain_validate(md_commit *commit, 
	const mdb_db *old_db, 
	const mdb_db *new_db, 
	const mdb_db_change_array *change_list, 
	const tstring *node_name, 
	const tstr_array *node_name_parts, 
	mdb_db_change_type change_type, 
	const bn_attrib_array *old_attribs, 
	const bn_attrib_array *new_attribs, 
	void *arg);

int 
md_nvsd_str_to_host_and_port(const char *name, char **host, char **port);

int md_nvsd_ns_exists(md_commit *commit, const mdb_db *db, 
		const char *ns_name, tbool *found);


#endif
