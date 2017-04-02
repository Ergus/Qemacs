/*
 * TTY Terminal handling for QEmacs
 *
 * Copyright (c) 2000-2001 Fabrice Bellard.
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

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/time.h>

#include "qe.h"

#if MAX_UNICODE_DISPLAY > 0xFFFF
typedef uint64_t TTYChar;
/* TTY composite style has 8 bit color number for FG and BG plus attribute bits */
#define TTY_STYLE_BITS      32
#define TTYCHAR(ch,fg,bg)   ((uint32_t)(ch) | ((uint64_t)((fg) | ((bg) << 16)) << 32))
#define TTYCHAR2(ch,col)    ((uint32_t)(ch) | ((uint64_t)(col) << 32))
#define TTYCHAR_GETCH(cc)   ((uint32_t)(cc))
#define TTYCHAR_GETCOL(cc)  ((uint32_t)((cc) >> 32))
#define TTYCHAR_GETFG(cc)   ((uint32_t)((cc) >> 32) & 0xFF)
#define TTYCHAR_GETBG(cc)   ((uint32_t)((cc) >> (32 + 16)) & 0xFF)
#define TTYCHAR_DEFAULT     TTYCHAR(' ', 7, 0)
#define TTYCHAR_COMB        0x200000
#define TTYCHAR_BAD         0xFFFD
#define TTYCHAR_NONE        0xFFFFFFFF
#define TTY_BOLD            0x0100
#define TTY_UNDERLINE       0x0200
#define TTY_BLINK           0x0400
#define TTY_ITALIC          0x0800
#define COMB_CACHE_SIZE     2048
#else
typedef uint32_t TTYChar;
/* TTY composite style has 4 bit color number for FG and BG plus attribute bits */
#define TTY_STYLE_BITS      16
#define TTYCHAR(ch,fg,bg)   ((uint32_t)(ch) | ((uint32_t)((fg) | ((bg) << 12)) << 16))
#define TTYCHAR2(ch,col)    ((uint32_t)(ch) | ((uint32_t)(col) << 16))
#define TTYCHAR_GETCH(cc)   ((cc) & 0xFFFF)
#define TTYCHAR_GETCOL(cc)  (((cc) >> 16) & 0xFFFF)
#define TTYCHAR_GETFG(cc)   (((cc) >> 16) & 0xFF)
#define TTYCHAR_GETBG(cc)   (((cc) >> (16 + 8)) & 0xFF)
#define TTYCHAR_DEFAULT     TTYCHAR(' ', 7, 0)
#define TTYCHAR_BAD         0xFFFD
#define TTYCHAR_NONE        0xFFFF
#define TTY_BOLD            0x0100
#define TTY_UNDERLINE       0x0200
#define TTY_BLINK           0x0400
#define TTY_ITALIC          0x0800
#define COMB_CACHE_SIZE     1
#endif

#if defined(CONFIG_UNLOCKIO)
#  define TTY_PUTC(c,f)         putc_unlocked(c, f)
#  define TTY_FWRITE(b,s,n,f)   fwrite_unlocked(b, s, n, f)
#  define TTY_FPRINTF           fprintf
static inline void TTY_FPUTS(const char *s, FILE *fp) {
    TTY_FWRITE(s, 1, strlen(s), fp);
}
#else
#  define TTY_PUTC(c,f)         putc(c, f)
#  define TTY_FWRITE(b,s,n,f)   fwrite(b, s, n, f)
#  define TTY_FPRINTF           fprintf
#  define TTY_FPUTS             fputs
#endif

enum InputState {
    IS_NORM,
    IS_ESC,
    IS_CSI,
    IS_CSI2,
    IS_ESC2,
};

enum TermCode {
    TERM_UNKNOWN = 0,
    TERM_ANSI,
    TERM_VT100,
    TERM_XTERM,
    TERM_LINUX,
    TERM_CYGWIN,
};

typedef struct TTYState {
    TTYChar *screen;
    int screen_size;
    unsigned char *line_updated;
    struct termios oldtty;
    int cursor_x, cursor_y;
    /* input handling */
    enum InputState input_state;
    int input_param, input_param2;
    int utf8_index;
    unsigned char buf[8];
    char *term_name;
    enum TermCode term_code;
    int term_flags;
#define KBS_CONTROL_H           0x01
#define USE_ERASE_END_OF_LINE   0x02
#define USE_BOLD_AS_BRIGHT_FG   0x04
#define USE_BLINK_AS_BRIGHT_BG  0x08
#define USE_256_COLORS          0x10
#define USE_24_BIT_COLORS       0x20
    const QEColor *term_colors;
    int term_fg_colors_count;
    int term_bg_colors_count;
    unsigned int comb_cache[COMB_CACHE_SIZE];
} TTYState;

static void tty_resize(int sig);
static void tty_term_exit(void);
static void tty_read_handler(void *opaque);

static QEditScreen *tty_screen;   /* for tty_term_exit and tty_resize */

static int tty_term_probe(void)
{
    return 1;
}

static int tty_term_init(QEditScreen *s,
                         qe__unused__ int w, qe__unused__ int h)
{
    TTYState *ts;
    struct termios tty;
    struct sigaction sig;

    ts = calloc(1, sizeof(*ts));
    if (ts == NULL) {
        return 1;
    }

    tty_screen = s;
    s->STDIN = stdin;
    s->STDOUT = stdout;
    s->priv_data = ts;
    s->media = CSS_MEDIA_TTY;

    /* Derive some settings from the TERM environment variable */
    ts->term_code = TERM_UNKNOWN;
    ts->term_flags = USE_ERASE_END_OF_LINE;
    ts->term_colors = xterm_colors;
    ts->term_fg_colors_count = 16;
    ts->term_bg_colors_count = 16;

    ts->term_name = getenv("TERM");
    if (ts->term_name) {
        /* linux and xterm -> kbs=\177
         * ansi cygwin vt100 -> kbs=^H
         */
        if (strstart(ts->term_name, "ansi", NULL)) {
            ts->term_code = TERM_ANSI;
            ts->term_flags |= KBS_CONTROL_H;
        } else
        if (strstart(ts->term_name, "vt100", NULL)) {
            ts->term_code = TERM_VT100;
            ts->term_flags |= KBS_CONTROL_H;
        } else
        if (strstart(ts->term_name, "xterm", NULL)) {
            ts->term_code = TERM_XTERM;
        } else
        if (strstart(ts->term_name, "linux", NULL)) {
            ts->term_code = TERM_LINUX;
        } else
        if (strstart(ts->term_name, "cygwin", NULL)) {
            ts->term_code = TERM_CYGWIN;
            ts->term_flags |= KBS_CONTROL_H |
                              USE_BOLD_AS_BRIGHT_FG | USE_BLINK_AS_BRIGHT_BG;
        }
#if defined(CONFIG_TINY)
        ts->term_flags &= ~(USE_256_COLORS | USE_24_BIT_COLORS);
#else
        if (strstr(ts->term_name, "256")) {
            ts->term_flags &= ~(USE_BOLD_AS_BRIGHT_FG | USE_BLINK_AS_BRIGHT_BG |
                                USE_256_COLORS | USE_24_BIT_COLORS);
            ts->term_flags |= USE_256_COLORS;
        }
        if (strstr(ts->term_name, "true") || strstr(ts->term_name, "24")) {
            ts->term_flags &= ~(USE_BOLD_AS_BRIGHT_FG | USE_BLINK_AS_BRIGHT_BG |
                                USE_256_COLORS | USE_24_BIT_COLORS);
            ts->term_flags |= USE_24_BIT_COLORS;
        }
#endif
    }

    if (ts->term_flags & (USE_256_COLORS | USE_24_BIT_COLORS)) {
        ts->term_fg_colors_count = 256;
#if TTY_STYLE_BITS == 32
        ts->term_bg_colors_count = 256;
#endif
    }

    tcgetattr(fileno(s->STDIN), &tty);
    ts->oldtty = tty;

    tty.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP
                     |INLCR|IGNCR|ICRNL|IXON);
    tty.c_oflag |= OPOST;
    tty.c_lflag &= ~(ECHO|ECHONL|ICANON|IEXTEN|ISIG);
    tty.c_cflag &= ~(CSIZE|PARENB);
    tty.c_cflag |= CS8;
    tty.c_cc[VMIN] = 1;
    tty.c_cc[VTIME] = 0;

    tcsetattr(fileno(s->STDIN), TCSANOW, &tty);

    /* First switch to full screen mode */
#if 0
    printf("\033[?1048h\033[?1047h"     /* enable cup */
           "\033)0\033(B"       /* select character sets in block 0 and 1 */
           "\017");             /* shift out */
#else
    TTY_FPRINTF(s->STDOUT,
                "\033[?1049h"       /* enter_ca_mode */
                "\033[m\033(B"      /* exit_attribute_mode */
                "\033[4l"           /* exit_insert_mode */
                "\033[?7h"          /* enter_am_mode */
                "\033[39;49m"       /* orig_pair */
                "\033[?1h\033="     /* keypad_xmit */
               );
#endif

    /* Get charset from command line option */
    s->charset = find_charset(qe_state.tty_charset);

    if (ts->term_code == TERM_CYGWIN)
        s->charset = &charset_8859_1;

    if (!s->charset && !isatty(fileno(s->STDOUT)))
        s->charset = &charset_8859_1;

    if (!s->charset) {
        int y, x, n;

        s->charset = &charset_8859_1;

        /* Test UTF8 support by looking at the cursor position (idea
         * from Ricardas Cepas <rch@pub.osf.lt>). Since uClibc actually
         * tests to ensure that the format string is a valid multibyte
         * sequence in the current locale (ANSI/ISO C99), use a format
         * specifier of %s to avoid printf() failing with EILSEQ.
         */

        /*               ^X  ^Z    ^M   \170101  */
        //printf("%s", "\030\032" "\r\xEF\x81\x81" "\033[6n\033D");
        /* Just print utf-8 encoding for eacute and check cursor position */
        TTY_FPRINTF(s->STDOUT, "%s",
                    "\030\032" "\r\xC3\xA9" "\033[6n\033D");
        fflush(s->STDOUT);
        n = fscanf(s->STDIN, "\033[%d;%d", &y, &x);  /* get cursor position */
        TTY_FPRINTF(s->STDOUT, "\r   \r");        /* go back, erase 3 chars */
        if (n == 2 && x == 2) {
            s->charset = &charset_utf8;
        }
    }
    put_status(NULL, "tty charset: %s", s->charset->name);

    atexit(tty_term_exit);

    sig.sa_handler = tty_resize;
    sigemptyset(&sig.sa_mask);
    sig.sa_flags = 0;
    sigaction(SIGWINCH, &sig, NULL);
    fcntl(fileno(s->STDIN), F_SETFL, O_NONBLOCK);
    /* If stdout is to a pty, make sure we aren't in nonblocking mode.
     * Otherwise, the printf()s in term_flush() can fail with EAGAIN,
     * causing repaint errors when running in an xterm or in a screen
     * session. */
    fcntl(fileno(s->STDOUT), F_SETFL, 0);

    set_read_handler(fileno(s->STDIN), tty_read_handler, s);

    tty_resize(0);

    if (ts->term_flags & KBS_CONTROL_H) {
        do_toggle_control_h(NULL, 1);
    }

    return 0;
}

static void tty_term_close(QEditScreen *s)
{
    TTYState *ts = s->priv_data;

    fcntl(fileno(s->STDIN), F_SETFL, 0);
#if 0
    /* go to the last line */
    printf("\033[%d;%dH\033[m\033[K"
           "\033[?1047l\033[?1048l",    /* disable cup */
           s->height, 1);
#else
    /* go to last line and clear it */
    TTY_FPRINTF(s->STDOUT, "\033[%d;%dH" "\033[m\033[K", s->height, 1);
    TTY_FPRINTF(s->STDOUT,
                "\033[?1049l"        /* exit_ca_mode */
                "\r"                 /* return */
                "\033[?1l\033>"      /* keypad_local */
                "\r"                 /* return */
               );
#endif
    fflush(s->STDOUT);

    qe_free(&ts->screen);
    qe_free(&ts->line_updated);
}

static void tty_term_exit(void)
{
    QEditScreen *s = tty_screen;
    TTYState *ts = s->priv_data;

    tcsetattr(fileno(s->STDIN), TCSANOW, &ts->oldtty);
}

static void tty_resize(qe__unused__ int sig)
{
    QEditScreen *s = tty_screen;
    TTYState *ts = s->priv_data;
    struct winsize ws;
    int i, count, size;
    TTYChar tc;

    s->width = 80;
    s->height = 24;
    if (ioctl(fileno(s->STDIN), TIOCGWINSZ, &ws) == 0) {
        s->width = ws.ws_col;
        s->height = ws.ws_row;
        if (s->width < 10)
            s->width = 10;
        if (s->width > MAX_SCREEN_WIDTH)
            s->width = MAX_SCREEN_WIDTH;
        if (s->height < 3)
            s->height = 3;
    }

    count = s->width * s->height;
    size = count * sizeof(TTYChar);
    /* screen buffer + shadow buffer + extra slot for loop guard */
    qe_realloc(&ts->screen, size * 2 + sizeof(TTYChar));
    qe_realloc(&ts->line_updated, s->height);
    ts->screen_size = count;

    /* Erase shadow buffer to impossible value */
    memset(ts->screen + count, 0xFF, size + sizeof(TTYChar));
    /* Fill screen buffer with black spaces */
    tc = TTYCHAR_DEFAULT;
    for (i = 0; i < count; i++) {
        ts->screen[i] = tc;
    }
    /* All rows need refresh */
    memset(ts->line_updated, 1, s->height);

    s->clip_x1 = 0;
    s->clip_y1 = 0;
    s->clip_x2 = s->width;
    s->clip_y2 = s->height;
}

static void tty_term_invalidate(QEditScreen *s)
{
    tty_resize(0);
}

static void tty_term_cursor_at(QEditScreen *s, int x1, int y1,
                               qe__unused__ int w, qe__unused__ int h)
{
    TTYState *ts = s->priv_data;
    ts->cursor_x = x1;
    ts->cursor_y = y1;
}

static int tty_term_is_user_input_pending(QEditScreen *s)
{
    fd_set rfds;
    struct timeval tv;

    tv.tv_sec = 0;
    tv.tv_usec = 0;
    FD_ZERO(&rfds);
    FD_SET(fileno(s->STDIN), &rfds);
    if (select(fileno(s->STDIN) + 1, &rfds, NULL, NULL, &tv) > 0)
        return 1;
    else
        return 0;
}

static int const csi_lookup[] = {
    KEY_NONE,   /* 0 */
    KEY_HOME,   /* 1 */
    KEY_INSERT, /* 2 */
    KEY_DELETE, /* 3 */
    KEY_END,    /* 4 */
    KEY_PAGEUP, /* 5 */
    KEY_PAGEDOWN, /* 6 */
    KEY_NONE,   /* 7 */
    KEY_NONE,   /* 8 */
    KEY_NONE,   /* 9 */
    KEY_NONE,   /* 10 */
    KEY_F1,     /* 11 */
    KEY_F2,     /* 12 */
    KEY_F3,     /* 13 */
    KEY_F4,     /* 14 */
    KEY_F5,     /* 15 */
    KEY_NONE,   /* 16 */
    KEY_F6,     /* 17 */
    KEY_F7,     /* 18 */
    KEY_F8,     /* 19 */
    KEY_F9,     /* 20 */
    KEY_F10,    /* 21 */
    KEY_NONE,   /* 22 */
    KEY_F11,    /* 23 */
    KEY_F12,    /* 24 */
    KEY_F13,    /* 25 */
    KEY_F14,    /* 26 */
    KEY_NONE,   /* 27 */
    KEY_F15,    /* 28 */
    KEY_F16,    /* 29 */
    KEY_NONE,   /* 30 */
    KEY_F17,    /* 31 */
    KEY_F18,    /* 32 */
    KEY_F19,    /* 33 */
    KEY_F20,    /* 34 */
};

static void tty_read_handler(void *opaque)
{
    QEditScreen *s = opaque;
    QEmacsState *qs = &qe_state;
    TTYState *ts = s->priv_data;
    QEEvent ev1, *ev = &ev1;
    u8 buf[1];
    int ch, len;

    if (read(fileno(s->STDIN), buf, 1) != 1)
        return;

    if (qs->trace_buffer &&
        qs->active_window &&
        qs->active_window->b != qs->trace_buffer) {
        eb_trace_bytes(buf, 1, EB_TRACE_TTY);
    }

    ch = buf[0];

    switch (ts->input_state) {
    case IS_NORM:
        /* charset handling */
        if (s->charset == &charset_utf8) {
            if (ts->utf8_index && !(ch > 0x80 && ch < 0xc0)) {
                /* not a valid continuation byte */
                /* flush stored prefix, restart from current byte */
                /* XXX: maybe should consume prefix byte as binary */
                ts->utf8_index = 0;
            }
            ts->buf[ts->utf8_index] = ch;
            len = utf8_length[ts->buf[0]];
            if (len > 1) {
                const char *p = cs8(ts->buf);
                if (++ts->utf8_index < len) {
                    /* valid utf8 sequence underway, wait for next */
                    return;
                }
                ch = utf8_decode(&p);
            }
        }
        if (ch == '\033') {
            if (!tty_term_is_user_input_pending(s)) {
                /* Trick to distinguish the ESC key from function and meta
                 * keys  transmitting escape sequences starting with \033
                 * but followed immediately by more characters.
                 */
                goto the_end;
            }
            ts->input_state = IS_ESC;
        } else {
            goto the_end;
        }
        break;
    case IS_ESC:
        if (ch == '\033') {
            /* cygwin A-right transmit ESC ESC[C ... */
            goto the_end;
        }
        if (ch == '[') {
            if (!tty_term_is_user_input_pending(s)) {
                ch = KEY_META('[');
                ts->input_state = IS_NORM;
                goto the_end;
            }
            ts->input_state = IS_CSI;
            ts->input_param = 0;
            ts->input_param2 = 0;
        } else if (ch == 'O') {
            ts->input_state = IS_ESC2;
            ts->input_param = 0;
            ts->input_param2 = 0;
        } else {
            ch = KEY_META(ch);
            ts->input_state = IS_NORM;
            goto the_end;
        }
        break;
    case IS_CSI:
        if (ch >= '0' && ch <= '9') {
            ts->input_param = ts->input_param * 10 + ch - '0';
            break;
        }
        ts->input_state = IS_NORM;
        switch (ch) {
        case ';': /* multi ignore but the last 2 */
            /* iterm2 uses this for some keys:
             * C-up, C-down, C-left, C-right, S-left, S-right,
             */
            ts->input_param2 = ts->input_param;
            ts->input_param = 0;
            ts->input_state = IS_CSI;
            break;
        case '[':
            ts->input_state = IS_CSI2;
            break;
        case '~':
            if (ts->input_param < countof(csi_lookup)) {
                ch = csi_lookup[ts->input_param];
                goto the_end;
            }
            break;
            /* All these for ansi|cygwin */
        default:
            if (ts->input_param == 5) {
                /* xterm CTRL-arrows */
                /* iterm2 CTRL-arrows:
                 * C-up    = ^[[1;5A
                 * C-down  = ^[[1;5B
                 * C-left  = ^[[1;5D
                 * C-right = ^[[1;5C
                 */
                switch (ch) {
                case 'A': ch = KEY_CTRL_UP;    goto the_end;
                case 'B': ch = KEY_CTRL_DOWN;  goto the_end;
                case 'C': ch = KEY_CTRL_RIGHT; goto the_end;
                case 'D': ch = KEY_CTRL_LEFT;  goto the_end;
                }
            } else
            if (ts->input_param == 2) {
                /* iterm2 SHIFT-arrows:
                 * S-left  = ^[[1;2D
                 * S-right = ^[[1;2C
                 * should set-mark if region not visible
                 */
                switch (ch) {
                case 'A': ch = KEY_UP;    goto the_end;
                case 'B': ch = KEY_DOWN;  goto the_end;
                case 'C': ch = KEY_RIGHT; goto the_end;
                case 'D': ch = KEY_LEFT;  goto the_end;
                }
            } else {
                switch (ch) {
                case 'A': ch = KEY_UP;        goto the_end; // kcuu1
                case 'B': ch = KEY_DOWN;      goto the_end; // kcud1
                case 'C': ch = KEY_RIGHT;     goto the_end; // kcuf1
                case 'D': ch = KEY_LEFT;      goto the_end; // kcub1
                case 'F': ch = KEY_END;       goto the_end; // kend
                //case 'G': ch = KEY_CENTER;  goto the_end; // kb2
                case 'H': ch = KEY_HOME;      goto the_end; // khome
                case 'L': ch = KEY_INSERT;    goto the_end; // kich1
                //case 'M': ch = KEY_MOUSE;   goto the_end; // kmous
                case 'Z': ch = KEY_SHIFT_TAB; goto the_end; // kcbt
                }
            }
            break;
        }
        break;
    case IS_CSI2:
        /* cygwin/linux terminal */
        ts->input_state = IS_NORM;
        switch (ch) {
        case 'A': ch = KEY_F1; goto the_end;
        case 'B': ch = KEY_F2; goto the_end;
        case 'C': ch = KEY_F3; goto the_end;
        case 'D': ch = KEY_F4; goto the_end;
        case 'E': ch = KEY_F5; goto the_end;
        }
        break;
    case IS_ESC2:       // "\EO"
        /* xterm/vt100 fn */
        ts->input_state = IS_NORM;
        switch (ch) {
        case 'A': ch = KEY_UP;         goto the_end;
        case 'B': ch = KEY_DOWN;       goto the_end;
        case 'C': ch = KEY_RIGHT;      goto the_end;
        case 'D': ch = KEY_LEFT;       goto the_end;
        case 'F': ch = KEY_CTRL_RIGHT; goto the_end; /* iterm2 F-right */
        case 'H': ch = KEY_CTRL_LEFT;  goto the_end; /* iterm2 F-left */
        case 'P': ch = KEY_F1;         goto the_end;
        case 'Q': ch = KEY_F2;         goto the_end;
        case 'R': ch = KEY_F3;         goto the_end;
        case 'S': ch = KEY_F4;         goto the_end;
        case 't': ch = KEY_F5;         goto the_end;
        case 'u': ch = KEY_F6;         goto the_end;
        case 'v': ch = KEY_F7;         goto the_end;
        case 'l': ch = KEY_F8;         goto the_end;
        case 'w': ch = KEY_F9;         goto the_end;
        case 'x': ch = KEY_F10;        goto the_end;
        }
        break;
    the_end:
        ev->key_event.type = QE_KEY_EVENT;
        ev->key_event.key = ch;
        qe_handle_event(ev);
        break;
    }
}

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
int get_tty_color(QEColor color, QEColor const *colors, int count)
{
    int i, cmin, dmin, d;

    dmin = INT_MAX;
    cmin = 0;
    for (i = 0; i < count; i++) {
        d = color_dist(color, colors[i]);
        if (d < dmin) {
            cmin = i;
            dmin = d;
        }
    }
#if 1
    if (dmin > 0 && count > 16) {
        unsigned int r = (color >> 16) & 0xff;
        unsigned int g = (color >>  8) & 0xff;
        unsigned int b = (color >>  0) & 0xff;
        if (r == g && g == b) {
            i = scale_grey[r];
            d = color_dist(color, colors[i]);
            if (d < dmin) {
                cmin = i;
                dmin = d;
            }
        } else {
            /* XXX: should use more precise projection */
            i = scale_cube[r] * 36 + scale_cube[g] * 6 + scale_cube[b];
            d = color_dist(color, colors[i]);
            if (d < dmin) {
                cmin = i;
                dmin = d;
            }
        }
    }
#endif
    return cmin;
}

static void tty_term_fill_rectangle(QEditScreen *s,
                                    int x1, int y1, int w, int h, QEColor color)
{
    TTYState *ts = s->priv_data;
    int x2 = x1 + w;
    int y2 = y1 + h;
    int x, y;
    TTYChar *ptr;
    int wrap = s->width - w;
    int bgcolor;

    ptr = ts->screen + y1 * s->width + x1;
    if (color == QECOLOR_XOR) {
        for (y = y1; y < y2; y++) {
            ts->line_updated[y] = 1;
            for (x = x1; x < x2; x++) {
                *ptr ^= TTYCHAR(0, 7, 7);
                ptr++;
            }
            ptr += wrap;
        }
    } else {
        bgcolor = get_tty_color(color, ts->term_colors, ts->term_bg_colors_count);
        for (y = y1; y < y2; y++) {
            ts->line_updated[y] = 1;
            for (x = x1; x < x2; x++) {
                *ptr = TTYCHAR(' ', 7, bgcolor);
                ptr++;
            }
            ptr += wrap;
        }
    }
}

/* XXX: could alloc font in wrapper */
static QEFont *tty_term_open_font(qe__unused__ QEditScreen *s,
                                  qe__unused__ int style, qe__unused__ int size)
{
    QEFont *font;

    font = qe_mallocz(QEFont);
    if (!font)
        return NULL;

    font->ascent = 0;
    font->descent = 1;
    font->priv_data = NULL;
    return font;
}

static void tty_term_close_font(qe__unused__ QEditScreen *s, QEFont **fontp)
{
    qe_free(fontp);
}

static inline int tty_term_glyph_width(qe__unused__ QEditScreen *s, unsigned int ucs)
{
    /* fast test for majority of non-wide scripts */
    if (ucs < 0x300)
        return 1;

    return unicode_tty_glyph_width(ucs);
}

static void tty_term_text_metrics(QEditScreen *s, qe__unused__ QEFont *font,
                                  QECharMetrics *metrics,
                                  const unsigned int *str, int len)
{
    int i, x;

    metrics->font_ascent = font->ascent;
    metrics->font_descent = font->descent;
    x = 0;
    for (i = 0; i < len; i++)
        x += tty_term_glyph_width(s, str[i]);

    metrics->width = x;
}

#if MAX_UNICODE_DISPLAY > 0xFFFF

static unsigned int comb_cache_add(TTYState *ts, const unsigned int *seq, int len) {
    unsigned int *ip;
    for (ip = ts->comb_cache; *ip; ip += *ip & 0xFFFF) {
        if (*ip == len + 1U && !memcmp(ip + 1, seq, len * sizeof(*ip))) {
            return TTYCHAR_COMB + (ip - ts->comb_cache);
        }
    }
    for (ip = ts->comb_cache; *ip; ip += *ip & 0xFFFF) {
        if (*ip >= 0x10001U + len) {
            /* found free slot */
            if (*ip > 0x10001U + len) {
                /* split free block */
                ip[len + 1] = *ip - (len + 1);
            }
            break;
        }
    }
    if (*ip == 0) {
        if ((ip - ts->comb_cache) + len + 1 >= countof(ts->comb_cache)) {
            return TTYCHAR_BAD;
        }
        ip[len + 1] = 0;
    }
    *ip = len + 1;
    memcpy(ip + 1, seq, len * sizeof(*ip));
    return TTYCHAR_COMB + (ip - ts->comb_cache);
}

static void comb_cache_clean(TTYState *ts, const TTYChar *screen, int len) {
    unsigned int *ip;
    int i;

    if (ts->comb_cache[0] == 0)
        return;

    for (ip = ts->comb_cache; *ip != 0; ip += *ip & 0xFFFF) {
        *ip |= 0x10000;
    }
    ip = ts->comb_cache;
    for (i = 0; i < len; i++) {
        int ch = TTYCHAR_GETCH(screen[i]);
        if (ch >= TTYCHAR_COMB && ch < TTYCHAR_COMB + countof(ts->comb_cache) - 1) {
            ip[ch - TTYCHAR_COMB] &= ~0x10000;
        }
    }
    for (; *ip != 0; ip += *ip & 0xFFFF) {
        if (*ip & 0x10000) {
            while (ip[*ip & 0xFFFF] & 0x10000) {
                /* coalesce subsequent free blocks */
                *ip += ip[*ip & 0xFFFF] & 0xFFFF;
            }
            if (ip[*ip & 0xFFFF] == 0) {
                /* truncate free list */
                *ip = 0;
                break;
            }
        }
    }
}

static void comb_cache_describe(QEditScreen *s, EditBuffer *b) {
    TTYState *ts = s->priv_data;
    unsigned int *ip;
    unsigned int i;
    int w = 16;

    eb_printf(b, "Device Description\n\n");

    eb_printf(b, "%*s: %s\n", w, "term_name", ts->term_name);
    eb_printf(b, "%*s: %d  %s\n", w, "term_code", ts->term_code,
              ts->term_code == TERM_UNKNOWN ? "UNKNOWN" :
              ts->term_code == TERM_ANSI ? "ANSI" :
              ts->term_code == TERM_VT100 ? "VT100" :
              ts->term_code == TERM_XTERM ? "XTERM" :
              ts->term_code == TERM_LINUX ? "LINUX" :
              ts->term_code == TERM_CYGWIN ? "CYGWIN" :
              "");
    eb_printf(b, "%*s: %#x %s%s%s%s%s%s\n", w, "term_flags", ts->term_flags,
              ts->term_flags & KBS_CONTROL_H ? " KBS_CONTROL_H" : "",
              ts->term_flags & USE_ERASE_END_OF_LINE ? " USE_ERASE_END_OF_LINE" : "",
              ts->term_flags & USE_BOLD_AS_BRIGHT_FG ? " USE_BOLD_AS_BRIGHT_FG" : "",
              ts->term_flags & USE_BLINK_AS_BRIGHT_BG ? " USE_BLINK_AS_BRIGHT_BG" : "",
              ts->term_flags & USE_256_COLORS ? " USE_256_COLORS" : "",
              ts->term_flags & USE_24_BIT_COLORS ? " USE_24_BIT_COLORS" : "");
    eb_printf(b, "%*s: fg:%d, bg:%d\n", w, "colors",
              ts->term_fg_colors_count, ts->term_bg_colors_count);
    
    eb_printf(b, "\n");
    eb_printf(b, "Unicode combination cache:\n\n");
    
    for (ip = ts->comb_cache; *ip != 0; ip += *ip & 0xFFFF) {
        if (*ip & 0x10000) {
            eb_printf(b, "   FREE   %d\n", (*ip & 0xFFFF) - 1);
        } else {
            eb_printf(b, "  %06X  %d:",
                      (unsigned int)(TTYCHAR_COMB + (ip - ts->comb_cache)),
                      (*ip & 0xFFFF) - 1);
            for (i = 1; i < (*ip & 0xFFFF); i++) {
                eb_printf(b, " %04X", ip[i]);
            }
            eb_printf(b, "\n");
        }
    }
}
#else
#define comb_cache_add(s, p, n)  TTYCHAR_BAD
#define comb_cache_clean(s, p, n)
#define comb_cache_describe(s, b)
#endif

static void tty_term_draw_text(QEditScreen *s, qe__unused__ QEFont *font,
                               int x, int y, const unsigned int *str0, int len,
                               QEColor color)
{
    TTYState *ts = s->priv_data;
    TTYChar *ptr;
    int fgcolor, w, n;
    unsigned int cc;
    const unsigned int *str = str0;

    if (y < s->clip_y1 || y >= s->clip_y2 || x >= s->clip_x2)
        return;

    ts->line_updated[y] = 1;
    fgcolor = get_tty_color(color, ts->term_colors, ts->term_fg_colors_count);
    if (font->style & QE_STYLE_UNDERLINE)
        fgcolor |= TTY_UNDERLINE;
    if (font->style & QE_STYLE_BOLD)
        fgcolor |= TTY_BOLD;
    if (font->style & QE_STYLE_BLINK)
        fgcolor |= TTY_BLINK;
    if (font->style & QE_STYLE_ITALIC)
        fgcolor |= TTY_ITALIC;
    ptr = ts->screen + y * s->width;

    if (x < s->clip_x1) {
        ptr += s->clip_x1;
        /* left clip */
        while (len > 0) {
            cc = *str++;
            len--;
            w = tty_term_glyph_width(s, cc);
            x += w;
            if (x >= s->clip_x1) {
                /* pad partially clipped wide char with spaces */
                n = x;
                if (n > s->clip_x2)
                    n = s->clip_x2;
                n -= s->clip_x1;
                for (; n > 0; n--) {
                    *ptr = TTYCHAR(' ', fgcolor, TTYCHAR_GETBG(*ptr));
                    ptr++;
                }
                /* skip combining code points if any */
                while (len > 0 && tty_term_glyph_width(s, *str) == 0) {
                    len--;
                    str++;
                }
                break;
            }
        }
    } else {
        ptr += x;
    }
    for (; len > 0; len--, str++) {
        cc = *str;
        w = tty_term_glyph_width(s, cc);
        if (x + w > s->clip_x2) {
            /* pad partially clipped wide char with spaces */
            while (x < s->clip_x2) {
                *ptr = TTYCHAR(' ', fgcolor, TTYCHAR_GETBG(*ptr));
                ptr++;
                x++;
            }
            break;
        }
        if (w == 0) {
            if (str == str0)
                continue;
            /* allocate synthetic glyph for multicharacter combination */
            int nacc = 1;
            while (nacc < len && tty_term_glyph_width(s, str[nacc]) == 0)
                nacc++;
            cc = comb_cache_add(ts, str - 1, nacc + 1);
            str += nacc - 1;
            len -= nacc - 1;
            ptr[-1] = TTYCHAR(cc, fgcolor, TTYCHAR_GETBG(ptr[-1]));
        } else {
            *ptr = TTYCHAR(cc, fgcolor, TTYCHAR_GETBG(*ptr));
            ptr++;
            x += w;
            while (w > 1) {
                /* put placeholders for wide chars */
                *ptr = TTYCHAR(TTYCHAR_NONE, fgcolor, TTYCHAR_GETBG(*ptr));
                ptr++;
                w--;
            }
        }
    }
}

static void tty_term_set_clip(qe__unused__ QEditScreen *s,
                              qe__unused__ int x, qe__unused__ int y,
                              qe__unused__ int w, qe__unused__ int h)
{
}

static void tty_term_flush(QEditScreen *s)
{
    TTYState *ts = s->priv_data;
    TTYChar *ptr, *ptr1, *ptr2, *ptr3, *ptr4, cc, blankcc;
    int y, shadow, ch, bgcolor, fgcolor, shifted, attr;

    TTY_FPUTS("\033[H\033[0m", s->STDOUT);

    if (ts->term_code != TERM_CYGWIN) {
        TTY_FPUTS("\033(B\033)0", s->STDOUT);
    }

    bgcolor = -1;
    fgcolor = -1;
    attr = 0;
    shifted = 0;

    /* CG: Should optimize output by computing it in a temporary buffer
     * and flushing it in one call to fwrite()
     */

    shadow = ts->screen_size;
    /* We cannot print anything on the bottom right screen cell,
     * pretend it's OK: */
    ts->screen[shadow - 1] = ts->screen[2 * shadow - 1];

    for (y = 0; y < s->height; y++) {
        if (ts->line_updated[y]) {
            ts->line_updated[y] = 0;
            ptr = ptr1 = ts->screen + y * s->width;
            ptr3 = ptr2 = ptr1 + s->width;

            /* quickly find the first difference on row:
             * patch loop guard cell to make sure the simple loop stops
             * without testing ptr1 < ptr2
             */
            cc = ptr2[shadow];
            ptr2[shadow] = ptr2[0] + 1;
            while (ptr1[0] == ptr1[shadow]) {
                ptr1++;
            }
            ptr2[shadow] = cc;

            if (ptr1 == ptr2)
                continue;

            /* quickly scan for last difference on row:
             * the first difference on row at ptr1 is before ptr2
             * so we do not need a test on ptr2 > ptr1
             */
            while (ptr2[-1] == ptr2[shadow - 1]) {
                --ptr2;
            }

            ptr4 = ptr2;

            /* Try to optimize with erase to end of line: if the last
             * difference on row is a space, measure the run of same
             * color spaces from the end of the row.  If this run
             * starts before the last difference, the row is a
             * candidate for a partial update with erase-end-of-line.
             * exception: do not use erase end of line for a bright
             * background color if emulated as bright.
             */
            if ((ts->term_flags & USE_ERASE_END_OF_LINE)
            &&  TTYCHAR_GETCH(ptr4[-1]) == ' '
            &&  (/*!(ts->term_flags & USE_BLINK_AS_BRIGHT_BG) ||*/
                 TTYCHAR_GETBG(ptr4[-1]) < 8))
            {
                /* find the last non blank char on row */
                blankcc = TTYCHAR2(' ', TTYCHAR_GETCOL(ptr3[-1]));
                while (ptr3 > ptr1 && ptr3[-1] == blankcc) {
                    --ptr3;
                }
                /* killing end of line is not worth it to erase 3
                 * spaces or less, because the escape sequence itself
                 * is 3 bytes long.
                 */
                if (ptr2 > ptr3 + 3) {
                    ptr4 = ptr3;
                    /* if the background color changes on the last
                     * space, use the generic loop to synchronize that
                     * space because the color change is non trivial
                     */
                    if (ptr3 == ptr1
                    ||  TTYCHAR_GETBG(*ptr3) != TTYCHAR_GETBG(ptr3[-1]))
                        ptr4++;
                }
            }

            /* Go to the first difference */
            /* CG: this probably does not work if there are
             * double-width glyphs on the row in front of this
             * difference (actually it should)
             */
            TTY_FPRINTF(s->STDOUT, "\033[%d;%dH",
                        y + 1, (int)(ptr1 - ptr + 1));

            while (ptr1 < ptr4) {
                cc = *ptr1;
                ptr1[shadow] = cc;
                ptr1++;
                ch = TTYCHAR_GETCH(cc);
                if ((unsigned int)ch != TTYCHAR_NONE) {
                    /* output attributes */
                    again:
                    if (bgcolor != (int)TTYCHAR_GETBG(cc)) {
                        int lastbg = bgcolor;
                        bgcolor = TTYCHAR_GETBG(cc);
                        if (ts->term_flags & (USE_256_COLORS | USE_24_BIT_COLORS)) {
                            TTY_FPRINTF(s->STDOUT, "\033[48;5;%dm", bgcolor);
                        } else
                        if (ts->term_flags & USE_BLINK_AS_BRIGHT_BG) {
                            if (bgcolor > 7) {
                                if (lastbg <= 7) {
                                    TTY_FPUTS("\033[5m", s->STDOUT);
                                }
                            } else {
                                if (lastbg > 7) {
                                    TTY_FPUTS("\033[25m", s->STDOUT);
                                    //fgcolor = -1;
                                }
                            }
                            TTY_FPRINTF(s->STDOUT, "\033[%dm",
                                        40 + (bgcolor & 7));
                        } else {
                            TTY_FPRINTF(s->STDOUT, "\033[%dm",
                                        bgcolor > 7 ? 100 + bgcolor - 8 :
                                        40 + bgcolor);
                        }
                    }
                    /* do not special case SPC on fg color change
                    * because of combining marks */
                    if (fgcolor != (int)TTYCHAR_GETFG(cc)) {
                        int lastfg = fgcolor;
                        fgcolor = TTYCHAR_GETFG(cc);
                        if (ts->term_flags & (USE_256_COLORS | USE_24_BIT_COLORS)) {
                            TTY_FPRINTF(s->STDOUT, "\033[38;5;%dm", fgcolor);
                        } else
                        if (ts->term_flags & USE_BOLD_AS_BRIGHT_FG) {
                            if (fgcolor > 7) {
                                if (lastfg <= 7) {
                                    TTY_FPUTS("\033[1m", s->STDOUT);
                                }
                            } else {
                                if (lastfg > 7) {
                                    TTY_FPUTS("\033[22m", s->STDOUT);
                                    //fgcolor = -1;
                                    //bgcolor = -1;
                                    goto again;
                                }
                            }
                            TTY_FPRINTF(s->STDOUT, "\033[%dm",
                                        30 + (fgcolor & 7));
                        } else {
                            TTY_FPRINTF(s->STDOUT, "\033[%dm",
                                        fgcolor > 8 ? 90 + fgcolor - 8 :
                                        30 + fgcolor);
                        }
                    }
#if 1
                    if (attr != (int)TTYCHAR_GETCOL(cc)) {
                        int lastattr = attr;
                        attr = TTYCHAR_GETCOL(cc);

                        if ((attr ^ lastattr) & TTY_BOLD) {
                            if (attr & TTY_BOLD) {
                                TTY_FPUTS("\033[1m", s->STDOUT);
                            } else {
                                TTY_FPUTS("\033[22m", s->STDOUT);
                            }
                        }
                        if ((attr ^ lastattr) & TTY_UNDERLINE) {
                            if (attr & TTY_UNDERLINE) {
                                TTY_FPUTS("\033[4m", s->STDOUT);
                            } else {
                                TTY_FPUTS("\033[24m", s->STDOUT);
                            }
                        }
                        if ((attr ^ lastattr) & TTY_BLINK) {
                            if (attr & TTY_BLINK) {
                                TTY_FPUTS("\033[5m", s->STDOUT);
                            } else {
                                TTY_FPUTS("\033[25m", s->STDOUT);
                            }
                        }
                        if ((attr ^ lastattr) & TTY_ITALIC) {
                            if (attr & TTY_ITALIC) {
                                TTY_FPUTS("\033[3m", s->STDOUT);
                            } else {
                                TTY_FPUTS("\033[23m", s->STDOUT);
                            }
                        }
#endif
                    }
                    if (shifted) {
                        /* Kludge for linedrawing chars */
                        if (ch < 128 || ch >= 128 + 32) {
                            TTY_FPUTS("\033(B", s->STDOUT);
                            shifted = 0;
                        }
                    }

                    /* do not display escape codes or invalid codes */
                    if (ch < 32 || ch == 127) {
                        TTY_PUTC('.', s->STDOUT);
                    } else
                    if (ch < 127) {
                        TTY_PUTC(ch, s->STDOUT);
                    } else
                    if (ch < 128 + 32) {
                        /* Kludges for linedrawing chars */
                        if (ts->term_code == TERM_CYGWIN) {
                            static const char unitab_xterm_poorman[32] =
                                "*#****o~**+++++-----++++|****L. ";
                            TTY_PUTC(unitab_xterm_poorman[ch - 128],
                                     s->STDOUT);
                        } else {
                            if (!shifted) {
                                TTY_FPUTS("\033(0", s->STDOUT);
                                shifted = 1;
                            }
                            TTY_PUTC(ch - 32, s->STDOUT);
                        }
                    } else
#if MAX_UNICODE_DISPLAY > 0xFFFF
                    if (ch >= TTYCHAR_COMB && ch < TTYCHAR_COMB + COMB_CACHE_SIZE - 1) {
                        u8 buf[10], *q;
                        unsigned int *ip = ts->comb_cache + (ch - TTYCHAR_COMB);
                        int ncc = *ip++;

                        if (ncc < 0x300) {
                            while (ncc-- > 1) {
                                q = s->charset->encode_func(s->charset, buf, *ip++);
                                if (q) {
                                    TTY_FWRITE(buf, 1, q - buf, s->STDOUT);
                                }
                            }
                        }
                    } else
#endif
                    {
                        u8 buf[10], *q;
                        int nc;

                        // was in qemacs-0.3.1.g2.gw/tty.c:
                        // if (cc == 0x2500)
                        //    printf("\016x\017");
                        /* s->charset is either latin1 or utf-8 */
                        q = s->charset->encode_func(s->charset, buf, ch);
                        if (!q) {
                            if (s->charset == &charset_8859_1) {
                                /* upside down question mark */
                                buf[0] = 0xBF;
                            } else {
                                buf[0] = '?';
                            }
                            q = buf + 1;
                            if (tty_term_glyph_width(s, ch) == 2) {
                                *q++ = '?';
                            }
                        }

                        nc = q - buf;
                        if (nc == 1) {
                            TTY_PUTC(*buf, s->STDOUT);
                        } else {
                            TTY_FWRITE(buf, 1, nc, s->STDOUT);
                        }
                    }
                }
            }
            if (shifted) {
                TTY_FPUTS("\033(B", s->STDOUT);
                shifted = 0;
            }
            if (ptr1 < ptr2) {
                /* More differences to synch in shadow, erase eol */
                cc = *ptr1;
                /* the current attribute is already set correctly */
                TTY_FPUTS("\033[K", s->STDOUT);
                while (ptr1 < ptr2) {
                    ptr1[shadow] = cc;
                    ptr1++;
                }
            }
//            if (ts->term_flags & USE_BLINK_AS_BRIGHT_BG)
            {
                if (bgcolor > 7) {
                    TTY_FPUTS("\033[0m", s->STDOUT);
                    fgcolor = bgcolor = -1;
                    attr = 0;
                }
            }
        }
    }

    TTY_FPUTS("\033[0m", s->STDOUT);
    if (ts->cursor_y + 1 >= 0 && ts->cursor_x + 1 >= 0) {
        TTY_FPRINTF(s->STDOUT, "\033[%d;%dH",
                    ts->cursor_y + 1, ts->cursor_x + 1);
    }
    fflush(s->STDOUT);

    /* Update combination cache from screen. Should do this before redisplay */
    comb_cache_clean(ts, ts->screen, ts->screen_size);
}

static void tty_term_describe(QEditScreen *s, EditBuffer *b) {
    comb_cache_describe(s, b);
}

static QEDisplay tty_dpy = {
    "vt100",
    tty_term_probe,
    tty_term_init,
    tty_term_close,
    tty_term_flush,
    tty_term_is_user_input_pending,
    tty_term_fill_rectangle,
    tty_term_open_font,
    tty_term_close_font,
    tty_term_text_metrics,
    tty_term_draw_text,
    tty_term_set_clip,
    NULL, /* dpy_selection_activate */
    NULL, /* dpy_selection_request */
    tty_term_invalidate,
    tty_term_cursor_at,
    NULL, /* dpy_bmp_alloc */
    NULL, /* dpy_bmp_free */
    NULL, /* dpy_bmp_draw */
    NULL, /* dpy_bmp_lock */
    NULL, /* dpy_bmp_unlock */
    NULL, /* dpy_full_screen */
    tty_term_describe,
    NULL, /* next */
};

static int tty_init(void)
{
    return qe_register_display(&tty_dpy);
}

qe_module_init(tty_init);
