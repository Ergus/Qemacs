/*
 * Shell script mode for QEmacs.
 *
 * Copyright (c) 2000-2008 Charlie Gordon.
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

static const char *script_mode_extensions = "sh|bash|zsh";

/*---------------- Perl colors ----------------*/

enum {
    SCRIPT_TEXT =       QE_STYLE_DEFAULT,
    SCRIPT_COMMENT =    QE_STYLE_COMMENT,
    SCRIPT_PREPROCESS = QE_STYLE_PREPROCESS,
    SCRIPT_COMMAND =    QE_STYLE_FUNCTION,
    SCRIPT_VARIABLE =   QE_STYLE_TYPE,
    SCRIPT_STRING =     QE_STYLE_STRING,
    SCRIPT_BACKTICK =   QE_STYLE_STRING_Q,
};

static int script_var(const unsigned int *str, int j, int n)
{
    for (; j < n; j++) {
        if (qe_isalnum_(str[j]) || str[j] == '-')
            continue;
        break;
    }
    return j;
}

static void script_colorize_line(unsigned int *str, int n, int *statep,
                                 __unused__ int state_only)
{
    int i = 0, j, style;

    style = SCRIPT_COMMAND;

    while (i < n) {
        switch (str[i]) {
        case '#':
            if (i > 0 && str[i - 1] == '$')
                break;
            style = SCRIPT_COMMENT;
            if (str[i + 1] == '!')
                style = SCRIPT_PREPROCESS;
            set_color(str + i, str + n, style);
            i = n;
            continue;
        case '`':
            style = SCRIPT_BACKTICK;
            goto has_string;
        case '\'':
        case '"':
            style = SCRIPT_STRING;
        has_string:
            /* parse string const */
            for (j = i + 1; j < n; j++) {
                if (str[j] == str[i]) {
                    j++;
                    break;
                }
            }
            set_color(str + i, str + j, style);
            i = j;
            continue;
        case ' ':
        case '\t':
            break;
        default:
            j = script_var(str, i, n);
            if (j > i) {
                while (qe_isblank(str[j]))
                    j++;
                if (str[j] == '=')
                    style = SCRIPT_VARIABLE;
                set_color(str + i, str + j, style);
                style = SCRIPT_TEXT;
                i = j;
                continue;
            }
            // Should support << syntax
            // Should support $ syntax
            style = SCRIPT_TEXT;
            break;
        }
        i++;
        continue;
    }
}

static int script_mode_probe(ModeProbeData *p)
{
    if (match_extension(p->filename, script_mode_extensions))
        return 80;

    if (p->buf[0] == '#') {
        if (p->buf[1] == '!')
            return 60;
        if (p->buf[1] == ' ')
            return 30;
    }
    return 0;
}

static int script_mode_init(EditState *s, ModeSavedData *saved_data)
{
    int ret;

    ret = text_mode_init(s, saved_data);
    if (ret)
        return ret;
    set_colorize_func(s, script_colorize_line);
    return ret;
}

/* specific script commands */
static CmdDef script_commands[] = {
    CMD_DEF_END,
};

static ModeDef script_mode;

static int script_init(void)
{
    /* c mode is almost like the text mode, so we copy and patch it */
    memcpy(&script_mode, &text_mode, sizeof(ModeDef));
    script_mode.name = "Shell-script";
    script_mode.mode_probe = script_mode_probe;
    script_mode.mode_init = script_mode_init;

    qe_register_mode(&script_mode);
    qe_register_cmd_table(script_commands, &script_mode);

    return 0;
}

qe_module_init(script_init);
