/*
 * C mode for QEmacs.
 *
 * Copyright (c) 2001-2002 Fabrice Bellard.
 * Copyright (c) 2002-2014 Charlie Gordon.
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

static const char cc_keywords[] = {
    "asm|catch|class|delete|friend|inline|new|operator|"
    "private|protected|public|template|try|this|virtual|throw|"
};

static const char js_keywords[] = {
    "break|case|catch|continue|debugger|default|delete|do|"
    "else|finally|for|function|if|in|instanceof|new|"
    "return|switch|this|throw|try|typeof|var|void|while|with|"
    /* FutureReservedWord */
    "class|const|enum|import|export|extends|super|"
    /* The following tokens are also considered to be
     * FutureReservedWords when parsing strict mode code */
    "implements|interface|let|package|private|protected|"
    "public|static|yield|"
    /* constants */
    "undefined|null|true|false|Infinity|NaN|"
    /* strict mode quasi keywords */
    "eval|arguments|"
};

static const char java_keywords[] = {
    "abstract|boolean|break|byte|case|catch|class|const|continue|"
    "default|do|alse|extends|false|final|finally|for|function|"
    "if|implements|import|in|instanceof|interface|native|new|null|"
    "package|private|protected|public|return|"
    "static|super|switch|synchronized|"
    "this|throw|throws|transient|true|try|var|while|with|"
};

static const char c_keywords[] = {
    "auto|break|case|const|continue|default|do|else|enum|extern|for|goto|"
    "if|inline|register|restrict|return|sizeof|static|struct|switch|"
    "typedef|union|volatile|while|"
};

/* NOTE: 'var' is added for javascript */
static const char c_mode_types[] = {
    "char|double|float|int|long|unsigned|short|signed|void|var|"
    "_Bool|_Complex|_Imaginary|"
};

static const char c_mode_extensions[] = {
    "c|h|C|H|"          /* C language */
    "y|l|lex|"          /* yacc, lex */
    "cc|hh|cpp|hpp|cxx|hxx|CPP|CC|c++|"   /* C++ */
    "m|"                /* Objective-C */
    "e|qe|cs|idl|st|"
    "jav|java|js|json|" /* Java, Javascript, JSon */
    "ec|ecp|"           /* Informix embedded C */
    "pgc|"              /* Postgres embedded C */
    "pcc|"              /* Oracle C++ */
    "cal"               /* GNU Calc */
};

/* grab a C identifier from a uint buf, stripping color.
 * return char count.
 */
static int get_c_identifier(char *buf, int buf_size, unsigned int *p)
{
    unsigned int c;
    int i, j;

    i = j = 0;
    c = p[i];
    if (qe_isalpha_(c & CHAR_MASK)) {
        do {
            if (j < buf_size - 1)
                buf[j++] = c;
            i++;
            c = p[i];
        } while (qe_isalnum_(c & CHAR_MASK));
    }
    buf[j] = '\0';
    return i;
}

enum {
    C_STYLE_DEFAULT    = 0,
    C_STYLE_PREPROCESS = QE_STYLE_PREPROCESS,
    C_STYLE_COMMENT    = QE_STYLE_COMMENT,
    C_STYLE_REGEX      = QE_STYLE_STRING_Q,
    C_STYLE_STRING     = QE_STYLE_STRING,
    C_STYLE_STRING_Q   = QE_STYLE_STRING_Q,
    C_STYLE_NUMBER     = QE_STYLE_NUMBER,
    C_STYLE_KEYWORD    = QE_STYLE_KEYWORD,
    C_STYLE_TYPE       = QE_STYLE_TYPE,
    C_STYLE_FUNCTION   = QE_STYLE_FUNCTION,
    C_STYLE_VARIABLE   = QE_STYLE_VARIABLE,
};

/* c-mode colorization states */
enum {
    IN_C_COMMENT    = 0x01   /* multiline comment */,
    IN_C_COMMENT1   = 0x02   /* single line comment with \ at EOL */,
    IN_C_STRING     = 0x04   /* double quoted string */,
    IN_C_STRING_Q   = 0x08   /* single quoted string */,
    IN_C_PREPROCESS = 0x10   /* preprocessor directive with \ at EOL */,
    IN_C_REGEX      = 0x20   /* regex */,
    IN_C_CHARCLASS  = 0x40   /* regex char class */,
};

void c_colorize_line(QEColorizeContext *cp,
                     unsigned int *str, int n, int mode_flags)
{
    int i = 0, start, i1, i2, indent;
    int c, state, style, style1, type_decl, klen, delim;
    char kbuf[32];

    for (indent = 0; qe_isspace(str[indent]); indent++)
        continue;

    state = cp->colorize_state;
    start = i;
    type_decl = 0;

    if (i >= n)
        goto the_end;

    c = 0;      /* turn off stupid egcs-2.91.66 warning */
    style = C_STYLE_DEFAULT;

    if (state) {
        /* if already in a state, go directly in the code parsing it */
        if (state & IN_C_PREPROCESS)
            style = C_STYLE_PREPROCESS;
        if (state & IN_C_COMMENT)
            goto parse_comment;
        if (state & IN_C_COMMENT1)
            goto parse_comment1;
        if (state & IN_C_STRING)
            goto parse_string;
        if (state & IN_C_STRING_Q)
            goto parse_string_q;
        if (state & IN_C_REGEX)
            goto parse_regex;
    }

    while (i < n) {
        start = i;
        c = str[i++];

        switch (c) {
        case '/':
            if (str[i] == '*') {
                /* normal comment */
                i++;
            parse_comment:
                state |= IN_C_COMMENT;
                for (; i < n; i++) {
                    if (str[i] == '*' && str[i + 1] == '/') {
                        i += 2;
                        state &= ~IN_C_COMMENT;
                        break;
                    }
                }
                SET_COLOR(str, start, i, C_STYLE_COMMENT);
                continue;
            } else
            if (str[i] == '/') {
                /* line comment */
            parse_comment1:
                state |= IN_C_COMMENT1;
                i = n;
                SET_COLOR(str, start, i, C_STYLE_COMMENT);
                continue;
            }
            /* XXX: should use more context to tell regex from divide */
            if ((mode_flags & CLANG_REGEX)
            &&  (start == indent
            ||   (str[i] != ' ' && str[i] != '='
            &&    !qe_isalnum(str[start - 1] & CHAR_MASK)
            &&    str[start - 1] != ')'))) {
                /* parse regex */
                state = IN_C_REGEX;
            parse_regex:
                while (i < n) {
                    c = str[i++];
                    if (c == '\\') {
                        if (i < n) {
                            i += 1;
                        }
                    } else
                    if (state & IN_C_CHARCLASS) {
                        if (c == ']') {
                            state &= ~IN_C_CHARCLASS;
                        }
                        /* ECMA 5: ignore '/' inside char classes */
                    } else {
                        if (c == '[') {
                            state |= IN_C_CHARCLASS;
                        } else
                        if (c == '/') {
                            while (qe_isalnum_(str[i])) {
                                i++;
                            }
                            state = 0;
                            break;
                        }
                    }
                }
                SET_COLOR(str, start, i, C_STYLE_REGEX);
                continue;
            }
            break;
        case '#':       /* preprocessor */
            /* XXX: C only */
            state = IN_C_PREPROCESS;
            style = C_STYLE_PREPROCESS;
            break;
        case 'L':       /* wide character and string literals */
            /* XXX: C only */
            if (str[i] == '\'') {
                i++;
                goto parse_string_q;
            }
            if (str[i] == '\"') {
                i++;
                goto parse_string;
            }
            goto normal;
        case '\'':      /* character constant */
        parse_string_q:
            state |= IN_C_STRING_Q;
            style1 = C_STYLE_STRING_Q;
            delim = '\'';
            goto string;
        case '\"':      /* string literal */
        parse_string:
            state |= IN_C_STRING;
            style1 = C_STYLE_STRING;
            delim = '\"';
        string:
            while (i < n) {
                c = str[i++];
                if (c == '\\') {
                    if (i >= n)
                        break;
                    i++;
                } else
                if (c == delim) {
                    state &= ~(IN_C_STRING | IN_C_STRING_Q);
                    break;
                }
            }
            if (state & IN_C_PREPROCESS)
                style1 = C_STYLE_PREPROCESS;
            SET_COLOR(str, start, i, style1);
            continue;
        case '=':
            /* exit type declaration */
            /* does not handle this: int i = 1, j = 2; */
            type_decl = 0;
            break;
        case '<':       /* JavaScript extension */
            /* XXX: js only */
            if (str[i] == '!' && str[i + 1] == '-' && str[i + 2] == '-')
                goto parse_comment1;
            break;
        default:
        normal:
            if (state & IN_C_PREPROCESS)
                break;
            if (qe_isdigit(c)) {
                while (qe_isalnum(str[i]) || str[i] == '.') {
                    i++;
                }
                SET_COLOR(str, start, i, C_STYLE_NUMBER);
                continue;
            }
            if (qe_isalpha_(c)) {
                /* XXX: should support :: and $ */
                klen = get_c_identifier(kbuf, countof(kbuf), str + start);
                i = start + klen;

                if (((mode_flags & (CLANG_C|CLANG_CPP|CLANG_OBJC)) && strfind(c_keywords, kbuf))
                ||  ((mode_flags & CLANG_CPP) && strfind(cc_keywords, kbuf))
                ||  ((mode_flags & CLANG_JS) && strfind(js_keywords, kbuf))
                ||  ((mode_flags & CLANG_JAVA) && strfind(java_keywords, kbuf))
                   ) {
                    SET_COLOR(str, start, i, C_STYLE_KEYWORD);
                    continue;
                }

                i1 = i;
                while (qe_isblank(str[i1]))
                    i1++;
                i2 = i1;
                while (str[i2] == '*' || qe_isblank(str[i2]))
                    i2++;

                /* XXX: should check type depending on flavor */
                if (strfind(c_mode_types, kbuf) || strend(kbuf, "_t", NULL)) {
                    /* c type */
                    /* if not cast, assume type declaration */
                    if (str[i2] != ')') {
                        type_decl = 1;
                    }
                    SET_COLOR(str, start, i, C_STYLE_TYPE);
                    continue;
                }

                if (str[i] == '(' || (str[i] == ' ' && str[i + 1] == '(')) {
                    /* function call */
                    /* XXX: different styles for call and definition */
                    SET_COLOR(str, start, i, C_STYLE_FUNCTION);
                    continue;
                }
                /* assume typedef if starting at first column */
                if (start == 0)
                    type_decl = 1;

                if (type_decl) {
                    if (start == 0) {
                        /* assume type if first column */
                        SET_COLOR(str, start, i, C_STYLE_TYPE);
                    } else {
                        SET_COLOR(str, start, i, C_STYLE_VARIABLE);
                    }
                }
                continue;
            }
            break;
        }
        SET_COLOR1(str, start, style);
    }
 the_end:
    /* strip state if not overflowing from a comment */
    if (!(state & IN_C_COMMENT) && n > 0 && ((str[n - 1] & CHAR_MASK) != '\\'))
        state &= ~(IN_C_COMMENT1 | IN_C_PREPROCESS);
    cp->colorize_state = state;
}

#define MAX_STACK_SIZE  64

/* gives the position of the first non white space character in
   buf. TABs are counted correctly */
static int find_indent1(EditState *s, unsigned int *buf)
{
    unsigned int *p;
    int pos, c;

    p = buf;
    pos = 0;
    for (;;) {
        c = *p++ & CHAR_MASK;
        if (c == '\t')
            pos += s->b->tab_width - (pos % s->b->tab_width);
        else if (c == ' ')
            pos++;
        else
            break;
    }
    return pos;
}

static int find_pos(EditState *s, unsigned int *buf, int size)
{
    int pos, c, i;

    pos = 0;
    for (i = 0; i < size; i++) {
        c = buf[i] & CHAR_MASK;
        if (c == '\t') {
            pos += s->b->tab_width - (pos % s->b->tab_width);
        } else {
            /* simplistic case: assume single width characters */
            pos++;
        }
    }
    return pos;
}

enum {
    INDENT_NORM,
    INDENT_FIND_EQ,
};

/* Check if indentation is already what it should be */
static int check_indent(EditState *s, int offset, int i, int *offset_ptr)
{
    int tw, col, ntabs, nspaces, bad;
    int offset1;

    tw = s->b->tab_width > 0 ? s->b->tab_width : 8;
    col = ntabs = nspaces = bad = 0;

    for (;;) {
        int c = eb_nextc(s->b, offset1 = offset, &offset);
        if (c == '\t') {
            col += tw - col % tw;
            bad |= nspaces;
            ntabs += 1;
        } else
        if (c == ' ') {
            col += 1;
            nspaces += 1;
        } else {
            break;
        }
    }

    *offset_ptr = offset1;

    if (col != i || bad)
        return 0;

    /* check tabs */
    if (s->indent_tabs_mode) {
        return (nspaces >= tw) ? 0 : 1;
    } else {
        return (ntabs > 0) ? 0 : 1;
    }
}

/* Insert n spaces at beginning of line at <offset>.
 * Store new offset after indentation to <*offset_ptr>.
 * Tabs are inserted if s->indent_tabs_mode is true.
 */
static void insert_indent(EditState *s, int offset, int i, int *offset_ptr)
{
    /* insert tabs */
    if (s->indent_tabs_mode) {
        int tw = s->b->tab_width > 0 ? s->b->tab_width : 8;
        while (i >= tw) {
            offset += eb_insert_uchar(s->b, offset, '\t');
            i -= tw;
        }
    }

    /* insert needed spaces */
    offset += eb_insert_spaces(s->b, offset, i);

    *offset_ptr = offset;
}

/* indent a line of C code starting at <offset> */
static void c_indent_line(EditState *s, int offset0)
{
    int offset, offset1, offsetl, c, pos, line_num, col_num;
    int i, j, eoi_found, len, pos1, lpos, style, line_num1, state;
    unsigned int buf[COLORED_MAX_LINE_SIZE], *p;
    unsigned char stack[MAX_STACK_SIZE];
    char buf1[64], *q;
    int stack_ptr;

    /* find start of line */
    eb_get_pos(s->b, &line_num, &col_num, offset0);
    line_num1 = line_num;
    offset = eb_goto_bol(s->b, offset0);
    /* now find previous lines and compute indent */
    pos = 0;
    lpos = -1; /* position of the last instruction start */
    offsetl = offset;
    eoi_found = 0;
    stack_ptr = 0;
    state = INDENT_NORM;
    for (;;) {
        if (offsetl == 0)
            break;
        line_num--;
        offsetl = eb_prev_line(s->b, offsetl);
        offset1 = offsetl;
        len = s->get_colorized_line(s, buf, countof(buf), &offset1, line_num);
        /* store indent position */
        pos1 = find_indent1(s, buf);
        p = buf + len;
        while (p > buf) {
            p--;
            c = *p;
            /* skip strings or comments */
            style = c >> STYLE_SHIFT;
            if (style == C_STYLE_COMMENT
            ||  style == C_STYLE_STRING
            ||  style == C_STYLE_PREPROCESS) {
                continue;
            }
            c = c & CHAR_MASK;
            if (state == INDENT_FIND_EQ) {
                /* special case to search '=' or ; before { to know if
                   we are in data definition */
                if (c == '=') {
                    /* data definition case */
                    pos = lpos;
                    goto end_parse;
                } else if (c == ';') {
                    /* normal instruction case */
                    goto check_instr;
                }
            } else {
                switch (c) {
                case '}':
                    if (stack_ptr >= MAX_STACK_SIZE)
                        return;
                    stack[stack_ptr++] = c;
                    goto check_instr;
                case '{':
                    if (stack_ptr == 0) {
                        if (lpos == -1) {
                            pos = pos1 + s->indent_size;
                            eoi_found = 1;
                            goto end_parse;
                        } else {
                            state = INDENT_FIND_EQ;
                        }
                    } else {
			if (stack[--stack_ptr] != '}') {
			    /* XXX: syntax check ? */
			    goto check_instr;
			}
			goto check_instr;
                    }
                    break;
                case ')':
                case ']':
                    if (stack_ptr >= MAX_STACK_SIZE)
                        return;
                    stack[stack_ptr++] = c;
                    break;
                case '(':
                case '[':
                    if (stack_ptr == 0) {
                        pos = find_pos(s, buf, p - buf) + 1;
                        goto end_parse;
                    } else {
			if (stack[--stack_ptr] != (c == '(' ? ')' : ']')) {
			    /* XXX: syntax check ? */
			}
                    }
                    break;
                case ' ':
                case '\t':
                case '\n':
                    break;
                case ';':
                    /* level test needed for 'for(;;)' */
                    if (stack_ptr == 0) {
                        /* ; { or } are found before an instruction */
                    check_instr:
                        if (lpos >= 0) {
                            /* start of instruction already found */
                            pos = lpos;
                            if (!eoi_found)
                                pos += s->indent_size;
                            goto end_parse;
                        }
                        eoi_found = 1;
                    }
                    break;
                case ':':
                    /* a label line is ignored */
                    /* XXX: incorrect */
                    if (style == C_STYLE_DEFAULT)
                        goto prev_line;
                    break;
                default:
                    if (stack_ptr == 0) {
                        if (style == C_STYLE_KEYWORD) {
                            unsigned int *p1, *p2;
                            /* special case for if/for/while */
                            p1 = p;
                            while (p > buf &&
                                   (p[-1] >> STYLE_SHIFT) == C_STYLE_KEYWORD)
                                p--;
                            p2 = p;
                            q = buf1;
                            while (q < buf1 + countof(buf1) - 1 && p2 <= p1) {
                                *q++ = *p2++ & CHAR_MASK;
                            }
                            *q = '\0';

                            if (!eoi_found && strfind("if|for|while", buf1)) {
                                pos = pos1 + s->indent_size;
                                goto end_parse;
                            }
                        }
                        lpos = pos1;
                    }
                    break;
                }
            }
        }
    prev_line: ;
    }
  end_parse:
    /* compute special cases which depend on the chars on the current line */
    offset1 = offset;
    len = s->get_colorized_line(s, buf, countof(buf), &offset1, line_num1);

    if (stack_ptr == 0) {
        if (!pos && lpos >= 0) {
            /* start of instruction already found */
            pos = lpos;
            if (!eoi_found)
                pos += s->indent_size;
        }
    }

    for (i = 0; i < len; i++) {
        c = buf[i];
        style = c >> STYLE_SHIFT;
        if (qe_isblank(c & CHAR_MASK))
            continue;
        /* if preprocess, no indent */
        if (style == C_STYLE_PREPROCESS) {
            pos = 0;
            break;
        }
        if (qe_isalpha_(c & CHAR_MASK)) {
            j = get_c_identifier(buf1, countof(buf1), buf + i);

            if (style == C_STYLE_KEYWORD) {
                if (strfind("case|default", buf1))
                    goto unindent;
            }
            for (j += i; qe_isblank(buf[j] & CHAR_MASK); j++)
                continue;
            if (buf[j] == ':')
                goto unindent;
        }
        /* NOTE: strings & comments are correctly ignored there */
        if ((c == '&' || c == '|') && buf[i + 1] == c)
            goto unindent;

        if (c == '}') {
        unindent:
            pos -= s->indent_size;
            if (pos < 0)
                pos = 0;
            break;
        }
        if (c == '{' && pos == s->indent_size && !eoi_found) {
            pos = 0;
            break;
        }
        break;
    }

    /* the computed indent is in 'pos' */
    /* if on a blank line, reset indent to 0 unless point is on it */
    if (eb_is_blank_line(s->b, offset, &offset1)
    &&  !(s->offset >= offset && s->offset < offset1)) {
        pos = 0;
    }
    /* Do not modify buffer if indentation in correct */
    if (!check_indent(s, offset, pos, &offset1)) {
        /* simple approach to normalization of indentation */
        eb_delete_range(s->b, offset, offset1);
        insert_indent(s, offset, pos, &offset1);
    }
    /* move to the indentation if point was in indent space */
    if (s->offset >= offset && s->offset < offset1) {
        s->offset = offset1;
    }
}

static void do_c_indent(EditState *s)
{
    if (eb_is_in_indentation(s->b, s->offset)
    &&  s->qe_state->last_cmd_func != (CmdFunc)do_c_indent) {
        c_indent_line(s, s->offset);
    } else {
        do_tab(s, 1);
    }
}

static void do_c_electric(EditState *s, int key)
{
    int offset = s->offset;

    do_char(s, key, 1);
    /* reindent line at original point */
    if (s->mode->indent_func)
        (s->mode->indent_func)(s, eb_goto_bol(s->b, offset));
}

static void do_c_return(EditState *s)
{
    int offset = s->offset;

    do_return(s, 1);
    /* reindent line to remove indent on blank line */
    if (s->mode->indent_func) {
        (s->mode->indent_func)(s, eb_goto_bol(s->b, offset));
        if (s->mode->auto_indent)
            (s->mode->indent_func)(s, s->offset);
    }
}

static int ustr_match_mask(const unsigned int *buf, const char *str)
{
    while (*str) {
        if ((*buf++ & CHAR_MASK) != *str++)
            return 0;
    }
    return 1;
}

/* forward / backward preprocessor */
static void do_c_forward_conditional(EditState *s, int dir)
{
    unsigned int buf[COLORED_MAX_LINE_SIZE], *p;
    int line_num, col_num, len, sharp, level;
    int offset, offset0, offset1;

    offset = offset0 = eb_goto_bol(s->b, s->offset);
    eb_get_pos(s->b, &line_num, &col_num, offset);
    level = 0;
    for (;;) {
        offset1 = offset;
        len = s->get_colorized_line(s, buf, countof(buf), &offset1, line_num);
        sharp = 0;
        for (p = buf; *p; p++) {
            int c = (*p & CHAR_MASK);
            int style = (*p >> STYLE_SHIFT);
            if (qe_isblank(c))
                continue;
            if (c == '#' && style == C_STYLE_PREPROCESS)
                sharp++;
            else
                break;
        }
        if (sharp == 1) {
            if (ustr_match_mask(p, dir < 0 ? "endif" : "if")) {
                if (level || offset == offset0)
                    level++;
                else
                    break;
            } else
            if (ustr_match_mask(p, "el")) {
                if (offset == offset0)
                    level++;
                else
                if (level <= 1)
                    break;
            } else
            if (ustr_match_mask(p, dir > 0 ? "endif" : "if")) {
                if (level)
                    level--;
                if (!level && offset != offset0)
                    break;
            }
        }
        if (dir > 0) {
            line_num++;
            offset = offset1;
            if (offset >= s->b->total_size)
                break;
        } else {
            if (offset <= 0)
                break;
            line_num--;
            offset = eb_prev_line(s->b, offset);
        }
    }
    s->offset = offset;
}

static void do_c_list_conditionals(EditState *s)
{
    unsigned int buf[COLORED_MAX_LINE_SIZE], *p;
    int line_num, col_num, len, sharp, level;
    int offset, offset1;
    EditBuffer *b;

    b = eb_scratch("Preprocessor conditionals", BF_UTF8);
    if (!b)
        return;

    offset = eb_goto_bol(s->b, s->offset);
    eb_get_pos(s->b, &line_num, &col_num, offset);
    level = 0;
    while (offset > 0) {
        line_num--;
        offset = eb_prev_line(s->b, offset);
        offset1 = offset;
        len = s->get_colorized_line(s, buf, countof(buf), &offset1, line_num);
        sharp = 0;
        for (p = buf; *p; p++) {
            int c = (*p & CHAR_MASK);
            int style = (*p >> STYLE_SHIFT);
            if (qe_isblank(c))
                continue;
            if (c == '#' && style == C_STYLE_PREPROCESS)
                sharp++;
            else
                break;
        }
        if (sharp == 1) {
            if (ustr_match_mask(p, "endif")) {
                level++;
            } else
            if (ustr_match_mask(p, "el")) {
                if (level == 0) {
                    eb_insert_buffer_convert(b, 0, s->b, offset, offset1 - offset);
                }
            } else
            if (ustr_match_mask(p, "if")) {
                if (level) {
                    level--;
                } else {
                    eb_insert_buffer_convert(b, 0, s->b, offset, offset1 - offset);
                }
            }
        }
    }
    if (b->total_size > 0) {
        show_popup(b);
    } else {
        eb_free(&b);
        put_status(s, "Not in a #if conditional");
    }
}

static int c_mode_probe(ModeDef *mode, ModeProbeData *p)
{
    /* trust the file extension */
    if (match_extension(p->filename, mode->extensions))
        return 80;

    /* weaker match on C comment start */
    if (p->buf[0] == '/' && p->buf[1] == '*')
        return 60;

    /* even weaker match on C++ comment start */
    if (p->buf[0] == '/' && p->buf[1] == '/')
        return 50;

    if (p->buf[0] == '#') {
        if (p->buf[1] == '!'
        &&  memstr(p->buf, p->line_len, "bin/calc")) {
            /* GNU Calc script */
            return 80;
        }
        /* same for file starting with '#include' */
        if (strstart(cs8(p->buf), "#include", NULL))
            return 50;

        /* same for file starting with '#pragma' as in #pragma once */
        if (strstart(cs8(p->buf), "#pragma", NULL))
            return 50;
    }

    return 1;
}

static int c_mode_init(EditState *s, ModeSavedData *saved_data)
{
    text_mode.mode_init(s, saved_data);

    /* Select C like flavor */
    if (match_extension(s->b->filename, "c|h|C|H")) {
        s->mode_flags = CLANG_C;
    } else
    if (match_extension(s->b->filename, "cc|hh|cpp|hpp|cxx|hxx|CPP|CC|c++")) {
        s->mode_name = "CPP";
        s->mode_flags = CLANG_CPP;
    } else
    if (match_extension(s->b->filename, "m")) {
        s->mode_name = "ObjC";
        s->mode_flags = CLANG_OBJC;
    } else
    if (match_extension(s->b->filename, "js|json")) {
        s->mode_name = "Javascript";
        s->mode_flags = CLANG_JS | CLANG_REGEX;
    } else
    if (match_extension(s->b->filename, "st")) {
        s->mode_name = "Syntax";
        s->mode_flags = CLANG_C | CLANG_REGEX;
    } else
    if (match_extension(s->b->filename, "jav|java")) {
        s->mode_name = "Java";
        s->mode_flags = CLANG_JAVA;
    } else
    if (match_extension(s->b->filename, "l|lex")) {
        s->mode_name = "Lex";
        s->mode_flags = CLANG_C | CLANG_LEX;
    } else
    if (match_extension(s->b->filename, "y")) {
        s->mode_name = "Yacc";
        s->mode_flags = CLANG_C | CLANG_YACC;
    }
    return 0;
}

/* C mode specific commands */
static CmdDef c_commands[] = {
    CMD2( KEY_CTRL('i'), KEY_NONE,
          "c-indent-command", do_c_indent, ES, "*")
            /* should map to KEY_META + KEY_CTRL_LEFT ? */
    CMD3( KEY_META('['), KEY_NONE,
          "c-backward-conditional", do_c_forward_conditional, ESi, -1, "*v")
    CMD3( KEY_META(']'), KEY_NONE,
          "c-forward-conditional", do_c_forward_conditional, ESi, 1, "*v")
    CMD2( KEY_META('i'), KEY_NONE,
          "c-list-conditionals", do_c_list_conditionals, ES, "")
    CMD2( '{', '}',
          "c-electric-key", do_c_electric, ESi, "*ki")
    CMD2( KEY_RET, KEY_NONE,
          "c-newline", do_c_return, ES, "*v")
    CMD_DEF_END,
};

static ModeDef c_mode;

static int c_init(void)
{
    const char *p;

    /* C mode is almost like the text mode, so we copy and patch it */
    memcpy(&c_mode, &text_mode, sizeof(ModeDef));
    c_mode.name = "C";
    c_mode.extensions = c_mode_extensions;
    c_mode.mode_probe = c_mode_probe;
    c_mode.mode_init = c_mode_init;
    c_mode.colorize_func = c_colorize_line;
    c_mode.indent_func = c_indent_line;
    c_mode.auto_indent = 1;

    qe_register_mode(&c_mode);
    qe_register_cmd_table(c_commands, &c_mode);
    for (p = ";:#&|"; *p; p++) {
        qe_register_binding(*p, "c-electric-key", &c_mode);
    }
    return 0;
}

qe_module_init(c_init);
