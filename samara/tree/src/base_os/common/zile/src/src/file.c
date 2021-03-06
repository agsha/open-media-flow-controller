/*	$Id: file.c,v 1.2 2003/09/19 17:21:28 gregs Exp $	*/

/*
 * Copyright (c) 1997-2001 Sandro Sigala.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <assert.h>
#include <errno.h>
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
#include <pwd.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <utime.h>

#include "zile.h"
#include "pathbuffer.h"
#include "extern.h"

int exist_file(char *filename)
{
	struct stat st;

	if (stat(filename, &st) == -1)
		if (errno == ENOENT)
			return FALSE;

	return TRUE;
}

int is_regular_file(char *filename)
{
	struct stat st;

	if (stat(filename, &st) == -1) {
		if (errno == ENOENT)
			return TRUE;
		return FALSE;
	}
	if (S_ISREG(st.st_mode))
		return TRUE;

	return FALSE;
}

#define CHECK_DP_AND_ENLARGE   do {                                           \
                                 if (dp == end_of_dp) {                       \
				   size_t length;                             \
                                   length = (size_t)(dp-pathbuffer_str(dir)); \
				   pathbuffer_realloc_larger(dir);            \
				   dp = pathbuffer_str(dir) + length;         \
				   end_of_dp = dp + pathbuffer_size(dir);     \
			         }                                            \
                               } while(0)

/*
 * This functions does some corrections and expansions to
 * the passed path:
 * - Splits the path into the directory and the filename;
 * - Expands the `~/' and `~name/' expressions;
 * - replaces the `//' with `/' (restarting from the root directory);
 * - removes the `..' and `.' entries.
 */
int expand_path(char *path, char *cwdir, pathbuffer_t *dir,
		pathbuffer_t *fname)
{
	char buf[1024];
	struct passwd *pw;
	char *sp, *dp, *fp, *p;
	char *end_of_dp;

	sp = path;
	dp = pathbuffer_str(dir);
	end_of_dp = dp + pathbuffer_size(dir);
	fp = pathbuffer_str(fname);

	if (*sp != '/') {
		p = cwdir;
		while (*p != '\0') {
			CHECK_DP_AND_ENLARGE;
			*dp++ = *p++;
		}
		if (*(dp - 1) != '/') {
			CHECK_DP_AND_ENLARGE;
			*dp++ = '/';
		}
	}

	while (sp != '\0')
		if (*sp == '/') {
			if (*(sp + 1) == '/') {
				/*
				 * Got `//'.  Restart from this point.
				 */
				while (*++sp == '/')
					;
				dp = pathbuffer_str(dir);
				CHECK_DP_AND_ENLARGE;
				*dp++ = '/';
			} else {
				++sp;
				CHECK_DP_AND_ENLARGE;
				*dp++ = '/';
			}
		} else if (*sp == '~') {
			if (*(sp + 1) == '/') {
				/*
				 * Got `~/'.  Restart from this point
				 * and insert the user home directory.
				 */
				dp = pathbuffer_str(dir);
				if ((pw = getpwuid(getuid())) == NULL)
					return FALSE;
				if (strcmp(pw->pw_dir, "/") != 0) {
					p = pw->pw_dir;
					while (*p != '\0') {
						CHECK_DP_AND_ENLARGE;
						*dp++ = *p++;
					}
				}
				++sp;
			} else {
				/*
				 * Got `~something'.  Restart from this point
				 * and insert that user home directory.
				 */
				dp = pathbuffer_str(dir);
				p = buf;
				++sp;
				while (*sp != '\0' && *sp != '/')
					*p++ = *sp++;
				*p = '\0';
				if ((pw = getpwnam(buf)) == NULL)
					return FALSE;
				p = pw->pw_dir;
				while (*p != '\0') {
					CHECK_DP_AND_ENLARGE;
					*dp++ = *p++;
				}
			}
		} else if (*sp == '.') {
			if (*(sp + 1) == '/' || *(sp + 1) == '\0') {
				++sp;
				if (*sp == '/' && *(sp + 1) != '/')
					++sp;
			} else if (*(sp + 1) == '.' &&
				   (*(sp + 2) == '/' || *(sp + 2) == '\0')) {
				if (dp > pathbuffer_str(dir) && 
				    *(dp - 1) == '/')
					--dp;
				while (*(dp - 1) != '/' &&
				       dp > pathbuffer_str(dir))
					--dp;
				sp += 2;
				if (*sp == '/' && *(sp + 1) != '/')
					++sp;
			} else
				goto gotfname;
		} else {
		gotfname:
			p = sp;
			while (*p != '\0' && *p != '/')
				p++;
			if (*p == '\0') {
				size_t fname_length;
				fname_length = strlen(sp);
				while (pathbuffer_size(fname) <
				       fname_length + 1) {
					pathbuffer_realloc_larger(fname);
					fp = pathbuffer_str(fname);
				}
				/* Final filename. */
				while ((*fp++ = *sp++) != '\0')
					;
				break;
			} else {
				/* Non-final directory. */
				while (*sp != '/') {
					CHECK_DP_AND_ENLARGE;
					*dp++ = *sp++;
				}
			}
		}

	if (dp == pathbuffer_str(dir)) {
		CHECK_DP_AND_ENLARGE;
		*dp++ = '/';
	}

	CHECK_DP_AND_ENLARGE;
	*dp = '\0';
	*fp = '\0';

	return TRUE;
}

/*
 * Return a `~/foo' like path if the user is under his home directory,
 * else the unmodified path.
 */
pathbuffer_t *compact_path(pathbuffer_t *buf, char *path)
{
	struct passwd *pw;
	int i;

	if ((pw = getpwuid(getuid())) == NULL) {
		/* User not found in password file. */
		pathbuffer_put(buf, path);
		return buf;
	}

	/*
	 * Replace `/userhome/' (if existent) with `~/'.
	 */
	i = strlen(pw->pw_dir);
	if (!strncmp(pw->pw_dir, path, i)) {
		pathbuffer_put(buf, "~/");
		if (!strcmp(pw->pw_dir, "/"))
			pathbuffer_append(buf, path + 1);
		else
			pathbuffer_append(buf, path + i + 1);
	} else
		pathbuffer_put(buf, path);

	return buf;
}

/*
 * Return the current directory.
 */
pathbuffer_t *get_current_dir(pathbuffer_t *buf)
{
	if (cur_bp->filename != NULL) {
		/*
		 * If the current buffer has a filename, get the current
		 * directory name from it.
		 */
		char *p;
		char *buf_p;

		pathbuffer_put(buf, cur_bp->filename);
		buf_p = pathbuffer_str(buf);
		if ((p = strrchr(buf_p, '/')) != NULL) {
			if (p != buf_p)
				p[0] = '\0';
			else
				p[1] = '\0';
		}
		if (buf_p[strlen(buf_p) - 1] != '/')
			pathbuffer_append(buf, "/");
	} else {
		/*
		 * Get the current directory name from the system.
		 */
		while (getcwd(pathbuffer_str(buf), pathbuffer_size(buf)) ==
		       NULL) {
			pathbuffer_realloc_larger(buf);
		}
		pathbuffer_append(buf, "/");
	}

	return buf;
}

void open_file(char *path, int lineno)
{
	pathbuffer_t *buf, *dir, *fname;

	buf = pathbuffer_create(0);
	dir = pathbuffer_create(0);
	fname = pathbuffer_create(0);
	get_current_dir(buf);
	if (!expand_path(path, pathbuffer_str(buf), dir, fname)) {
		fprintf(stderr, "zile: %s: invalid filename or path\n", path);
		zile_exit(1);
	}
	pathbuffer_put(buf, pathbuffer_str(dir));
	pathbuffer_append(buf, pathbuffer_str(fname));
	pathbuffer_free(dir);
	pathbuffer_free(fname);

	find_file(pathbuffer_str(buf));
	pathbuffer_free(buf);
	if (lineno > 0)
		ngotodown(lineno);
	resync_redisplay();
}

/*
 * Add the character `c' to the end of the line `lp'.
 * Reallocate the line if there is no more space left for the addition.
 */
static linep fadd_char(linep lp, int c)
{
	linep lp1;

	if (lp->size + 1 >= lp->maxsize) {
		lp->maxsize += 10;
		lp1 = (linep)zrealloc(lp, sizeof *lp + lp->maxsize - sizeof lp->text);
		if (lp != lp1) {
			if (cur_bp->limitp->next == lp)
				cur_bp->limitp->next = lp1;
			lp1->prev->next = lp1;
			lp1->next->prev = lp1;
			lp = lp1;
		}
	}
	lp->text[lp->size++] = c;

	return lp;
}

/*
 * Add a newline at the end of the line `lp'.
 */
static linep fadd_newline(linep lp)
{
	linep lp1;

#if 0
	lp->maxsize = lp->size ? lp->size : 1;
	lp1 = (linep)zrealloc(lp, sizeof *lp + lp->maxsize - sizeof lp->text);
	if (lp != lp1) {
		if (cur_bp->limitp->next == lp)
			cur_bp->limitp->next = lp1;
		lp1->prev->next = lp1;
		lp1->next->prev = lp1;
		lp = lp1;
	}
#endif
	lp1 = new_line(1);
	lp1->next = lp->next;
	lp1->next->prev = lp1;
	lp->next = lp1;
	lp1->prev = lp;
	lp = lp1;
	++cur_bp->num_lines;

	return lp;
}

/*
 * Read the file contents into a buffer.
 * Return quietly if the file doesn't exist.
 */
void read_from_disk(char *filename)
{
	linep lp;
	int fd, i, size;
	char buf[BUFSIZ];

	if ((fd = open(filename, O_RDONLY)) < 0) {
		if (errno != ENOENT) {
			minibuf_write("@b%s@@: %s", filename, strerror(errno));
			cur_bp->flags |= BFLAG_READONLY;
		}
		return;
	}

	lp = cur_wp->pointp;

	while ((size = read(fd, buf, BUFSIZ)) > 0)
		for (i = 0; i < size; i++)
			if (buf[i] != '\n')
				lp = fadd_char(lp, buf[i]);
			else
				lp = fadd_newline(lp);

	lp->next = cur_bp->limitp;
	cur_bp->limitp->prev = lp;
	cur_wp->pointp = cur_bp->limitp->next;

	close(fd);
}

static int have_extension(const char *filename, const char *exts[])
{
	int i, len, extlen;

	len = strlen(filename);
	for (i = 0; exts[i] != NULL; ++i) {
		extlen = strlen(exts[i]);
		if (len > extlen && !strcmp(filename + len - extlen, exts[i]))
			return TRUE;
	}

	return FALSE;
}

static int exists_corr_file(char *dest, char *src, const char *ext)
{
	int retvalue;
	char *p;
	strcpy(dest, src);
	if ((p = strrchr(dest, '.')) != NULL)
		*p = '\0';
	strcat(dest, ext);
	if (exist_file(dest))
		return TRUE;
	strcpy(dest, src);
	strcat(dest, ext);
	if (exist_file(dest))
		return TRUE;
	return FALSE;
}

DEFUN("switch-to-correlated-buffer", switch_to_correlated_buffer)
/*+
Find and open a file correlated with the current buffer.
Some examples of correlated files are the following:
    anyfile.c  --> anyfile.h
    anyfile.h  --> anyfile.c
    anyfile.in --> anyfile
    anyfile    --> anyfile.in
+*/
{
	const char *c_src[] = { ".c", ".C", ".cc", ".cpp", ".cxx", ".m", NULL };
	const char *c_hdr[] = { ".h", ".H", ".hpp", NULL };
	const char *in_ext[] = { ".in", ".IN", NULL };
	int i;
	char *nfile;
	char *fname;

	fname = cur_bp->filename;
	if (fname == NULL)
		fname = cur_bp->name;
	nfile = (char *)zmalloc(strlen(fname) + 10);

	if (have_extension(fname, c_src))
		for (i = 0; c_hdr[i] != NULL; ++i)
			if (exists_corr_file(nfile, fname, c_hdr[i])) {
				int retvalue = find_file(nfile);
				free(nfile);
				return retvalue;
			}
	if (have_extension(fname, c_hdr))
		for (i = 0; c_src[i] != NULL; ++i)
			if (exists_corr_file(nfile, fname, c_src[i])) {
				int retvalue = find_file(nfile);
				free(nfile);
				return retvalue;
			}
	if (have_extension(fname, in_ext))
		if (exists_corr_file(nfile, fname, "")) {
			int retvalue = find_file(nfile);
			free(nfile);
			return retvalue;
		}
	if (exists_corr_file(nfile, fname, ".in")) {
		int retvalue = find_file(nfile);
		free(nfile);
		return retvalue;
	}

	minibuf_error("No correlated file was found for this buffer");
	return FALSE;
}

/*
 * Try to identify a shell script file.
 */
static int is_shell_file(const char *filename)
{
	FILE *f;
	char buf[1024], *p;
	if ((f = fopen(filename, "r")) == NULL)
		return FALSE;
	p = fgets(buf, 1024, f);
	fclose(f);
	if (p == NULL)
		return FALSE;
	while (isspace(*p))
		++p;
	if (*p != '#')
		return FALSE;
	++p;
	while (isspace(*p))
		++p;
	if (*p != '!')
		return FALSE;
	return TRUE;
}

static void find_file_hooks(char *filename)
{
	const char *c_file[] = { ".c", ".h", ".m", NULL };
	const char *cpp_file[] = { ".C", ".H", ".cc", ".cpp",
				   ".cxx", ".hpp", NULL };
	const char *shell_file[] = { ".sh", ".csh", NULL };
	if (have_extension(filename, c_file))
		FUNCALL(c_mode);
	else if (have_extension(filename, cpp_file))
		FUNCALL(cpp_mode);
	else if (have_extension(filename, shell_file) ||
		 is_shell_file(filename))
		FUNCALL(shell_script_mode);

	if (cur_bp->mode == BMODE_TEXT) {
		if (lookup_bool_variable("text-mode-auto-fill"))
			FUNCALL(auto_fill_mode);
	} else if (lookup_bool_variable("auto-font-lock"))
		FUNCALL(font_lock_mode);
}

int find_file(char *filename)
{
	bufferp bp;
	char *s;

	for (bp = head_bp; bp != NULL; bp = bp->next)
		if (bp->filename != NULL && !strcmp(bp->filename, filename)) {
			switch_to_buffer(bp);
			return TRUE;
		}

	s = make_buffer_name(filename);
	if (strlen(s) < 1) {
		free(s);
		return FALSE;
	}

	if (!is_regular_file(filename)) {
		minibuf_error("@b%s@@ is not a regular file", filename);
		waitkey(1 * 1000);
		return FALSE;
	}

	bp = create_buffer(s);
	free(s);
	bp->filename = zstrdup(filename);
	bp->flags = 0;

	switch_to_buffer(bp);
	read_from_disk(filename);
	find_file_hooks(filename);

	thisflag |= FLAG_NEED_RESYNC;

	return TRUE;
}

historyp make_buffer_history(void)
{
	bufferp bp;
	historyp hp;

	hp = new_history(FALSE);
	for (bp = head_bp; bp != NULL; bp = bp->next)
		alist_append(hp->completions, zstrdup(bp->name));

	return hp;
}

DEFUN("find-file", find_file)
/*+
Edit a file specified by the user.  Switch to a buffer visiting the file,
creating one if none already exists.
+*/
{
	char *ms;
	pathbuffer_t* buf;

	buf = pathbuffer_create(0);
	get_current_dir(buf);
	if ((ms = minibuf_read_dir("Find file: ", pathbuffer_str(buf)))
	    == NULL) {
		pathbuffer_free(buf);
		return cancel();
	}
	pathbuffer_free(buf);

	if (ms[0] != '\0') {
		int ret_value = find_file(ms);
		free(ms);
		return ret_value;
	}

	free(ms);
	return FALSE;
}

DEFUN("find-alternate-file", find_alternate_file)
/*+
Find the file specified by the user, select its buffer, kill previous buffer.
If the current buffer now contains an empty file that you just visited
(presumably by mistake), use this command to visit the file you really want.
+*/
{
	char *ms;
	pathbuffer_t* buf;

	buf = pathbuffer_create(0);
	get_current_dir(buf);
	if ((ms = minibuf_read_dir("Find alternate: ", pathbuffer_str(buf)))
	    == NULL) {
		pathbuffer_free(buf);
		return cancel();
	}
	pathbuffer_free(buf);

	if (ms[0] != '\0' && check_modified_buffer(cur_bp)) {
		int ret_value;
		kill_buffer(cur_bp);
		ret_value = find_file(ms);
		free(ms);
		return ret_value;
	}

	free(ms);
	return FALSE;
}

DEFUN("switch-to-buffer", switch_to_buffer)
/*+
Select to the user specified buffer in the current window.
+*/
{
	char *ms;
	bufferp swbuf;
	historyp hp;

	swbuf = prev_bp != NULL ? prev_bp : cur_bp->next != NULL ? cur_bp->next : head_bp;

	hp = make_buffer_history();
	ms = minibuf_read_history("Switch to buffer (default @b%s@@): ", "", hp, swbuf->name);
	free_history(hp);
	if (ms == NULL)
		return cancel();

	if (ms[0] != '\0') {
		if ((swbuf = find_buffer(ms, FALSE)) == NULL) {
			swbuf = find_buffer(ms, TRUE);
			swbuf->flags = BFLAG_NEEDNAME | BFLAG_NOSAVE;
		}
	}

	prev_bp = cur_bp;
	switch_to_buffer(swbuf);

	return TRUE;
}

/*
 * Check if the buffer has been modified.  If so, asks the user if
 * he/she wants to save the changes.  If the response is positive, return
 * TRUE, else FALSE.
 */
int check_modified_buffer(bufferp bp)
{
	int ans;

	if (bp->flags & BFLAG_MODIFIED && !(bp->flags & BFLAG_NOSAVE))
		for (;;) {
			if ((ans = minibuf_read_yesno("Buffer @b%s@@ modified; kill anyway? (yes or no) ", bp->name)) == -1)
				return cancel();
			else if (!ans)
				return FALSE;
			break;
		}

	return TRUE;
}

/*
 * Remove the specified buffer from the buffer list and deallocate
 * its space.  Avoid killing the sole buffers and creates the scratch
 * buffer when required.
 */
void kill_buffer(bufferp kill_bp)
{
	bufferp next_bp;

	if (kill_bp == prev_bp)
		prev_bp = NULL;

	if (kill_bp->next != NULL)
		next_bp = kill_bp->next;
	else
		next_bp = head_bp;

	if (next_bp == kill_bp) {
		/*
		 * This is the sole buffer available, then
		 * remove the contents and set the name to `*scratch*'
		 * if it is not already set.
		 */
		assert(cur_bp == kill_bp);
		zap_buffer_content();
		cur_bp->flags = BFLAG_NOSAVE | BFLAG_NEEDNAME | BFLAG_TEMPORARY;
		if (strcmp(cur_bp->name, "*scratch*") != 0) {
			set_buffer_name(cur_bp, "*scratch*");
			if (cur_bp->filename != NULL) {
				free(cur_bp->filename);
				cur_bp->filename = NULL;
			}
		}
	} else {
		bufferp bp;
		windowp wp;
		linep pointp;
		int pointo, pointn;

		assert(kill_bp != next_bp);

		pointp = next_bp->save_pointp;
		pointo = next_bp->save_pointo;
		pointn = next_bp->save_pointn;

		/*
		 * Search for windows displaying the next buffer available.
		 */
		for (wp = head_wp; wp != NULL; wp = wp->next)
			if (wp->bp == next_bp) {
				pointp = wp->pointp;
				pointo = wp->pointo;
				pointn = wp->pointn;
				break;
			}

		/*
		 * Search for windows displaying the buffer to kill.
		 */
		for (wp = head_wp; wp != NULL; wp = wp->next)
			if (wp->bp == kill_bp) {
				--kill_bp->num_windows;
				++next_bp->num_windows;
				wp->bp = next_bp;
				wp->pointp = pointp;
				wp->pointo = pointo;
				wp->pointn = pointn;
			}

		assert(kill_bp->num_windows == 0);

		/*
		 * Remove the buffer from the buffer list.
		 */
		cur_bp = next_bp;
		if (head_bp == kill_bp)
			head_bp = head_bp->next;
		for (bp = head_bp; bp->next != NULL; bp = bp->next)
			if (bp->next == kill_bp) {
				bp->next = bp->next->next;
				break;
			}

		free_buffer(kill_bp);

		thisflag |= FLAG_NEED_RESYNC;
	}
}

DEFUN("kill-buffer", kill_buffer)
/*+
Kill the current buffer or the user specified one.
+*/
{
	bufferp bp;
	char *ms;
	historyp hp;

	hp = make_buffer_history();
	if ((ms = minibuf_read_history("Kill buffer (default @b%s@@): ", "", hp, cur_bp->name)) == NULL)
		return cancel();
	free_history(hp);
	if (ms[0] != '\0') {
		if ((bp = find_buffer(ms, FALSE)) == NULL) {
			minibuf_error("Buffer `@b%s@@' not found", ms);
			return FALSE;
		}
	} else
		bp = cur_bp;

	if (check_modified_buffer(bp)) {
		kill_buffer(bp);
		return TRUE;
	}

	return FALSE;
}

void insert_buffer(bufferp bp)
{
	linep lp;
	char *p;

	for (lp = bp->limitp->next; lp != bp->limitp; lp = lp->next) {
		for (p = lp->text; p < lp->text + lp->size; ++p)
			insert_char(*p);
		if (lp->next != bp->limitp)
			insert_newline();
	}
}

DEFUN("insert-buffer", insert_buffer)
/*+
Insert after point the contents of the user specified buffer.
Puts mark after the inserted text.
+*/
{
	bufferp bp, swbuf;
	char *ms;
	historyp hp;

	if (warn_if_readonly_buffer())
		return FALSE;

	swbuf = prev_bp != NULL ? prev_bp : cur_bp->next != NULL ? cur_bp->next : head_bp;
	hp = make_buffer_history();
	if ((ms = minibuf_read_history("Insert buffer (default @b%s@@): ", "", hp, swbuf->name)) == NULL)
		return cancel();
	free_history(hp);
	if (ms[0] != '\0') {
		if ((bp = find_buffer(ms, FALSE)) == NULL) {
			minibuf_error("Buffer `@b%s@@' not found", ms);
			return FALSE;
		}
	} else
		bp = swbuf;

	if (bp == cur_bp) {
		minibuf_error("Cannot insert the current buffer");
		return FALSE;
	}

	insert_buffer(bp);
	set_mark_command();

	return TRUE;
}

int insert_file(char *filename)
{
	int fd, i, size;
	char buf[BUFSIZ];

	if (!exist_file(filename)) {
		minibuf_error("Unable to read file `@b%s@@'", filename);
		return FALSE;
	}

	if ((fd = open(filename, O_RDONLY)) < 0) {
		minibuf_write("@b%s@@: %s", filename, strerror(errno));
		return FALSE;
	}

	while ((size = read(fd, buf, BUFSIZ)) > 0)
		for (i = 0; i < size; i++)
			if (buf[i] != '\n')
				insert_char(buf[i]);
			else
				insert_newline();
	close(fd);

	return TRUE;
}

DEFUN("insert-file", insert_file)
/*+
Insert contents of the user specified file into buffer after point.
Set mark after the inserted text.
+*/
{
	char *ms;
	pathbuffer_t *buf;

	if (warn_if_readonly_buffer())
		return FALSE;

	buf = pathbuffer_create(0);
	get_current_dir(buf);
	if ((ms = minibuf_read_dir("Insert file: ", pathbuffer_str(buf)))
	    == NULL) {
		pathbuffer_free(buf);
		return cancel();
	}
	pathbuffer_free(buf);

	if (ms[0] == '\0') {
		free(ms);
		return FALSE;
	}

	if (!insert_file(ms)) {
		free(ms);
		return FALSE;
	}

	set_mark_command();

	free(ms);
	return TRUE;
}

/*
 * Create a backup filename according to user specified variables.
 */

static char *create_backup_filename(char *filename, int withrevs,
				    int withdirectory)
{
	pathbuffer_t *buf;
	char *s;

	buf = NULL;  /* to know if it has been used */
	/* Add the backup directory path to the filename */
	if (withdirectory) {
		pathbuffer_t *dir, *fname;
		char *bp, *backupdir;

		backupdir = get_variable("backup-directory");
		buf = pathbuffer_create(strlen(backupdir) + strlen(filename) +
					1);
		bp = pathbuffer_str(buf);

		while (*backupdir != '\0')
			*bp++ = *backupdir++;
		if (*(bp-1) != '/')
			*bp++ = '/';
		while (*filename != '\0') {
			if (*filename == '/')
				*bp++ = '!';
			else
				*bp++ = *filename;
			++filename;
		}
		*bp = '\0';

		dir = pathbuffer_create(0);
		fname = pathbuffer_create(0);
		if (!expand_path(pathbuffer_str(buf), "", dir, fname)) {
			fprintf(stderr, "zile: %s: invalid backup directory\n", pathbuffer_str(dir));
			zile_exit(1);
		}
		pathbuffer_put(buf, pathbuffer_str(dir));
		pathbuffer_append(buf, pathbuffer_str(fname));
		pathbuffer_free(dir);
		pathbuffer_free(fname);
		filename = pathbuffer_str(buf);
 	}

	s = (char *)zmalloc(strlen(filename) + 10);

	if (withrevs) {
		int n = 1, fd;
		/* Find a non existing filename. */
		for (;;) {
			sprintf(s, "%s~%d~", filename, n);
			if ((fd = open(s, O_RDONLY, 0)) != -1)
				close(fd);
			else
				break;
			++n;
		}
	} else { /* simple backup */
		strcpy(s, filename);
		strcat(s, "~");
	}

	if (buf != NULL)
		pathbuffer_free(buf);

	return s;
}

/*
 * Copy a file.
 */
static int copy_file(char *source, char *dest)
{
	char buf[BUFSIZ];	
	int ifd, ofd, len, stat_valid;
	struct stat st;

	ifd = open(source, O_RDONLY, 0);
	if (ifd < 0) {
		minibuf_error("@b%s@@: unable to backup", source);
		return FALSE;
	}

	ofd = open(dest, O_WRONLY | O_CREAT | O_TRUNC, 0600);
	if (ifd < 0) {
		close(ifd);
		minibuf_error("@b%s@@: unable to create backup", dest);
		return FALSE;
	}

	while ((len = read(ifd, buf, sizeof buf)) > 0)
		if(write(ofd, buf, len) < 0) {
			minibuf_error("unable to write to backup @b%s@@", dest);
			close(ifd);
			close(ofd);
			return FALSE;
		}

	stat_valid = fstat(ifd, &st) != -1;

	/* Recover file permissions and ownership. */
	if (stat_valid) {
		fchmod(ofd, st.st_mode);
		fchown(ofd, st.st_uid, st.st_gid);
	}

	close(ifd);
	close(ofd);

	/* Recover file modification time. */
	if (stat_valid) {
		struct utimbuf t;
		t.actime = st.st_atime; 
		t.modtime = st.st_mtime; 
		utime(dest, &t);
	}

	return TRUE;
}

static int raw_write_to_disk(bufferp bp, char *filename)
{
	int fd;
	linep lp;

	if ((fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0600)) < 0)
		return FALSE;

	/* Save all the lines. */
	for (lp = bp->limitp->next; lp != bp->limitp; lp = lp->next) {
		write(fd, lp->text, lp->size);
		if (lp->next != bp->limitp)
			write(fd, "\n", 1);
	}

	close(fd);

	return TRUE;
}

/*
 * Write the buffer contents to a file.  Create a backup file if specified
 * by the user variables.
 */
static int write_to_disk(bufferp bp, char *filename)
{
	int fd, backupsimple, backuprevs, backupwithdir;
	linep lp;

	backupsimple = is_variable_equal("backup-method", "simple");
	backuprevs = is_variable_equal("backup-method", "revision");
	backupwithdir = lookup_bool_variable("backup-with-directory");

	/*
	 * Make backup of original file.
	 */
	if (!(bp->flags & BFLAG_BACKUP) && (backupsimple || backuprevs)
	    && (fd = open(filename, O_RDWR, 0)) != -1) {
		char *bfilename;
		close(fd);
		bfilename = create_backup_filename(filename, backuprevs,
						   backupwithdir);
		if (!copy_file(filename, bfilename))
			waitkey_discard(3 * 1000);
		free(bfilename);
		bp->flags |= BFLAG_BACKUP;
	}

	if ((fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644)) < 0) {
		minibuf_error("@b%s@@: %s", filename, strerror(errno));
		return FALSE;
	}

	/* Save all the lines. */
	for (lp = bp->limitp->next; lp != bp->limitp; lp = lp->next) {
		write(fd, lp->text, lp->size);
		if (lp->next != bp->limitp)
			write(fd, "\n", 1);
	}

	close(fd);

	return TRUE;
}

static int save_buffer(bufferp bp)
{
	char *ms, *fname = bp->filename != NULL ? bp->filename : bp->name;
	int ms_is_from_minibuffer = 0;

	if (!(bp->flags & BFLAG_MODIFIED))
		minibuf_write("(No changes need to be saved)");
	else {
		if (bp->flags & BFLAG_NEEDNAME) {
			if ((ms = minibuf_read_dir("File to save in: ", fname)) == NULL)
				return cancel();
			ms_is_from_minibuffer = 1;
			if (ms[0] == '\0') {
				free(ms);
				return FALSE;
			}

			set_buffer_filename(bp, ms);

			bp->flags &= ~BFLAG_NEEDNAME;
		} else
			ms = bp->filename;
		if (write_to_disk(bp, ms)) {
			minibuf_write("Wrote @b%s@@", ms);
			bp->flags &= ~BFLAG_MODIFIED;
		}
		bp->flags &= ~BFLAG_TEMPORARY;

		if (ms_is_from_minibuffer)
			free(ms);
	}

	return TRUE;
}

DEFUN("save-buffer", save_buffer)
/*+
Save current buffer in visited file if modified. By default, makes the
previous version into a backup file if this is the first save.
+*/
{
	return save_buffer(cur_bp);
}

DEFUN("write-file", write_file)
/*+
Write current buffer into the user specified file.
Makes buffer visit that file, and marks it not modified.
+*/
{
	char *fname = cur_bp->filename != NULL ? cur_bp->filename : cur_bp->name;
	char *ms;

	if ((ms = minibuf_read_dir("Write file: ", fname)) == NULL)
		return cancel();
	if (ms[0] == '\0') {
		free(ms);
		return FALSE;
	}

	set_buffer_filename(cur_bp, ms);

	cur_bp->flags &= ~(BFLAG_NEEDNAME | BFLAG_TEMPORARY);

	if (write_to_disk(cur_bp, ms)) {
		minibuf_write("Wrote @b%s@@", ms);
		cur_bp->flags &= ~BFLAG_MODIFIED;
	}

	free(ms);
	return TRUE;
}

static int save_some_buffers(void)
{
	bufferp bp;
	int i = 0, noask = FALSE, c;

	for (bp = head_bp; bp != NULL; bp = bp->next)
		if (bp->flags & BFLAG_MODIFIED
		    && !(bp->flags & BFLAG_NOSAVE)) {
			char *fname = bp->filename != NULL ? bp->filename : bp->name;

			++i;

			if (noask)
				save_buffer(bp);
			else {
				for (;;) {
					minibuf_write("Save file @b%s@@? (y, n, !, ., q) ", fname);

					c = cur_tp->getkey();
					switch (c) {
					case KBD_CANCEL:
					case KBD_RET:
					case ' ':
					case 'y':
					case 'n':
					case 'q':
					case '.':
					case '!':
						goto exitloop;
					}

					minibuf_error("Please answer y, n, !, . or q.");
					waitkey(2 * 1000);
				}

			exitloop:
				minibuf_clear();

				switch (c) {
				case KBD_CANCEL: /* C-g */
					return cancel();
				case 'q':
					goto endoffunc;
				case '.':
					save_buffer(bp);
					++i;
					return TRUE;
				case '!':
					noask = TRUE;
					/* FALLTHROUGH */
				case ' ':
				case 'y':
					save_buffer(bp);
					++i;
					break;
				case 'n':
				case KBD_RET:
				case KBD_DEL:
					break;
				}
			}
		}

endoffunc:
	if (i == 0)
		minibuf_write("(No files need saving)");

	return TRUE;
}


DEFUN("save-some-buffers", save_some_buffers)
/*+
Save some modified file-visiting buffers.  Asks user about each one.
+*/
{
	return save_some_buffers();
}

DEFUN("save-buffers-kill-zile", save_buffers_kill_zile)
/*+
Offer to save each buffer, then kill this Zile process.
+*/
{
	bufferp bp;
	int ans, i = 0;

	if (!save_some_buffers())
		return FALSE;

	for (bp = head_bp; bp != NULL; bp = bp->next)
		if (bp->flags & BFLAG_MODIFIED && !(bp->flags & BFLAG_NEEDNAME))
			++i;

	if (i > 0)
		for (;;) {
			if ((ans = minibuf_read_yesno("Modified buffers exist; exit anyway? (yes or no) ", "")) == -1)
				return cancel();
			else if (!ans)
				return FALSE;
			break;
		}

	thisflag |= FLAG_QUIT_ZILE;

	return TRUE;
}

/*
 * Function called on unexpected error or Zile crash (SIGSEGV).
 * Attempts to save modified buffers.
 */
void zile_exit(int exitcode)
{
	bufferp bp;

	fprintf(stderr, "Trying to save modified buffers (if any)...\r\n");
	for (bp = head_bp; bp != NULL; bp = bp->next)
		if (bp->flags & BFLAG_MODIFIED &&
		    !(bp->flags & BFLAG_NOSAVE)) {
			pathbuffer_t *buf;
			buf = pathbuffer_create(0);
			if (bp->filename != NULL)
				pathbuffer_put(buf, bp->filename);
			else
				pathbuffer_put(buf, bp->name);
			pathbuffer_append(buf, ".ZILESAVE");
			fprintf(stderr, "Saving %s...\r\n",
				pathbuffer_str(buf));
			raw_write_to_disk(bp, pathbuffer_str(buf));
			pathbuffer_free(buf);
		}
	exit(exitcode);
}

DEFUN("cd", cd)
/*+
Make the user specified directory become the current buffer's default
directory.
+*/
{
	char *ms;
	pathbuffer_t *buf;
	struct stat st;

	buf = pathbuffer_create(0);
	get_current_dir(buf);
	if ((ms = minibuf_read_dir("Change default directory: ",
				   pathbuffer_str(buf))) == NULL) {
		pathbuffer_free(buf);
		return cancel();
	}
	pathbuffer_free(buf);

	if (ms[0] != '\0') {
		if (stat(ms, &st) != 0 || !S_ISDIR(st.st_mode)) {
			minibuf_error("`@b%s@@' is not a directory", ms);
			free(ms);
			return FALSE;
		}
		if (chdir(ms) == -1) {
			minibuf_write("@b%s@@: %s", ms, strerror(errno));
			free(ms);
			return FALSE;
		}
		free(ms);
		return TRUE;
	}

	free(ms);
	return FALSE;
}
