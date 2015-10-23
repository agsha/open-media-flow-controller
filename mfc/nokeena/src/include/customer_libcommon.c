/*
 * (C) Copyright 2015 Juniper Networks
 * All rights reserved.
 */

/*
 * Do not include an rcsid definition because this file is not
 * compiled standalone; it is #included into another C file which
 * already has one of its own.
 */

int
lc_is_if_driver_virtio(const char *ifname, int ifc_sock, int *if_virtio);

#ifdef PROD_TARGET_OS_LINUX
typedef unsigned long long u64;/* hack, so we may include kernel's ethtool.h */
typedef __uint32_t u32;         /* ditto */
typedef __uint16_t u16;         /* ditto */
typedef __uint8_t u8;           /* ditto */

#include <linux/types.h>
#include <linux/sockios.h>
#include <linux/ethtool.h>
#include <linux/if.h>
#endif

int
lc_is_if_driver_virtio(const char *ifname, int ifc_sock, int *if_virtio)
{
    int err = 0;
    struct ifreq ifr;
    struct ethtool_drvinfo drvinfo;
    int errno_save = 0;

    bail_require(ifc_sock >= 0);

    memset(&ifr, 0, sizeof(ifr));
    drvinfo.cmd = ETHTOOL_GDRVINFO;
    strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name)-1);
    ifr.ifr_data = ( void *)&drvinfo;

    err = ioctl(ifc_sock, SIOCETHTOOL, &ifr);
    errno_save = errno;
    if (err < 0) {
	lc_log_basic(LOG_NOTICE, "ethtool error: %s/%d", ifname,errno_save);
	err = 0;
    } else {
	if (strcmp(drvinfo.driver, "virtio_net") == 0) {
	    *if_virtio = 1;
	    lc_log_basic(LOG_DEBUG, "found virtio interface: %s", ifname);
	} else {
	    *if_virtio = 0;
	    lc_log_basic(LOG_DEBUG, " not a ethtool interface: %s", ifname);
	}
    }
bail:
    return err;
}
/*
 * The following is the string that will be used when a password fails
 * validation.
 */
#define lc_password_invalid_error_msg                                         \
    D_("The password you entered does not meet the password requirements "    \
       "for\nthis system.", CUSTOMER_INCLUDE_GETTEXT_DOMAIN)

int lc_customer_password_validate(const char *password, tbool *ret_ok);


/* ------------------------------------------------------------------------- */
int
lc_customer_password_validate(const char *password, tbool *ret_ok)
{
    int err = 0;

    bail_null(password);
    bail_null(ret_ok);
    *ret_ok = true;

    /*
     * If you want to apply any custom checks to user passwords, put
     * them here.  Set *ret_ok to false if you don't approve of a
     * password.
     */

 bail:
    return(err);
}
