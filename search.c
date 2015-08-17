/*
 * QEmacs, tiny but powerful multimode editor
 *
 * Copyright (c) 2000-2002 Fabrice Bellard.
 * Copyright (c) 2000-2015 Charlie Gordon.
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
#include "variables.h"

/* Search stuff */

#define SEARCH_FLAG_SMARTCASE  0x0001  /* case fold unless upper case present */
#define SEARCH_FLAG_IGNORECASE 0x0002
#define SEARCH_FLAG_WORD       0x0004
#define SEARCH_FLAG_WRAPPED    0x0008
#define SEARCH_FLAG_HEX        0x0010
#define SEARCH_FLAG_UNIHEX     0x0020

/* XXX: OPTIMIZE ! */
static int eb_search(EditBuffer *b, int start_offset, int dir, int flags,
                     const unsigned int *buf, int len,
                     CSSAbortFunc *abort_func, void *abort_opaque,
                     int *found_offset, int *found_end)
{
    int total_size = b->total_size;
    int c, c2, offset = start_offset, offset1, offset2, offset3, pos;

    if (len == 0)
        return 0;

    *found_offset = -1;
    *found_end = -1;

    /* analyze buffer if smart case */
    if (flags & SEARCH_FLAG_SMARTCASE) {
        int upper_count = 0;
        int lower_count = 0;
        for (pos = 0; pos < len; pos++) {
            lower_count += qe_islower(buf[pos]);
            upper_count += qe_isupper(buf[pos]);
        }
        if (lower_count > 0 && upper_count == 0)
            flags |= SEARCH_FLAG_IGNORECASE;
    }

    if (flags & SEARCH_FLAG_HEX) {
        /* handle buffer as single bytes */
        /* XXX: should handle ucs2 and ucs4 as words */
        for (;; (void)(dir >= 0 && offset++)) {
            if (dir < 0) {
                if (offset == 0)
                    return 0;
                offset--;
            }
            if (offset >= total_size)
                return 0;

            if ((offset & 0xfffff) == 0) {
                /* check for search abort every megabyte */
                if (abort_func && abort_func(abort_opaque))
                    return -1;
            }

            pos = 0;
            for (offset2 = offset; offset2 < total_size;) {
                /* CG: Should bufferize a bit ? */
                c = eb_read_one_byte(b, offset2++);
                c2 = buf[pos++];
                if (c != c2)
                    break;
                if (pos >= len) {
                    /* check end of word */
                    *found_offset = offset;
                    *found_end = offset2;
                    return 1;
                }
            }
        }
    }

    for (offset1 = offset;;) {
        if (dir < 0) {
            if (offset == 0)
                return 0;
            eb_prevc(b, offset, &offset);
        } else {
            offset = offset1;
            if (offset >= total_size)
                return 0;
        }
        if ((offset & 0xfffff) == 0) {
            /* check for search abort every megabyte */
            if (abort_func && abort_func(abort_opaque))
                return -1;
        }

        if (flags & SEARCH_FLAG_WORD) {
            /* check for start of word */
            c = eb_prevc(b, offset, &offset3);
            if (qe_isword(c))
                continue;
        }

        /* CG: XXX: Should use buffer specific accelerator */
        /* Get first char separately to compute offset1 */
        c = eb_nextc(b, offset, &offset1);

        pos = 0;
        for (offset2 = offset1;;) {
            c2 = buf[pos++];
            if (flags & SEARCH_FLAG_IGNORECASE) {
                if (qe_toupper(c) != qe_toupper(c2))
                    break;
            } else {
                if (c != c2)
                    break;
            }
            if (pos >= len) {
                if (flags & SEARCH_FLAG_WORD) {
                    /* check for end of word */
                    c = eb_nextc(b, offset2, &offset3);
                    if (qe_isword(c))
                        break;
                }
                if (dir > 0 || offset2 <= start_offset) {
                    *found_offset = offset;
                    *found_end = offset2;
                    return 1;
                }
            }
            if (offset2 >= total_size)
                break;
            c = eb_nextc(b, offset2, &offset2);
        }
    }
}

/* should separate search string length and number of match positions */
#define SEARCH_LENGTH  256
#define FOUND_TAG      0x80000000
#define FOUND_REV      0x40000000

/* store last searched string */
static unsigned int last_search_u32[SEARCH_LENGTH];
static int last_search_u32_len = 0;

static int search_abort_func(__unused__ void *opaque)
{
    return is_user_input_pending();
}

typedef struct ISearchState {
    EditState *s;
    int saved_mark;
    int start_offset;
    int start_dir;
    int quoting;
    int dir;
    int pos;
    int search_flags;
    int found_offset, found_end;
    unsigned int search_u32[SEARCH_LENGTH];
} ISearchState;

static void buf_encode_search_u32(buf_t *out, const unsigned int *str, int len)
{
    int i;

    for (i = 0; i < len; i++) {
        unsigned int v = str[i];
        if (!(v & FOUND_TAG)) {
            if (v < 32 || v == 127) {
                buf_printf(out, "^%c", (v + '@') & 127);
            } else {
                buf_putc_utf8(out, v);
            }
            if (buf_avail(out) <= 0)
                break;
        }
    }
}

static void buf_encode_search_str(buf_t *out, const char *str)
{
    while (*str) {
        int c = utf8_decode(&str);
        if (c < 32 || c == 127) {
            buf_printf(out, "^%c", (c + '@') & 127);
        } else {
            buf_putc_utf8(out, c);
        }
        if (buf_avail(out) <= 0)
            break;
    }
}

static void isearch_display(ISearchState *is)
{
    EditState *s = is->s;
    char ubuf[256];
    buf_t outbuf, *out;
    unsigned int buf[SEARCH_LENGTH];
    int c, i, len, hex_nibble, max_nibble, h, hc;
    unsigned int v;
    int search_offset, flags, dir = is->start_dir;

    /* prepare the search bytes */
    len = 0;
    search_offset = is->start_offset;
    max_nibble = hex_nibble = hc = 0;
    flags = is->search_flags;
    if (flags & SEARCH_FLAG_UNIHEX)
        max_nibble = 6;
    if (flags & SEARCH_FLAG_HEX)
        max_nibble = 2;

    for (i = 0; i < is->pos; i++) {
        v = is->search_u32[i];
        if (v & FOUND_TAG) {
            dir = (v & FOUND_REV) ? -1 : 1;
            search_offset = v & ~(FOUND_TAG | FOUND_REV);
            continue;
        }
        c = v;
        if (len < countof(buf)) {
            if (max_nibble) {
                h = to_hex(c);
                if (h >= 0) {
                    hc = (hc << 4) | h;
                    if (++hex_nibble == max_nibble) {
                        buf[len++] = hc;
                        hex_nibble = hc = 0;
                    }
                } else {
                    if (c == ' ' && hex_nibble) {
                        buf[len++] = hc;
                        hex_nibble = hc = 0;
                    }
                }
            } else {
                buf[len++] = c;
            }
        }
    }
    if (hex_nibble >= 2) {
        buf[len++] = hc;
        hex_nibble = hc = 0;
    }

    is->dir = dir;

    if (len == 0) {
        s->b->mark = is->saved_mark;
        s->offset = is->start_offset;
        s->region_style = 0;
        is->found_offset = -1;
    } else {
        if (eb_search(s->b, search_offset, is->dir, flags,
                      buf, len, search_abort_func, NULL,
                      &is->found_offset, &is->found_end) > 0) {
            s->region_style = QE_STYLE_SEARCH_MATCH;
            if (is->dir > 0) {
                s->b->mark = is->found_offset;
                s->offset = is->found_end;
            } else {
                s->b->mark = is->found_end;
                s->offset = is->found_offset;
            }
        }
    }

    /* display search string */
    out = buf_init(&outbuf, ubuf, sizeof(ubuf));
    if (is->found_offset < 0 && len > 0)
        buf_puts(out, "Failing ");
    else
    if (is->search_flags & SEARCH_FLAG_WRAPPED) {
        buf_puts(out, "Wrapped ");
        is->search_flags &= ~SEARCH_FLAG_WRAPPED;
    }
    if (is->search_flags & SEARCH_FLAG_UNIHEX)
        buf_puts(out, "Unihex ");
    if (is->search_flags & SEARCH_FLAG_HEX)
        buf_puts(out, "Hex ");
    if (is->search_flags & SEARCH_FLAG_WORD)
        buf_puts(out, "Word ");
    if (is->search_flags & SEARCH_FLAG_IGNORECASE)
        buf_puts(out, "Folding ");
    else
    if (!(is->search_flags & SEARCH_FLAG_SMARTCASE))
        buf_puts(out, "Exact ");

    buf_puts(out, "I-search");
    if (is->dir < 0)
        buf_puts(out, " backward");
    buf_puts(out, ": ");
    buf_encode_search_u32(out, is->search_u32, is->pos);
    if (is->quoting)
        buf_puts(out, "^Q-");

    /* display text */
    do_center_cursor_maybe(s);
    edit_display(s->qe_state);
    put_status(NULL, "%s", out->buf);
    dpy_flush(s->screen);
}

static int isearch_grab(ISearchState *is, EditBuffer *b, int from, int to)
{
    int offset, c, last = is->pos;
    if (b) {
        if (to < 0 || to > b->total_size)
            to = b->total_size;
        for (offset = from; is->pos < SEARCH_LENGTH && offset < to;) {
            c = eb_nextc(b, offset, &offset);
            is->search_u32[is->pos++] = c;
        }
    }
    return is->pos - last;
}

static void isearch_key(void *opaque, int ch)
{
    ISearchState *is = opaque;
    EditState *s = is->s;
    QEmacsState *qs = &qe_state;
    int i, j, offset0, offset1, curdir = is->dir;
    int emacs_behaviour = !qs->emulation_flags;

    if (is->quoting) {
        is->quoting = 0;
        if (!KEY_IS_SPECIAL(ch))
            goto addch;
    }
    /* XXX: all these should be isearch-mode bindings */
    switch (ch) {
    case KEY_DEL:
    case KEY_BS:
        if (is->pos > 0)
            is->pos--;
        break;
    case KEY_CTRL('g'):
        s->b->mark = is->saved_mark;
        s->offset = is->start_offset;
        s->region_style = 0;
        put_status(s, "Quit");
    the_end:
        /* save current searched string */
        if (is->pos > 0) {
            j = 0;
            for (i = 0; i < is->pos; i++) {
                if (!(is->search_u32[i] & FOUND_TAG))
                    last_search_u32[j++] = is->search_u32[i];
            }
            last_search_u32_len = j;
        }
        qe_ungrab_keys();
        qe_free(&is);
        edit_display(s->qe_state);
        dpy_flush(s->screen);
        return;
    case KEY_CTRL('s'):         /* next match */
        is->dir = 1;
        goto addpos;
    case KEY_CTRL('r'):         /* previous match */
        is->dir = -1;
    addpos:
        /* use last seached string if no input */
        if (is->pos == 0 && is->dir == curdir) {
            memcpy(is->search_u32, last_search_u32,
                   last_search_u32_len * sizeof(*is->search_u32));
            is->pos = last_search_u32_len;
        } else
        if (is->pos < SEARCH_LENGTH) {
            /* add the match position, if any */
            unsigned long v = is->dir > 0 ? FOUND_TAG : FOUND_TAG | FOUND_REV;
            if (is->found_offset < 0) {
                is->search_flags |= SEARCH_FLAG_WRAPPED;
                if (is->dir < 0)
                    v |= s->b->total_size;
            } else {
                v |= s->offset;
            }
            is->search_u32[is->pos++] = v;
        }
        break;
    case KEY_CTRL('q'):
        is->quoting = 1;
        break;
    case KEY_META('w'):
        emacs_behaviour ^= 1;
        /* fall thru */
    case KEY_CTRL('w'):
        if (emacs_behaviour) {
            /* grab word at cursor */
            offset0 = s->offset;
            do_word_right(s, 1);
            offset1 = s->offset;
            s->offset = offset0;
            isearch_grab(is, s->b, offset0, offset1);
        } else {
            /* toggle word match */
            is->search_flags ^= SEARCH_FLAG_WORD;
        }
        break;
    case KEY_META('y'):
        emacs_behaviour ^= 1;
        /* fall thru */
    case KEY_CTRL('y'):
        if (emacs_behaviour) {
            /* grab line at cursor */
            offset0 = s->offset;
            if (eb_nextc(s->b, offset0, &offset1) == '\n')
                offset0 = offset1;
            do_eol(s);
            offset1 = s->offset;
            s->offset = offset0;
            isearch_grab(is, s->b, offset0, offset1);
        } else {
            /* yank into search string */
            isearch_grab(is, qs->yank_buffers[qs->yank_current], 0, -1);
        }
        break;
    case KEY_META('b'):
    case KEY_CTRL('b'):
        /* cycle unihex, hex, normal search */
        if (is->search_flags & SEARCH_FLAG_UNIHEX)
            is->search_flags ^= SEARCH_FLAG_HEX | SEARCH_FLAG_UNIHEX;
        else
        if (is->search_flags & SEARCH_FLAG_HEX)
            is->search_flags ^= SEARCH_FLAG_HEX;
        else
            is->search_flags ^= SEARCH_FLAG_UNIHEX;
        break;
    case KEY_META('c'):
    case KEY_CTRL('c'):
        /* toggle case sensitivity */
        if (is->search_flags & (SEARCH_FLAG_IGNORECASE | SEARCH_FLAG_SMARTCASE)) {
            is->search_flags &= ~SEARCH_FLAG_IGNORECASE;
        } else {
            is->search_flags |= SEARCH_FLAG_IGNORECASE;
        }
        is->search_flags &= ~SEARCH_FLAG_SMARTCASE;
        break;
    case KEY_CTRL('l'):
        do_center_cursor(s);
        break;
    default:
        if ((KEY_IS_SPECIAL(ch) || KEY_IS_CONTROL(ch)) && ch != '\t') {
            /* exit search mode */
#if 0
            // FIXME: behaviour from qemacs-0.3pre13
            if (is->found_offset >= 0) {
                s->b->mark = is->found_offset;
            } else {
                s->b->mark = is->start_offset;
            }
            put_status(s, "Marked");
#endif
            s->b->mark = is->start_offset;
            s->region_style = 0;
            put_status(s, "Mark saved where search started");
            /* repost key */
            if (ch != KEY_RET)
                unget_key(ch);
            goto the_end;
        } else {
        addch:
            if (is->pos < SEARCH_LENGTH) {
                is->search_u32[is->pos++] = ch;
            }
        }
        break;
    }
    isearch_display(is);
}

/* XXX: handle busy */
void do_isearch(EditState *s, int dir)
{
    ISearchState *is;
    int flags = SEARCH_FLAG_SMARTCASE;

    is = qe_mallocz(ISearchState);
    if (!is)
        return;
    is->s = s;
    is->saved_mark = s->b->mark;
    is->start_offset = s->offset;
    is->start_dir = is->dir = dir;
    is->pos = 0;
    if (s->hex_mode) {
        if (s->unihex_mode)
            flags |= SEARCH_FLAG_UNIHEX;
        else
            flags |= SEARCH_FLAG_HEX;
    }
    is->search_flags = flags;

    qe_grab_keys(isearch_key, is);
    isearch_display(is);
}

static int search_to_u32(unsigned int *buf, int size,
                         const char *str, int flags)
{
    if (flags & (SEARCH_FLAG_HEX | SEARCH_FLAG_UNIHEX)) {
        /* CG: XXX: Should mix utf8 and hex syntax in hex modes */
        const char *s;
        int c, hex_nibble, max_nibble, h, hc, len;

        max_nibble = (flags & SEARCH_FLAG_UNIHEX) ? 6 : 2;
        s = str;
        hex_nibble = hc = 0;
        for (len = 0; len < size;) {
            c = *s++;
            if (c == '\0') {
                if (hex_nibble >= 2) {
                    buf[len++] = hc;
                    hex_nibble = hc = 0;
                }
                break;
            }
            h = to_hex(c);
            if (h >= 0) {
                hc = (hc << 4) | h;
                if (++hex_nibble == max_nibble) {
                    buf[len++] = hc;
                    hex_nibble = hc = 0;
                }
            } else {
                if (c == ' ' && hex_nibble) {
                    buf[len++] = hc;
                    hex_nibble = hc = 0;
                }
            }
        }
        return len;
    } else {
        return utf8_to_unicode(buf, size, str);
    }
}

typedef struct QueryReplaceState {
    EditState *s;
    int start_offset;
    int search_flags;
    int replace_all;
    int nb_reps;
    int search_u32_len, replace_u32_len;
    int found_offset, found_end;
    int last_offset;
    char search_str[SEARCH_LENGTH];     /* may be in hex */
    char replace_str[SEARCH_LENGTH];    /* may be in hex */
    unsigned int search_u32[SEARCH_LENGTH];   /* code points */
    unsigned int replace_u32[SEARCH_LENGTH];  /* code points */
} QueryReplaceState;

static void query_replace_abort(QueryReplaceState *is)
{
    EditState *s = is->s;

    qe_ungrab_keys();
    s->b->mark = is->start_offset;
    s->region_style = 0;
    put_status(NULL, "Replaced %d occurrences", is->nb_reps);
    qe_free(&is);
    edit_display(s->qe_state);
    dpy_flush(s->screen);
}

static void query_replace_replace(QueryReplaceState *is)
{
    EditState *s = is->s;

    /* XXX: handle smart case replacement */
    is->nb_reps++;
    eb_delete(s->b, is->found_offset, is->found_end - is->found_offset);
    is->found_offset += eb_insert_u32_buf(s->b, is->found_offset,
        is->replace_u32, is->replace_u32_len);
}

static void query_replace_display(QueryReplaceState *is)
{
    EditState *s = is->s;
    char ubuf[256];
    buf_t outbuf, *out;

    is->last_offset = is->found_offset;
    is->search_u32_len = search_to_u32(is->search_u32,
                                       countof(is->search_u32),
                                       is->search_str, is->search_flags);
    is->replace_u32_len = search_to_u32(is->replace_u32,
                                        countof(is->replace_u32),
                                        is->replace_str, is->search_flags);

    for (;;) {
        if (eb_search(s->b, is->found_offset, 1, is->search_flags,
                      is->search_u32, is->search_u32_len,
                      NULL, NULL, &is->found_offset, &is->found_end) <= 0) {
            query_replace_abort(is);
            return;
        }
        if (is->replace_all) {
            query_replace_replace(is);
            continue;
        }
        break;
    }
    /* display prompt string */
    out = buf_init(&outbuf, ubuf, sizeof(ubuf));
    if (is->search_flags & SEARCH_FLAG_UNIHEX)
        buf_puts(out, "Unihex ");
    if (is->search_flags & SEARCH_FLAG_HEX)
        buf_puts(out, "Hex ");
    if (is->search_flags & SEARCH_FLAG_WORD)
        buf_puts(out, "Word ");
    if (is->search_flags & SEARCH_FLAG_IGNORECASE)
        buf_puts(out, "Folding ");
    else
    if (!(is->search_flags & SEARCH_FLAG_SMARTCASE))
        buf_puts(out, "Exact ");

    buf_puts(out, "Query replace ");
    buf_encode_search_str(out, is->search_str);
    buf_puts(out, " with ");
    buf_encode_search_str(out, is->replace_str);
    buf_puts(out, ": ");

    s->offset = is->found_end;
    s->b->mark = is->found_offset;
    s->region_style = QE_STYLE_SEARCH_MATCH;
    do_center_cursor_maybe(s);
    edit_display(s->qe_state);
    put_status(NULL, "%s", out->buf);
    dpy_flush(s->screen);
}

static void query_replace_key(void *opaque, int ch)
{
    QueryReplaceState *is = opaque;
    EditState *s = is->s;
    QEmacsState *qs = &qe_state;

    switch (ch) {
    case 'Y':
    case 'y':
    case KEY_SPC:
        query_replace_replace(is);
        s->offset = is->found_offset;
        break;
    case '!':
        is->replace_all = 1;
        break;
    case 'N':
    case 'n':
    case KEY_DELETE:
        is->found_offset = is->found_end;
        break;
    case KEY_META('w'):
    case KEY_CTRL('w'):
        /* toggle word match */
        is->search_flags ^= SEARCH_FLAG_WORD;
        is->found_offset = is->last_offset;
        break;
    case KEY_META('b'):
    case KEY_CTRL('b'):
        /* cycle unihex, hex, normal search */
        if (is->search_flags & SEARCH_FLAG_UNIHEX)
            is->search_flags ^= SEARCH_FLAG_HEX | SEARCH_FLAG_UNIHEX;
        else
        if (is->search_flags & SEARCH_FLAG_HEX)
            is->search_flags ^= SEARCH_FLAG_HEX;
        else
            is->search_flags ^= SEARCH_FLAG_UNIHEX;
        is->found_offset = is->last_offset;
        break;
    case KEY_META('c'):
    case KEY_CTRL('c'):
        /* toggle case sensitivity */
        if (is->search_flags & (SEARCH_FLAG_IGNORECASE | SEARCH_FLAG_SMARTCASE)) {
            is->search_flags &= ~SEARCH_FLAG_IGNORECASE;
        } else {
            is->search_flags |= SEARCH_FLAG_IGNORECASE;
        }
        is->search_flags &= ~SEARCH_FLAG_SMARTCASE;
        is->found_offset = is->last_offset;
        break;
    case KEY_CTRL('g'):
        /* abort */
        if (qs->emulation_flags) {
            /* restore point to original location */
            s->offset = is->start_offset;
        }
        query_replace_abort(is);
        return;
    case KEY_CTRL('l'):
        do_center_cursor(s);
        break;
    case '.':
        query_replace_replace(is);
        s->offset = is->found_offset;
        /* FALL THRU */
    default:
        query_replace_abort(is);
        return;
    }
    query_replace_display(is);
}

static void query_replace(EditState *s, const char *search_str,
                          const char *replace_str, int all, int flags)
{
    QueryReplaceState *is;

    if (s->b->flags & BF_READONLY)
        return;

    is = qe_mallocz(QueryReplaceState);
    if (!is)
        return;
    is->s = s;
    pstrcpy(is->search_str, sizeof(is->search_str), search_str);
    pstrcpy(is->replace_str, sizeof(is->replace_str), replace_str);

    if (s->hex_mode) {
        if (s->unihex_mode)
            flags = SEARCH_FLAG_UNIHEX;
        else
            flags = SEARCH_FLAG_HEX;
    }
    is->search_flags = flags;
    is->replace_all = all;
    is->start_offset = is->last_offset = s->offset;
    is->found_offset = is->found_end = s->offset;

    qe_grab_keys(query_replace_key, is);
    query_replace_display(is);
}

void do_query_replace(EditState *s, const char *search_str,
                      const char *replace_str)
{
    int flags = SEARCH_FLAG_SMARTCASE;
    query_replace(s, search_str, replace_str, 0, flags);
}

void do_replace_string(EditState *s, const char *search_str,
                       const char *replace_str, int argval)
{
    int flags = SEARCH_FLAG_SMARTCASE;
    if (argval != NO_ARG)
        flags |= SEARCH_FLAG_WORD;
    query_replace(s, search_str, replace_str, 1, flags);
}

void do_search_string(EditState *s, const char *search_str, int dir)
{
    unsigned int search_u32[SEARCH_LENGTH];
    int search_u32_len;
    int found_offset, found_end;
    int flags = SEARCH_FLAG_SMARTCASE;

    if (s->hex_mode) {
        if (s->unihex_mode)
            flags |= SEARCH_FLAG_UNIHEX;
        else
            flags |= SEARCH_FLAG_HEX;
    }
    search_u32_len = search_to_u32(search_u32, countof(search_u32),
                                   search_str, flags);
    if (eb_search(s->b, s->offset, dir, flags, search_u32, search_u32_len,
                  NULL, NULL, &found_offset, &found_end) > 0) {
        s->offset = (dir < 0) ? found_offset : found_end;
        do_center_cursor_maybe(s);
    } else {
        put_status(s, "Search failed: \"%s\"", search_str);
    }
}
