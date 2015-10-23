

//
// These definitions copied from md_iptable_decls.inc.c (TallMaple/samara)
//
typedef enum {
	mirn_none = 0,    /* no target */
	mirn_accept,      /* ACCEPT */
	mirn_redirect,	  /* REDIRECT */
	mirn_tproxy,

} md_ipt_target_name;

typedef struct md_ipt_tproxy {
	char 	*on_ip;
	char 	*on_port;
} md_ipt_tproxy;

typedef struct md_ipt_state_rule {
	tstring *misr_output;	// single line of iptable -t tproxy -L -n -v
	tstr_array *misr_parts;	// output broke into space delimited parts
	md_ipt_target_name misr_target;
	md_ipt_tproxy	*misr_match;

} md_ipt_state_rule;

typedef struct {
	const char *ip_tproxy;
	tbool found;
	tbool gen_accept_found;
} md_ipt_rule_match_ctxt;


TYPED_ARRAY_HEADER_NEW_NONE(md_ipt_state_rule, md_ipt_state_rule *);



static int
md_ipt_match_new(md_ipt_tproxy **ret_match)
{
    int err = 0;
    md_ipt_tproxy *match = NULL;

    bail_null(ret_match);

    match = calloc(1, sizeof(md_ipt_tproxy));
    bail_null(match);

bail:
    if (ret_match)
	*ret_match = match;
    return err;
}

static int
md_ipt_match_free(md_ipt_tproxy **inout_match)
{
    int err = 0;
    md_ipt_tproxy *match = NULL;

    bail_null(inout_match);
    match = *inout_match;

    if (match) {
	safe_free(match->on_ip);
	safe_free(match->on_port);
	safe_free(*inout_match);
    }
bail:
    return err;
}


static int
md_ipt_state_rule_new(md_ipt_state_rule **ret_rule)
{
    int err = 0;
    bail_null(ret_rule);

    *ret_rule = calloc(1, sizeof(md_ipt_state_rule));
    bail_null(*ret_rule);
bail:
    return err;
}

static int
md_ipt_state_rule_free(md_ipt_state_rule **inout_rule)
{
    int err = 0;
    md_ipt_state_rule *rule = NULL;

    bail_null(inout_rule);
    rule = *inout_rule;
    if (rule) {
	ts_free(&(rule->misr_output));
	tstr_array_free(&(rule->misr_parts));
	md_ipt_match_free(&(rule->misr_match));
	safe_free(*inout_rule);
    }
bail:
    return err;
}

static void
md_ipt_state_rule_free_for_array(void *rule_void)
{
	md_ipt_state_rule *rule = (md_ipt_state_rule *)rule_void;
	md_ipt_state_rule_free(&rule);
}

TYPED_ARRAY_IMPL_NEW_NONE(md_ipt_state_rule, md_ipt_state_rule *, NULL);

static int
md_ipt_state_rule_array_new(md_ipt_state_rule_array **ret_array)
{
    int err = 0;
    array_options ao;

    err = array_options_get_defaults(&ao);
    bail_error(err);

    ao.ao_elem_free_func = md_ipt_state_rule_free_for_array;

    err = md_ipt_state_rule_array_new_full(ret_array, &ao);
    bail_error_null(err, ret_array);

bail:
    return(err);
}

static int
md_ipt_rule_match_cmp(const md_ipt_state_rule_array *rules, uint32 idx, md_ipt_state_rule *rule, void *data)
{
    int err = 0;
    md_ipt_rule_match_ctxt *mirmc = (md_ipt_rule_match_ctxt*) data;

    if (mirmc->found)
	goto bail;

    if (rule && ((rule->misr_target != mirn_tproxy) && (rule->misr_target != mirn_accept)))
	goto bail;

    if (rule && rule->misr_match &&
	    safe_strcmp(mirmc->ip_tproxy, rule->misr_match->on_ip) == 0) {
	mirmc->found = true;
    }

    if (rule && (rule->misr_target == mirn_accept))
	mirmc->gen_accept_found = true;

bail:
    return err;
}


/*---------------------------------------------------------------------------*/


typedef struct {
	char *misc_name;
	char *misc_policy;
	md_ipt_state_rule_array *misc_rules;
} md_ipt_state_chain;

TYPED_ARRAY_HEADER_NEW_NONE(md_ipt_state_chain, md_ipt_state_chain *);

static int
md_ipt_state_chain_new(md_ipt_state_chain **ret_chain)
{
    int err = 0;
    md_ipt_state_chain *ptr = NULL;

    bail_null(ret_chain);

    ptr = calloc(1, sizeof(md_ipt_state_chain));
    bail_null(ptr);

    err = md_ipt_state_rule_array_new(&(ptr->misc_rules));
    bail_error(err);

bail:
    if (ret_chain) {
	*ret_chain = ptr;
    }
    return err;
}


static int
md_ipt_state_chain_free(md_ipt_state_chain **inout_chain)
{
    int err = 0;
    md_ipt_state_chain *chain = NULL;

    bail_null(inout_chain);
    chain = *inout_chain;

    if (chain) {
	safe_free(chain->misc_name);
	safe_free(chain->misc_policy);
	md_ipt_state_rule_array_free(&(chain->misc_rules));
	safe_free(*inout_chain);
    }
bail:
    return err;
}

static void
md_ipt_state_chain_free_for_array(void *chain_void)
{
    md_ipt_state_chain *chain = (md_ipt_state_chain *)chain_void;
    md_ipt_state_chain_free(&chain);
}

TYPED_ARRAY_IMPL_NEW_NONE(md_ipt_state_chain, md_ipt_state_chain *, NULL);

static int
md_ipt_state_chain_array_new(md_ipt_state_chain_array **ret_array)
{
    int err = 0;
    array_options ao;

    err = array_options_get_defaults(&ao);
    bail_error(err);

    ao.ao_elem_free_func = md_ipt_state_chain_free_for_array;
    err = md_ipt_state_chain_array_new_full(ret_array, &ao);
    bail_error_null(err, ret_array);

bail:
    return err;
}



//
// End of copied declarations
//
