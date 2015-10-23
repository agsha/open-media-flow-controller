/*
 * (C) Copyright 2015 Juniper Networks
 * All rights reserved.
 */

/* ------------------------------------------------------------------------- */
/* md_passwd.inc.c: shows how to use a graft point in md_passwd.c to
 * add another gid (group id) to the system.  Note that for groups
 * used to designate capability levels for normal visible user
 * accounts, this is not the place to add them.  These are groups to
 * be used by reserved accounts.
 */

/* Graft point 3: additional system groups */
#if MD_PASSWD_INC_GRAFT_POINT == 3

//    {"tmsresv",        2100},

#endif /* GRAFT_POINT 3 */

/* Graft point 2: set default passwords  */
#if MD_PASSWD_INC_GRAFT_POINT == 2
const bn_str_value md_nkn_passwd_initial_values[] = {
    {"/auth/passwd/user/__mfcd", bt_string, "__mfcd"},
    {"/auth/passwd/user/__mfcd/enable", bt_bool, "true"},
    {"/auth/passwd/user/__mfcd/password", bt_string,
	"$1$XZ3xQtuz$dCSRRI4iP.KWb8qSVfbQa."},
    {"/auth/passwd/user/__mfcd/uid", bt_uint32, "0"},
    {"/auth/passwd/user/__mfcd/gid", bt_uint32, "0"},
    {"/auth/passwd/user/__mfcd/gecos", bt_string, "DMI MFCD"},
    {"/auth/passwd/user/__mfcd/home_dir", bt_string, "/home/mfcd"},
    {"/auth/passwd/user/__mfcd/shell", bt_string, CLI_SHELL_PATH},
    {"/auth/passwd/user/__mfcd/newly_created", bt_bool, "false"},

};

err = mdb_create_node_bindings(commit, db, md_nkn_passwd_initial_values,
				sizeof(md_nkn_passwd_initial_values) / sizeof(bn_str_value));
bail_error(err);
#endif /* GRAFT_POINT 2 */



