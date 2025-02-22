/*
   Widgets for the Midnight Commander

   Copyright (C) 1994-2019
   Free Software Foundation, Inc.

   Authors:
   Radek Doulik, 1994, 1995
   Miguel de Icaza, 1994, 1995
   Jakub Jelinek, 1995
   Andrej Borsenkow, 1996
   Norbert Warmuth, 1997
   Andrew Borodin <aborodin@vmail.ru>, 2009, 2010, 2011, 2012, 2013

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

/** \file history.c
 *  \brief Source: save, load and show history
 */

#include <config.h>

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "lib/global.h"

#include "lib/tty/tty.h"        /* LINES, COLS */
#include "lib/mcconfig.h"       /* for history loading and saving */
#include "lib/fileloc.h"
#include "lib/strutil.h"
#include "lib/util.h"           /* list_append_unique */
#include "lib/widget.h"
#include "lib/keybind.h"        /* CK_* */

/*** global variables ****************************************************************************/

/* how much history items are used */
int num_history_items_recorded = 60;

/*** file scope macro definitions ****************************************************************/

#define B_VIEW (B_USER + 1)
#define B_EDIT (B_USER + 2)

/*** file scope type declarations ****************************************************************/

typedef struct
{
    int y;
    int x;
    size_t count;
    size_t max_width;
} history_dlg_data;

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/

static cb_ret_t
history_dlg_reposition (WDialog * dlg_head)
{
    history_dlg_data *data;
    int x = 0, y, he, wi;

    /* guard checks */
    if ((dlg_head == NULL) || (dlg_head->data == NULL))
        return MSG_NOT_HANDLED;

    data = (history_dlg_data *) dlg_head->data;

    y = data->y;
    he = data->count + 2;

    if (he <= y || y > (LINES - 6))
    {
        he = MIN (he, y - 1);
        y -= he;
    }
    else
    {
        y++;
        he = MIN (he, LINES - y);
    }

    if (data->x > 2)
        x = data->x - 2;

    wi = data->max_width + 4;

    if ((wi + x) > COLS)
    {
        wi = MIN (wi, COLS);
        x = COLS - wi;
    }

    dlg_set_position (dlg_head, y, x, he, wi);

    return MSG_HANDLED;
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
history_dlg_callback (Widget * w, Widget * sender, widget_msg_t msg, int parm, void *data)
{
    switch (msg)
    {
    case MSG_RESIZE:
        return history_dlg_reposition (DIALOG (w));

    case MSG_NOTIFY:
        {
            /* message from listbox */
            WDialog *d = DIALOG (w);

            switch (parm)
            {
            case CK_View:
                d->ret_value = B_VIEW;
                break;
            case CK_Edit:
                d->ret_value = B_EDIT;
                break;
            case CK_Enter:
                d->ret_value = B_ENTER;
                break;
            default:
                return MSG_NOT_HANDLED;
            }

            dlg_stop (d);
            return MSG_HANDLED;
        }

    default:
        return dlg_default_callback (w, sender, msg, parm, data);
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
history_create_item (history_descriptor_t * hd, void *data)
{
    char *text = (char *) data;
    size_t width;

    width = str_term_width1 (text);
    hd->max_width = MAX (width, hd->max_width);

    listbox_add_item (hd->listbox, LISTBOX_APPEND_AT_END, 0, text, NULL, TRUE);
}

/* --------------------------------------------------------------------------------------------- */

static void *
history_release_item (history_descriptor_t * hd, WLEntry * le)
{
    void *text;

    (void) hd;

    text = le->text;
    le->text = NULL;

    return text;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/**
 * Load the history from the ${XDG_CACHE_HOME}/mc/history file.
 * It is called with the widgets history name and returns the GList list.
 */

GList *
history_get (const char *input_name)
{
    GList *hist = NULL;
    char *profile;
    mc_config_t *cfg;

    if (num_history_items_recorded == 0)        /* this is how to disable */
        return NULL;
    if ((input_name == NULL) || (*input_name == '\0'))
        return NULL;

    profile = mc_config_get_full_path (MC_HISTORY_FILE);
    cfg = mc_config_init (profile, TRUE);

    hist = history_load (cfg, input_name);

    mc_config_deinit (cfg);
    g_free (profile);

    return hist;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Load history from the mc_config
 */
GList *
history_load (mc_config_t * cfg, const char *name)
{
    size_t i;
    GList *hist = NULL;
    char **keys;
    size_t keys_num = 0;
    GIConv conv = INVALID_CONV;
    GString *buffer;

    if (name == NULL || *name == '\0')
        return NULL;

    /* get number of keys */
    keys = mc_config_get_keys (cfg, name, &keys_num);
    g_strfreev (keys);

    /* create charset conversion handler to convert strings
       from utf-8 to system codepage */
    if (!mc_global.utf8_display)
        conv = str_crt_conv_from ("UTF-8");

    buffer = g_string_sized_new (64);

    for (i = 0; i < keys_num; i++)
    {
        char key[BUF_TINY];
        char *this_entry;

        g_snprintf (key, sizeof (key), "%lu", (unsigned long) i);
        this_entry = mc_config_get_string_raw (cfg, name, key, "");

        if (this_entry == NULL)
            continue;

        if (conv == INVALID_CONV)
            hist = list_append_unique (hist, this_entry);
        else
        {
            g_string_set_size (buffer, 0);
            if (str_convert (conv, this_entry, buffer) == ESTR_FAILURE)
                hist = list_append_unique (hist, this_entry);
            else
            {
                hist = list_append_unique (hist, g_strndup (buffer->str, buffer->len));
                g_free (this_entry);
            }
        }
    }

    g_string_free (buffer, TRUE);
    if (conv != INVALID_CONV)
        str_close_conv (conv);

    /* return pointer to the last entry in the list */
    return g_list_last (hist);
}

/* --------------------------------------------------------------------------------------------- */

/**
  * Save history to the mc_config, but don't save config to file
  */
void
history_save (mc_config_t * cfg, const char *name, GList * h)
{
    GIConv conv = INVALID_CONV;
    GString *buffer;
    int i;

    if (name == NULL || *name == '\0' || h == NULL)
        return;

    /* go to end of list */
    h = g_list_last (h);

    /* go back 60 places */
    for (i = 0; (i < num_history_items_recorded - 1) && (h->prev != NULL); i++)
        h = g_list_previous (h);

    if (name != NULL)
        mc_config_del_group (cfg, name);

    /* create charset conversion handler to convert strings
       from system codepage to UTF-8 */
    if (!mc_global.utf8_display)
        conv = str_crt_conv_to ("UTF-8");

    buffer = g_string_sized_new (64);
    /* dump history into profile */
    for (i = 0; h != NULL; h = g_list_next (h))
    {
        char key[BUF_TINY];
        char *text = (char *) h->data;

        /* We shouldn't have null entries, but let's be sure */
        if (text == NULL)
            continue;

        g_snprintf (key, sizeof (key), "%d", i++);

        if (conv == INVALID_CONV)
            mc_config_set_string_raw (cfg, name, key, text);
        else
        {
            g_string_set_size (buffer, 0);
            if (str_convert (conv, text, buffer) == ESTR_FAILURE)
                mc_config_set_string_raw (cfg, name, key, text);
            else
                mc_config_set_string_raw (cfg, name, key, buffer->str);
        }
    }

    g_string_free (buffer, TRUE);
    if (conv != INVALID_CONV)
        str_close_conv (conv);
}

/* --------------------------------------------------------------------------------------------- */

void
history_descriptor_init (history_descriptor_t * hd, int y, int x, GList * history, int current)
{
    hd->list = history;
    hd->y = y;
    hd->x = x;
    hd->current = current;
    hd->action = CK_IgnoreKey;
    hd->text = NULL;
    hd->max_width = 0;
    hd->listbox = listbox_new (1, 1, 2, 2, TRUE, NULL);
    /* in most cases history list contains string only and no any other data */
    hd->create = history_create_item;
    hd->release = history_release_item;
    hd->free = g_free;
}

/* --------------------------------------------------------------------------------------------- */

void
history_show (history_descriptor_t * hd)
{
    GList *z, *hi;
    size_t count;
    WDialog *query_dlg;
    history_dlg_data hist_data;
    int dlg_ret;

    if (hd == NULL || hd->list == NULL)
        return;

    hd->max_width = str_term_width1 (_("History")) + 2;

    for (z = hd->list; z != NULL; z = g_list_previous (z))
        hd->create (hd, z->data);
    /* after this, the order of history items is following: recent at begin, oldest at end */

    count = listbox_get_length (hd->listbox);

    hist_data.y = hd->y;
    hist_data.x = hd->x;
    hist_data.count = count;
    hist_data.max_width = hd->max_width;

    query_dlg =
        dlg_create (TRUE, 0, 0, 4, 4, WPOS_KEEP_DEFAULT, TRUE, dialog_colors, history_dlg_callback,
                    NULL, "[History-query]", _("History"));
    query_dlg->data = &hist_data;

    /* this call makes list stick to all sides of dialog, effectively make
       it be resized with dialog */
    add_widget_autopos (query_dlg, hd->listbox, WPOS_KEEP_ALL, NULL);

    /* to avoid diplicating of (calculating sizes in two places)
       code, call history_dlg_callback function here, to set dialog and
       controls positions.
       The main idea - create 4x4 dialog and add 2x2 list in
       center of it, and let dialog function resize it to needed size. */
    send_message (query_dlg, NULL, MSG_RESIZE, 0, NULL);

    if (WIDGET (query_dlg)->y < hd->y)
    {
        /* history is below base widget -- place recent item on top */
        /* revert history direction */
        g_queue_reverse (hd->listbox->list);
        if (hd->current < 0 || (size_t) hd->current >= count)
            listbox_select_last (hd->listbox);
        else
            listbox_select_entry (hd->listbox, count - 1 - (size_t) hd->current);
    }
    else
    {
        /* history is above base widget -- place recent item at bottom  */
        if (hd->current > 0)
            listbox_select_entry (hd->listbox, hd->current);
    }

    dlg_ret = dlg_run (query_dlg);
    if (dlg_ret != B_CANCEL)
    {
        char *q;

        switch (dlg_ret)
        {
        case B_EDIT:
            hd->action = CK_Edit;
            break;
        case B_VIEW:
            hd->action = CK_View;
            break;
        default:
            hd->action = CK_Enter;
        }

        listbox_get_current (hd->listbox, &q, NULL);
        hd->text = g_strdup (q);
    }

    /* get modified history from dialog */
    z = NULL;
    for (hi = listbox_get_first_link (hd->listbox); hi != NULL; hi = g_list_next (hi))
        /* history is being reverted here again */
        z = g_list_prepend (z, hd->release (hd, LENTRY (hi->data)));

    /* restore history direction */
    if (WIDGET (query_dlg)->y >= hd->y)
        z = g_list_reverse (z);

    dlg_destroy (query_dlg);

    hd->list = g_list_first (hd->list);
    g_list_free_full (hd->list, hd->free);
    hd->list = g_list_last (z);
}

/* --------------------------------------------------------------------------------------------- */
