/*
 * QEmacs, tiny but powerful multimode editor
 *
 * Copyright (c) 2000-2001 Fabrice Bellard.
 * Copyright (c) 2000-2019 Charlie Gordon.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef QE_H
#define QE_H

#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <errno.h>
#include <limits.h>
#include <inttypes.h>
#include <time.h>

#ifdef HAVE_QE_CONFIG_H
#include "config.h"
#endif

/* OS specific defines */

#ifdef CONFIG_WIN32
#define snprintf   _snprintf
#define vsnprintf  _vsnprintf
#endif

#if (defined(__GNUC__) || defined(__TINYC__))
/* make sure that the keyword is not disabled by glibc (TINYC case) */
#define qe__attr_printf(a, b)  __attribute__((format(printf, a, b)))
#else
#define qe__attr_printf(a, b)
#endif

#if defined(__GNUC__) && __GNUC__ > 2
#define qe__attr_nonnull(l)   __attribute__((nonnull l))
#define qe__unused__          __attribute__((unused))
#else
#define qe__attr_nonnull(l)
#define qe__unused__
#endif

#ifndef offsetof
#define offsetof(s,m)  ((size_t)(&((s *)0)->m))
#endif
#ifndef countof
#define countof(a)  ((int)(sizeof(a) / sizeof((a)[0])))
#endif
#ifndef ssizeof
#define ssizeof(a)  ((int)(sizeof(a)))
#endif

#define OWNED     /* ptr attribute for allocated data owned by a structure */

/* prevent gcc warning about shadowing a global declaration */
#define index  index__

/************************/

#include "cutils.h"

/************************/

/* allocation wrappers and utilities */
void *qe_malloc_bytes(size_t size);
void *qe_mallocz_bytes(size_t size);
void *qe_malloc_dup(const void *src, size_t size);
char *qe_strdup(const char *str);
void *qe_realloc(void *pp, size_t size);
#define qe_malloc(t)            ((t *)qe_malloc_bytes(sizeof(t)))
#define qe_mallocz(t)           ((t *)qe_mallocz_bytes(sizeof(t)))
#define qe_malloc_array(t, n)   ((t *)qe_malloc_bytes((n) * sizeof(t)))
#define qe_mallocz_array(t, n)  ((t *)qe_mallocz_bytes((n) * sizeof(t)))
#define qe_malloc_hack(t, n)    ((t *)qe_malloc_bytes(sizeof(t) + (n)))
#define qe_mallocz_hack(t, n)   ((t *)qe_mallocz_bytes(sizeof(t) + (n)))
#ifdef CONFIG_HAS_TYPEOF
#define qe_free(pp)    do { typeof(**(pp)) **__ = (pp); (free)(*__); *__ = NULL; } while (0)
#else
#define qe_free(pp)    \
    do if (sizeof(**(pp)) >= 0) { void *_ = (pp); (free)(*(void **)_); *(void **)_ = NULL; } while (0)
#endif

#ifndef free
#define free(p)       do_not_use_free!!(p)
#endif
#ifndef malloc
#define malloc(s)     do_not_use_malloc!!(s)
#endif
#ifndef realloc
#define realloc(p,s)  do_not_use_realloc!!(p,s)
#endif

/************************/

typedef unsigned char u8;
typedef struct EditState EditState;
typedef struct EditBuffer EditBuffer;
typedef struct QEmacsState QEmacsState;
typedef struct DisplayState DisplayState;
typedef struct ModeProbeData ModeProbeData;
typedef struct QEModeData QEModeData;
typedef struct ModeDef ModeDef;
typedef struct QETimer QETimer;
typedef struct QEColorizeContext QEColorizeContext;
typedef struct KeyDef KeyDef;
typedef struct InputMethod InputMethod;
typedef struct ISearchState ISearchState;
typedef struct QEProperty QEProperty;

static inline char *s8(u8 *p) { return (char*)p; }
static inline const char *cs8(const u8 *p) { return (const char*)p; }

#ifndef INT_MAX
#define INT_MAX  0x7fffffff
#endif
#ifndef INT_MIN
#define INT_MIN  (-0x7fffffff-1)
#endif
#define NO_ARG  INT_MIN
#define MAX_FILENAME_SIZE    1024       /* Size for a filename buffer */
#define MAX_BUFFERNAME_SIZE  256        /* Size for a buffer name buffer */
#define MAX_CMDNAME_SIZE     32         /* Size for a command name buffer */

extern const char str_version[];
extern const char str_credits[];

/* low level I/O events */
void set_read_handler(int fd, void (*cb)(void *opaque), void *opaque);
void set_write_handler(int fd, void (*cb)(void *opaque), void *opaque);
int set_pid_handler(int pid,
                    void (*cb)(void *opaque, int status), void *opaque);
void url_exit(void);
void url_redisplay(void);
void register_bottom_half(void (*cb)(void *opaque), void *opaque);
void unregister_bottom_half(void (*cb)(void *opaque), void *opaque);

QETimer *qe_add_timer(int delay, void *opaque, void (*cb)(void *opaque));
void qe_kill_timer(QETimer **tip);

/* main loop for Unix programs using liburlio */
void url_main_loop(void (*init)(void *opaque), void *opaque);

/* util.c */

/* string arrays */
typedef struct StringItem {
    void *opaque;  /* opaque data that the user can use */
    char selected; /* true if selected */
    char group;    /* used to group sorted items */
    char str[1];
} StringItem;

typedef struct StringArray {
    int nb_allocated;
    int nb_items;
    StringItem **items;
} StringArray;
#define NULL_STRINGARRAY  { 0, 0, NULL }

typedef struct CompleteState {
    StringArray cs;
    struct EditState *s;
    struct EditState *target;
    int len;
    char current[MAX_FILENAME_SIZE];
} CompleteState;

/* media definitions */
#define CSS_MEDIA_TTY     0x0001
#define CSS_MEDIA_SCREEN  0x0002
#define CSS_MEDIA_PRINT   0x0004
#define CSS_MEDIA_TV      0x0008
#define CSS_MEDIA_SPEECH  0x0010

#define CSS_MEDIA_ALL     0xffff

typedef struct CSSRect {
    int x1, y1, x2, y2;
} CSSRect;

typedef unsigned int QEColor;
#define QEARGB(a,r,g,b)    (((a) << 24) | ((r) << 16) | ((g) << 8) | (b))
#define QERGB(r,g,b)       QEARGB(0xff, r, g, b)
#define QERGB25(r,g,b)     QEARGB(1, r, g, b)
#define COLOR_TRANSPARENT  0
#define QERGB_ALPHA(c)     (((c) >> 24) & 255)
#define QERGB_RED(c)       (((c) >> 16) & 255)
#define QERGB_GREEN(c)     (((c) >>  8) & 255)
#define QERGB_BLUE(c)      (((c) >>  0) & 255)

/* A qemacs style is a named set of attributes including:
 * - colors for foreground and background
 * - font style bits
 * - font style size
 *
 * Styles are applied to text in successive phases:
 * - the current syntax mode computes style numbers for each character
 *   on the line. These nubers are stored into the high bits of the
 *   32-bit code point.
 * - these style numbers are then extracted into an array of 32-bit composite
 *   style values.
 * - the styles from the optional style buffer are combined into these style
 *   values.
 * - search matches and selection styles are applied if relevant.
 * - the bidirectional algorithm is applied to compute the display order
 * - sequences of code-point with the same compsite style are formed into
 *   display units, ligatures are applied.
 * - the composite style is expanded and converted into display attributes
 *   to trace the display unit on the device surface
 */

/* Style numbers are limited to 8 bits, the default set has 27 entries */
/* Composite styles are 32-bit values that specify
 * - a style number
 * - display attributes for underline, bold, blink
 * - text and background colors as either palette numbers or 4096 rgb values
 */

#if 0   /* 25-bit color for FG and BG */

#define QE_TERM_STYLE_BITS  64
typedef uint64_t QETermStyle;
#define QE_STYLE_NUM        0x00FF
#define QE_STYLE_SEL        0x02000000  /* special selection style (cumulative with another style) */
#define QE_TERM_COMPOSITE   0x04000000  /* special bit to indicate qe-term composite style */
/* XXX: reversed as attribute? */
/* XXX: faint? */
#define QE_TERM_UNDERLINE   0x08000000
#define QE_TERM_BOLD        0x10000000
#define QE_TERM_ITALIC      0x20000000
#define QE_TERM_BLINK       0x40000000
#define QE_TERM_BG_BITS     25
#define QE_TERM_BG_SHIFT    32
#define QE_TERM_FG_BITS     25
#define QE_TERM_FG_SHIFT    0

#elif 1   /* 8K colors for FG and BG */

#define QE_TERM_STYLE_BITS  32
typedef uint32_t QETermStyle;
#define QE_STYLE_NUM        0x00FF
#define QE_STYLE_SEL        0x02000  /* special selection style (cumulative with another style) */
#define QE_TERM_COMPOSITE   0x04000  /* special bit to indicate qe-term composite style */
#define QE_TERM_UNDERLINE   0x08000
#define QE_TERM_BOLD        0x10000
#define QE_TERM_ITALIC      0x20000
#define QE_TERM_BLINK       0x40000
#define QE_TERM_BG_BITS     13
#define QE_TERM_BG_SHIFT    19
#define QE_TERM_FG_BITS     13
#define QE_TERM_FG_SHIFT    0

#elif 1   /* 256 colors for FG and BG */

#define QE_TERM_STYLE_BITS  32
typedef uint32_t QETermStyle;
#define QE_STYLE_NUM        0x00FF
#define QE_STYLE_SEL        0x0100  /* special selection style (cumulative with another style) */
#define QE_TERM_COMPOSITE   0x0200  /* special bit to indicate qe-term composite style */
#define QE_TERM_UNDERLINE   0x0400
#define QE_TERM_BOLD        0x0800
#define QE_TERM_ITALIC      0x1000
#define QE_TERM_BLINK       0x2000
#define QE_TERM_BG_BITS     8
#define QE_TERM_BG_SHIFT    16
#define QE_TERM_FG_BITS     8
#define QE_TERM_FG_SHIFT    0

#else   /* 16 colors for FG and 16 color BG */

#define QE_TERM_STYLE_BITS  16
typedef uint16_t QETermStyle;
#define QE_STYLE_NUM        0x00FF
#define QE_STYLE_SEL        0x0100  /* special selection style (cumulative with another style) */
#define QE_TERM_COMPOSITE   0x0200  /* special bit to indicate qe-term composite style */
#define QE_TERM_UNDERLINE   0x0400
#define QE_TERM_BOLD        0x0800
#define QE_TERM_ITALIC      0x1000
#define QE_TERM_BLINK       0x2000
#define QE_TERM_BG_BITS     4
#define QE_TERM_BG_SHIFT    0
#define QE_TERM_FG_BITS     4
#define QE_TERM_FG_SHIFT    4

#endif

#define QE_TERM_DEF_FG      7
#define QE_TERM_DEF_BG      0
#define QE_TERM_BG_COLORS   (1 << QE_TERM_BG_BITS)
#define QE_TERM_FG_COLORS   (1 << QE_TERM_FG_BITS)
#define QE_TERM_BG_MASK     ((QETermStyle)(QE_TERM_BG_COLORS - 1) << QE_TERM_BG_SHIFT)
#define QE_TERM_FG_MASK     ((QETermStyle)(QE_TERM_FG_COLORS - 1) << QE_TERM_FG_SHIFT)
#define QE_TERM_MAKE_COLOR(fg, bg)  (((QETermStyle)(fg) << QE_TERM_FG_SHIFT) | ((QETermStyle)(bg) << QE_TERM_BG_SHIFT))
#define QE_TERM_SET_FG(col, fg)  ((col) = ((col) & ~QE_TERM_FG_MASK) | ((QETermStyle)(fg) << QE_TERM_FG_SHIFT))
#define QE_TERM_SET_BG(col, bg)  ((col) = ((col) & ~QE_TERM_BG_MASK) | ((QETermStyle)(bg) << QE_TERM_BG_SHIFT))
#define QE_TERM_GET_FG(color)  (((color) & QE_TERM_FG_MASK) >> QE_TERM_FG_SHIFT)
#define QE_TERM_GET_BG(color)  (((color) & QE_TERM_BG_MASK) >> QE_TERM_BG_SHIFT)

typedef struct FindFileState FindFileState;

FindFileState *find_file_open(const char *path, const char *pattern);
int find_file_next(FindFileState *s, char *filename, int filename_size_max);
void find_file_close(FindFileState **sp);
int is_directory(const char *path);
int is_filepattern(const char *filespec);
void canonicalize_path(char *buf, int buf_size, const char *path);
void canonicalize_absolute_path(EditState *s, char *buf, int buf_size, const char *path1);
void canonicalize_absolute_buffer_path(EditBuffer *b, int offset, 
                                       char *buf, int buf_size, 
                                       const char *path1);
char *make_user_path(char *buf, int buf_size, const char *path);
char *reduce_filename(char *dest, int size, const char *filename);
int match_extension(const char *filename, const char *extlist);
int match_shell_handler(const char *p, const char *list);
int remove_slash(char *buf);
int append_slash(char *buf, int buf_size);
char *makepath(char *buf, int buf_size, const char *path, const char *filename);
void splitpath(char *dirname, int dirname_size,
               char *filename, int filename_size, const char *pathname);

extern unsigned char const qe_digit_value__[128];
static inline int qe_digit_value(int c) {
    return (unsigned int)c < 128 ? qe_digit_value__[c] : 255;
}
static inline int qe_inrange(int c, int a, int b) {
    return (unsigned int)(c - a) <= (unsigned int)(b - a);
}
/* character classification tests assume ASCII superset */
static inline int qe_isspace(int c) {
    /* CG: what about \v and \f */
    return (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == 160);
}
static inline int qe_isblank(int c) {
    return (c == ' ' || c == '\t' || c == 160);
}
static inline int qe_isdigit(int c) {
    return qe_inrange(c, '0', '9');
}
static inline int qe_isdigit_(int c) {
    return (qe_inrange(c, '0', '9') || c == '_');
}
static inline int qe_isupper(int c) {
    return qe_inrange(c, 'A', 'Z');
}
static inline int qe_isupper_(int c) {
    return (qe_inrange(c, 'A', 'Z') || c == '_');
}
static inline int qe_islower(int c) {
    return qe_inrange(c, 'a', 'z');
}
static inline int qe_islower_(int c) {
    return (qe_inrange(c, 'a', 'z') || (c == '_'));
}
static inline int qe_isalpha(int c) {
    return qe_inrange(c | ('a' - 'A'), 'a', 'z');
}
static inline int qe_isalpha_(int c) {
    return (qe_inrange(c | ('a' - 'A'), 'a', 'z') || c == '_');
}
static inline int qe_isoctdigit(int c) {
    return qe_inrange(c, '0', '7');
}
static inline int qe_isoctdigit_(int c) {
    return qe_inrange(c, '0', '7') || (c == '_');
}
static inline int qe_isbindigit(int c) {
    return qe_inrange(c, '0', '1');
}
static inline int qe_isbindigit_(int c) {
    return qe_inrange(c, '0', '1') || (c == '_');
}
static inline int qe_isxdigit(int c) {
    return qe_digit_value(c) < 16;
}
static inline int qe_isxdigit_(int c) {
    return (qe_digit_value(c) < 16) || (c == '_');
}
static inline int qe_isalnum(int c) {
    return qe_digit_value(c) < 36;
}
static inline int qe_isalnum_(int c) {
    return (qe_digit_value(c) < 36) || (c == '_');
}
static inline int qe_isword(int c) {
    /* XXX: any unicode char >= 128 is considered as word. */
    return qe_isalnum_(c) || (c >= 128);
}
static inline int qe_toupper(int c) {
    return (qe_inrange(c, 'a', 'z') ? c + 'A' - 'a' : c);
}
static inline int qe_tolower(int c) {
    return (qe_inrange(c, 'A', 'Z') ? c + 'a' - 'A' : c);
}
static inline int qe_findchar(const char *str, int c) {
    return qe_inrange(c, 1, 255) && strchr(str, c) != NULL;
}
static inline int qe_indexof(const char *str, int c) {
    if (qe_inrange(c, 1, 255)) {
        const char *p = strchr(str, c);
        if (p) return p - str;
    }
    return -1;
}

static inline int qe_match2(int c, int c1, int c2) {
    return c == c1 || c == c2;
}

static inline int check_fcall(const unsigned int *str, int i) {
    while (str[i] == ' ')
        i++;
    return str[i] == '(';
}

int ustr_get_identifier(char *buf, int buf_size, int c,
                        const unsigned int *str, int i, int n);
int ustr_get_identifier_lc(char *buf, int buf_size, int c,
                           const unsigned int *str, int i, int n);
int ustr_get_word(char *buf, int buf_size, int c,
                  const unsigned int *str, int i, int n);

int qe_strcollate(const char *s1, const char *s2);
int qe_strtobool(const char *s, int def);
void qe_strtolower(char *buf, int buf_size, const char *str);
void skip_spaces(const char **pp);

static inline int strequal(const char *s1, const char *s2) {
    return !strcmp(s1, s2);
}

int memfind(const char *list, const char *p, int len);
int strfind(const char *list, const char *s);
int strxfind(const char *list, const char *s);
const char *strmem(const char *str, const void *mem, int size);
const void *memstr(const void *buf, int size, const char *str);

#define stristart(str, val, ptr)   qe_stristart(str, val, ptr)
int stristart(const char *str, const char *val, const char **ptr);
int strxstart(const char *str, const char *val, const char **ptr);
int strxcmp(const char *str1, const char *str2);
int ustrstart(const unsigned int *str, const char *val, int *lenp);
int ustristart(const unsigned int *str, const char *val, int *lenp);
const unsigned int *ustrstr(const unsigned int *str, const char *val);
const unsigned int *ustristr(const unsigned int *str, const char *val);
static inline unsigned int *umemmove(unsigned int *dest,
                                     const unsigned int *src, int count) {
    return (unsigned int *)memmove(dest, src, count * sizeof(unsigned int));
}
static inline unsigned int *umemcpy(unsigned int *dest,
                                    const unsigned int *src, int count) {
    return (unsigned int *)memcpy(dest, src, count * sizeof(unsigned int));
}
int umemcmp(const unsigned int *s1, const unsigned int *s2, int count);
int qe_memicmp(const void *p1, const void *p2, int count);
const char *qe_stristr(const char *s1, const char *s2);

int strsubst(char *buf, int buf_size, const char *from,
             const char *s1, const char *s2);
int strquote(char *dest, int size, const char *str, int len);
int strunquote(char *dest, int size, const char *str, int len);

void get_str(const char **pp, char *buf, int buf_size, const char *stop);
int css_get_enum(const char *str, const char *enum_str);

extern QEColor const xterm_colors[];
/* XXX: should have a more generic API with precomputed mapping scales */
/* Convert RGB triplet to a composite color */
unsigned int qe_map_color(QEColor color, QEColor const *colors, int count, int *dist);
/* Convert a composite color to an RGB triplet */
QEColor qe_unmap_color(int color, int count);

void color_completion(CompleteState *cp);
int css_define_color(const char *name, const char *value);
int css_get_color(QEColor *color_ptr, const char *p);
void css_free_colors(void);
int css_get_font_family(const char *str);
void css_union_rect(CSSRect *a, const CSSRect *b);
static inline int css_is_null_rect(const CSSRect *a) {
    return (a->x2 <= a->x1 || a->y2 <= a->y1);
}
static inline void css_set_rect(CSSRect *a, int x1, int y1, int x2, int y2) {
    a->x1 = x1;
    a->y1 = y1;
    a->x2 = x2;
    a->y2 = y2;
}
/* return true if a and b intersect */
static inline int css_is_inter_rect(const CSSRect *a, const CSSRect *b) {
    return (!(a->x2 <= b->x1 ||
              a->x1 >= b->x2 ||
              a->y2 <= b->y1 ||
              a->y1 >= b->y2));
}

int get_clock_ms(void);
int get_clock_usec(void);

/* Various string packages: should unify these but keep API simple */

StringItem *set_string(StringArray *cs, int index, const char *str, int group);
StringItem *add_string(StringArray *cs, const char *str, int group);
void free_strings(StringArray *cs);

/* simple dynamic strings wrappers. The strings are always terminated
   by zero except if they are empty. */
typedef struct QString {
    u8 *data;
    int len; /* string length excluding trailing '\0' */
} QString;

static inline void qstrinit(QString *q) {
    q->data = NULL;
    q->len = 0;
}

static inline void qstrfree(QString *q) {
    qe_free(&q->data);
}

int qmemcat(QString *q, const u8 *data1, int len1);
int qstrcat(QString *q, const char *str);
int qprintf(QString *q, const char *fmt, ...) qe__attr_printf(2,3);

/* Dynamic buffers with static allocation */
typedef struct buf_t buf_t;
struct buf_t {
    char *buf;
    int size, len, pos;
};

static inline buf_t *buf_init(buf_t *bp, char *buf, int size) {
    if (size > 0) {
        bp->buf = buf;
        bp->size = size;
        *buf = '\0';
    } else {
        bp->buf = NULL;
        bp->size = 0;
    }
    bp->len = bp->pos = 0;
    return bp;
}
static inline buf_t *buf_attach(buf_t *bp, char *buf, int size, int pos) {
    /* assuming 0 <= pos < size */
    bp->buf = buf;
    bp->size = size;
    bp->len = bp->pos = pos;
    return bp;
}
static inline int buf_avail(buf_t *bp) {
    return bp->size - bp->pos - 1;
}
static inline int buf_put_byte(buf_t *bp, int c) {
    if (bp->len < bp->size - 1) {
        bp->buf[bp->len++] = c;
        bp->buf[bp->len] = '\0';
    }
    return bp->pos++;
}
int buf_write(buf_t *bp, const void *src, int size);
static inline int buf_puts(buf_t *bp, const char *str) {
    return buf_write(bp, str, strlen(str));
}

int buf_printf(buf_t *bp, const char *fmt, ...) qe__attr_printf(2,3);
int buf_putc_utf8(buf_t *bp, int c);

/* Bounded constant strings used in various parse functions */
typedef struct bstr_t {
    const char *s;
    int len;
} bstr_t;

static inline bstr_t bstr_make(const char *s) {
    bstr_t bs = { s, s ? strlen(s) : 0 };
    return bs;
}

bstr_t bstr_token(const char *s, int sep, const char **pp);
bstr_t bstr_get_nth(const char *s, int n);

static inline int bstr_equal(bstr_t s1, bstr_t s2) {
    /* NULL and empty strings are equivalent */
    return s1.len == s2.len && !memcmp(s1.s, s2.s, s1.len);
}

/* our own implementation of qsort_r() */
void qe_qsort_r(void *base, size_t nmemb, size_t size, void *thunk,
                int (*compar)(void *, const void *, const void *));

/* Command line options */
enum CmdLineOptionType {
    CMD_LINE_TYPE_NONE   = 0,  /* nothing */
    CMD_LINE_TYPE_BOOL   = 1,  /* boolean ptr */
    CMD_LINE_TYPE_INT    = 2,  /* int ptr */
    CMD_LINE_TYPE_STRING = 3,  /* string ptr */
    CMD_LINE_TYPE_FVOID  = 4,  /* function() */
    CMD_LINE_TYPE_FARG   = 5,  /* function(string) */
    CMD_LINE_TYPE_NEXT   = 6,  /* next pointer */
};

typedef struct CmdLineOptionDef {
    const char *desc;
    enum CmdLineOptionType type;
    union {
        int *int_ptr;
        const char **string_ptr;
        void (*func_noarg)(void);
        void (*func_arg)(const char *);
        struct CmdLineOptionDef *next;
    } u;
} CmdLineOptionDef;

#define CMD_LINE_NONE()          { NULL, CMD_LINE_TYPE_NONE, { NULL }}
#define CMD_LINE_BOOL(s,n,p,h)   { s "|" n "||" h, CMD_LINE_TYPE_BOOL, { .int_ptr = p }}
#define CMD_LINE_INT(s,n,a,p,h)  { s "|" n "|" a "|" h, CMD_LINE_TYPE_INT, { .int_ptr = p }}
#define CMD_LINE_STRING(s,n,a,p,h) { s "|" n "|" a "|" h, CMD_LINE_TYPE_STRING, { .string_ptr = p }}
#define CMD_LINE_FVOID(s,n,p,h)  { s "|" n "||" h, CMD_LINE_TYPE_FVOID, { .func_noarg = p }}
#define CMD_LINE_FARG(s,n,a,p,h) { s "|" n "|" a "|" h, CMD_LINE_TYPE_FARG, { .func_arg = p }}
#define CMD_LINE_LINK()          { NULL, CMD_LINE_TYPE_NEXT, { NULL }}

void qe_register_cmd_line_options(CmdLineOptionDef *table);

int find_resource_file(char *path, int path_size, const char *pattern);

typedef int (CSSAbortFunc)(void *);

static inline int max(int a, int b) {
    if (a > b)
        return a;
    else
        return b;
}

static inline int maxp(int *pa, int b) {
    int a = *pa;
    if (a > b)
        return a;
    else
        return *pa = b;
}

static inline int max3(int a, int b, int c) {
    return max(max(a, b), c);
}

static inline int min(int a, int b) {
    if (a < b)
        return a;
    else
        return b;
}

static inline int minp(int *pa, int b) {
    int a = *pa;
    if (a < b)
        return a;
    else
        return *pa = b;
}

static inline int min3(int a, int b, int c) {
    return min(min(a, b), c);
}

static inline int clamp(int a, int b, int c) {
    if (a < b)
        return b;
    else
    if (a > c)
        return c;
    else
        return a;
}

static inline int clampp(int *pa, int b, int c) {
    int a = *pa;
    if (a < b)
        return *pa = b;
    else
    if (a > c)
        return *pa = c;
    else
        return a;
}

static inline int compute_percent(int a, int b) {
    return b <= 0 ? 0 : (int)((long long)a * 100 / b);
}

int compose_keys(unsigned int *keys, int *nb_keys);
int strtokey(const char **pp);
int strtokeys(const char *keystr, unsigned int *keys, int max_keys);
int buf_put_key(buf_t *out, int key);
int buf_put_keys(buf_t *out, unsigned int *keys, int nb_keys);

/* charset.c */

/* maximum number of bytes for a character in all the supported charsets */
#define MAX_CHAR_BYTES 6

typedef struct CharsetDecodeState CharsetDecodeState;
#if defined(__cplusplus)
typedef struct QECharset QECharset;
#else
typedef const struct QECharset QECharset;
#endif

struct QECharset {
    const char *name;
    const char *aliases;
    int (*probe_func)(QECharset *charset, const u8 *buf, int size);
    void (*decode_init)(CharsetDecodeState *s);
    int (*decode_func)(CharsetDecodeState *s);
    /* return NULL if cannot encode. Currently no state since speed is
       not critical yet */
    u8 *(*encode_func)(QECharset *charset, u8 *buf, int size);
    void (*get_pos_func)(CharsetDecodeState *s, const u8 *buf, int size,
                         int *line_ptr, int *col_ptr);
    int (*get_chars_func)(CharsetDecodeState *s, const u8 *buf, int size);
    int (*goto_char_func)(CharsetDecodeState *s, const u8 *buf, int size, int pos);
    int (*goto_line_func)(CharsetDecodeState *s, const u8 *buf, int size, int lines);
    unsigned int char_size : 3;
    unsigned int variable_size : 1;
    unsigned int table_alloc : 1; /* true if CharsetDecodeState.table must be malloced */
    /* private data for some charsets */
    u8 eol_char; /* 0x0A for ASCII, 0x25 for EBCDIC */
    u8 min_char, max_char;
    const unsigned short *encode_table;
    const unsigned short *private_table;
    struct QECharset *next;
};

extern struct QECharset *first_charset;
/* predefined charsets */
extern struct QECharset charset_raw;
extern struct QECharset charset_8859_1;
extern struct QECharset charset_utf8;
extern struct QECharset charset_vt100; /* used for the tty output */
extern struct QECharset charset_mac_roman;
extern struct QECharset charset_ucs2le, charset_ucs2be;
extern struct QECharset charset_ucs4le, charset_ucs4be;

typedef enum EOLType {
    EOL_UNIX = 0,
    EOL_DOS,
    EOL_MAC,
} EOLType;

struct CharsetDecodeState {
    /* 256 ushort table for hyper fast decoding */
    const unsigned short *table;
    int char_size;
    EOLType eol_type;
    int eol_char;
    const u8 *p;
    /* slower decode function for complicated cases */
    int (*decode_func)(CharsetDecodeState *s);
    void (*get_pos_func)(CharsetDecodeState *s, const u8 *buf, int size,
                         int *line_ptr, int *col_ptr);
    QECharset *charset;
};

#define INVALID_CHAR 0xfffd
#define ESCAPE_CHAR  0xffff

void charset_init(void);
int charset_more_init(void);
int charset_jis_init(void);

void qe_register_charset(struct QECharset *charset);

extern unsigned char const utf8_length[256];
static inline int utf8_is_trailing_byte(int c) { return (c & 0xC0) == 0x80; }
int utf8_encode(char *q, int c);
int utf8_decode(const char **pp);
char *utf8_char_to_string(char *buf, int c);
int utf8_to_unicode(unsigned int *dest, int dest_length, const char *str);

void charset_completion(CompleteState *cp);
QECharset *find_charset(const char *str);
void charset_decode_init(CharsetDecodeState *s, QECharset *charset,
                         EOLType eol_type);
void charset_decode_close(CharsetDecodeState *s);
void charset_get_pos_8bit(CharsetDecodeState *s, const u8 *buf, int size,
                          int *line_ptr, int *col_ptr);
int charset_get_chars_8bit(CharsetDecodeState *s, const u8 *buf, int size);
int charset_goto_char_8bit(CharsetDecodeState *s, const u8 *buf, int size, int pos);
int charset_goto_line_8bit(CharsetDecodeState *s, const u8 *buf, int size, int nlines);

QECharset *detect_charset(const u8 *buf, int size, EOLType *eol_typep);

void decode_8bit_init(CharsetDecodeState *s);
int decode_8bit(CharsetDecodeState *s);
u8 *encode_8bit(QECharset *charset, u8 *q, int c);

int unicode_tty_glyph_width(unsigned int ucs);

static inline int qe_isaccent(int c) {
    return c >= 0x300 && unicode_tty_glyph_width(c) == 0;
}

/* arabic.c */
int arab_join(unsigned int *line, unsigned int *ctog, int len);

/* indic.c */
int devanagari_log2vis(unsigned int *str, unsigned int *ctog, int len);

/* unicode_join.c */
int unicode_to_glyphs(unsigned int *dst, unsigned int *char_to_glyph_pos,
                      int dst_size, unsigned int *src, int src_size,
                      int reverse);
int combine_accent(unsigned int *buf, int c, int accent);
int expand_ligature(unsigned int *buf, int c);
int load_ligatures(void);
void unload_ligatures(void);

/* qe event handling */

enum QEEventType {
    QE_KEY_EVENT,
    QE_EXPOSE_EVENT, /* full redraw */
    QE_UPDATE_EVENT, /* update content */
    QE_BUTTON_PRESS_EVENT, /* mouse button press event */
    QE_BUTTON_RELEASE_EVENT, /* mouse button release event */
    QE_MOTION_EVENT, /* mouse motion event */
    QE_SELECTION_CLEAR_EVENT, /* request selection clear (X11 type selection) */
};

#define KEY_CTRL(c)     ((c) & 0x001f)
#define KEY_META(c)     ((c) | 0xe000)
#define KEY_ESC1(c)     ((c) | 0xe100)
#define KEY_CTRLX(c)    ((c) | 0xe200)
#define KEY_CTRLXRET(c) ((c) | 0xe300)
#define KEY_CTRLH(c)    ((c) | 0xe500)
#define KEY_CTRLC(c)    ((c) | 0xe600)
#define KEY_IS_SPECIAL(c)  ((c) >= 0xe000 && (c) < 0xf000)
#define KEY_IS_CONTROL(c)  (((c) >= 0 && (c) < 32) || (c) == 127)

#define KEY_NONE        0xffff
#define KEY_DEFAULT     0xe401 /* to handle all non special keys */

#define KEY_TAB         KEY_CTRL('i')
#define KEY_RET         KEY_CTRL('m')
#define KEY_ESC         KEY_CTRL('[')
#define KEY_SPC         0x0020
#define KEY_DEL         127             // kbs
#define KEY_BS          KEY_CTRL('h')   // kbs

#define KEY_UP          KEY_ESC1('A')   // kcuu1
#define KEY_DOWN        KEY_ESC1('B')   // kcud1
#define KEY_RIGHT       KEY_ESC1('C')   // kcuf1
#define KEY_LEFT        KEY_ESC1('D')   // kcub1
#define KEY_CTRL_UP     KEY_ESC1('a')
#define KEY_CTRL_DOWN   KEY_ESC1('b')
#define KEY_CTRL_RIGHT  KEY_ESC1('c')
#define KEY_CTRL_LEFT   KEY_ESC1('d')
#define KEY_CTRL_END    KEY_ESC1('f')
#define KEY_CTRL_HOME   KEY_ESC1('h')
#define KEY_CTRL_PAGEUP KEY_ESC1('i')
#define KEY_CTRL_PAGEDOWN KEY_ESC1('j')
#define KEY_SHIFT_TAB   KEY_ESC1('Z')   // kcbt
#define KEY_HOME        KEY_ESC1(1)     // khome
#define KEY_INSERT      KEY_ESC1(2)     // kich1
#define KEY_DELETE      KEY_ESC1(3)     // kdch1
#define KEY_END         KEY_ESC1(4)     // kend
#define KEY_PAGEUP      KEY_ESC1(5)     // kpp
#define KEY_PAGEDOWN    KEY_ESC1(6)     // knp
#define KEY_F1          KEY_ESC1(11)
#define KEY_F2          KEY_ESC1(12)
#define KEY_F3          KEY_ESC1(13)
#define KEY_F4          KEY_ESC1(14)
#define KEY_F5          KEY_ESC1(15)
#define KEY_F6          KEY_ESC1(17)
#define KEY_F7          KEY_ESC1(18)
#define KEY_F8          KEY_ESC1(19)
#define KEY_F9          KEY_ESC1(20)
#define KEY_F10         KEY_ESC1(21)
#define KEY_F11         KEY_ESC1(23)
#define KEY_F12         KEY_ESC1(24)
#define KEY_F13         KEY_ESC1(25)
#define KEY_F14         KEY_ESC1(26)
#define KEY_F15         KEY_ESC1(28)
#define KEY_F16         KEY_ESC1(29)
#define KEY_F17         KEY_ESC1(31)
#define KEY_F18         KEY_ESC1(32)
#define KEY_F19         KEY_ESC1(33)
#define KEY_F20         KEY_ESC1(34)

typedef struct QEKeyEvent {
    enum QEEventType type;
    int key;
} QEKeyEvent;

typedef struct QEExposeEvent {
    enum QEEventType type;
    /* currently, no more info */
} QEExposeEvent;

#define QE_BUTTON_LEFT   0x0001
#define QE_BUTTON_MIDDLE 0x0002
#define QE_BUTTON_RIGHT  0x0004
#define QE_WHEEL_UP      0x0008
#define QE_WHEEL_DOWN    0x0010

/* should probably go somewhere else, or in the config file */
/* how many text lines to scroll when mouse wheel is used */
#define WHEEL_SCROLL_STEP 4

typedef struct QEButtonEvent {
    enum QEEventType type;
    int x;
    int y;
    int button;
} QEButtonEvent;

typedef struct QEMotionEvent {
    enum QEEventType type;
    int x;
    int y;
} QEMotionEvent;

typedef union QEEvent {
    enum QEEventType type;
    QEKeyEvent key_event;
    QEExposeEvent expose_event;
    QEButtonEvent button_event;
    QEMotionEvent motion_event;
} QEEvent;

void qe_handle_event(QEEvent *ev);
/* CG: Should optionally attach grab to a window */
/* CG: Should deal with opaque object life cycle */
void qe_grab_keys(void (*cb)(void *opaque, int key), void *opaque);
void qe_ungrab_keys(void);
KeyDef *qe_find_binding(unsigned int *keys, int nb_keys, KeyDef *kd);
KeyDef *qe_find_current_binding(unsigned int *keys, int nb_keys, ModeDef *m);

#define COLORED_MAX_LINE_SIZE  4096

/* colorize & transform a line, lower level then ColorizeFunc */
/* XXX: should return `len`, the number of valid codepoints copied to 
 * destination excluding the null terminator and newline if present.
 * Truncation can be detected by testing if a newline is present 
 * at this offset.
 */
typedef int (*GetColorizedLineFunc)(EditState *s,
                                    unsigned int *buf, int buf_size,
                                    QETermStyle *sbuf,
                                    int offset, int *offsetp, int line_num);

struct QEColorizeContext {
    EditState *s;
    EditBuffer *b;
    int offset;
    int colorize_state;
    int state_only;
    int combine_start, combine_stop; /* region for combine_static_colorized_line() */
};

/* colorize a line: this function modifies buf to set the char
 * styles. 'buf' is guaranted to have one more '\0' char after its len.
 */
typedef void (*ColorizeFunc)(QEColorizeContext *cp,
                             unsigned int *buf, int n, ModeDef *syn);

/* buffer.c */

/* begin to mmap files from this size */
#define MIN_MMAP_SIZE  (2*1024*1024)
#define MAX_LOAD_SIZE  (512*1024*1024)

#define MAX_PAGE_SIZE  4096
//#define MAX_PAGE_SIZE 16

#define NB_LOGS_MAX     100000  /* need better way to limit undo information */

#define PG_READ_ONLY    0x0001 /* the page is read only */
#define PG_VALID_POS    0x0002 /* set if the nb_lines / col fields are up to date */
#define PG_VALID_CHAR   0x0004 /* nb_chars is valid */
#define PG_VALID_COLORS 0x0008 /* color state is valid (unused) */

typedef struct Page {   /* should pack this */
    int size;     /* data size */
    int flags;
    u8 *data;
    /* the following are needed to handle line / column computation */
    int nb_lines; /* Number of EOL characters in data */
    int col;      /* Number of chars since the last EOL */
    /* the following is needed for char offset computation */
    int nb_chars;
} Page;

#define DIR_LTR 0
#define DIR_RTL 1

typedef int DirType;

enum LogOperation {
    LOGOP_FREE = 0,
    LOGOP_WRITE,
    LOGOP_INSERT,
    LOGOP_DELETE,
};

/* Each buffer modification can be caught with this callback */
typedef void (*EditBufferCallback)(EditBuffer *b, void *opaque, int arg,
                                   enum LogOperation op, int offset, int size);

typedef struct EditBufferCallbackList {
    void *opaque;
    int arg;
    EditBufferCallback callback;
    struct EditBufferCallbackList *next;
} EditBufferCallbackList;

/* high level buffer type handling */
typedef struct EditBufferDataType {
    const char *name; /* name of buffer data type (text, image, ...) */
    int (*buffer_load)(EditBuffer *b, FILE *f);
    int (*buffer_save)(EditBuffer *b, int start, int end, const char *filename);
    void (*buffer_close)(EditBuffer *b);
    struct EditBufferDataType *next;
} EditBufferDataType;

/* buffer flags */
#define BF_SAVELOG   0x0001  /* activate buffer logging */
#define BF_SYSTEM    0x0002  /* buffer system, cannot be seen by the user */
#define BF_READONLY  0x0004  /* read only buffer */
#define BF_PREVIEW   0x0008  /* used in dired mode to mark previewed files */
#define BF_LOADING   0x0010  /* buffer is being loaded */
#define BF_SAVING    0x0020  /* buffer is being saved */
#define BF_DIRED     0x0100  /* buffer is interactive dired */
#define BF_UTF8      0x0200  /* buffer charset is utf-8 */
#define BF_RAW       0x0400  /* buffer charset is raw (no charset translation) */
#define BF_TRANSIENT 0x0800  /* buffer is deleted upon window close */
#define BF_STYLES    0x7000  /* buffer has styles */
#define BF_STYLE1    0x1000  /* buffer has 1 byte styles */
#define BF_STYLE2    0x2000  /* buffer has 2 byte styles */
#define BF_STYLE4    0x3000  /* buffer has 4 byte styles */
#define BF_STYLE8    0x4000  /* buffer has 8 byte styles */
#define BF_IS_STYLE  0x8000  /* buffer is a styles buffer */
#define BF_IS_LOG    0x10000  /* buffer is a log buffer */

struct EditBuffer {
    OWNED Page *page_table;
    int nb_pages;
    int mark;       /* current mark (moved with text) */
    int total_size; /* total size of the buffer */
    int modified;

    /* page cache */
    Page *cur_page;
    int cur_offset;
    int flags;

    /* mmap data, including file handle if kept open */
    void *map_address;
    int map_length;
    int map_handle;

    /* buffer data type (default is raw) */
    ModeDef *data_mode;
    const char *data_type_name;
    EditBufferDataType *data_type;
    void *data_data;    /* associated buffer data, used if data_type != raw_data */

    /* buffer syntax or major mode */
    ModeDef *syntax_mode;
    ColorizeFunc colorize_func; /* line colorization function */
    unsigned short *colorize_states; /* state before line n, one per line */
    int colorize_nb_lines;
    int colorize_nb_valid_lines;
    /* maximum valid offset, INT_MAX if not modified. Needed to
     * invalidate 'colorize_states' */
    int colorize_max_valid_offset;

    /* charset handling */
    CharsetDecodeState charset_state;
    QECharset *charset;
    int char_bytes, char_shift;

    /* undo system */
    int save_log;    /* if true, each buffer operation is logged */
    int log_new_index, log_current;
    enum LogOperation last_log;
    int last_log_char;
    int nb_logs;
    EditBuffer *log_buffer;

    /* style system */
    EditBuffer *b_styles;
    QETermStyle cur_style;  /* current style for buffer writing APIs */
    int style_bytes;  /* 0, 1, 2, 4 or 8 bytes per char */
    int style_shift;  /* 0, 0, 1, 2 or 3 */

    /* modification callbacks */
    OWNED EditBufferCallbackList *first_callback;
    OWNED QEProperty *property_list;

#if 0
    /* asynchronous loading/saving support */
    struct BufferIOState *io_state;
    /* used during loading */
    int probed;
#endif

    ModeDef *default_mode;

    /* Saved window data from the last closed window attached to this buffer.
     * Used to restore mode and position when buffer gets re-attached
     * to the same window.
     */
    ModeDef *saved_mode;
    OWNED u8 *saved_data; /* SAVED_DATA_SIZE bytes */

    /* list of mode data associated with buffer */
    OWNED QEModeData *mode_data_list;

    /* default mode stuff when buffer is detached from window */
    int offset;

    int tab_width;
    int fill_column;
    EOLType eol_type;

    OWNED EditBuffer *next; /* next editbuffer in qe_state buffer list */

    int st_mode;                        /* unix file mode */
    const char name[MAX_BUFFERNAME_SIZE];     /* buffer name */
    const char filename[MAX_FILENAME_SIZE];   /* file name */

    /* Should keep a stat buffer to check for file type and
     * asynchronous modifications
     */
};

/* the log buffer is used for the undo operation */
/* header of log operation */
typedef struct LogBuffer {
    u8 pad1, pad2;    /* for Log buffer readability */
    u8 op;
    u8 was_modified;
    int offset;
    int size;
} LogBuffer;

void eb_trace_bytes(const void *buf, int size, int state);

void eb_init(void);
int eb_read_one_byte(EditBuffer *b, int offset);
int eb_read(EditBuffer *b, int offset, void *buf, int size);
int eb_write(EditBuffer *b, int offset, const void *buf, int size);
int eb_insert_buffer(EditBuffer *dest, int dest_offset,
                     EditBuffer *src, int src_offset,
                     int size);
int eb_insert(EditBuffer *b, int offset, const void *buf, int size);
int eb_delete(EditBuffer *b, int offset, int size);
void eb_replace(EditBuffer *b, int offset, int size,
                const void *buf, int size1);
void eb_free_log_buffer(EditBuffer *b);
EditBuffer *eb_new(const char *name, int flags);
EditBuffer *eb_scratch(const char *name, int flags);
void eb_clear(EditBuffer *b);
void eb_free(EditBuffer **ep);
EditBuffer *eb_find(const char *name);
EditBuffer *eb_find_new(const char *name, int flags);
EditBuffer *eb_find_file(const char *filename);
EditState *eb_find_window(EditBuffer *b, EditState *e);

void eb_set_charset(EditBuffer *b, QECharset *charset, EOLType eol_type);
qe__attr_nonnull((3))
int eb_nextc(EditBuffer *b, int offset, int *next_ptr);
qe__attr_nonnull((3))
int eb_prevc(EditBuffer *b, int offset, int *prev_ptr);
int eb_skip_chars(EditBuffer *b, int offset, int n);
int eb_delete_chars(EditBuffer *b, int offset, int n);
int eb_goto_pos(EditBuffer *b, int line1, int col1);
int eb_get_pos(EditBuffer *b, int *line_ptr, int *col_ptr, int offset);
int eb_goto_char(EditBuffer *b, int pos);
int eb_get_char_offset(EditBuffer *b, int offset);
int eb_delete_range(EditBuffer *b, int p1, int p2);
static inline int eb_at_bol(EditBuffer *b, int offset) {
    return eb_prevc(b, offset, &offset) == '\n';
}
static inline int eb_next(EditBuffer *b, int offset) {
    eb_nextc(b, offset, &offset);
    return offset;
}
static inline int eb_prev(EditBuffer *b, int offset) {
    eb_prevc(b, offset, &offset);
    return offset;
}

//int eb_clip_offset(EditBuffer *b, int offset);
void do_undo(EditState *s);
void do_redo(EditState *s);

int eb_raw_buffer_load1(EditBuffer *b, FILE *f, int offset);
int eb_mmap_buffer(EditBuffer *b, const char *filename);
void eb_munmap_buffer(EditBuffer *b);
int eb_write_buffer(EditBuffer *b, int start, int end, const char *filename);
int eb_save_buffer(EditBuffer *b);

int eb_set_buffer_name(EditBuffer *b, const char *name1);
void eb_set_filename(EditBuffer *b, const char *filename);

int eb_add_callback(EditBuffer *b, EditBufferCallback cb, void *opaque, int arg);
void eb_free_callback(EditBuffer *b, EditBufferCallback cb, void *opaque);
void eb_offset_callback(EditBuffer *b, void *opaque, int edge,
                        enum LogOperation op, int offset, int size);
int eb_create_style_buffer(EditBuffer *b, int flags);
void eb_free_style_buffer(EditBuffer *b);
QETermStyle eb_get_style(EditBuffer *b, int offset);
void eb_set_style(EditBuffer *b, QETermStyle style, enum LogOperation op,
                  int offset, int size);
void eb_style_callback(EditBuffer *b, void *opaque, int arg,
                       enum LogOperation op, int offset, int size);
int eb_delete_uchar(EditBuffer *b, int offset);
int eb_encode_uchar(EditBuffer *b, char *buf, unsigned int c);
int eb_insert_uchar(EditBuffer *b, int offset, int c);
int eb_replace_uchar(EditBuffer *b, int offset, int c);
int eb_insert_uchars(EditBuffer *b, int offset, int c, int n);
static inline int eb_insert_spaces(EditBuffer *b, int offset, int n) {
    return eb_insert_uchars(b, offset, ' ', n);
}

int eb_insert_utf8_buf(EditBuffer *b, int offset, const char *buf, int len);
int eb_insert_u32_buf(EditBuffer *b, int offset, const unsigned int *buf, int len);
int eb_insert_str(EditBuffer *b, int offset, const char *str);
int eb_match_uchar(EditBuffer *b, int offset, int c, int *offsetp);
int eb_match_str(EditBuffer *b, int offset, const char *str, int *offsetp);
int eb_match_istr(EditBuffer *b, int offset, const char *str, int *offsetp);
int eb_vprintf(EditBuffer *b, const char *fmt, va_list ap) qe__attr_printf(2,0);
int eb_printf(EditBuffer *b, const char *fmt, ...) qe__attr_printf(2,3);
int eb_puts(EditBuffer *b, const char *s);
int eb_putc(EditBuffer *b, int c);
void eb_line_pad(EditBuffer *b, int n);
int eb_get_region_content_size(EditBuffer *b, int start, int stop);
static inline int eb_get_content_size(EditBuffer *b) {
    return eb_get_region_content_size(b, 0, b->total_size);
}
int eb_get_region_contents(EditBuffer *b, int start, int stop,
                           char *buf, int buf_size);
static inline int eb_get_contents(EditBuffer *b, char *buf, int buf_size) {
    return eb_get_region_contents(b, 0, b->total_size, buf, buf_size);
}
int eb_insert_buffer_convert(EditBuffer *dest, int dest_offset,
                             EditBuffer *src, int src_offset,
                             int size);
int eb_get_line(EditBuffer *b, unsigned int *buf, int buf_size,
                int offset, int *offset_ptr);
int eb_fgets(EditBuffer *b, char *buf, int buf_size, 
             int offset, int *offset_ptr);
int eb_prev_line(EditBuffer *b, int offset);
int eb_goto_bol(EditBuffer *b, int offset);
int eb_goto_bol2(EditBuffer *b, int offset, int *countp);
int eb_is_blank_line(EditBuffer *b, int offset, int *offset1);
int eb_is_in_indentation(EditBuffer *b, int offset);
int eb_goto_eol(EditBuffer *b, int offset);
int eb_next_line(EditBuffer *b, int offset);

void eb_register_data_type(EditBufferDataType *bdt);
EditBufferDataType *eb_probe_data_type(const char *filename, int st_mode,
                                       uint8_t *buf, int buf_size);
void eb_set_data_type(EditBuffer *b, EditBufferDataType *bdt);
void eb_invalidate_raw_data(EditBuffer *b);
extern EditBufferDataType raw_data_type;

struct QEProperty {
    int offset;
#define QE_PROP_FREE  1
#define QE_PROP_TAG   3
    int type;
    void *data;
    QEProperty *next;
};

void eb_add_property(EditBuffer *b, int offset, int type, void *data);
QEProperty *eb_find_property(EditBuffer *b, int offset, int offset2, int type);
void eb_delete_properties(EditBuffer *b, int offset, int offset2);

/* qe module handling */

#ifdef QE_MODULE

/* dynamic module case */

#define qe_module_init(fn) \
        int qe__module_init(void) { return fn(); }

#define qe_module_exit(fn) \
        void qe__module_exit(void) { fn(); }

#else /* QE_MODULE */

#if (defined(__GNUC__) || defined(__TINYC__)) && defined(CONFIG_INIT_CALLS)
#if __GNUC__ < 3 || (__GNUC__ == 3 && __GNUC_MINOR__ < 3)
/* same method as the linux kernel... */
#define qe__init_call   __attribute__((unused, __section__ (".initcall.init")))
#define qe__exit_call   __attribute__((unused, __section__ (".exitcall.exit")))
#else
#define qe__attr_used   __attribute__((__used__))
#define qe__init_call   qe__attr_used __attribute__((__section__ (".initcall.init")))
#define qe__exit_call   qe__attr_used __attribute__((__section__ (".exitcall.exit")))
#endif

#define qe_module_init(fn) \
        static int (*qe__initcall_##fn)(void) qe__init_call = fn

#define qe_module_exit(fn) \
        static void (*qe__exitcall_##fn)(void) qe__exit_call = fn
#else

#define qe__init_call
#define qe__exit_call

#define qe_module_init(fn) \
        extern int module_ ## fn (void); \
        int module_ ## fn (void) { return fn(); }

#define qe_module_exit(fn)

#endif

#endif /* QE_MODULE */

/* display.c */

#include "display.h"

/* qe.c */

/* contains all the information necessary to uniquely identify a line,
   to avoid displaying it */
typedef struct QELineShadow {
    uint64_t crc;
    int x;
    short y;
    short height;
} QELineShadow;

enum WrapType {
    WRAP_AUTO = 0,
    WRAP_TRUNCATE,
    WRAP_LINE,
    WRAP_TERM,
    WRAP_WORD,
};

#define DIR_LTR 0
#define DIR_RTL 1

struct EditState {
    int offset;     /* offset of the cursor */
    /* text display state */
    int offset_top; /* offset of first character displayed in window */
    int offset_bottom; /* offset of first character beyond window or -1
                        * if end of file displayed */
    int y_disp;    /* virtual position of the displayed text */
    int x_disp[2]; /* position for LTR and RTL text resp. */
    int dump_width;  /* width in binary, hex and unihex modes */
    int hex_mode;    /* true if we are currently editing hexa */
    int unihex_mode; /* true if unihex editing (width of hex char dump) */
    int hex_nibble;  /* current hexa nibble */
    int insert;      /* insert/overtype mode */
    int bidir;
    int cur_rtl;     /* TRUE if the cursor on over RTL chars */
    enum WrapType wrap;
    int wrap_cols;   /* number of columns in terminal emulator */
    int line_numbers;
    /* XXX: these should be buffer specific rather than window specific */
    int indent_size;
    int indent_tabs_mode; /* if true, use tabs to indent */
    int interactive; /* true if interaction is done instead of editing
                        (e.g. for shell mode or HTML) */
    int force_highlight;  /* if true, force showing of cursor even if
                             window not focused (list mode only) */
    int mouse_force_highlight; /* if true, mouse can force highlight
                                  (list mode only) */
    int up_down_last_x; /* last x offset for vertical movement */

    /* low level colorization function */
    GetColorizedLineFunc get_colorized_line;
    ColorizeFunc colorize_func; /* colorization function */

    QETermStyle default_style;  /* default text style */

    /* after this limit, the fields are not saved into the buffer */
    int end_of_saved_data;

    EditBuffer *b;

    EditBuffer *last_buffer;    /* for predict_switch_to_buffer */
    ISearchState *isearch_state;  /* active search to colorize matches */
    EditState *target_window;   /* for minibuf, popleft and popup windows */

    /* mode specific info */
    ModeDef *mode;
    OWNED QEModeData *mode_data; /* mode private window based data */

    /* state before line n, one short per line */
    /* XXX: move this to buffer based mode_data */
    unsigned short *colorize_states;
    int colorize_nb_lines;
    int colorize_nb_valid_lines;
    /* maximum valid offset, INT_MAX if not modified. Needed to invalide
       'colorize_states' */
    int colorize_max_valid_offset;

    int busy; /* true if editing cannot be done if the window
                 (e.g. the parser HTML is parsing the buffer to
                 produce the display */
    int display_invalid; /* true if the display was invalidated. Full
                            redraw should be done */
    int borders_invalid; /* true if window borders should be redrawn */
    int show_selection;  /* if true, the selection is displayed */

    int region_style;
    int curline_style;

    /* display area info */
    int xleft, ytop;
    int width, height;
    int char_width, line_height;
    int cols, rows;
    /* full window size, including borders */
    int x1, y1, x2, y2;         /* window coordinates in device units */
    //int xx1, yy1, xx2, yy2;     /* window coordinates in 1/1000 */

    int flags; /* display flags */
#define WF_POPUP      0x0001 /* popup window (with borders) */
#define WF_MODELINE   0x0002 /* mode line must be displayed */
#define WF_RSEPARATOR 0x0004 /* right window separator */
#define WF_POPLEFT    0x0008 /* left side window */
#define WF_HIDDEN     0x0010 /* hidden window, used for temporary changes */
#define WF_MINIBUF    0x0020 /* true if single line editing */
#define WF_FILELIST   0x1000 /* window is interactive file list */

    OWNED char *prompt;  /* optional window prompt, utf8 */
    OWNED char *caption;  /* optional window caption or title, utf8 */
    //const char *mode_line;
    //const char *title;
    struct QEmacsState *qe_state;
    struct QEditScreen *screen; /* copy of qe_state->screen */
    /* display shadow to optimize redraw */
    char modeline_shadow[MAX_SCREEN_WIDTH];
    OWNED QELineShadow *line_shadow; /* per window shadow CRC data */
    int shadow_nb_lines;
    /* compose state for input method */
    InputMethod *input_method; /* current input method */
    InputMethod *selected_input_method; /* selected input method (used to switch) */
    int compose_len;
    int compose_start_offset;
    unsigned int compose_buf[20];
    OWNED EditState *next_window;
};

/* Ugly patch for saving/restoring window data upon switching buffer */
#define SAVED_DATA_SIZE  offsetof(EditState, end_of_saved_data)

struct ModeProbeData {
    const char *real_filename;
    const char *filename;  /* reduced filename for mode matching purposes */
    const u8 *buf;
    int buf_size;
    int line_len;
    int st_errno;    /* errno from the stat system call */
    int st_mode;     /* unix file mode */
    long total_size;
    EOLType eol_type;
    CharsetDecodeState charset_state;
    QECharset *charset;
    EditBuffer *b;
};

struct QEModeData {
    QEModeData *next;
    ModeDef *mode;
    EditState *s;
    EditBuffer *b;
    /* Other mode specific data follows */
};

struct ModeDef {
    const char *name;           /* pretty name for the mode */
    const char *alt_name;       /* alternate name, for the mode setting cmd */
    const char *extensions;
    const char *shell_handlers;
    const char *keywords;
    const char *types;

    int flags;
#define MODEF_NOCMD        0x8000 /* do not register xxx-mode command automatically */
#define MODEF_VIEW         0x01
#define MODEF_SYNTAX       0x02
#define MODEF_MAJOR        0x04
#define MODEF_DATATYPE     0x10
#define MODEF_SHELLPROC    0x20
#define MODEF_NEWINSTANCE  0x100
    int buffer_instance_size;   /* size of malloced buffer state  */
    int window_instance_size;   /* size of malloced window state */

    /* return the percentage of confidence */
    int (*mode_probe)(ModeDef *m, ModeProbeData *pd);
    int (*mode_init)(EditState *s, EditBuffer *b, int flags);
    void (*mode_close)(EditState *s);
    void (*mode_free)(EditBuffer *b, void *state);

    /* low level display functions (must be NULL to use text related
       functions)*/
    void (*display_hook)(EditState *);
    void (*display)(EditState *);

    /* text related functions */
    int (*display_line)(EditState *, DisplayState *, int);
    int (*backward_offset)(EditState *, int);

    ColorizeFunc colorize_func;
    int colorize_flags;
    int auto_indent;
    int default_wrap;

    /* common functions are defined here */
    /* TODO: Should have single move function with move type and argument */
    void (*move_up_down)(EditState *s, int dir);
    void (*move_left_right)(EditState *s, int dir);
    void (*move_bol)(EditState *s);
    void (*move_eol)(EditState *s);
    void (*move_bof)(EditState *s);
    void (*move_eof)(EditState *s);
    void (*move_word_left_right)(EditState *s, int dir);
    void (*scroll_up_down)(EditState *s, int dir);
    void (*scroll_line_up_down)(EditState *s, int dir);
    void (*mouse_goto)(EditState *s, int x, int y);

    /* Functions to insert and delete contents: */
    void (*write_char)(EditState *s, int c);
    void (*delete_bytes)(EditState *s, int offset, int size);

    EditBufferDataType *data_type; /* native buffer data type (NULL = raw) */
    void (*get_mode_line)(EditState *s, buf_t *out);
    void (*indent_func)(EditState *s, int offset);
    /* Get the current directory for the window, return NULL if none */
    char *(*get_default_path)(EditBuffer *s, int offset, 
                              char *buf, int buf_size);

    /* mode specific key bindings */
    struct KeyDef *first_key;

    ModeDef *fallback;  /* use bindings from fallback mode */

    ModeDef *next;
};

QEModeData *qe_create_buffer_mode_data(EditBuffer *b, ModeDef *m);
void *qe_get_buffer_mode_data(EditBuffer *b, ModeDef *m, EditState *e);
QEModeData *qe_create_window_mode_data(EditState *s, ModeDef *m);
void *qe_get_window_mode_data(EditState *e, ModeDef *m, int status);
void *check_mode_data(void **pp);
int qe_free_mode_data(QEModeData *md);

/* from tty.c */
/* set from command line option to prevent GUI such as X11 */
extern int force_tty;

enum QEStyle {
#define STYLE_DEF(constant, name, fg_color, bg_color, \
                  font_style, font_size) \
                  constant,

#include "qestyles.h"

#undef STYLE_DEF
    QE_STYLE_NB,
};

typedef struct QEStyleDef {
    const char *name;
    /* if any style is 0, then default edit style applies */
    QEColor fg_color, bg_color;
    short font_style;
    short font_size;
} QEStyleDef;

/* CG: Should register styles as well */
extern QEStyleDef qe_styles[QE_STYLE_NB];

/* QEmacs state structure */

#define NB_YANK_BUFFERS 10

typedef struct QErrorContext {
    const char *function;
    const char *filename;
    int lineno;
} QErrorContext;

typedef void (*CmdFunc)(void);

struct QEmacsState {
    QEditScreen *screen;
    //struct QEDisplay *first_dpy;
    struct ModeDef *first_mode;
    struct KeyDef *first_key;
    struct CmdDef *first_cmd;
    struct CompletionEntry *first_completion;
    struct HistoryEntry *first_history;
    //struct QECharset *first_charset;
    //struct QETimer *first_timer;
    struct VarDef *first_variable;
    InputMethod *input_methods;
    EditState *first_window;
    EditState *active_window; /* window in which we edit */
    EditBuffer *first_buffer;
    EditBufferDataType *first_buffer_data_type;
    //EditBuffer *message_buffer;
#ifndef CONFIG_TINY
    EditBuffer **buffer_cache;
    int buffer_cache_size;
    int buffer_cache_len;
#endif
    EditBuffer *trace_buffer;
    int trace_flags;
    int trace_buffer_state;
#define EB_TRACE_TTY      0x01
#define EB_TRACE_SHELL    0x02
#define EB_TRACE_PTY      0x04
#define EB_TRACE_EMULATE  0x08
#define EB_TRACE_COMMAND  0x10
#define EB_TRACE_ALL      0x1F
#define EB_TRACE_FLUSH    0x100

    /* global layout info : DO NOT modify these directly. do_refresh
       does it */
    int status_height;
    int mode_line_height;
    int content_height; /* height excluding status line */
    int width, height;
    int border_width;
    int separator_width;
    /* full screen state */
    int hide_status; /* true if status should be hidden */
    int complete_refresh;
    int is_full_screen;
    /* select display aspect for non-latin1 characters:
     * 0 (auto) -> display as unicode on utf-8 capable ttys and x11
     * 1 (nc) -> display as ? or ?? non character symbols
     * 2 (escape) -> display as \uXXXX escape sequence
     */
    int show_unicode;

    /* commands */
    /* XXX: move these to ec */
    CmdFunc last_cmd_func; /* last executed command function call */
    CmdFunc this_cmd_func; /* current executing command */
    int cmd_start_time;
    /* keyboard macros */
    int defining_macro;
    int executing_macro;
    unsigned short *macro_keys;
    int nb_macro_keys;
    int macro_keys_size;
    int macro_key_index; /* -1 means no macro is being executed */
    int ungot_key;
    /* yank buffers */
    EditBuffer *yank_buffers[NB_YANK_BUFFERS];
    int yank_current;
    int argc;  /* command line arguments */
    char **argv;
    char *tty_charset;
    char res_path[1024];        /* exported as QEPATH */
    char status_shadow[MAX_SCREEN_WIDTH];
    char diag_shadow[MAX_SCREEN_WIDTH];
    QErrorContext ec;
    char system_fonts[NB_FONT_FAMILIES][256];

    ///* global variables */
    int it;             /* last result from expression evaluator */
    //int force_tty;      /* prevent graphics display (X11...) */
    //int no_config;      /* prevent config file eval */
    //int force_refresh;  /* force a complete screen refresh */
    int ignore_spaces;  /* ignore spaces when comparing windows */
    int ignore_comments;  /* ignore comments when comparing windows */
    int hilite_region;  /* hilite the current region when selecting */
    int mmap_threshold; /* minimum file size for mmap */
    int max_load_size;  /* maximum file size for loading in memory */
    int default_tab_width;      /* 8 */
    int default_fill_column;    /* 70 */
    EOLType default_eol_type;  /* EOL_UNIX */
    int flag_split_window_change_focus;
    int emulation_flags;
    int backspace_is_control_h;
    int backup_inhibited;  /* prevent qemacs from backing up files */
    int fuzzy_search;    /* use fuzzy search for completion matcher */
    int c_label_indent;
    const char *user_option;
};

extern QEmacsState qe_state;

/* key bindings definitions */

/* dynamic key binding storage */

#define MAX_KEYS 10

struct KeyDef {
    struct KeyDef *next;
    struct CmdDef *cmd;
    int nb_keys;
    unsigned int keys[1];
};

void unget_key(int key);

/* command definitions */

enum CmdArgType {
    CMD_ARG_INT = 0,
    CMD_ARG_INTVAL,
    CMD_ARG_STRING,
    CMD_ARG_STRINGVAL,
    CMD_ARG_WINDOW,
    CMD_ARG_TYPE_MASK = 0x3f,
    CMD_ARG_USE_KEY = 0x40,
    CMD_ARG_USE_ARGVAL = 0x80,
};

typedef enum CmdSig {
    CMD_void = 0,
    CMD_ES,     /* (ES*) -> void */
    CMD_ESi,    /* (ES*, int) -> void */
    CMD_ESs,    /* (ES*, string) -> void */
    CMD_ESii,   /* (ES*, int, int) -> void */
    CMD_ESsi,   /* (ES*, string, int) -> void */
    CMD_ESss,   /* (ES*, string, string) -> void */
    CMD_ESssi,  /* (ES*, string, string, int) -> void */
    CMD_ESsss,  /* (ES*, string, string, string) -> void */
} CmdSig;

#define MAX_CMD_ARGS 5

typedef union CmdArg {
    EditState *s;
    const char *p;
    int n;
} CmdArg;

typedef union CmdProto {
    void (*func)(void);
    void (*ES)(EditState *);
    void (*ESi)(EditState *, int);
    void (*ESs)(EditState *, const char *);
    void (*ESii)(EditState *, int, int);
    void (*ESsi)(EditState *, const char *, int);
    void (*ESss)(EditState *, const char *, const char *);
    void (*ESssi)(EditState *, const char *, const char *, int);
    void (*ESsss)(EditState *, const char *, const char *, const char *);
    struct CmdDef *next;
} CmdProto;

typedef struct CmdDef {
    unsigned short key;       /* normal key */
    unsigned short alt_key;   /* alternate key */
    const char *name;
    CmdProto action;
    CmdSig sig : 8;
    signed int val : 24;
} CmdDef;

/* new command macros */
#define CMD2(key, key_alt, name, func, sig, args) \
    { key, key_alt, name "\0" args, { .sig = func }, CMD_ ## sig, 0 },
#define CMD3(key, key_alt, name, func, sig, val, args) \
    { key, key_alt, name "\0" args, { .sig = func }, CMD_ ## sig, val },

/* old macros for compatibility */
#define CMD0(key, key_alt, name, func) \
    { key, key_alt, name "\0", { .ES = func }, CMD_ES, 0 },
#define CMD1(key, key_alt, name, func, val) \
    { key, key_alt, name "\0" "v", { .ESi = func }, CMD_ESi, val },
#define CMD_DEF_END \
    { 0, 0, NULL, { NULL }, CMD_void, 0 }

ModeDef *qe_find_mode(const char *name, int flags);
ModeDef *qe_find_mode_filename(const char *filename, int flags);
void qe_register_mode(ModeDef *m, int flags);
void mode_completion(CompleteState *cp);
void qe_register_cmd_table(CmdDef *cmds, ModeDef *m);
int qe_register_binding(int key, const char *cmd_name, ModeDef *m);
CmdDef *qe_find_cmd(const char *cmd_name);
int qe_get_prototype(CmdDef *d, char *buf, int size);

/* text display system */

typedef struct TextFragment {
    unsigned short embedding_level;
    short width;       /* fragment width */
    short ascent;
    short descent;
    QETermStyle style; /* composite style */
    short line_index;  /* index into line_buf */
    short len;         /* number of glyphs */
#if QE_TERM_STYLE_BITS == 16
    short dummy;       /* alignment, must be set for CRC */
#endif
} TextFragment;

#ifdef CONFIG_TINY
#define MAX_UNICODE_DISPLAY  0xFFFF
#else
#define MAX_UNICODE_DISPLAY  0x10FFFF
#endif

#define MAX_WORD_SIZE  128
#define NO_CURSOR      0x7fffffff

#define STYLE_BITS     8
#define STYLE_SHIFT    (32 - STYLE_BITS)
#define CHAR_MASK      ((1 << STYLE_SHIFT) - 1)

struct DisplayState {
    int do_disp;        /* true if real display */
    int width;          /* display window width */
    int height;         /* display window height */
    int eol_width;      /* width of eol char */
    int default_line_height;  /* line height if no chars */
    int space_width;    /* width of space character */
    int tab_width;      /* width of tabulation */
    int x_disp;         /* starting x display */
    int x_start;        /* start_x adjusted for RTL */
    int x_line;         /* updated x position for line */
    int left_gutter;    /* width of the gutter at the left of output */
    int x;              /* current x position */
    int y;              /* current y position */
    int line_num;       /* current text line number */
    int cur_hex_mode;   /* true if current char is in hex mode */
    int hex_mode;       /* hex mode from edit_state, -1 if all chars wanted */
    int line_numbers;   /* display line numbers if enough space */
    void *cursor_opaque;
    int (*cursor_func)(struct DisplayState *,
                       int offset1, int offset2, int line_num,
                       int x, int y, int w, int h, int hex_mode);
    int eod;            /* end of display requested */
    /* if base == RTL, then all x are equivalent to width - x */
    DirType base;
    int embedding_level_max;
    int wrap;
    int eol_reached;
    EditState *edit_state;
    QETermStyle style;   /* current style for display_printf... */

    /* fragment buffers */
    TextFragment fragments[MAX_SCREEN_WIDTH];
    int nb_fragments;
    int last_word_space; /* true if last word was a space */
    int word_index;     /* fragment index of the start of the current word */

    /* line char (in fact glyph) buffer */
    unsigned int line_chars[MAX_SCREEN_WIDTH];
    short line_char_widths[MAX_SCREEN_WIDTH];
    int line_offsets[MAX_SCREEN_WIDTH][2];
    unsigned char line_hex_mode[MAX_SCREEN_WIDTH];
    int line_index;

    /* fragment temporary buffer */
    unsigned int fragment_chars[MAX_WORD_SIZE];
    int fragment_offsets[MAX_WORD_SIZE][2];
    unsigned char fragment_hex_mode[MAX_WORD_SIZE];
    int fragment_index;
    int last_space;
    int last_embedding_level;
    QETermStyle last_style;
};

enum DisplayType {
    DISP_NONE,
    DISP_CURSOR,
    DISP_PRINT,
    DISP_CURSOR_SCREEN,
};

void display_init(DisplayState *s, EditState *e, enum DisplayType do_disp,
                  int (*cursor_func)(DisplayState *,
                                     int offset1, int offset2, int line_num,
                                     int x, int y, int w, int h, int hex_mode),
                  void *cursor_opaque);
void display_close(DisplayState *s);
void display_bol(DisplayState *s);
void display_setcursor(DisplayState *s, DirType dir);
int display_char_bidir(DisplayState *s, int offset1, int offset2,
                       int embedding_level, int ch);
void display_eol(DisplayState *s, int offset1, int offset2);

void display_printf(DisplayState *ds, int offset1, int offset2,
                    const char *fmt, ...) qe__attr_printf(4,5);
void display_printhex(DisplayState *s, int offset1, int offset2,
                      unsigned int h, int n);

static inline int display_char(DisplayState *s, int offset1, int offset2,
                               int ch)
{
    return display_char_bidir(s, offset1, offset2, 0, ch);
}

#define SET_COLOR(str,a,b,style)  set_color((str) + (a), (str) + (b), style)

static inline void set_color(unsigned int *p, unsigned int *to, int style) {
    unsigned int bits = (unsigned int)style << STYLE_SHIFT;
    while (p < to)
        *p++ |= bits;
}

#define SET_COLOR1(str,a,style)  set_color1((str) + (a), style)

static inline void set_color1(unsigned int *p, int style) {
    *p |= (unsigned int)style << STYLE_SHIFT;
}

/* input.c */

#define INPUTMETHOD_NOMATCH   (-1)
#define INPUTMETHOD_MORECHARS (-2)

struct InputMethod {
    const char *name;
    /* input match returns:
       n > 0: number of code points in replacement found for a sequence
       of keystrokes in buf.  number of keystrokes in match was stored
       in '*match_len_ptr'.
       INPUTMETHOD_NOMATCH if no match was found
       INPUTMETHOD_MORECHARS if more chars need to be typed to find
         a suitable completion
     */
    int (*input_match)(int *match_buf, int match_buf_size,
                       int *match_len_ptr, const u8 *data,
                       const unsigned int *buf, int len);
    const u8 *data;
    InputMethod *next;
};

void register_input_method(InputMethod *m);
void do_set_input_method(EditState *s, const char *method);
void do_switch_input_method(EditState *s);
void init_input_methods(void);
int load_input_methods(void);
void unload_input_methods(void);

/* the following will be suppressed */
#define LINE_MAX_SIZE 256

static inline int align(int a, int n) {
    return (a / n) * n;
}

static inline int scale(int a, int b, int c) {
    return (a * b + c / 2) / c;
}

/* minibuffer & status */

void minibuffer_init(void);

extern CmdDef minibuffer_commands[];
extern CmdDef popup_commands[];

typedef void (*CompletionFunc)(CompleteState *cp);

typedef struct CompletionEntry {
    const char *name;
    CompletionFunc completion_func;
    struct CompletionEntry *next;
} CompletionEntry;

void complete_test(CompleteState *cp, const char *str);

void register_completion(const char *name, CompletionFunc completion_func);
void put_status(EditState *s, const char *fmt, ...) qe__attr_printf(2,3);
void put_error(EditState *s, const char *fmt, ...) qe__attr_printf(2,3);
void minibuffer_edit(EditState *e, const char *input, const char *prompt,
                     StringArray *hist, const char *completion_name,
                     void (*cb)(void *opaque, char *buf), void *opaque);
void command_completion(CompleteState *cp);
void file_completion(CompleteState *cp);
void buffer_completion(CompleteState *cp);

#ifdef CONFIG_WIN32
static inline int is_user_input_pending(void) {
    return 0;
}
#else
extern int qe__fast_test_event_poll_flag;
int qe__is_user_input_pending(void);

static inline int is_user_input_pending(void) {
    if (qe__fast_test_event_poll_flag) {
        qe__fast_test_event_poll_flag = 0;
        return qe__is_user_input_pending();
    } else {
        return 0;
    }
}
#endif

/* file loading */
#define LF_KILL_BUFFER    0x01
#define LF_LOAD_RESOURCE  0x02
#define LF_CWD_RELATIVE   0x04
#define LF_SPLIT_WINDOW   0x08
#define LF_NOSELECT       0x10
#define LF_NOWILDCARD     0x20
int qe_load_file(EditState *s, const char *filename, int lflags, int bflags);

/* config file support */
void do_load_config_file(EditState *e, const char *file);
void do_load_qerc(EditState *e, const char *file);
void do_add_resource_path(EditState *s, const char *path);

/* popup / low level window handling */
EditState *show_popup(EditState *s, EditBuffer *b, const char *caption);
int check_read_only(EditState *s);
EditState *insert_window_left(EditBuffer *b, int width, int flags);
EditState *find_window(EditState *s, int key, EditState *def);
EditState *qe_find_target_window(EditState *s, int activate);
void do_find_window(EditState *s, int key);

/* window handling */
void edit_close(EditState **sp);
EditState *edit_new(EditBuffer *b,
                    int x1, int y1, int width, int height, int flags);
EditBuffer *check_buffer(EditBuffer **sp);
EditState *check_window(EditState **sp);
int get_glyph_width(QEditScreen *screen, EditState *s, QETermStyle style, int c);
int get_line_height(QEditScreen *screen, EditState *s, QETermStyle style);
void do_refresh(EditState *s);
// should take direction argument
void do_other_window(EditState *s);
void do_previous_window(EditState *s);
void do_delete_window(EditState *s, int force);
#define SW_STACKED       0
#define SW_SIDE_BY_SIDE  1
EditState *qe_split_window(EditState *s, int prop, int side_by_side);
void do_split_window(EditState *s, int prop, int side_by_side);
void do_create_window(EditState *s, const char *filename, const char *layout);
void qe_save_window_layout(EditState *s, EditBuffer *b);

void edit_display(QEmacsState *qs);
void edit_invalidate(EditState *s, int all);
void display_mode_line(EditState *s);
int edit_set_mode(EditState *s, ModeDef *m);
void qe_set_next_mode(EditState *s, int dir, int status);
void do_set_next_mode(EditState *s, int dir);

/* loading files */
void do_exit_qemacs(EditState *s, int argval);
char *get_default_path(EditBuffer *b, int offset, char *buf, int buf_size);
void do_find_file(EditState *s, const char *filename, int bflags);
void do_load_from_path(EditState *s, const char *filename, int bflags);
void do_find_file_other_window(EditState *s, const char *filename, int bflags);
void do_switch_to_buffer(EditState *s, const char *bufname);
void do_preview_mode(EditState *s, int set);
void do_break(EditState *s);
void do_insert_file(EditState *s, const char *filename);
// should take argument?
void do_save_buffer(EditState *s);
void do_write_file(EditState *s, const char *filename);
void do_write_region(EditState *s, const char *filename);
void isearch_colorize_matches(EditState *s, unsigned int *buf, int len,
                              QETermStyle *sbuf, int offset);
void do_isearch(EditState *s, int dir, int argval);
void do_query_replace(EditState *s, const char *search_str,
                      const char *replace_str);
void do_replace_string(EditState *s, const char *search_str,
                       const char *replace_str, int argval);
void do_search_string(EditState *s, const char *search_str, int dir);
void do_refresh_complete(EditState *s);
void do_kill_buffer(EditState *s, const char *bufname, int force);
void switch_to_buffer(EditState *s, EditBuffer *b);
void qe_kill_buffer(EditBuffer *b);

/* text mode */

extern ModeDef text_mode;

int text_backward_offset(EditState *s, int offset);
int text_display_line(EditState *s, DisplayState *ds, int offset);

void set_colorize_func(EditState *s, ColorizeFunc colorize_func);
int generic_get_colorized_line(EditState *s, unsigned int *buf, int buf_size,
                               QETermStyle *sbuf,
                               int offset, int *offsetp, int line_num);

int do_delete_selection(EditState *s);
void do_char(EditState *s, int key, int argval);
void do_combine_char(EditState *s, int accent);
void do_set_mode(EditState *s, const char *name);
void text_move_left_right_visual(EditState *s, int dir);
void text_move_word_left_right(EditState *s, int dir);
void text_move_up_down(EditState *s, int dir);
void text_scroll_up_down(EditState *s, int dir);
void text_write_char(EditState *s, int key);
void do_return(EditState *s, int move);
void do_backspace(EditState *s, int argval);
void do_delete_char(EditState *s, int argval);
void do_tab(EditState *s, int argval);
EditBuffer *new_yank_buffer(QEmacsState *qs, EditBuffer *base);
void do_append_next_kill(EditState *s);
void do_kill(EditState *s, int p1, int p2, int dir, int keep);
void do_kill_region(EditState *s, int keep);
void do_kill_line(EditState *s, int argval);
void do_kill_beginning_of_line(EditState *s, int argval);
void do_kill_word(EditState *s, int dir);
void text_move_bol(EditState *s);
void text_move_eol(EditState *s);
void text_move_bof(EditState *s);
void text_move_eof(EditState *s);
void word_right(EditState *s, int w);
void word_left(EditState *s, int w);
int qe_get_word(EditState *s, char *buf, int buf_size,
                int offset, int *offset_ptr);
void do_goto(EditState *s, const char *str, int unit);
void do_goto_line(EditState *s, int line, int column);
void do_up_down(EditState *s, int dir);
void do_left_right(EditState *s, int dir);
void text_mouse_goto(EditState *s, int x, int y);
void basic_mode_line(EditState *s, buf_t *out, int c1);
void text_mode_line(EditState *s, buf_t *out);
void do_toggle_full_screen(EditState *s);
void do_toggle_control_h(EditState *s, int set);

/* misc */

void do_set_emulation(EditState *s, const char *name);
void do_start_trace_mode(EditState *s);
void do_set_trace_options(EditState *s, const char *options);
void do_cd(EditState *s, const char *name);
int qe_mode_set_key(ModeDef *m, const char *keystr, const char *cmd_name);
void do_set_key(EditState *s, const char *keystr, const char *cmd_name,
                int local);
//void do_unset_key(EditState *s, const char *keystr, int local);
void do_bof(EditState *s);
void do_eof(EditState *s);
void do_bol(EditState *s);
void do_eol(EditState *s);
void do_word_right(EditState *s, int dir);
void do_mark_region(EditState *s, int mark, int offset);
int eb_next_paragraph(EditBuffer *b, int offset);
int eb_start_paragraph(EditBuffer *b, int offset);
void do_mark_paragraph(EditState *s);
void do_backward_paragraph(EditState *s);
void do_forward_paragraph(EditState *s);
void do_kill_paragraph(EditState *s, int dir);
void do_fill_paragraph(EditState *s);
void do_changecase_word(EditState *s, int up);
void do_changecase_region(EditState *s, int up);
void do_delete_word(EditState *s, int dir);
int cursor_func(DisplayState *ds,
                int offset1, int offset2, int line_num,
                int x, int y, int w, int h, int hex_mode);
// should take argval
void do_scroll_left_right(EditState *s, int dir);
void do_scroll_up_down(EditState *s, int dir);
void perform_scroll_up_down(EditState *s, int h);
void do_center_cursor(EditState *s, int force);
void do_quote(EditState *s, int argval);
void do_overwrite_mode(EditState *s, int argval);
// should take argval
void do_set_mark(EditState *s);
void do_mark_whole_buffer(EditState *s);
void do_yank(EditState *s);
void do_yank_pop(EditState *s);
void do_exchange_point_and_mark(EditState *s);
QECharset *read_charset(EditState *s, const char *charset_str,
                        EOLType *eol_typep);
void do_show_coding_system(EditState *s);
void do_set_auto_coding(EditState *s, int verbose);
void do_set_buffer_file_coding_system(EditState *s, const char *charset_str);
void do_convert_buffer_file_coding_system(EditState *s,
    const char *charset_str);
void do_toggle_bidir(EditState *s);
void do_toggle_line_numbers(EditState *s);
void do_toggle_truncate_lines(EditState *s);
void do_word_wrap(EditState *s);
void do_count_lines(EditState *s);
void do_what_cursor_position(EditState *s);
void do_set_tab_width(EditState *s, int tab_width);
void do_set_indent_width(EditState *s, int indent_width);
void do_set_indent_tabs_mode(EditState *s, int val);
void display_window_borders(EditState *e);
int find_style_index(const char *name);
QEStyleDef *find_style(const char *name);
void style_completion(CompleteState *cp);
void get_style(EditState *e, QEStyleDef *stp, QETermStyle style);
void style_property_completion(CompleteState *cp);
int find_style_property(const char *name);
void do_define_color(EditState *e, const char *name, const char *value);
void do_set_style(EditState *e, const char *stylestr,
                  const char *propstr, const char *value);
void do_set_display_size(EditState *s, int w, int h);
void do_toggle_mode_line(EditState *s);
void do_set_system_font(EditState *s, const char *qe_font_name,
                        const char *system_fonts);
void do_set_window_style(EditState *s, const char *stylestr);
void call_func(CmdSig sig, CmdProto func, int nb_args, CmdArg *args,
               unsigned char *args_type);
int parse_arg(const char **pp, unsigned char *argtype,
              char *prompt, int prompt_size,
              char *completion, int completion_size,
              char *history, int history_size);
void exec_command(EditState *s, CmdDef *d, int argval, int key);
void do_execute_command(EditState *s, const char *cmd, int argval);
void window_display(EditState *s);
void do_numeric_argument(EditState *s);
void do_start_macro(EditState *s);
void do_end_macro(EditState *s);
void do_call_macro(EditState *s);
void do_execute_macro_keys(EditState *s, const char *keys);
void do_define_kbd_macro(EditState *s, const char *name, const char *keys,
                         const char *key_bind);
void qe_save_macros(EditState *s, EditBuffer *b);

#define COMPLETION_TAB    0
#define COMPLETION_SPACE  1
#define COMPLETION_OTHER  2
void do_minibuffer_complete(EditState *s, int type);
void do_minibuffer_complete_space(EditState *s);
void do_minibuffer_electric(EditState *s, int key);
void minibuf_complete_scroll_up_down(EditState *s, int dir);
void do_minibuffer_history(EditState *s, int dir);
void do_minibuffer_get_binary(EditState *s);
void do_minibuffer_exit(EditState *s, int fabort);
void do_popup_exit(EditState *s);
void do_toggle_read_only(EditState *s);
void do_not_modified(EditState *s, int argval);
void do_find_alternate_file(EditState *s, const char *filename, int bflags);
void do_find_file_noselect(EditState *s, const char *filename, int bflags);
void do_load_file_from_path(EditState *s, const char *filename, int bflags);
void do_set_visited_file_name(EditState *s, const char *filename,
                              const char *renamefile);
void qe_save_open_files(EditState *s, EditBuffer *b);

void do_delete_other_windows(EditState *s, int all);
void do_hide_window(EditState *s, int set);
void do_delete_hidden_windows(EditState *s);
void do_describe_key_briefly(EditState *s);
void do_show_bindings(EditState *s, const char *cmd_name);
void do_apropos(EditState *s, const char *str);
EditBuffer *new_help_buffer(void);
void do_describe_bindings(EditState *s);
void do_help_for_help(EditState *s);
void qe_event_init(void);
void window_get_min_size(EditState *s, int *w_ptr, int *h_ptr);
void window_resize(EditState *s, int target_w, int target_h);
void wheel_scroll_up_down(EditState *s, int dir);
void qe_mouse_event(QEEvent *ev);
void set_user_option(const char *user);
void set_tty_charset(const char *name);

/* parser.c */

int parse_config_file(EditState *s, const char *filename);
void do_eval_expression(EditState *s, const char *expression);
void do_eval_region(EditState *s); /* should pass actual offsets */
void do_eval_buffer(EditState *s);
extern int use_session_file;
int qe_load_session(EditState *s);
void do_save_session(EditState *s, int popup);

/* extras.c */

void do_compare_windows(EditState *s, int argval);
void do_compare_files(EditState *s, const char *filename, int bflags);
void do_delete_horizontal_space(EditState *s);
void do_show_date_and_time(EditState *s, int argval);

enum {
    CMD_TRANSPOSE_CHARS = 1,
    CMD_TRANSPOSE_WORDS,
    CMD_TRANSPOSE_LINES,
};
void do_transpose(EditState *s, int cmd);

/* hex.c */

void hex_write_char(EditState *s, int key);

/* lisp.c */

extern ModeDef lisp_mode;  /* used for org_mode */

/* list.c */

extern ModeDef list_mode;

void list_toggle_selection(EditState *s, int dir);
int list_get_pos(EditState *s);
int list_get_offset(EditState *s);

/* dired.c */

void do_dired(EditState *s, int argval);
void do_filelist(EditState *s, int argval);

/* syntax colorizers */

extern ModeDef c_mode;
extern ModeDef cpp_mode;
extern ModeDef js_mode;
extern ModeDef php_mode;
extern ModeDef csharp_mode;
extern ModeDef css_mode;
extern ModeDef xml_mode;  /* used in docbook_mode */
extern ModeDef htmlsrc_mode;

/* html.c */

extern ModeDef html_mode;  /* used in docbook_mode */
extern int use_html;

/* flags from libqhtml/css.h */
int gxml_mode_init(EditBuffer *b, int flags, const char *default_stylesheet);

/* image.c */

void fill_border(EditState *s, int x, int y, int w, int h, int color);
int qe_bitmap_format_to_pix_fmt(int format);

/* shell.c */

const char *get_shell(void);
void shell_colorize_line(QEColorizeContext *cp,
                         unsigned int *str, int n, ModeDef *syn);

#define SF_INTERACTIVE   0x01
#define SF_COLOR         0x02
#define SF_INFINITE      0x04
#define SF_AUTO_CODING   0x08
#define SF_AUTO_MODE     0x10
#define SF_BUFED_MODE    0x20
EditBuffer *new_shell_buffer(EditBuffer *b0, EditState *e,
                             const char *bufname, const char *caption,
                             const char *path,
                             const char *cmd, int shell_flags);

#define QASSERT(e)      do { if (!(e)) fprintf(stderr, "%s:%d: assertion failed: %s\n", __FILE__, __LINE__, #e); } while (0)

#endif
