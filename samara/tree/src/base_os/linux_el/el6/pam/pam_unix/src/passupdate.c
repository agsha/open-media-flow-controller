/*
 * Main coding by Elliot Lee <sopwith@redhat.com>, Red Hat Software.
 * Copyright (C) 1996.
 * Copyright (c) Jan Rêkorajski, 1999.
 * Copyright (c) Red Hat, Inc., 2007
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, and the entire permission notice in its entirety,
 *    including the disclaimer of warranties.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * ALTERNATIVELY, this product may be distributed under the terms of
 * the GNU Public License, in which case the provisions of the GPL are
 * required INSTEAD OF the above restrictions.  (This clause is
 * necessary due to a potential bad interaction between the GPL and
 * the restrictions contained in a BSD-style copyright.)
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* this will be included from module and update helper */

#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>

#if defined(USE_LCKPWDF) && !defined(HAVE_LCKPWDF)
# include "./lckpwdf.-c"
#endif

/* passwd/salt conversion macros */

#define ascii_to_bin(c) ((c)>='a'?(c-59):(c)>='A'?((c)-53):(c)-'.')
#define bin_to_ascii(c) ((c)>=38?((c)-38+'a'):(c)>=12?((c)-12+'A'):(c)+'.')

#define PW_TMPFILE		"/etc/npasswd"
#define SH_TMPFILE		"/etc/nshadow"
#define OPW_TMPFILE		"/etc/security/nopasswd"
#define OLD_PASSWORDS_FILE	"/etc/security/opasswd"

/*
 * i64c - convert an integer to a radix 64 character
 */
static int i64c(int i)
{
	if (i < 0)
		return ('.');
	else if (i > 63)
		return ('z');
	if (i == 0)
		return ('.');
	if (i == 1)
		return ('/');
	if (i >= 2 && i <= 11)
		return ('0' - 2 + i);
	if (i >= 12 && i <= 37)
		return ('A' - 12 + i);
	if (i >= 38 && i <= 63)
		return ('a' - 38 + i);
	return ('\0');
}

static void
crypt_make_salt(char *where, int length)
{
        struct timeval tv;
        MD5_CTX ctx;
        unsigned char tmp[16];
        unsigned char *src = (unsigned char *)where;
        int i;
#ifdef PATH_RANDOMDEV
	int fd;
	int rv;

	if ((rv = fd = open (PATH_RANDOMDEV, O_RDONLY)) != -1) {
		while ((rv = read(fd, where, length)) != length && errno == EINTR);
		close (fd);
	}
	if (rv != length) {
#endif
        /*
         * Code lifted from Marek Michalkiewicz's shadow suite. (CG)
         * removed use of static variables (AGM)
         *
	 * will work correctly only for length <= 16 */
	src = tmp;
        GoodMD5Init(&ctx);
        gettimeofday(&tv, (struct timezone *) 0);
        GoodMD5Update(&ctx, (void *) &tv, sizeof tv);
        i = getpid();
        GoodMD5Update(&ctx, (void *) &i, sizeof i);
        i = clock();
        GoodMD5Update(&ctx, (void *) &i, sizeof i);
        GoodMD5Update(&ctx, src, length);
        GoodMD5Final(tmp, &ctx);
#ifdef PATH_RANDOMDEV
	}
#endif
        for (i = 0; i < length; i++)
                *where++ = i64c(src[i] & 077);
        *where = '\0';
}

static char *
crypt_md5_wrapper(const char *pass_new)
{
        unsigned char result[16];
        char *cp = (char *) result;

        cp = stpcpy(cp, "$1$");      /* magic for the MD5 */
	crypt_make_salt(cp, 9);

        /* no longer need cleartext */
        cp = Goodcrypt_md5(pass_new, (const char *) result);
	pass_new = NULL;

        return cp;
}

#ifndef HELPER_COMPILE
static char *
create_password_hash(const char *password, unsigned int ctrl, int rounds)
{
	const char *algoid;
	char salt[64]; /* contains rounds number + max 16 bytes of salt + algo id */
	char *sp;

	if (on(UNIX_MD5_PASS, ctrl)) {
		return crypt_md5_wrapper(password);
	}
	if (on(UNIX_SHA256_PASS, ctrl)) {
		algoid = "$5$";
	} else if (on(UNIX_SHA512_PASS, ctrl)) {
		algoid = "$6$";
	} else { /* must be crypt/bigcrypt */
		char tmppass[9];
		char *crypted;

		crypt_make_salt(salt, 2);
		if (off(UNIX_BIGCRYPT, ctrl) && strlen(password) > 8) {
			strncpy(tmppass, password, sizeof(tmppass)-1);
			tmppass[sizeof(tmppass)-1] = '\0';
			password = tmppass;
		}
		crypted = bigcrypt(password, salt);
		memset(tmppass, '\0', sizeof(tmppass));
		password = NULL;
		return crypted;
	}

	sp = stpcpy(salt, algoid);
	if (on(UNIX_ALGO_ROUNDS, ctrl)) {
		sp += snprintf(sp, sizeof(salt) - 3, "rounds=%u$", rounds);
	}
	crypt_make_salt(sp, 8);
	/* For now be conservative so the resulting hashes
	 * are not too long. 8 bytes of salt prevents dictionary
	 * attacks well enough. */
	return x_strdup(crypt(password, salt));
}
#endif

#ifdef USE_LCKPWDF
static int lock_pwdf(void)
{
	int i;
	int retval;

#ifndef HELPER_COMPILE
	if (selinux_confined()) {
		return PAM_SUCCESS;
	}
#endif
	/* These values for the number of attempts and the sleep time
	   are, of course, completely arbitrary.
	   My reading of the PAM docs is that, once pam_chauthtok() has been
	   called with PAM_UPDATE_AUTHTOK, we are obliged to take any
	   reasonable steps to make sure the token is updated; so retrying
	   for 1/10 sec. isn't overdoing it. */
	i=0;
	while((retval = lckpwdf()) != 0 && i < 100) {
		usleep(1000);
		i++;
	}
	if(retval != 0) {
		return PAM_AUTHTOK_LOCK_BUSY;
	}
	return PAM_SUCCESS;
}

static void unlock_pwdf(void)
{
#ifndef HELPER_COMPILE
	if (selinux_confined()) {
		return;
	}
#endif
	ulckpwdf();
}
#endif

static int
save_old_password(const char *forwho, const char *oldpass,
		  int howmany)
{
    static char buf[16384];
    static char nbuf[16384];
    char *s_luser, *s_uid, *s_npas, *s_pas, *pass;
    int npas;
    FILE *pwfile, *opwfile;
    int err = 0;
    int oldmask;
    int found = 0;
    struct passwd *pwd = NULL;
    struct stat st;

    if (howmany < 0) {
	return PAM_SUCCESS;
    }

    if (oldpass == NULL) {
	return PAM_SUCCESS;
    }

    oldmask = umask(077);

#ifdef WITH_SELINUX
    if (SELINUX_ENABLED) {
      security_context_t passwd_context=NULL;
      if (getfilecon("/etc/passwd",&passwd_context)<0) {
        return PAM_AUTHTOK_ERR;
      };
      if (getfscreatecon(&prev_context)<0) {
        freecon(passwd_context);
        return PAM_AUTHTOK_ERR;
      }
      if (setfscreatecon(passwd_context)) {
        freecon(passwd_context);
        freecon(prev_context);
        return PAM_AUTHTOK_ERR;
      }
      freecon(passwd_context);
    }
#endif
    pwfile = fopen(OPW_TMPFILE, "w");
    umask(oldmask);
    if (pwfile == NULL) {
      err = 1;
      goto done;
    }

    opwfile = fopen(OLD_PASSWORDS_FILE, "r");
    if (opwfile == NULL) {
	fclose(pwfile);
      err = 1;
      goto done;
    }

    if (fstat(fileno(opwfile), &st) == -1) {
	fclose(opwfile);
	fclose(pwfile);
	err = 1;
	goto done;
    }

    if (fchown(fileno(pwfile), st.st_uid, st.st_gid) == -1) {
	fclose(opwfile);
	fclose(pwfile);
	err = 1;
	goto done;
    }
    if (fchmod(fileno(pwfile), st.st_mode) == -1) {
	fclose(opwfile);
	fclose(pwfile);
	err = 1;
	goto done;
    }

    while (fgets(buf, 16380, opwfile)) {
	if (!strncmp(buf, forwho, strlen(forwho))) {
	    char *sptr = NULL;
	    found = 1;
	    if (howmany == 0)
	    	continue;
	    buf[strlen(buf) - 1] = '\0';
	    s_luser = strtok_r(buf, ":", &sptr);
	    s_uid = strtok_r(NULL, ":", &sptr);
	    s_npas = strtok_r(NULL, ":", &sptr);
	    s_pas = strtok_r(NULL, ":", &sptr);
	    npas = strtol(s_npas, NULL, 10) + 1;
	    while (npas > howmany) {
		s_pas = strpbrk(s_pas, ",");
		if (s_pas != NULL)
		    s_pas++;
		npas--;
	    }
	    pass = crypt_md5_wrapper(oldpass);
	    if (s_pas == NULL)
		snprintf(nbuf, sizeof(nbuf), "%s:%s:%d:%s\n",
			 s_luser, s_uid, npas, pass);
	    else
		snprintf(nbuf, sizeof(nbuf),"%s:%s:%d:%s,%s\n",
			 s_luser, s_uid, npas, s_pas, pass);
	    _pam_delete(pass);
	    if (fputs(nbuf, pwfile) < 0) {
		err = 1;
		break;
	    }
	} else if (fputs(buf, pwfile) < 0) {
	    err = 1;
	    break;
	}
    }
    fclose(opwfile);

    if (!found) {
	pwd = getpwnam(forwho);
	if (pwd == NULL) {
	    err = 1;
	} else {
	    pass = crypt_md5_wrapper(oldpass);
	    snprintf(nbuf, sizeof(nbuf), "%s:%lu:1:%s\n",
		     forwho, (unsigned long)pwd->pw_uid, pass);
	    _pam_delete(pass);
	    if (fputs(nbuf, pwfile) < 0) {
		err = 1;
	    }
	}
    }

    if (fclose(pwfile)) {
	D(("error writing entries to old passwords file: %m"));
	err = 1;
    }

done:
    if (!err) {
	if (rename(OPW_TMPFILE, OLD_PASSWORDS_FILE))
	    err = 1;
    }
#ifdef WITH_SELINUX
    if (SELINUX_ENABLED) {
      if (setfscreatecon(prev_context)) {
        err = 1;
      }
      if (prev_context)
        freecon(prev_context);
      prev_context=NULL;
    }
#endif
    if (!err) {
	return PAM_SUCCESS;
    } else {
	unlink(OPW_TMPFILE);
	return PAM_AUTHTOK_ERR;
    }
}

#ifdef HELPER_COMPILE
static int
_update_passwd(const char *forwho, const char *towhat)
#else
static int
_update_passwd(pam_handle_t *pamh, const char *forwho, const char *towhat)
#endif
{
    struct passwd *tmpent = NULL;
    struct stat st;
    FILE *pwfile, *opwfile;
    int err = 1;
    int oldmask;

    oldmask = umask(077);
#ifdef WITH_SELINUX
    if (SELINUX_ENABLED) {
      security_context_t passwd_context=NULL;
      if (getfilecon("/etc/passwd",&passwd_context)<0) {
	return PAM_AUTHTOK_ERR;
      };
      if (getfscreatecon(&prev_context)<0) {
	freecon(passwd_context);
	return PAM_AUTHTOK_ERR;
      }
      if (setfscreatecon(passwd_context)) {
	freecon(passwd_context);
	freecon(prev_context);
	return PAM_AUTHTOK_ERR;
      }
      freecon(passwd_context);
    }
#endif
    pwfile = fopen(PW_TMPFILE, "w");
    umask(oldmask);
    if (pwfile == NULL) {
      err = 1;
      goto done;
    }

    opwfile = fopen("/etc/passwd", "r");
    if (opwfile == NULL) {
	fclose(pwfile);
	err = 1;
	goto done;
    }

    if (fstat(fileno(opwfile), &st) == -1) {
	fclose(opwfile);
	fclose(pwfile);
	err = 1;
	goto done;
    }

    if (fchown(fileno(pwfile), st.st_uid, st.st_gid) == -1) {
	fclose(opwfile);
	fclose(pwfile);
	err = 1;
	goto done;
    }
    if (fchmod(fileno(pwfile), st.st_mode) == -1) {
	fclose(opwfile);
	fclose(pwfile);
	err = 1;
	goto done;
    }

    tmpent = fgetpwent(opwfile);
    while (tmpent) {
	if (!strcmp(tmpent->pw_name, forwho)) {
	    /* To shut gcc up */
	    union {
		const char *const_charp;
		char *charp;
	    } assigned_passwd;
	    assigned_passwd.const_charp = towhat;

	    tmpent->pw_passwd = assigned_passwd.charp;
	    err = 0;
	}
	if (putpwent(tmpent, pwfile)) {
	    D(("error writing entry to password file: %m"));
	    err = 1;
	    break;
	}
	tmpent = fgetpwent(opwfile);
    }
    fclose(opwfile);

    if (fclose(pwfile)) {
	D(("error writing entries to password file: %m"));
	err = 1;
    }

done:
    if (!err) {
	if (!rename(PW_TMPFILE, "/etc/passwd"))
#ifdef HELPER_COMPILE
	    _log_err(
#else
	    pam_syslog(pamh, 
#endif
		LOG_NOTICE, "password changed for %s", forwho);
	else
	    err = 1;
    }
#ifdef WITH_SELINUX
    if (SELINUX_ENABLED) {
      if (setfscreatecon(prev_context)) {
	err = 1;
      }
      if (prev_context)
	freecon(prev_context);
      prev_context=NULL;
    }
#endif
    if (!err) {
	return PAM_SUCCESS;
    } else {
	unlink(PW_TMPFILE);
	return PAM_AUTHTOK_ERR;
    }
}

#ifdef HELPER_COMPILE
static int
_update_shadow(const char *forwho, char *towhat)
#else
static int
_update_shadow(pam_handle_t *pamh, const char *forwho, char *towhat)
#endif
{
    struct spwd *spwdent = NULL, *stmpent = NULL;
    struct stat st;
    FILE *pwfile, *opwfile;
    int err = 1;
    int oldmask;

    spwdent = getspnam(forwho);
    if (spwdent == NULL) {
	return PAM_USER_UNKNOWN;
    }
    oldmask = umask(077);

#ifdef WITH_SELINUX
    if (SELINUX_ENABLED) {
      security_context_t shadow_context=NULL;
      if (getfilecon("/etc/shadow",&shadow_context)<0) {
	return PAM_AUTHTOK_ERR;
      };
      if (getfscreatecon(&prev_context)<0) {
	freecon(shadow_context);
	return PAM_AUTHTOK_ERR;
      }
      if (setfscreatecon(shadow_context)) {
	freecon(shadow_context);
	freecon(prev_context);
	return PAM_AUTHTOK_ERR;
      }
      freecon(shadow_context);
    }
#endif
    pwfile = fopen(SH_TMPFILE, "w");
    umask(oldmask);
    if (pwfile == NULL) {
	err = 1;
	goto done;
    }

    opwfile = fopen("/etc/shadow", "r");
    if (opwfile == NULL) {
	fclose(pwfile);
	err = 1;
	goto done;
    }

    if (fstat(fileno(opwfile), &st) == -1) {
	fclose(opwfile);
	fclose(pwfile);
	err = 1;
	goto done;
    }

    if (fchown(fileno(pwfile), st.st_uid, st.st_gid) == -1) {
	fclose(opwfile);
	fclose(pwfile);
	err = 1;
	goto done;
    }
    if (fchmod(fileno(pwfile), st.st_mode) == -1) {
	fclose(opwfile);
	fclose(pwfile);
	err = 1;
	goto done;
    }

    stmpent = fgetspent(opwfile);
    while (stmpent) {

	if (!strcmp(stmpent->sp_namp, forwho)) {
	    stmpent->sp_pwdp = towhat;
	    stmpent->sp_lstchg = time(NULL) / (60 * 60 * 24);
	    err = 0;
	    D(("Set password %s for %s", stmpent->sp_pwdp, forwho));
	}

	if (putspent(stmpent, pwfile)) {
	    D(("error writing entry to shadow file: %m"));
	    err = 1;
	    break;
	}

	stmpent = fgetspent(opwfile);
    }
    fclose(opwfile);

    if (fclose(pwfile)) {
	D(("error writing entries to shadow file: %m"));
	err = 1;
    }

 done:
    if (!err) {
	if (!rename(SH_TMPFILE, "/etc/shadow"))
#ifdef HELPER_COMPILE
	    _log_err(
#else
	    pam_syslog(pamh, 
#endif
		LOG_NOTICE, "password changed for %s", forwho);
	else
	    err = 1;
    }

#ifdef WITH_SELINUX
    if (SELINUX_ENABLED) {
      if (setfscreatecon(prev_context)) {
	err = 1;
      }
      if (prev_context)
	freecon(prev_context);
      prev_context=NULL;
    }
#endif

    if (!err) {
	return PAM_SUCCESS;
    } else {
	unlink(SH_TMPFILE);
	return PAM_AUTHTOK_ERR;
    }
}
