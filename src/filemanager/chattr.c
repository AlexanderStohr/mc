/*
   Chattr command -- for the Midnight Commander

   Copyright (C) 2019
   Free Software Foundation, Inc.

   This file is part of the Midnight Commander.

   The Midnight Commander is free software: you can redistribute it
   and/or modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the License,
   or (at your option) any later version.

   The Midnight Commander is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/** \file chattr.c
 *  \brief Source: chattr command
 */

#include <config.h>

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <ext2fs/ext2_fs.h>

#include "lib/global.h"

#include "lib/tty/tty.h"        /* COLS */
#include "lib/vfs/vfs.h"
#include "lib/widget.h"

#include "midnight.h"           /* current_panel */
#include "panel.h"              /* do_file_mark() */

#include "chattr.h"

/* external functions */
extern int fgetflags (const char *name, unsigned long *flags);
extern int fsetflags (const char *name, unsigned long flags);

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

#define PX 3
#define PY 2

#define B_MARKED B_USER
#define B_SETALL (B_USER + 1)
#define B_SETMRK (B_USER + 2)
#define B_CLRMRK (B_USER + 3)

#define BUTTONS  6

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

static struct
{
    unsigned long flags;
    char attr;
    const char *text;
    gboolean selected;
    WCheck *check;
} check_attr[] =
{
#if 0
#define EXT2_SECRM_FL                   0x00000001 /* Secure deletion */
#define EXT2_UNRM_FL                    0x00000002 /* Undelete */
#define EXT2_COMPR_FL                   0x00000004 /* Compress file */
#define EXT2_SYNC_FL                    0x00000008 /* Synchronous updates */
#define EXT2_IMMUTABLE_FL               0x00000010 /* Immutable file */
#define EXT2_APPEND_FL                  0x00000020 /* writes to file may only append */
#define EXT2_NODUMP_FL                  0x00000040 /* do not dump file */
#define EXT2_NOATIME_FL                 0x00000080 /* do not update atime */
/* Reserved for compression usage... */
#define EXT2_DIRTY_FL                   0x00000100
#define EXT2_COMPRBLK_FL                0x00000200 /* One or more compressed clusters */
#define EXT2_NOCOMPR_FL                 0x00000400 /* Access raw compressed data */
        /* nb: was previously EXT2_ECOMPR_FL */
#define EXT4_ENCRYPT_FL                 0x00000800 /* encrypted inode */
/* End compression flags --- maybe not all used */
#define EXT2_BTREE_FL                   0x00001000 /* btree format dir */
#define EXT2_INDEX_FL                   0x00001000 /* hash-indexed directory */
#define EXT2_IMAGIC_FL                  0x00002000
#define EXT3_JOURNAL_DATA_FL            0x00004000 /* file data should be journaled */
#define EXT2_NOTAIL_FL                  0x00008000 /* file tail should not be merged */
#define EXT2_DIRSYNC_FL                 0x00010000 /* Synchronous directory modifications */
#define EXT2_TOPDIR_FL                  0x00020000 /* Top of directory hierarchies*/
#define EXT4_HUGE_FILE_FL               0x00040000 /* Set to each huge file */
#define EXT4_EXTENTS_FL                 0x00080000 /* Inode uses extents */
#define EXT4_VERITY_FL                  0x00100000 /* Verity protected inode */
#define EXT4_EA_INODE_FL                0x00200000 /* Inode used for large EA */
/* EXT4_EOFBLOCKS_FL 0x00400000 was here */
#define FS_NOCOW_FL                     0x00800000 /* Do not cow file */
#define EXT4_SNAPFILE_FL                0x01000000 /* Inode is a snapshot */
#define EXT4_SNAPFILE_DELETED_FL        0x04000000 /* Snapshot is being deleted */
#define EXT4_SNAPFILE_SHRUNK_FL         0x08000000 /* Snapshot shrink has completed */
#define EXT4_INLINE_DATA_FL             0x10000000 /* Inode has inline data */
#define EXT4_PROJINHERIT_FL             0x20000000 /* Create with parents projid */
#define EXT4_CASEFOLD_FL                0x40000000 /* Casefolded file */
#endif
    /* *INDENT-OFF* */
    { EXT2_SECRM_FL,        's', N_("Secure deletion"),               FALSE, NULL },
    { EXT2_UNRM_FL,         'u', N_("Undelete"),                      FALSE, NULL },
    { EXT2_SYNC_FL,         'S', N_("Synchronous updates"),           FALSE, NULL },
    { EXT2_DIRSYNC_FL,      'D', N_("Synchronous directory updates"), FALSE, NULL },
    { EXT2_IMMUTABLE_FL,    'i', N_("Immutable"),                     FALSE, NULL },
    { EXT2_APPEND_FL,       'a', N_("Append only"),                   FALSE, NULL },
    { EXT2_NODUMP_FL,       'd', N_("No dump"),                       FALSE, NULL },
    { EXT2_NOATIME_FL,      'A', N_("No update atime"),               FALSE, NULL },
    { EXT2_COMPR_FL,        'c', N_("Compress"),                      FALSE, NULL },
#ifdef EXT2_COMPRBLK_FL
    /* removed in v1.43-WIP-2015-05-18
       ext2fsprogs 4a05268cf86f7138c78d80a53f7e162f32128a3d 2015-04-12 */
    { EXT2_COMPRBLK_FL,     'B', N_("Compressed clusters"),           FALSE, NULL },
#endif
#ifdef EXT2_DIRTY_FL
    /* removed in v1.43-WIP-2015-05-18
       ext2fsprogs 4a05268cf86f7138c78d80a53f7e162f32128a3d 2015-04-12 */
    { EXT2_DIRTY_FL,        'Z', N_("Compressed dirty file"),         FALSE, NULL },
#endif
#ifdef EXT2_NOCOMPR_FL
    /* removed in v1.43-WIP-2015-05-18
       ext2fsprogs 4a05268cf86f7138c78d80a53f7e162f32128a3d 2015-04-12 */
    { EXT2_NOCOMPR_FL,      'X', N_("Compression raw access"),        FALSE, NULL },
#endif
    { EXT4_ENCRYPT_FL,      'E', N_("Encrypted inode"),               FALSE, NULL },
    { EXT3_JOURNAL_DATA_FL, 'j', N_("Journaled data"),                FALSE, NULL },
    { EXT2_INDEX_FL,        'I', N_("Indexed directory"),             FALSE, NULL },
    { EXT2_NOTAIL_FL,       't', N_("No tail merging"),               FALSE, NULL },
    { EXT2_TOPDIR_FL,       'T', N_("Top of directory hierarchies"),  FALSE, NULL },
    { EXT4_EXTENTS_FL,      'e', N_("Inode uses extents"),            FALSE, NULL },
#ifdef EXT4_HUGE_FILE_FL
    /* removed in v1.43.9
       ext2fsprogs 4825daeb0228e556444d199274b08c499ac3706c 2018-02-06 */
    { EXT4_HUGE_FILE_FL,    'h', N_("Huge_file"),                     FALSE, NULL },
#endif
    { FS_NOCOW_FL,          'C', N_("No COW"),                        FALSE, NULL },
#ifdef EXT4_CASEFOLD_FL
    /* added in v1.45.0
       ext2fsprogs 1378bb6515e98a27f0f5c220381d49d20544204e 2018-12-01 */
    { EXT4_CASEFOLD_FL,     'F', N_("Casefolded file"),               FALSE, NULL },
#endif
    { EXT4_INLINE_DATA_FL,  'N', N_("Inode has inline data"),         FALSE, NULL },
#ifdef EXT4_PROJINHERIT_FL
    /* added in v1.43-WIP-2016-05-12
       ext2fsprogs e1cec4464bdaf93ea609de43c5cdeb6a1f553483 2016-03-07
                   97d7e2fdb2ebec70c3124c1a6370d28ec02efad0 2016-05-09 */
    { EXT4_PROJINHERIT_FL,  'P', N_("Project hierarchy")              FALSE, NULL },
#endif
#ifdef EXT4_VERITY_FL
    /* added in v1.44.4
       ext2fsprogs faae7aa00df0abe7c6151fc4947aa6501b981ee1 2018-08-14
       v1.44.5
       ext2fsprogs 7e5a95e3d59719361661086ec7188ca6e674f139 2018-08-21 */
    { EXT4_VERITY_FL,       'V', N_("Verity protected inode"),        FALSE, NULL }
#endif
    /* *INDENT-ON* */
};

/* number of attributes */
static const size_t ATTR_NUM = G_N_ELEMENTS (check_attr);

/* number of real buttons (modifable attributes) */
static size_t check_attr_num = 0;

static int check_attr_len = 0;

static struct
{
    int ret_cmd;
    button_flags_t flags;
    int y;                      /* vertical position relatively to dialog bottom boundary */
    int len;
    const char *text;
} chattr_but[BUTTONS] =
{
    /* *INDENT-OFF* */
    { B_SETALL, NORMAL_BUTTON, 6, 0, N_("Set &all")      },
    { B_MARKED, NORMAL_BUTTON, 6, 0, N_("&Marked all")   },
    { B_SETMRK, NORMAL_BUTTON, 5, 0, N_("S&et marked")   },
    { B_CLRMRK, NORMAL_BUTTON, 5, 0, N_("C&lear marked") },
    { B_ENTER, DEFPUSH_BUTTON, 3, 0, N_("&Set")          },
    { B_CANCEL, NORMAL_BUTTON, 3, 0, N_("&Cancel")       }
    /* *INDENT-ON* */
};

static gboolean flags_change;
static int current_file;
static gboolean ignore_all;

static unsigned long and_mask, or_mask, flags;

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static void
chattr_i18n (void)
{
    static gboolean i18n = FALSE;
    size_t i;
    int len;

    if (i18n)
        return;

    i18n = TRUE;

#ifdef ENABLE_NLS
    for (i = 0; i < ATTR_NUM; i++)
        if ((check_attr[i].flags & EXT2_FL_USER_MODIFIABLE) != 0)
        {
            check_attr[i].text = _(check_attr[i].text);
            check_attr_num++;
        }

    for (i = 0; i < BUTTONS; i++)
        chattr_but[i].text = _(chattr_but[i].text);
#endif /* ENABLE_NLS */

    for (i = 0; i < ATTR_NUM; i++)
        if ((check_attr[i].flags & EXT2_FL_USER_MODIFIABLE) != 0)
        {
            len = str_term_width1 (check_attr[i].text);
            check_attr_len = MAX (check_attr_len, len);
        }

    check_attr_len += 1 + 3 + 1;        /* mark, [x] and space */

    for (i = 0; i < BUTTONS; i++)
    {
        chattr_but[i].len = str_term_width1 (chattr_but[i].text) + 3;     /* [], spaces and w/o & */
        if (chattr_but[i].flags == DEFPUSH_BUTTON)
            chattr_but[i].len += 2;      /* <> */
    }
}

/* --------------------------------------------------------------------------------------------- */

static WDialog *
chattr_init (const char *fname, unsigned long attr)
{
    gboolean single_set;
    WDialog *ch_dlg;
    int lines, cols;
    size_t i;
    int y;
    char buffer[BUF_TINY];

    flags_change = FALSE;

    single_set = (current_panel->marked < 2);
    perm_gb_len = check_attr_len + 2;
    cols = str_term_width1 (fname) + 2 + 1;

    lines = single_set ? 20 : 23;
    cols = perm_gb_len + 6;

    if (cols > COLS)
        cols = COLS;

    ch_dlg =
        dlg_create (TRUE, 0, 0, lines, cols, WPOS_CENTER, FALSE, dialog_colors,
                    chattr_callback, NULL, "[Chmod]", _("Chmod command"));

    add_widget (ch_dlg, groupbox_new (PY, PX, ATTR_NUM + 2, perm_gb_len, _("Permission")));

    for (i = 0; i < ATTR_NUM; i++)
        if ((check_attr[i].flags & EXT2_FL_USER_MODIFIABLE) != 0)
        {
            check_attr[i].check = check_new (PY + i + 1, PX + 2, (attr & check_attr[i].flags) != 0,
                                             check_attr[i].text);
            add_widget (ch_dlg, check_attr[i].check);
        }

    if (!single_set)
    {
        i = 0;
        add_widget (ch_dlg, hline_new (lines - chattr_but[i].y - 1, -1, -1));
        for (; i < BUTTONS - 2; i++)
        {
            y = lines - chattr_but[i].y;
            add_widget (ch_dlg,
                        button_new (y, WIDGET (ch_dlg)->cols / 2 - chattr_but[i].len,
                                    chattr_but[i].ret_cmd, chattr_but[i].flags, chattr_but[i].text,
                                    NULL));
            i++;
            add_widget (ch_dlg,
                        button_new (y, WIDGET (ch_dlg)->cols / 2 + 1,
                                    chattr_but[i].ret_cmd, chattr_but[i].flags, chattr_but[i].text,
                                    NULL));
        }
    }

    i = BUTTONS - 2;
    y = lines - chattr_but[i].y;
    add_widget (ch_dlg, hline_new (y - 1, -1, -1));
    add_widget (ch_dlg,
                button_new (y, WIDGET (ch_dlg)->cols / 2 - chattr_but[i].len, chattr_but[i].ret_cmd,
                            chattr_but[i].flags, chattr_but[i].text, NULL));
    i++;
    add_widget (ch_dlg,
                button_new (y, WIDGET (ch_dlg)->cols / 2 + 1, chattr_but[i].ret_cmd,
                            chattr_but[i].flags, chattr_but[i].text, NULL));

    /* select first checkbox */
    widget_select (WIDGET (check_attr[0].check));

    return ch_dlg;
}

/* --------------------------------------------------------------------------------------------- */

static void
chattr_done (gboolean need_update)
{
    if (need_update)
        update_panels (UP_OPTIMIZE, UP_KEEPSEL);
    repaint_screen ();
}

/* --------------------------------------------------------------------------------------------- */

static const char *
next_file (void)
{
    while (!current_panel->dir.list[current_file].f.marked)
        current_file++;

    return current_panel->dir.list[current_file].fname;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
try_chattr (const char *p, unsigned long a)
{
    while (fsetflags (p, a) == -1 && !ignore_all)
    {
        int my_errno = errno;
        int result;
        char *msg;

        msg =
             g_strdup_printf (_("Cannot chattr \"%s\"\n%s"), x_basename (p),
                              unix_error_string (my_errno));
        result =
            query_dialog (MSG_ERROR, msg, D_ERROR, 4, _("&Ignore"), _("Ignore &all"), _("&Retry"),
                          _("&Cancel"));
        g_free (msg);

        switch (result)
        {
        case 0:
            /* try next file */
            return TRUE;

        case 1:
            ignore_all = TRUE;
            /* try next file */
            return TRUE;

        case 2:
            /* retry this file */
            break;

        case 3:
        default:
            /* stop remain files processing */
            return FALSE;
        }
    }

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static void
chattr_done (gboolean need_update)
{
    if (need_update)
        update_panels (UP_OPTIMIZE, UP_KEEPSEL);
    repaint_screen ();
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
chattr_cmd (void)
{
    gboolean need_update;
    gboolean end_chattr;

    chattr_i18n ();

    current_file = 0;
    ignore_all = FALSE;

    do
    {                           /* do while any files remaining */
        vfs_path_t *vpath;
        WDialog *ch_dlg;
        struct stat sf_stat;
        const char *fname, *fname2
        size_t i;
        int result;

        if (!vfs_current_is_local ())
        {
            message (D_ERROR, MSG_ERROR, "%s", _("Cannot change attributes on non-local filesystems"));
            break;
        }

        do_refresh ();

        need_update = FALSE;
        end_chattr = FALSE;

        if (current_panel->marked != 0)
            fname = next_file ();       /* next marked file */
        else
            fname = selection (current_panel)->fname;   /* single file */

        vpath = vfs_path_from_str (fname);
        fname2 = vfs_path_as_str (vpath);

        if (fgetflags (fname2, &flags) != 0)
        {
            message (D_ERROR, MSG_ERROR, _("Cannot get flags of \"%s\"\n%s"), fname,
                     unix_error_string (errno));
            vfs_path_free (vpath);
            break;
        }

        ch_dlg = chattr_init (fname, flags);

        result = dlg_run (ch_dlg);

        switch (result)
        {
        case B_CANCEL:
            end_chattr = TRUE;
            break;

        case B_ENTER:
            if (flags_change)
            {
                if (current_panel->marked <= 1)
                {
                    /* single or last file */
                    if (fsetflags (fname2, flags) == -1 && !ignore_all)
                        message (D_ERROR, MSG_ERROR, _("Cannot chattr \"%s\"\n%s"), fname,
                                 unix_error_string (errno));
                    end_chattr = TRUE;
                }
                else if (!try_chattr (fname2, flags))
                {
                    /* stop multiple files processing */
                    result = B_CANCEL;
                    end_chattr = TRUE;
                }
            }

            need_update = TRUE;
            break;

        case B_SETALL:
        case B_MARKED:
            and_mask = or_mask = 0;
            and_mask = ~and_mask;

            for (i = 0; i < ATTR_NUM; i++)
                if ((check_attr[i].flags & EXT2_FL_USER_MODIFIABLE) != 0)
                    && (check_attr[i].selected || result == B_SETALL))
                {
                    if (check_attr[i].check->state)
                        or_mask |= check_attr[i].flags;
                    else
                        and_mask &= ~check_attr[i].flags;
                }

            apply_mask (vpath, &flags);
            need_update = TRUE;
            end_chattr = TRUE;
            break;

        case B_SETMRK:
            and_mask = or_mask = 0;
            and_mask = ~and_mask;

            for (i = 0; i < ATTR_NUM; i++)
                if ((check_attr[i].flags & EXT2_FL_USER_MODIFIABLE) != 0) && check_attr[i].selected)
                    or_mask |= check_attr[i].flags;

            apply_mask (vpath, &flags);
            need_update = TRUE;
            end_chattr = TRUE;
            break;

        case B_CLRMRK:
            and_mask = or_mask = 0;
            and_mask = ~and_mask;

            for (i = 0; i < ATTR_NUM; i++)
                if ((check_attr[i].flags & EXT2_FL_USER_MODIFIABLE) != 0) && check_attr[i].selected)
                    and_mask &= ~check_attr[i].flags;

            apply_mask (vpath, &flags);
            need_update = TRUE;
            end_chattr = TRUE;
            break;

        default:
            break;
        }

        if (current_panel->marked != 0 && result != B_CANCEL)
        {
            do_file_mark (current_panel, current_file, 0);
            need_update = TRUE;
        }

        vfs_path_free (vpath);

        dlg_destroy (ch_dlg);
    }
    while (current_panel->marked != 0 && !end_chattr);

    chattr_done (need_update);
}

/* --------------------------------------------------------------------------------------------- */
