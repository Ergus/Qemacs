/*
 * Markdown mode for QEmacs.
 *
 * Copyright (c) 2014-2017 Charlie Gordon.
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

static ModeDef litcoffee_mode;

enum {
    /* TODO: define specific styles */
    MKD_STYLE_HEADING1    = QE_STYLE_FUNCTION,
    MKD_STYLE_HEADING2    = QE_STYLE_STRING,
    MKD_STYLE_HEADING3    = QE_STYLE_VARIABLE,
    MKD_STYLE_HEADING4    = QE_STYLE_TYPE,
    MKD_STYLE_TILDE       = QE_STYLE_PREPROCESS,
    MKD_STYLE_COMMENT     = QE_STYLE_COMMENT,
    MKD_STYLE_BLOCK_QUOTE = QE_STYLE_FUNCTION,
    MKD_STYLE_TABLE       = QE_STYLE_TYPE,
    MKD_STYLE_HBAR        = QE_STYLE_VARIABLE,
    MKD_STYLE_STRONG2     = QE_STYLE_FUNCTION,
    MKD_STYLE_STRONG1     = QE_STYLE_FUNCTION,
    MKD_STYLE_EMPHASIS2   = QE_STYLE_VARIABLE,
    MKD_STYLE_EMPHASIS1   = QE_STYLE_VARIABLE,
    MKD_STYLE_CODE        = QE_STYLE_STRING,
    MKD_STYLE_IMAGE_LINK  = QE_STYLE_KEYWORD,
    MKD_STYLE_REF_LINK    = QE_STYLE_KEYWORD,
    MKD_STYLE_REF_HREF    = QE_STYLE_COMMENT,
    MKD_STYLE_DLIST       = QE_STYLE_NUMBER,
    MKD_STYLE_LIST        = QE_STYLE_NUMBER,
};

enum {
    IN_MKD_LANG_STATE   = 0x00FF,
    IN_MKD_LEVEL        = 0x0700,
    IN_MKD_BLOCK        = 0x7800,
    IN_MKD_HTML_BLOCK   = 0x8000,
    IN_MKD_HTML_COMMENT = 0xC000,
    MKD_LANG_SHIFT      = 11,
    MKD_LANG_MAX        = (IN_MKD_BLOCK >> MKD_LANG_SHIFT),
    MKD_LEVEL_SHIFT     = 8,
    MKD_LEVEL_MAX       = 7,
};

static ModeDef *mkd_lang_def[MKD_LANG_MAX + 1];
static char mkd_lang_char[MKD_LANG_MAX + 1];

#define MKD_BULLET_STYLES  4
static int MkdBulletStyles[MKD_BULLET_STYLES] = {
    MKD_STYLE_HEADING1,
    MKD_STYLE_HEADING2,
    MKD_STYLE_HEADING3,
    MKD_STYLE_HEADING4,
};

static int mkd_scan_chunk(const unsigned int *str,
                          const char *begin, const char *end, int min_width)
{
    int i, j;

    for (i = 0; begin[i]; i++) {
        if (str[i] != begin[i])
            return 0;
    }
    if (qe_isspace(str[i]))
        return 0;

    for (j = 0; j < min_width; j++) {
        if (str[i + j] == '\0')
            return 0;
    }
    for (i += j; str[i] != '\0'; i++) {
        if (!qe_isspace(str[i - 1])) {
            for (j = 0; end[j]; j++) {
                if (str[i + j] != end[j])
                    break;
            }
            if (!end[j])
                return i + j;
        }
    }
    return 0;
}

static int mkd_add_lang(const char *lang_name, char c) {
    ModeDef *m;
    int lang = 0;

    if (lang_name && (m = qe_find_mode(lang_name, MODEF_SYNTAX)) != NULL) {
        for (lang = 1; lang < MKD_LANG_MAX; lang++) {
            if (mkd_lang_def[lang] == NULL)
                mkd_lang_def[lang] = m;
            if (mkd_lang_def[lang] == m) {
                mkd_lang_char[lang] = c;
                break;
            }
        }
    }
    return lang;
}

static void mkd_colorize_line(QEColorizeContext *cp,
                              unsigned int *str, int n, ModeDef *syn)
{
    int colstate = cp->colorize_state;
    int level, indent, i = 0, j, start = i, base_style = 0;

    if (syn == &litcoffee_mode)
        base_style = MKD_STYLE_COMMENT;

    for (indent = j = 0;; j++) {
        if (str[j] == ' ')
            indent++;
        else
        if (str[j] == '\t')
            indent += 4;
        else
            break;
    }

    if (str[i] == '<' && str[i + 1] == '!' && str[i + 2] == '-' && str[i + 3] == '-') {
        colstate |= IN_MKD_HTML_COMMENT;
        i += 3;
    }

    if ((colstate & IN_MKD_HTML_COMMENT) == IN_MKD_HTML_COMMENT) {
        while (i < n) {
            int c = str[i++];
            if (c == '-' && str[i] == '-' && str[i + 1] == '>') {
                i += 2;
                colstate &= ~IN_MKD_HTML_COMMENT;
                break;
            }
        }
        SET_COLOR(str, start, i, MKD_STYLE_COMMENT);
        cp->colorize_state = colstate;
        return;
    }

    if (str[i] == '>') {
        if (str[++i] == ' ')
            i++;
        SET_COLOR(str, start, i, MKD_STYLE_BLOCK_QUOTE);
        start = i;
    }

    if (colstate & IN_MKD_BLOCK) {
        int lang = (colstate & IN_MKD_BLOCK) >> MKD_LANG_SHIFT;

        /* Should count number of ~ to detect end of block */
        if (ustrstart(str + j, "~~~", NULL)
        ||  ustrstart(str + j, "```", NULL)
        ||  (indent < 4 && mkd_lang_char[lang] == ':')) {
            colstate &= ~IN_MKD_BLOCK;
            i = n;
            SET_COLOR(str, start, i, MKD_STYLE_TILDE);
        } else {
            if (mkd_lang_def[lang]) {
                cp->colorize_state = colstate & IN_MKD_LANG_STATE;
                mkd_lang_def[lang]->colorize_func(cp, str + i, n - i, mkd_lang_def[lang]);
                colstate &= ~IN_MKD_LANG_STATE;
                colstate |= cp->colorize_state & IN_MKD_LANG_STATE;
            } else {
                SET_COLOR(str, start, n, MKD_STYLE_CODE);
            }
            i = n;
        }
        cp->colorize_state = colstate;
        return;
    }

    if (colstate & IN_MKD_HTML_BLOCK) {
        if (i < n && str[i] != '<' && !qe_isblank(str[i])) {
            /* formating error, exit html block */
            colstate = 0;
        }
    }

    if ((colstate & IN_MKD_HTML_BLOCK)
    ||  (str[i] == '<' && (str[i + 1] == '!' || str[i + 1] == '?' || qe_isalpha(str[i + 1])))) {
        /* block level HTML markup */
        colstate &= ~IN_MKD_HTML_BLOCK;
        cp->colorize_state = colstate;
        htmlsrc_mode.colorize_func(cp, str, n, &htmlsrc_mode);
        colstate = cp->colorize_state;
        colstate |= IN_MKD_HTML_BLOCK;
        if ((str[i] & CHAR_MASK) == '<' && (str[i + 1] & CHAR_MASK) == '/')
            colstate = 0;
        cp->colorize_state = colstate;
        return;
    }

    if (str[i] == '#') {
        /* Check for heading: initial string of '#' followed by ' ' */
        for (i++; str[i] == '#'; i++)
            continue;

        if (qe_isblank(str[i])) {
            base_style = MkdBulletStyles[(i - start - 1) % MKD_BULLET_STYLES];
            i++;
            SET_COLOR(str, start, i, base_style);
        }
    } else
    if (str[i] == '%') {
        /* pandoc extension: line comment */
        i = n;
        SET_COLOR(str, start, i, MKD_STYLE_COMMENT);
    } else
    if (str[i] == '-') {
        /* dashes underline a heading */
        for (i++; str[i] == '-'; i++)
            continue;
        if (i == n) {
            SET_COLOR(str, start, i, MKD_STYLE_HEADING2);
        }
    } else
    if (str[i] == '=') {
        /* equal signs indicate a heading */
        for (i++; str[i] == '='; i++)
            continue;
        if (i == n) {
            SET_COLOR(str, start, i, MKD_STYLE_HEADING1);
        }
    } else
    if (str[i] == '|') {
        base_style = MKD_STYLE_TABLE;
    } else
    if (ustrstart(str + j, "~~~", NULL)
    ||  ustrstart(str + j, "```", NULL)
    ||  ustrstart(str + j, ":::", NULL)) {
        /* verbatim block */
        char lang_name[16];
        int lang = syn->colorize_flags, len;  // was MKD_LANG_MAX

        colstate &= ~(IN_MKD_BLOCK | IN_MKD_LANG_STATE);
        for (i = j + 3; qe_isblank(str[i]); i++)
            continue;
        for (len = 0; i < n && !qe_isblank(str[i]); i++) {
            if (len < countof(lang_name) - 1)
                lang_name[len++] = str[i];
        }
        lang_name[len] = '\0';
        if (len) {
            lang = mkd_add_lang(lang_name, str[j]);
        }
        colstate |= lang << MKD_LANG_SHIFT;
        i = n;
        SET_COLOR(str, start, i, MKD_STYLE_TILDE);
    }

    /* [X] unordered lists: /[-*+] / */
    /* [X] ordered lists: /[0-9]+[.] / */
    /* [ ] list continuation lines are indented 1 level */
    /* [ ] code blocks are indented one extra level */
    /* [X] horizontal rules: /^ *([-*_][ ]*){3-}$/ */
    /* [/] inline links: /[[].*[]]([(].*[)])?/ */
    /* [/] reference links: /[[].*[]][ ]*[[][a-zA-Z0-9 ,.;:?]*[]])/ */
    /* [/] references: /[ ]{0-3}[[][a-zA-Z0-9 ,.;:?]+[]]:.*)/ */
    /* [/] images: same as links, preceded by ! */
    /* [X] automatic links and email addresses: <http://adress> */
    /* [X] emphasis: _.*_  \*.*\*  __.*__  \*\*.*\*\*  */
    /* [X] code span: `code` */
    /* [X] code span with embedded `: ``code`` or `` code `` */
    /* [X] litteral chars: isolate them or escape them with \ */
    /*                 \ ` * _ { } [ ] ( ) # + - . ! */

    level = (colstate & IN_MKD_LEVEL) >> MKD_LEVEL_SHIFT;
    i = j;

    if (i < n) {
        start = i;
        /* ignore blank lines for level and code triggers */
        if (indent < level * 4) {
            level = indent >> 2;
        }
        indent -= level * 4;

        if (indent >= 4) {
            int lang = syn->colorize_flags; /* default language */

            /* Should detect sequel lines in ordered/unordered lists */
            if (mkd_lang_def[lang]) {
                cp->colorize_state = colstate & IN_MKD_LANG_STATE;
                mkd_lang_def[lang]->colorize_func(cp, str + 4, n - 4, mkd_lang_def[lang]);
                colstate &= ~IN_MKD_LANG_STATE;
                colstate |= cp->colorize_state & IN_MKD_LANG_STATE;
            } else {
                SET_COLOR(str, i, n, MKD_STYLE_CODE);
            }
            i = n;
        }
    }

    if (str[i] == '*' || str[i] == '-' || str[i] == '_') {
        int count = 1;
        for (j = i + 1; j < n; j++) {
            if (qe_isblank(str[j]))
                continue;
            if (str[j] == str[i])
                count++;
            else
                break;
        }
        if (j == n && count >= 3) {
            /* Horizontal rule */
            start = i;
            i = n;
            SET_COLOR(str, start, i, MKD_STYLE_HBAR);
        }
    }

    if (level) {
        base_style = MKD_STYLE_LIST;
    }

    if (qe_isdigit(str[i])) {
        for (j = i + 1; qe_isdigit(str[j]); j++)
            continue;
        if (str[j] == '.' && qe_isblank(str[j + 1])) {
            level++;
            base_style = MKD_STYLE_DLIST;
            start = i;
            i = j;
            SET_COLOR(str, start, i, base_style);
        }
    } else
    if ((str[i] == '-' || str[i] == '*' || str[i] == '+')
    &&  qe_isblank(str[i + 1])) {
        start = i;
        level++;
        base_style = MKD_STYLE_LIST;
        SET_COLOR(str, start, i, base_style);
    }

    for (;;) {
        int chunk = 0, chunk_style = base_style;
        int c, flags;

        start = i;
        c = str[i];

        if (c == '\0')
            break;

        switch (c) {
        case '#':
            break;
        case '*':  /* bold */
            chunk_style = MKD_STYLE_STRONG2;
            chunk = mkd_scan_chunk(str + i, "**", "**", 1);
            if (chunk)
                break;
            chunk_style = MKD_STYLE_STRONG1;
            chunk = mkd_scan_chunk(str + i, "*", "*", 1);
            if (chunk)
                break;
            break;
        case '_':  /* emphasis */
            chunk_style = MKD_STYLE_EMPHASIS2;
            chunk = mkd_scan_chunk(str + i, "__", "__", 1);
            if (chunk)
                break;
            chunk_style = MKD_STYLE_EMPHASIS1;
            chunk = mkd_scan_chunk(str + i, "_", "_", 1);
            if (chunk)
                break;
            break;
        case '`':  /* code */
            chunk_style = MKD_STYLE_CODE;
            chunk = mkd_scan_chunk(str + i, "`` ", " ``", 1);
            if (chunk)
                break;
            chunk = mkd_scan_chunk(str + i, "``", "``", 1);
            if (chunk)
                break;
            chunk = mkd_scan_chunk(str + i, "`", "`", 1);
            if (chunk)
                break;
            break;
        case '!':  /* image link ^[...: <...>] */
            chunk_style = MKD_STYLE_IMAGE_LINK;
            chunk = mkd_scan_chunk(str + i, "![", "]", 1);
            break;
        case '[':  /* link ^[...: <...>] */
            chunk_style = MKD_STYLE_REF_LINK;
            chunk = mkd_scan_chunk(str + i, "[", "]", 1);
            if (chunk && str[i + chunk] == '(') {
                i += chunk;
                SET_COLOR(str, start, i, chunk_style);
                chunk_style = MKD_STYLE_REF_HREF;
                chunk = mkd_scan_chunk(str + i, "(", ")", 1);
            }
            break;
        case '<':  /* automatic link <http://address> */
            chunk_style = MKD_STYLE_REF_LINK;
            chunk = mkd_scan_chunk(str + i, "<http", ">", 1);
            if (chunk)
                break;
            /* match an email address */
            for (flags = 0, j = i + 1; j < n;) {
                int d = str[j++];
                if (d == '@')
                    flags++;
                if (d == '>') {
                    if (flags == 1) {
                        chunk = j - i;
                    }
                    break;
                }
            }
            break;
        case '\\':  /* escape */
            if (strchr("\\`*_{}[]()#+-.!", str[i + 1])) {
                chunk = 2;
                break;
            }
            break;
        }
        if (chunk) {
            i += chunk;
            SET_COLOR(str, start, i, chunk_style);
        } else {
            i++;
            SET_COLOR1(str, start, base_style);
        }
    }

    colstate &= ~IN_MKD_LEVEL;
    colstate |= (level << MKD_LEVEL_SHIFT) & IN_MKD_LEVEL;
    cp->colorize_state = colstate;
}

static int mkd_is_header_line(EditState *s, int offset)
{
    /* Check if line starts with '#' */
    /* XXX: should ignore blocks using colorstate */
    return eb_nextc(s->b, eb_goto_bol(s->b, offset), &offset) == '#';
}

static int mkd_find_heading(EditState *s, int offset, int *level, int silent)
{
    int offset1, nb, c;

    offset = eb_goto_bol(s->b, offset);
    for (;;) {
        /* Find line starting with '#' */
        /* XXX: should ignore blocks using colorstate */
        if (eb_nextc(s->b, offset, &offset1) == '#') {
            for (nb = 1; (c = eb_nextc(s->b, offset1, &offset1)) == '#'; nb++)
                continue;
            if (qe_isblank(c)) {
                *level = nb;
                return offset;
            }
        }
        if (offset == 0)
            break;
        offset = eb_prev_line(s->b, offset);
    }
    if (!silent)
        put_status(s, "Before first heading");

    return -1;
}

static int mkd_next_heading(EditState *s, int offset, int target, int *level)
{
    int offset1, nb, c;

    for (;;) {
        offset = eb_next_line(s->b, offset);
        if (offset >= s->b->total_size) {
            nb = 0;
            break;
        }
        /* XXX: should ignore blocks using colorstate */
        if (eb_nextc(s->b, offset, &offset1) == '#') {
            for (nb = 1; (c = eb_nextc(s->b, offset1, &offset1)) == '#'; nb++)
                continue;
            if (qe_isblank(c) && nb <= target) {
                break;
            }
        }
    }
    if (level)
        *level = nb;
    return offset;
}

static int mkd_prev_heading(EditState *s, int offset, int target, int *level)
{
    int offset1, nb, c;

    for (;;) {
        if (offset == 0) {
            nb = 0;
            break;
        }
        offset = eb_prev_line(s->b, offset);
        /* XXX: should ignore blocks using colorstate */
        if (eb_nextc(s->b, offset, &offset1) == '#') {
            for (nb = 1; (c = eb_nextc(s->b, offset1, &offset1)) == '#'; nb++)
                continue;
            if (qe_isblank(c) && nb <= target) {
                break;
            }
        }
    }
    if (level)
        *level = nb;
    return offset;
}

static void do_outline_next_vsible_heading(EditState *s)
{
    s->offset = mkd_next_heading(s, s->offset, MKD_LEVEL_MAX, NULL);
}

static void do_outline_previous_vsible_heading(EditState *s)
{
    s->offset = mkd_prev_heading(s, s->offset, MKD_LEVEL_MAX, NULL);
}

static void do_outline_up_heading(EditState *s)
{
    int offset, level;

    offset = mkd_find_heading(s, s->offset, &level, 0);
    if (offset < 0)
        return;

    if (level <= 1) {
        put_status(s, "Already at top level of the outline");
        return;
    }

    s->offset = mkd_prev_heading(s, offset, level - 1, &level);
}

static void do_mkd_backward_same_level(EditState *s)
{
    int offset, level, level1;

    offset = mkd_find_heading(s, s->offset, &level, 0);
    if (offset < 0)
        return;

    offset = mkd_prev_heading(s, offset, level, &level1);
    if (level1 != level) {
        put_status(s, "No previous same-level heading");
        return;
    }
    s->offset = offset;
}

static void do_mkd_forward_same_level(EditState *s)
{
    int offset, level, level1;

    offset = mkd_find_heading(s, s->offset, &level, 0);
    if (offset < 0)
        return;

    offset = mkd_next_heading(s, offset, level, &level1);
    if (level1 != level) {
        put_status(s, "No following same-level heading");
        return;
    }
    s->offset = offset;
}

static void do_mkd_goto(EditState *s, const char *dest)
{
    int offset, level, level1, nb;
    const char *p = dest;

    /* XXX: Should pop up a window with numbered outline index
     * and let the user select the target interactively.
     */

    /* Jump to numbered destination. */
    for (offset = 0, level = 0; qe_isdigit(*p); ) {
        nb = strtol(p, (char **)&p, 10);
        if (*p == '.')
            p++;
        level++;
        for (; nb > 0; nb--) {
            offset = mkd_next_heading(s, offset, level, &level1);
            if (level != level1) {
                put_status(s, "Heading not found");
                return;
            }
        }
    }
    if (level)
        s->offset = offset;
}

static void do_mkd_mark_element(EditState *s, int subtree)
{
    QEmacsState *qs = s->qe_state;
    int offset, offset1, level;

    offset = mkd_find_heading(s, s->offset, &level, 0);
    if (offset < 0)
        return;

    offset1 = mkd_next_heading(s, offset, subtree ? level : MKD_LEVEL_MAX, NULL);

    /* XXX: if repeating last command, add subtree to region */
    if (qs->last_cmd_func != qs->this_cmd_func)
        s->b->mark = offset;

    s->offset = offset1;
    /* activate region hilite */
    if (s->qe_state->hilite_region)
        s->region_style = QE_STYLE_REGION_HILITE;
}

static void do_mkd_insert_heading(EditState *s, int flags)
{
    int offset, offset0, offset1, level = 1;

    if (check_read_only(s))
        return;

    offset = mkd_find_heading(s, s->offset, &level, 1);
    offset0 = eb_goto_bol(s->b, s->offset);
    offset1 = eb_goto_eol(s->b, s->offset);

    /* if at beginning of heading line, insert sibling heading before,
     * if in the middle of a heading line, split the heading,
     * otherwise, make the current line a heading line at current level.
     */
    if (flags & 2) {
        /* respect-content: insert heading at end of subtree */
        offset = mkd_next_heading(s, offset, level, NULL);
        eb_insert_uchars(s->b, offset, '\n', 2);
    } else
    if (s->offset <= offset + level + 1) {
        eb_insert_uchar(s->b, offset, '\n');
    } else
    if (offset == offset0 || offset == offset1) {
        offset = s->offset;
        offset += eb_insert_uchar(s->b, offset, '\n');
    } else {
        offset = offset0;
    }
    offset1 = offset;
    while (eb_match_uchar(s->b, offset1, ' ', &offset1))
        continue;
    eb_delete(s->b, offset, offset1 - offset);

    offset += eb_insert_uchars(s->b, offset, '#', level);
    offset += eb_insert_uchar(s->b, offset, ' ');
    s->offset = eb_goto_eol(s->b, offset);
}

static void do_mkd_promote(EditState *s, int dir)
{
    int offset, level;

    if (check_read_only(s))
        return;

    offset = mkd_find_heading(s, s->offset, &level, 0);
    if (offset < 0)
        return;

    if (dir < 0) {
        eb_insert_uchar(s->b, offset, '#');
    } else
    if (dir > 0) {
        if (level > 1)
            eb_delete_uchar(s->b, offset);
        else
            put_status(s, "Cannot promote to level 0");
    }
}

static void do_mkd_promote_subtree(EditState *s, int dir)
{
    int offset, level, level1;

    if (check_read_only(s))
        return;

    offset = mkd_find_heading(s, s->offset, &level, 0);
    if (offset < 0)
        return;

    for (;;) {
        if (dir < 0) {
            eb_insert_uchar(s->b, offset, '#');
        } else
        if (dir > 0) {
            if (level > 1) {
                eb_delete_uchar(s->b, offset);
            } else {
                put_status(s, "Cannot promote to level 0");
                return;
            }
        }
        offset = mkd_next_heading(s, offset, MKD_LEVEL_MAX, &level1);
        if (level1 <= level)
            break;
    }
}

static void do_mkd_move_subtree(EditState *s, int dir)
{
    int offset, offset1, offset2, level, level1, level2, size;
    EditBuffer *b1;

    if (check_read_only(s))
        return;

    if (!mkd_is_header_line(s, s->offset)) {
        put_status(s, "Not on header line");
        return;
    }

    offset = mkd_find_heading(s, s->offset, &level, 0);
    if (offset < 0)
        return;

    offset1 = mkd_next_heading(s, offset, level, &level1);
    size = offset1 - offset;

    if (dir < 0) {
        offset2 = mkd_prev_heading(s, offset, level, &level2);
        if (level2 < level) {
            put_status(s, "Cannot move substree");
            return;
        }
    } else {
        if (offset1 == s->b->total_size || level1 < level) {
            put_status(s, "Cannot move substree");
            return;
        }
        offset2 = mkd_next_heading(s, offset1, level, &level2);
    }
    b1 = eb_new("*tmp*", BF_SYSTEM | (s->b->flags & BF_STYLES));
    eb_set_charset(b1, s->b->charset, s->b->eol_type);
    eb_insert_buffer_convert(b1, 0, s->b, offset, size);
    eb_delete(s->b, offset, size);
    if (offset2 > offset)
        offset2 -= size;
    eb_insert_buffer_convert(s->b, offset2, b1, 0, b1->total_size);
    eb_free(&b1);
    s->offset = offset2;
}

static void do_mkd_meta_return(EditState *s)
{
    do_mkd_insert_heading(s, 0);
}

static void do_mkd_metaleft(EditState *s)
{
    if (mkd_is_header_line(s, s->offset))
        do_mkd_promote(s, +1);
    else
        do_word_right(s, -1);
}

static void do_mkd_metaright(EditState *s)
{
    if (mkd_is_header_line(s, s->offset))
        do_mkd_promote(s, -1);
    else
        do_word_right(s, +1);
}

static void do_mkd_metadown(EditState *s)
{
    do_mkd_move_subtree(s, +1);
}

static void do_mkd_metaup(EditState *s)
{
    do_mkd_move_subtree(s, -1);
}

/* Mkd mode specific commands */
static CmdDef mkd_commands[] = {
    /* Motion */
    CMD2( KEY_CTRLC(KEY_CTRL('n')), KEY_NONE,   /* C-c C-n */
          "mkd-next-visible-heading",
          do_outline_next_vsible_heading, ES, "")
    CMD2( KEY_CTRLC(KEY_CTRL('p')), KEY_NONE,   /* C-c C-p */
          "mkd-previous-visible-heading",
          do_outline_previous_vsible_heading, ES, "")
    CMD2( KEY_CTRLC(KEY_CTRL('u')), KEY_NONE,   /* C-c C-u */
          "mkd-up-heading", do_outline_up_heading, ES, "")
    CMD2( KEY_CTRLC(KEY_CTRL('b')), KEY_NONE,   /* C-c C-b */
          "mkd-backward-same-level", do_mkd_backward_same_level, ES, "")
    CMD2( KEY_CTRLC(KEY_CTRL('f')), KEY_NONE,   /* C-c C-f */
          "mkd-forward-same-level", do_mkd_forward_same_level, ES, "")
    CMD2( KEY_CTRLC(KEY_CTRL('j')), KEY_NONE,   /* C-c C-j */
          "mkd-goto", do_mkd_goto, ESs,
          "s{select location to jump to: }[mkdjump]|mkdjump|")
    CMD3( KEY_META('h'), KEY_NONE,   /* M-h */
          "mkd-mark-element", do_mkd_mark_element, ESi, 0, "v")
    CMD3( KEY_CTRLC('@'), KEY_NONE,   /* C-c @ */
          "mkd-mark-subtree", do_mkd_mark_element, ESi, 1, "v")
    /* Editing */
    CMD3( KEY_NONE, KEY_NONE,    /* indirect through M-RET */
          "mkd-insert-heading", do_mkd_insert_heading, ESi, 0, "*v")
    CMD3( KEY_CTRL('j'), KEY_NONE,    /* actually C-RET */
          "mkd-insert-heading-respect-content", do_mkd_insert_heading, ESi, 2, "*v")
    CMD3( KEY_NONE, KEY_NONE,
          "mkd-do-demote", do_mkd_promote, ESi, -1, "*v")
    CMD3( KEY_NONE, KEY_NONE,
          "mkd-do-promote", do_mkd_promote, ESi, +1, "*v")
    CMD3( KEY_CTRLX('>'), KEY_NONE,    /* actually M-S-right | C-c C-x R */
          "mkd-demote-subtree", do_mkd_promote_subtree, ESi, -1, "*v")
    CMD3( KEY_CTRLX('<'), KEY_NONE,    /* actually M-S-left | C-c C-x L */
          "mkd-promote-subtree", do_mkd_promote_subtree, ESi, +1, "*v")
    CMD3( KEY_NONE, KEY_NONE,
          "mkd-move-subtree-down", do_mkd_move_subtree, ESi, +1, "*v")
    CMD3( KEY_NONE, KEY_NONE,
          "mkd-move-subtree-up", do_mkd_move_subtree, ESi, -1, "*v")
    CMD2( KEY_META(KEY_RET), KEY_NONE,    /* Actually M-RET | C-c C-x m */
          "mkd-meta-return", do_mkd_meta_return, ES, "*")
    CMD2( KEY_ESC, KEY_LEFT,    /* actually M-left | C-c C-x l */
          "mkd-metaleft", do_mkd_metaleft, ES, "")
    CMD2( KEY_ESC, KEY_RIGHT,    /* actually M-right | C-c C-x r */
          "mkd-metaright", do_mkd_metaright, ES, "")
    CMD2( KEY_ESC, KEY_DOWN,    /* actually M-down | C-c C-x d */
          "mkd-metadown", do_mkd_metadown, ES, "")
    CMD2( KEY_ESC, KEY_UP,    /* actually M-up | C-c C-x u */
          "mkd-metaup", do_mkd_metaup, ES, "")
    CMD_DEF_END,
};

static int mkd_mode_init(EditState *s, EditBuffer *b, int flags)
{
    if (s) {
        s->b->tab_width = 4;
        s->indent_tabs_mode = 0;
        /* XXX: should come from mode.default_wrap */
        s->wrap = WRAP_WORD;
    }
    return 0;
}

static ModeDef mkd_mode = {
    .name = "markdown",
    .extensions = "mkd|md|markdown",
    .mode_init = mkd_mode_init,
    .colorize_func = mkd_colorize_line,
};

static int litcoffee_mode_init(EditState *s, EditBuffer *b, int flags)
{
    if (s) {
        s->b->tab_width = 4;
        s->indent_tabs_mode = 0;
        /* XXX: should come from mode.default_wrap */
        s->wrap = WRAP_WORD;
        s->mode->colorize_flags = mkd_add_lang("coffee", 0);
    }
    return 0;
}

static ModeDef litcoffee_mode = {
    .name = "LitCoffee",
    .extensions = "litcoffee",
    .mode_init = litcoffee_mode_init,
    .colorize_func = mkd_colorize_line,
};

static int mkd_init(void)
{
    qe_register_mode(&mkd_mode, MODEF_SYNTAX);
    qe_register_cmd_table(mkd_commands, &mkd_mode);
    qe_register_mode(&litcoffee_mode, MODEF_SYNTAX);
    qe_register_cmd_table(mkd_commands, &litcoffee_mode);

    return 0;
}

qe_module_init(mkd_init);
