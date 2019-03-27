/*
 * Utilities for qemacs.
 *
 * Copyright (c) 2001 Fabrice Bellard.
 * Copyright (c) 2002-2017 Charlie Gordon.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "qe.h"
#include <dirent.h>

#ifdef CONFIG_WIN32
#include <sys/timeb.h>

/* XXX: not sufficient, but OK for basic operations */
int fnmatch(const char *pattern, const char *string, int flags)
{
    if (pattern[0] == '*' && pattern[1] == '\0')
        return 0;
    else
        return !strequal(pattern, string);
}

#else
#include <fnmatch.h>
#endif

struct FindFileState {
    char path[MAX_FILENAME_SIZE];
    char dirpath[MAX_FILENAME_SIZE]; /* current dir path */
    char pattern[MAX_FILENAME_SIZE]; /* search pattern */
    const char *bufptr;
    DIR *dir;
};

FindFileState *find_file_open(const char *path, const char *pattern)
{
    FindFileState *s;

    s = qe_mallocz(FindFileState);
    if (!s)
        return NULL;
    pstrcpy(s->path, sizeof(s->path), path);
    pstrcpy(s->pattern, sizeof(s->pattern), pattern);
    s->bufptr = s->path;
    s->dirpath[0] = '\0';
    s->dir = NULL;
    return s;
}

int find_file_next(FindFileState *s, char *filename, int filename_size_max)
{
    struct dirent *dirent;
    const char *p;
    char *q;

    if (s->dir == NULL)
        goto redo;

    for (;;) {
        dirent = readdir(s->dir);
        if (dirent == NULL) {
        redo:
            if (s->dir) {
                closedir(s->dir);
                s->dir = NULL;
            }
            p = s->bufptr;
            if (*p == '\0')
                return -1;
            /* CG: get_str(&p, s->dirpath, sizeof(s->dirpath), ":") */
            q = s->dirpath;
            while (*p != ':' && *p != '\0') {
                if ((q - s->dirpath) < ssizeof(s->dirpath) - 1)
                    *q++ = *p;
                p++;
            }
            *q = '\0';
            if (*p == ':')
                p++;
            s->bufptr = p;
            s->dir = opendir(s->dirpath);
            if (!s->dir)
                goto redo;
        } else {
            if (fnmatch(s->pattern, dirent->d_name, 0) == 0) {
                makepath(filename, filename_size_max,
                         s->dirpath, dirent->d_name);
                return 0;
            }
        }
    }
}

void find_file_close(FindFileState **sp)
{
    if (*sp) {
        FindFileState *s = *sp;

        if (s->dir)
            closedir(s->dir);
        qe_free(sp);
    }
}

#ifdef CONFIG_WIN32
/* convert '\' to '/' */
static void path_win_to_unix(char *buf)
{
    char *p;

    for (p = buf; *p; p++) {
        if (*p == '\\')
            *p = '/';
    }
}
#endif

int is_directory(const char *path)
{
    struct stat st;

    if (!stat(path, &st) && S_ISDIR(st.st_mode))
        return 1;
    else
        return 0;
}

int is_filepattern(const char *filespec) 
{
    // XXX: should also accept character ranges and {} comprehensions
    int pos = strcspn(filespec, "*?");
    return filespec[pos] != '\0';
}

/* suppress redundant ".", ".." and "/" from paths */
/* XXX: make it better */
static void canonicalize_path1(char *buf, int buf_size, const char *path)
{
    const char *p;
    char *q, *q1;
    int c, abs_path;
    char file[MAX_FILENAME_SIZE];

    p = path;
    abs_path = (p[0] == '/');
    buf[0] = '\0';
    for (;;) {
        /* extract file */
        q = file;
        for (;;) {
            c = *p;
            if (c == '\0')
                break;
            p++;
            if (c == '/')
                break;
            if ((q - file) < (int)sizeof(file) - 1)
                *q++ = c;
        }
        *q = '\0';

        if (file[0] == '\0') {
            /* nothing to do */
        } else if (file[0] == '.' && file[1] == '\0') {
            /* nothing to do */
        } else if (file[0] == '.' && file[1] == '.' && file[2] == '\0') {
            /* go up one dir */
            if (buf[0] == '\0') {
                if (!abs_path)
                    goto copy;
            } else {
                /* go to previous directory, if possible */
                q1 = strrchr(buf, '/');
                /* if already going up, cannot do more */
                if (!q1 || (q1[1] == '.' && q1[2] == '.' && q1[3] == '\0'))
                    goto copy;
                else
                    *q1 = '\0';
            }
        } else {
        copy:
            /* add separator if needed */
            if (buf[0] != '\0' || (buf[0] == '\0' && abs_path))
                pstrcat(buf, buf_size, "/");
            pstrcat(buf, buf_size, file);
        }
        if (c == '\0')
            break;
    }

    /* add at least '.' or '/' */
    if (buf[0] == '\0') {
        if (abs_path)
            pstrcat(buf, buf_size, "/");
        else
            pstrcat(buf, buf_size, ".");
    }
}

void canonicalize_path(char *buf, int buf_size, const char *path)
{
    const char *p;

    /* check for URL protocol or windows drive */
    /* CG: should not skip '/' */
    /* XXX: bogus if filename contains ':' */
    p = strchr(path, ':');
    if (p) {
        if ((p - path) == 1) {
            /* windows drive: we canonicalize only the following path */
            buf[0] = p[0];
            buf[1] = p[1];
            /* CG: this will not work for non current drives */
            canonicalize_path1(buf + 2, buf_size - 2, p);
        } else {
            /* URL: it is already canonical */
            pstrcpy(buf, buf_size, path);
        }
    } else {
        /* simple unix path */
        canonicalize_path1(buf, buf_size, path);
    }
}

/* reduce path relative to homedir */
char *make_user_path(char *buf, int buf_size, const char *path)
{
    char *homedir;

    homedir = getenv("HOME");
    if (homedir) {
        int len = strlen(homedir);

        if (len && homedir[len - 1] == '/')
            len--;

        if (!memcmp(path, homedir, len) && path[len] == '/') {
            pstrcpy(buf, buf_size, "~");
            return pstrcat(buf, buf_size, path + len);
        }
    }
    return pstrcpy(buf, buf_size, path);
}

char *reduce_filename(char *dest, int size, const char *filename)
{
    const char *base = get_basename(filename);
    char *dbase, *ext, *p;

    /* Copy path unchanged */
    pstrncpy(dest, size, filename, base - filename);

    /* Strip cvs temp file prefix */
    if (base[0] == '.' && base[1] == '#' && base[2] != '\0')
        base += 2;

    pstrcat(dest, size, base);

    dbase = get_basename_nc(dest);

    /* Strip some numeric extensions (vcs version numbers) */
    for (;;) {
        /* get the last extension */
        ext = get_extension_nc(dbase);
        /* no extension */
        if (*ext != '.')
            break;
        /* keep non numeric extension */
        if (!qe_isdigit(ext[1]))
            break;
        /* keep the last extension */
        if (strchr(dbase, '.') == ext)
            break;
        /* only strip multidigit extensions */
        if (!qe_isdigit(ext[2]))
            break;
        *ext = '\0';
    }

    if (*ext == '.') {
        /* Convert all upper case filenames with extension to lower
         * case */
        for (p = dbase; *p; p++) {
            if (qe_islower(*p))
                break;
        }
        if (!*p) {
            qe_strtolower(dbase, dest + size - dbase, dbase);
        }
    }

    /* Strip backup file suffix or cvs temp file suffix */
    p = dbase + strlen(dbase);
    if (p > dbase + 1 && (p[-1] == '~' || p[-1] == '#'))
        *--p = '\0';

    return dest;
}

/* Return 1 iff filename extension appears in | separated list extlist.
 * Initial and final | do not match an empty extension, but || does.
 * Multiple tacked extensions may appear un extlist eg. |tar.gz|
 * Initial dots do not account as extension delimiters.
 * . and .. do not have an empty extension, nor do they match ||
 */
int match_extension(const char *filename, const char *extlist)
{
    const char *base, *p, *q;
    int len;

    if (!extlist)
        return 0;

    base = get_basename(filename);
    while (*base == '.')
        base++;
    len = strlen(base);
    if (len == 0)
        return 0;

    for (p = q = extlist;; p++) {
        int c = *p;
        if (c == '|' || c == '\0') {
            int len1 = p - q;
            if (len1 != 0 || (q != extlist && c != '\0')) {
                if (len > len1 && base[len - len1 - 1] == '.'
                &&  !qe_memicmp(base + (len - len1), q, len1)) {
                    return 1;
                }
            }
            if (c == '|')
                q = p + 1;
            else
                break;
        }
    }
    return 0;
}

int match_shell_handler(const char *p, const char *list)
{
    const char *base;

    if (!list)
        return 0;

    if (p[0] == '#' && p[1] == '!') {
        for (p += 2; qe_isblank(*p); p++)
            continue;
        for (base = p; *p && !qe_isspace(*p); p++) {
            if (*p == '/')
                base = p + 1;
        }
        if (memfind(list, base, p - base))
            return 1;
        if (p - base == 3 && !memcmp(base, "env", 3)) {
            while (*p != '\0' && *p != '\n') {
                for (; qe_isblank(*p); p++)
                    continue;
                base = p;
                for (; *p && !qe_isspace(*p); p++)
                    continue;
                if (*base != '-')
                    return memfind(list, base, p - base);
            }
        }
    }
    return 0;
}

/* Remove trailing slash from path, except for / directory */
int remove_slash(char *buf)
{
    int len;

    len = strlen(buf);
    if (len > 1 && buf[len - 1] == '/') {
        buf[--len] = '\0';
    }
    return len;
}

/* Append trailing slash to path if none there already */
int append_slash(char *buf, int buf_size)
{
    int len;

    len = strlen(buf);
    if (len > 0 && buf[len - 1] != '/' && len + 1 < buf_size) {
        buf[len++] = '/';
        buf[len] = '\0';
    }
    return len;
}

char *makepath(char *buf, int buf_size, const char *path,
               const char *filename)
{
    pstrcpy(buf, buf_size, path);
    append_slash(buf, buf_size);
    return pstrcat(buf, buf_size, filename);
}

void splitpath(char *dirname, int dirname_size,
               char *filename, int filename_size, const char *pathname)
{
    const char *base;

    base = get_basename(pathname);
    if (dirname)
        pstrncpy(dirname, dirname_size, pathname, base - pathname);
    if (filename)
        pstrcpy(filename, filename_size, base);
}

/* smart compare strings, lexicographical order, but collate numbers in
 * numeric order, and push * at end */
int qe_strcollate(const char *s1, const char *s2)
{
    int last, c1, c2, res, flags;

    last = '\0';
    for (;;) {
        c1 = (unsigned char)*s1++;
        c2 = (unsigned char)*s2++;
        if (c1 == c2) {
            last = c1;
            if (c1 == '\0')
                return 0;
        } else {
            break;
        }
    }
    if (c1 == '*')
        res = 1;
    else
    if (c2 == '*')
        res = -1;
    else
        res = (c1 < c2) ? -1 : 1;

    for (;;) {
        flags = qe_isdigit(c1) * 2 + qe_isdigit(c2);
        if (flags == 3) {
            last = c1;
            c1 = (unsigned char)*s1++;
            c2 = (unsigned char)*s2++;
        } else {
            break;
        }
    }
    if (!qe_isdigit(last) || flags == 0)
        return res;
    return (flags == 1) ? -1 : 1;
}

/* CG: need a local version of strcasecmp: qe_strcasecmp() */

int qe_strtobool(const char *s, int def) {
    if (s && *s) {
        return strxfind("1|y|yes|t|true", s) ? 1 : 0;
    } else {
        return def;
    }
}

/* Should return int, length of converted string? */
void qe_strtolower(char *buf, int size, const char *str)
{
    int c;

    /* This version only handles ASCII */
    if (size > 0) {
        while ((c = (unsigned char)*str++) != '\0' && size > 1) {
            *buf++ = qe_tolower(c);
            size--;
        }
        *buf = '\0';
    }
}

void skip_spaces(const char **pp)
{
    const char *p;

    p = *pp;
    while (qe_isspace(*p))
        p++;
    *pp = p;
}

int memfind(const char *list, const char *s, int len)
{
    const char *q = list;

    if (!q)
        return 0;

    for (;;) {
        int i = 0;
        for (;;) {
            int c2 = q[i];
            if (c2 == '|') {
                if (i == len)
                    return 1;
                break;
            }
            if (c2 == '\0') {
                /* match the empty string against || only */
                return (len > 0 && i == len);
            }
            if (c2 == s[i++] && i <= len)
                continue;
            for (q += i;;) {
                c2 = *q++;
                if (c2 == '\0')
                    return 0;
                if (c2 == '|')
                    break;
            }
            break;
        }
    }
}

#if 0
/* find a word in a list using '|' as separator */
int strfind(const char *keytable, const char *str)
{
    int c, len;
    const char *p;

    if (!keytable)
        return 0;

    c = *str;
    len = strlen(str);
    /* need to special case the empty string */
    if (len == 0)
        return strstr(keytable, "||") != NULL;

    /* initial and trailing | are optional */
    /* they do not cause the empty string to match */
    for (p = keytable;;) {
        if (!memcmp(p, str, len) && (p[len] == '|' || p[len] == '\0'))
            return 1;
        for (;;) {
            p = strchr(p + 1, c);
            if (!p)
                return 0;
            if (p[-1] == '|')
                break;
        }
    }
}
#else
/* Search for the string s in '|' delimited list of strings */
int strfind(const char *list, const char *s)
{
    const char *p, *q;
    int c1, c2;

    q = list;
    if (!q)
        return 0;

    if (*s == '\0') {
        /* special case the empty string: must match || in list */
        while (*q) {
            if (q[0] == '|' && q[1] == '|')
                return 1;
            q++;
        }
        return 0;
    } else {
    scan:
        p = s;
        for (;;) {
            c1 = *p++;
            c2 = *q++;
            if (c1 == '\0') {
                if (c2 == '\0' || c2 == '|')
                    return 1;
                goto skip;
            }
            if (c1 != c2) {
                for (;;) {
                    if (c2 == '|')
                        goto scan;

                    if (c2 == '\0')
                        return 0;
                skip:
                    c2 = *q++;
                }
            }
        }
    }
}
#endif

/* find a word in a list using '|' as separator, ignore case and "-_ ". */
int strxfind(const char *list, const char *s)
{
    const char *p, *q;
    int c1, c2;

    q = list;
    if (!q)
        return 0;

    if (*s == '\0') {
        /* special case the empty string: must match || in list */
        while (*q) {
            if (q[0] == '|' && q[1] == '|')
                return 1;
            q++;
        }
        return 0;
    } else {
    scan:
        p = s;
        for (;;) {
            do {
                c1 = qe_toupper((unsigned char)*p++);
            } while (c1 == '-' || c1 == '_' || c1 == ' ');
            do {
                c2 = qe_toupper((unsigned char)*q++);
            } while (c2 == '-' || c2 == '_' || c2 == ' ');
            if (c1 == '\0') {
                if (c2 == '\0' || c2 == '|')
                    return 1;
                goto skip;
            }
            if (c1 != c2) {
                for (;;) {
                    if (c2 == '|')
                        goto scan;

                    if (c2 == '\0')
                        return 0;
                skip:
                    c2 = *q++;
                }
            }
        }
    }
}

const char *strmem(const char *str, const void *mem, int size)
{
    int c, len;
    const char *p, *str_max, *p1 = mem;

    if (size <= 0)
        return (size < 0) ? NULL : str;

    size--;
    c = *p1++;
    if (!c) {
        /* cannot find chunk with embedded nuls */
        return NULL;
    }

    len = strlen(str);
    if (size >= len)
        return NULL;

    str_max = str + len - size;
    for (p = str; p < str_max; p++) {
        if (*p == c && !memcmp(p + 1, p1, size))
            return p;
    }
    return NULL;
}

const void *memstr(const void *buf, int size, const char *str)
{
    int c, len;
    const u8 *p, *buf_max;

    c = *str++;
    if (!c) {
        /* empty string matches start of buffer */
        return buf;
    }

    len = strlen(str);
    if (len >= size)
        return NULL;

    buf_max = (const u8*)buf + size - len;
    for (p = buf; p < buf_max; p++) {
        if (*p == c && !memcmp(p + 1, str, len))
            return p;
    }
    return NULL;
}

int qe_memicmp(const void *p1, const void *p2, int count)
{
    const u8 *s1 = (const u8 *)p1;
    const u8 *s2 = (const u8 *)p2;

    for (; count-- > 0; s1++, s2++) {
        if (*s1 != *s2) {
            int c1 = qe_toupper(*s1);
            int c2 = qe_toupper(*s2);
            if (c1 != c2)
                return (c2 < c1) - (c1 < c2);
        }
    }
    return 0;
}

const char *qe_stristr(const char *s1, const char *s2)
{
    int c, c1, c2, len;

    len = strlen(s2);
    if (!len)
        return s1;

    c = *s2++;
    len--;
    c1 = qe_toupper(c);
    c2 = qe_tolower(c);

    while ((c = *s1++) != '\0') {
        if (c == c1 || c == c2) {
            if (!qe_memicmp(s1, s2, len))
                return s1 - 1;
        }
    }
    return NULL;
}

/**
 * Return TRUE if val is a prefix of str (case independent). If it
 * returns TRUE, ptr is set to the next character in 'str' after the
 * prefix.
 *
 * @param str input string
 * @param val prefix to test
 * @param ptr updated after the prefix in str in there is a match
 * @return TRUE if there is a match */
int stristart(const char *str, const char *val, const char **ptr)
{
    const char *p, *q;

    p = str;
    q = val;
    while (*q != '\0') {
        if (qe_toupper((unsigned char)*p) != qe_toupper((unsigned char)*q)) {
            return 0;
        }
        p++;
        q++;
    }
    if (ptr)
        *ptr = p;
    return 1;
}

/**
 * Return TRUE if val is a prefix of str (case independent). If it
 * returns TRUE, ptr is set to the next character in 'str' after the
 * prefix.
 *
 * Spaces, dashes and underscores are also ignored in this comparison.
 *
 * @param str input string
 * @param val prefix to test
 * @param ptr updated after the prefix in str in there is a match
 * @return TRUE if there is a match */
int strxstart(const char *str, const char *val, const char **ptr)
{
    const char *p, *q;
    p = str;
    q = val;
    while (*q != '\0') {
        if (qe_toupper((unsigned char)*p) != qe_toupper((unsigned char)*q)) {
            if (*q == '-' || *q == '_' || *q == ' ') {
                q++;
                continue;
            }
            if (*p == '-' || *p == '_' || *p == ' ') {
                p++;
                continue;
            }
            return 0;
        }
        p++;
        q++;
    }
    if (ptr)
        *ptr = p;
    return 1;
}

/**
 * Compare strings str1 and str2 case independently.
 * Spaces, dashes and underscores are also ignored in this comparison.
 *
 * @param str1 input string 1 (left operand)
 * @param str2 input string 2 (right operand)
 * @return -1, 0, +1 reflecting the sign of str1 <=> str2
 */
int strxcmp(const char *str1, const char *str2)
{
    const char *p, *q;
    int d;

    p = str1;
    q = str2;
    for (;;) {
        d = qe_toupper((unsigned char)*p) - qe_toupper((unsigned char)*q);
        if (d) {
            if (*q == '-' || *q == '_' || *q == ' ') {
                q++;
                continue;
            }
            if (*p == '-' || *p == '_' || *p == ' ') {
                p++;
                continue;
            }
            return d < 0 ? -1 : +1;
        }
        if (!*p)
            break;
        p++;
        q++;
    }
    return 0;
}

int ustrstart(const unsigned int *str0, const char *val, int *lenp)
{
    const unsigned int *str = str0;

    for (; *val != '\0'; val++, str++) {
        /* assuming val is ASCII or Latin1 */
        if (*str != *val)
            return 0;
    }
    if (lenp)
        *lenp = str - str0;
    return 1;
}

const unsigned int *ustrstr(const unsigned int *str, const char *val)
{
    int c = val[0];

    for (; *str != '\0'; str++) {
        if (*str == (unsigned int)c && ustrstart(str, val, NULL))
            return str;
    }
    return NULL;
}

int ustristart(const unsigned int *str0, const char *val, int *lenp)
{
    const unsigned int *str = str0;

    for (; *val != '\0'; val++, str++) {
        /* assuming val is ASCII or Latin1 */
        if (qe_toupper(*str) != qe_toupper(*val))
            return 0;
    }
    if (lenp)
        *lenp = str - str0;
    return 1;
}

const unsigned int *ustristr(const unsigned int *str, const char *val)
{
    int c = qe_toupper(val[0]);

    for (; *str != '\0'; str++) {
        if (qe_toupper(*str) == c && ustristart(str, val, NULL))
            return str;
    }
    return NULL;
}

int umemcmp(const unsigned int *s1, const unsigned int *s2, int count)
{
    for (; count > 0; count--, s1++, s2++) {
        if (*s1 != *s2) {
            return *s1 < *s2 ? -1 : 1;
        }
    }
    return 0;
}

int ustr_get_identifier(char *buf, int buf_size, int c,
                        const unsigned int *str, int i, int n)
{
    int len = 0, j;

    if (len < buf_size) {
        /* c is assumed to be an ASCII character */
        buf[len++] = c;
    }
    for (j = i; j < n; j++) {
        c = str[j];
        if (!qe_isalnum_(c))
            break;
        if (len < buf_size - 1)
            buf[len++] = c;
    }
    if (len < buf_size) {
        buf[len] = '\0';
    }
    return j - i;
}

int ustr_get_identifier_lc(char *buf, int buf_size, int c,
                           const unsigned int *str, int i, int n)
{
    int len = 0, j;

    if (len < buf_size) {
        /* c is assumed to be an ASCII character */
        buf[len++] = qe_tolower(c);
    }
    for (j = i; j < n; j++) {
        c = str[j];
        if (!qe_isalnum_(c))
            break;
        if (len < buf_size - 1)
            buf[len++] = qe_tolower(c);
    }
    if (len < buf_size) {
        buf[len] = '\0';
    }
    return j - i;
}

int ustr_get_word(char *buf, int buf_size, int c,
                  const unsigned int *str, int i, int n)
{
    buf_t outbuf, *out;
    int j;

    out = buf_init(&outbuf, buf, buf_size);

    buf_putc_utf8(out, c);
    for (j = i; j < n; j++) {
        c = str[j];
        if (!qe_isword(c))
            break;
        buf_putc_utf8(out, c);
    }
    return j - i;
}

/* Read a token from a string, stop on a set of characters.
 * Skip spaces before and after token.
 */
void get_str(const char **pp, char *buf, int buf_size, const char *stop)
{
    char *q;
    const char *p;
    int c;

    skip_spaces(pp);
    p = *pp;
    q = buf;
    for (;;) {
        c = *p;
        /* Stop on spaces and eat them */
        if (c == '\0' || qe_isspace(c) || strchr(stop, c))
            break;
        if ((q - buf) < buf_size - 1)
            *q++ = c;
        p++;
    }
    *q = '\0';
    *pp = p;
    skip_spaces(pp);
}

/* scans a comma separated list of entries, return index of match or -1 */
/* CG: very similar to strfind */
int css_get_enum(const char *str, const char *enum_str)
{
    int val, len;
    const char *s, *s1;

    s = enum_str;
    val = 0;
    len = strlen(str);
    for (;;) {
        s1 = strchr(s, ',');
        if (s1) {
            if (len == (s1 - s) && !memcmp(s, str, len))
                return val;
            s = s1 + 1;
        } else {
            if (strequal(s, str))
                return val;
            else
                break;
        }
        val++;
    }
    return -1;
}

static unsigned short const keycodes[] = {
    KEY_SPC, KEY_DEL, KEY_RET, KEY_ESC, KEY_TAB, KEY_SHIFT_TAB,
    KEY_CTRL(' '), KEY_DEL, KEY_CTRL('\\'),
    KEY_CTRL(']'), KEY_CTRL('^'), KEY_CTRL('_'), KEY_CTRL('_'),
    KEY_LEFT, KEY_RIGHT, KEY_UP, KEY_DOWN,
    KEY_HOME, KEY_END, KEY_PAGEUP, KEY_PAGEDOWN,
    KEY_CTRL_LEFT, KEY_CTRL_RIGHT, KEY_CTRL_UP, KEY_CTRL_DOWN,
    KEY_CTRL_HOME, KEY_CTRL_END, KEY_CTRL_PAGEUP, KEY_CTRL_PAGEDOWN,
    KEY_PAGEUP, KEY_PAGEDOWN, KEY_CTRL_PAGEUP, KEY_CTRL_PAGEDOWN,
    KEY_INSERT, KEY_DELETE, KEY_DEFAULT,
    KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5,
    KEY_F6, KEY_F7, KEY_F8, KEY_F9, KEY_F10,
    KEY_F11, KEY_F12, KEY_F13, KEY_F14, KEY_F15,
    KEY_F16, KEY_F17, KEY_F18, KEY_F19, KEY_F20,
    '{', '}', '|',
};

static const char * const keystr[] = {
    "SPC",    "DEL",      "RET",      "ESC",    "TAB", "S-TAB",
    "C-SPC",  "C-?",      "C-\\",     "C-]",    "C-^", "C-_", "C-/",
    "left",   "right",    "up",       "down",
    "home",   "end",      "prior",    "next",
    "C-left", "C-right",  "C-up",     "C-down",
    "C-home", "C-end",    "C-prior",  "C-next",
    "pageup", "pagedown", "C-pageup", "C-pagedown",
    "insert", "delete",   "default",
    "f1",     "f2",       "f3",       "f4",    "f5",
    "f6",     "f7",       "f8",       "f9",    "f10",
    "f11",    "f12",      "f13",      "f14",   "f15",
    "f16",    "f17",      "f18",      "f19",   "f20",
    "LB",     "RB",       "VB",
};

int compose_keys(unsigned int *keys, int *nb_keys)
{
    unsigned int *keyp;

    if (*nb_keys < 2)
        return 0;

    /* compose KEY_ESC as META prefix */
    keyp = keys + *nb_keys - 2;
    if (keyp[0] == KEY_ESC) {
        if (keyp[1] <= 0xff) {
            keyp[0] = KEY_META(keyp[1]);
            --*nb_keys;
            return 1;
        }
    }
    return 0;
}

/* CG: this code is still quite inelegant */
static int strtokey1(const char **pp)
{
    const char *p, *p1, *q;
    int i, key;

    /* should return KEY_NONE at end and KEY_UNKNOWN if unrecognized */
    p = *pp;

    /* scan for separator */
    for (p1 = p; *p1 && *p1 != ' '; p1++)
        continue;

    for (i = 0; i < countof(keycodes); i++) {
        if (strstart(p, keystr[i], &q) && q == p1) {
            key = keycodes[i];
            *pp = p1;
            return key;
        }
    }
#if 0
    /* Cannot do this because KEY_F1..KEY_F20 are not consecutive */
    if (p[0] == 'f' && p[1] >= '1' && p[1] <= '9') {
        i = p[1] - '0';
        p += 2;
        if (qe_isdigit(*p))
            i = i * 10 + *p++ - '0';
        key = KEY_F1 + i - 1;
        *pp = p1;
        return key;
    }
#endif
    /* Should also support backslash escapes: \000 \x00 \u0000 */
    /* Should also support ^x and syntax and Ctrl- prefix for control keys */
    /* Should test for p[2] in range 'a'..'z', '@'..'_', '?' */
    if (p[0] == 'C' && p[1] == '-' && p1 == p + 3) {
        /* control */
        key = KEY_CTRL(p[2]);
        *pp = p1;
    } else {
        key = utf8_decode(&p);
        *pp = p;
    }

    return key;
}

int strtokey(const char **pp)
{
    const char *p;
    int key;

    p = *pp;
    /* Should also support A- and Alt- prefix for meta keys */
    if (p[0] == 'M' && p[1] == '-') {
        p += 2;
        key = KEY_META(strtokey1(&p));
    } else
    if (p[0] == 'C' && p[1] == '-' && p[2] == 'M' && p[3] == '-') {
        /* Should pass buffer with C-xxx to strtokey1 */
        p += 4;
        key = KEY_META(KEY_CTRL(strtokey1(&p)));
    } else {
        key = strtokey1(&p);
    }
    *pp = p;
    return key;
}

int strtokeys(const char *kstr, unsigned int *keys, int max_keys)
{
    int key, nb_keys;
    const char *p;

    p = kstr;
    nb_keys = 0;

    for (;;) {
        skip_spaces(&p);
        if (*p == '\0')
            break;
        key = strtokey(&p);
        keys[nb_keys++] = key;
        compose_keys(keys, &nb_keys);
        if (nb_keys >= max_keys)
            break;
    }
    return nb_keys;
}

int buf_put_key(buf_t *out, int key)
{
    int i, start = out->len;

    for (i = 0; i < countof(keycodes); i++) {
        if (keycodes[i] == key) {
            return buf_puts(out, keystr[i]);
        }
    }
    if (key >= KEY_META(0) && key <= KEY_META(0xff)) {
        buf_puts(out, "M-");
        buf_put_key(out, key & 0xff);
    } else
    if (key >= KEY_CTRL('a') && key <= KEY_CTRL('z')) {
        buf_printf(out, "C-%c", key + 'a' - 1);
    } else
#if 0
    /* Cannot do this because KEY_F1..KEY_F20 are not consecutive */
    if (key >= KEY_F1 && key <= KEY_F20) {
        buf_printf(out, "f%d", key - KEY_F1 + 1);
    } else
#endif
    {
        buf_putc_utf8(out, key);
    }
    return out->len - start;
}

int buf_put_keys(buf_t *out, unsigned int *keys, int nb_keys)
{
    int i, start = out->len;

    for (i = 0; i < nb_keys; i++) {
        if (i != 0)
            buf_put_byte(out, ' ');
        buf_put_key(out, keys[i]);
    }
    return out->len - start;
}

unsigned char const qe_digit_value__[128] = {
#define REPEAT16(x)  x,x,x,x,x,x,x,x,x,x,x,x,x,x,x,x
    REPEAT16(255), REPEAT16(255), REPEAT16(255),
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 255, 255, 255, 255, 255, 255,
    255, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24,
    25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 255, 255, 255, 255, 255,
    255, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24,
    25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 255, 255, 255, 255, 255,
};

#if 1
/* Should move all this to a separate source file color.c */

/* For 8K colors, We use a color system with 7936 colors:
 *   - 16 standard colors
 *   - 240 standard palette colors
 *   - 4096 colors in a 16x16x16 cube
 *   - a 256 level gray ramp
 *   - 6 256-level fade to black ramps
 *   - 6 256-level fade to white ramps
 *   - a 256 color palette with default xterm values
 *   - 256 unused slots
 */

/* Alternately we could use a system with 8157 colors:
 *   - 2 default color
 *   - 16 standard colors
 *   - 256 standard palette colors
 *   - 6859 colors in a 19x19x19 cube
 *     with ramp 0,15,31,47,63,79,95,108,121,135,
 *               148,161,175,188,201,215,228,241,255 values
 *   - 256 level gray ramp
 *   - extra space for 3 256 level ramps or 12 64 level ramps
 *   - 15 unused slots
 */

/* Another possible system for 8K colors has 8042+ colors:
 *   - 2 default color
 *   - 16 standard colors
 *   - 24 standard grey scale colors
 *   - 8000 colors in a 20x20x20 cube
 *     with ramp 0,13,27,40,54,67,81,95,108,121,135,
 *               148,161,175,188,201,215,228,241,255 values
 *   - extra grey scale colors
 *   - some unused slots
 */

typedef struct ColorDef {
    const char *name;
    unsigned int color;
} ColorDef;

static ColorDef const default_colors[] = {
    /* From HTML 4.0 spec */
    { "black",   QERGB(0x00, 0x00, 0x00) },
    { "green",   QERGB(0x00, 0x80, 0x00) },
    { "silver",  QERGB(0xc0, 0xc0, 0xc0) },
    { "lime",    QERGB(0x00, 0xff, 0x00) },

    { "gray",    QERGB(0xbe, 0xbe, 0xbe) },
    { "olive",   QERGB(0x80, 0x80, 0x00) },
    { "white",   QERGB(0xff, 0xff, 0xff) },
    { "yellow",  QERGB(0xff, 0xff, 0x00) },

    { "maroon",  QERGB(0x80, 0x00, 0x00) },
    { "navy",    QERGB(0x00, 0x00, 0x80) },
    { "red",     QERGB(0xff, 0x00, 0x00) },
    { "blue",    QERGB(0x00, 0x00, 0xff) },

    { "purple",  QERGB(0x80, 0x00, 0x80) },
    { "teal",    QERGB(0x00, 0x80, 0x80) },
    { "fuchsia", QERGB(0xff, 0x00, 0xff) },
    { "aqua",    QERGB(0x00, 0xff, 0xff) },

    /* more colors */
    { "cyan",    QERGB(0x00, 0xff, 0xff) },
    { "magenta", QERGB(0xff, 0x00, 0xff) },
    { "grey",    QERGB(0xbe, 0xbe, 0xbe) },
    { "transparent", COLOR_TRANSPARENT },
};
#define nb_default_colors  countof(default_colors)

static ColorDef *qe_colors = (ColorDef *)default_colors;
static int nb_qe_colors = nb_default_colors;

#if 0
static QEColor const tty_full_colors[8] = {
    QERGB(0x00, 0x00, 0x00),
    QERGB(0xff, 0x00, 0x00),
    QERGB(0x00, 0xff, 0x00),
    QERGB(0xff, 0xff, 0x00),
    QERGB(0x00, 0x00, 0xff),
    QERGB(0xff, 0x00, 0xff),
    QERGB(0x00, 0xff, 0xff),
    QERGB(0xff, 0xff, 0xff),
};
#endif

QEColor const xterm_colors[256] = {
    QERGB(0x00, 0x00, 0x00),
    QERGB(0xbb, 0x00, 0x00),
    QERGB(0x00, 0xbb, 0x00),
    QERGB(0xbb, 0xbb, 0x00),
    QERGB(0x00, 0x00, 0xbb),
    QERGB(0xbb, 0x00, 0xbb),
    QERGB(0x00, 0xbb, 0xbb),
    QERGB(0xbb, 0xbb, 0xbb),

    QERGB(0x55, 0x55, 0x55),
    QERGB(0xff, 0x55, 0x55),
    QERGB(0x55, 0xff, 0x55),
    QERGB(0xff, 0xff, 0x55),
    QERGB(0x55, 0x55, 0xff),
    QERGB(0xff, 0x55, 0xff),
    QERGB(0x55, 0xff, 0xff),
    QERGB(0xff, 0xff, 0xff),
#if 1
    /* Extended color palette for xterm 256 color mode */

    /* From XFree86: xc/programs/xterm/256colres.h,
     * v 1.5 2002/10/05 17:57:11 dickey Exp
     */

    /* 216 entry RGB cube with axes 0,95,135,175,215,255 */
    /* followed by 24 entry grey scale 8,18..238 */
    QERGB(0x00, 0x00, 0x00),  /* 16: Grey0 */
    QERGB(0x00, 0x00, 0x5f),  /* 17: NavyBlue */
    QERGB(0x00, 0x00, 0x87),  /* 18: DarkBlue */
    QERGB(0x00, 0x00, 0xaf),  /* 19: Blue3 */
    QERGB(0x00, 0x00, 0xd7),  /* 20: Blue3 */
    QERGB(0x00, 0x00, 0xff),  /* 21: Blue1 */
    QERGB(0x00, 0x5f, 0x00),  /* 22: DarkGreen */
    QERGB(0x00, 0x5f, 0x5f),  /* 23: DeepSkyBlue4 */
    QERGB(0x00, 0x5f, 0x87),  /* 24: DeepSkyBlue4 */
    QERGB(0x00, 0x5f, 0xaf),  /* 25: DeepSkyBlue4 */
    QERGB(0x00, 0x5f, 0xd7),  /* 26: DodgerBlue3 */
    QERGB(0x00, 0x5f, 0xff),  /* 27: DodgerBlue2 */
    QERGB(0x00, 0x87, 0x00),  /* 28: Green4 */
    QERGB(0x00, 0x87, 0x5f),  /* 29: SpringGreen4 */
    QERGB(0x00, 0x87, 0x87),  /* 30: Turquoise4 */
    QERGB(0x00, 0x87, 0xaf),  /* 31: DeepSkyBlue3 */
    QERGB(0x00, 0x87, 0xd7),  /* 32: DeepSkyBlue3 */
    QERGB(0x00, 0x87, 0xff),  /* 33: DodgerBlue1 */
    QERGB(0x00, 0xaf, 0x00),  /* 34: Green3 */
    QERGB(0x00, 0xaf, 0x5f),  /* 35: SpringGreen3 */
    QERGB(0x00, 0xaf, 0x87),  /* 36: DarkCyan */
    QERGB(0x00, 0xaf, 0xaf),  /* 37: LightSeaGreen */
    QERGB(0x00, 0xaf, 0xd7),  /* 38: DeepSkyBlue2 */
    QERGB(0x00, 0xaf, 0xff),  /* 39: DeepSkyBlue1 */
    QERGB(0x00, 0xd7, 0x00),  /* 40: Green3 */
    QERGB(0x00, 0xd7, 0x5f),  /* 41: SpringGreen3 */
    QERGB(0x00, 0xd7, 0x87),  /* 42: SpringGreen2 */
    QERGB(0x00, 0xd7, 0xaf),  /* 43: Cyan3 */
    QERGB(0x00, 0xd7, 0xd7),  /* 44: DarkTurquoise */
    QERGB(0x00, 0xd7, 0xff),  /* 45: Turquoise2 */
    QERGB(0x00, 0xff, 0x00),  /* 46: Green1 */
    QERGB(0x00, 0xff, 0x5f),  /* 47: SpringGreen2 */
    QERGB(0x00, 0xff, 0x87),  /* 48: SpringGreen1 */
    QERGB(0x00, 0xff, 0xaf),  /* 49: MediumSpringGreen */
    QERGB(0x00, 0xff, 0xd7),  /* 50: Cyan2 */
    QERGB(0x00, 0xff, 0xff),  /* 51: Cyan1 */
    QERGB(0x5f, 0x00, 0x00),  /* 52: DarkRed */
    QERGB(0x5f, 0x00, 0x5f),  /* 53: DeepPink4 */
    QERGB(0x5f, 0x00, 0x87),  /* 54: Purple4 */
    QERGB(0x5f, 0x00, 0xaf),  /* 55: Purple4 */
    QERGB(0x5f, 0x00, 0xd7),  /* 56: Purple3 */
    QERGB(0x5f, 0x00, 0xff),  /* 57: BlueViolet */
    QERGB(0x5f, 0x5f, 0x00),  /* 58: Orange4 */
    QERGB(0x5f, 0x5f, 0x5f),  /* 59: Grey37 */
    QERGB(0x5f, 0x5f, 0x87),  /* 60: MediumPurple4 */
    QERGB(0x5f, 0x5f, 0xaf),  /* 61: SlateBlue3 */
    QERGB(0x5f, 0x5f, 0xd7),  /* 62: SlateBlue3 */
    QERGB(0x5f, 0x5f, 0xff),  /* 63: RoyalBlue1 */
    QERGB(0x5f, 0x87, 0x00),  /* 64: Chartreuse4 */
    QERGB(0x5f, 0x87, 0x5f),  /* 65: DarkSeaGreen4 */
    QERGB(0x5f, 0x87, 0x87),  /* 66: PaleTurquoise4 */
    QERGB(0x5f, 0x87, 0xaf),  /* 67: SteelBlue */
    QERGB(0x5f, 0x87, 0xd7),  /* 68: SteelBlue3 */
    QERGB(0x5f, 0x87, 0xff),  /* 69: CornflowerBlue */
    QERGB(0x5f, 0xaf, 0x00),  /* 70: Chartreuse3 */
    QERGB(0x5f, 0xaf, 0x5f),  /* 71: DarkSeaGreen4 */
    QERGB(0x5f, 0xaf, 0x87),  /* 72: CadetBlue */
    QERGB(0x5f, 0xaf, 0xaf),  /* 73: CadetBlue */
    QERGB(0x5f, 0xaf, 0xd7),  /* 74: SkyBlue3 */
    QERGB(0x5f, 0xaf, 0xff),  /* 75: SteelBlue1 */
    QERGB(0x5f, 0xd7, 0x00),  /* 76: Chartreuse3 */
    QERGB(0x5f, 0xd7, 0x5f),  /* 77: PaleGreen3 */
    QERGB(0x5f, 0xd7, 0x87),  /* 78: SeaGreen3 */
    QERGB(0x5f, 0xd7, 0xaf),  /* 79: Aquamarine3 */
    QERGB(0x5f, 0xd7, 0xd7),  /* 80: MediumTurquoise */
    QERGB(0x5f, 0xd7, 0xff),  /* 81: SteelBlue1 */
    QERGB(0x5f, 0xff, 0x00),  /* 82: Chartreuse2 */
    QERGB(0x5f, 0xff, 0x5f),  /* 83: SeaGreen2 */
    QERGB(0x5f, 0xff, 0x87),  /* 84: SeaGreen1 */
    QERGB(0x5f, 0xff, 0xaf),  /* 85: SeaGreen1 */
    QERGB(0x5f, 0xff, 0xd7),  /* 86: Aquamarine1 */
    QERGB(0x5f, 0xff, 0xff),  /* 87: DarkSlateGray2 */
    QERGB(0x87, 0x00, 0x00),  /* 88: DarkRed */
    QERGB(0x87, 0x00, 0x5f),  /* 89: DeepPink4 */
    QERGB(0x87, 0x00, 0x87),  /* 90: DarkMagenta */
    QERGB(0x87, 0x00, 0xaf),  /* 91: DarkMagenta */
    QERGB(0x87, 0x00, 0xd7),  /* 92: DarkViolet */
    QERGB(0x87, 0x00, 0xff),  /* 93: Purple */
    QERGB(0x87, 0x5f, 0x00),  /* 94: Orange4 */
    QERGB(0x87, 0x5f, 0x5f),  /* 95: LightPink4 */
    QERGB(0x87, 0x5f, 0x87),  /* 96: Plum4 */
    QERGB(0x87, 0x5f, 0xaf),  /* 97: MediumPurple3 */
    QERGB(0x87, 0x5f, 0xd7),  /* 98: MediumPurple3 */
    QERGB(0x87, 0x5f, 0xff),  /* 99: SlateBlue1 */
    QERGB(0x87, 0x87, 0x00),  /* 100: Yellow4 */
    QERGB(0x87, 0x87, 0x5f),  /* 101: Wheat4 */
    QERGB(0x87, 0x87, 0x87),  /* 102: Grey53 */
    QERGB(0x87, 0x87, 0xaf),  /* 103: LightSlateGrey */
    QERGB(0x87, 0x87, 0xd7),  /* 104: MediumPurple */
    QERGB(0x87, 0x87, 0xff),  /* 105: LightSlateBlue */
    QERGB(0x87, 0xaf, 0x00),  /* 106: Yellow4 */
    QERGB(0x87, 0xaf, 0x5f),  /* 107: DarkOliveGreen3 */
    QERGB(0x87, 0xaf, 0x87),  /* 108: DarkSeaGreen */
    QERGB(0x87, 0xaf, 0xaf),  /* 109: LightSkyBlue3 */
    QERGB(0x87, 0xaf, 0xd7),  /* 110: LightSkyBlue3 */
    QERGB(0x87, 0xaf, 0xff),  /* 111: SkyBlue2 */
    QERGB(0x87, 0xd7, 0x00),  /* 112: Chartreuse2 */
    QERGB(0x87, 0xd7, 0x5f),  /* 113: DarkOliveGreen3 */
    QERGB(0x87, 0xd7, 0x87),  /* 114: PaleGreen3 */
    QERGB(0x87, 0xd7, 0xaf),  /* 115: DarkSeaGreen3 */
    QERGB(0x87, 0xd7, 0xd7),  /* 116: DarkSlateGray3 */
    QERGB(0x87, 0xd7, 0xff),  /* 117: SkyBlue1 */
    QERGB(0x87, 0xff, 0x00),  /* 118: Chartreuse1 */
    QERGB(0x87, 0xff, 0x5f),  /* 119: LightGreen */
    QERGB(0x87, 0xff, 0x87),  /* 120: LightGreen */
    QERGB(0x87, 0xff, 0xaf),  /* 121: PaleGreen1 */
    QERGB(0x87, 0xff, 0xd7),  /* 122: Aquamarine1 */
    QERGB(0x87, 0xff, 0xff),  /* 123: DarkSlateGray1 */
    QERGB(0xaf, 0x00, 0x00),  /* 124: Red3 */
    QERGB(0xaf, 0x00, 0x5f),  /* 125: DeepPink4 */
    QERGB(0xaf, 0x00, 0x87),  /* 126: MediumVioletRed */
    QERGB(0xaf, 0x00, 0xaf),  /* 127: Magenta3 */
    QERGB(0xaf, 0x00, 0xd7),  /* 128: DarkViolet */
    QERGB(0xaf, 0x00, 0xff),  /* 129: Purple */
    QERGB(0xaf, 0x5f, 0x00),  /* 130: DarkOrange3 */
    QERGB(0xaf, 0x5f, 0x5f),  /* 131: IndianRed */
    QERGB(0xaf, 0x5f, 0x87),  /* 132: HotPink3 */
    QERGB(0xaf, 0x5f, 0xaf),  /* 133: MediumOrchid3 */
    QERGB(0xaf, 0x5f, 0xd7),  /* 134: MediumOrchid */
    QERGB(0xaf, 0x5f, 0xff),  /* 135: MediumPurple2 */
    QERGB(0xaf, 0x87, 0x00),  /* 136: DarkGoldenrod */
    QERGB(0xaf, 0x87, 0x5f),  /* 137: LightSalmon3 */
    QERGB(0xaf, 0x87, 0x87),  /* 138: RosyBrown */
    QERGB(0xaf, 0x87, 0xaf),  /* 139: Grey63 */
    QERGB(0xaf, 0x87, 0xd7),  /* 140: MediumPurple2 */
    QERGB(0xaf, 0x87, 0xff),  /* 141: MediumPurple1 */
    QERGB(0xaf, 0xaf, 0x00),  /* 142: Gold3 */
    QERGB(0xaf, 0xaf, 0x5f),  /* 143: DarkKhaki */
    QERGB(0xaf, 0xaf, 0x87),  /* 144: NavajoWhite3 */
    QERGB(0xaf, 0xaf, 0xaf),  /* 145: Grey69 */
    QERGB(0xaf, 0xaf, 0xd7),  /* 146: LightSteelBlue3 */
    QERGB(0xaf, 0xaf, 0xff),  /* 147: LightSteelBlue */
    QERGB(0xaf, 0xd7, 0x00),  /* 148: Yellow3 */
    QERGB(0xaf, 0xd7, 0x5f),  /* 149: DarkOliveGreen3 */
    QERGB(0xaf, 0xd7, 0x87),  /* 150: DarkSeaGreen3 */
    QERGB(0xaf, 0xd7, 0xaf),  /* 151: DarkSeaGreen2 */
    QERGB(0xaf, 0xd7, 0xd7),  /* 152: LightCyan3 */
    QERGB(0xaf, 0xd7, 0xff),  /* 153: LightSkyBlue1 */
    QERGB(0xaf, 0xff, 0x00),  /* 154: GreenYellow */
    QERGB(0xaf, 0xff, 0x5f),  /* 155: DarkOliveGreen2 */
    QERGB(0xaf, 0xff, 0x87),  /* 156: PaleGreen1 */
    QERGB(0xaf, 0xff, 0xaf),  /* 157: DarkSeaGreen2 */
    QERGB(0xaf, 0xff, 0xd7),  /* 158: DarkSeaGreen1 */
    QERGB(0xaf, 0xff, 0xff),  /* 159: PaleTurquoise1 */
    QERGB(0xd7, 0x00, 0x00),  /* 160: Red3 */
    QERGB(0xd7, 0x00, 0x5f),  /* 161: DeepPink3 */
    QERGB(0xd7, 0x00, 0x87),  /* 162: DeepPink3 */
    QERGB(0xd7, 0x00, 0xaf),  /* 163: Magenta3 */
    QERGB(0xd7, 0x00, 0xd7),  /* 164: Magenta3 */
    QERGB(0xd7, 0x00, 0xff),  /* 165: Magenta2 */
    QERGB(0xd7, 0x5f, 0x00),  /* 166: DarkOrange3 */
    QERGB(0xd7, 0x5f, 0x5f),  /* 167: IndianRed */
    QERGB(0xd7, 0x5f, 0x87),  /* 168: HotPink3 */
    QERGB(0xd7, 0x5f, 0xaf),  /* 169: HotPink2 */
    QERGB(0xd7, 0x5f, 0xd7),  /* 170: Orchid */
    QERGB(0xd7, 0x5f, 0xff),  /* 171: MediumOrchid1 */
    QERGB(0xd7, 0x87, 0x00),  /* 172: Orange3 */
    QERGB(0xd7, 0x87, 0x5f),  /* 173: LightSalmon3 */
    QERGB(0xd7, 0x87, 0x87),  /* 174: LightPink3 */
    QERGB(0xd7, 0x87, 0xaf),  /* 175: Pink3 */
    QERGB(0xd7, 0x87, 0xd7),  /* 176: Plum3 */
    QERGB(0xd7, 0x87, 0xff),  /* 177: Violet */
    QERGB(0xd7, 0xaf, 0x00),  /* 178: Gold3 */
    QERGB(0xd7, 0xaf, 0x5f),  /* 179: LightGoldenrod3 */
    QERGB(0xd7, 0xaf, 0x87),  /* 180: Tan */
    QERGB(0xd7, 0xaf, 0xaf),  /* 181: MistyRose3 */
    QERGB(0xd7, 0xaf, 0xd7),  /* 182: Thistle3 */
    QERGB(0xd7, 0xaf, 0xff),  /* 183: Plum2 */
    QERGB(0xd7, 0xd7, 0x00),  /* 184: Yellow3 */
    QERGB(0xd7, 0xd7, 0x5f),  /* 185: Khaki3 */
    QERGB(0xd7, 0xd7, 0x87),  /* 186: LightGoldenrod2 */
    QERGB(0xd7, 0xd7, 0xaf),  /* 187: LightYellow3 */
    QERGB(0xd7, 0xd7, 0xd7),  /* 188: Grey84 */
    QERGB(0xd7, 0xd7, 0xff),  /* 189: LightSteelBlue1 */
    QERGB(0xd7, 0xff, 0x00),  /* 190: Yellow2 */
    QERGB(0xd7, 0xff, 0x5f),  /* 191: DarkOliveGreen1 */
    QERGB(0xd7, 0xff, 0x87),  /* 192: DarkOliveGreen1 */
    QERGB(0xd7, 0xff, 0xaf),  /* 193: DarkSeaGreen1 */
    QERGB(0xd7, 0xff, 0xd7),  /* 194: Honeydew2 */
    QERGB(0xd7, 0xff, 0xff),  /* 195: LightCyan1 */
    QERGB(0xff, 0x00, 0x00),  /* 196: Red1 */
    QERGB(0xff, 0x00, 0x5f),  /* 197: DeepPink2 */
    QERGB(0xff, 0x00, 0x87),  /* 198: DeepPink1 */
    QERGB(0xff, 0x00, 0xaf),  /* 199: DeepPink1 */
    QERGB(0xff, 0x00, 0xd7),  /* 200: Magenta2 */
    QERGB(0xff, 0x00, 0xff),  /* 201: Magenta1 */
    QERGB(0xff, 0x5f, 0x00),  /* 202: OrangeRed1 */
    QERGB(0xff, 0x5f, 0x5f),  /* 203: IndianRed1 */
    QERGB(0xff, 0x5f, 0x87),  /* 204: IndianRed1 */
    QERGB(0xff, 0x5f, 0xaf),  /* 205: HotPink */
    QERGB(0xff, 0x5f, 0xd7),  /* 206: HotPink */
    QERGB(0xff, 0x5f, 0xff),  /* 207: MediumOrchid1 */
    QERGB(0xff, 0x87, 0x00),  /* 208: DarkOrange */
    QERGB(0xff, 0x87, 0x5f),  /* 209: Salmon1 */
    QERGB(0xff, 0x87, 0x87),  /* 210: LightCoral */
    QERGB(0xff, 0x87, 0xaf),  /* 211: PaleVioletRed1 */
    QERGB(0xff, 0x87, 0xd7),  /* 212: Orchid2 */
    QERGB(0xff, 0x87, 0xff),  /* 213: Orchid1 */
    QERGB(0xff, 0xaf, 0x00),  /* 214: Orange1 */
    QERGB(0xff, 0xaf, 0x5f),  /* 215: SandyBrown */
    QERGB(0xff, 0xaf, 0x87),  /* 216: LightSalmon1 */
    QERGB(0xff, 0xaf, 0xaf),  /* 217: LightPink1 */
    QERGB(0xff, 0xaf, 0xd7),  /* 218: Pink1 */
    QERGB(0xff, 0xaf, 0xff),  /* 219: Plum1 */
    QERGB(0xff, 0xd7, 0x00),  /* 220: Gold1 */
    QERGB(0xff, 0xd7, 0x5f),  /* 221: LightGoldenrod2 */
    QERGB(0xff, 0xd7, 0x87),  /* 222: LightGoldenrod2 */
    QERGB(0xff, 0xd7, 0xaf),  /* 223: NavajoWhite1 */
    QERGB(0xff, 0xd7, 0xd7),  /* 224: MistyRose1 */
    QERGB(0xff, 0xd7, 0xff),  /* 225: Thistle1 */
    QERGB(0xff, 0xff, 0x00),  /* 226: Yellow1 */
    QERGB(0xff, 0xff, 0x5f),  /* 227: LightGoldenrod1 */
    QERGB(0xff, 0xff, 0x87),  /* 228: Khaki1 */
    QERGB(0xff, 0xff, 0xaf),  /* 229: Wheat1 */
    QERGB(0xff, 0xff, 0xd7),  /* 230: Cornsilk1 */
    QERGB(0xff, 0xff, 0xff),  /* 231: Grey100 */
    QERGB(0x08, 0x08, 0x08),  /* 232: Grey3 */
    QERGB(0x12, 0x12, 0x12),  /* 233: Grey7 */
    QERGB(0x1c, 0x1c, 0x1c),  /* 234: Grey11 */
    QERGB(0x26, 0x26, 0x26),  /* 235: Grey15 */
    QERGB(0x30, 0x30, 0x30),  /* 236: Grey19 */
    QERGB(0x3a, 0x3a, 0x3a),  /* 237: Grey23 */
    QERGB(0x44, 0x44, 0x44),  /* 238: Grey27 */
    QERGB(0x4e, 0x4e, 0x4e),  /* 239: Grey30 */
    QERGB(0x58, 0x58, 0x58),  /* 240: Grey35 */
    QERGB(0x62, 0x62, 0x62),  /* 241: Grey39 */
    QERGB(0x6c, 0x6c, 0x6c),  /* 242: Grey42 */
    QERGB(0x76, 0x76, 0x76),  /* 243: Grey46 */
    QERGB(0x80, 0x80, 0x80),  /* 244: Grey50 */
    QERGB(0x8a, 0x8a, 0x8a),  /* 245: Grey54 */
    QERGB(0x94, 0x94, 0x94),  /* 246: Grey58 */
    QERGB(0x9e, 0x9e, 0x9e),  /* 247: Grey62 */
    QERGB(0xa8, 0xa8, 0xa8),  /* 248: Grey66 */
    QERGB(0xb2, 0xb2, 0xb2),  /* 249: Grey70 */
    QERGB(0xbc, 0xbc, 0xbc),  /* 250: Grey74 */
    QERGB(0xc6, 0xc6, 0xc6),  /* 251: Grey78 */
    QERGB(0xd0, 0xd0, 0xd0),  /* 252: Grey82 */
    QERGB(0xda, 0xda, 0xda),  /* 253: Grey85 */
    QERGB(0xe4, 0xe4, 0xe4),  /* 254: Grey89 */
    QERGB(0xee, 0xee, 0xee),  /* 255: Grey93 */
#endif
};

#if 0
static unsigned char const scale_cube[256] = {
    /* This array is used for mapping rgb colors to the standard palette */
    /* 216 entry RGB cube with axes 0,95,135,175,215,255 */
#define REP5(x)   (x), (x), (x), (x), (x)
#define REP10(x)  REP5(x), REP5(x)
#define REP20(x)  REP10(x), REP10(x)
#define REP40(x)  REP20(x), REP20(x)
#define REP25(x)  REP5(x), REP5(x), REP5(x), REP5(x), REP5(x)
#define REP47(x)  REP10(x), REP10(x), REP10(x), REP10(x), REP5(x), (x), (x)
    REP47(0), REP47(1), REP20(1), REP40(2), REP40(3), REP40(4), REP20(5), 5
};

static unsigned char const scale_grey[256] = {
    /* This array is used for mapping gray levels to the standard palette */
    /* 232..255: 24 entry grey scale 8,18..238 */
    16, 16, 16, 16,
    REP10(232), REP10(233), REP10(234), REP10(235),
    REP10(236), REP10(237), REP10(238), REP10(239), 
    REP10(240), REP10(241), REP10(242), REP10(243), 
    REP10(244), REP10(245), REP10(246), REP10(247),
    REP10(248), REP10(249), REP10(250), REP10(251),
    REP10(252), REP10(253), REP10(254), REP10(255),
    255, 255, 255,
    231, 231, 231, 231, 231, 231, 231, 231, 231
};
#endif

static inline int color_dist(QEColor c1, QEColor c2) {
    /* using casts because c1 and c2 are unsigned */
#if 0
    /* using a quick approximation to give green extra weight */
    return      abs((int)((c1 >>  0) & 0xff) - (int)((c2 >>  0) & 0xff)) +
            2 * abs((int)((c1 >>  8) & 0xff) - (int)((c2 >>  8) & 0xff)) +
                abs((int)((c1 >> 16) & 0xff) - (int)((c2 >> 16) & 0xff));
#else
    /* using different weights to R, G, B according to luminance levels */
    return  11 * abs((int)((c1 >>  0) & 0xff) - (int)((c2 >>  0) & 0xff)) +
            59 * abs((int)((c1 >>  8) & 0xff) - (int)((c2 >>  8) & 0xff)) +
            30 * abs((int)((c1 >> 16) & 0xff) - (int)((c2 >> 16) & 0xff));
#endif
}

/* XXX: should have a more generic API with precomputed mapping scales */
/* Convert RGB triplet to a composite color */
unsigned int qe_map_color(QEColor color, QEColor const *colors, int count, int *dist)
{
    int i, cmin, dmin, d;

    color &= 0xFFFFFF;  /* mask off the alpha channel */

    if (count >= 0x1000000) {
        cmin = color | 0x1000000;  /* force explicit RGB triplet */
        dmin = 0;
    } else {
        dmin = INT_MAX;
        cmin = 0;
        if (count <= 16) {
            for (i = 0; i < count; i++) {
                d = color_dist(color, colors[i]);
                if (d < dmin) {
                    cmin = i;
                    dmin = d;
                }
            }
        } else { /* if (dmin > 0 && count > 16) */
            unsigned int r = (color >> 16) & 0xff;
            unsigned int g = (color >>  8) & 0xff;
            unsigned int b = (color >>  0) & 0xff;
#if 0
            if (r == g && g == b) {
                i = scale_grey[r];
                d = color_dist(color, colors[i]);
                if (d < dmin) {
                    cmin = i;
                    dmin = d;
                }
            } else {
                /* XXX: should use more precise projection */
                i = 16 + scale_cube[r] * 36 +
                    scale_cube[g] * 6 +
                    scale_cube[b];
                d = color_dist(color, colors[i]);
                if (d < dmin) {
                    cmin = i;
                    dmin = d;
                }
            }
#else
            if (r == g && g == b) {
                /* color is a gray tone:
                 * map to the closest palette entry
                 */
                d = color_dist(color, colors[16]);
                if (d < dmin) {
                    cmin = 16;
                    dmin = d;
                }
                for (i = 231; i < 256; i++) {
                    d = color_dist(color, colors[i]);
                    if (d < dmin) {
                        cmin = i;
                        dmin = d;
                    }
                }
            } else {
                /* general case: try and match a palette entry
                 * from the 6x6x6 color cube .
                 */
                /* XXX: this causes gliches on true color terminals
                 * with a non standard xterm palette, such as iTerm2.
                 * On true color terminals, we should treat palette
                 * colors and rgb colors differently in the shell buffer
                 * terminal emulator.
                 */
                for (i = 16; i < 232; i++) {
                    d = color_dist(color, colors[i]);
                    if (d < dmin) {
                        cmin = i;
                        dmin = d;
                    }
                }
            }
#endif
            if (dmin > 0 && count >= 4096) {
                /* 13-bit 7936 color system */
                d = 0;
                for (;;) {
                    if (r == g) {
                        if (g == b) {  /* #xxxxxx */
                            i = 0x700 + r;
                            break;
                        }
                        if (r == 0) {  /* #0000xx */
                            i = 0x100 + b;
                            break;
                        }
                        if (r == 255) {  /* #FFFFxx */
                            i = 0x800 + 0x100 + b;
                            break;
                        }
                        if (b == 0) {  /* #xxxx00 */
                            i = 0x600 + r;
                            break;
                        }
                        if (b == 255) {  /* #xxxxFF */
                            i = 0x800 + 0x600 + r;
                            break;
                        }
                    } else
                    if (r == b) {
                        if (r == 0) {  /* #00xx00 */
                            i = 0x200 + g;
                            break;
                        }
                        if (r == 255) {  /* #FFxxFF */
                            i = 0x800 + 0x200 + g;
                            break;
                        }
                        if (g == 0) {  /* #xx00xx */
                            i = 0x500 + r;
                            break;
                        }
                        if (g == 255) {  /* #xxFFxx */
                            i = 0x800 + 0x500 + r;
                            break;
                        }
                    } else
                    if (g == b) {
                        if (g == 0) {  /* #xx0000 */
                            i = 0x400 + r;
                            break;
                        }
                        if (g == 255) {  /* #xxFFFF */
                            i = 0x800 + 0x400 + r;
                            break;
                        }
                        if (r == 0) {  /* #00xxxx */
                            i = 0x300 + g;
                            break;
                        }
                        if (r == 255) {  /* #FFxxxx */
                            i = 0x800 + 0x300 + g;
                            break;
                        }
                    }
                    i = 0x1000 | ((r >> 4) << 8) | ((g >> 4) << 4) | (b >> 4);
                    d = color_dist(color, (color & 0xF0F0F0) | ((color & 0xF0F0F0) >> 4));
                    break;
                }
                if (d < dmin) {
                    cmin = i;
                    dmin = d;
                }
            }
        }
    }
    if (dist) {
        *dist = dmin;
    }
    return cmin;
}

/* Convert a composite color to an RGB triplet */
QEColor qe_unmap_color(int color, int count) {
    /* XXX: Should use an 8K array for all colors <= 8192 */
    if (color < 256) {
        return xterm_colors[color];
    }
    if (color < 8192) {
        /* 13-bit 7936 color system */
        if (color & 0x1000) {
            /* explicit 12-bit color */
            QEColor rgb = (((color & 0xF00) << 12) |
                           ((color & 0x0F0) <<  4) |
                           ((color & 0x00F) <<  0));
            return rgb | (rgb << 4);
        }
        if ((color & 0xf00) < 0xf00) {
            /* 256 level color ramps */
            /* 0x800 is unused and converts to white */
            int r, g, b;
            r = g = b = color & 0xFF;
            if (!(color & 0x400)) r = -(color >> 11) & 0xFF;
            if (!(color & 0x200)) g = -(color >> 11) & 0xFF;
            if (!(color & 0x100)) b = -(color >> 11) & 0xFF;
            return QERGB(r, g, b);
        } else {
            /* 0xf00 indicates the standard xterm color palette */
            return xterm_colors[color & 255];
        }
    }
    /* explicit RGB color */
    return color & 0xFFFFFF;
}

void color_completion(CompleteState *cp)
{
    ColorDef const *def;
    int count;

    def = qe_colors;
    count = nb_qe_colors;
    while (count > 0) {
        if (strxstart(def->name, cp->current, NULL))
            add_string(&cp->cs, def->name, 0);
        def++;
        count--;
    }
}

static int css_lookup_color(ColorDef const *def, int count,
                            const char *name)
{
    int i;

    for (i = 0; i < count; i++) {
        if (!strxcmp(def[i].name, name))
            return i;
    }
    return -1;
}

int css_define_color(const char *name, const char *value)
{
    ColorDef *def;
    QEColor color;
    int index;

    /* Check color validity */
    if (css_get_color(&color, value))
        return -1;

    /* First color definition: allocate modifiable array */
    if (qe_colors == default_colors) {
        qe_colors = qe_malloc_dup(default_colors, sizeof(default_colors));
    }

    /* Make room: reallocate table in chunks of 8 entries */
    if (((nb_qe_colors - nb_default_colors) & 7) == 0) {
        if (!qe_realloc(&qe_colors,
                        (nb_qe_colors + 8) * sizeof(ColorDef))) {
            return -1;
        }
    }
    /* Check for redefinition */
    index = css_lookup_color(qe_colors, nb_qe_colors, name);
    if (index >= 0) {
        qe_colors[index].color = color;
        return 0;
    }

    def = &qe_colors[nb_qe_colors];
    def->name = qe_strdup(name);
    def->color = color;
    nb_qe_colors++;

    return 0;
}

void css_free_colors(void)
{
    if (qe_colors != default_colors) {
        while (nb_qe_colors > nb_default_colors) {
            nb_qe_colors--;
            qe_free((char **)&qe_colors[nb_qe_colors].name);
        }
        qe_free(&qe_colors);
        qe_colors = (ColorDef *)default_colors;
    }
}

/* XXX: make HTML parsing optional ? */
int css_get_color(QEColor *color_ptr, const char *p)
{
    ColorDef const *def;
    int count, index, len, v, i, n;
    unsigned char rgba[4];

    /* search in tables */
    def = qe_colors;
    count = nb_qe_colors;
    index = css_lookup_color(def, count, p);
    if (index >= 0) {
        *color_ptr = def[index].color;
        return 0;
    }

    rgba[3] = 0xff;
    if (qe_isxdigit((unsigned char)*p)) {
        goto parse_num;
    } else
    if (*p == '#') {
        /* handle '#' notation */
        p++;
    parse_num:
        len = strlen(p);
        switch (len) {
        case 3:
            for (i = 0; i < 3; i++) {
                v = qe_digit_value(*p++);
                rgba[i] = v | (v << 4);
            }
            break;
        case 6:
            for (i = 0; i < 3; i++) {
                v = qe_digit_value(*p++) << 4;
                v |= qe_digit_value(*p++);
                rgba[i] = v;
            }
            break;
        default:
            /* error */
            return -1;
        }
    } else
    if (strstart(p, "rgb(", &p)) {
        n = 3;
        goto parse_rgba;
    } else
    if (strstart(p, "rgba(", &p)) {
        /* extension for alpha */
        n = 4;
    parse_rgba:
        for (i = 0; i < n; i++) {
            /* XXX: floats ? */
            skip_spaces(&p);
            v = strtol(p, (char **)&p, 0);
            if (*p == '%') {
                v = (v * 255) / 100;
                p++;
            }
            rgba[i] = v;
            skip_spaces(&p);
            if (*p == ',')
                p++;
        }
    } else {
        return -1;
    }
    *color_ptr = (rgba[0] << 16) | (rgba[1] << 8) |
        (rgba[2]) | (rgba[3] << 24);
    return 0;
}

/* return 0 if unknown font */
int css_get_font_family(const char *str)
{
    int v;

    if (!strcasecmp(str, "serif") ||
        !strcasecmp(str, "times"))
        v = QE_FONT_FAMILY_SERIF;
    else
    if (!strcasecmp(str, "sans") ||
        !strcasecmp(str, "arial") ||
        !strcasecmp(str, "helvetica"))
        v = QE_FONT_FAMILY_SANS;
    else
    if (!strcasecmp(str, "fixed") ||
        !strcasecmp(str, "monospace") ||
        !strcasecmp(str, "courier"))
        v = QE_FONT_FAMILY_FIXED;
    else
        v = 0; /* inherit */
    return v;
}
#endif  /* style stuff */

/* a = a union b */
void css_union_rect(CSSRect *a, const CSSRect *b)
{
    if (css_is_null_rect(b))
        return;
    if (css_is_null_rect(a)) {
        *a = *b;
    } else {
        if (b->x1 < a->x1)
            a->x1 = b->x1;
        if (b->y1 < a->y1)
            a->y1 = b->y1;
        if (b->x2 > a->x2)
            a->x2 = b->x2;
        if (b->y2 > a->y2)
            a->y2 = b->y2;
    }
}

#ifdef __TINYC__

/* the glibc folks use wrappers, but forgot to put a compatibility
   function for non GCC compilers ! */
int stat(__const char *__path,
         struct stat *__statbuf)
{
    return __xstat(_STAT_VER, __path, __statbuf);
}
#endif

int get_clock_ms(void)
{
#ifdef CONFIG_WIN32
    struct _timeb tb;

    _ftime(&tb);
    return tb.time * 1000 + tb.millitm;
#else
    struct timeval tv;

    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000 + (tv.tv_usec / 1000);
#endif
}

int get_clock_usec(void)
{
#ifdef CONFIG_WIN32
    struct _timeb tb;

    _ftime(&tb);
    return tb.time * 1000000 + tb.millitm * 1000;
#else
    struct timeval tv;

    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000 + tv.tv_usec;
#endif
}

/* set one string. */
StringItem *set_string(StringArray *cs, int index, const char *str, int group)
{
    StringItem *v;
    int len;

    if (index >= cs->nb_items)
        return NULL;

    len = strlen(str);
    v = qe_malloc_hack(StringItem, len);
    if (!v)
        return NULL;
    v->selected = 0;
    v->group = group;
    memcpy(v->str, str, len + 1);
    if (cs->items[index])
        qe_free(&cs->items[index]);
    cs->items[index] = v;
    return v;
}

/* make a generic array alloc */
StringItem *add_string(StringArray *cs, const char *str, int group)
{
    int n;

    if (cs->nb_items >= cs->nb_allocated) {
        n = cs->nb_allocated + 32;
        if (!qe_realloc(&cs->items, n * sizeof(StringItem *)))
            return NULL;
        cs->nb_allocated = n;
    }
    cs->items[cs->nb_items++] = NULL;
    return set_string(cs, cs->nb_items - 1, str, group);
}

void free_strings(StringArray *cs)
{
    int i;

    for (i = 0; i < cs->nb_items; i++)
        qe_free(&cs->items[i]);
    qe_free(&cs->items);
    memset(cs, 0, sizeof(StringArray));
}

/**
 * Add a memory region to a dynamic string. In case of allocation
 * failure, the data is not added. The dynamic string is guaranteed to
 * be 0 terminated, although it can be longer if it contains zeros.
 *
 * @return 0 if OK, -1 if allocation error.
 */
int qmemcat(QString *q, const unsigned char *data1, int len1)
{
    int new_len, len, alloc_size;

    len = q->len;
    new_len = len + len1;
    /* see if we got a new power of two */
    /* NOTE: we got this trick from the excellent 'links' browser */
    if ((len ^ new_len) >= len) {
        /* find immediately bigger 2^n - 1 */
        alloc_size = new_len;
        alloc_size |= (alloc_size >> 1);
        alloc_size |= (alloc_size >> 2);
        alloc_size |= (alloc_size >> 4);
        alloc_size |= (alloc_size >> 8);
        alloc_size |= (alloc_size >> 16);
        /* allocate one more byte for end of string marker */
        if (!qe_realloc(&q->data, alloc_size + 1))
            return -1;
    }
    memcpy(q->data + len, data1, len1);
    q->data[new_len] = '\0'; /* we force a trailing '\0' */
    q->len = new_len;
    return 0;
}

/*
 * add a string to a dynamic string
 */
int qstrcat(QString *q, const char *str)
{
    return qmemcat(q, (unsigned char *)str, strlen(str));
}

/* XXX: we use a fixed size buffer */
int qprintf(QString *q, const char *fmt, ...)
{
    char buf[4096];
    va_list ap;
    int len, ret;

    va_start(ap, fmt);
    len = vsnprintf(buf, sizeof(buf), fmt, ap);
    /* avoid problems for non C99 snprintf() which can return -1 if overflow */
    if (len < 0)
        len = strlen(buf);
    ret = qmemcat(q, (unsigned char *)buf, len);
    va_end(ap);
    return ret;
}

int buf_write(buf_t *bp, const void *src, int size)
{
    int n = 0;

    if (bp->pos < bp->size) {
        int n = bp->size - bp->pos - 1;
        if (n > size)
            n = size;
        memcpy(bp->buf + bp->len, src, n);
        bp->len += n;
        bp->buf[bp->len] = '\0';
    }
    bp->pos += size;
    return n;
}

int buf_printf(buf_t *bp, const char *fmt, ...)
{
    va_list ap;
    int len;

    va_start(ap, fmt);
    len = vsnprintf(bp->buf + bp->len,
                    (bp->pos < bp->size) ? bp->size - bp->pos : 1, fmt, ap);
    va_end(ap);

    bp->pos += len;
    bp->len += len;
    if (bp->len >= bp->size) {
        bp->len = bp->size - 1;
        if (bp->len < 0)
            bp->len = 0;
    }

    return len;
}

int buf_putc_utf8(buf_t *bp, int c)
{
    if (c < 0x80) {
        bp->pos++;
        if (bp->pos < bp->size) {
            bp->buf[bp->len++] = c;
            bp->buf[bp->len] = '\0';
            return 1;
        } else {
            return 0;
        }
    } else {
        char buf[6];
        int len;

        len = utf8_encode(buf, c);

        if (bp->pos + len < bp->size) {
            memcpy(bp->buf + bp->len, buf, len);
            bp->pos += len;
            bp->len += len;
            bp->buf[bp->len] = '\0';
            return len;
        }
        bp->pos += len;
        return 0;
    }
}

int strsubst(char *buf, int buf_size, const char *from,
             const char *s1, const char *s2)
{
    const char *p, *q;
    buf_t outbuf, *out;

    out = buf_init(&outbuf, buf, buf_size);

    p = from;
    while ((q = strstr(p, s1)) != NULL) {
        buf_write(out, p, q - p);
        buf_puts(out, s2);
        p = q + strlen(s1);
    }
    buf_puts(out, p);

    return out->pos;
}

int strquote(char *dest, int size, const char *str, int len)
{
    if (str) {
        if (len < 0)
            len = strlen(str);
        /* TODO: quote special chars */
        return snprintf(dest, size, "\"%.*s\"", len, str);
    } else {
        return snprintf(dest, size, "null");
    }
}

#if 0
/* TODO */
int strunquote(char *dest, int size, const char *str, int len)
{
}
#endif

/*---------------- allocation routines ----------------*/

void *qe_malloc_bytes(size_t size)
{
    return (malloc)(size);
}

void *qe_mallocz_bytes(size_t size)
{
    void *p = (malloc)(size);
    if (p)
        memset(p, 0, size);
    return p;
}

void *qe_malloc_dup(const void *src, size_t size)
{
    void *p = (malloc)(size);
    if (p)
        memcpy(p, src, size);
    return p;
}

char *qe_strdup(const char *str)
{
    size_t size = strlen(str) + 1;
    char *p = (malloc)(size);

    if (p)
        memcpy(p, str, size);
    return p;
}

void *qe_realloc(void *pp, size_t size)
{
    void *p = (realloc)(*(void **)pp, size);
    if (p || !size)
        *(void **)pp = p;
    return p;
}

/*---------------- bounded strings ----------------*/

/* get the n-th string from a `|` separated list */
bstr_t bstr_get_nth(const char *s, int n) {
    bstr_t bs;

    for (bs.s = s;; s++) {
        if (*s == '\0' || *s == '|') {
            if (n-- == 0) {
                bs.len = s - bs.s;
                break;
            }
            if (*s) {
                bs.s = s + 1;
            } else {
                bs.len = 0;
                bs.s = NULL;
                break;
            }
        }
    }
    return bs;
}

/* get the first string from a list and push pointer */
bstr_t bstr_token(const char *s, int sep, const char **pp) {
    bstr_t bs = { s, 0 };

    if (s) {
        /* XXX: should special case spaces? */
        for (; s != '\0' && *s != sep; s++)
            continue;

        bs.len = s - bs.s;
        if (pp) {
            *pp = *s ? s + 1 : NULL;
        }
    }
    return bs;
}

/*---------------- qe_qsort_r ----------------*/

/* Our own implementation of qsort_r() since it is not available
 * on some targets, such as OpenBSD.
 */

typedef void (*exchange_f)(void *a, void *b, size_t size);

static void exchange_bytes(void *a, void *b, size_t size) {
    unsigned char t;
    unsigned char *ac = (unsigned char *)a;
    unsigned char *bc = (unsigned char *)b;

    while (size-- > 0) {
        t = *ac;
        *ac++ = *bc;
        *bc++ = t;
    }
}

static void exchange_ints(void *a, void *b, size_t size) {
    int *ai = (int *)a;
    int *bi = (int *)b;

    for (size /= sizeof(int); size-- != 0;) {
        int t = *ai;
        *ai++ = *bi;
        *bi++ = t;
    }
}

static void exchange_one_int(void *a, void *b, size_t size) {
    int *ai = (int *)a;
    int *bi = (int *)b;
    int t = *ai;
    *ai = *bi;
    *bi = t;
}

#if LONG_MAX != INT_MAX
static void exchange_longs(void *a, void *b, size_t size) {
    long *ai = (long *)a;
    long *bi = (long *)b;

    for (size /= sizeof(long); size-- != 0;) {
        long t = *ai;
        *ai++ = *bi;
        *bi++ = t;
    }
}

static void exchange_one_long(void *a, void *b, size_t size) {
    long *ai = (long *)a;
    long *bi = (long *)b;
    long t = *ai;
    *ai = *bi;
    *bi = t;
}
#endif

static inline exchange_f exchange_func(void *base, size_t size) {
    exchange_f exchange = exchange_bytes;
#if LONG_MAX != INT_MAX
    if ((((uintptr_t)base | (uintptr_t)size) & (sizeof(long) - 1)) == 0) {
        exchange = exchange_longs;
        if (size == sizeof(long))
            exchange = exchange_one_long;
    } else
#endif
    if ((((uintptr_t)base | (uintptr_t)size) & (sizeof(int) - 1)) == 0) {
        exchange = exchange_ints;
        if (size == sizeof(int))
            exchange = exchange_one_int;
    }
    return exchange;
}

static inline void *med3_r(void *a, void *b, void *c, void *thunk,
                           int (*compare)(void *, const void *, const void *))
{
    return compare(thunk, a, b) < 0 ?
        (compare(thunk, b, c) < 0 ? b : (compare(thunk, a, c) < 0 ? c : a )) :
        (compare(thunk, b, c) > 0 ? b : (compare(thunk, a, c) < 0 ? a : c ));
}

#define MAXSTACK 64

void qe_qsort_r(void *base, size_t nmemb, size_t size, void *thunk,
                int (*compare)(void *, const void *, const void *))
{
    struct {
        unsigned char *base;
        size_t count;
    } stack[MAXSTACK], *sp;
    size_t m0, n;
    unsigned char *lb, *m, *i, *j;
    exchange_f exchange = exchange_func(base, size);

    if (nmemb < 2 || size <= 0)
        return;

    sp = stack;
    sp->base = base;
    sp->count = nmemb;
    sp++;
    while (sp-- > stack) {
        lb = sp->base;
        n = sp->count;

        while (n >= 7) {
            /* partition into two segments */
            i = lb + size;
            j = lb + (n - 1) * size;
            /* select pivot and exchange with 1st element */
            m0 = (n >> 2) * size;
            /* should use median of 3 or 9 */
            m = med3_r(lb + m0, lb + 2 * m0, lb + 3 * m0, thunk, compare);

            exchange(lb, m, size);

            m0 = n - 1;  /* m is the offset of j */
            for (;;) {
                while (i < j && compare(thunk, lb, i) > 0) {
                    i += size;
                }
                while (j >= i && compare(thunk, j, lb) > 0) {
                    j -= size;
                    m0--;
                }
                if (i >= j)
                    break;
                exchange(i, j, size);
                i += size;
                j -= size;
                m0--;
            }

            /* pivot belongs in A[j] */
            exchange(lb, j, size);

            /* keep processing smallest segment,
            * and stack largest */
            n = n - m0 - 1;
            if (m0 < n) {
                if (n > 1) {
                    sp->base = j + size;
                    sp->count = n;
                    sp++;
                }
                n = m0;
            } else {
                if (m0 > 1) {
                    sp->base = lb;
                    sp->count = m0;
                    sp++;
                }
                lb = j + size;
            }
        }
        /* Use insertion sort for small fragments */
        for (i = lb + size, j = lb + n * size; i < j; i += size) {
			for (m = i; m > lb && compare(thunk, m - size, m) > 0; m -= size)
                exchange(m, m - size, size);
        }
    }
}
