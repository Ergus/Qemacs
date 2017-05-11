/*
 * QEmacs, tiny but powerful multimode editor
 *
 * Copyright (c) 2000-2001 Fabrice Bellard.
 * Copyright (c) 2000-2017 Charlie Gordon.
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

/*
 * default qemacs configuration
 */
static CmdDef basic_commands[] = {

    /*---------------- Simple commands ----------------*/

    /* Character insertion */

    CMD2( KEY_DEFAULT, KEY_NONE,
          "self-insert-command", do_char, ESii,
          "*" "kiui")
    CMD2( KEY_META('#'), KEY_NONE,
          "insert-char", do_char, ESii,
          "*" "i{Insert char: }|charvalue|" "ui")
    /* do_tab will not change read only buffer */
    CMD2( KEY_CTRL('i'), KEY_NONE,
          "tabulate", do_tab, ESi, "ui")
    //CMD2( KEY_SPC, KEY_NONE, "space", do_space, "*ui")
    CMD2( KEY_CTRL('q'), KEY_NONE,
          "quoted-insert", do_quote, ESi, "*ui")
    CMD3( KEY_CTRL('j'), KEY_RET,
          "newline", do_return, ESi, 1, "*v")
    CMD3( KEY_CTRL('o'), KEY_NONE,
          "open-line", do_return, ESi, 0, "*v")

    CMD2( KEY_INSERT, KEY_NONE,
          "overwrite-mode", do_overwrite_mode, ESi, "ui")
    CMD3( KEY_NONE, KEY_NONE,
          "insert-mode", do_overwrite_mode, ESi, 0, "v")

#ifdef CONFIG_UNICODE_JOIN
    /* Insert combining accent: combine with letter if possible */
    CMD3( KEY_META('`'), KEY_NONE,
          "combine-grave-accent", do_combine_char, ESi, 0x300, "*v")
    CMD3( KEY_META('\''), KEY_NONE,
          "combine-acute-accent", do_combine_char, ESi, 0x301, "*v")
    CMD3( KEY_META('^'), KEY_NONE,
          "combine-circumflex-accent", do_combine_char, ESi, 0x302, "*v")
    CMD3( KEY_META('"'), KEY_NONE,
          "combine-diaeresis", do_combine_char, ESi, 0x308, "*v")
    CMD3( KEY_META('~'), KEY_NONE,
          "combine-tilde", do_combine_char, ESi, 0x303, "*v")
#endif

    /* Moving around */

    CMD1( KEY_CTRL('p'), KEY_UP,
          "previous-line", do_up_down, -1 )
    CMD1( KEY_CTRL('n'), KEY_DOWN,
          "next-line", do_up_down, 1 )
    CMD1( KEY_CTRL('b'), KEY_LEFT,
          "backward-char", do_left_right, -1 )
    CMD1( KEY_CTRL('f'), KEY_RIGHT,
          "forward-char", do_left_right, 1 )
    CMD1( KEY_META('b'), KEY_CTRL_LEFT,
          "backward-word", do_word_right, -1 )
    CMD1( KEY_META('f'), KEY_CTRL_RIGHT,
          "forward-word", do_word_right, 1 )
    CMD1( KEY_META('v'), KEY_PAGEUP,
          "scroll-down", do_scroll_up_down, -2 ) /* u? */
    CMD1( KEY_CTRL('v'), KEY_PAGEDOWN,
          "scroll-up", do_scroll_up_down, 2 ) /* u? */
    CMD1( KEY_META('z'), KEY_NONE,
          "scroll-down-one", do_scroll_up_down, -1 ) /* u? */
    CMD1( KEY_CTRL('z'), KEY_NONE,
          "scroll-up-one", do_scroll_up_down, 1 ) /* u? */
    CMD0( KEY_CTRL('a'), KEY_HOME,
          "beginning-of-line", do_bol)
    CMD0( KEY_CTRL('e'), KEY_END,
          "end-of-line", do_eol)
    CMD0( KEY_META('<'), KEY_CTRL_HOME,
          "beginning-of-buffer", do_bof )
    CMD0( KEY_META('>'), KEY_CTRL_END,
          "end-of-buffer", do_eof )

    /*---------------- Region handling / Kill commands ----------------*/

    /* deletion commands should be allowed in read only buffers,
     * they should merely copy the data to the kill ring */
    CMD2( KEY_CTRL('d'), KEY_DELETE,
          "delete-char", do_delete_char, ESi, "*ui")
    CMD2( 127, KEY_NONE,
          "backward-delete-char", do_backspace, ESi, "*ui")
    CMD0( KEY_CTRL('@'), KEY_NONE,
          "set-mark-command", do_set_mark)
    CMD0( KEY_CTRLX(KEY_CTRL('x')), KEY_NONE,
          "exchange-point-and-mark", do_exchange_point_and_mark)
    CMD0( KEY_CTRLX('h'), KEY_NONE,
          "mark-whole-buffer", do_mark_whole_buffer)
    CMD0( KEY_META(KEY_CTRL('w')), KEY_NONE,
          "append-next-kill", do_append_next_kill)
    CMD2( KEY_CTRL('k'), KEY_NONE,
          "kill-line", do_kill_line, ESi, "ui" )
    CMD2( KEY_NONE, KEY_NONE,
          "kill-beginning-of-line", do_kill_beginning_of_line, ESi, "ui" )
    CMD3( KEY_META(KEY_DEL), KEY_META(KEY_BS),
          "backward-kill-word", do_kill_word, ESi, -1, "v" )
    CMD3( KEY_META('d'), KEY_NONE,
          "kill-word", do_kill_word, ESi, 1, "v" )
    CMD1( KEY_CTRL('w'), KEY_NONE,
          "kill-region", do_kill_region, 0 )
    CMD1( KEY_META('w'), KEY_NONE,
          "copy-region", do_kill_region, 1 )
    CMD2( KEY_CTRL('y'), KEY_NONE,
          "yank", do_yank, ES, "*")
    CMD2( KEY_META('y'), KEY_NONE,
          "yank-pop", do_yank_pop, ES, "*")

    /*---------------- Buffer and file handling ----------------*/

    CMD3( KEY_CTRLX(KEY_CTRL('f')), KEY_NONE,
          "find-file", do_find_file, ESsi, 0,
          "s{Find file: }[file]|file|"
          "v") /* u? */
    CMD3( KEY_CTRL('x'), KEY_META('f'),
          "find-file-other-window", do_find_file_other_window, ESsi, 0,
          "s{Find file: }[file]|file|"
          "v") /* u? */
    CMD3( KEY_CTRLX(KEY_CTRL('v')), KEY_NONE,
          "find-alternate-file", do_find_alternate_file, ESsi, 0,
          "s{Find alternate file: }[file]|file|"
          "v") /* u? */
    CMD3( KEY_NONE, KEY_NONE,
          "find-file-noselect", do_find_file_noselect, ESsi, 0,
          "s{Find file: }[file]|file|"
          "v") /* u? */
    CMD2( KEY_CTRLX('i'), KEY_NONE,
          "insert-file", do_insert_file, ESs,
          "*s{Insert file: }[file]|file|") /* u? */
    CMD0( KEY_CTRLX(KEY_CTRL('s')), KEY_NONE,
          "save-buffer", do_save_buffer) /* u? */
    CMD2( KEY_CTRLX(KEY_CTRL('w')), KEY_NONE,
          "write-file", do_write_file, ESs,
          "s{Write file: }[file]|file|") /* u? */
    CMD2( KEY_CTRLX('w'), KEY_NONE,
          "write-region", do_write_region, ESs,
          "s{Write region to file: }[file]|file|") /* u? */
    CMD2( KEY_CTRLX('b'), KEY_NONE,
          "switch-to-buffer", do_switch_to_buffer, ESs,
          "s{Switch to buffer: }[buffer]|buffer|")
    CMD3( KEY_CTRLX('k'), KEY_NONE,
          "kill-buffer", do_kill_buffer, ESsi, 0,
          "s{Kill buffer: }[buffer]|buffer|"
          "v")
    CMD0( KEY_CTRLX(KEY_CTRL('q')), KEY_CTRLC('~'),
          "toggle-read-only", do_toggle_read_only)
    CMD2( KEY_META('~'), KEY_NONE,
          "not-modified", do_not_modified, ESi, "ui")
    CMD2( KEY_NONE, KEY_NONE,
          "set-visited-file-name", do_set_visited_file_name, ESss,
          "s{Set visited file name: }[file]|file|"
          "s{Rename file? }|newname|")

    /*---------------- Paragraph / case handling ----------------*/

    CMD0( KEY_META('h'), KEY_NONE,
          "mark-paragraph", do_mark_paragraph)
    CMD0( KEY_META('{'), KEY_CTRL_UP,  /* KEY_META('[') */
          "backward-paragraph", do_backward_paragraph)
    CMD0( KEY_META('}'), KEY_CTRL_DOWN, /* KEY_META(']') */
          "forward-paragraph", do_forward_paragraph)
    CMD2( KEY_META('q'), KEY_NONE,
          "fill-paragraph", do_fill_paragraph, ES, "*")
    CMD3( KEY_NONE, KEY_NONE,
          "kill-paragraph", do_kill_paragraph, ESi, 1, "v")

    CMD3( KEY_META('c'), KEY_NONE,
          "capitalize-word", do_changecase_word, ESi, 2, "*v")
    CMD3( KEY_META('l'), KEY_NONE,
          "downcase-word", do_changecase_word, ESi, -1, "*v")
    CMD3( KEY_META('u'), KEY_NONE,
          "upcase-word", do_changecase_word, ESi, 1, "*v")
    CMD3( KEY_META(KEY_CTRL('c')), KEY_NONE,
          "capitalize-region", do_changecase_region, ESi, 2, "*v")
    CMD3( KEY_CTRLX(KEY_CTRL('l')), KEY_NONE,
          "downcase-region", do_changecase_region, ESi, -1, "*v")
    CMD3( KEY_CTRLX(KEY_CTRL('u')), KEY_NONE,
          "upcase-region", do_changecase_region, ESi, 1, "*v")

    /*---------------- Command handling ----------------*/

    CMD2( KEY_META('x'), KEY_NONE,
          "execute-command", do_execute_command, ESsi,
          "s{Command: }[command]|command|"
          "ui")
    /* M-0 thru M-9 also start numeric argument */
    CMD0( KEY_CTRL('u'), KEY_META('-'),
          "numeric-argument", do_numeric_argument)
    CMD0( KEY_CTRL('g'), KEY_CTRLX(KEY_CTRL('g')),
          "abort", do_break)
    CMD0( KEY_CTRLX('('), KEY_NONE,
          "start-kbd-macro", do_start_macro)
    CMD0( KEY_CTRLX(')'), KEY_NONE,
          "end-kbd-macro", do_end_macro)
    CMD0( KEY_CTRLX('e'), KEY_CTRL('\\'),
          "call-last-kbd-macro", do_call_macro)
    CMD2( KEY_NONE, KEY_NONE,
          "define-kbd-macro", do_define_kbd_macro, ESsss,
          "s{Macro name: }[command]"
          "s{Macro keys: }|macrokeys|"
          "s{Bind to key: }[key]")
    /* set/unset key? */
    CMD3( KEY_NONE, KEY_NONE,
          "global-set-key", do_set_key, ESssi, 0,
          "s{Set key globally: }[key]"
          "s{command: }[command]|command|"
          "v")
    CMD3( KEY_NONE, KEY_NONE,
          "local-set-key", do_set_key, ESssi, 1,
          "s{Set key locally: }[key]"
          "s{command: }[command]|command|"
          "v")

    /*---------------- Window handling ----------------*/

    /* should merge these functions */
    CMD0( KEY_CTRLX('o'), KEY_NONE,
          "other-window", do_other_window)
    CMD0( KEY_CTRLX('n'), KEY_NONE,
          "next-window", do_other_window)
    CMD0( KEY_CTRLX('p'), KEY_NONE,
          "previous-window", do_previous_window)
#ifndef CONFIG_TINY
    CMD1( KEY_META(KEY_CTRL('l')), KEY_NONE,
          "center-cursor", do_center_cursor, 1)
    CMD1( KEY_CTRL('x'), KEY_UP,
          "find-window-up", do_find_window, KEY_UP)
    CMD1( KEY_CTRL('x'), KEY_DOWN,
          "find-window-down", do_find_window, KEY_DOWN)
    CMD1( KEY_CTRL('x'), KEY_LEFT,
          "find-window-left", do_find_window, KEY_LEFT)
    CMD1( KEY_CTRL('x'), KEY_RIGHT,
          "find-window-right", do_find_window, KEY_RIGHT)
    CMD1( KEY_META('('), KEY_NONE,
          "scroll-left", do_scroll_left_right, -1)
    CMD1( KEY_META(')'), KEY_NONE,
          "scroll-right", do_scroll_left_right, 1)
    CMD1( KEY_NONE, KEY_NONE,
          "preview-mode", do_preview_mode, 1)
#endif
    CMD1( KEY_CTRLX('0'), KEY_NONE,
          "delete-window", do_delete_window, 0)
    CMD1( KEY_CTRLX('1'), KEY_NONE,
          "delete-other-windows", do_delete_other_windows, 0)
    CMD1( KEY_NONE, KEY_NONE,
          "delete-all-windows", do_delete_other_windows, 1)
    CMD1( KEY_NONE, KEY_NONE,
          "hide-window", do_hide_window, 1)
    CMD0( KEY_NONE, KEY_NONE,
          "delete-hidden-windows", do_delete_hidden_windows)
    CMD3( KEY_CTRLX('2'), KEY_NONE,
          "split-window-vertically", do_split_window, ESii,
          SW_STACKED, "uiv")
    CMD3( KEY_CTRLX('3'), KEY_NONE,
          "split-window-horizontally", do_split_window, ESii,
          SW_SIDE_BY_SIDE, "uiv")
    CMD0( KEY_CTRLC('f'), KEY_NONE,
          "toggle-full-screen", do_toggle_full_screen)
    CMD0( KEY_CTRLC('m'), KEY_NONE,
          "toggle-mode-line", do_toggle_mode_line)
    CMD2( KEY_NONE, KEY_NONE,
          "create-window", do_create_window, ESss,
          "s{Filename: }[file]|file|"
          "s{Layout: }|layout|")

    /*---------------- Help ----------------*/

    CMD0( KEY_CTRLH('d'), KEY_NONE,
          "start-trace-mode", do_start_trace_mode)
    CMD2( KEY_CTRLH('t'), KEY_NONE,
          "set-trace-options", do_set_trace_options, ESs,
          "s{Trace options: }|trace|")
    CMD0( KEY_CTRLH('c'), KEY_CTRLH('k'),
          "describe-key-briefly", do_describe_key_briefly)
    CMD0( KEY_CTRLH(KEY_CTRL('h')), KEY_F1,
          "help-for-help", do_help_for_help)

    /*---------------- International ----------------*/

    CMD2( KEY_CTRLXRET('f'), KEY_CTRLC('c'),
          "set-buffer-file-coding-system", do_set_buffer_file_coding_system, ESs,
          "s{Charset: }[charset]|charset|")
    CMD2( KEY_NONE, KEY_NONE,
          "convert-buffer-file-coding-system", do_convert_buffer_file_coding_system, ESs,
          "*" "s{Charset: }[charset]|charset|")
    CMD0( KEY_CTRLXRET('b'), KEY_CTRLC('b'),
          "toggle-bidir", do_toggle_bidir)
    CMD2( KEY_CTRLXRET(KEY_CTRL('\\')), KEY_CTRLC(KEY_CTRL('\\')),
          "set-input-method", do_set_input_method, ESs,
          "s{Input method: }[input]")
    CMD0( KEY_CTRLX(KEY_CTRL('\\')), KEY_NONE,
          "switch-input-method", do_switch_input_method)

    /*---------------- Styles & display ----------------*/

    CMD2( KEY_NONE, KEY_NONE,
          "define-color", do_define_color, ESss,
          "s{Color name: }[color]|color|"
          "s{Color value: }[color]|color|")
    CMD2( KEY_NONE, KEY_NONE,
          "set-style", do_set_style, ESsss,
          "s{Style: }[style]|style|"
          "s{CSS Property Name: }[style-property]|style-property|"
          "s{CSS Property Value: }|value|")
    CMD2( KEY_NONE, KEY_NONE,
          "set-display-size", do_set_display_size, ESii,
          "i{Width: }|width|"
          "i{Height: }|height|")
    CMD2( KEY_NONE, KEY_NONE,
          "set-system-font", do_set_system_font, ESss,
          "s{Font family: }|fontfamily|"
          "s{System fonts: }|fontnames|")
    CMD2( KEY_NONE, KEY_NONE,
          "set-window-style", do_set_window_style, ESs,
          "s{Style: }[style]|style|")

    /*---------------- Miscellaneous ----------------*/

    CMD2( KEY_CTRLX(KEY_CTRL('c')), KEY_NONE,
          "exit-qemacs", do_exit_qemacs, ESi, "ui")
    CMD0( KEY_CTRL('l'), KEY_NONE,
          "refresh", do_refresh_complete)
    CMD0( KEY_CTRLX('u'), KEY_CTRL('_'),
          "undo", do_undo)
    CMD0( KEY_CTRLX('r'), KEY_CTRLX(KEY_CTRL('_')),
          "redo", do_redo)
    CMD3( KEY_META('g'), KEY_NONE,
          "goto-line", do_goto, ESsi, 'l',
          "us{Goto line: }" "v")
    CMD3( KEY_CTRLX('g'), KEY_NONE,
          "goto-char", do_goto, ESsi, 'c',
          "us{Goto char: }" "v")
    CMD0( KEY_CTRLX('l'), KEY_NONE,
          "count-lines", do_count_lines)
    CMD0( KEY_CTRLX('='), KEY_NONE,
          "what-cursor-position", do_what_cursor_position)

    /* non standard mappings */
    CMD0( KEY_CTRLXRET('l'), KEY_CTRLC('l'),
          "toggle-line-numbers", do_toggle_line_numbers)
    CMD0( KEY_CTRLXRET('t'), KEY_CTRLC('t'),
          "toggle-truncate-lines", do_toggle_truncate_lines)
    CMD0( KEY_CTRLXRET('w'), KEY_CTRLC('w'),
          "word-wrap", do_word_wrap)
    CMD1( KEY_NONE, KEY_NONE,
          "toggle-control-h", do_toggle_control_h, 0)
    CMD2( KEY_NONE, KEY_NONE,
          "set-emulation", do_set_emulation, ESs,
          "s{Emulation mode: }|emulation|")
    CMD2( KEY_NONE, KEY_NONE,
          "cd", do_cd, ESs,
          "s{Change default directory: }[file]|file|")
    CMD2( KEY_NONE, KEY_NONE,
          "set-mode", do_set_mode, ESs,
          "s{Set mode: }[mode]")
    CMD1( KEY_NONE, KEY_NONE,
          "set-auto-coding", do_set_auto_coding, 1)
    CMD1( KEY_NONE, KEY_NONE,
          "set-auto-mode", do_set_next_mode, 0)
    CMD1( KEY_META('m'), KEY_NONE,
          "set-next-mode", do_set_next_mode, 1)
    CMD1( KEY_NONE, KEY_NONE,
          "set-previous-mode", do_set_next_mode, -1)

    /* tab & indent */
    CMD2( KEY_NONE, KEY_NONE,
          "set-tab-width", do_set_tab_width, ESi,
          "ui{Tab width: }")
    CMD2( KEY_NONE, KEY_NONE,
          "set-indent-width", do_set_indent_width, ESi,
          "ui{Indent width: }")
    CMD2( KEY_NONE, KEY_NONE,
          "set-indent-tabs-mode", do_set_indent_tabs_mode, ESi,
          "ui{Indent tabs mode (0 or 1): }")
    CMD2( KEY_NONE, KEY_NONE,
          "set-fill-column", do_set_fill_column, ESi,
          "ui{Fill column: }")

    /* other stuff */
    CMD3( KEY_CTRLC(KEY_CTRL('f')), KEY_NONE,
          "load-file-from-path", do_load_file_from_path, ESsi, 0,
          "s{Load file from path: }|file|"
          "v")
    CMD2( KEY_NONE, KEY_NONE,
          "load-config-file", do_load_config_file, ESs,
          "s{Configuration file: }[file]|file|")
    CMD2( KEY_NONE, KEY_NONE,
          "load-qerc", do_load_qerc, ESs,
          "s{path: }[file]|file|")
    CMD2( KEY_NONE, KEY_NONE,
          "add-resource-path", do_add_resource_path, ESs,
          "s{resource path: }[file]|file|")


    CMD_DEF_END,
};

CmdDef minibuffer_commands[] = {
    CMD2( KEY_DEFAULT, KEY_NONE,
          "minibuffer-insert", do_minibuffer_char, ESii,
          "*" "kiui")
    CMD1( KEY_RET, KEY_NONE,
          "minibuffer-exit", do_minibuffer_exit, 0)
    CMD1( KEY_CTRL('g'), KEY_CTRLX(KEY_CTRL('g')),
          "minibuffer-abort", do_minibuffer_exit, 1)
    CMD1( KEY_CTRL('i'), KEY_NONE,
          "minibuffer-complete", do_minibuffer_complete, COMPLETION_TAB)
    /* should take numeric prefix to specify word size */
    CMD0( KEY_META('='), KEY_NONE,
          "minibuffer-get-binary", do_minibuffer_get_binary)
    CMD0( ' ', KEY_NONE,
          "minibuffer-complete-space", do_minibuffer_complete_space)
    CMD1( KEY_CTRL('p'), KEY_UP,
          "minibuffer-previous-history-element", do_minibuffer_history, -1)
    CMD1( KEY_CTRL('n'), KEY_DOWN,
          "minibuffer-next-history-element", do_minibuffer_history, 1)
    CMD3( '/', KEY_NONE,
          "minibuffer-electric-slash", do_minibuffer_electric, ESi, '/', "*v")
    CMD3( '~', KEY_NONE,
          "minibuffer-electric-tilde", do_minibuffer_electric, ESi, '~', "*v")
    CMD_DEF_END,
};

CmdDef popup_commands[] = {
    CMD0( 'q', KEY_CTRL('g'),
          "popup-exit", do_popup_exit)
    CMD3( '/', KEY_NONE,
          "popup-isearch", do_isearch, ESii, 1, "vui" )
    CMD_DEF_END,
};


QEStyleDef qe_styles[QE_STYLE_NB] = {

#define STYLE_DEF(constant, name, fg_color, bg_color, \
                  font_style, font_size) \
{ name, fg_color, bg_color, font_style, font_size },

#include "qestyles.h"

#undef STYLE_DEF
};
