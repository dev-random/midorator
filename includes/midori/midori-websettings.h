/*
 Copyright (C) 2008-2010 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#ifndef __MIDORI_WEB_SETTINGS_H__
#define __MIDORI_WEB_SETTINGS_H__

#include <webkit/webkit.h>

#include <katze/katze.h>

G_BEGIN_DECLS

#define MIDORI_TYPE_WEB_SETTINGS \
    (midori_web_settings_get_type ())
#define MIDORI_WEB_SETTINGS(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), MIDORI_TYPE_WEB_SETTINGS, MidoriWebSettings))
#define MIDORI_WEB_SETTINGS_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass), MIDORI_TYPE_WEB_SETTINGS, MidoriWebSettingsClass))
#define MIDORI_IS_WEB_SETTINGS(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MIDORI_TYPE_WEB_SETTINGS))
#define MIDORI_IS_WEB_SETTINGS_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE ((klass), MIDORI_TYPE_WEB_SETTINGS))
#define MIDORI_WEB_SETTINGS_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj), MIDORI_TYPE_WEB_SETTINGS, MidoriWebSettingsClass))

typedef struct _MidoriWebSettings                MidoriWebSettings;
typedef struct _MidoriWebSettingsClass           MidoriWebSettingsClass;

enum
{
    MIDORI_CLEAR_NONE = 0,
    MIDORI_CLEAR_HISTORY = 1,
    MIDORI_CLEAR_COOKIES = 2,
    MIDORI_CLEAR_FLASH_COOKIES = 4,
    MIDORI_CLEAR_WEBSITE_ICONS = 8,
    MIDORI_CLEAR_TRASH = 16,
    MIDORI_CLEAR_ON_QUIT = 32,
    MIDORI_CLEAR_WEB_CACHE = 64,
};

typedef enum
{
    MIDORI_WINDOW_NORMAL,
    MIDORI_WINDOW_MINIMIZED,
    MIDORI_WINDOW_MAXIMIZED,
    MIDORI_WINDOW_FULLSCREEN,
} MidoriWindowState;

GType
midori_window_state_get_type (void) G_GNUC_CONST;

#define MIDORI_TYPE_WINDOW_STATE \
    (midori_window_state_get_type ())

typedef enum
{
    MIDORI_STARTUP_BLANK_PAGE,
    MIDORI_STARTUP_HOMEPAGE,
    MIDORI_STARTUP_LAST_OPEN_PAGES
} MidoriStartup;

GType
midori_startup_get_type (void) G_GNUC_CONST;

#define MIDORI_TYPE_STARTUP \
    (midori_startup_get_type ())

typedef enum
{
    MIDORI_ENCODING_CHINESE,
    MIDORI_ENCODING_JAPANESE,
    MIDORI_ENCODING_KOREAN,
    MIDORI_ENCODING_RUSSIAN,
    MIDORI_ENCODING_UNICODE,
    MIDORI_ENCODING_WESTERN,
    MIDORI_ENCODING_CUSTOM
} MidoriPreferredEncoding;

GType
midori_preferred_encoding_get_type (void) G_GNUC_CONST;

#define MIDORI_TYPE_PREFERRED_ENCODING \
    (midori_preferred_encoding_get_type ())

typedef enum
{
    MIDORI_NEW_PAGE_TAB,
    MIDORI_NEW_PAGE_WINDOW,
    MIDORI_NEW_PAGE_CURRENT
} MidoriNewPage;

GType
midori_new_page_get_type (void) G_GNUC_CONST;

#define MIDORI_TYPE_NEW_PAGE \
    (midori_new_page_get_type ())

typedef enum
{
    MIDORI_TOOLBAR_DEFAULT,
    MIDORI_TOOLBAR_ICONS,
    MIDORI_TOOLBAR_SMALL_ICONS,
    MIDORI_TOOLBAR_TEXT,
    MIDORI_TOOLBAR_BOTH,
    MIDORI_TOOLBAR_BOTH_HORIZ
} MidoriToolbarStyle;

GType
midori_toolbar_style_get_type (void) G_GNUC_CONST;

#define MIDORI_TYPE_TOOLBAR_STYLE \
    (midori_toolbar_style_get_type ())

typedef enum
{
    MIDORI_PROXY_AUTOMATIC,
    MIDORI_PROXY_HTTP,
    MIDORI_PROXY_NONE
} MidoriProxy;

GType
midori_proxy_get_type (void) G_GNUC_CONST;

#define MIDORI_TYPE_PROXY \
    (midori_proxy_get_type ())

typedef enum
{
    MIDORI_ACCEPT_COOKIES_ALL,
    MIDORI_ACCEPT_COOKIES_SESSION,
    MIDORI_ACCEPT_COOKIES_NONE
} MidoriAcceptCookies;

GType
midori_accept_cookies_get_type (void) G_GNUC_CONST;

#define MIDORI_TYPE_ACCEPT_COOKIES \
    (midori_accept_cookies_get_type ())

typedef enum
{
    MIDORI_IDENT_MIDORI,
    MIDORI_IDENT_SAFARI,
    MIDORI_IDENT_IPHONE,
    MIDORI_IDENT_FIREFOX,
    MIDORI_IDENT_EXPLORER,
    MIDORI_IDENT_CUSTOM,
} MidoriIdentity;

GType
midori_identity_get_type (void) G_GNUC_CONST;

#define MIDORI_TYPE_IDENTITY \
    (midori_identity_get_type ())

GType
midori_web_settings_get_type               (void) G_GNUC_CONST;

MidoriWebSettings*
midori_web_settings_new                    (void);

G_END_DECLS

#endif /* __MIDORI_WEB_SETTINGS_H__ */
