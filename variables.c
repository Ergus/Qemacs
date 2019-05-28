/*
 * Module for handling variables in QEmacs
 *
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

#include "qe.h"
#include "variables.h"

const char * const var_domain[] = {
    "global",   /* VAR_GLOBAL */
    "state",    /* VAR_STATE */
    "buffer",   /* VAR_BUFFER */
    "window",   /* VAR_WINDOW */
    "mode",     /* VAR_MODE */
    "self",     /* VAR_SELF */
};

static VarDef var_table[] = {

    S_VAR( "screen-width", width, VAR_NUMBER, VAR_RO )
    S_VAR( "screen-height", height, VAR_NUMBER, VAR_RO )
    S_VAR( "is-full-screen", is_full_screen, VAR_NUMBER, VAR_RO )
    S_VAR( "flag-split-window-change-focus", flag_split_window_change_focus, VAR_NUMBER, VAR_RW_SAVE )
    S_VAR( "backspace-is-control-h", backspace_is_control_h, VAR_NUMBER, VAR_RW_SAVE )
    S_VAR( "ungot-key", ungot_key, VAR_NUMBER, VAR_RW )
    S_VAR( "QEPATH", res_path, VAR_CHARS, VAR_RO )
    //S_VAR( "it", it, VAR_NUMBER, VAR_RW )
    S_VAR( "ignore-spaces", ignore_spaces, VAR_NUMBER, VAR_RW_SAVE )
    S_VAR( "ignore-comments", ignore_comments, VAR_NUMBER, VAR_RW_SAVE )
    S_VAR( "hilite-region", hilite_region, VAR_NUMBER, VAR_RW_SAVE )
    S_VAR( "mmap-threshold", mmap_threshold, VAR_NUMBER, VAR_RW_SAVE )
    S_VAR( "max-load-size", max_load_size, VAR_NUMBER, VAR_RW_SAVE )
    S_VAR( "show-unicode", show_unicode, VAR_NUMBER, VAR_RW_SAVE )
    S_VAR( "default-tab-width", default_tab_width, VAR_NUMBER, VAR_RW_SAVE )
    S_VAR( "default-fill-column", default_fill_column, VAR_NUMBER, VAR_RW_SAVE )
    S_VAR( "backup-inhibited", backup_inhibited, VAR_NUMBER, VAR_RW_SAVE )
    S_VAR( "fuzzy-search", fuzzy_search, VAR_NUMBER, VAR_RW_SAVE )
    S_VAR( "c-label-indent", c_label_indent, VAR_NUMBER, VAR_RW_SAVE )

    //B_VAR( "screen-charset", charset, VAR_NUMBER, VAR_RW )

    B_VAR( "mark", mark, VAR_NUMBER, VAR_RW )
    B_VAR( "bufsize", total_size, VAR_NUMBER, VAR_RO )
    B_VAR( "bufname", name, VAR_CHARS, VAR_RO )
    B_VAR( "filename", filename, VAR_CHARS, VAR_RO )
    B_VAR( "tab-width", tab_width, VAR_NUMBER, VAR_RW )
    B_VAR( "fill-column", fill_column, VAR_NUMBER, VAR_RW )

    W_VAR( "point", offset, VAR_NUMBER, VAR_RW )
    W_VAR( "indent-width", indent_size, VAR_NUMBER, VAR_RW )
    W_VAR( "indent-tabs-mode", indent_tabs_mode, VAR_NUMBER, VAR_RW )
    W_VAR( "default-style", default_style, VAR_NUMBER, VAR_RW )
    W_VAR( "region-style", region_style, VAR_NUMBER, VAR_RW )
    W_VAR( "curline-style", curline_style, VAR_NUMBER, VAR_RW )
    W_VAR( "window-width", width, VAR_NUMBER, VAR_RW )
    W_VAR( "window-height", height, VAR_NUMBER, VAR_RW )
    W_VAR( "window-left", xleft, VAR_NUMBER, VAR_RW )
    W_VAR( "window-top", ytop, VAR_NUMBER, VAR_RW )
    W_VAR( "window-prompt", prompt, VAR_STRING, VAR_RW )
    W_VAR( "dump-width", dump_width, VAR_NUMBER, VAR_RW )

    M_VAR( "mode-name", name, VAR_STRING, VAR_RO )
    M_VAR( "auto-indent", auto_indent, VAR_NUMBER, VAR_RW )

    G_VAR( "use-session-file", use_session_file, VAR_NUMBER, VAR_RW )
    G_VAR( "force-tty", force_tty, VAR_NUMBER, VAR_RW )
    G_VAR( "use-html", use_html, VAR_NUMBER, VAR_RW )

    /* more buffer fields: modified, readonly, binary, charset */

    /* more window fields: mode_line, color, input_method...
     */

    //G_VAR( "text-mode-line", text_mode.mode_line, VAR_STRING, VAR_RW )
    //G_VAR( "binary-mode-line", binary_mode.mode_line, VAR_STRING, VAR_RW )
    //G_VAR( "hex-mode-line", hex_mode.mode_line, VAR_STRING, VAR_RW )
    //G_VAR( "unicode-mode-line", unihex_mode.mode_line, VAR_STRING, VAR_RW )

    //Dispatch these to the appropriate modules
    //G_VAR( "c-mode-extensions", c_mode.extensions, VAR_STRING, VAR_RW )
    //G_VAR( "c-mode-keywords", c_mode.keywords, VAR_STRING, VAR_RW )
    //G_VAR( "c-mode-types", c_mode.types, VAR_STRING, VAR_RW )
    //G_VAR( "html-src-mode-extensions", htmlsrc_mode.extensions, VAR_STRING, VAR_RW )
    //G_VAR( "html-mode-extensions", html_mode.extensions, VAR_STRING, VAR_RW )
    //G_VAR( "perl-mode-extensions", perl_mode.extensions, VAR_STRING, VAR_RW )
};

VarDef *qe_find_variable(const char *name)
{
    QEmacsState *qs = &qe_state;
    VarDef *vp;

    for (vp = qs->first_variable; vp; vp = vp->next) {
        if (strequal(vp->name, name))
              return vp;
    }
    /* Should have a list of local variables for buffer/window/mode
     * instances
     */
    return NULL;
}

void qe_complete_variable(CompleteState *cp)
{
    QEmacsState *qs = cp->s->qe_state;
    const VarDef *vp;

    for (vp = qs->first_variable; vp; vp = vp->next) {
        complete_test(cp, vp->name);
    }
}

QVarType qe_get_variable(EditState *s, const char *name,
                         char *buf, int size, int *pnum, int as_source)
{
    const VarDef *vp;
    int num = 0;
    const char *str = NULL;
    const void *ptr;

    vp = qe_find_variable(name);
    if (!vp) {
        /* Should enumerate user variables ? */

        /* Try environment */
        str = getenv(name);
        if (as_source)
            strquote(buf, size, str, -1);
        else
            pstrcpy(buf, size, str ? str : "");
        return str ? VAR_STRING : VAR_UNKNOWN;
    }
    switch (vp->domain) {
    case VAR_SELF:
        ptr = &vp->value;
        break;
    case VAR_GLOBAL:
        ptr = vp->value.ptr;
        break;
    case VAR_STATE:
        ptr = (const u8*)s->qe_state + vp->value.offset;
        break;
    case VAR_BUFFER:
        ptr = (const u8*)s->b + vp->value.offset;
        break;
    case VAR_WINDOW:
        ptr = (const u8*)s + vp->value.offset;
        break;
    case VAR_MODE:
        ptr = (const u8*)s->mode + vp->value.offset;
        break;
    default:
        if (size > 0)
            *buf = '\0';
        return VAR_UNKNOWN;
    }

    switch (vp->type) {
    case VAR_STRING:
        str = *(const char**)ptr;
        if (as_source)
            strquote(buf, size, str, -1);
        else
            pstrcpy(buf, size, str ? str : "");
        break;
    case VAR_CHARS:
        str = (const char*)ptr;
        if (as_source)
            strquote(buf, size, str, -1);
        else
            pstrcpy(buf, size, str);
        break;
    case VAR_NUMBER:
        num = *(const int*)ptr;
        if (pnum)
            *pnum = num;
        else
            snprintf(buf, size, "%d", num);
        break;
    default:
        if (size > 0)
            *buf = '\0';
        return VAR_UNKNOWN;
    }
    return vp->type;
}

/* Ugly kludge to check for allocated data: pointers above end are
 * assumed to be allocated with qe_malloc
 */
extern u8 end[];
#ifdef CONFIG_DARWIN
/* XXX: not really at the end, but should be beyond initialized data */
/* XXX: should remove this hack */
u8 end[8];
#endif

static QVarType qe_generic_set_variable(EditState *s, VarDef *vp, void *ptr,
                                        const char *value, int num)
{
    char buf[32];
    char **pstr;

    switch (vp->type) {
    case VAR_STRING:
        if (!value) {
            snprintf(buf, sizeof(buf), "%d", num);
            value = buf;
        }
        if (!strequal(ptr, value)) {
            pstr = (char **)ptr;
            if ((u8 *)*pstr > end)
                qe_free(pstr);
            *pstr = qe_strdup(value);
            vp->modified = 1;
        }
        break;
    case VAR_CHARS:
        if (!value) {
            snprintf(buf, sizeof(buf), "%d", num);
            value = buf;
        }
        if (!strequal(ptr, value)) {
            pstrcpy(ptr, vp->size, value);
            vp->modified = 1;
        }
        break;
    case VAR_NUMBER:
        if (!value) {
            if (*(int*)ptr != num) {
                *(int*)ptr = num;
                vp->modified = 1;
            }
        } else {
            /* XXX: should have default, min and max values */
            return VAR_INVALID;
        }
        break;
    default:
        return VAR_UNKNOWN;
    }
    return vp->type;
}

QVarType qe_set_variable(EditState *s, const char *name,
                         const char *value, int num)
{
    void *ptr;
    VarDef *vp;

    vp = qe_find_variable(name);
    if (!vp) {
        /* Create user variable (global/buffer/window/mode?) */
        vp = qe_mallocz(VarDef);
        vp->name = qe_strdup(name);
        vp->modified = 1;
        vp->domain = VAR_SELF;
        vp->rw = VAR_RW_SAVE;
        if (value) {
            vp->value.str = qe_strdup(value);
            vp->type = VAR_STRING;
        } else {
            vp->value.num = num;
            vp->type = VAR_NUMBER;
        }
        qe_register_variables(vp, 1);
        return vp->type;
    } else
    if (vp->rw == VAR_RO) {
        return VAR_READONLY;
    } else {
        switch (vp->domain) {
        case VAR_SELF:
            ptr = &vp->value;
            break;
        case VAR_GLOBAL:
            ptr = vp->value.ptr;
            break;
        case VAR_STATE:
            ptr = (u8*)s->qe_state + vp->value.offset;
            break;
        case VAR_BUFFER:
            ptr = (u8*)s->b + vp->value.offset;
            break;
        case VAR_WINDOW:
            ptr = (u8*)s + vp->value.offset;
            break;
        case VAR_MODE:
            ptr = (u8*)s->mode + vp->value.offset;
            break;
        default:
            return VAR_UNKNOWN;
        }
        if (vp->type == VAR_NUMBER && value) {
            char *p;
            num = strtol(value, &p, 0);
            if (!*p)
                value = NULL;
        }
        return vp->set_value(s, vp, ptr, value, num);
    }
}

void do_show_variable(EditState *s, const char *name)
{
    char buf[MAX_FILENAME_SIZE];

    if (qe_get_variable(s, name, buf, sizeof(buf), NULL, 1) == VAR_UNKNOWN)
        put_status(s, "No variable %s", name);
    else
        put_status(s, "%s -> %s", name, buf);
}

void do_set_variable(EditState *s, const char *name, const char *value)
{
    switch (qe_set_variable(s, name, value, 0)) {
    case VAR_UNKNOWN:
        put_status(s, "Variable %s is invalid", name);
        break;
    case VAR_READONLY:
        put_status(s, "Variable %s is read-only", name);
        break;
    case VAR_INVALID:
        put_status(s, "Invalid value for variable %s: %s", name, value);
        break;
    default:
        do_show_variable(s, name);
        break;
    }
}

void qe_register_variables(VarDef *vars, int count)
{
    QEmacsState *qs = &qe_state;
    VarDef *vp;

    for (vp = vars; vp < vars + count; vp++) {
        if (!vp->set_value)
            vp->set_value = qe_generic_set_variable;
        vp->next = vp + 1;
    }
    vp[-1].next = qs->first_variable;
    qs->first_variable = vars;
}

/* should register this as help function */
void qe_list_variables(EditState *s, EditBuffer *b)
{
    QEmacsState *qs = s->qe_state;
    char buf[MAX_FILENAME_SIZE];
    char typebuf[32];
    const char *type;
    const VarDef *vp;

    eb_puts(b, "\n  variables:\n\n");
    for (vp = qs->first_variable; vp; vp = vp->next) {
        switch (vp->type) {
        case VAR_NUMBER:
            type = "int";
            break;
        case VAR_STRING:
            type = "string";
            break;
        case VAR_CHARS:
            type = typebuf;
            snprintf(typebuf, sizeof(typebuf), "char[%d]", vp->size);
            break;
        default:
            type = "var";
            break;
        }
        qe_get_variable(s, vp->name, buf, sizeof(buf), NULL, 1);
        eb_printf(b, "    %s %s %s%s -> %s\n",
                  var_domain[vp->domain], type,
                  vp->rw ? "" : "read-only ",
                  vp->name, buf);
    }
}

void qe_save_variables(EditState *s, EditBuffer *b)
{
    QEmacsState *qs = s->qe_state;
    char buf[MAX_FILENAME_SIZE];
    char varname[32], *p;
    const VarDef *vp;

    eb_puts(b, "// variables:\n");
    /* Only save customized variables */
    for (vp = qs->first_variable; vp; vp = vp->next) {
        if (vp->rw != VAR_RW_SAVE || !vp->modified)
            continue;
        pstrcpy(varname, countof(varname), vp->name);
        for (p = varname; *p; p++) {
            if (*p == '-')
                *p = '_';
        }
        qe_get_variable(s, vp->name, buf, sizeof(buf), NULL, 1);
        eb_printf(b, "%s = %s;\n", varname, buf);
    }
    eb_putc(b, '\n');
}

/*---------------- commands ----------------*/

static CmdDef var_commands[] = {
    CMD2( KEY_NONE, KEY_NONE,
          "show-variable", do_show_variable, ESs,
          "s{Show variable: }[var]|var|")
    CMD2( KEY_F8, KEY_NONE,
          "set-variable", do_set_variable, ESss,
          "s{Set variable: }[var]|var|s{to value: }|value|")

    CMD_DEF_END,
};

static int vars_init(void)
{
    qe_register_variables(var_table, countof(var_table));
    qe_register_cmd_table(var_commands, NULL);
    register_completion("var", qe_complete_variable);
    return 0;
}

qe_module_init(vars_init);
