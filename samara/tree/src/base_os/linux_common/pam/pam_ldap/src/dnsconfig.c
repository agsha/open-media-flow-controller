
/* Copyright (C) 1997-2001 Luke Howard.
   This file started off as part of the nss_ldap library.
   Contributed by Luke Howard, <lukeh@padl.com>, 1997.
   (The author maintains a non-exclusive licence to distribute this file
   under their own conditions.)

   The nss_ldap library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The nss_ldap library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the nss_ldap library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
 */

/*
 * Support DNS SRV records. I look up the SRV record for
 * _ldap._tcp.gnu.org.
 * and build the DN DC=gnu,DC=org.
 * Thanks to Assar & co for resolve.[ch].
 */

static char rcsId[] = "$Id: dnsconfig.c,v 1.2 2010/11/24 00:29:37 gregs Exp $";

#include "config.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/param.h>
#include <netdb.h>
#include <syslog.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <resolv.h>
#include <string.h>

#ifdef HAVE_LBER_H
#include <lber.h>
#endif
#ifdef HAVE_LDAP_H
#include <ldap.h>
#endif

#ifndef HAVE_SNPRINTF
#include "snprintf.h"
#endif

#include "pam_ldap.h"
#include "resolve.h"
#include "dnsconfig.h"

#define DC_ATTR "DC"
#define DC_ATTR_AVA DC_ATTR "="
#define DC_ATTR_AVA_LEN (sizeof(DC_ATTR_AVA) - 1)

/* map gnu.org into DC=gnu,DC=org */
int
_pam_ldap_getdnsdn (char *src_domain, char **rval)
{
  char *p;
  int len = 0;
#ifdef HAVE_STRTOK_R
  char *st = NULL;
#endif
  char *domain;
  char domain_copy[BUFSIZ], buffer[BUFSIZ];

  /* we need to take a copy of domain, because strtok() modifies
   * it in place. Bad.
   */
  if (strlen (src_domain) >= sizeof (domain_copy))
    {
      return PAM_SYSTEM_ERR;
    }
  memset (domain_copy, '\0', sizeof (domain_copy));
  memset (buffer, '\0', sizeof (buffer));
  strcpy (domain_copy, src_domain);

  domain = domain_copy;

#ifndef HAVE_STRTOK_R
  while ((p = strtok (domain, ".")))
#else
  while ((p = strtok_r (domain, ".", &st)))
#endif
    {
      len = strlen (p);

      if (strlen (buffer) + DC_ATTR_AVA_LEN + len + 1 >= sizeof (buffer))
	{
	  return PAM_SYSTEM_ERR;
	}

      if (domain == NULL)
	{
	  strcat (buffer, ",");
	}
      else
	{
	  domain = NULL;
	}

      strcat (buffer, DC_ATTR_AVA);
      strcat (buffer, p);
    }

  if (rval != NULL)
    {
      *rval = strdup (buffer);
    }

  return PAM_SUCCESS;
}


int
_pam_ldap_readconfigfromdns (pam_ldap_config_t * result)
{
  int stat = PAM_SUCCESS;
  struct dns_reply *r;
  struct resource_record *rr;
  char domain[MAXHOSTNAMELEN + 1];

  /* only reinitialize variables we'll change here */
  result->host = NULL;
  result->base = NULL;
  result->port = LDAP_PORT;
#ifdef LDAP_VERSION3
  result->version = LDAP_VERSION3;
#else
  result->version = LDAP_VERSION2;
#endif /* LDAP_VERSION3 */

  if ((_res.options & RES_INIT) == 0 && res_init () == -1)
    {
      return PAM_SYSTEM_ERR;
    }

  snprintf (domain, sizeof (domain), "_ldap._tcp.%s.", _res.defdname);

  r = dns_lookup (domain, "srv");
  if (r == NULL)
    {
      return PAM_SYSTEM_ERR;
    }

  /* XXX need to sort by priority and reorder using weights */
  for (rr = r->head; rr != NULL; rr = rr->next)
    {
      if (rr->type == T_SRV)
	{
	  if (result->host != NULL)
	    {
	      /* need more space */
	      int length;
	      char *tmp;
	      length = strlen (result->host) + 1 +
	               strlen (rr->u.srv->target) + 1 + 5 + 1;
	      tmp = malloc (length);
	      if (tmp == NULL)
	        {
	          dns_free_data (r);
		  return PAM_BUF_ERR;
	        }
	      snprintf (tmp, length, "%s %s:%d", result->host, rr->u.srv->target,
	               rr->u.srv->port);
	      free (result->host);
	      result->host = tmp;
	    }
	  else
	    {
	      /* Server Host */
	      result->host = strdup (rr->u.srv->target);
	      if (result->host == NULL)
	        {
	          dns_free_data (r);
		  return PAM_BUF_ERR;
	        }
	      /* Port */
	      result->port = rr->u.srv->port;
	    }

#ifdef LDAPS_PORT
	  /* Hack: if the port is the registered SSL port, enable SSL. */
	  if (result->port == LDAPS_PORT)
	    {
	      result->ssl_on = SSL_LDAPS;
	    }
#endif /* SSL */

	  /* DN */
	  stat = _pam_ldap_getdnsdn (_res.defdname, &result->base);
	  if (stat != PAM_SUCCESS)
	    {
	      dns_free_data (r);
	      return stat;
	    }
	}
    }

  dns_free_data (r);
  stat = PAM_SUCCESS;

  return stat;
}
