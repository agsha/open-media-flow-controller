/*
 *
 * src/bin/mgmtd/md_upgrade.h
 *
 *
 *
 */

#ifndef __MD_UPGRADE_H_
#define __MD_UPGRADE_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif


#include "common.h"
#include "mdb_db.h"

/**
 * \file src/bin/mgmtd/md_upgrade.h Helper functions for mgmtd
 * module upgrades.
 * \ingroup mgmtd
 *
 * A mgmtd module wanting to do an upgrade can register its own
 * upgrade function and do whatever it wants.  The declarations in
 * this file add a concept of an "upgrade rule", which is a
 * data-driven way a module can specify the upgrades it wants to do,
 * as long as they follow a standard pattern.  The module should
 * create an md_upgrade_rule_array and populate it with rules during
 * initialization.
 *
 * If all of the module's upgrades can be handled with rules, you can
 * just pass md_generic_upgrade_downgrade as your upgrade function
 * (updown_func).  Your upgrade function argument (updown_arg) should
 * be a pointer to a pointer to an md_upgrade_rule_array.
 */

/**
 * Specifies the type of an upgrade rule.
 */
typedef enum {
    MUTT_none =     0,

    /**
     * Add a new node to the database, or modify an existing node.
     * This is mostly interchangeable with MUTT_MODIFY, except that
     * certain features such as making a modification conditional on
     * the prior value (the mur_modify_cond_value_str field of the
     * upgrade rule) only work with MUTT_MODIFY.
     *
     * NOTE: infrastructure nodes being added on upgrade should
     * consider using ::mur_cond_name_does_not_exist to make it easier
     * for customers to override the node value.
     */
    MUTT_ADD =      1,

    /**
     * Add a new node to the database, or modify an existing node.
     */
    MUTT_MODIFY =   2,

    /**
     * Delete a node from the database.
     */
    MUTT_DELETE =   3,

    /**
     * Rename a node and/or move it to a different part of the tree.
     */
    MUTT_REPARENT = 4,

    /**
     * Clamp a node's value at its min or max if it is out of range.
     * Useful if you are narrowing the valid range and do not want to
     * fail the initial commit on any nodes that were outside the new
     * valid range.
     */
    MUTT_CLAMP = 5,

    /**
     * Restrict allowed characters in a string value.
     *
     * NOTE: if you are using mur_allowed_chars, this can be used on
     * either literals or wildcards.  If you are using
     * mur_disallowed_chars, this can only be used on literal nodes.
     *
     * In any case, we start by replacing any disallowed chars
     * (whether explicitly or implicitly specified) with
     * mur_replace_char.  If this is a literal, we are then finished.
     * If it was a wildcard, we can take one of two actions:
     * (a) Make sure the value is unique among its peers, replacing
     * trailing characters with other legal characters as necessary.
     * Once we get a unique value, we rename the node.  This is the
     * default behavior; or (b) Delete the wildcard instance.
     * This behavior is selected by setting mur_delete_bad_instances.
     */
    MUTT_RESTRICT_CHARS = 6,

    /**
     * Restrict the length of a string value to an upper and/or lower
     * bound number of characters.
     *
     * Note that this can be used on either literal or wildcard nodes,
     * of any string data type.  In the case of a literal node, the
     * value will simply be truncated or padded as needed.  In the
     * case of a wildcard node, whose value must be unique among its
     * peers, we try simple truncation or padding at first, but if 
     * that produces a conflict, will try varying the final characters
     * of the value to uniquify it.  If all attempts to find a unique
     * name fail, the wildcard instance will be deleted.
     *
     * Note that no other constraints are considered, besides
     * (a) uniqueness among peers in the case of wildcards, and
     * (b) set of permissible characters according to mur_allowed_chars.
     * If your value needs any other constraints followed, you'll need
     * to write custom upgrade code instead of using a rule.
     *
     * Whether or not the node is a wildcard instance is determined
     * automatically from mur_name.  If the last name component of
     * mur_name is a "*" we use our wildcard instance handling logic;
     * otherwise, we treat it as a literal.
     */
    MUTT_RESTRICT_LENGTH = 7,

} md_upgrade_transform_type;

/**
 * Data structure specifying one upgrade rule.  This is a data-driven
 * way to specify what changes should be made to the database during
 * an upgrade.  If these rules are not powerful enough to express what
 * you want to do, you can always write a custom upgrade function.
 *
 * The following fields are common for all upgrade types:
 *   - mur_name
 *   - mur_from_module_version
 *   - mur_to_module_version
 *   - mur_cond_name_exists
 *   - mur_cond_name_does_not_exist
 *
 * For a MUTT_ADD rule, these fields are relevant:
 *   - mur_new_value_type
 *   - mur_new_value_str
 *   - mur_new_value_int
 *   - mur_new_value_uint64
 *   - mur_copy_from_node
 *
 * For a MUTT_MODIFY rule, these fields are relevant:
 *   - mur_new_value_type
 *   - mur_new_value_str
 *   - mur_new_value_int
 *   - mur_new_value_uint64
 *   - mur_copy_from_node
 *   - mur_new_value_str_use_existing (uncommon)
 *   - mur_modify_cond_value_str (uncommon)
 *   - mur_modify_cond_value_length_longer (uncommon)
 *
 * For a MUTT_DELETE rule, no additional fields are required
 * or permitted.
 *
 * For a MUTT_REPARENT rule, these fields are relevant:
 *   - mur_reparent_graft_under_name
 *   - mur_reparent_new_self_name
 *   - mur_reparent_self_value_follows_name
 *
 * For a MUTT_CLAMP rule, these fields are relevant:
 *   - mur_lowerbound
 *   - mur_upperbound
 *
 * For a MUTT_RESTRICT_CHARS rule, these fields are relevant:
 *   - mur_allowed_chars
 *   - mur_disallowed_chars
 *   - mur_replace_char
 *   - mur_delete_bad_instances
 *
 * For a MUTT_RESTRICT_LENGTH rule, these fields are relevant:
 *   - mur_len_min
 *   - mur_len_max
 *   - mur_len_pad_char
 *   - mur_allowed_chars
 */
typedef struct md_upgrade_rule {
    /**
     * The type of transformation to do.
     */
    md_upgrade_transform_type mur_upgrade_type;

    /**
     * Name or pattern of node(s) to upgrade.
     *
     * If this field contains no '*' binding name components, it names
     * a single node, and only that one node is affected.
     *
     * If this field contains one or more '*' binding name components,
     * it is a binding name pattern, and it will operate on all
     * instances of the wildcards specified.  So this can be used to
     * add, modify, or remove literal nodes underneath wildcard
     * instances.
     *
     * For example, these would be valid (please ignore whitespace
     * around '*' characters, which required to fit in a C comment,
     * but would not be present in your actual code):
     *   /a/b      Operate on "/a/b"
     *   /a/ * /b  Operate on the "b" child of every instance of "/a/ *"
     */
    const char *mur_name;

    /**
     * The db semantic version to upgrade from.  That is, this rule 
     * will be applied when we are upgrading from this version.
     * Can specify -1 to mean any version (presumably you will 
     * specify mur_to_module_version).
     */
    int32 mur_from_module_version;

    /**
     * The db semantic version to upgrade to.  That is, this rule
     * will be applied when we are upgrading to this version.
     * Can specify -1 to mean any version (presumably you will 
     * specify mur_from_module_version).  If both fields are 
     * specified, they must be apart by exactly 1.
     */
    int32 mur_to_module_version;

    /**
     * If set, will only run this upgrade rule if the node mentioned
     * already exists.
     */
    tbool mur_cond_name_exists;

    /**
     * If set, will only run this rule if the node mentioned does not
     * already exist.
     *
     * Note: this is often a good flag to use when using MUTT_ADD to
     * add infrastructure nodes which the customer might want to
     * override.  If you don't use this, and they are trying to
     * override the node you are setting, they have to worry about
     * ordering, lest their module run first and then its value get
     * overwritten.  But if you do set this, it doesn't matter which
     * one runs first.
     */
    tbool mur_cond_name_does_not_exist;

    /**
     * Type of node to set (for MUTT_ADD and MUTT_MODIFY).
     */
    bn_type mur_new_value_type;

    /**
     * Value of node to set (for MUTT_ADD and MUTT_MODIFY).
     */
    const char *mur_new_value_str;

    /**
     * If using MUTT_MODIFY, specifies that we should use the value that
     * the node already has.  Use this to change the data type holding
     * the node while preserving its value (assuming the value is also
     * valid in the new data type, e.g. going from uint32 to string).
     */
    tbool mur_new_value_str_use_existing;

    /**
     * Value of node to set (for MUTT_ADD and MUTT_MODIFY), as an
     * integer.  This is an optional alternative for
     * mur_new_value_str.  It defaults to INT64_MIN, and will be
     * ignored if it has this value.
     */
    int64 mur_new_value_int;

    /**
     * Uint64 counterpart to mur_new_value_int.  Use this, instead of
     * mur_new_value_int, if and only if your node's data type is
     * bt_uint64.  It defaults to UINT64_MAX, and will be ignored
     * if it has that value.
     */
    uint64 mur_new_value_uint64;

    /**
     * For MUTT_MODIFY, only do the modify if the current value
     * (represented in string form) equals this.
     */
    const char *mur_modify_cond_value_str;

    /**
     * For MUTT_MODIFY, only do the modify if the current value
     * (represented in string form) is longer than this many characters.
     * This might be useful if you are adding (or lowering) a string
     * length limit, and are willing to reset the value to some default
     * for over-length values.
     */
    uint32 mur_modify_cond_value_length_longer;

    /**
     * For MUTT_REPARENT: new parent of this node (or NULL to leave it
     * at same parent).  If you're just renaming a wildcard instance
     * in place, its parent will remain the same, so you can pass
     * NULL.
     *
     * If this name has wildcards in it (e.g. "/a/b/ * /c" with the spaces
     * removed), the value to substitute in for the asterisk will be 
     * taken from the corresponding binding name part in the original
     * binding name.
     */
    const char *mur_reparent_graft_under_name;

    /**
     * For MUTT_REPARENT: new last component of name of this node (or
     * NULL to leave it the same).  If you are renaming a wildcard
     * instance in place, this will be the new name of the wildcard.
     */
    const char *mur_reparent_new_self_name;

    /**
     * For MUTT_REPARENT: if this is a wildcard and we're changing the
     * node name, this says to change the value of the node to match
     * the new name.  Otherwise, the value is left as is.
     */
    tbool mur_reparent_self_value_follows_name;

    /* XXX: issue: wish graft under name could have magic *'s */

    /** For MUTT_CLAMP: minimum value the node should be allowed to have */
    int64 mur_lowerbound;

    /** For MUTT_CLAMP: maximum value the node should be allowed to have */
    int64 mur_upperbound;

    /**
     * For MUTT_RESTRICT_CHARS: list of allowed characters.
     * Either this or mur_disallowed_chars must be provided.
     *
     * For MUTT_RESTRICT_LENGTH: the set of legal characters to use
     * when trying to uniquify the value.  If not provided,
     * CLIST_ALPHANUM is used by default.  This is only used in the
     * case of a wildcard value, when a simple truncation or padding
     * produces a value which matches another existing wildcard
     * instance.  In that case, we cycle through the characters
     * provided here, starting from the last character and working our
     * way backwards as needed, to find a unique value.  Besides that,
     * this value is not used for this rule type -- we will NOT
     * constrain the rest of the value to use this set of characters.
     * (If you want that, use a MUTT_RESTRICT_CHARS rule first.)
     * 
     */
    const char *mur_allowed_chars;

    /**
     * For MUTT_RESTRICT_CHARS: list of disallowed chars.
     * Either this or mur_allowed_chars must be provided.
     */
    const char *mur_disallowed_chars;

    /** For MUTT_RESTRICT_CHARS: If invalid char found, replace it with this */
    char mur_replace_char;

    /**
     * For MUTT_RESTRICT_CHARS, only when used with a wildcard node.
     * Specify that if we find any instances that do not meet the new
     * restrictions, they are to be deleted (instead of the default
     * behavior of sanitizing the value and renaming them).
     */
    tbool mur_delete_bad_instances;

    /**
     * For MUTT_ADD and MUTT_MODIFY: name or pattern of node from which the
     * value will be copied.
     *
     * If this field contains no '*' binding name components, it names a
     * single node where the source value resides.
     *
     * If this field contains one or more '*' binding name components, it
     * is a binding name pattern (literal '*' values are not supported).
     * Working from right to left, each wildcard '*' in the source pattern
     * must match a corresponding wildcard '*' in mur_name.
     *
     * For every literal target node in the database that matches the
     * pattern in mur_name, the literal instance values from the wildcards
     * in the target node's pattern are used to resolve the wildcards in
     * correpsonding positions in the mur_copy_from_node pattern, and there
     * must be a corresponding literal node in database for every such
     * match, or a bail will occur.
     *
     * For example, these would be valid (please ignore whitespace around
     * '*' characters, which required to fit in a C comment, but would not
     * be present in your actual code):
     *
     *   From       To            Result
     *   ---------- -----------   -----------------------------------------
     *   /a/b/c     /x/y          Copy value from "/a/b/c" to "/x/y"
     *   /a/b/c     /x/ * /y      Copy value from "/a/b/c" to all instances
     *                            of "/x/ * /y" in the database.
     *   /a/ * /b   /x/ * /y/z    Copy value from every "b" child of every
     *                            instance of "/a/ *" to the "/y/z" child
     *                            in corresponding instances of "/x/ *".
     *   /a/ * /b  /x/ * /y/ * /z Copy value from every "b" child of every
     *                            instance of "/a/ *" to the "z" child of
     *                            every "/x/ * /y/ * /z" node instance
     *                            in the database, where "/a/ *" matches
     *                            "/y/ * ".
     *
     * Note that since wildcard positions are matched from right to left,
     * it is possible and accceptable for the mur_copy_from_node to have
     * fewer wildcards than the mur_name node.  In this case, the set of
     * database nodes that match the mur_copy_from_node pattern will be
     * reused in all matching instance sets of wildcards residing further
     * to the left in the mur_name node pattern, as illustrated in the
     * 2nd and 4th cases above.
     *
     * It is illegal, however, to have fewer wildcards in mur_name than in
     * mur_copy_from_node.
     */
    const char *mur_copy_from_node;

    /**
     * For MUTT_RESTRICT_LENGTH: the minimum permissible length of this
     * node's value, in number of characters.  The default of -1 means
     * no minimum (same as zero).
     */
    int32 mur_len_min;

    /**
     * For MUTT_RESTRICT_LENGTH: the maximum permissible length of this
     * node's value, in number of characters.  The default of -1 means
     * no maximum.
     */
    int32 mur_len_max;

    /**
     * For MUTT_RESTRICT_LENGTH: the character with which to pad the
     * current value, if the value is shorter than the minimum length.
     * If there is no minimum length, or if the value is at least that
     * long already, this is not used.
     *
     * The default of '\\0' means to use our own default, which is the
     * underscore ('_').  If that character is not legal for this
     * value, you must provide a legal character here.
     */ 
    char mur_len_pad_char;

} md_upgrade_rule;


TYPED_ARRAY_HEADER_NEW_NONE(md_upgrade_rule, md_upgrade_rule *); 

int md_upgrade_rule_array_new(md_upgrade_rule_array **ret_arr);

/** Apply the specified upgrade rules to the provided database */
int md_upgrade_db_transform(const mdb_db *old_db, mdb_db *inout_new_db, 
                            uint32 from_module_version,
                            uint32 to_module_version, 
                            const md_upgrade_rule_array *rules);

/** Create a new empty rule record */
int md_upgrade_new_rule(const md_upgrade_rule_array *ruleset,
                        md_upgrade_rule **ret_rule);

/**
 * Add the specified rule to the rule list.  Use this in preference
 * to md_upgrade_rule_array_append_takeover() because it will
 * do some sanity checks to validate the rule.
 */
int md_upgrade_add_rule(md_upgrade_rule_array *ruleset,
                        md_upgrade_rule **inout_rule);

/**
 * Upgrade/downgrade function that can be passed for the updown_func
 * parameter to mdm_add_module().  This is simply a shim that calls
 * md_upgrade_db_transform() with the correct parameters according
 * to what it is passed.  It gets the rule array to pass from the
 * void *arg parameter.  arg should be of type
 * (md_upgrade_rule_array **).
 *
 * (Rationale: really all we want is an md_upgrade_rule_array *, but
 * this would require the caller to create their rule array before
 * calling mdm_add_module(), which is generally done at the beginning
 * of initialization.  This is a tradeoff between two possible
 * developer errors we can be vulnerable to: forgetting to initialize
 * the array soon enough, or omitting the '&' character in registration.
 * The rationale for the choice made is that a developer copying sample
 * code is more likely to realize that he cannot remove the '&' than
 * that he cannot move seemingly unrelated chunks of code around.
 */
int md_generic_upgrade_downgrade(const mdb_db *old_db,
                                 mdb_db *inout_new_db,
                                 uint32 from_module_version,
                                 uint32 to_module_version, void *arg);

/**
 * Creates and adds a set of rules to your rule array which, at
 * upgrade time, will add a set of nodes to the database during an
 * upgrade.  Is designed to take an array of bn_str_value, the same as
 * mdb_create_node_bindings(), so you can cut and paste the same code
 * from one to the other when you are adding new nodes.
 */
int md_upgrade_add_add_rules(md_upgrade_rule_array *rules,
                             const bn_str_value values[], uint32 num_values,
                             int32 old_mod_ver, int32 new_mod_ver);


/*
 * Some helper macros to make upgrade rule registration more compact
 */
#define MD_NEW_RULE(ug_rules, old_ver, new_ver)                             \
    err = md_upgrade_new_rule(ug_rules, &rule);                             \
    bail_error_null(err, rule);                                             \
    rule->mur_from_module_version = old_ver;                                \
    rule->mur_to_module_version = new_ver;

#define MD_ADD_RULE(ug_rules)                                               \
    err = md_upgrade_add_rule(ug_rules, &rule);                             \
    bail_error(err);

#ifdef __cplusplus
}
#endif

#endif /* __MD_UPGRADE_H_ */
