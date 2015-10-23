/*

 $Id: ssmtp.c,v 1.25 2012/08/26 20:53:32 et Exp $

 sSMTP -- send messages via SMTP to a mailhub for local delivery or forwarding.
 This program is used in place of /usr/sbin/sendmail, called by "mail" (et al.).
 sSMTP does a selected subset of sendmail's standard tasks (including exactly
 one rewriting task), and explains if you ask it to do something it can't. It
 then sends the mail to the mailhub via an SMTP connection. Believe it or not,
 this is nothing but a filter

 See COPYRIGHT for the license

*/

/* 
 * NOTE: This file has been modified by Tall Maple Systems, Inc. 
 *
 * The changes are:
 *
 *   - Support for a backup mailhub, with optional SMTP authentication
 *     for this backup mailhub.
 *
 *   - Support for alternate configuration files.
 *
 *   - Continue sending to other recipients if one recipient is invalid.
 *
 *   - Tweak timeouts.
 *
 *   - Fix handling of 64-character hostnames.
 *
 *   - Change destination directory and naming for dead.letter.
 *
 *   - Support "DeadLetterEnable" option in config file, which can be set
 *     to "n" to disable writing of any dead.letter files.  (It defaults
 *     to enabled, and anything other than "y" will disable)
 *
 *   - Support "DeadLetterSeparate" option in config file, to request that
 *     each dead.letter be given a unique filename, instead of appending
 *     everything to the same file.  This causes a timestamp and the pid of
 *     the ssmtp process to be appended to the filename, and the file to be
 *     placed in the ~/dead.letters directory, instead of just ~ (the user's
 *     home directory).  Set to "y" to enable this behavior.
 *
 *   - Support "DeadLetterMaxAge" option in config file, to request that
 *     any dead.letter files older than the specified amount (in seconds)
 *     be deleted.
 *
 *   - Apply patches described in change of 2009/03/06 19:16:16 -0800.
 *
 *   - Interpret escape sequences placed in the config file by TMS software to
 *     work around native token parsing limitations.  This allows special
 *     characters that normally serve as delimiters (" \t\r\n:#") to be in the
 *     original configuration fields without disturbing the parser logic.  This
 *     is required (among other things) for proper IPv6 support as the ':'
 *     character in an IPv6 address thwarts the native ssmtp config parse
 *     logic.
 *
 *   - Additional TLS support, in several parts:
 *
 *       - Support the "UseSTARTTLS" config directive.  This is taken from
 *         sSMTP upstream, via a patch provided at:
 *
 *            http://bugs.debian.org/cgi-bin/bugreport.cgi?
 *            msg=5;filename=patch.starttls;att=1;bug=244666
 *
 *         which was found from:
 *
 *            http://bugs.debian.org/cgi-bin/bugreport.cgi?bug=244666
 *
 *       - Support the "TLSCiphers" directive, which gives a list of
 *         names of ciphers we wish to accept, to be passed to
 *         SSL_CTX_set_cipher_list().
 *
 *       - Support the "TLSCertVerify" directive.  A "YES" value for this
 *         (in conjunction with UseTLS and/or UseSTARTTLS) means that we 
 *         will check the validity of the server's certificate using our
 *         CA list.  If it is found to be invalid, we fail out (but see
 *         'STARTTLSFallback' directive below for an exception).
 *
 *       - Support the "TLSCACertFile" and "TLSCACertDir" directives, which
 *         gives a full path to a file and/or a directory, respectively,
 *         which are passed to SSL_CTX_load_verify_locations().
 *         The file typically has some base set of certificates that come
 *         with the system, while the directory is available for
 *         adding extra certificates.
 *
 *       - Support the "STARTTLSFallback" directive.  A "YES" value for
 *         this says that if TLS fails, we should fall back to plaintext
 *         rather than failing out.  The failure can be at one of two
 *         points, and fallback is handled differently depending on which
 *         is the site of failure:
 *
 *         1. When we first issue the "STARTTLS" command.  An error message
 *            here means the server does not want to do TLS, but we have an
 *            otherwise perfectly fine SMTP session still open with it, so
 *            we just proceed from here in plaintext.
 *
 *         2. After "STARTTLS" succeeds, when we are trying to set up 
 *            SSL communications, or when we are verifying the server cert.
 *            If this fails, we cannot just continue with the current 
 *            session in plaintext since the STARTTLS has already been 
 *            issued and succeeded.  So we break the connection and start
 *            a new one, in plaintext.
 *
 * This program is distributed under the GNU General Public License version 2,
 *     with ABSOLUTELY NO WARRANTY.
 */


#define VERSION "2.60.4"

#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/param.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <syslog.h>
#include <signal.h>
#include <setjmp.h>
#include <string.h>
#include <ctype.h>
#include <netdb.h>
#include <time.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>

#ifdef HAVE_SSL
#include <openssl/crypto.h>
#include <openssl/x509.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#endif
#ifdef MD5AUTH
#include "md5auth/hmac_md5.h"
#endif
#include "ssmtp.h"

#include "escape_utils.h"

/* config file string unescape definitions */
#define CFG_ESC_CHR '\\'
#define CFG_STR_UNESC(S) lc_unescape_str_quick(S, CFG_ESC_CHR, lucf_defaults)
#define DIE_UNESC(F) die("%s -- lc_unescape_str_quick() failed", F)
/*
 * XXX/SML:  prior to TMS changes, calls to "die" from read_config advertised
 * themselves as "parse_config".  Preserving this for legacy parallelism only.
 */
#define DIE_UNESC_PARSECONFIG() DIE_UNESC("parse_config")

#define SAFE_FREE(var)                                                  \
        do {                                                            \
                if (var) {                                              \
                        free(var);                                      \
                        var = NULL;                                     \
                }                                                       \
        } while(0)


typedef enum {
    sfr_none = 0,
    sfr_starttls_not_supported,
    sfr_cert_verify_failure,
} ssmtp_failure_reason;

ssmtp_failure_reason failure_reason = sfr_none;

bool_t have_date = False;
bool_t have_from = False;
#ifdef HASTO_OPTION
bool_t have_to = False;
#endif
bool_t minus_t = False;
bool_t minus_v = False;
bool_t override_from = False;
bool_t rewrite_domain = False;
bool_t use_tls = False;			/* Use SSL to transfer mail to HUB */
bool_t use_starttls = False;            /* SSL only after STARTTLS (RFC2487) */
bool_t cert_verify = False;             /* Verify server cert */
bool_t starttls_fallback = False;       /* Allow fallback to plaintext if STARTTLS fails */
bool_t use_cert = False;		/* Use a certificate to transfer SSL mail */
char *tls_ciphers = (char *)NULL;
char *tls_cacert_file = (char *)NULL;
char *tls_cacert_dir = (char *)NULL;
bool_t doing_ehlo = False;
bool_t starttls_supported = False;

#define ARPADATE_LENGTH 32		/* Current date in RFC format */
char arpadate[ARPADATE_LENGTH];

char *auth_user = (char *)NULL;
char *auth_pass = (char *)NULL;
char *auth_method = (char *)NULL;      /* Mechanism for SMTP authentication */
bool_t use_oldauth = False;            /* use old AUTH LOGIN username style */

/*
 * Auth info for use with backup mailhub.
 */
char *backup_auth_user = (char *)NULL;
char *backup_auth_pass = (char *)NULL;
char *backup_auth_method = (char *)NULL;
bool_t backup_use_oldauth = False;

char *mail_domain = (char *)NULL;
char *from = (char *)NULL;		/* Use this as the From: address */
char hostname[MAXHOSTNAMELEN+1] = "localhost"; /* +1 to allow NUL delimiter */
char *mailhost = NULL;
char *backup_mailhost = NULL;
char *minus_f = (char *)NULL;
char *minus_F = (char *)NULL;
char *gecos;
char *prog = (char *)NULL;
char *root = "postmaster";
char *tls_cert = "/etc/ssl/certs/ssmtp.pem";	/* Default Certificate */
char *uad = (char *)NULL;
char *config_file = NULL;
int dl_enable = 1;
int dl_separate = 0;
long dl_max_age = 0;

headers_t headers, *ht;

void die(char *format, ...);

#ifdef DEBUG
int log_level = 1;
#else
int log_level = 0;
#endif

int mailhost_port = 25;
int backup_mailhost_port = 25;

#ifdef INET6
int p_family = PF_UNSPEC;		/* Protocol family used in SMTP connection */
#endif

jmp_buf TimeoutJmpBuf;			/* Timeout waiting for input from network */

rcpt_t rcpt_list, *rt;

#ifdef HAVE_SSL
SSL *ssl;
#endif

#ifdef MD5AUTH
static char hextab[]="0123456789abcdef";
#endif

#define NUL ((char) '\0')

/*
log_event() -- Write event to syslog (or log file if defined)
*/
void log_event(int priority, char *format, ...)
{
	char buf[(BUF_SZ + 1)];
	va_list ap;

	va_start(ap, format);
	(void)vsnprintf(buf, BUF_SZ, format, ap);
	va_end(ap);

#ifdef LOGFILE
	if((fp = fopen("/tmp/ssmtp.log", "a")) != (FILE *)NULL) {
		(void)fprintf(fp, "%s\n", buf);
		(void)fclose(fp);
	}
	else {
		(void)fprintf(stderr, "Can't write to /tmp/ssmtp.log\n");
	}
#endif

#if HAVE_SYSLOG_H
#if OLDSYSLOG
	openlog("sSMTP", LOG_PID);
#else
	openlog("sSMTP", LOG_PID, LOG_MAIL);
#endif
	syslog(priority, "%s", buf);
	closelog();
#endif
}

/*
 * Append 'newstr' onto the end of 'base', and make sure the result is
 * NUL-terminated.  Do not grow base longer than 'maxlen' characters,
 * including the terminating NUL.  This is better than strncat()
 * because that takes the maximum number of chars to take from
 * 'newstr', irrespective of how much space is left in 'base'.
 *
 * Returns 'base'.
 */
char *safe_strcat(char *base, const char *newstr, int maxlen)
{
	int maxchars = 0;

	if (base == NULL || newstr == NULL) {
		return(base);
	}

	maxchars = maxlen - strlen(base) - 1;
	if (maxchars <= 0) {
		return(base);
	}

	strncat(base, newstr, maxchars);

	/* Should not be necessary, strncat() is supposed to do this */
	base[maxlen - 1] = '\0';

	return(base);
}

void dead_letter_cleanup(const char *dir_path, long max_age)
{
	int err = 0;
	int numchars = 0;
	DIR *dir = NULL;
	struct dirent *ent = NULL; 
	char path[(MAXPATHLEN + 1)];
	struct stat st;
	time_t now = 0, mtime = 0;

	dir = opendir(dir_path);
	if (dir == NULL) {
		die("Opendir(\"%s\") failed", dir_path);
	}

	memset(&st, 0, sizeof(st));
	now = time(NULL);

	while ((ent = readdir(dir)) != NULL) {
		numchars = snprintf(path, MAXPATHLEN, "%s/%s", dir_path,
				    ent->d_name);
		if (numchars < 0) {
			log_event(LOG_ERR, "Snprintf() failed");
			continue;
		}
		err = stat(path, &st);
		if (err) {
			log_event(LOG_ERR, "Stat(\"%s\") failed", path);
			continue;
		}

		if (!S_ISREG(st.st_mode)) {
			continue;
		}

		mtime = st.st_mtime;
		if (now > mtime && (now - mtime) > max_age) {
			err = unlink(path);
			if (err) {
				log_event(LOG_ERR, "Error cleaning up old "
						   "file: %s", path);
			}
			else {
				log_event(LOG_INFO, "Cleaned up old file: %s",
					  path);
			}
		}
	}

	err = closedir(dir);
	if (err) {
		die("Closedir() failed");
	}
}

void smtp_write(int fd, char *format, ...);
int smtp_read(int fd, char *response);
int smtp_read_all(int fd, char *response);
int smtp_okay(int fd, char *response);

/*
dead_letter() -- Save stdin to dead.letter file if possible
*/
void dead_letter(void)
{
	int err = 0;
	int nc = 0;
	char path[(MAXPATHLEN + 1)], buf[(BUF_SZ + 1)];
	char dir[(MAXPATHLEN + 1)];
	struct passwd *pw;
	uid_t uid;
	FILE *fp;
	time_t tt = 0;	
	struct tm tm;
	int running = 0;

	/* Prevent reentrancy, so we can call die() */
	if (running) {
		return;
	}
	running = 1;

	if (!dl_enable) {
		goto done;
	}

	memset(path, 0, MAXPATHLEN + 1);
	memset(&tm, 0, sizeof(tm));
	uid = getuid();
	pw = getpwuid(uid);

	if (dl_separate) {
		nc = snprintf(dir, MAXPATHLEN, "%s/dead.letters", pw->pw_dir);
		if (nc == -1) {
			die("Snprintf() failed (making dead.letters path)");
		}
		mkdir(dir, S_IRUSR | S_IWUSR | S_IXUSR);
		/*
		 * Ignore errors for now, since dir may already be there.
		 * Would be nicer to verify it's there...
		 */
	}
	else {
		nc = snprintf(dir, MAXPATHLEN, "%s", pw->pw_dir);
		if (nc == -1) {
			die("Snprintf() failed (making dead.letters path)");
		}
	}
		
	if (dl_max_age > 0) {
		dead_letter_cleanup(dir, dl_max_age);
	}

	if(snprintf(path, MAXPATHLEN, "%s/dead.letter", dir) == -1) {
		die("Snprintf() failed (dead.letter path)");
	}

	if (dl_separate) {
		tt = time(NULL);
		localtime_r(&tt, &tm);
		if (snprintf(buf, BUF_SZ, "-%04d%02d%02d-%02d%02d%02d-%d",
			tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
			tm.tm_hour, tm.tm_min, tm.tm_sec, getpid()) == -1) {
			die("Snprintf() failed (timestamp)");
		}
		safe_strcat(path, buf, MAXPATHLEN);
	}

	if(isatty(fileno(stdin))) {
		if(log_level > 0) {
			log_event(LOG_ERR,
				"stdin is a TTY - not saving to %s, path");
		}
		goto done;
	}

	if(pw == (struct passwd *)NULL) {
		/* Far to early to save things */
		if(log_level > 0) {
			log_event(LOG_ERR, "No sender failing horribly!");
		}
		goto done;
	}

	if((fp = fopen(path, "a")) == (FILE *)NULL) {
		/* Perhaps the person doesn't have a homedir... */
		if(log_level > 0) {
			log_event(LOG_ERR, "Can't open %s failing horribly!", path);
		}
		goto done;
	}

	/* We start on a new line with a blank line separating messages */
	(void)fprintf(fp, "\n\n");

	while(fgets(buf, sizeof(buf), stdin)) {
		(void)fputs(buf, fp);
	}

	if(fclose(fp) == -1) {
		if(log_level > 0) {
			log_event(LOG_ERR,
				"Can't close %s, possibly truncated", path);
		}
	}

	err = chmod(path, S_IRUSR | S_IWUSR);
	if (err) {
		log_event(LOG_ERR, "Could not chmod %s", path);
	}

	log_event(LOG_INFO, "Saved undeliverable mail to %s", path);

 done:
	running = 0;
	return;
}

/*
die() -- Write error message, dead.letter and exit
*/
void die(char *format, ...)
{
	char buf[(BUF_SZ + 1)];
	va_list ap;

	va_start(ap, format);
	(void)vsnprintf(buf, BUF_SZ, format, ap);
	va_end(ap);

	(void)fprintf(stderr, "%s: %s\n", prog, buf);
	log_event(LOG_ERR, "%s", buf);

	/* Send message to dead.letter */
	(void)dead_letter();

	log_event(LOG_INFO, "Exiting with error");

	exit(1);
}

/*
basename() -- Return last element of path
*/
char *basename(char *str)
{
	char buf[MAXPATHLEN +1], *p;

	if((p = strrchr(str, '/'))) {
		if(strncpy(buf, ++p, MAXPATHLEN) == (char *)NULL) {
			die("basename() -- strncpy() failed");
		}
	}
	else {
		if(strncpy(buf, str, MAXPATHLEN) == (char *)NULL) {
			die("basename() -- strncpy() failed");
		}
	}
	buf[MAXPATHLEN] = NUL;

	return(strdup(buf));
}

/*
strip_pre_ws() -- Return pointer to first non-whitespace character
*/
char *strip_pre_ws(char *str)
{
	char *p;

	p = str;
	while(*p && isspace(*p)) p++;

	return(p);
}

/*
strip_post_ws() -- Return pointer to last non-whitespace character
*/
char *strip_post_ws(char *str)
{
	char *p;

	p = (str + strlen(str));
	while(isspace(*--p)) {
		*p = NUL;
	}

	return(p);
}

/*
addr_parse() -- Parse <user@domain.com> from full email address
*/
char *addr_parse(char *str)
{
	char *p, *q;

#if 0
	(void)fprintf(stderr, "*** addr_parse(): str = [%s]\n", str);
#endif

	/* Simple case with email address enclosed in <> */
	if((p = strdup(str)) == (char *)NULL) {
		die("addr_parse(): strdup()");
	}

	if((q = strchr(p, '<'))) {
		q++;

		if((p = strchr(q, '>'))) {
			*p = NUL;
		}

#if 0
		(void)fprintf(stderr, "*** addr_parse(): q = [%s]\n", q);
#endif

		return(q);
	}

	q = strip_pre_ws(p);
	if(*q == '(') {
		while((*q++ != ')'));
	}
	p = strip_pre_ws(q);

#if 0
	(void)fprintf(stderr, "*** addr_parse(): p = [%s]\n", p);
#endif

	q = strip_post_ws(p);
	if(*q == ')') {
		while((*--q != '('));
		*q = NUL;
	}
	(void)strip_post_ws(p);

#if 0
	(void)fprintf(stderr, "*** addr_parse(): p = [%s]\n", p);
#endif

	return(p);
}

/*
append_domain() -- Fix up address with @domain.com
*/
char *append_domain(char *str)
{
	char buf[(BUF_SZ + 1)];

	if(strchr(str, '@') == (char *)NULL) {
		if(snprintf(buf, BUF_SZ, "%s@%s", str,
#ifdef REWRITE_DOMAIN
			rewrite_domain == True ? mail_domain : hostname
#else
			hostname
#endif
														) == -1) {
				die("append_domain() -- snprintf() failed");
		}
		return(strdup(buf));
	}

	return(strdup(str));
}

/*
standardise() -- Trim off '\n's and double leading dots
*/
void standardise(char *str)
{
	size_t sl;
	char *p;

	if((p = strchr(str, '\n'))) {
		*p = NUL;
	}

	/* Any line beginning with a dot has an additional dot inserted;
	not just a line consisting solely of a dot. Thus we have to slide
	the buffer down one */
	sl = strlen(str);

	if(*str == '.') {
		if((sl + 2) > BUF_SZ) {
			die("standardise() -- Buffer overflow");
		}
		(void)memmove((str + 1), str, (sl + 1));	/* Copy trailing \0 */

		*str = '.';
	}
}

/*
revaliases() -- Parse the reverse alias file
	Fix globals to use any entry for sender
*/
void revaliases(struct passwd *pw)
{
	char buf[(BUF_SZ + 1)], *p;
	FILE *fp;

	/* Try to open the reverse aliases file */
	if((fp = fopen(REVALIASES_FILE, "r"))) {
		/* Search if a reverse alias is defined for the sender */
		while(fgets(buf, sizeof(buf), fp)) {
			/* Make comments invisible */
			if((p = strchr(buf, '#'))) {
				*p = NUL;
			}

			/* Ignore malformed lines and comments */
			if(strchr(buf, ':') == (char *)NULL) {
				continue;
			}

			/* Parse the alias */
			if(((p = strtok(buf, ":"))) && !strcmp(p, pw->pw_name)) {
				if((p = strtok(NULL, ": \t\r\n"))) {
					if((uad = CFG_STR_UNESC(p)) == (char *)NULL) {
						DIE_UNESC("revaliases");
					}
				}

				if((p = strtok(NULL, " \t\r\n:"))) {
					if((mailhost = CFG_STR_UNESC(p)) == (char *)NULL) {
						DIE_UNESC("revaliases");
					}

					if((p = strtok(NULL, " \t\r\n:"))) {
						mailhost_port = atoi(p);
					}

					if(log_level > 0) {
						log_event(LOG_INFO, "Set MailHub=\"%s\"\n", mailhost);
						log_event(LOG_INFO,
							"via SMTP Port Number=\"%d\"\n", mailhost_port);
					}
				}
			}
		}

		fclose(fp);
	}
}

/* 
from_strip() -- Transforms "Name <login@host>" into "login@host" or "login@host (Real name)"
*/
char *from_strip(char *str)
{
	char *p;

#if 0
	(void)fprintf(stderr, "*** from_strip(): str = [%s]\n", str);
#endif

	if(strncmp("From:", str, 5) == 0) {
		str += 5;
	}

	/* Remove the real name if necessary - just send the address */
	if((p = addr_parse(str)) == (char *)NULL) {
		die("from_strip() -- addr_parse() failed");
	}
#if 0
	(void)fprintf(stderr, "*** from_strip(): p = [%s]\n", p);
#endif

	return(strdup(p));
}

/*
from_format() -- Generate standard From: line
*/
char *from_format(char *str, bool_t override_from)
{
	char buf[(BUF_SZ + 1)];

	if(override_from) {
		if(minus_f) {
			str = append_domain(minus_f);
		}

		if(minus_F) {
			if(snprintf(buf,
				BUF_SZ, "\"%s\" <%s>", minus_F, str) == -1) {
				die("from_format() -- snprintf() failed");
			}
		}
		else if(gecos) {
			if(snprintf(buf, BUF_SZ, "\"%s\" <%s>", gecos, str) == -1) {
				die("from_format() -- snprintf() failed");
			}
		}
		else {
			if(snprintf(buf, BUF_SZ, "%s", str) == -1) {
				die("from_format() -- snprintf() failed");
			}
		}
	}
	else {
		if(gecos) {
			if(snprintf(buf, BUF_SZ, "\"%s\" <%s>", gecos, str) == -1) {
				die("from_format() -- snprintf() failed");
			}
		}

                else {
                    if(snprintf(buf, BUF_SZ, "%s", str) == -1) {
                        die("from_format() -- snprintf() failed");
                    }
                }
	}

#if 0
	(void)fprintf(stderr, "*** from_format(): buf = [%s]\n", buf);
#endif

	return(strdup(buf));
}

/*
rcpt_save() -- Store entry into RCPT list
*/
void rcpt_save(char *str)
{
	char *p;

# if 1
	/* Horrible botch for group stuff */
	p = str;
	while(*p) p++;

	if(*--p == ';') {
		return;
	}
#endif

#if 0
	(void)fprintf(stderr, "*** rcpt_save(): str = [%s]\n", str);
#endif

	/* Ignore missing usernames */
	if(*str == NUL) {
		return;
	}

	if((rt->string = strdup(str)) == (char *)NULL) {
		die("rcpt_save() -- strdup() failed");
	}

	rt->next = (rcpt_t *)malloc(sizeof(rcpt_t));
	if(rt->next == (rcpt_t *)NULL) {
		die("rcpt_save() -- malloc() failed");
	}
	rt = rt->next;

	rt->next = (rcpt_t *)NULL;
}

/*
rcpt_parse() -- Break To|Cc|Bcc into individual addresses
*/
void rcpt_parse(char *str)
{
	bool_t in_quotes = False, got_addr = False;
	char *p, *q, *r;

#if 0
	(void)fprintf(stderr, "*** rcpt_parse(): str = [%s]\n", str);
#endif

	if((p = strdup(str)) == (char *)NULL) {
		die("rcpt_parse(): strdup() failed");
	}
	q = p;

	/* Replace <CR>, <LF> and <TAB> */
	while(*q) {
		switch(*q) {
			case '\t':
			case '\n':
			case '\r':
					*q = ' ';
		}
		q++;
	}
	q = p;

#if 0
	(void)fprintf(stderr, "*** rcpt_parse(): q = [%s]\n", q);
#endif

	r = q;
	while(*q) {
		if(*q == '"') {
			in_quotes = (in_quotes ? False : True);
		}

		/* End of string? */
		if(*(q + 1) == NUL) {
			got_addr = True;
		}

		/* End of address? */
		if((*q == ',') && (in_quotes == False)) {
			got_addr = True;

			*q = NUL;
		}

		if(got_addr) {
			while(*r && isspace(*r)) r++;

			rcpt_save(addr_parse(r));
			r = (q + 1);
#if 0
			(void)fprintf(stderr, "*** rcpt_parse(): r = [%s]\n", r);
#endif
			got_addr = False;
		}
		q++;
	}
	free(p);
}

#ifdef MD5AUTH
int crammd5(char *challengeb64, char *username, char *password, char *responseb64)
{
	int i;
	unsigned char digest[MD5_DIGEST_LEN];
	unsigned char digascii[MD5_DIGEST_LEN * 2];
	unsigned char challenge[(BUF_SZ + 1)];
	unsigned char response[(BUF_SZ + 1)];
	unsigned char secret[(MD5_BLOCK_LEN + 1)]; 

	memset (secret,0,sizeof(secret));
	memset (challenge,0,sizeof(challenge));
	strncpy (secret, password, sizeof(secret));	
	if (!challengeb64 || strlen(challengeb64) > sizeof(challenge) * 3 / 4)
		return 0;
	from64tobits(challenge, challengeb64);

	hmac_md5(challenge, strlen(challenge), secret, strlen(secret), digest);

	for (i = 0; i < MD5_DIGEST_LEN; i++) {
		digascii[2 * i] = hextab[digest[i] >> 4];
		digascii[2 * i + 1] = hextab[(digest[i] & 0x0F)];
	}
	digascii[MD5_DIGEST_LEN * 2] = '\0';

	if (sizeof(response) <= strlen(username) + sizeof(digascii))
		return 0;
	
	strncpy (response, username, sizeof(response) - sizeof(digascii) - 2);
	strcat (response, " ");
	strcat (response, digascii);
	to64frombits(responseb64, response, strlen(response));

	return 1;
}
#endif

/*
rcpt_remap() -- Alias systems-level users to the person who
	reads their mail. This is variously the owner of a workstation,
	the sysadmin of a group of stations and the postmaster otherwise.
	We don't just mail stuff off to root on the mailhub :-)
*/
char *rcpt_remap(char *str)
{
	struct passwd *pw;

	if(strchr(str, '@') ||
		((pw = getpwnam(str)) == NULL) || (pw->pw_uid > MAXSYSUID)) {
		return(append_domain(str));	/* It's not a local systems-level user */
	}
	else {
		return(append_domain(root));
	}
}

/*
header_save() -- Store entry into header list
*/
void header_save(char *str)
{
	char *p;

#if 0
	(void)fprintf(stderr, "header_save(): str = [%s]\n", str);
#endif

	if((p = strdup(str)) == (char *)NULL) {
		die("header_save() -- strdup() failed");
	}
	ht->string = p;

	if(strncasecmp(ht->string, "From:", 5) == 0) {
#if 1
		/* Hack check for NULL From: line */
		if(*(p + 6) == NUL) {
			return;
		}
#endif

#ifdef REWRITE_DOMAIN
		if(override_from == True) {
			uad = from_strip(ht->string);
		}
		else {
			return;
		}
#endif
		have_from = True;
	}
#ifdef HASTO_OPTION
	else if(strncasecmp(ht->string, "To:" ,3) == 0) {
		have_to = True;
	}
#endif
	else if(strncasecmp(ht->string, "Date:", 5) == 0) {
		have_date = True;
	}

	if(minus_t) {
		/* Need to figure out recipients from the e-mail */
		if(strncasecmp(ht->string, "To:", 3) == 0) {
			p = (ht->string + 3);
			rcpt_parse(p);
		}
		else if(strncasecmp(ht->string, "Bcc:", 4) == 0) {
			p = (ht->string + 4);
			rcpt_parse(p);
		}
		else if(strncasecmp(ht->string, "CC:", 3) == 0) {
			p = (ht->string + 3);
			rcpt_parse(p);
		}
	}

#if 0
	(void)fprintf(stderr, "header_save(): ht->string = [%s]\n", ht->string);
#endif

	ht->next = (headers_t *)malloc(sizeof(headers_t));
	if(ht->next == (headers_t *)NULL) {
		die("header_save() -- malloc() failed");
	}
	ht = ht->next;

	ht->next = (headers_t *)NULL;
}

/*
header_parse() -- Break headers into seperate entries
*/
void header_parse(FILE *stream)
{
	size_t size = BUF_SZ, len = 0;
	char *p = (char *)NULL, *q;
	bool_t in_header = True;
	char l = NUL;
	int c;

	while(in_header && ((c = fgetc(stream)) != EOF)) {
		/* Must have space for up to two more characters, since we
			may need to insert a '\r' */
		if((p == (char *)NULL) || (len >= (size - 1))) {
			size += BUF_SZ;

			p = (char *)realloc(p, (size * sizeof(char)));
			if(p == (char *)NULL) {
				die("header_parse() -- realloc() failed");
			}
			q = (p + len);
		}
		len++;

		if(l == '\n') {
			switch(c) {
				case ' ':
				case '\t':
						/* Must insert '\r' before '\n's embedded in header
						   fields otherwise qmail won't accept our mail
						   because a bare '\n' violates some RFC */
						
						*(q - 1) = '\r';	/* Replace previous \n with \r */
						*q++ = '\n';		/* Insert \n */
						len++;
						
						break;

				case '\n':
						in_header = False;

				default:
						*q = NUL;
						if((q = strrchr(p, '\n'))) {
							*q = NUL;
						}
						header_save(p);

						q = p;
						len = 0;
			}
		}
		*q++ = c;

		l = c;
	}
	(void)free(p);
}

/*
read_config() -- Open and parse config file and extract values of variables
*/
bool_t read_config(const char *config_file)
{
	char buf[(BUF_SZ + 1)], *p, *q, *r;
	FILE *fp;
        char *tmp_str = NULL;

        if (!config_file) {
                config_file = CONFIGURATION_FILE;
        }

	if((fp = fopen(config_file, "r")) == NULL) {
		return(False);
	}

	while(fgets(buf, sizeof(buf), fp)) {
		/* Make comments invisible */
		if((p = strchr(buf, '#'))) {
			*p = NUL;
		}

		/* Ignore malformed lines and comments */
		if(strchr(buf, '=') == (char *)NULL) continue;

		/* Parse out keywords */
		if(((p = strtok(buf, "= \t\n")) != (char *)NULL)
			&& ((q = strtok(NULL, "= \t\n:")) != (char *)NULL)) {
			if(strcasecmp(p, "Root") == 0) {
				if((root = CFG_STR_UNESC(q)) == (char *)NULL) {
                                        DIE_UNESC_PARSECONFIG();
				}

				if(log_level > 0) {
					log_event(LOG_INFO, "Set Root=\"%s\"\n", root);
				}
			}
			else if(strcasecmp(p, "MailHub") == 0) {
                                if((mailhost = CFG_STR_UNESC(q)) == (char *)NULL) {
                                        DIE_UNESC_PARSECONFIG();
				}

				if((r = strtok(NULL, "= \t\n:")) != NULL) {
					mailhost_port = atoi(r);
				}

				if(log_level > 0) {
					log_event(LOG_INFO, "Set MailHub=\"%s\"\n", mailhost);
					log_event(LOG_INFO, "Set RemotePort=\"%d\"\n", mailhost_port);
				}
			}
			else if(strcasecmp(p, "BackupMailHub") == 0) {
				if((backup_mailhost = CFG_STR_UNESC(q)) == (char *)NULL) {
					DIE_UNESC_PARSECONFIG();
				}

				if((r = strtok(NULL, "= \t\n:")) != NULL) {
					backup_mailhost_port = atoi(r);
				}

				if(log_level > 0) {
					log_event(LOG_INFO, "Set BackupMailHub=\"%s\"\n", backup_mailhost);
					log_event(LOG_INFO, "Set BackupRemotePort=\"%d\"\n", backup_mailhost_port);
				}
			}
			else if(strcasecmp(p, "DeadLetterEnable") == 0) {
				if(strcmp(q, "y")) {
					dl_enable = 0;
				}
				if(log_level > 0) {
					log_event(LOG_INFO, "Set DeadLetterEnable=\"%s\" (%s)", q, dl_enable ? "enabled" : "disabled");
				}
			}
			else if(strcasecmp(p, "DeadLetterSeparate") == 0) {
				if(!strcmp(q, "y")) {
					dl_separate = 1;
				}
				if(log_level > 0) {
					log_event(LOG_INFO, "Set DeadLetterSeparate=\"%s\" (%s)", q, dl_separate ? "enabled" : "disabled");
				}
			}
			else if(strcasecmp(p, "DeadLetterMaxAge") == 0) {
				dl_max_age = atol(q);
				if(log_level > 0) {
					log_event(LOG_INFO, "Set DeadLetterMaxAge=%ld", dl_max_age);
				}
			}
			else if(strcasecmp(p, "HostName") == 0) {
                                SAFE_FREE(tmp_str);
                                if((tmp_str = CFG_STR_UNESC(q)) == (char *)NULL) {
                                        DIE_UNESC_PARSECONFIG();
                                }
                                if(strncpy(hostname, tmp_str, sizeof(hostname)) == NULL) {
                                        DIE_UNESC_PARSECONFIG();
				}
                                SAFE_FREE(tmp_str);

				if(log_level > 0) {
					log_event(LOG_INFO, "Set HostName=\"%s\"\n", hostname);
				}
			}
#ifdef REWRITE_DOMAIN
			else if(strcasecmp(p, "RewriteDomain") == 0) {
                                SAFE_FREE(tmp_str);
                                if((tmp_str = CFG_STR_UNESC(q)) == (char *)NULL) {
                                        DIE_UNESC_PARSECONFIG();
                                }
				if((p = strrchr(tmp_str, '@'))) {
					mail_domain = strdup(++p);

					log_event(LOG_ERR,
						"Set RewriteDomain=\"%s\" is invalid\n", q);
					log_event(LOG_ERR,
						"Set RewriteDomain=\"%s\" used\n", mail_domain);
				}
				else {
					mail_domain = strdup(tmp_str);
				}
                                SAFE_FREE(tmp_str);

				if(mail_domain == (char *)NULL) {
					DIE_UNESC_PARSECONFIG();
				}
				rewrite_domain = True;

				if(log_level > 0) {
					log_event(LOG_INFO,
						"Set RewriteDomain=\"%s\"\n", mail_domain);
				}
			}
#endif
			else if(strcasecmp(p, "FromLineOverride") == 0) {
				if(strcasecmp(q, "YES") == 0) {
					override_from = True;
				}
				else {
					override_from = False;
				}

				if(log_level > 0) {
					log_event(LOG_INFO,
						"Set FromLineOverride=\"%s\"\n",
						override_from ? "True" : "False");
				}
			}
			else if(strcasecmp(p, "RemotePort") == 0) {
				mailhost_port = atoi(q);
				if(log_level > 0) {
					log_event(LOG_INFO, "Set RemotePort=\"%d\"\n", mailhost_port);
				}
			}
			else if(strcasecmp(p, "BackupRemotePort") == 0) {
				backup_mailhost_port = atoi(q);
				if(log_level > 0) {
					log_event(LOG_INFO, "Set BackupRemotePort=\"%d\"\n", backup_mailhost_port);
				}
			}
#ifdef HAVE_SSL
			else if(strcasecmp(p, "UseTLS") == 0) {
				if(strcasecmp(q, "YES") == 0) {
					use_tls = True;
				}
				else {
					use_tls = False;
					use_starttls = False;
				}

				if(log_level > 0) { 
					log_event(LOG_INFO,
						"Set UseTLS=\"%s\"\n", use_tls ? "True" : "False");
				}
			}
                        else if(strcasecmp(p, "UseSTARTTLS") == 0) {
                            if(strcasecmp(q, "YES") == 0) {
                                use_starttls = True;
                                use_tls = True;
                            }
                            else {
                                use_starttls = False;
                            }

                            if(log_level > 0) { 
                                log_event(LOG_INFO,
                                          "Set UseSTARTTLS=\"%s\"\n", use_starttls ? "True" : "False");
                            }
                        }
                        else if(strcasecmp(p, "TLSCertVerify") == 0) {
                            if(strcasecmp(q, "YES") == 0) {
                                cert_verify = True;
                            }
                            else {
                                cert_verify = False;
                            }

                            if(log_level > 0) { 
                                log_event(LOG_INFO,
                                          "Set TLSCertVerify=\"%s\"\n", cert_verify ? "True" : "False");
                            }
                        }
                        else if(strcasecmp(p, "STARTTLSFallback") == 0) {
                            if(strcasecmp(q, "YES") == 0) {
                                starttls_fallback = True;
                            }
                            else {
                                starttls_fallback = False;
                            }

                            if(log_level > 0) { 
                                log_event(LOG_INFO,
                                          "Set STARTTLSFallback=\"%s\"\n", starttls_fallback ? "True" : "False");
                            }
                        }
			else if(strcasecmp(p, "UseTLSCert") == 0) {
				if(strcasecmp(q, "YES") == 0) {
					use_cert = True;
				}
				else {
					use_cert = False;
				}

				if(log_level > 0) {
					log_event(LOG_INFO,
						"Set UseTLSCert=\"%s\"\n",
						use_cert ? "True" : "False");
				}
			}
			else if(strcasecmp(p, "TLSCert") == 0) {
				if((tls_cert = CFG_STR_UNESC(q)) == (char *)NULL) {
					DIE_UNESC_PARSECONFIG();
				}

				if(log_level > 0) {
					log_event(LOG_INFO, "Set TLSCert=\"%s\"\n", tls_cert);
				}
			}
			else if(strcasecmp(p, "TLSCiphers") == 0 && !tls_ciphers) {
                            /*
                             * Note that unescaping is particularly important here,
                             * since (a) ':' is listed as a delimiter char in our
                             * strtok() call above, and (b) ':' is used in the strings
                             * we'll likely receive here, as a separator between
                             * different cipher options.  We want it all as one string,
                             * to let the SSL library sort them out.
                             */
                                if((tls_ciphers = CFG_STR_UNESC(q)) == (char *)NULL) {
                                        DIE_UNESC_PARSECONFIG();
                                }

                                if(log_level > 0) {
                                        log_event(LOG_INFO, "Set TLSCiphers=\"%s\"\n", tls_ciphers);
                                }
                        }
			else if(strcasecmp(p, "TLSCACertFile") == 0) {
                            if(tls_cacert_file) {
                                log_event(LOG_INFO, "Overwriting prevous TLSCACertFile: %s", tls_cacert_file);
                            }

                                if((tls_cacert_file = CFG_STR_UNESC(q)) == (char *)NULL) {
                                        DIE_UNESC_PARSECONFIG();
                                }

                                if(log_level > 0) {
                                        log_event(LOG_INFO, "Set TLSCACertFile=\"%s\"\n", tls_cacert_file);
                                }
                        }
			else if(strcasecmp(p, "TLSCACertDir") == 0 && !tls_cacert_dir) {
                                if((tls_cacert_dir = CFG_STR_UNESC(q)) == (char *)NULL) {
                                        DIE_UNESC_PARSECONFIG();
                                }

                                if(log_level > 0) {
                                        log_event(LOG_INFO, "Set TLSCACertDir=\"%s\"\n", tls_cacert_dir);
                                }
                        }
#endif
			else if(strcasecmp(p, "AuthUser") == 0 && !auth_user) {
                                if((auth_user = CFG_STR_UNESC(q)) == (char *)NULL) {
                                        DIE_UNESC_PARSECONFIG();
                                }

                                if(log_level > 0) {
                                        log_event(LOG_INFO, "Set AuthUser=\"%s\"\n", auth_user);
                                }
                        }
                        else if(strcasecmp(p, "AuthPass") == 0 && !auth_pass) {
                                if((auth_pass = CFG_STR_UNESC(q)) == (char *)NULL) {
                                        DIE_UNESC_PARSECONFIG();
                                }

                                if(log_level > 0) {
                                        log_event(LOG_INFO, "Set AuthPass=\"%s\"\n", auth_pass);
                                }
                        }
                        else if(strcasecmp(p, "AuthMethod") == 0 && !auth_method) {
                                if((auth_method = CFG_STR_UNESC(q)) == (char *)NULL) {
                                        DIE_UNESC_PARSECONFIG();
                                }

                                if(log_level > 0) {
                                        log_event(LOG_INFO, "Set AuthMethod=\"%s\"\n", auth_method);
                                }
                        }
                        else if(strcasecmp(p, "UseOldAUTH") == 0) {
                            if(strcasecmp(q, "YES") == 0) {
                                use_oldauth = True;
                            }
                            else {
                                use_oldauth = False;
                            }
 
                            if(log_level > 0) {
                                log_event(LOG_INFO,
                                          "Set UseOldAUTH=\"%s\"\n",
                                          use_oldauth ? "True" : "False");
                            }
                        }
			else if(strcasecmp(p, "BackupAuthUser") == 0 && !backup_auth_user) {
                                if((backup_auth_user = CFG_STR_UNESC(q)) == (char *)NULL) {
                                        DIE_UNESC_PARSECONFIG();
                                }

                                if(log_level > 0) {
                                        log_event(LOG_INFO, "Set BackupAuthUser=\"%s\"\n", backup_auth_user);
                                }
                        }
                        else if(strcasecmp(p, "BackupAuthPass") == 0 && !backup_auth_pass) {
                                if((backup_auth_pass = CFG_STR_UNESC(q)) == (char *)NULL) {
                                        DIE_UNESC_PARSECONFIG();
                                }
                                
                                if(log_level > 0) {
                                        log_event(LOG_INFO, "Set BackupAuthPass=\"%s\"\n", backup_auth_pass);
                                }
                        }
                        else if(strcasecmp(p, "BackupAuthMethod") == 0 && !backup_auth_method) {
                                if((backup_auth_method = CFG_STR_UNESC(q)) == (char *)NULL) {
                                        DIE_UNESC_PARSECONFIG();
                                }

                                if(log_level > 0) {
                                        log_event(LOG_INFO, "Set BackupAuthMethod=\"%s\"\n", backup_auth_method);
                                }
                        }
                        else if(strcasecmp(p, "BackupUseOldAUTH") == 0) {
                            if(strcasecmp(q, "YES") == 0) {
                                backup_use_oldauth = True;
                            }
                            else {
                                backup_use_oldauth = False;
                            }
 
                            if(log_level > 0) {
                                log_event(LOG_INFO,
                                          "Set BackupUseOldAUTH=\"%s\"\n",
                                          backup_use_oldauth ? "True" : "False");
                            }
                        }
                        else {
				log_event(LOG_INFO, "Unable to set %s=\"%s\"\n", p, q);
			}
		}
	}
	(void)fclose(fp);

	return(True);
}


int
ssmtp_cert_verify_log_result(int ok, X509_STORE_CTX *ctx)
{
    int errnum = 0;
    int depth = 0;
    X509 *cert = NULL;
    X509_NAME *subject = NULL;
    X509_NAME *issuer = NULL;
    char *sname = NULL;
    char *iname = NULL;
    char *certerr = NULL;
    int log_level = 0;
    
    cert = X509_STORE_CTX_get_current_cert(ctx);
    errnum = X509_STORE_CTX_get_error(ctx);
    depth = X509_STORE_CTX_get_error_depth(ctx);

    if (!ok) {
        log_level = LOG_WARNING;
        failure_reason = sfr_cert_verify_failure;
    }
    else if (depth == 0) {
        log_level = LOG_INFO;
    }
    else {
        log_level = LOG_DEBUG;
    }

    /*
     * X509_get_*_name return pointers to the internal copies of
     * those things requested.  So do not free them.  But we can't
     * make them 'const' because other functions we pass them back
     * to don't take const pointers.
     *
     * X509_NAME_oneline, if passed a NULL buffer, allocates memory.
     */
    subject = X509_get_subject_name(cert);
    sname = X509_NAME_oneline(subject, NULL, 0);
    issuer = X509_get_issuer_name(cert);
    iname = X509_NAME_oneline( issuer, NULL, 0 );

    if (ok) {
        log_event(log_level,
                  "TLS server cert verify depth %d: success", depth);
    }
    else {
        certerr = (char *)X509_verify_cert_error_string(errnum);
        log_event(log_level,
                  "TLS server cert verify depth %d: FAILURE: %s (code %d)",
                  depth, certerr, errnum);
    }

    log_event(log_level,
              "TLS server cert verify depth %d: issuer:  %s",
              depth, iname ? iname : "-unknown-");
    log_event(log_level,
              "TLS server cert verify depth %d: subject: %s",
              depth, sname ? sname : "-unknown-");
    
    if (sname) {
        CRYPTO_free(sname);
    }
    if (iname) {
        CRYPTO_free(iname);
    }

    /*
     * Don't change the result.
     */
    return(ok);
}


/*
 * We use this callback when we didn't want it to verify the cert at all.
 * It seems to do so and call us anyway, but we can at least not shout it
 * from the mountaintops.
 */
int
ssmtp_cert_verify_silent(int ok, X509_STORE_CTX *ctx)
{
    return(1);
}


/*
smtp_open() -- Open connection to a remote SMTP listener

Return:
  >=0  Success: returns fd of socket to server
  -1   Failure, fatal
  -2   Failure, suitable for retrying

XXX/EMT: in some of the original failure cases, it doesn't seem to
close() the socket... why not?  Not likely a real problem, since this
is a short-lived process, though.

*/
int smtp_open(char *host, int port)
{
    bool_t okay = False;

#ifdef INET6
	struct addrinfo hints, *ai0, *ai;
	char servname[NI_MAXSERV];
	int s;
#else
	struct sockaddr_in name;
	struct hostent *hent;
	int s, namelen;
#endif

#ifdef HAVE_SSL
        /* ....................................................................
         * SSL part 1: local initialization.  We haven't created the
         * connection yet, so we're not actually doing anything with the
         * remote endpoint here.  So starttls_fallback does not come into
         * play until part 2, below.
         */

	int err;
	char buf[(BUF_SZ + 1)];

	/* Init SSL stuff */
	SSL_CTX *ctx;
	X509 *server_cert;

	SSL_load_error_strings();
	SSLeay_add_ssl_algorithms();
	ctx = SSL_CTX_new(SSLv23_client_method());

	if(!ctx) {
		log_event(LOG_ERR, "SSL_CTX_new() failed");
		return(-2);
	}

        if (tls_ciphers) {
            if(!SSL_CTX_set_cipher_list(ctx, tls_ciphers)) {
                log_event(LOG_ERR, "Failed set cipher list: %s", tls_ciphers);
                return(-2);
            }
            else {
                if(log_level > 0) {
                    log_event(LOG_INFO, "Cipher list accepted: %s",
                              tls_ciphers);
                }
            }
        }

	if(use_cert == True) { 
		if(SSL_CTX_use_certificate_chain_file(ctx, tls_cert) <= 0) {
			perror("Use certfile");
			return(-1);
		}

		if(SSL_CTX_use_PrivateKey_file(ctx, tls_cert, SSL_FILETYPE_PEM) <= 0) {
			perror("Use PrivateKey");
			return(-1);
		}

		if(!SSL_CTX_check_private_key(ctx)) {
			log_event(LOG_ERR, "Private key does not match the certificate public key\n");
			return(-1);
		}
	}
#endif

#ifdef INET6
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = p_family;
	hints.ai_socktype = SOCK_STREAM;
	snprintf(servname, sizeof(servname), "%d", port);

	/* Check we can reach the host */
	if (getaddrinfo(host, servname, &hints, &ai0)) {
		log_event(LOG_ERR, "Unable to locate %s", host);
		return(-1);
	}

	for (ai = ai0; ai; ai = ai->ai_next) {
		/* Create a socket for the connection */
		s = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
		if (s < 0) {
			continue;
		}

		if (connect(s, ai->ai_addr, ai->ai_addrlen) < 0) {
			s = -1;
			continue;
		}
		break;
	}
	freeaddrinfo(ai0);
	ai = ai0 = NULL;

	if(s < 0) {
		log_event (LOG_ERR,
			"Unable to connect to \"%s\" port %d.\n", host, port);

		return(-1);
	}
#else
	/* Check we can reach the host */
	if((hent = gethostbyname(host)) == (struct hostent *)NULL) {
		log_event(LOG_ERR, "Unable to locate %s", host);
		return(-1);
	}

	if(hent->h_length > sizeof(hent->h_addr)) {
		log_event(LOG_ERR, "Buffer overflow in gethostbyname()");
		return(-1);
	}

	/* Create a socket for the connection */
	if((s = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
		log_event(LOG_ERR, "Unable to create a socket");
		return(-1);
	}

	/* This SHOULD already be in Network Byte Order from gethostbyname() */
	name.sin_addr.s_addr = ((struct in_addr *)(hent->h_addr))->s_addr;
	name.sin_family = hent->h_addrtype;
	name.sin_port = htons(port);

	namelen = sizeof(struct sockaddr_in);
	if(connect(s, (struct sockaddr *)&name, namelen) < 0) {
		log_event(LOG_ERR, "Unable to connect to %s:%d", host, port);
		return(-1);
	}
#endif

#ifdef HAVE_SSL
        /*
         * SSL part 2: negotiate TLS with remote host.
         *
         * In the non-TLS case, we're all ready to return the socket
         * number; the caller will then call smtp_okay() to confirm that
         * we got a welcome line like "220 hexagon.tallmaple.com ESMTP
         * Sendmail ...".
         *
         * In the TLS case, it's up to us to do EHLO and STARTTLS, and
         * then check the server's reply to make sure it's OK with that.
         * Three possible outcomes:
         *
         *   1. Everything goes well: return 's', after having read
         *      success response line (i.e. we call smtp_okay(), not
         *      the caller).
         *
         *   2. Something goes wrong, either with the server or with our own
         *      verification of the server cert (if any)...
         *      (a) starttls_fallback false: return -1, indicating an error.
         *      (b) starttls_fallback true: return 's' anyway, just like #1.
         *          The caller will not check smtp_okay(), but will proceed
         *          with a HELO/EHLO, as if TLS was all just a bad dream.
         *
         * Note that fallback only makes sense with STARTTLS.  We verify
         * that here to make sure.
         *
         * The sequence when STARTTLS is involved, any step of which can
         * fail, is:
         *   1. Check server's initial greeting.
         *   2. Send our EHLO, and check the response.
         *   3. Send STARTTLS, and check the response.  
         *   4. SSL_new(), SSL_connect(), and SSL_get_peer_certificate().
         *
         * If #1 or #2 fail, they are pre-TLS, so the fallback setting
         * doesn't matter.  We fail anyway.
         *
         * If #3 fails, that's where fallback can just go ahead and return
         * the socket anyway, and the caller can pick it up where we left
         * off.
         *
         * If #4 fails, that's the tricky part for fallback.  We have
         * already issued STARTTLS and it succeeded, but now we cannot
         * proceed.  We have no choice but to break the connection and
         * start fresh.  We'll return -2 and let our caller sort that out.
         */
	if(use_tls == True) {
            log_event(LOG_INFO, "Attempting SSL connection to host %s:%d",
                      host, port);
            
            if (use_starttls == False && starttls_fallback == True) {
                log_event(LOG_WARNING,
                          "Cannot use STARTTLSFallback without "
                          "UseSTARTTLS.  Disabling fallback.");
                starttls_fallback = False;
            }
            
            if (use_starttls == True) {
                use_tls=False; /* need to write plain text for a while */
                if (smtp_okay(s, buf)) {
                    /* ....................
                     * #1 success --> #2
                     */
                    smtp_write(s, "EHLO %s", hostname);
                    doing_ehlo = True;
                    okay = smtp_okay(s, buf);
                    doing_ehlo = False;
                    if (okay) {
                        /* ....................
                         * #2 success --> #3
                         */
                        if (!starttls_supported) {
                            if (starttls_fallback) {
                                log_event(LOG_NOTICE, "TLS not offered "
                                          "by server, using plaintext instead");
                                return(s);
                            }
                            else {
                                log_event(LOG_NOTICE, "TLS not offered "
                                          "by server, giving up");
                                failure_reason = sfr_starttls_not_supported;
                                return(-1);
                            }
                        }

                        smtp_write(s, "STARTTLS");
                        if (!smtp_okay(s, buf)) {
                            if (starttls_fallback) {
                                /* .................................
                                 * #3 failure --> done (fallback)
                                 */
                                log_event(LOG_NOTICE, "STARTTLS failed (%s), "
                                          "falling back on plaintext", buf);
                                return(s);
                            }
                            else {
                                /* .............................
                                 * #3 failure --> done (fail)
                                 */
                                log_event(LOG_ERR, "STARTTLS failed (%s), "
                                          "giving up", buf);
                                return(-1);
                            }
                        }
                        else {
                            /* ................................................
                             * #3 success --> fall through to further code
                             *                below
                             */
                        }
                    }
                    else {
                            /* .............................
                             * #2 failure --> done (fail)
                             */
                        log_event(LOG_ERR, "EHLO failed (%s), giving up",
                                  buf);
                        return(-1);
                    }
                }
                else {
                    /* .............................
                     * #1 failure --> done (fail)
                     */
                    log_event(LOG_ERR, "Invalid initial greeting (%s), "
                              "giving up", buf);
                    return(-1);
                }
                
                /* ..........................
                 * #3 success --> #4
                 */
                use_tls=True; /* now continue as normal for SSL */
            }
            
            /* ............................................................
             * Setup for #4...
             */
            if (cert_verify) {
                SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER,
                                   ssmtp_cert_verify_log_result);
            }
            else {
                /*
                 * Per SSL_CTX_set_verify() man page, this will still
                 * verify the cert.  But we won't log anything about
                 * the results from our callback, and we'll ignore the
                 * results (though it probably would anyway regardless
                 * of what we return).
                 */
                SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE,
                                   ssmtp_cert_verify_silent);
            }
            
            if (tls_cacert_file || tls_cacert_dir) {
                if (!SSL_CTX_load_verify_locations(ctx, tls_cacert_file,
                                                   tls_cacert_dir)) {
                    log_event(LOG_ERR, "SSL loading cert verify "
                              "locations failed");
                    close(s);
                    return(-2);
                }
                else if (log_level > 0) {
                    log_event(LOG_INFO, "Successfully set verify "
                              "locations: %s and %s",
                              tls_cacert_file ? tls_cacert_file : "NULL",
                              tls_cacert_dir ? tls_cacert_dir : "NULL");
                }
            }
            
            /* ............................................................
             * #4: form the SSL connection.  SSL_connect() will do
             * cert verification automatically, and our callback
             * installed by SSL_CTX_set_verify() will get called and
             * log the result.  If verification is enabled and fails,
             * this will fail.
             *
             * If this fails and fallback is enabled, we have to close
             * the connection and restart.  That's tricky to do in here,
             * so we just return failure and let our caller sort it out
             * (by disabling TLS and calling us back).
             */
            ssl = SSL_new(ctx);
            if(!ssl) {
                log_event(LOG_ERR, "SSL initialization failed");
                close(s);
                return(-2);
            }
            SSL_set_fd(ssl, s);
            
            err = SSL_connect(ssl);
            if(err < 0) { 
                /* XXX/EMT: too alarming in the fallback case? */
                log_event(LOG_ERR, "SSL connection or certificate "
                          "verification failed");
                close(s);
                return(-2);
            }
            
            if(log_level > 0 || 1) { 
                log_event(LOG_INFO, "SSL connection using %s",
                          SSL_get_cipher(ssl));
            }
            
            /*
             * If we were verifying, our callback would have been
             * called during SSL_connect() and we would have logged
             * the result.
             */
            if (!cert_verify) {
                log_event(LOG_INFO, "TLS server certificate not verified");
            }
            
            server_cert = SSL_get_peer_certificate(ssl);
            if(!server_cert) {
                /* .................................................
                 * #4 failure --> done
                 */
                log_event(LOG_ERR, "Could not get peer certificate "
                          "(STARTTLS)");
                close(s);
                return(-2);
            }
            
            X509_free(server_cert);
            
            /* TODO: Check server cert if changed! */
	}
#endif
        
	return(s);
}

/*
fd_getc() -- Read a character from an fd
*/
ssize_t fd_getc(int fd, void *c)
{
#ifdef HAVE_SSL
	if(use_tls == True) { 
		return(SSL_read(ssl, c, 1));
	}
#endif
	return(read(fd, c, 1));
}

/*
fd_gets() -- Get characters from a fd instead of an fp
*/
char *fd_gets(char *buf, int size, int fd)
{
	int i = 0;
	char c;

	while((i < size) && (fd_getc(fd, &c) == 1)) {
		if(c == '\r');	/* Strip <CR> */
		else if(c == '\n') {
			break;
		}
		else {
			buf[i++] = c;
		}
	}
	buf[i] = NUL;

	return(buf);
}

/*
smtp_read() -- Get a line and return the initial digit
*/
int smtp_read(int fd, char *response)
{
	do {
		if(fd_gets(response, BUF_SZ, fd) == NULL) {
			return(0);
		}
		if(log_level > 0) {
			log_event(LOG_INFO, "Receive: %s\n", response);
		}

                /*
                 * We have to do this in two parts because we don't know
                 * if there'll be a '-' in between.
                 */
                if (doing_ehlo &&
                    !strncmp(response, "250", 3) &&
                    !strncmp(response + 4, "STARTTLS", 8)) {
                    starttls_supported = True;
                    if (log_level > 0) {
                        log_event(LOG_INFO, "Server says it supports "
                                  "STARTTLS");
                    }
                }
	}
	while(response[3] == '-');

	if(minus_v) {
		(void)fprintf(stderr, "[<-] %s\n", response);
	}

	return(atoi(response) / 100);
}

/*
smtp_okay() -- Get a line and test the three-number string at the beginning
				If it starts with a 2, it's OK
*/
int smtp_okay(int fd, char *response)
{
	return((smtp_read(fd, response) == 2) ? 1 : 0);
}

/*
fd_puts() -- Write characters to fd
*/
ssize_t fd_puts(int fd, const void *buf, size_t count) 
{
#ifdef HAVE_SSL
	if(use_tls == True) { 
		return(SSL_write(ssl, buf, count));
	}
#endif
	return(write(fd, buf, count));
}

/*
smtp_write() -- A printf to an fd and append <CR/LF>
*/
void smtp_write(int fd, char *format, ...)
{
	char buf[(BUF_SZ + 1)];
	va_list ap;

	va_start(ap, format);
	if(vsnprintf(buf, (BUF_SZ - 2), format, ap) == -1) {
		die("smtp_write() -- vsnprintf() failed");
	}
	va_end(ap);

	if(log_level > 0) {
		log_event(LOG_INFO, "Send: %s\n", buf);
	}

	if(minus_v) {
		(void)fprintf(stderr, "[->] %s\n", buf);
	}
	(void)strcat(buf, "\r\n");

	(void)fd_puts(fd, buf, strlen(buf));
}

/*
handler() -- A "normal" non-portable version of an alarm handler
			Alas, setting a flag and returning is not fully functional in
			BSD: system calls don't fail when reading from a ``slow'' device
			like a socket. So we longjump instead, which is erronious on
			a small number of machines and ill-defined in the language
*/
void handler(void)
{
	extern jmp_buf TimeoutJmpBuf;

	longjmp(TimeoutJmpBuf, (int)1);
}

/*
ssmtp() -- send the message (exactly one) from stdin to the mailhub SMTP port
*/
int ssmtp(char *argv[])
{
	char buf[(BUF_SZ + 1)], *p, *q;
        char errmsg[(BUF_SZ + 1)];
#ifdef MD5AUTH
	char challenge[(BUF_SZ + 1)];
#endif
	struct passwd *pw;
	int i, sock, got_okay, mhc, count_mailhost;
	uid_t uid;
        bool_t minus_v_save = False;
        char *curr_mailhost;
        int curr_mailhost_port;
        const char *curr_auth_user = NULL;
        const char *curr_auth_pass = NULL;
        const char *curr_auth_method = NULL;
        bool_t curr_use_oldauth = False;

	uid = getuid();
	if((pw = getpwuid(uid)) == (struct passwd *)NULL) {
		die("Could not find password entry for UID %d", uid);
	}
	get_arpadate(arpadate);

	if(read_config(config_file) == False) {
		log_event(LOG_INFO, "%s/ssmtp.conf not found", SSMTPCONFDIR);
	}

	if((p = strtok(pw->pw_gecos, ";,"))) {
		if((gecos = strdup(p)) == (char *)NULL) {
			die("ssmtp() -- strdup() failed");
		}
	}
	revaliases(pw);

	/* revaliases() may have defined this */
	if(uad == (char *)NULL) {
		uad = append_domain(pw->pw_name);
	}

	ht = &headers;
	rt = &rcpt_list;

	header_parse(stdin);

#if 1
	/* With FromLineOverride=YES set, try to recover sane MAIL FROM address */
	uad = append_domain(uad);
#endif

	from = from_format(uad, override_from);

	/* Now to the delivery of the message */
	(void)signal(SIGALRM, (void(*)())handler);	/* Catch SIGALRM */
	(void)alarm((unsigned) MINWAIT);		/* Set initial timer */
	if(setjmp(TimeoutJmpBuf) != 0) {
		/* Then the timer has gone off and we bail out */
		die("Connection lost in middle of processing");
	}

        got_okay = 0;
        count_mailhost = ( (mailhost != NULL ? 1 : 0) + 
                           (backup_mailhost != NULL ? 1 : 0) );
        if(!count_mailhost) {
                die("No mailhub configured.  Not sending mail.");
        }
        for (mhc = 0; mhc < count_mailhost; mhc++) {
                if (mhc == 0) {
                        if (mailhost && *mailhost) {
                                curr_mailhost = mailhost;
                                curr_mailhost_port = mailhost_port;
                        }
                        else {
                                curr_mailhost = backup_mailhost;
                                curr_mailhost_port = backup_mailhost_port;
                        }
                }
                else {
                        if (backup_mailhost && *backup_mailhost) {
                                curr_mailhost = backup_mailhost;
                                curr_mailhost_port = backup_mailhost_port;
                        }
                        else {
                                continue;
                        }

                }

                sock = smtp_open(curr_mailhost, curr_mailhost_port);
                
                /*
                 * -2 is our special value meaning that we failed doing TLS,
                 * so we could retry (if we are configured to fallback),
                 * since plaintext might still work.
                 */
                if (sock == -2 && starttls_fallback) {
                    log_event(LOG_NOTICE, "Trying plaintext instead");
                    use_tls = False;
                    use_starttls = False;
                    sock = smtp_open(curr_mailhost, curr_mailhost_port);
                }

                if(sock < 0) {
                    switch (failure_reason) {
                    case sfr_cert_verify_failure:
                        snprintf(errmsg, BUF_SZ,
                                 "TLS certificate verification failed for "
                                 "%s:%d", curr_mailhost, curr_mailhost_port);
                        break;

                    case sfr_starttls_not_supported:
                        snprintf(errmsg, BUF_SZ,
                                 "STARTTLS not supported by server %s:%d",
                                 curr_mailhost, curr_mailhost_port);
                        break;

                    default:
                        snprintf(errmsg, BUF_SZ,
                                 "Cannot open %s:%d",
                                 curr_mailhost, curr_mailhost_port);
                    }

                    errmsg[BUF_SZ] = '\0';

                    if (mhc == count_mailhost - 1) {
                        die("%s", errmsg);
                    }
                    else {
                        log_event(LOG_NOTICE, "%s", errmsg);
                    }

                }

                else if (use_starttls == False) { /* no initial response after STARTTLS */
                        (void)alarm((unsigned) MAXWAIT);
                        got_okay = smtp_okay(sock, buf);
                        if (!got_okay) {
                                if (mhc == count_mailhost - 1) {
                                        die("Invalid response SMTP server for %s:%d",
                                                  curr_mailhost, curr_mailhost_port);
                                }
                                else {
                                        log_event(LOG_NOTICE, "Invalid response SMTP server for %s:%d",
                                                  curr_mailhost, curr_mailhost_port);
                                }
                        }
                }
                else {
                    /*
                     * When (use_starttls == True), we already checked smtp_okay()
                     * in smtp_open().  It's not idempotent, so don't call it again!
                     */
                    got_okay = True;
                }

                if (got_okay) {
                        break;
                }
        }
        if(!got_okay) {
                die("Could not connect to a mailhub.");
        }

	/* If user supplied username and password, then try ELHO */
	if(auth_user) {
		smtp_write(sock, "EHLO %s", hostname);
	}
	else {
		smtp_write(sock, "HELO %s", hostname);
	}
	(void)alarm((unsigned) MEDWAIT);

	if(smtp_okay(sock, buf) == False) {
		die("%s (%s)", buf, hostname);
	}

	/* Try to log in if username was supplied. */
        if (curr_mailhost == mailhost) {
            curr_auth_user = auth_user;
            curr_auth_pass = auth_pass;
            curr_auth_method = auth_method;
            curr_use_oldauth = use_oldauth;
        }
        else {
            curr_auth_user = backup_auth_user;
            curr_auth_pass = backup_auth_pass;
            curr_auth_method = backup_auth_method;
            curr_use_oldauth = use_oldauth;
        }

	if(curr_auth_user) {
#ifdef MD5AUTH
		if(strcasecmp(curr_auth_method, "cram-md5")) {
			smtp_write(sock, "AUTH CRAM-MD5");
			(void)alarm((unsigned) MEDWAIT);

			if(smtp_read(sock, buf) != 3) {
				die("Server rejected AUTH CRAM-MD5 (%s)", buf);
			}
			strncpy(challenge, strchr(buf,' ') + 1, sizeof(challenge));

			memset(buf, 0, sizeof(buf));
			crammd5(challenge, authUsername, authPassword, buf);
		}
		else {
#endif
		memset(buf, 0, sizeof(buf));
		to64frombits(buf, curr_auth_user, strlen(curr_auth_user));
                if (curr_use_oldauth) {
                    smtp_write(sock, "AUTH LOGIN %s", buf);
                }
                else {
                    smtp_write(sock, "AUTH LOGIN");
                    (void)alarm((unsigned) MEDWAIT);
                    if(smtp_read(sock, buf) != 3) {
                        die("Server didn't like our AUTH LOGIN (%s)", buf);
                    }
                    /* we assume server asked us for Username */
                    memset(buf, 0, sizeof(buf));
                    to64frombits(buf, curr_auth_user, strlen(curr_auth_user));
                    smtp_write(sock, buf);
                }

		(void)alarm((unsigned) MEDWAIT);
		if(smtp_read(sock, buf) != 3) {
			die("Server didn't accept AUTH LOGIN (%s)", buf);
		}
		memset(buf, 0, sizeof(buf));

		to64frombits(buf, curr_auth_pass, strlen(curr_auth_pass));
#ifdef MD5AUTH
		}
#endif
		/* We do NOT want the password output to STDERR
                 * even base64 encoded.*/
                minus_v_save = minus_v;
                minus_v = False;
                smtp_write(sock, "%s", buf);
                minus_v = minus_v_save;
                (void)alarm((unsigned) MEDWAIT);

		if(smtp_okay(sock, buf) == False) {
			die("Authorization failed (%s)", buf);
		}
	}

	/* Send "MAIL FROM:" line */
	smtp_write(sock, "MAIL FROM:<%s>", uad);

	(void)alarm((unsigned) MEDWAIT);

	if(smtp_okay(sock, buf) == 0) {
		die("%s", buf);
	}

	/* Send all the To: adresses */
	/* Either we're using the -t option, or we're using the arguments */
	if(minus_t) {
		if(rcpt_list.next == (rcpt_t *)NULL) {
			die("No recipients specified although -t option used");
		}
		rt = &rcpt_list;

		while(rt->next) {
			p = rcpt_remap(rt->string);
			smtp_write(sock, "RCPT TO:<%s>", p);

			(void)alarm((unsigned)MEDWAIT);

			if(smtp_okay(sock, buf) == 0) {
#if 0
				die("RCPT TO:<%s> (%s)", p, buf);
#else
					/*
					 * We'd like to behave differently for
					 * certain responses, such as 421.
					 * But even smtp_okay() only has 
					 * access to the '4', and some 4xx 
					 * responses are ok.  So for now,
					 * always try to continue.
					 */
					log_event
					    (LOG_WARNING, "Invalid recipient "
					     "%s; continuing to send to "
					     "other recipients", p);
#endif
			}

			rt = rt->next;
		}
	}
	else {
		for(i = 1; (argv[i] != NULL); i++) {
			p = strtok(argv[i], ",");
			while(p) {
				/* RFC822 Address -> "foo@bar" */
				q = rcpt_remap(addr_parse(p));
				smtp_write(sock, "RCPT TO:<%s>", q);

				(void)alarm((unsigned) MEDWAIT);

				if(smtp_okay(sock, buf) == 0) {
#if 0
					die("RCPT TO:<%s> (%s)", q, buf);
#else
					/*
					 * See above comment
					 */
					log_event
					    (LOG_WARNING, "Invalid recipient "
					     "%s; continuing to send to "
					     "other recipients", q);
#endif
				}

				p = strtok(NULL, ",");
			}
		}
	}

	/* Send DATA */
	smtp_write(sock, "DATA");
	(void)alarm((unsigned) MEDWAIT);

	if(smtp_read(sock, buf) != 3) {
		/* Oops, we were expecting "354 send your data" */
		die("%s", buf);
	}

	smtp_write(sock,
		"Received: by %s (sSMTP sendmail emulation); %s", hostname, arpadate);

	if(have_from == False) {
		smtp_write(sock, "From: %s", from);
	}

	if(have_date == False) {
		smtp_write(sock, "Date: %s", arpadate);
	}

#ifdef HASTO_OPTION
	if(have_to == False) {
		smtp_write(sock, "To: postmaster");
	}
#endif

	ht = &headers;
	while(ht->next) {
		smtp_write(sock, "%s", ht->string);
		ht = ht->next;
	}

	(void)alarm((unsigned) MEDWAIT);

	/* End of headers, start body */
	smtp_write(sock, "");

	while(fgets(buf, sizeof(buf) - 2, stdin)) {
		/* Trim off \n, double leading .'s */
		standardise(buf);

		smtp_write(sock, "%s", buf);

		(void)alarm((unsigned) MEDWAIT);
	}
	/* End of body */

	smtp_write(sock, ".");
	(void)alarm((unsigned) MAXWAIT);

	if(smtp_okay(sock, buf) == 0) {
		die("%s", buf);
	}

	/* Close conection */
	(void)signal(SIGALRM, SIG_IGN);

	smtp_write(sock, "QUIT");
	(void)smtp_okay(sock, buf);
	(void)close(sock);

	log_event(LOG_INFO, "Sent mail for %s (%s)", from_strip(uad), buf);

	return(0);
}

/*
paq() - Write error message and exit
*/
void paq(char *format, ...)
{
	va_list ap;   

	va_start(ap, format);
	(void)vfprintf(stderr, format, ap);
	va_end(ap);

	exit(0);
}

/*
parse_options() -- Pull the options out of the command-line
	Process them (special-case calls to mailq, etc) and return the rest
*/
char **parse_options(int argc, char *argv[])
{
	static char Version[] = VERSION;
	static char *new_argv[MAXARGS];
	int i, j, add, new_argc;

	new_argv[0] = argv[0];
	new_argc = 1;

	if(strcmp(prog, "mailq") == 0) {
		/* Someone wants to know the queue state... */
		paq("mailq: Mail queue is empty\n");
	}
	else if(strcmp(prog, "newaliases") == 0) {
		/* Someone wanted to rebuild aliases */
		paq("newaliases: Aliases are not used in sSMTP\n");
	}

	i = 1;
	while(i < argc) {
		if(argv[i][0] != '-') {
			new_argv[new_argc++] = argv[i++];
			continue;
		}
		j = 0;

		add = 1;
		while(argv[i][++j] != NUL) {
			switch(argv[i][j]) {
#ifdef INET6
			case '6':
				p_family = PF_INET6;
				continue;

			case '4':
				p_family = PF_INET;
			continue;
#endif

			case 'a':
				switch(argv[i][++j]) {
				case 'u':
					if((!argv[i][(j + 1)])
						&& argv[(i + 1)]) {
						auth_user = strdup(argv[i+1]);
						if(auth_user == (char *)NULL) {
							die("parse_options() -- strdup() failed");
						}
						add++;
					}
					else {
						auth_user = strdup(argv[i]+j+1);
						if(auth_user == (char *)NULL) {
							die("parse_options() -- strdup() failed");
						}
					}
					goto exit;

				case 'p':
					if((!argv[i][(j + 1)])
						&& argv[(i + 1)]) {
						auth_pass = strdup(argv[i+1]);
						if(auth_pass == (char *)NULL) {
							die("parse_options() -- strdup() failed");
						}
						add++;
					}
					else {
						auth_pass = strdup(argv[i]+j+1);
						if(auth_pass == (char *)NULL) {
							die("parse_options() -- strdup() failed");
						}
					}
					goto exit;

/*
#ifdef MD5AUTH
*/
				case 'm':
					if(!argv[i][j+1]) { 
						auth_method = strdup(argv[i+1]);
						add++;
					}
					else {
						auth_method = strdup(argv[i]+j+1);
					}
				}
				goto exit;
/*
#endif
*/

			case 'b':
				switch(argv[i][++j]) {

				case 'a':	/* ARPANET mode */
						paq("-ba is not supported by sSMTP\n");
				case 'd':	/* Run as a daemon */
						paq("-bd is not supported by sSMTP\n");
				case 'i':	/* Initialise aliases */
						paq("%s: Aliases are not used in sSMTP\n", prog);
				case 'm':	/* Default addr processing */
						continue;

				case 'p':	/* Print mailqueue */
						paq("%s: Mail queue is empty\n", prog);
				case 's':	/* Read SMTP from stdin */
						paq("-bs is not supported by sSMTP\n");
				case 't':	/* Test mode */
						paq("-bt is meaningless to sSMTP\n");
				case 'v':	/* Verify names only */
						paq("-bv is meaningless to sSMTP\n");
				case 'z':	/* Create freeze file */
						paq("-bz is meaningless to sSMTP\n");
				}

			/* Configfile name */
			case 'C':
                                if (config_file) {
                                        free(config_file);
                                }
                                if((!argv[i][(j + 1)])
                                   && argv[(i + 1)]) {
                                        config_file = strdup(argv[i+1]);
                                        if(config_file == (char *)NULL) {
                                                die("parse_options() -- strdup() failed");
                                        }
                                        add++;
                                }
                                else {
                                        config_file = strdup(argv[i]+j+1);
                                        if(config_file == (char *)NULL) {
                                                die("parse_options() -- strdup() failed");
                                        }
                                }
                                goto exit;

			/* Debug */
			case 'd':
				log_level = 1;
				/* Almost the same thing... */
				minus_v = True;

				continue;

			/* Insecure channel, don't trust userid */
			case 'E':
					continue;

			case 'R':
				/* Amount of the message to be returned */
				if(!argv[i][j+1]) {
					add++;
					goto exit;
				}
				else {
					/* Process queue for recipient */
					continue;
				}

			/* Fullname of sender */
			case 'F':
				if((!argv[i][(j + 1)]) && argv[(i + 1)]) {
					minus_F = strdup(argv[(i + 1)]);
					if(minus_F == (char *)NULL) {
						die("parse_options() -- strdup() failed");
					}
					add++;
				}
				else {
					minus_F = strdup(argv[i]+j+1);
					if(minus_F == (char *)NULL) {
						die("parse_options() -- strdup() failed");
					}
				}
				goto exit;

			/* Set from/sender address */
			case 'f':
			/* Obsolete -f flag */
			case 'r':
				if((!argv[i][(j + 1)]) && argv[(i + 1)]) {
					minus_f = strdup(argv[(i + 1)]);
					if(minus_f == (char *)NULL) {
						die("parse_options() -- strdup() failed");
					}
					add++;
				}
				else {
					minus_f = strdup(argv[i]+j+1);
					if(minus_f == (char *)NULL) {
						die("parse_options() -- strdup() failed");
					}
				}
				goto exit;

			/* Set hopcount */
			case 'h':
				continue;

			/* Ignore originator in adress list */
			case 'm':
				continue;

			/* Use specified message-id */
			case 'M':
				goto exit;

			/* DSN options */
			case 'N':
				add++;
				goto exit;

			/* No aliasing */
			case 'n':
				continue;

			case 'o':
				switch(argv[i][++j]) {

				/* Alternate aliases file */
				case 'A':
					goto exit;

				/* Delay connections */
				case 'c':
					continue;

				/* Run newaliases if required */
				case 'D':
					paq("%s: Aliases are not used in sSMTP\n", prog);

				/* Deliver now, in background or queue */
				/* This may warrant a diagnostic for b or q */
				case 'd':
						continue;

				/* Errors: mail, write or none */
				case 'e':
					j++;
					continue;

				/* Set tempfile mode */
				case 'F':
					goto exit;

				/* Save ``From ' lines */
				case 'f':
					continue;

				/* Set group id */
				case 'g':
					goto exit;

				/* Helpfile name */
				case 'H':
					continue;

				/* DATA ends at EOF, not \n.\n */
				case 'i':
					continue;

				/* Log level */
				case 'L':
					goto exit;

				/* Send to me if in the list */
				case 'm':
					continue;

				/* Old headers, spaces between adresses */
				case 'o':
					paq("-oo is not supported by sSMTP\n");

				/* Queue dir */
				case 'Q':
					goto exit;

				/* Read timeout */
				case 'r':
					goto exit;

				/* Always init the queue */
				case 's':
					continue;

				/* Stats file */
				case 'S':
					goto exit;

				/* Queue timeout */
				case 'T':
					goto exit;

				/* Set timezone */
				case 't':
					goto exit;

				/* Set uid */
				case 'u':
					goto exit;

				/* Set verbose flag */
				case 'v':
					minus_v = True;
					continue;
				}
				break;

			/* Process the queue [at time] */
			case 'q':
					paq("%s: Mail queue is empty\n", prog);

			/* Read message's To/Cc/Bcc lines */
			case 't':
				minus_t = True;
				continue;

			/* minus_v (ditto -ov) */
			case 'v':
				minus_v = True;
				break;

			/* Say version and quit */
			/* Similar as die, but no logging */
			case 'V':
				paq("sSMTP %s (Not sendmail at all)\n", Version);
			}
		}

		exit:
		i += add;
	}
	new_argv[new_argc] = NULL;

	if(new_argc <= 1 && !minus_t) {
		paq("%s: No recipients supplied - mail will not be sent\n", prog);
	}

	if(new_argc > 1 && minus_t) {
		paq("%s: recipients with -t option not supported\n", prog);
	}

	return(&new_argv[0]);
}

/*
main() -- make the program behave like sendmail, then call ssmtp
*/
int main(int argc, char **argv)
{
	char **new_argv;

	/* Try to be bulletproof :-) */
	(void)signal(SIGHUP, SIG_IGN);
	(void)signal(SIGINT, SIG_IGN);
	(void)signal(SIGTTIN, SIG_IGN);
	(void)signal(SIGTTOU, SIG_IGN);

	/* Set the globals */
	prog = basename(argv[0]);

	if(gethostname(hostname, sizeof(hostname)) == -1) {
		die("Cannot get the name of this machine");
	}
        hostname[sizeof(hostname) - 1] = '\0';
	new_argv = parse_options(argc, argv);

	exit(ssmtp(new_argv));
}
