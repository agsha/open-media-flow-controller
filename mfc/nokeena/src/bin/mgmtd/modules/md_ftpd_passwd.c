
#include "common.h"
#include "dso.h"
#include "mdb_db.h"
#include "md_mod_reg.h"


int md_ftpd_passwd_init(const lc_dso_info *info, void *data);



const bn_str_value md_ftpd_passwd_initial_values[] = {
    {"/auth/passwd/user/ftp", bt_string, "ftp"},
    {"/auth/passwd/user/ftp/enable", bt_bool, "true"},
    {"/auth/passwd/user/ftp/password", bt_string, "*"},
    {"/auth/passwd/user/ftp/uid", bt_uint32, "50"},
    {"/auth/passwd/user/ftp/gid", bt_uint32, "50"},
    {"/auth/passwd/user/ftp/gecos", bt_string, "FTP User"},
    {"/auth/passwd/user/ftp/home_dir", bt_string, "/var/hello"},
    {"/auth/passwd/user/ftp/shell", bt_string, "/usr/bin/true"},
};


static int
md_ftpd_passwd_add_initial(md_commit *commit, mdb_db *db, void *arg)
{
    int err = 0;

    err = mdb_create_node_bindings
            (commit, db, md_ftpd_passwd_initial_values,
          sizeof(md_ftpd_passwd_initial_values) / sizeof(bn_str_value));
    bail_error(err);

bail:
    return(err);
}


int md_ftpd_passwd_init(
        const lc_dso_info *info,
        void *data)
{
    int err = 0;

    md_module *module = NULL;
    md_reg_node *node = NULL;

    err = mdm_add_module(
            "ftpd_passwd",
            1,
            "/auth/customer/ftpd", NULL,
            modrf_namespace_unrestricted,
            NULL, NULL, NULL, NULL, NULL, NULL, 0, 0,
            NULL, NULL, md_ftpd_passwd_add_initial, NULL,
            NULL, NULL, NULL, NULL, &module);
    bail_error(err);

bail:
    return err;
}
