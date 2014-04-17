/*
 * Mode for viewing archive files for QEmacs.
 *
 * Copyright (c) 2002-2013 Charlie Gordon.
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

/* Archivers */
typedef struct ArchiveType {
    const char *name;           /* name of archive format */
    const char *extensions;
    const char *list_cmd;       /* list archive contents to stdout */
    const char *extract_cmd;    /* extract archive element to stdout */
    struct ArchiveType *next;
} ArchiveType;

static ArchiveType archive_type_array[] = {
    { "tar", "tar|tar.Z|tgz|tar.gz|tbz|tbz2|tar.bz2|tar.bzip2|"
            "txz|tar.xz|tlz|tar.lzma",
            "tar tvf '%s'" },
    { "zip", "zip|ZIP|jar|apk", "unzip -l '%s'" },
    { "rar", "rar|RAR", "unrar l '%s'" },
    { "arj", "arj|ARJ", "unarj l '%s'" },
    { "cab", "cab", "cabextract -l '%s'" },
    { "7zip", "7z", "7z l '%s'" },
    { "ar", "a|ar", "ar -tv '%s'" },
    { "xar", "xar", "xar -tvf '%s'" },
    { "zoo", "zoo", "zoo l '%s'" },
};

static ArchiveType *archive_types;

/* Compressors */
typedef struct CompressType {
    const char *name;           /* name of compressed format */
    const char *extensions;
    const char *load_cmd;       /* uncompress file to stdout */
    const char *save_cmd;       /* compress to file from stdin */
    struct CompressType *next;
} CompressType;

static CompressType compress_type_array[] = {
    { "gzip", "gz", "gunzip -c '%s'", "gzip > '%s'" },
    { "bzip2", "bz2|bzip2", "bunzip2 -c '%s'", "bzip2 > '%s'" },
    { "compress", "Z", "uncompress -c '%s'", "compress > '%s'" },
    { "LZMA", "lzma", "unlzma -c '%s'", "lzma > '%s'" },
    { "XZ", "xz", "unxz -c '%s'", "xz > '%s'" },
    /* Need to fix binhex encode command to read from file */
    { "BinHex", "hqx", "binhex decode -p '%s'", NULL },
};

static CompressType *compress_types;

/*---------------- Archivers ----------------*/

static ArchiveType *find_archive_type(const char *filename)
{
    char rname[MAX_FILENAME_SIZE];
    ArchiveType *atp;

    /* File extension based test */
    reduce_filename(rname, sizeof(rname), get_basename(filename));
    for (atp = archive_types; atp; atp = atp->next) {
        if (match_extension(rname, atp->extensions))
            return atp;
    }
    return NULL;
}

static int archive_mode_probe(ModeDef *mode, ModeProbeData *p)
{
    ArchiveType *atp = find_archive_type(p->filename);

    if (atp) {
        if (p->b && p->b->priv_data) {
            /* buffer loaded, re-selecting mode causes buffer reload */
            return 9;
        } else {
            /* buffer not yet loaded */
            return 70;
        }
    }

    return 0;
}

static int archive_mode_init(EditState *s, ModeSavedData *saved_data)
{
    return text_mode_init(s, saved_data);
}

/* specific archive commands */
static CmdDef archive_commands[] = {
    CMD_DEF_END,
};

static ModeDef archive_mode;

static int archive_buffer_load(EditBuffer *b, FILE *f)
{
    /* Launch subprocess to list archive contents */
    char cmd[1024];
    ArchiveType *atp;

    atp = find_archive_type(b->filename);
    if (atp) {
        eb_clear(b);
        eb_printf(b, "  Directory of %s archive %s\n",
                  atp->name, b->filename);
        snprintf(cmd, sizeof(cmd), atp->list_cmd, b->filename);
        new_shell_buffer(b, get_basename(b->filename), NULL, cmd,
                         SF_INFINITE | SF_BUFED_MODE);

        /* XXX: should check for archiver error */
        /* XXX: should delay BF_SAVELOG until buffer is fully loaded */
        b->flags |= BF_READONLY;

        return 0;
    } else {
        eb_printf(b, "Cannot find archiver\n");
        return -1;
    }
}

static int archive_buffer_save(EditBuffer *b, int start, int end,
                               const char *filename)
{
    /* XXX: prevent saving parsed contents to archive file */
    return -1;
}

static void archive_buffer_close(EditBuffer *b)
{
    /* XXX: kill process? */
}

static EditBufferDataType archive_data_type = {
    "archive",
    archive_buffer_load,
    archive_buffer_save,
    archive_buffer_close,
    NULL, /* next */
};

static int archive_init(void)
{
    int i;

    /* copy and patch text_mode */
    memcpy(&archive_mode, &text_mode, sizeof(ModeDef));
    archive_mode.name = "archive";
    archive_mode.mode_probe = archive_mode_probe;
    archive_mode.mode_init = archive_mode_init;
    archive_mode.data_type = &archive_data_type;

    for (i = 1; i < countof(archive_type_array); i++) {
        archive_type_array[i - 1].next = archive_type_array + i;
    }
    archive_types = archive_type_array;

    eb_register_data_type(&archive_data_type);
    qe_register_mode(&archive_mode);
    qe_register_cmd_table(archive_commands, &archive_mode);

    return 0;
}

/*---------------- Compressors ----------------*/

static CompressType *find_compress_type(const char *filename)
{
    char rname[MAX_FILENAME_SIZE];
    CompressType *ctp;

    /* File extension based test */
    reduce_filename(rname, sizeof(rname), get_basename(filename));
    for (ctp = compress_types; ctp; ctp = ctp->next) {
        if (match_extension(rname, ctp->extensions))
            return ctp;
    }
    return NULL;
}

static int compress_mode_probe(ModeDef *mode, ModeProbeData *p)
{
    CompressType *ctp = find_compress_type(p->filename);

    if (ctp) {
        if (p->b && p->b->priv_data) {
            /* buffer loaded, re-selecting mode causes buffer reload */
            return 9;
        } else {
            /* buffer not yet loaded */
            return 60;
        }
    }

    return 0;
}

static int compress_mode_init(EditState *s, ModeSavedData *saved_data)
{
    return text_mode_init(s, saved_data);
}

/* specific compress commands */
static CmdDef compress_commands[] = {
    CMD_DEF_END,
};

static ModeDef compress_mode;

static int compress_buffer_load(EditBuffer *b, FILE *f)
{
    /* Launch subprocess to expand compressed contents */
    char cmd[1024];
    CompressType *ctp;

    ctp = find_compress_type(b->filename);
    if (ctp) {
        eb_clear(b);
        snprintf(cmd, sizeof(cmd), ctp->load_cmd, b->filename);
        new_shell_buffer(b, get_basename(b->filename), NULL, cmd,
                         SF_INFINITE | SF_AUTO_CODING | SF_AUTO_MODE);
        /* XXX: should check for archiver error */
        /* XXX: should delay BF_SAVELOG until buffer is fully loaded */
        b->flags |= BF_READONLY;

        return 0;
    } else {
        eb_printf(b, "cannot find compressor\n");
        return -1;
    }
}

static int compress_buffer_save(EditBuffer *b, int start, int end,
                               const char *filename)
{
    /* XXX: should recompress contents to compressed file */
    return -1;
}

static void compress_buffer_close(EditBuffer *b)
{
    /* XXX: kill process? */
}

static EditBufferDataType compress_data_type = {
    "compress",
    compress_buffer_load,
    compress_buffer_save,
    compress_buffer_close,
    NULL, /* next */
};

static int compress_init(void)
{
    int i;

    /* copy and patch text_mode */
    memcpy(&compress_mode, &text_mode, sizeof(ModeDef));
    compress_mode.name = "compress";
    compress_mode.mode_probe = compress_mode_probe;
    compress_mode.mode_init = compress_mode_init;
    compress_mode.data_type = &compress_data_type;

    for (i = 1; i < countof(compress_type_array); i++) {
        compress_type_array[i - 1].next = compress_type_array + i;
    }
    compress_types = compress_type_array;

    eb_register_data_type(&compress_data_type);
    qe_register_mode(&compress_mode);
    qe_register_cmd_table(compress_commands, &compress_mode);

    return 0;
}

/*---------------- Wget ----------------*/

static ModeDef wget_mode;

static int wget_mode_probe(ModeDef *mode, ModeProbeData *p)
{
    if (strstart(p->real_filename, "http:", NULL)
    ||  strstart(p->real_filename, "https:", NULL)
    ||  strstart(p->real_filename, "ftp:", NULL)) {
        if (p->b && p->b->priv_data) {
            /* buffer loaded, re-selecting mode causes buffer reload */
            return 9;
        } else {
            /* buffer not yet loaded */
            return 90;
        }
    }

    return 0;
}

static int wget_buffer_load(EditBuffer *b, FILE *f)
{
    /* Launch wget subprocess to retrieve contents */
    char cmd[1024];

    eb_clear(b);
    snprintf(cmd, sizeof(cmd), "wget -q -O - %s", b->filename);
    new_shell_buffer(b, get_basename(b->filename), NULL, cmd,
                     SF_INFINITE | SF_AUTO_CODING | SF_AUTO_MODE);
    /* XXX: should check for wget error */
    /* XXX: should delay BF_SAVELOG until buffer is fully loaded */
    b->flags |= BF_READONLY;

    return 0;
}

static int wget_buffer_save(EditBuffer *b, int start, int end,
                               const char *filename)
{
    /* XXX: should put contents back to web server */
    return -1;
}

static void wget_buffer_close(EditBuffer *b)
{
    /* XXX: kill process? */
}

static EditBufferDataType wget_data_type = {
    "wget",
    wget_buffer_load,
    wget_buffer_save,
    wget_buffer_close,
    NULL, /* next */
};

static int wget_init(void)
{
    /* copy and patch text_mode */
    memcpy(&wget_mode, &text_mode, sizeof(ModeDef));
    wget_mode.name = "wget";
    wget_mode.mode_probe = wget_mode_probe;
    wget_mode.data_type = &wget_data_type;

    eb_register_data_type(&wget_data_type);
    qe_register_mode(&wget_mode);

    return 0;
}

/*---------------- Manual pages ----------------*/

static ModeDef man_mode;

static int man_mode_probe(ModeDef *mode, ModeProbeData *p)
{
    if (match_extension(p->real_filename,
                        "1.gz|2.gz|3.gz|4.gz|5.gz|6.gz|7.gz|8.gz")) {
        goto has_man;
    }
   
    if (match_extension(p->real_filename, "1|2|3|4|5|6|7|8")
    &&  !strchr(p->filename, '.')
    &&  (p->buf[0] == '.' || !memcmp(p->buf, "'\\\"", 3))) {
    has_man:
        if (p->b && p->b->priv_data) {
            /* buffer loaded, re-selecting mode causes buffer reload */
            return 9;
        } else {
            /* buffer not yet loaded */
            return 90;
        }
    }

    return 0;
}

static int man_buffer_load(EditBuffer *b, FILE *f)
{
    /* Launch man subprocess to format manual page */
    char cmd[1024];

    eb_clear(b);
    snprintf(cmd, sizeof(cmd), "man %s", b->filename);
    new_shell_buffer(b, get_basename(b->filename), NULL, cmd,
                     SF_COLOR | SF_INFINITE);
    /* XXX: should check for man error */
    /* XXX: should delay BF_SAVELOG until buffer is fully loaded */
    b->flags |= BF_READONLY;

    return 0;
}

static int man_buffer_save(EditBuffer *b, int start, int end,
                               const char *filename)
{
    /* XXX: should put contents back to web server */
    return -1;
}

static void man_buffer_close(EditBuffer *b)
{
    /* XXX: kill process? */
}

static EditBufferDataType man_data_type = {
    "man",
    man_buffer_load,
    man_buffer_save,
    man_buffer_close,
    NULL, /* next */
};

static int man_init(void)
{
    /* copy and patch text_mode */
    memcpy(&man_mode, &text_mode, sizeof(ModeDef));
    man_mode.name = "man";
    man_mode.mode_probe = man_mode_probe;
    man_mode.data_type = &man_data_type;

    eb_register_data_type(&man_data_type);
    qe_register_mode(&man_mode);

    return 0;
}

/*---------------- Initialization ----------------*/

static int archive_compress_init(void)
{
    return archive_init() ||
            compress_init() ||
            wget_init() ||
            man_init();
}

qe_module_init(archive_compress_init);
