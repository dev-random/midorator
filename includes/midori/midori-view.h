/*
 Copyright (C) 2008 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#ifndef __MIDORI_VIEW_H__
#define __MIDORI_VIEW_H__

#include "midori-websettings.h"

#include <katze/katze.h>

G_BEGIN_DECLS

typedef enum
{
    MIDORI_LOAD_PROVISIONAL,
    MIDORI_LOAD_COMMITTED,
    MIDORI_LOAD_FINISHED
} MidoriLoadStatus;

GType
midori_load_status_get_type (void) G_GNUC_CONST;

#define MIDORI_TYPE_LOAD_STATUS \
    (midori_load_status_get_type ())

typedef enum
{
    MIDORI_NEW_VIEW_TAB,
    MIDORI_NEW_VIEW_BACKGROUND,
    MIDORI_NEW_VIEW_WINDOW
} MidoriNewView;

GType
midori_new_view_get_type (void) G_GNUC_CONST;

#define MIDORI_TYPE_NEW_VIEW \
    (midori_new_view_get_type ())

#define MIDORI_TYPE_VIEW \
    (midori_view_get_type ())

typedef enum
{
    MIDORI_SECURITY_NONE, /* The connection is neither encrypted nor verified. */
    MIDORI_SECURITY_UNKNOWN, /* The security is unknown, due to lack of validation. */
    MIDORI_SECURITY_TRUSTED /* The security is validated and trusted. */
} MidoriSecurity;

GType
midori_security_get_type (void) G_GNUC_CONST;

#define MIDORI_TYPE_SECURITY \
    (midori_security_get_type ())

#define MIDORI_VIEW(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), MIDORI_TYPE_VIEW, MidoriView))
#define MIDORI_VIEW_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass), MIDORI_TYPE_VIEW, MidoriViewClass))
#define MIDORI_IS_VIEW(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MIDORI_TYPE_VIEW))
#define MIDORI_IS_VIEW_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE ((klass), MIDORI_TYPE_VIEW))
#define MIDORI_VIEW_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj), MIDORI_TYPE_VIEW, MidoriViewClass))

typedef struct _MidoriView                MidoriView;
typedef struct _MidoriViewClass           MidoriViewClass;

GType
midori_view_get_type                   (void) G_GNUC_CONST;

GtkWidget*
midori_view_new                        (KatzeNet*          net);

void
midori_view_set_settings               (MidoriView*        view,
                                        MidoriWebSettings* settings);

gdouble
midori_view_get_progress               (MidoriView*        view);

MidoriLoadStatus
midori_view_get_load_status            (MidoriView*        view);

void
midori_view_set_uri                    (MidoriView*        view,
                                        const gchar*       uri);

gboolean
midori_view_is_blank                   (MidoriView*        view);

const gchar*
midori_view_get_display_uri            (MidoriView*        view);

const gchar*
midori_view_get_display_title          (MidoriView*        view);

GdkPixbuf*
midori_view_get_icon                   (MidoriView*        view);

const gchar*
midori_view_get_icon_uri               (MidoriView*        view);

const gchar*
midori_view_get_link_uri               (MidoriView*        view);

gboolean
midori_view_has_selection              (MidoriView*        view);

const gchar*
midori_view_get_selected_text          (MidoriView*        view);

gboolean
midori_view_can_cut_clipboard          (MidoriView*        view);

gboolean
midori_view_can_copy_clipboard         (MidoriView*        view);

gboolean
midori_view_can_paste_clipboard        (MidoriView*        view);

GtkWidget*
midori_view_get_proxy_menu_item        (MidoriView*        view);

GtkWidget*
midori_view_get_tab_menu               (MidoriView*        view);

PangoEllipsizeMode
midori_view_get_label_ellipsize        (MidoriView*        view);

GtkWidget*
midori_view_get_proxy_tab_label        (MidoriView*        view);

KatzeItem*
midori_view_get_proxy_item             (MidoriView*        view);

gfloat
midori_view_get_zoom_level             (MidoriView*        view);

gboolean
midori_view_can_zoom_in                (MidoriView*        view);

gboolean
midori_view_can_zoom_out               (MidoriView*        view);

void
midori_view_set_zoom_level             (MidoriView*        view,
                                        gfloat             zoom_level);

gboolean
midori_view_can_reload                 (MidoriView*        view);

void
midori_view_reload                     (MidoriView*        view,
                                        gboolean           from_cache);

void
midori_view_stop_loading               (MidoriView*        view);

gboolean
midori_view_can_go_back                (MidoriView*        view);

void
midori_view_go_back                    (MidoriView*        view);

gboolean
midori_view_can_go_forward             (MidoriView*        view);

void
midori_view_go_forward                 (MidoriView*        view);

const gchar*
midori_view_get_previous_page          (MidoriView*        view);

const gchar*
midori_view_get_next_page              (MidoriView*        view);

gboolean
midori_view_can_print                  (MidoriView*        view);

void
midori_view_print                      (MidoriView*        view);

gboolean
midori_view_can_view_source            (MidoriView*        view);

gboolean
midori_view_can_find                   (MidoriView*        view);

void
midori_view_unmark_text_matches        (MidoriView*        view);

void
midori_view_search_text                (MidoriView*        view,
                                        const gchar*       text,
                                        gboolean           case_sensitive,
                                        gboolean           forward);

void
midori_view_mark_text_matches          (MidoriView*        view,
                                        const gchar*       text,
                                        gboolean           case_sensitive);

void
midori_view_set_highlight_text_matches (MidoriView*        view,
                                        gboolean           highlight);

gboolean
midori_view_execute_script             (MidoriView*        view,
                                        const gchar*       script,
                                        gchar**            exception);

GdkPixbuf*
midori_view_get_snapshot               (MidoriView*        view,
                                        gint               width,
                                        gint               height);

GtkWidget*
midori_view_get_web_view               (MidoriView*        view);

MidoriSecurity
midori_view_get_security               (MidoriView*        view);

void
midori_view_populate_popup             (MidoriView*        view,
                                        GtkWidget*         menu,
                                        gboolean           manual);


G_END_DECLS

#endif /* __MIDORI_VIEW_H__ */
