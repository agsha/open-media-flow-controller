#include "jmd_common.h"

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
	void *arg)
{
    int err = 0;
    const bn_attrib *new_value = NULL;
    tstring *t_domain = NULL;
    const char *c_domain = NULL;
    const char *bad_chars = "/\\*:|`\"?,<>;'[]{}+=_()&^%$#@!~ \t\n\r";
    uint32 i = 0;
    uint32 j = 0;
    uint32 len = 0;
    uint32 maybe_ipv4_addr = 0;
    uint16 maybe_port = 0;
    char *host = NULL;
    char * port = NULL;
    tbool valid = false;

    if (change_type != mdct_delete) {
	/* Get the value */
	new_value = bn_attrib_array_get_quick(new_attribs, ba_value);

	if (new_value) {
	    err = bn_attrib_get_tstr(new_value, NULL, bt_string, NULL, &t_domain);
	    bail_error(err);

	    if ( ts_length(t_domain) < 1 )
		goto bail;
	    err = lia_str_is_ipv4addr(ts_str(t_domain),&valid);
	    if(valid) {
		err = 0;
		goto bail;
	    }
	    err = lia_str_is_ipv6addr(ts_str(t_domain),&valid);
	    if(valid) {
		err = 0;
		goto bail;
	    }

	    /* Before we do any checks.. move everything to lower case, 
	     *
	     * RFC952: No distinction is made between upper and lower case.
	     */
	    err = ts_trans_lowercase(t_domain);
	    bail_error(err);

	    c_domain = ts_str(t_domain);

	    len = strlen(c_domain);

	    if (strcmp(c_domain, "*") == 0) {
		/* Default case . nothing to check */
		goto bail;
	    }

	    /* RFC952: The last character must not be a minus sign or period.
	     */
	    if (c_domain[len - 1] == '.') {
		err = md_commit_set_return_status_msg_fmt(commit, 1,
			_("error : domain name cannot end with '.'"));
		bail_error(err);
		goto bail;
	    }
	    if (c_domain[len - 1] == '-') {
		err = md_commit_set_return_status_msg_fmt(commit, 1,
			_("error : domain name cannot end with '-'"));
		bail_error(err);
		goto bail;

	    }

	    /* This is tricky - the name could be an IP address instead 
	     * of a true domain name. So check if this is an ipv4 address
	     * or not.
	     */
	    maybe_ipv4_addr = 0;
	    maybe_port = 0;

	    err = md_nvsd_str_to_host_and_port(c_domain, &host, &port);
	    if ( err == -1 ) {
		err = 0;
		err = md_commit_set_return_status_msg_fmt(commit, 1, 
			_("error : Malformed name"));
		bail_error(err);
		goto bail;
	    }
	    /* 06/Apr/2010 - Commenting out the check for
	     * digit since  domain name can start with a
	     * digit and need not be ipv4 address. This is
	     * fix for Bug 4288.
	     * What is valid fqdn address with digits ?
	     */

	    /* Check if the host is an IP address instead of a name 
	     */
	    /* RFC952:  The first character must be an alpha character.
	     */
	    if ( (!isalpha(host[0]))&&(!isdigit(host[0])) ) {
		err = md_commit_set_return_status_msg_fmt(commit, 1,
			_("error : name cannot start with a non-alphanumeric character"));
		bail_error(err);
		goto bail;
	    }

	    /* Check if invalid characters in name */
	    /* RFC952: A "name" (Net, Host, Gateway, or 
	     * Domain name) is a text string up to 24 
	     * characters drawn from the alphabet (A-Z), 
	     * digits (0-9), minus sign (-), and period (.).  
	     */
	    for(i = 0; i < strlen(host); i++) {
		for(j = 0; j < strlen(bad_chars); j++) {
		    if ((host[i] == bad_chars[j])) {
			err = md_commit_set_return_status_msg_fmt(commit, 1,
				_("error : bad name. Cannot use '%c' in domain name"),
				bad_chars[j]);
			bail_error(err);
			goto bail;
		    }
		}
	    }
	    /* TODO: RFC952: Get the hostname part and make sure 
	     * it is < 24 characters
	     */

	/* Check for port */
	if ( port ) {
	    maybe_port = atoi(port);
	    if ( maybe_port == 0 ) {
		err =  md_commit_set_return_status_msg_fmt(commit, 1,
			_("error : Bad port number : %s"), port);
		bail_error(err);
		goto bail;
	    }
	}
    }
}
bail:
ts_free(&t_domain);
safe_free(host);
safe_free(port);
return err;
}


int 
md_nvsd_str_to_host_and_port(const char *name, char **host, char **port)
{
    int err = 0;
    int len = 0;
    tstr_array *tokens = NULL;

    bail_null(name);
    bail_null(host);

    err = ts_tokenize_str(name, 
	    ':',
	    '\\',
	    '"',
	    0,
	    &tokens);
    bail_error(err);

    len = tstr_array_length_quick(tokens);

    /* Check for exactly 2 or less tokens - anything 
     * more means we have multiple ':' - which is illegal 
     */
    if ( len > 2 ) {
	bail_force_quiet(-1);
    }

    if ( host ) {
	*host = smprintf("%s", tstr_array_get_str_quick(tokens, 0));
    }

    if ( port && (len > 1) ) {
	*port = smprintf("%s", tstr_array_get_str_quick(tokens, 1));
    }

bail:
    tstr_array_free(&tokens);
    return err;
}

int 
md_nvsd_ns_exists(md_commit *commit, const mdb_db *db, 
		const char *ns_name, tbool *found)
{
    int err = 0;
    node_name_t node_name = {0};
    tstring *ret_value = NULL;

    bail_require(found);
    bail_require(db);
    bail_require(ns_name);

    snprintf(node_name, sizeof(node_name),  "/nkn/nvsd/namespace/%s", ns_name);

    err = mdb_get_node_value_tstr (commit, db, node_name, 0, 
		    found, &ret_value);
    bail_error(err);

    /* not using  ret_value  */

bail:
    ts_free(&ret_value);
    return err;
}
