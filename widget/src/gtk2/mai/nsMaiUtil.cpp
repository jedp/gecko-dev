/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Sun Microsystems, Inc.
 * Portions created by Sun Microsystems are Copyright (C) 2002 Sun
 * Microsystems, Inc. All Rights Reserved.
 *
 * Original Author: Bolian Yin (bolian.yin@sun.com)
 *
 * Contributor(s): 
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include <stdlib.h>
#include "nsMaiHook.h"
#include "nsMaiUtil.h"
#include "nsMaiRoot.h"

void mai_init();
static void mai_util_class_init(MaiUtilClass *klass);

/* atkutil.h */

static guint mai_util_add_global_event_listener(GSignalEmissionHook listener,
                                                const gchar* event_type);
static void  mai_util_remove_global_event_listener(guint remove_listener);
static AtkObject* mai_util_get_root(void);
static G_CONST_RETURN gchar *mai_util_get_toolkit_name(void);
static gboolean mai_add_toplevel_accessible(nsIAccessible *toplevel);
static gboolean mai_remove_toplevel_accessible(nsIAccessible *toplevel);

/* Misc */

static void _listener_info_destroy(gpointer data);

static GHashTable *listener_list = NULL;
static gint listener_idx = 1;
typedef struct _MaiUtilListenerInfo MaiUtilListenerInfo;

/* supporting */
static int mai_initialized = FALSE;
static MaiHook maiHook;

static MaiRoot *mai_get_root_accessible(void);
static AtkObject* mai_get_root_atk_object(void);

struct _MaiUtilListenerInfo
{
    gint key;
    guint signal_id;
    gulong hook_id;
};

GType
mai_util_get_type(void)
{
    static GType type = 0;

    if (!type) {
        static const GTypeInfo tinfo = {
            sizeof(MaiUtilClass),
            (GBaseInitFunc) NULL, /* base init */
            (GBaseFinalizeFunc) NULL, /* base finalize */
            (GClassInitFunc) mai_util_class_init, /* class init */
            (GClassFinalizeFunc) NULL, /* class finalize */
            NULL, /* class data */
            sizeof(MaiUtil), /* instance size */
            0, /* nb preallocs */
            (GInstanceInitFunc) NULL, /* instance init */
            NULL /* value table */
        };

        type = g_type_register_static(ATK_TYPE_UTIL,
                                      "MaiUtil", &tinfo, GTypeFlags(0));
    }
    return type;
}

static void
mai_util_class_init(MaiUtilClass *klass)
{
    AtkUtilClass *atk_class;
    gpointer data;

    data = g_type_class_peek(ATK_TYPE_UTIL);
    atk_class = ATK_UTIL_CLASS(data);

    atk_class->add_global_event_listener =
        mai_util_add_global_event_listener;
    atk_class->remove_global_event_listener =
        mai_util_remove_global_event_listener;
    atk_class->get_root = mai_util_get_root;
    atk_class->get_toolkit_name = mai_util_get_toolkit_name;

    listener_list = g_hash_table_new_full(g_int_hash, g_int_equal, NULL,
                                          _listener_info_destroy);
}

static guint
mai_util_add_global_event_listener(GSignalEmissionHook listener,
                                   const gchar *event_type)
{
    GType type;
    guint signal_id;
    gchar **split_string;
    gint  rc = 0;

    split_string = g_strsplit(event_type, ":", 3);

    type = g_type_from_name(split_string[1]);
    if (type > 0) {
        signal_id  = g_signal_lookup(split_string[2], type);
        if (signal_id > 0) {
            MaiUtilListenerInfo *listener_info;

            rc = listener_idx;

            listener_info = (MaiUtilListenerInfo *)
                g_malloc(sizeof(MaiUtilListenerInfo));
            listener_info->key = listener_idx;
            listener_info->hook_id =
                g_signal_add_emission_hook(signal_id, 0, listener,
                                           g_strdup(event_type),
                                           (GDestroyNotify) g_free);
            listener_info->signal_id = signal_id;

            g_hash_table_insert(listener_list, &(listener_info->key),
                                listener_info);
            listener_idx++;
        }
        else {
            g_warning("Invalid signal type %s\n", split_string[2]);
        }
    }
    else {
        g_warning("Invalid object type %s\n", split_string[1]);
    }

    if (split_string != NULL)
        g_strfreev(split_string);

    return rc;
}

static void
mai_util_remove_global_event_listener(guint remove_listener)
{
    if (remove_listener > 0) {
        MaiUtilListenerInfo *listener_info;
        gint tmp_idx = remove_listener;

        listener_info = (MaiUtilListenerInfo *)
            g_hash_table_lookup(listener_list, &tmp_idx);

        if (listener_info != NULL) {
            /* Hook id of 0 and signal id of 0 are invalid */
            if (listener_info->hook_id != 0 && listener_info->signal_id != 0) {
                /* Remove the emission hook */
                g_signal_remove_emission_hook(listener_info->signal_id,
                                              listener_info->hook_id);

                /* Remove the element from the hash */
                g_hash_table_remove(listener_list, &tmp_idx);
            }
            else {
                /* do not use g_warning with such complex format args, */
                /* Forte CC can not preprocess it correctly */
                g_log((gchar *)0, G_LOG_LEVEL_WARNING,
                      "Invalid listener hook_id %ld or signal_id %d\n",
                      listener_info->hook_id, listener_info->signal_id);

            }
        }
        else {
            /* do not use g_warning with such complex format args, */
            /* Forte CC can not preprocess it correctly */
            g_log((gchar *)0, G_LOG_LEVEL_WARNING,
                  "No listener with the specified listener id %d",
                  remove_listener);
        }
    }
    else {
        g_warning("Invalid listener_id %d", remove_listener);
    }
}

static AtkObject*
mai_util_get_root(void)
{
    return mai_get_root_atk_object();
}

static G_CONST_RETURN gchar *
mai_util_get_toolkit_name(void)
{
    return "MAI";
}

static void
_listener_info_destroy(gpointer data)
{
    free(data);
}

void
mai_init()
{
    if (mai_initialized) {
        return;
    }
    mai_initialized = TRUE;
  
    g_print("Mozilla Atk Implementation initialized\n");
    g_type_init();
    /* Initialize the MAI Utility class */
    g_type_class_unref(g_type_class_ref(MAI_TYPE_UTIL));

    /* initialize the mai hook */
    gMaiHook = &maiHook;
    maiHook.AddTopLevelAccessible = mai_add_toplevel_accessible;
    maiHook.RemoveTopLevelAccessible = mai_remove_toplevel_accessible;
}

int
gtk_module_init(gint *argc, char** argv[])
{
    mai_init();
    return 0;
}

/* supporting funcs */
MaiRoot*
mai_get_root_accessible(void)
{
    static MaiRoot *gRootAccessible = NULL;

    if (gRootAccessible || (gRootAccessible = new MaiRoot()))
        return gRootAccessible;

    return NULL;
}

AtkObject*
mai_get_root_atk_object(void)
{
    static AtkObject *gRootAtkObject = NULL;
    MaiRoot *root;

    if (!gRootAtkObject && (root = mai_get_root_accessible()))
        gRootAtkObject = root->GetAtkObject();
    return gRootAtkObject;
}

gboolean
mai_add_toplevel_accessible(nsIAccessible *toplevel)
{
    g_return_val_if_fail(toplevel != NULL, TRUE);

    MaiRoot *root;
    root = mai_get_root_accessible();
    if (root) {
        root->AddTopLevelAccessible(toplevel);
        return TRUE;
    }
    else
        return FALSE;
}

gboolean
mai_remove_toplevel_accessible(nsIAccessible *toplevel)
{
    g_return_val_if_fail(toplevel != NULL, TRUE);

    MaiRoot *root;
    root = mai_get_root_accessible();
    if (root) {
        root->RemoveTopLevelAccessible(toplevel);
        return TRUE;
    }
    else
        return FALSE;
}
