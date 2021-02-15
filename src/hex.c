/*
 * Hexadecimal modes for QEmacs.
 *
 * Copyright (c) 2000-2001 Fabrice Bellard.
 * Copyright (c) 2002-2019 Charlie Gordon.
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

enum {
    HEX_STYLE_OFFSET = QE_STYLE_COMMENT,
    HEX_STYLE_DUMP   = QE_STYLE_FUNCTION,
};

static ModeDef hex_mode;

static int to_disp(int c)
{
#if 1
    /* Allow characters in range 160-255 to show as graphics */
    if ((c & 127) < ' ' || c == 127)
        c = '.';
#else
    if (c < ' ' || c >= 127)
        c = '.';
#endif
    return c;
}

static int hex_backward_offset(EditState *s, int offset)
{
    return align(offset, s->dump_width);
}

static int hex_display_line(EditState *s, DisplayState *ds, int offset)
{
    int j, len, ateof;
    int offset1, offset2;
    unsigned char b;

    display_bol(ds);

    ds->style = HEX_STYLE_OFFSET;
    display_printf(ds, -1, -1, "%08x ", offset);

    ateof = 0;
    len = s->b->total_size - offset;
    if (len > s->dump_width)
        len = s->dump_width;

    if (s->mode == &hex_mode) {

        ds->style = HEX_STYLE_DUMP;

        for (j = 0; j < s->dump_width; j++) {
            display_char(ds, -1, -1, ' ');
            offset1 = offset + j;
            offset2 = offset1 + 1;
            if (j < len) {
                eb_read(s->b, offset1, &b, 1);
                display_printhex(ds, offset1, offset2, b, 2);
            } else {
                if (!ateof) {
                    ateof = 1;
                } else {
                    offset1 = offset2 = -1;
                }
                ds->cur_hex_mode = s->hex_mode;
                display_printf(ds, offset1, offset2, "  ");
                ds->cur_hex_mode = 0;
            }
            if ((j & 7) == 7)
                display_char(ds, -1, -1, ' ');
        }
        display_char(ds, -1, -1, ' ');
    }
    ds->style = 0;

    display_char(ds, -1, -1, ' ');

    ateof = 0;
    for (j = 0; j < s->dump_width; j++) {
        offset1 = offset + j;
        offset2 = offset1 + 1;
        if (j < len) {
            eb_read(s->b, offset1, &b, 1);
        } else {
            b = ' ';
            if (!ateof) {
                ateof = 1;
            } else {
                offset1 = offset2 = -1;
            }
        }
        display_char(ds, offset1, offset2, to_disp(b));
    }
    display_eol(ds, -1, -1);

    if (len >= s->dump_width)
        return offset + len;
    else
        return -1;
}

static void do_set_width(EditState *s, int w)
{
    if (w >= 1) {
        s->dump_width = w;
        s->offset_top = s->mode->backward_offset(s, s->offset_top);
    }
}

static void do_incr_width(EditState *s, int incr)
{
    int w;
    w = s->dump_width + incr;
    if (w >= 1)
        do_set_width(s, w);
}

static void do_toggle_hex(EditState *s)
{
    s->hex_mode = !s->hex_mode;
}

/* specific hex commands */
static CmdDef hex_commands[] = {
    CMD1( KEY_CTRL_LEFT, KEY_NONE,
          "decrease-width", do_incr_width, -1)
    CMD1( KEY_CTRL_RIGHT, KEY_NONE,
          "increase-width", do_incr_width, 1)
    CMD2( KEY_NONE, KEY_NONE,
          "set-width", do_set_width, ESi,
          "ui{Width: }")
    CMD3( KEY_META('g'), KEY_NONE,
          "goto-byte", do_goto, ESsi, 'b',
          "us{Goto byte: }"
          "v")
    CMD0( KEY_NONE, KEY_NONE,
          "toggle-hex", do_toggle_hex)
    CMD_DEF_END,
};

static int binary_mode_init(EditState *s, EditBuffer *b, int flags)
{
    if (s) {
        int num_width;

        /* get typical number width */
        num_width = get_glyph_width(s->screen, s, QE_STYLE_DEFAULT, '0');

        s->dump_width = s->screen->width / num_width;
        if (s->b->flags & BF_PREVIEW)
            s->dump_width = s->dump_width * 4 / 5;

        s->dump_width -= 10;
        /* align on 16 byte boundary */
        s->dump_width &= ~15;
        if (s->dump_width < 16)
            s->dump_width = 16;
        s->hex_mode = 0;
        s->unihex_mode = 0;
        s->insert = 0;
        /* XXX: should come from mode.default_wrap */
        s->wrap = WRAP_TRUNCATE;
    }
    return 0;
}

static int hex_mode_init(EditState *s, EditBuffer *b, int flags)
{
    if (s) {
        s->dump_width = 16;
        s->hex_mode = 1;
        s->hex_nibble = 0;
        s->unihex_mode = 0;
        s->insert = 0;
        /* XXX: should come from mode.default_wrap */
        s->wrap = WRAP_TRUNCATE;
    }
    return 0;
}

static int detect_binary(const unsigned char *buf, int size)
{
    const uint32_t magic = (1U << '\b') | (1U << '\t') | (1U << '\f') |
                           (1U << '\n') | (1U << '\r') | (1U << '\033') |
                           (1U << 0x0e) | (1U << 0x0f) | (1U << 0x1a) |
                           (1U << 0x1f);
    int i, c;

    for (i = 0; i < size; i++) {
        c = buf[i];
        if (c < 32 && !(magic & (1U << c)))
            return 1;
    }
    return 0;
}

static int hex_mode_probe(ModeDef *mode, ModeProbeData *p)
{
    if (detect_binary(p->buf, p->buf_size))
        return 50;
    else
        return 10;
}

static void hex_move_bol(EditState *s)
{
    s->offset = align(s->offset, s->dump_width);
}

static void hex_move_eol(EditState *s)
{
    int offset = align(s->offset, s->dump_width) + s->dump_width - 1;
    if (offset > s->b->total_size)
        offset = s->b->total_size;
    s->offset = offset;
}

static void hex_move_left_right(EditState *s, int dir)
{
    s->offset += dir;
    if (s->offset < 0)
        s->offset = 0;
    else
    if (s->offset > s->b->total_size)
        s->offset = s->b->total_size;
}

static void hex_move_up_down(EditState *s, int dir)
{
    s->offset += dir * s->dump_width;
    if (s->offset < 0)
        s->offset = 0;
    else
    if (s->offset > s->b->total_size)
        s->offset = s->b->total_size;
}

void hex_write_char(EditState *s, int key)
{
    unsigned int cur_ch, ch;
    int hsize, shift, cur_len, len, h;
    int offset = s->offset;
    char buf[10];

    if (s->hex_mode) {
        if (s->unihex_mode)
            hsize = s->unihex_mode;
        else
            hsize = 2;
        h = qe_digit_value(key);
        if (h >= 16)
            return;
        if ((s->insert || offset >= s->b->total_size) && s->hex_nibble == 0) {
            ch = h << ((hsize - 1) * 4);
            if (s->unihex_mode || s->b->charset->char_size > 1) {
                len = eb_encode_uchar(s->b, buf, ch);
            } else {
                len = 1;
                buf[0] = ch;
            }
            eb_insert(s->b, offset, buf, len);
        } else {
            if (s->unihex_mode) {
                cur_ch = eb_nextc(s->b, offset, &cur_len);
                cur_len -= offset;
            } else {
                eb_read(s->b, offset, buf, 1);
                cur_ch = buf[0];
                cur_len = 1;
            }

            shift = (hsize - s->hex_nibble - 1) * 4;
            ch = (cur_ch & ~(0xf << shift)) | (h << shift);

            if (s->unihex_mode) {
                len = eb_encode_uchar(s->b, buf, ch);
            } else {
                len = 1;
                buf[0] = ch;
            }
#if 1
            eb_replace(s->b, offset, cur_len, buf, len);
#else
            if (cur_len == len) {
                eb_write(s->b, offset, buf, len);
            } else {
                eb_delete(s->b, offset, cur_len);
                eb_insert(s->b, offset, buf, len);
            }
#endif
        }
        s->offset = offset;
        if (++s->hex_nibble == hsize) {
            s->hex_nibble = 0;
            if (offset < s->b->total_size)
                s->offset += len;
        }
    } else {
        text_write_char(s, key);
    }
}

static void hex_mode_line(EditState *s, buf_t *out)
{
    basic_mode_line(s, out, '-');
    buf_printf(out, "0x%x--0x%x", s->offset, s->b->total_size);
    buf_printf(out, "--%d%%", compute_percent(s->offset, s->b->total_size));
}

static int binary_mode_probe(ModeDef *mode, ModeProbeData *p)
{
    return 5;
}

static ModeDef binary_mode = {
    .name = "binary",
    .mode_probe = binary_mode_probe,
    .mode_init = binary_mode_init,
    .display_line = hex_display_line,
    .backward_offset = hex_backward_offset,
    .move_up_down = hex_move_up_down,
    .move_left_right = hex_move_left_right,
    .move_bol = hex_move_bol,
    .move_eol = hex_move_eol,
    .move_bof = text_move_bof,
    .move_eof = text_move_eof,
    .scroll_up_down = text_scroll_up_down,
    .mouse_goto = text_mouse_goto,
    .write_char = text_write_char,
    .get_mode_line = hex_mode_line,
};

static ModeDef hex_mode = {
    .name = "hex",
    .mode_probe = hex_mode_probe,
    .mode_init = hex_mode_init,
    .display_line = hex_display_line,
    .backward_offset = hex_backward_offset,
    .move_up_down = hex_move_up_down,
    .move_left_right = hex_move_left_right,
    .move_bol = hex_move_bol,
    .move_eol = hex_move_eol,
    .move_bof = text_move_bof,
    .move_eof = text_move_eof,
    .scroll_up_down = text_scroll_up_down,
    .mouse_goto = text_mouse_goto,
    .write_char = hex_write_char,
    .get_mode_line = hex_mode_line,
};

static int hex_init(void)
{
    /* first register mode(s) */
    qe_register_mode(&binary_mode, MODEF_VIEW);
    qe_register_mode(&hex_mode, MODEF_VIEW);

    /* commands and default keys */
    qe_register_cmd_table(hex_commands, &hex_mode);
    qe_register_cmd_table(hex_commands, &binary_mode);

    /* additional mode specific keys */
    qe_register_binding(KEY_TAB, "toggle-hex", &hex_mode);
    qe_register_binding(KEY_SHIFT_TAB, "toggle-hex", &hex_mode);

    return 0;
}

qe_module_init(hex_init);
