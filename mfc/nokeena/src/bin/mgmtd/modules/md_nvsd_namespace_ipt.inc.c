#include "md_nvsd_namespace_ipt_decls.inc.c"

static int
md_nvsd_namespace_check_tproxy_rule(
	md_commit	*commit,
        const mdb_db *old_db,
        const mdb_db *new_db,
	const char *ip_tproxy,
	tbool	*is_existing);

static int
md_nvsd_namespace_check_and_delete_tproxy_rule(
	md_commit	*commit,
        const mdb_db *old_db,
        const mdb_db *new_db,
	const char *ip_tproxy);

static int
md_nvsd_namespace_check_and_create_tproxy_rule(
	md_commit	*commit,
        const mdb_db *old_db,
        const mdb_db *new_db,
	const char *ip_tproxy);

static int
md_nvsd_namespace_check_nat_rule(
	md_commit	*commit,
        const mdb_db *old_db,
        const mdb_db *new_db,
	const char *interface,
	tbool *is_existing);

static int
md_nvsd_namespace_check_and_delete_nat_rule(
	md_commit	*commit,
        const mdb_db *old_db,
        const mdb_db *new_db,
	const char *interface);


static int
md_nvsd_namespace_check_and_create_nat_rule(
	md_commit	*commit,
        const mdb_db *old_db,
        const mdb_db *new_db,
	const char *interface);

static int
md_nvsd_ns_get_iptable_state(const char *table_name, tstring **ret_output);

static int
md_nvsd_ns_load_tproxy_rule(const char *table_name,
		const char *operation_prefix,
		const char *on_ip, const char *on_port,
		tstring **ret_output);

static int
md_nvsd_ns_check_if_ns_tproxied(
		const char 	*ns_name,
		const char 	*ip_tproxy,
		const mdb_db 	*old_db,
		mdb_db		*inout_new_db,
		tbool 		*can_delete);

static int
md_nvsd_ns_load_accept_rule(const char *table_name,
		const char *operation_prefix,
		const char *on_ip, const char *on_port,
		tstring **ret_output);


/*----------------------------------------------------------------------------
 * Helper API to find out if we can unload an iptable rule or not.
 * If more than one namespace references the same rule, then we cant unload
 * the rule.
 *--------------------------------------------------------------------------*/
static int
md_nvsd_ns_check_if_ns_tproxied(
		const char 	*ns_name,
		const char 	*ip_tproxy,
		const mdb_db 	*old_db,
		mdb_db		*inout_new_db,
		tbool 		*can_delete)
{
	int err = 0;
	tstr_array *ts_ns = NULL;
	char *self_node = NULL;
	const char *name = NULL;
	int i = 0, num_ns = 0;
	tstring *t_value = NULL;

	// we absolutely MUST have these args
	bail_null(can_delete);
	bail_null(ns_name);
	bail_null(ip_tproxy);

	*can_delete = true;

	// our own node name
	self_node = smprintf("/nkn/nvsd/namespace/%s/ip_tproxy", ns_name);
	bail_null(self_node);

	// get a list of all the namespaces that have ip_tproxy
	err = mdb_get_matching_tstr_array(NULL, inout_new_db,
			"/nkn/nvsd/namespace/*/ip_tproxy",
			0,
			&ts_ns);
	bail_error_null(err, ts_ns);

	// remove our own node - we dont want to compare against
	// ourselves
	lc_log_basic(LOG_DEBUG, "deleting our own node (%s) from list of namespaces...", self_node);
	tstr_array_delete_str(ts_ns, self_node);

	// remove the probe node also
	tstr_array_delete_str(ts_ns, "mfc_probe");

	num_ns = tstr_array_length_quick(ts_ns);
	lc_log_basic(LOG_DEBUG, "Got (%d) namespaces.", num_ns);
	for (i = 0; i < num_ns; i++) {

		name = tstr_array_get_str_quick(ts_ns, i);
		// Get te ip_tproxy value. if this is set to
		// 0.0.0.0, then we arent interested in this
		// namespace
		err = mdb_get_node_value_tstr(NULL, inout_new_db,
				name, 0, NULL, &t_value);
		bail_error(err);

		lc_log_basic(LOG_DEBUG, "Checking if ns is tproxy enabled.. (%s)", name);

		if (ts_equal_str(t_value, "0.0.0.0", false)) {
			// this namespace doesnt do a tproxy ..
			// so skip it.
			ts_free(&t_value);
			lc_log_basic(LOG_DEBUG, "No. (%s) is not a tproxy namespace.", name);
			continue;
		}

		if (ts_equal_str(t_value, ip_tproxy, false)) {
			// This namespace does a tproxy and its
			// ip address is same as the one we are
			// asked to check against.
			// Set the flag and break this loop..
			// we can remove the IP Table rule!!
			lc_log_basic(LOG_DEBUG, "Yes. (%s) is a tproxy namespace. breaking scan loop", name);
			*can_delete = false;
			break;
		}
	}


bail:
	safe_free(self_node);
	tstr_array_free(&ts_ns);
	ts_free(&t_value);
	return err;
}


static int
md_ipt_state_parse_rule(tstring *line, md_ipt_state_rule *rule)
{
	int err = 0;
	tstr_array *parts = NULL;
	tstr_array *ip_parts = NULL;

	bail_null(rule);
	bail_null(line);
	
	// break into parts
	err = ts_tokenize(line, ' ', '\\', '"', ttf_ignore_leading_separator,
			&parts);
	bail_error(err);


	// format:  pkts bytes target     prot opt in     out     source               destination
	// index :   0    1      2         3    4   5      6       7                    8
	//
	// Example:
	//  	pkts bytes target     prot opt in     out     source               destination
	//  	29111 3122K ACCEPT     all  --  *      *       0.0.0.0/0            0.0.0.0/0
	//          0     0 TPROXY     all  --  *      *       0.0.0.0/0            0.0.0.0/0           TPROXY redirect 192.168.10.159:0
	// idx:   0     1    2         3    4   5      6          7                    8                   9     10         11
	if (ts_equal_str(tstr_array_get_quick(parts, 2), "ACCEPT", false)) {
		rule->misr_target = mirn_accept;
		rule->misr_parts = parts;

	} else if (ts_equal_str(tstr_array_get_quick(parts, 2), "TPROXY", false)) {
		rule->misr_target = mirn_tproxy;
		rule->misr_parts = parts;
		err = md_ipt_match_new(&(rule->misr_match));
		bail_error(err);

		err = ts_tokenize(tstr_array_get_quick(parts, 11), ':', '\\', '"', ttf_ignore_leading_separator,
				&ip_parts);
		bail_error(err);

		err = ts_dup_str(tstr_array_get_quick(ip_parts, 0), &(rule->misr_match->on_ip), NULL);
		bail_error(err);
		err = ts_dup_str(tstr_array_get_quick(ip_parts, 1), &(rule->misr_match->on_port), NULL);
		bail_error(err);
	}

bail:
	tstr_array_free(&ip_parts);
	return err;

}

static int
md_nvsd_ns_iptable_parse_chain(const tstr_array *lines, uint32 *inout_pos,
		md_ipt_state_chain **inout_chain)
{
	int err = 0;
	tstring *line = NULL;
	tstr_array *parts = NULL;
	md_ipt_state_chain *chain = NULL;
	md_ipt_state_rule *rule = NULL;
	uint32 pos = 0;

	bail_null(lines);
	bail_null(inout_chain);
	bail_null(inout_pos);

	pos = *inout_pos;

	line = tstr_array_get_quick(lines, pos);
	bail_null(line);

	if (!ts_has_prefix_str(line, "Chain ", false)) {
		bail_force_msg(lc_err_parse_error,
				"Could not parse '%s'. Missing Chain.",
				ts_str(line));
	}

	// break into parts
	err = ts_tokenize(line, ' ', '\\', '"', ttf_ignore_leading_separator,
			&parts);
	bail_error(err);

	// format: Chain PREROUTING (policy ACCEPT 0 packets, 0 bytes)
	// index :  0       1         2      3     4   5      6  7
	//
	if (tstr_array_length_quick(parts) != 8 &&
	    tstr_array_length_quick(parts) != 4) { // for user-defined chains
		bail_force_msg(lc_err_parse_error,
				"Incorrect part count while parsing '%s'",
				ts_str(line));
	}

	err = md_ipt_state_chain_new(&chain);
	bail_error_null(err, chain);

	// assign chain name
	err = ts_dup_str(tstr_array_get_quick(parts, 1), &(chain->misc_name), NULL);
	bail_error(err);

	// assign chain policy
	if (ts_equal_str(tstr_array_get_quick(parts, 2), "(policy", false)) {
		err = ts_dup_str(tstr_array_get_quick(parts, 3), &(chain->misc_policy), NULL);
		bail_error(err);
	}
	else {
		chain->misc_policy = NULL;
	}

	// skip header line
	pos++;
	pos++;

	// now handle rules
	while (pos < tstr_array_length_quick(lines)) {
		line = tstr_array_get_quick(lines, pos);
		// loop until we hit another chain or a blank line
		if (!line || ts_has_prefix_str(line, "Chain ", false) ||
			ts_length(line) == 0) {
			break;
		}

		err = md_ipt_state_rule_new(&rule);
		bail_error(err);

		err = ts_dup(line, &(rule->misr_output));
		bail_error(err);

		// this is where the on-ip & on-port needs to be plugged in
		err = md_ipt_state_parse_rule(line, rule);
		bail_error(err);

		err = md_ipt_state_rule_array_append_takeover(
				chain->misc_rules, &rule);
		bail_error(err);

		pos++;
	}

	*inout_chain = chain;
	*inout_pos = pos;

	chain = NULL;

bail:
	md_ipt_state_chain_free(&chain);
	md_ipt_state_rule_free(&rule);
	tstr_array_free(&parts);
	return err;
}

static int
md_nvsd_namespace_check_generic_accept_rule(
	md_commit	*commit,
        const mdb_db *old_db,
        const mdb_db *new_db,
	const char *ip_tproxy,
	tbool	*is_existing)
{
	int err = 0;
	tstring *output = NULL;
	tstr_array *lines = NULL;
	uint32 i = 0;
	md_ipt_state_chain *chain = NULL;

	bail_null(is_existing);
	bail_null(ip_tproxy);

	*is_existing = true;

	// get current table state
	err = md_nvsd_ns_get_iptable_state("tproxy", &output);
	bail_error(err);

	// break o/p into individual lines
	err = ts_tokenize(output, '\n', '\\', '"', 0, &lines);

	while (i < tstr_array_length_quick(lines)) {

		// if line is blank, skip
		if (ts_length(tstr_array_get_quick(lines, i)) == 0) {
			i++;
			continue;
		}

		err = md_nvsd_ns_iptable_parse_chain(lines, &i, &chain);
		bail_error(err);

	}

	 md_ipt_rule_match_ctxt mirmc;

	mirmc.ip_tproxy = ip_tproxy;
	mirmc.found = false;

	// TODO: what next.. we parsed a single line..
	// should we add into an array? OR, simply compare and return
	//
	err = md_ipt_state_rule_array_foreach(chain->misc_rules,
			md_ipt_rule_match_cmp,
			&mirmc);
	bail_error(err);

	*is_existing = mirmc.gen_accept_found;

bail:
	ts_free(&output);
	tstr_array_free(&lines);
	md_ipt_state_chain_free(&chain);
	return err;
}

static int
md_nvsd_namespace_check_tproxy_rule(
	md_commit	*commit,
        const mdb_db *old_db,
        const mdb_db *new_db,
	const char *ip_tproxy,
	tbool	*is_existing)
{
	int err = 0;
	tstring *output = NULL;
	tstr_array *lines = NULL;
	uint32 i = 0;
	md_ipt_state_chain *chain = NULL;

	bail_null(is_existing);
	bail_null(ip_tproxy);

	*is_existing = true;

	// get current table state
	err = md_nvsd_ns_get_iptable_state("tproxy", &output);
	bail_error(err);

	// break o/p into individual lines
	err = ts_tokenize(output, '\n', '\\', '"', 0, &lines);

	while (i < tstr_array_length_quick(lines)) {

		// if line is blank, skip
		if (ts_length(tstr_array_get_quick(lines, i)) == 0) {
			i++;
			continue;
		}

		err = md_nvsd_ns_iptable_parse_chain(lines, &i, &chain);
		bail_error(err);

	}

	 md_ipt_rule_match_ctxt mirmc;

	mirmc.ip_tproxy = ip_tproxy;
	mirmc.found = false;

	// TODO: what next.. we parsed a single line..
	// should we add into an array? OR, simply compare and return
	//
	err = md_ipt_state_rule_array_foreach(chain->misc_rules,
			md_ipt_rule_match_cmp,
			&mirmc);
	bail_error(err);

	*is_existing = mirmc.found;

bail:
	ts_free(&output);
	tstr_array_free(&lines);
	md_ipt_state_chain_free(&chain);
	return err;
}

/*----------------------------------------------------------------------------
 * Helper API to check if a tproxy rule exists. If not, do nothing. Else,
 * delete the tproxy rule.
 *--------------------------------------------------------------------------*/
static int
md_nvsd_namespace_check_and_delete_tproxy_rule(
	md_commit	*commit,
        const mdb_db *old_db,
        const mdb_db *new_db,
	const char *ip_tproxy)
{
	int err = 0;
	tbool is_existing = false;
	tstring *output = NULL;
	tbool is_duped_rule = true;

	// Check if the rule already exists.
	err = md_nvsd_namespace_check_tproxy_rule(commit,
			old_db, new_db, ip_tproxy, &is_existing);
	bail_error(err);


	if (is_existing) {
		err = md_nvsd_ns_load_tproxy_rule(
				"tproxy", "-D", ip_tproxy, "0", &output);
		bail_error(err);
	}


bail:
	return err;
}



/*----------------------------------------------------------------------------
 * Helper API to check if a tproxy rule exists. If not, create a rule. Else,
 * do nothing.
 *--------------------------------------------------------------------------*/
static int
md_nvsd_namespace_check_and_create_tproxy_rule(
	md_commit	*commit,
        const mdb_db *old_db,
        const mdb_db *new_db,
	const char *ip_tproxy)
{
	int err = 0;
	tbool is_existing = true;
	tstring *output = NULL;

	// Check if generic rule exists.. if it does
	// dont do anything. If not, create it
	err = md_nvsd_namespace_check_generic_accept_rule(commit,
			old_db, new_db, ip_tproxy, &is_existing);
	bail_error(err);

	if (!is_existing) {
		err = md_nvsd_ns_load_accept_rule(
				"tproxy", "-A", NULL, NULL, &output);
		bail_error(err);
	}
	ts_free(&output);

	// Check if the rule already exists.
	err = md_nvsd_namespace_check_tproxy_rule(commit,
			old_db, new_db, ip_tproxy, &is_existing);
	bail_error(err);


	if (!is_existing) {
		err = md_nvsd_ns_load_tproxy_rule(
				"tproxy", "-A", ip_tproxy, "0", &output);
		bail_error(err);
	}

bail:
	ts_free(&output);
	return err;
}

static int
md_nvsd_namespace_check_nat_rule(
	md_commit	*commit,
        const mdb_db *old_db,
        const mdb_db *new_db,
	const char *interface,
	tbool *is_existing)
{
	int err = 0;

	bail_null(is_existing);
	bail_null(interface);

	*is_existing = true;

bail:
	return 0;
}


/*----------------------------------------------------------------------------
 * Helper API to check if a nat rule exists. If not, do nothing. Else, create
 * a nat redirect rule.
 *--------------------------------------------------------------------------*/
static int
md_nvsd_namespace_check_and_delete_nat_rule(
	md_commit	*commit,
        const mdb_db *old_db,
        const mdb_db *new_db,
	const char *interface)
{
	int err = 0;

bail:
	return err;
}

/*----------------------------------------------------------------------------
 * Helper API to check if a nat rule exists. If not, create a NAT rule. Else,
 * dont do anything.
 *--------------------------------------------------------------------------*/
static int
md_nvsd_namespace_check_and_create_nat_rule(
	md_commit	*commit,
        const mdb_db *old_db,
        const mdb_db *new_db,
	const char *interface)
{
	int err = 0;

bail:
	return err;
}


/*----------------------------------------------------------------------------
 * Helper API to get the current table state so that we can parse it later on.
 * This API taken from md_iptables_state.inc.c (TallMaple/samara)
 *--------------------------------------------------------------------------*/
static int
md_nvsd_ns_get_iptable_state(const char *table_name, tstring **ret_output)
{
	int err = 0;
	lc_launch_params *lp = NULL;
	lc_launch_result lr;

	memset(&lr, 0, sizeof(lr));

	bail_null(table_name);
	bail_null(ret_output);

	err = lc_launch_params_get_defaults(&lp);
	bail_error_null(err, lp);

	lp->lp_wait = true;
	lp->lp_log_level = LOG_INFO;
	lp->lp_env_opts = eo_preserve_all;

	if (ret_output) {
		lp->lp_io_origins[lc_io_stdout].lio_opts = lioo_capture;
		lp->lp_io_origins[lc_io_stderr].lio_opts = lioo_capture;
	}


	*ret_output = lr.lr_captured_output;
	lr.lr_captured_output = NULL;


bail:
	lc_launch_params_free(&lp);
	lc_launch_result_free_contents(&lr);
	return err;
}


static int
md_nvsd_ns_load_tproxy_rule(const char *table_name,
		const char *operation_prefix,
		const char *on_ip, const char *on_port,
		tstring **ret_output)
{
	int err = 0;
	lc_launch_params *lp = NULL;
	lc_launch_result lr;

	memset(&lr, 0, sizeof(lr));

	bail_null(table_name);
	bail_null(ret_output);

	err = lc_launch_params_get_defaults(&lp);
	bail_error_null(err, lp);

	lp->lp_wait = true;
	lp->lp_log_level = LOG_INFO;
	lp->lp_env_opts = eo_preserve_all;

	if (ret_output) {
		lp->lp_io_origins[lc_io_stdout].lio_opts = lioo_capture;
		lp->lp_io_origins[lc_io_stderr].lio_opts = lioo_capture;
	}

	*ret_output = lr.lr_captured_output;
	lr.lr_captured_output = NULL;

bail:
	lc_launch_params_free(&lp);
	lc_launch_result_free_contents(&lr);
	return err;
}

static int
md_nvsd_ns_load_accept_rule(const char *table_name,
		const char *operation_prefix,
		const char *on_ip, const char *on_port,
		tstring **ret_output)
{
	int err = 0;
	lc_launch_params *lp = NULL;
	lc_launch_result lr;

	memset(&lr, 0, sizeof(lr));

	bail_null(table_name);
	bail_null(ret_output);

	err = lc_launch_params_get_defaults(&lp);
	bail_error_null(err, lp);

	lp->lp_wait = true;
	lp->lp_log_level = LOG_INFO;
	lp->lp_env_opts = eo_preserve_all;

	if (ret_output) {
		lp->lp_io_origins[lc_io_stdout].lio_opts = lioo_capture;
		lp->lp_io_origins[lc_io_stderr].lio_opts = lioo_capture;
	}


	*ret_output = lr.lr_captured_output;
	lr.lr_captured_output = NULL;


bail:
	lc_launch_params_free(&lp);
	lc_launch_result_free_contents(&lr);
	return err;
}

