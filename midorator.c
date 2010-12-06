#if 0
# /*
	sed -i 's:\(#[[:space:]]*define[[:space:]]\+MIDORATOR_VERSION[[:space:]]\+\).*:\1"0.0'"$(date -r "$0" '+%Y%m%d')"'":g' midorator.h
	make debug || exit 1
	cgdb midori
	exit $?
# */
#endif

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <midori/midori.h>
#include <JavaScriptCore/JavaScript.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "midorator.h"
#include "midorator-entry.h"
#include "midorator-history.h"
#include "midorator-commands.h"
#include "midorator-message.h"



static_f void midorator_current_view(GtkWidget **web_view);
static_f void midorator_del_browser_cb (MidoriExtension* extension, MidoriBrowser* browser);
static_f void midorator_add_browser_cb (MidoriApp* app, MidoriBrowser* browser, MidoriExtension* extension);
GtkWidget *midorator_entry(GtkWidget* web_view, const char *text);
static_f gboolean midorator_key_press_event_cb (GtkWidget* web_view, GdkEventKey* event, MidoriView* view);
static_f GtkStatusbar* midorator_find_sb(GtkWidget *w);
char midorator_mode(GtkWidget* web_view, char mode);
void midorator_message(GtkWidget* web_view, const char *message, const char *bg, const char *fg);
const char* midorator_options(const char *group, const char *name, const char *value);
static_f GtkWidget* midorator_js_get_wv(JSContextRef ctx);
static_f char* midorator_js_getattr(JSContextRef ctx, JSObjectRef obj, const char *name);
static_f char* midorator_html_decode(char *str);

#include "keycodes.h"

#undef g_signal_handlers_disconnect_by_func
#define g_signal_handlers_disconnect_by_func(i, f, d) (g_signal_handlers_disconnect_matched((i), G_SIGNAL_MATCH_FUNC, 0, 0, NULL, (f), NULL))


// replacement of original function, made to work with midori-0.2.4 that doesn't have it
GtkWidget *midori_view_get_web_view(MidoriView *view) {
	GtkWidget *w = GTK_WIDGET(view);
	GtkWidget *ret;
	midorator_findwidget_macro(w, ret, WEBKIT_IS_WEB_VIEW(ret));
	return ret;
}

GtkWidget *midori_view_from_web_view(GtkWidget *web_view) {
	GtkWidget *ret;
	for (ret = web_view; ret && !MIDORI_IS_VIEW(ret); ret = gtk_widget_get_parent(ret));
	return ret;
}

static_f gboolean midorator_string_to_bool(const char *string) {
	return string && (
		g_ascii_strcasecmp(string, "true") == 0 ||
		g_ascii_strcasecmp(string, "yes") == 0 ||
		g_ascii_strcasecmp(string, "on") == 0 ||
		g_ascii_strcasecmp(string, "+") == 0 ||
		g_ascii_strcasecmp(string, "1") == 0);
}

void midorator_error(GtkWidget *web_view, char *fmt, ...) {
	va_list l;
	va_start(l, fmt);
	char *msg = g_strdup_vprintf(fmt, l);
	va_end(l);
	midorator_message(web_view, msg, "red", "black");
	fprintf(stderr, "Error message: %s\n", msg);
#	ifdef DEBUG
		void *a[16];
		int len = backtrace(a, sizeof(a)/sizeof(a[0]));
		backtrace_symbols_fd(a, len, 2);
		fprintf(stderr, "\n");
#	endif
	g_free(msg);
}

GHashTable *opt_table = NULL;

// Sets option (if "value" is non-NULL) and returns its value.
// If value is supplied, returned value is not original one, but a table's internal copy.
const char* midorator_options(const char *group, const char *name, const char *value) {
	logextra("'%s', '%s', '%s'", group, name, value);
	if (!opt_table)
		opt_table = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, (GDestroyNotify)g_hash_table_destroy);
	GHashTable *settings = (GHashTable*)g_hash_table_lookup(opt_table, group);
	if (!settings) {
		settings = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
		g_hash_table_insert(opt_table, g_strdup(group), settings);
	}
	if (value)
		g_hash_table_insert(settings, g_strdup(name), g_strdup(value));
	return (const char *)g_hash_table_lookup(settings, name);
}

char ** midorator_options_keylist(const char *group) {
	if (!opt_table)
		opt_table = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, (GDestroyNotify)g_hash_table_destroy);
	GHashTable *settings = (GHashTable*)g_hash_table_lookup(opt_table, group);
	guint len = g_hash_table_size(settings);
	char **ret = g_new(char*, len + 1);
	GHashTableIter it;
	g_hash_table_iter_init(&it, settings);
	int i;
	void *r;
	for (i = 0; g_hash_table_iter_next(&it, &r, NULL); i++)
		ret[i] = g_strdup(r);
	ret[len] = NULL;
	return ret;
}

MidoriLocationAction *midorator_locationaction(MidoriBrowser *browser) {
	logextra("%p", browser);
	GtkActionGroup *actions = midori_browser_get_action_group(browser);
	GtkAction *action = gtk_action_group_get_action(actions, "Location");
	return MIDORI_LOCATION_ACTION (action);
}

GtkWidget *midorator_findwidget(GtkWidget *web_view, const char *name) {
	logextra("%p, '%s'", web_view, name);
	GtkWidget *w;
	if (strcmp(name, "view") == 0)
		return web_view;
	else if (strcmp(name, "tab") == 0) {
		midorator_findwidget_up_macro(web_view, w, GTK_IS_NOTEBOOK(gtk_widget_get_parent(w)));
		return w;
	} else if (strcmp(name, "tablabel") == 0) {
		midorator_findwidget_up_macro(web_view, w, GTK_IS_NOTEBOOK(gtk_widget_get_parent(w)));
		return gtk_notebook_get_tab_label(GTK_NOTEBOOK(gtk_widget_get_parent(w)), w);
	} else if (strcmp(name, "tablabeltext") == 0) {
		midorator_findwidget_up_macro(web_view, w, GTK_IS_NOTEBOOK(gtk_widget_get_parent(w)));
		midorator_findwidget_macro(gtk_notebook_get_tab_label(GTK_NOTEBOOK(gtk_widget_get_parent(w)), w), w, GTK_IS_LABEL(w));
		return w;
	} else if (strcmp(name, "tabs") == 0) {
		midorator_findwidget_up_macro(web_view, w, GTK_IS_NOTEBOOK(w));
		return w;
	} else if (strcmp(name, "scrollbox") == 0) {
		midorator_findwidget_up_macro(web_view, w, GTK_IS_SCROLLED_WINDOW(w));
		return w;
	} else if (strcmp(name, "hscroll") == 0) {
		midorator_findwidget_up_macro(web_view, w, GTK_IS_SCROLLED_WINDOW(w));
		return w ? GTK_WIDGET(gtk_scrolled_window_get_hscrollbar(GTK_SCROLLED_WINDOW(w))) : NULL;
	} else if (strcmp(name, "vscroll") == 0) {
		midorator_findwidget_up_macro(web_view, w, GTK_IS_SCROLLED_WINDOW(w));
		return w ? GTK_WIDGET(gtk_scrolled_window_get_vscrollbar(GTK_SCROLLED_WINDOW(w))) : NULL;
	} else if (strcmp(name, "statusbar") == 0) {
		return GTK_WIDGET(midorator_find_sb(web_view));
	} else {
		midorator_findwidget_up_macro(web_view, w, GTK_IS_NOTEBOOK(gtk_widget_get_parent(w)));
		midorator_findwidget_macro(w, w, strcmp(gtk_widget_get_name(w), name) == 0);
		if (!w) {
			midorator_findwidget_macro(gtk_widget_get_toplevel(web_view), w, strcmp(gtk_widget_get_name(w), name) == 0);
		}
		return w;
	}
}

void midorator_setclipboard(GdkAtom atom, const char *str) {
	logextra("%p, '%s'", atom, str);
	if (atom == GDK_NONE)
		atom = GDK_SELECTION_PRIMARY;
	GtkClipboard *c = gtk_clipboard_get(atom);
	if (!c)
		return;
	gtk_clipboard_set_text(c, str, -1);
}

char* midorator_getclipboard(GdkAtom atom) {
	logextra("%p", atom);
	if (atom == GDK_NONE)
		atom = GDK_SELECTION_PRIMARY;
	GtkClipboard *c = gtk_clipboard_get(atom);
	if (!c)
		return NULL;
	gchar *str = gtk_clipboard_wait_for_text(c);
	if (!str)
		return NULL;
	char *ret = strdup(str);
	g_free(str);
	logextra("ret: %s", ret);
	return ret;
}

static_f char* midorator_js_value_to_string(JSContextRef ctx, JSValueRef val) {
	JSStringRef js = JSValueToStringCopy(ctx, val, NULL);
	if (!js)
		return NULL;
	int len = JSStringGetMaximumUTF8CStringSize(js);
	char *ret = (char*)malloc(len + 1);
	JSStringGetUTF8CString(js, ret, len);
	JSStringRelease(js);
	return ret;
}

static_f bool midorator_js_error(JSContextRef ctx, JSValueRef e, const char *prefix) {
	if (!e || JSValueIsNull(ctx, e))
		return false;

	// Get preperty 'midorator_internal' (we stored pointer to 'web_view' there)
	JSStringRef ps = JSStringCreateWithUTF8CString("midorator_internal");
	JSObjectRef obj = JSContextGetGlobalObject(ctx);
	if (!JSObjectHasProperty(ctx, obj, ps)) {
		JSStringRelease(ps);
		return false;
	}
	JSValueRef v = JSObjectGetProperty(ctx, obj, ps, NULL);
	JSStringRelease(ps);
	
	if (!v || !JSValueIsString(ctx, v))
		return true;

	// Extract actual 'web_view' pointer from property
	char *buf = midorator_js_value_to_string(ctx, v);
	GtkWidget *web_view;
	sscanf(buf, "%p", &web_view);
	free(buf);

	if (!prefix)
		prefix = "JavaScript error";
	
	char *err = midorator_js_value_to_string(ctx, e);
	midorator_error(web_view, "%s: %s", prefix, err ? err : "(null)");
	free(err);
	return true;
}

static_f JSObjectRef midorator_js_v2o(JSContextRef ctx, JSValueRef val) {
	JSValueRef e = NULL;
	if (val == NULL) {
		midorator_js_error(ctx, JSValueMakeNumber(ctx, 0), "JS Error: Testing if value is object");
		return NULL;
	}
	JSObjectRef ret = JSValueToObject(ctx, val, &e);
	if (midorator_js_error(ctx, e, "JS Error: Testing if value is object"))
		return NULL;
	return ret;
}

static_f bool midorator_js_hasprop(JSContextRef ctx, JSObjectRef obj, const char *name) {
	if (!obj)
		obj = JSContextGetGlobalObject(ctx);
	JSStringRef ps = JSStringCreateWithUTF8CString(name);
	bool ret = JSObjectHasProperty(ctx, obj, ps);
	JSStringRelease(ps);
	return ret;
}

static_f JSValueRef midorator_js_getprop(JSContextRef ctx, JSObjectRef obj, const char *name) {
	if (!obj)
		obj = JSContextGetGlobalObject(ctx);
	JSStringRef ps = JSStringCreateWithUTF8CString(name);
	JSValueRef e = NULL;
	JSValueRef ret = JSObjectGetProperty(ctx, obj, ps, &e);
	JSStringRelease(ps);
	if (midorator_js_error(ctx, e, "Error while getting property"))
		return NULL;
	return ret;
}

static_f void midorator_js_setprop(JSContextRef ctx, JSObjectRef obj, const char *name, JSValueRef val) {
	if (!obj)
		obj = JSContextGetGlobalObject(ctx);
	JSStringRef ps = JSStringCreateWithUTF8CString(name);
	JSValueRef e = NULL;
	if (!val)
		JSObjectDeleteProperty(ctx, obj, ps, &e);
	else
		JSObjectSetProperty(ctx, obj, ps, val, 0, &e);
	midorator_js_error(ctx, e, NULL);
	JSStringRelease(ps);
}

static_f void midorator_js_setstrprop(JSContextRef ctx, JSObjectRef obj, const char *name, const char *val) {
	JSStringRef vs = JSStringCreateWithUTF8CString(val);
	midorator_js_setprop(ctx, obj, name, JSValueMakeString(ctx, vs));
	JSStringRelease(vs);
}

static_f bool midorator_js_is_js_enabled(JSContextRef ctx) {
	GtkWidget *web_view = midorator_js_get_wv(ctx);
	if (!web_view)
		return false;
	GtkWidget *view = midori_view_from_web_view(web_view);
	MidoriWebSettings* settings = katze_object_get_object (web_view, "settings");
	if (!settings) {
		MidoriBrowser* browser = midori_browser_get_for_widget(web_view);
		MidoriWebSettings* settings = katze_object_get_object (browser, "settings");
		if (!settings)
			return false;
	}
	return katze_object_get_boolean(settings, "enable-scripts");
}

static_f JSValueRef midorator_js_callprop_proto(JSContextRef ctx, JSObjectRef obj, const char *name, int argc, const JSValueRef argv[]) {
	JSValueRef prop = midorator_js_getprop(ctx, midorator_js_v2o(ctx, JSObjectGetPrototype(ctx, obj)), name);
	if (!prop || !JSValueIsObject(ctx, prop)) {
		//midorator_error(midorator_js_get_wv(ctx), "No such method: %s", name);
		return NULL;
	}
	JSObjectRef handler = JSValueToObject(ctx, prop, NULL);
	if (!handler)
		return NULL;
	if (!JSObjectIsFunction(ctx, handler)) {
		char *s = midorator_js_value_to_string(ctx, handler);
		midorator_error(midorator_js_get_wv(ctx), "Not a function: %s (%s)", name, s);
		free(s);
		return NULL;
	}
	JSValueRef ex = NULL;
	JSValueRef ret = JSObjectCallAsFunction(ctx, handler, obj, argc, argv, &ex);
	if (midorator_js_error(ctx, ex, "Error while calling JS method"))
		return NULL;
	return ret;
}

static_f JSValueRef midorator_js_callprop(JSContextRef ctx, JSObjectRef obj, const char *name, int argc, const JSValueRef argv[]) {
	JSValueRef prop = midorator_js_getprop(ctx, obj, name);
	if (!prop || !JSValueIsObject(ctx, prop)) {
		//midorator_error(midorator_js_get_wv(ctx), "No such method: %s", name);
		return NULL;
	}
	JSObjectRef handler = JSValueToObject(ctx, prop, NULL);
	if (!handler)
		return NULL;
	if (!JSObjectIsFunction(ctx, handler)) {
		char *s = midorator_js_value_to_string(ctx, handler);
		midorator_error(midorator_js_get_wv(ctx), "Not a function: %s (%s)", name, s);
		free(s);
		return NULL;
	}
	JSValueRef ex = NULL;
	JSValueRef ret = JSObjectCallAsFunction(ctx, handler, obj, argc, argv, &ex);
	if (midorator_js_error(ctx, ex, "Error while calling JS method"))
		return NULL;
	return ret;
}

static_f bool midorator_js_handle(JSContextRef ctx, JSObjectRef obj, const char *name, JSValueRef event) {
	if (!midorator_js_is_js_enabled(ctx))
		return true;
	if (name && name[0] == 'o' && name[1] == 'n')
		name += 2;
	if (!event) {
		JSValueRef doc = midorator_js_getprop(ctx, obj, "ownerDocument");
		JSStringRef s = JSStringCreateWithUTF8CString("Event");
		JSValueRef v = JSValueMakeString(ctx, s);
		JSStringRelease(s);
		event = midorator_js_callprop_proto(ctx, midorator_js_v2o(ctx, doc), "createEvent", 1, &v);

		s = JSStringCreateWithUTF8CString(name);
		JSValueRef args[] = { JSValueMakeString(ctx, s), JSValueMakeBoolean(ctx, true), JSValueMakeBoolean(ctx, true) };
		JSStringRelease(s);
		midorator_js_callprop(ctx, JSValueToObject(ctx, event, NULL), "initEvent", 3, args);
	}
	JSValueRef ret = midorator_js_callprop_proto(ctx, obj, "dispatchEvent", 1, &event);
	return JSValueToBoolean(ctx, ret);
}

typedef struct {
	JSValueRef val;	// public

	JSContextRef ctx;
	JSObjectRef arr;
	int index, length;
} midorator_js_array_iter;

static_f midorator_js_array_iter midorator_js_array_next(midorator_js_array_iter iter) {
	iter.val = NULL;
	iter.index++;
	if (iter.index >= iter.length)
		return iter;
	if (iter.arr) {
		JSValueRef num = JSValueMakeNumber(iter.ctx, iter.index);
		JSStringRef snum = JSValueToStringCopy(iter.ctx, num, NULL);
		iter.val = JSObjectGetProperty(iter.ctx, iter.arr, snum, NULL);
		JSStringRelease(snum);
	}
	return iter;
}

static_f midorator_js_array_iter midorator_js_array_prev(midorator_js_array_iter iter) {
	iter.val = NULL;
	iter.index--;
	if (iter.index < 0)
		return iter;
	if (iter.arr) {
		JSValueRef num = JSValueMakeNumber(iter.ctx, iter.index);
		JSStringRef snum = JSValueToStringCopy(iter.ctx, num, NULL);
		iter.val = JSObjectGetProperty(iter.ctx, iter.arr, snum, NULL);
		JSStringRelease(snum);
	}
	return iter;
}

static_f midorator_js_array_iter midorator_js_array_nth(JSContextRef ctx, JSObjectRef arr, int n) {
	midorator_js_array_iter ret = {NULL, ctx, arr, -1, 0};
	JSValueRef len = midorator_js_getprop(ctx, arr, "length");
	if (!len || !JSValueIsNumber(ctx, len))
		return ret;
	ret.length = (int)JSValueToNumber(ctx, len, NULL);
	ret.index = n - 1;
	return midorator_js_array_next(ret);
}

static_f midorator_js_array_iter midorator_js_array_first(JSContextRef ctx, JSObjectRef arr) {
	return midorator_js_array_nth(ctx, arr, 0);
}

static_f midorator_js_array_iter midorator_js_array_last(JSContextRef ctx, JSObjectRef arr) {
	midorator_js_array_iter ret = {NULL, ctx, arr, -1, 0};
	JSValueRef len = midorator_js_getprop(ctx, arr, "length");
	if (!len || !JSValueIsNumber(ctx, len))
		return ret;
	ret.index = ret.length = (int)JSValueToNumber(ctx, len, NULL);
	return midorator_js_array_prev(ret);
}

static_f void midorator_js_getrelpos(JSContextRef ctx, JSObjectRef el, double *l, double *t) {
	JSObjectRef parent = JSValueToObject(ctx, midorator_js_getprop(ctx, el, "parentNode"), NULL);
	JSObjectRef oparent = JSValueToObject(ctx, midorator_js_getprop(ctx, el, "offsetParent"), NULL);
	if (!parent) {
		*l = 0;
		*t = 0;
		return;
	}
	*l = JSValueToNumber(ctx, midorator_js_getprop(ctx, el, "offsetLeft"), NULL);
	*t = JSValueToNumber(ctx, midorator_js_getprop(ctx, el, "offsetTop"), NULL);
	if (isnan(*l) || isnan(*t)) {
		*l = 0;
		*t = 0;
		return;
	}
	if (JSValueIsEqual(ctx, parent, oparent, NULL))
		return;
	double ld = JSValueToNumber(ctx, midorator_js_getprop(ctx, parent, "offsetLeft"), NULL);
	double td = JSValueToNumber(ctx, midorator_js_getprop(ctx, parent, "offsetTop"), NULL);
	if (isnan(ld) || isnan(td))
		return;
	*l -= ld;
	*t -= td;
}

static_f struct _rect { int l, t, w, h; } midorator_js_getpos(JSContextRef ctx, JSObjectRef el) {
	struct _rect ret = { -1, -1, -1, -1 };
	/*	// OLD
	JSObjectRef rect_o = midorator_js_v2o(ctx, midorator_js_callprop_proto(ctx, el, "getBoundingClientRect", 0, NULL));
	if (!rect_o)
		return ret;*/
	JSObjectRef rect_o = midorator_js_v2o(ctx, midorator_js_callprop_proto(ctx, el, "getClientRects", 0, NULL));
	if (!rect_o)
		return ret;
	JSValueRef v = midorator_js_array_first(ctx, rect_o).val;
	if (v)
		rect_o = midorator_js_v2o(ctx, v);
	else
		rect_o = midorator_js_v2o(ctx, midorator_js_callprop_proto(ctx, el, "getBoundingClientRect", 0, NULL));
	if (!rect_o)
		return ret;
	ret.l = JSValueToNumber(ctx, midorator_js_getprop(ctx, rect_o, "left"), NULL);
	ret.t = JSValueToNumber(ctx, midorator_js_getprop(ctx, rect_o, "top"), NULL);
	ret.w = JSValueToNumber(ctx, midorator_js_getprop(ctx, rect_o, "width"), NULL);
	ret.h = JSValueToNumber(ctx, midorator_js_getprop(ctx, rect_o, "height"), NULL);

	JSObjectRef doc = midorator_js_v2o(ctx, midorator_js_getprop(ctx, el, "ownerDocument"));
	if (!doc)
		return ret;
	JSObjectRef w = midorator_js_v2o(ctx, midorator_js_getprop(ctx, doc, "defaultView"));
	if (!w)
		return ret;
	
	ret.l += JSValueToNumber(ctx, midorator_js_getprop(ctx, w, "pageXOffset"), NULL);
	ret.t += JSValueToNumber(ctx, midorator_js_getprop(ctx, w, "pageYOffset"), NULL);
	
	return ret;
}

static_f bool midorator_js_is_visible(JSContextRef ctx, JSObjectRef el) {
	JSObjectRef i;
	for (i = el; i; i = JSValueToObject(ctx, midorator_js_getprop(ctx, i, "parentNode"), NULL))
		if (JSValueToBoolean(ctx, midorator_js_getprop(ctx, i, "ownerDocument"))) {
			JSObjectRef style = midorator_js_v2o(ctx, midorator_js_getprop(ctx, i, "style"));
			if (!style)
				return false;
			char *display = midorator_js_value_to_string(ctx, midorator_js_getprop(ctx, style, "display"));
			if (g_ascii_strcasecmp(display, "none") == 0) {
				free(display);
				return false;
			}
			free(display);
			char *visibility = midorator_js_value_to_string(ctx, midorator_js_getprop(ctx, style, "visibility"));
			if (g_ascii_strcasecmp(visibility, "hidden") == 0) {
				free(visibility);
				return false;
			}
			free(visibility);
			char *type = midorator_js_value_to_string(ctx, midorator_js_getprop(ctx, i, "type"));
			if (g_ascii_strcasecmp(type, "hidden") == 0) {
				free(type);
				return false;
			}
			free(type);
		}

	// Check if element is on viewport:
	JSObjectRef doc = midorator_js_v2o(ctx, midorator_js_getprop(ctx, el, "ownerDocument"));
	JSObjectRef w = midorator_js_v2o(ctx, midorator_js_getprop(ctx, doc, "defaultView"));
	struct _rect r;
	r.l = JSValueToNumber(ctx, midorator_js_getprop(ctx, w, "pageXOffset"), NULL);
	r.t = JSValueToNumber(ctx, midorator_js_getprop(ctx, w, "pageYOffset"), NULL);
	r.w = JSValueToNumber(ctx, midorator_js_getprop(ctx, w, "innerWidth"), NULL);
	r.h = JSValueToNumber(ctx, midorator_js_getprop(ctx, w, "innerHeight"), NULL);
	for (i = el; JSValueToBoolean(ctx, midorator_js_getprop(ctx, i, "parentElement")); i = JSValueToObject(ctx, midorator_js_getprop(ctx, i, "parentElement"), NULL)) {
		JSObjectRef style = midorator_js_v2o(ctx, midorator_js_callprop_proto(ctx, w, "getComputedStyle", 1, (JSValueRef*)&i));
		if (i != el) {
			char *of = midorator_js_value_to_string(ctx, midorator_js_getprop(ctx, style, "overflow"));
			char *ofx = midorator_js_value_to_string(ctx, midorator_js_getprop(ctx, style, "overflowX"));
			char *ofy = midorator_js_value_to_string(ctx, midorator_js_getprop(ctx, style, "overflowY"));
			if (strcmp(of, "auto") && strcmp(of, "scroll") && strcmp(of, "hidden") &&
					strcmp(ofx, "auto") && strcmp(ofx, "scroll") && strcmp(ofx, "hidden") &&
					strcmp(ofy, "auto") && strcmp(ofy, "scroll") && strcmp(ofy, "hidden")) {
				free(of);
				free(ofx);
				free(ofy);
				continue;
			}
			free(of);
			free(ofx);
			free(ofy);
		}
		struct _rect r2 = midorator_js_getpos(ctx, i);
		if (r.l < r2.l) {
			r.w -= r2.l - r.l;
			r.l = r2.l;
		}
		if (r.t < r2.t) {
			r.h -= r2.t - r.t;
			r.t = r2.t;
		}
		if (r.l + r.w > r2.l + r2.w)
			r.w = r2.l + r2.w - r.l;
		if (r.t + r.h > r2.t + r2.h)
			r.h = r2.t + r2.h - r.t;
		if (r.w <= 0 || r.h <= 0)
			return false;
	}

	return true;
}

static_f JSValueRef midorator_js_do_find_elements(JSContextRef ctx, JSObjectRef w, const char *sel) {
	JSObjectRef ac = JSValueToObject(ctx, midorator_js_getprop(ctx, NULL, "Array"), NULL);
	JSValueRef ret = JSObjectCallAsConstructor(ctx, ac, 0, NULL, NULL);
	if (!ret || !JSValueToObject(ctx, ret, NULL))
		*((char*)NULL) = 1;
	JSObjectRef frames = JSValueToObject(ctx, midorator_js_getprop(ctx, w, "frames"), NULL);
	midorator_js_array_iter i;
	for (i = midorator_js_array_first(ctx, frames); i.val; i = midorator_js_array_next(i)) {
		JSValueRef sub = midorator_js_do_find_elements(ctx, JSValueToObject(ctx, i.val, NULL), sel);
		if (JSValueIsObject(ctx, sub))
			ret = midorator_js_callprop(ctx, JSValueToObject(ctx, ret, NULL), "concat", 1, &sub);
	}

	JSObjectRef doc = JSValueToObject(ctx, midorator_js_getprop(ctx, w, "document"), NULL);
	if (!doc)
		return JSValueMakeNull(ctx);	// happens when frame is blocked by AdBlock
	JSStringRef ss = JSStringCreateWithUTF8CString(sel);
	JSValueRef sv = JSValueMakeString(ctx, ss);
	JSObjectRef all = midorator_js_v2o(ctx, midorator_js_callprop(ctx, doc, "querySelectorAll", 1, &sv));
	JSStringRelease(ss);
	if (all)
		for (i = midorator_js_array_first(ctx, all); i.val; i = midorator_js_array_next(i))
			if (midorator_js_is_visible(ctx, JSValueToObject(ctx, i.val, NULL))) {
				midorator_js_callprop(ctx, JSValueToObject(ctx, ret, NULL), "push", 1, &i.val);
				if (!ret || !JSValueToObject(ctx, ret, NULL))
					*((char*)NULL) = 1;
			}
	return ret;
}

static_f JSValueRef midorator_js_find_elements(JSContextRef ctx, const char *action) {
	const char *selectors = NULL;
	if (action) {
		char *a = g_strconcat("hint_", action, NULL);
		selectors = midorator_options("option", a, NULL);
		g_free(a);
	}
	if (!selectors || !selectors[0])
		selectors = midorator_options("option", "hint_default", NULL);
	if (!selectors || !selectors[0])
		return NULL;
	JSValueRef ret = midorator_js_do_find_elements(ctx, JSContextGetGlobalObject(ctx), selectors);
	return ret;
}

static_f JSObjectRef midorator_js_do_find_frame(JSContextRef ctx, const char *name, JSObjectRef win) {
	if (!win)
		return NULL;
	JSStringRef n = JSValueToStringCopy(ctx, midorator_js_getprop(ctx, win, "name"), NULL);
	if (JSStringIsEqualToUTF8CString(n, name)) {
		JSStringRelease(n);
		return win;
	}
	midorator_js_array_iter iter;
	for (iter = midorator_js_array_first(ctx, JSValueToObject(ctx, midorator_js_getprop(ctx, win, "frames"), NULL)); iter.val; iter = midorator_js_array_next(iter)) {
		JSObjectRef ret = midorator_js_do_find_frame(ctx, name, JSValueToObject(iter.ctx, iter.val, NULL));
		if (ret)
			return ret;
	}
	return NULL;
}

static_f JSObjectRef midorator_js_find_frame(JSContextRef ctx, const char *name) {
	return midorator_js_do_find_frame(ctx, name, JSContextGetGlobalObject(ctx));
}

static_f void midorator_js_delhints(JSContextRef ctx, JSObjectRef win) {
	JSObjectRef doc = JSValueToObject(ctx, midorator_js_getprop(ctx, win, "document"), NULL);
	if (!doc)		// AdBlock again
		return;
	JSStringRef s = JSStringCreateWithUTF8CString("midorator_hint");
	JSValueRef vs = JSValueMakeString(ctx, s);
	JSStringRelease(s);
	JSObjectRef hints = JSValueToObject(ctx, midorator_js_callprop_proto(ctx, doc, "getElementsByName", 1, &vs), NULL);

	midorator_js_array_iter iter;
	for (iter = midorator_js_array_last(ctx, hints); iter.val; iter = midorator_js_array_prev(iter)) {
		JSObjectRef parent = midorator_js_v2o(ctx, midorator_js_getprop(ctx, midorator_js_v2o(ctx, iter.val), "parentNode"));
		midorator_js_callprop_proto(ctx, parent, "removeChild", 1, &iter.val);
	}

	for (iter = midorator_js_array_first(ctx, JSValueToObject(ctx, midorator_js_getprop(ctx, win, "frames"), NULL)); iter.val; iter = midorator_js_array_next(iter))
		midorator_js_delhints(ctx, midorator_js_v2o(iter.ctx, iter.val));
}

static_f JSObjectRef midorator_js_create_element(JSContextRef ctx, const char *tag, JSObjectRef near) {
	JSObjectRef doc;
	if (near)
		doc = JSValueToObject(ctx, midorator_js_getprop(ctx, near, "ownerDocument"), NULL);
	else
		doc = JSValueToObject(ctx, midorator_js_getprop(ctx, NULL, "document"), NULL);
	if (!doc) {
		//((char*)NULL)[0] = 1;
		midorator_error(midorator_js_get_wv(ctx), "JS Error: Creating element: target document not found");
		return NULL;
	}
	JSStringRef sr = JSStringCreateWithUTF8CString(tag);
	JSValueRef str = JSValueMakeString(ctx, sr);
	JSValueRef ret = midorator_js_callprop_proto(ctx, doc, "createElement", 1, &str);
	JSStringRelease(sr);
	if (!ret || !JSValueIsObject(ctx, ret)) {
		midorator_error(midorator_js_get_wv(ctx), "JS Error: Creating element: not created");
		return NULL;
	}
	JSObjectRef style = JSValueToObject(ctx, midorator_js_getprop(ctx, JSValueToObject(ctx, ret, NULL), "style"), NULL);
	if (style) {
		midorator_js_setstrprop(ctx, style, "position", "absolute");
	}
	char *ename = midorator_js_value_to_string(ctx, midorator_js_getprop(ctx, near, "tagName"));
	/*if (g_ascii_strcasecmp(ename, "td") == 0 || g_ascii_strcasecmp(ename, "li") == 0) {
		JSObjectRef child = JSValueToObject(ctx, midorator_js_getprop(ctx, near, "firstChild"), NULL);
		if (child) {
			JSValueRef args[] = { ret, child };
			midorator_js_callprop(ctx, near, "insertBefore", 2, args);
		} else
			midorator_js_callprop(ctx, near, "appendChild", 1, &ret);
	} else {
		JSObjectRef parent = midorator_js_v2o(ctx, midorator_js_getprop(ctx, near, "parentNode"));
		if (parent) {
			JSValueRef args[] = { ret, near };
			midorator_js_callprop(ctx, parent, "insertBefore", 2, args);
		} else
			midorator_error(midorator_js_get_wv(ctx), "JS Error: Creating element: can't find parent");
	}*/
	JSObjectRef body = midorator_js_v2o(ctx, midorator_js_getprop(ctx, doc, "body"));
	midorator_js_callprop_proto(ctx, body, "appendChild", 1, &ret);

	midorator_js_setprop(ctx, JSValueToObject(ctx, ret, NULL), "near", near);
	return JSValueToObject(ctx, ret, NULL);
}

static_f void midorator_js_setattr(JSContextRef ctx, JSObjectRef obj, const char *name, const char *value) {
	JSStringRef sn = JSStringCreateWithUTF8CString(name);
	JSStringRef sv = JSStringCreateWithUTF8CString(value);
	JSValueRef args[] = { JSValueMakeString(ctx, sn), JSValueMakeString(ctx, sv) };
	midorator_js_callprop_proto(ctx, obj, "setAttribute", 2, args);
	JSStringRelease(sn);
	JSStringRelease(sv);
}

static_f char* midorator_js_getattr(JSContextRef ctx, JSObjectRef obj, const char *name) {
	JSStringRef sn = JSStringCreateWithUTF8CString(name);
	JSValueRef args[] = { JSValueMakeString(ctx, sn) };
	JSValueRef v = midorator_js_callprop_proto(ctx, obj, "getAttribute", 1, args);
	JSStringRelease(sn);
	if (!v || JSValueIsNull(ctx, v))
		return NULL;
	return midorator_js_value_to_string(ctx, v);
}

static_f GtkWidget* midorator_js_get_wv(JSContextRef ctx) {
	// Get preperty 'midorator_internal' (we stored pointer to 'web_view' there)
	JSValueRef v = midorator_js_getprop(ctx, NULL, "midorator_internal");
	
	if (!JSValueIsString(ctx, v))
		return NULL;

	// Extract actual 'web_view' pointer from property
	char *buf = midorator_js_value_to_string(ctx, v);
	GtkWidget *web_view;
	sscanf(buf, "%p", &web_view);
	free(buf);

	return web_view;
}

static_f void midorator_js_form_click(JSContextRef ctx, JSObjectRef item, bool force_submit) {
	if (!item)
		return;
	char *tagname = midorator_js_value_to_string(ctx, midorator_js_getprop(ctx, item, "tagName"));
	if (!tagname)
		return;

	JSObjectRef form;
	if (g_ascii_strcasecmp(tagname, "form") == 0)
		form = item;
	else
		form = JSValueToObject(ctx, midorator_js_getprop(ctx, item, "form"), NULL);
	free(tagname);

	if (item != form) {
		midorator_js_callprop_proto(ctx, item, "focus", 0, NULL);
		midorator_mode(midorator_js_get_wv(ctx), 'i');
		if (!force_submit) {
			midorator_js_callprop_proto(ctx, item, "click", 0, NULL);
			return;
		}
	}

	if (form) {
		if (!force_submit && !midorator_js_handle(ctx, form, "onsubmit", NULL))
			return;
		// TODO: do something with "name=value" of button
		midorator_js_callprop_proto(ctx, form, "submit", 0, NULL);
	} else
		midorator_error(midorator_js_get_wv(ctx), "To submit a form, select it first");
}

static_f void midorator_js_script(JSContextRef ctx, const char *code, int force) {
	if (!force && !midorator_js_is_js_enabled(ctx))
		return;
	JSStringRef scode = JSStringCreateWithUTF8CString(code);
	JSValueRef e = NULL;
	JSEvaluateScript(ctx, scode, NULL, NULL, 0, &e);
	JSStringRelease(scode);
	midorator_js_error(ctx, e, code);
}

static_f void midorator_js_open(JSContextRef ctx, const char *href, JSObjectRef frame) {
	if (strncmp(href, "javascript:", strlen("javascript:")) == 0) {
		if (!midorator_js_is_js_enabled(ctx))
			return;
		char *js = g_uri_unescape_string(href + strlen("javascript:"), NULL);
		midorator_js_script(ctx, js, false);
		g_free(js);
		return;
	}
	JSObjectRef location = JSValueToObject(ctx, midorator_js_getprop(ctx, frame, "location"), NULL);
	if (!location)
		return;
	JSStringRef uri = JSStringCreateWithUTF8CString(href);
	JSValueRef vuri = JSValueMakeString(ctx, uri);
	midorator_js_callprop(ctx, location, "assign", 1, &vuri);
	JSStringRelease(uri);
}

static_f void midorator_js_click(JSContextRef ctx, JSObjectRef item, bool manual) {
	if (JSValueToObject(ctx, midorator_js_getprop(ctx, item, "form"), NULL)) {
		midorator_js_form_click(ctx, item, false);
		return;
	}
	midorator_js_callprop_proto(ctx, item, "focus", 0, NULL);
	if (midorator_js_is_js_enabled(ctx) && !manual)
		midorator_js_handle(ctx, item, "onclick", NULL);
	else {
		char *href = midorator_js_value_to_string(ctx, midorator_js_getprop(ctx, item, "href"));
		if (!href)
			return;
		char *tg = midorator_js_value_to_string(ctx, midorator_js_getprop(ctx, item, "target"));
		if (tg && !tg[0]) {
			free(tg);
			midorator_js_open(ctx, href, NULL);
		} else if (tg) {
			JSObjectRef frame = midorator_js_find_frame(ctx, tg);
			midorator_js_open(ctx, href, frame);
			free(tg);
		} else
			midorator_js_open(ctx, href, NULL);
		free(href);
	}
}

static_f JSValueRef midorator_js_genhint(JSContextRef ctx, JSObjectRef el, const char *text, bool scroll) {
	JSObjectRef hint = midorator_js_create_element(ctx, "div", el);
	if (!hint)
		return JSValueMakeNull(ctx);
	midorator_js_setstrprop(ctx, hint, "innerText", text);
	midorator_js_setattr(ctx, hint, "name", "midorator_hint");
	const char *css = midorator_options("option", "hintstyle", NULL);
	struct _rect p = midorator_js_getpos(ctx, el);
	char *pos = g_strdup_printf("left: %ipx !important; top: %ipx; !important; ", p.l, p.t);
	char *fullstyle = g_strconcat(
			"width:auto; height:auto; text-decoration:none; font-weight:normal; margin:0; padding:0; border:none; font-style:normal; font-family: Terminus monospace fixed; ",
			css, pos,
			"display:inline-block !important; position:absolute !important;",
			NULL);
	g_free(pos);
	midorator_js_setattr(ctx, hint, "style", fullstyle);
	g_free(fullstyle);
	if (scroll)
		midorator_js_callprop_proto(ctx, hint, "scrollIntoViewIfNeeded", 0, NULL);
	return hint;
}

static_f JSValueRef midorator_js_callback(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception) {
	if (argumentCount < 1 || !JSValueIsString(ctx, arguments[0]))
		return JSValueMakeNull(ctx);
	
	GtkWidget *web_view = midorator_js_get_wv(ctx);
	if (!web_view)
		return JSValueMakeNull(ctx);
	JSStringRef param = JSValueToStringCopy(ctx, arguments[0], NULL);

	// Process commands
	if (JSStringIsEqualToUTF8CString(param, "hide entry"))
		midorator_entry(web_view, NULL);
	else if (JSStringIsEqualToUTF8CString(param, "insert mode"))
		midorator_mode(web_view, 'n');
	else if (JSStringIsEqualToUTF8CString(param, "click")) {
		JSStringRelease(param);
		if (argumentCount < 2 || !JSValueIsObject(ctx, arguments[1]))
			return JSValueMakeNull(ctx);
		JSObjectRef obj = JSValueToObject(ctx, arguments[1], NULL);
		if (!obj)
			return JSValueMakeNull(ctx);
		midorator_js_click(ctx, obj, false);
		return JSValueMakeNull(ctx);
	} else if (JSStringIsEqualToUTF8CString(param, "tabnew") || JSStringIsEqualToUTF8CString(param, "bgtab") || JSStringIsEqualToUTF8CString(param, "multitab")) {
		bool bg = ! JSStringIsEqualToUTF8CString(param, "tabnew");
		bool multi = JSStringIsEqualToUTF8CString(param, "multitab");
		JSStringRelease(param);
		if (argumentCount < 2 || !JSValueIsObject(ctx, arguments[1]))
			return JSValueMakeNull(ctx);
		JSObjectRef obj = JSValueToObject(ctx, arguments[1], NULL);
		if (!obj)
			return JSValueMakeNull(ctx);
		char *tagname = midorator_js_value_to_string(ctx, midorator_js_getprop(ctx, obj, "tagName"));
		if (tagname) {
			if (strcmp(tagname, "A") == 0) {
				char *href = midorator_js_value_to_string(ctx, midorator_js_getprop(ctx, obj, "href"));
				if (href) {
					midorator_process_command(web_view, NULL, bg ? "tabnew!" : "tabnew", href);
					free(href);
				}
			}
			free(tagname);
		}
		if (multi)
			midorator_process_command(web_view, "entry ;m");
		return JSValueMakeNull(ctx);
	} else if (JSStringIsEqualToUTF8CString(param, "yank")) {
		JSStringRelease(param);
		if (argumentCount < 2 || !JSValueIsObject(ctx, arguments[1]))
			return JSValueMakeNull(ctx);
		JSObjectRef obj = JSValueToObject(ctx, arguments[1], NULL);
		if (!obj)
			return JSValueMakeNull(ctx);
		char *tagname = midorator_js_value_to_string(ctx, midorator_js_getprop(ctx, obj, "tagName"));
		if (tagname) {
			if (strcmp(tagname, "A") == 0) {
				char *href = midorator_js_value_to_string(ctx, midorator_js_getprop(ctx, obj, "href"));
				if (href) {
					midorator_setclipboard(GDK_SELECTION_PRIMARY, href);
					midorator_setclipboard(GDK_SELECTION_CLIPBOARD, href);
					free(href);
				}
			}
			free(tagname);
		}
		return JSValueMakeNull(ctx);
	} else if (JSStringIsEqualToUTF8CString(param, "genhint")) {
//fprintf(stderr, "%s():%i, %i\n", __func__, __LINE__, (int)time(NULL));
		JSStringRelease(param);
		if (argumentCount < 3 || !JSValueIsObject(ctx, arguments[1]) || !JSValueIsString(ctx, arguments[2]))
			return JSValueMakeNull(ctx);
		JSObjectRef el = JSValueToObject(ctx, arguments[1], NULL);
		char *text = midorator_js_value_to_string(ctx, arguments[2]);
		JSValueRef hint = midorator_js_genhint(ctx, el, text, false);
		free(text);
		return hint;
	} else if (JSStringIsEqualToUTF8CString(param, "getelems")) {
		JSStringRelease(param);
		char *s = midorator_js_value_to_string(ctx, arguments[1]);
		JSValueRef v = midorator_js_find_elements(ctx, s);
		free(s);
		return v;
	} else if (JSStringIsEqualToUTF8CString(param, "delhints")) {
		midorator_js_delhints(ctx, NULL);
	}
	// TODO other commands

	JSStringRelease(param);
	return JSValueMakeNull(ctx);
}

static_f JSValueRef midorator_js_private_callback(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception) {
	if (argumentCount < 1 || argumentCount > 1024)
		return JSValueMakeNull(ctx);
	char *cmd[argumentCount + 1];
	int i;
	for (i=0; i < argumentCount; i++)
		cmd[i] = midorator_js_value_to_string(ctx, arguments[i]);
	cmd[argumentCount] = NULL;

	if (cmd[0] && strcmp(cmd[0], "get") == 0) {
		char *ret = midorator_process_request(midorator_js_get_wv(ctx), cmd + 1, argumentCount - 1);
		for (i=0; i < argumentCount; i++)
			free(cmd[i]);
		if (ret) {
			JSStringRef s = JSStringCreateWithUTF8CString(ret);
			g_free(ret);
			JSValueRef retJ = JSValueMakeString(ctx, s);
			JSStringRelease(s);
			return retJ;
		} else
			return JSValueMakeNull(ctx);
	} else {
		bool ret = midorator_process_command_v(midorator_js_get_wv(ctx), cmd, argumentCount);
		for (i=0; i < argumentCount; i++)
			free(cmd[i]);
		return JSValueMakeBoolean(ctx, ret);
	}
}

static_f void midorator_js_hints(JSContextRef ctx, const char *charset, const char *follow, const char *cmd) {
	int i;
	midorator_js_delhints(ctx, NULL);

	if (!charset || !follow || !cmd)
		return;

	if (follow[strspn(follow, charset)])
		return;	// invalid char entered

	int cslen = strlen(charset);
	if (cslen < 2)
		return;

	JSObjectRef elems = NULL;
	if (follow && follow[0])
		elems = JSValueToObject(ctx, midorator_js_getprop(ctx, NULL, "midorator_hinted_elements"), NULL);
	if (!elems)
		elems = midorator_js_v2o(ctx, midorator_js_find_elements(ctx, cmd));
	if (!elems)
		return;
	midorator_js_setprop(ctx, NULL, "midorator_hinted_elements", elems);

	midorator_js_array_iter iter = midorator_js_array_first(ctx, elems);
	int count = iter.length;
	int keylen = 0;
	for (i = count - 1; i; i = div(i, cslen).quot)
		keylen++;
	if (keylen == 0)
		keylen = 1;

	int num = 0;
	for (i = 0; follow[i]; i++)
		num = num * cslen + (strchr(charset, follow[i]) - charset);
	for (/*i = i*/; i < keylen; i++)
		num *= cslen;

	if (strlen(follow) == keylen && num < count) {
		midorator_entry(midorator_js_get_wv(ctx), NULL);
		iter = midorator_js_array_nth(ctx, elems, num);
		JSStringRef cs = JSStringCreateWithUTF8CString(cmd);
		JSValueRef args[] = { JSValueMakeString(ctx, cs), iter.val };
		JSStringRelease(cs);
		midorator_js_callback(ctx, NULL, NULL, 2, args, NULL);
		return;
	}

	int remcount = 1;
	for (i = strlen(follow); i < keylen; i++)
		remcount *= cslen;

	for (iter = midorator_js_array_nth(ctx, elems, num); iter.val && iter.index < num + remcount; iter = midorator_js_array_next(iter)) {
		char text[keylen + 1];
		text[keylen] = 0;
		int n = iter.index;
		for (i = keylen - 1; i >= 0; i--) {
			div_t dt = div(n, cslen);
			text[i] = charset[dt.rem];
			n = dt.quot;
		}
		midorator_js_genhint(ctx, midorator_js_v2o(ctx, iter.val), text + strlen(follow), follow[0] ? true : false);
	}
}

static_f void midorator_js_go(JSContextRef ctx, const char *direction) {
	midorator_js_array_iter iter;
	JSObjectRef doc = midorator_js_v2o(ctx, midorator_js_getprop(ctx, NULL, "document"));
	if (!doc)
		return;
	JSObjectRef all = midorator_js_v2o(ctx, midorator_js_getprop(ctx, doc, "all"));
	if (!all)
		return;
	for (iter = midorator_js_array_first(ctx, all); iter.val; iter = midorator_js_array_next(iter)) {
		char *rel = midorator_js_getattr(ctx, midorator_js_v2o(ctx, iter.val), "rel");
		if (!rel)
			continue;
		if (g_ascii_strcasecmp(rel, direction) == 0) {
			free(rel);
			midorator_js_click(ctx, midorator_js_v2o(ctx, iter.val), true);
			return;
		}
		free(rel);
	}
	char *dir2 = g_strconcat("go_", direction, NULL);
	const char *rule = midorator_options("option", dir2, NULL);
	g_free(dir2);
	if (!rule || !rule[0])
		return;
	char **res = g_strsplit(rule, ",", -1);
	if (!res)
		return;
	GRegex *tagre = g_regex_new("<[^>]*>", 0, 0, NULL);
	int i;
	JSObjectRef links = midorator_js_v2o(ctx, midorator_js_getprop(ctx, doc, "links"));
	for (i=0; res[i]; i++) {
		for (rule = res[i]; strchr(" \t\n\r", rule[0]); rule++);
		if (!links) {
			g_strfreev(res);
			return;
		}
		GError *e = NULL;
		GRegex *re = g_regex_new(rule, G_REGEX_CASELESS, 0, &e);
		if (e) {
			midorator_error(midorator_js_get_wv(ctx), "Invalid regex in go_%s: %s", direction, e->message);
			g_strfreev(res);
			g_error_free(e);
			return;
		}
		for (iter = midorator_js_array_first(ctx, links); iter.val; iter = midorator_js_array_next(iter)) {
			char *html = midorator_js_value_to_string(ctx, midorator_js_getprop(ctx, midorator_js_v2o(ctx, iter.val), "innerHTML"));
			if (!html)
				continue;
			if (strlen(html) > 128) {
				g_free(html);
				continue;
			}
			char *text = midorator_html_decode(g_regex_replace_literal(tagre, html, -1, 0, "", 0, NULL));
			g_free(html);
			if (!text)
				continue;
			if (g_regex_match(re, text, 0, NULL)) {
				g_strfreev(res);
				g_free(text);
				g_regex_unref(re);
				g_regex_unref(tagre);
				midorator_js_click(ctx, midorator_js_v2o(ctx, iter.val), false);
				return;
			}
			g_free(text);
		}
		g_regex_unref(re);
	}
	g_regex_unref(tagre);
	g_strfreev(res);
}

static_f void midorator_js_execute_user_script(JSContextRef ctx, const char *code) {
	JSStringRef s = JSStringCreateWithUTF8CString("command");
	JSStringRef body = JSStringCreateWithUTF8CString(code);
	JSValueRef cb = JSObjectMakeFunctionWithCallback(ctx, s, midorator_js_private_callback);
	JSValueRef e = NULL;
	JSObjectRef script = JSObjectMakeFunction(ctx, NULL, 1, &s, body, NULL, 1, &e);
	JSStringRelease(body);
	JSStringRelease(s);
	if (midorator_js_error(ctx, e, "Parsing user script"))
		return;
	JSObjectCallAsFunction(ctx, script, NULL, 1, &cb, &e);
	midorator_js_error(ctx, e, "Executing user script");
}

static_f void midorator_make_js_callback(JSContextRef ctx, GtkWidget *web_view) {
	JSObjectRef global = JSContextGetGlobalObject(ctx);

	JSStringRef s = JSStringCreateWithUTF8CString("midorator_command");

	if (!JSObjectHasProperty(ctx, global, s)) {
		JSStringRef sw = JSStringCreateWithUTF8CString("midorator_internal");
		JSObjectRef func = JSObjectMakeFunctionWithCallback(ctx, s, midorator_js_callback);
		JSObjectSetProperty(ctx, global, s, func, kJSPropertyAttributeReadOnly, NULL);
		char buf[32] = {};
		snprintf(buf, 31, "%p", web_view);
		JSStringRef sv = JSStringCreateWithUTF8CString(buf);
		JSObjectSetProperty(ctx, global, sw, JSValueMakeString(ctx, sv), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(sv);
		JSStringRelease(sw);
	}

	JSStringRelease(s);
}

// Decodes html-entities IN-PLACE. Returns its argument.
static_f char* midorator_html_decode(char *str) {
	/*static const char *tre_text = "&([[:alnum:]]+);";
	static const char *tre_num = "&#([0-9]+);";
	static const char *tre_amp = "&#38;|&amp;";*/
	char *in, *out;
	for (in = out = str; (out[0] = in[0]); in++, out++)
		if (in[0] == '&') {
			char *end = strchr(in, ';');
			if (!end)
				continue;
			if (in[1] == '#' && (in[2] == 'x' || in[2] == 'X')) {
				char *end2;
				long n = strtol(&in[3], &end2, 16);
				if (end != end2)
					continue;
				in = end;
				out += -1 + g_unichar_to_utf8(n, out);
			} else if (in[1] == '#') {
				char *end2;
				long n = strtol(&in[2], &end2, 10);
				if (end != end2)
					continue;
				in = end;
				out += -1 + g_unichar_to_utf8(n, out);
			} else if (strncmp(in, "&amp;", strlen("&amp;")) == 0) {
				in += -1 + strlen("&amp;");
			} else if (strncmp(in, "&lt;", strlen("&lt;")) == 0) {
				in += -1 + strlen("&lt;");
				out[0] = '<';
			} else if (strncmp(in, "&gt;", strlen("&gt;")) == 0) {
				in += -1 + strlen("&gt;");
				out[0] = '>';
			} // TODO: more entities
		}
	return str;
}

void midorator_hints(GtkWidget* web_view, const char *charset, const char *follow, const char *cmd) {
	WebKitWebFrame *frame = webkit_web_view_get_main_frame(WEBKIT_WEB_VIEW(web_view));
	if (!frame)
		return;
	JSGlobalContextRef ctx = webkit_web_frame_get_global_context(frame);
	if (!ctx)
		return;
	midorator_make_js_callback(ctx, GTK_WIDGET(web_view));
	midorator_js_hints(ctx, charset, follow, cmd);
}

void midorator_go(GtkWidget* web_view, const char *dir) {
	WebKitWebFrame *frame = webkit_web_view_get_main_frame(WEBKIT_WEB_VIEW(web_view));
	if (!frame)
		return;
	JSGlobalContextRef ctx = webkit_web_frame_get_global_context(frame);
	if (!ctx)
		return;
	midorator_make_js_callback(ctx, GTK_WIDGET(web_view));
	midorator_js_go(ctx, dir);
}

void midorator_jscmd(GtkWidget* web_view, const char *code, char **args, int argnum) {
	WebKitWebFrame *frame = webkit_web_view_get_main_frame(WEBKIT_WEB_VIEW(web_view));
	if (!frame)
		return;
	JSGlobalContextRef ctx = webkit_web_frame_get_global_context(frame);
	if (!ctx)
		return;
	midorator_make_js_callback(ctx, GTK_WIDGET(web_view));
	JSObjectRef ac = JSValueToObject(ctx, midorator_js_getprop(ctx, NULL, "Array"), NULL);
	JSObjectRef arr = JSValueToObject(ctx, JSObjectCallAsConstructor(ctx, ac, 0, NULL, NULL), NULL);
	int i;
	for (i=0; i<argnum; i++) {
		JSStringRef s = JSStringCreateWithUTF8CString(args[i]);
		JSValueRef v = JSValueMakeString(ctx, s);
		JSStringRelease(s);
		midorator_js_callprop(ctx, arr, "push", 1, &v);
	}
	
	JSStringRef an[] = { JSStringCreateWithUTF8CString("command"), JSStringCreateWithUTF8CString("args") };
	JSStringRef body = JSStringCreateWithUTF8CString(code);
	JSValueRef av[] = { JSObjectMakeFunctionWithCallback(ctx, an[0], midorator_js_private_callback), arr };
	JSValueRef e = NULL;
	JSObjectRef script = JSObjectMakeFunction(ctx, NULL, 2, an, body, NULL, 1, &e);
	JSStringRelease(body);
	JSStringRelease(an[0]);
	JSStringRelease(an[1]);
	if (midorator_js_error(ctx, e, "Parsing user script"))
		return;
	JSObjectCallAsFunction(ctx, script, NULL, 2, av, &e);
	midorator_js_error(ctx, e, "Executing user script");
}

void midorator_submit_form(GtkWidget* web_view) {
	WebKitWebFrame *frame = webkit_web_view_get_main_frame(WEBKIT_WEB_VIEW(web_view));
	if (!frame)
		return;
	JSGlobalContextRef ctx = webkit_web_frame_get_global_context(frame);
	if (!ctx)
		return;
	JSObjectRef doc = midorator_js_v2o(ctx, midorator_js_getprop(ctx, NULL, "document"));
	if (!doc)
		return;
	JSObjectRef elem = midorator_js_v2o(ctx, midorator_js_getprop(ctx, doc, "activeElement"));
	if (!elem)
		return;
	JSObjectRef form = midorator_js_v2o(ctx, midorator_js_getprop(ctx, elem, "form"));
	if (!form)
		return;
	midorator_js_form_click(ctx, form, true);
}

void midorator_execute_user_script(GtkWidget *web_view, const char *code) {
	WebKitWebFrame *frame = webkit_web_view_get_main_frame(WEBKIT_WEB_VIEW(web_view));
	if (!frame)
		return;
	JSGlobalContextRef ctx = webkit_web_frame_get_global_context(frame);
	if (!ctx)
		return;
	midorator_make_js_callback(ctx, GTK_WIDGET(web_view));
	midorator_js_execute_user_script(ctx, code);
}

static_f GtkStatusbar* midorator_find_sb(GtkWidget *w) {
	/*for (; w; w = gtk_widget_get_parent(w))
		if (GTK_IS_VBOX(w)) {
			GList *l = gtk_container_get_children(GTK_CONTAINER(w));
			GList *li;
			if (l) {
				for (li = l; li; li = li->next)
					if (GTK_IS_STATUSBAR(li->data)) {
						GtkStatusbar *ret = GTK_STATUSBAR(li->data);
						g_list_free(l);
						return ret;
					}
				g_list_free(l);
			}
		}*/
	midorator_findwidget_macro(gtk_widget_get_toplevel(w), w, GTK_IS_STATUSBAR(w));
	return GTK_STATUSBAR(w);
}


void midorator_search(GtkWidget* web_view, const char *match, gboolean forward, gboolean remember) {
	static char *lastmatch = NULL;
	WebKitWebView *wv = WEBKIT_WEB_VIEW(web_view);
	if (match && remember) {
		if (lastmatch)
			free(lastmatch);
		lastmatch = strdup(match);
	}
	if (!match && !lastmatch)
		return;
	if (match && remember) {
		webkit_web_view_unmark_text_matches(wv);
		if (lastmatch[0])
			webkit_web_view_mark_text_matches(wv, match, false, -1);
	}
	webkit_web_view_search_text(wv, match ? match : lastmatch, false, forward, true);
	webkit_web_view_set_highlight_text_matches(wv, true);
}

static_f void midorator_entry_edited_cb(GtkEntry *e) {
	GtkWidget* web_view = NULL;
	g_object_get(e, "current-browser", &web_view, NULL);
	midorator_current_view(&web_view);
	const char *t = gtk_entry_get_text(e);
	if (!t)
		return;
	if (t[0] == ';' && t[1])
		midorator_process_command(web_view, "hint %s", t + 1);
	else if (t[0] == '/')
		midorator_search(web_view, t + 1, true, false);
	else if (t[0] == '?')
		midorator_search(web_view, t + 1, false, false);
}

static_f void midorator_entry_cancel_cb (GtkEntry* e) {
	GtkWidget* web_view = NULL;
	g_object_get(e, "current-browser", &web_view, NULL);
	midorator_current_view(&web_view);
	gtk_widget_grab_focus(web_view);
	midorator_process_command(web_view, "unhint");
}

static_f void midorator_entry_execute_cb (GtkEntry* e, const char *t) {
	GtkWidget* web_view = NULL;
	g_object_get(e, "current-browser", &web_view, NULL);
	midorator_current_view(&web_view);
	gtk_widget_grab_focus(web_view);
	if (t[0] == ':')
		midorator_process_command(web_view, "%s", t + 1);
	else if (t[0] == ';' && t[1])
		midorator_process_command(web_view, "hint %s", t + 1);
	else if (t[0] == '/')
		midorator_search(web_view, t + 1, true, true);
	else if (t[0] == '?')
		midorator_search(web_view, t + 1, false, true);
	midorator_process_command(web_view, "unhint");
}

static_f void midorator_entry_completion_cb (GtkEntry* e, const char *t) {
	if (!t || t[0] != ':')
		return;
	if (!strchr(t, ' ')) {
		KatzeArray *list = midorator_commands_list();
		int l = katze_array_get_length(list);
		int i;
		for (i = 0; i < l; i++) {
			KatzeItem *it = katze_array_get_nth_item(list, i);
			char *str = g_strconcat(":", katze_item_get_token(it), NULL);
			katze_item_set_token(it, str);
			g_free(str);
		}
		g_object_set(e, "completion-array", list, NULL);
		g_object_unref(list);
	}
	// TODO
}

// Is used to fix focus. Sometimes after switching tabs, input focus remains on old tab
// (now background). This function replaces background web_view with foreground.
static_f void midorator_current_view(GtkWidget **web_view) {
	GtkWidget *w;
	for (w = *web_view; w && !GTK_IS_NOTEBOOK(w); w = gtk_widget_get_parent(w));
	if (!w)
		return;
	GtkNotebook *n = GTK_NOTEBOOK(w);
	w = gtk_notebook_get_nth_page(n, gtk_notebook_get_current_page(n));
	GList *l = gtk_container_get_children(GTK_CONTAINER(w));
	GList *li;
	for (li = l; li; li = li->next) {
		if (KATZE_IS_SCROLLED(li->data)) {
			li = gtk_container_get_children(GTK_CONTAINER(l->data));
			g_list_free(l);
			l = li;
		}
		if (WEBKIT_IS_WEB_VIEW(li->data))
			*web_view = GTK_WIDGET(li->data);
	}
	g_list_free(l);
}

static_f gboolean midorator_is_current_view(GtkWidget *w) {
	midorator_findwidget_up_macro(w, w, w && GTK_IS_NOTEBOOK(gtk_widget_get_parent(w)));
	if (!w)
		return false;
	GtkNotebook *n = GTK_NOTEBOOK(gtk_widget_get_parent(w));
	return gtk_notebook_page_num(n, w) == gtk_notebook_get_current_page(n);
}

GtkWidget *midorator_entry(GtkWidget* web_view, const char *text) {
	if (!text)
		text = "";

	// Find box
	GtkStatusbar *sb = midorator_find_sb(web_view);
	if (!sb)
		return;
	GtkWidget *w = gtk_statusbar_get_message_area(sb);

	// Find existing entry in box
	GList *l = gtk_container_get_children(GTK_CONTAINER(w));
	GList *li;
	GtkEntry *e = NULL;
	for (li = l; li; li = li->next)
		if (GTK_IS_ENTRY(li->data)) {
			e = GTK_ENTRY(li->data);
			break;
		}
	g_list_free(l);

	// Add new entry if not found
	if (!e) {
		e = GTK_ENTRY(midorator_entry_new(w));
		logextra("%p", e);
		MidoriApp *app = g_object_get_data(G_OBJECT(web_view), "midori-app");
		KatzeArray *a = midorator_history_get_command_history(app);
		g_object_set(e, "command-history", a, NULL);
		g_signal_connect (e, "notify::text",
			G_CALLBACK (midorator_entry_edited_cb), NULL);
		g_signal_connect (e, "execute",
			G_CALLBACK (midorator_entry_execute_cb), NULL);
		g_signal_connect (e, "cancel",
			G_CALLBACK (midorator_entry_cancel_cb), NULL);
		g_signal_connect (e, "completion-request",
			G_CALLBACK (midorator_entry_completion_cb), NULL);
	}

	// Set text and stuff
	g_object_set(e, "current-browser", web_view, NULL);
	const char *oldtext = gtk_entry_get_text(e);
	gtk_entry_set_text(e, text);
	gtk_editable_set_position(GTK_EDITABLE(e), -1);
	/*if (strcmp(oldtext, text) != 0)
		g_signal_emit_by_name(e, "changed");*/
	return GTK_WIDGET(e);
}

void midorator_uri_cb (GtkWidget *widget, MidoriBrowser *browser) {
	MidoriLocationAction* location_action = midorator_locationaction(browser);
	const char *uri = midori_location_action_get_uri (location_action);
	if (!uri || !uri[0])
		uri = "about:blank";
	GtkStatusbar *sb = midorator_find_sb(widget);
	//guint id = gtk_statusbar_get_context_id(sb, "midorator");
	guint id = 42;
	gtk_statusbar_pop(sb, id);
	gtk_statusbar_push(sb, id, uri);
}

void midorator_uri_push_cb (GtkStatusbar *statusbar, guint context_id, gchar *text, MidoriBrowser *browser) {
	//g_signal_stop_emission_by_name(statusbar, "text-pushed");
	if (context_id == 1 && (!text || !text[0])) {
		midorator_uri_cb (GTK_WIDGET(statusbar), browser);
	}
}

static_f void midorator_show_mode(GtkWidget* web_view, const char *str) {
	GtkStatusbar *sb = midorator_find_sb(web_view);
	if (!sb)
		return;
	GtkBox *box = GTK_BOX(gtk_statusbar_get_message_area(sb));
	if (!box)
		return;
	GList *l = gtk_container_get_children(GTK_CONTAINER(box));
	GList *li;
	GtkLabel *lab = NULL;
	for (li = l; li; li = li->next)
		if (GTK_IS_LABEL(li->data) && gtk_widget_get_name(GTK_WIDGET(li->data)) && strcmp(gtk_widget_get_name(GTK_WIDGET(li->data)), "midorator_mode") == 0)
			lab = GTK_LABEL(li->data);
	if (l)
		g_list_free(l);
	if (lab) {
		gtk_label_set_text(lab, str);
	} else {
		GtkWidget *w = gtk_label_new(str);
		lab = GTK_LABEL(w);
		gtk_widget_set_name(w, "midorator_mode");
		gtk_box_pack_start(box, w, false, false, 0);
		gtk_widget_show(w);
		gtk_box_reorder_child(box, w, 0);
		PangoAttrList *attr = pango_attr_list_new();
		pango_attr_list_insert(attr, pango_attr_weight_new(PANGO_WEIGHT_BOLD));
		gtk_label_set_attributes(lab, attr);
	}
}

gboolean (*midorator_midori_browser_key_press_event_orig) (GtkWidget* widget, GdkEventKey* event) = NULL;

static_f gboolean midorator_midori_browser_key_press_event_cb (GtkWidget* widget, GdkEventKey* event) {
	if (gtk_window_propagate_key_event (GTK_WINDOW(widget), event))
		return true;
	else
		return midorator_midori_browser_key_press_event_orig(widget, event);
}

char midorator_mode(GtkWidget* web_view, char mode) {
	static const char* modenames[256] = {};
	if (!modenames['i']) {
		modenames['i'] = "-- INSERT --";
		modenames['n'] = "";
		modenames['?'] = "-- UNKNOWN MODE --";
	}
	const char *modestring;
	if (mode) {
		modestring = modenames[mode];
		if (!modestring)
			modestring = modenames['?'];
	} else {
		GtkLabel *l = GTK_LABEL(midorator_findwidget(web_view, "midorator_mode"));
		modestring = gtk_label_get_text(l);
		if (!modestring)
			modestring = "";
		mode = '?';
		int i;
		for (i = 0; i < 256; i++)
			if (modenames[i] && strcmp(modenames[i], modestring) == 0) {
				mode = i;
				break;
			}
	}
	if (web_view) {
		midorator_show_mode(web_view, modestring);

		// Hack to let our key sequences override Midori's accelerators.
		// (property 'gtk-enable-accels' doesn't work in Midori)
		GtkWidget *win = gtk_widget_get_toplevel(GTK_WIDGET(web_view));
		GtkWidgetClass *class = GTK_WIDGET_CLASS(G_OBJECT_GET_CLASS(G_OBJECT(win)));
		if (class->key_press_event != midorator_midori_browser_key_press_event_cb) {
			midorator_midori_browser_key_press_event_orig = class->key_press_event;
			class->key_press_event = midorator_midori_browser_key_press_event_cb;
		}
	}
	return mode;
}

static_f gboolean midorator_key_press_event_cb (GtkWidget* web_view, GdkEventKey* event, MidoriView* view) {
	logextra("%p, %p, %p", web_view, event, view);
	logextra("%i", event->keyval);
	midorator_message(web_view, NULL, NULL, NULL);
	midorator_current_view(&web_view);
	gtk_widget_grab_focus(web_view);

	gboolean insert = midorator_mode(web_view, 0) == 'i';
	if (insert) {
		if (event->keyval == GDK_Escape) {
			midorator_mode(web_view, 'n');
			return true;
		} else
			return false;
	}

	static int numprefix = 0;
	static char *sequence = NULL;
	if (event->keyval >= GDK_0 && event->keyval <= GDK_9 && !sequence) {
		numprefix = numprefix * 10 + event->keyval - GDK_0;
		return true;
	} else if (event->is_modifier) {
		return true;
		// Do nothing
	} else {
		GdkKeymap *km = gdk_keymap_get_default();
		logextra("%p", km);
		GdkKeymapKey kk = {event->hardware_keycode};
		logextra("%i, %i, %i", event->hardware_keycode, event->state, event->group);
		gdk_keymap_translate_keyboard_state(km, event->hardware_keycode, event->state, event->group, NULL, NULL, &kk.level, NULL);
		guint kv = gdk_keymap_lookup_key(km, &kk);
		logextra("%i", kv);
		if (!kv)
			kv = event->keyval;
		
		if (!sequence)
			sequence = g_strdup_printf("%03x;", kv);
		else {
			char *os = sequence;
			sequence = g_strdup_printf("%s%03x;", os, kv);
			g_free(os);
		}
		int mask = GDK_CONTROL_MASK | GDK_MOD1_MASK | GDK_MOD4_MASK;
		if (kv > 127 || kv <= 32)
			mask |= GDK_SHIFT_MASK;
		if (event->state & mask) {
			char *os = sequence;
			sequence = g_strdup_printf("%s%x;", os, event->state & mask);
			g_free(os);
		}
		const char *meaning = midorator_options("map", sequence, NULL);
		const char *meaningN = midorator_options("nmap", sequence, NULL);
		if (!meaning || !meaning[0]) {
			numprefix = 0;
			g_free(sequence);
			sequence = NULL;
			return !midorator_string_to_bool(midorator_options("option", "pass_unhandled", NULL));
		} else if (strcmp(meaning, "wait") == 0) {
			return true;
		} else if (strcmp(meaning, "pass") == 0) {
			numprefix = 0;
			g_free(sequence);
			sequence = NULL;
			return false;
		} else {
			int pr = numprefix;
			numprefix = 0;
			g_free(sequence);
			sequence = NULL;
			if (pr && meaningN && meaningN[0])
				midorator_process_command(web_view, meaningN, pr);
			else if (pr)
				midorator_process_command(web_view, "%i%s", pr, meaning);
			else
				midorator_process_command(web_view, "%s", meaning);
			return true;
		}
	}
}

static_f void midorator_context_ready_cb (WebKitWebView* web_view, WebKitWebFrame* web_frame, JSContextRef js_context, JSObjectRef js_window, MidoriExtension* extension) {
	midorator_process_command(GTK_WIDGET(web_view), "js_hook_pageload");
	// TODO: normal hooks
}

static_f void midorator_notify_uri_cb (WebKitWebView* web_view) {
	if (midorator_is_current_view(GTK_WIDGET(web_view)))
		midorator_process_command(web_view, NULL, "js_fix_mode");
}

static_f gboolean midorator_navrequest_cb (WebKitWebView *web_view, WebKitWebFrame *frame, WebKitNetworkRequest *request, WebKitWebNavigationAction *navigation_action, WebKitWebPolicyDecision *policy_decision, MidoriExtension* extension) {
//	midorator_entry(GTK_WIDGET(web_view), NULL);

	return false;
}

static_f void midorator_del_tab_cb (MidoriView* view, MidoriBrowser* browser) {
	GtkWidget* web_view = midori_view_get_web_view (view);

	g_signal_handlers_disconnect_by_func (
		web_view, midorator_key_press_event_cb, browser);
	g_signal_handlers_disconnect_by_func (
		web_view, midorator_context_ready_cb, extension);
	g_signal_handlers_disconnect_by_func (
		web_view, midorator_navrequest_cb, extension);
}

static_f void midorator_default_config (GtkWidget* web_view) {
	char *cmds[] = {
#		include "default.h"
		NULL
	};
	int i;
	for (i=0; cmds[i]; i++) {
		if (strcmp(cmds[i], "{{{") == 0) {
			char *s = NULL;
			for (i++; cmds[i] && strcmp(cmds[i], "}}}"); i++) {
				char *sn = g_strconcat(s ? s : "", "\n", cmds[i], NULL);
				if (s)
					g_free(s);
				s = sn;
			}
			if (!cmds[i]) {
				midorator_error(web_view, "}}} expected, EOF found");
				if (s)
					g_free(s);
				return;
			}
			if (s) {
				midorator_process_command(web_view, "%s", s);
				g_free(s);
			}
		} else
			midorator_process_command(web_view, "%s", cmds[i]);
	}
}

static_f void midorator_paste_clipboard_cb(WebKitWebView* web_view) {
	if (!midorator_string_to_bool(midorator_options("option", "paste_primary", NULL)))
		return;
	g_signal_stop_emission_by_name(web_view, "paste-clipboard");
	GtkClipboard *cb = gtk_clipboard_get(GDK_SELECTION_PRIMARY);
	char *text = gtk_clipboard_wait_for_text(cb);
	gtk_clipboard_set_text(cb, text, -1);	// Otherwise WebKit will clear buffer
	const char *js = midorator_options("jscmd", "js_paste", NULL);
	if (js)
		midorator_jscmd(GTK_WIDGET(web_view), js, &text, 1);
	g_free(text);
}

static_f void midorator_add_tab_cb (MidoriBrowser* browser, MidoriView* view, MidoriExtension* extension) {
	GtkWidget* web_view = midori_view_get_web_view (view);

	g_signal_connect (web_view, "key-press-event",
		G_CALLBACK (midorator_key_press_event_cb), browser);
	g_signal_connect (web_view, "window-object-cleared",
		G_CALLBACK (midorator_context_ready_cb), extension);
	g_signal_connect (web_view, "navigation-policy-decision-requested",
		G_CALLBACK (midorator_navrequest_cb), extension);
	g_signal_connect (web_view, "paste-clipboard",
		G_CALLBACK (midorator_paste_clipboard_cb), browser);
	g_signal_connect (web_view, "notify::uri",
		G_CALLBACK (midorator_notify_uri_cb), NULL);
	
	GtkWidget *w = midorator_findwidget(GTK_WIDGET(browser), "MidoriLocationEntry");
	GtkEditable *entry;
	if (GTK_IS_BIN(w))
		entry = GTK_EDITABLE(gtk_bin_get_child(GTK_BIN(w)));
	else
		entry = GTK_EDITABLE(w);
	if (GTK_IS_EDITABLE(entry)) {
		g_signal_connect (entry, "changed",
			G_CALLBACK (midorator_uri_cb), browser);
	}

	GtkWidget *bar = midorator_findwidget(GTK_WIDGET(browser), "statusbar");
	if (GTK_IS_STATUSBAR(bar)) {
		g_signal_connect_after (bar, "text-pushed",
			G_CALLBACK (midorator_uri_push_cb), browser);
	}

	static processed = false;
	if (!processed) {
		processed = true;
		midorator_default_config(web_view);
	}

	MidoriApp *app = g_object_get_data(G_OBJECT(browser), "midori-app");
	g_object_set_data(G_OBJECT(view), "midori-app", app);
	g_object_set_data(G_OBJECT(web_view), "midori-app", app);
	gtk_widget_grab_focus(web_view);
}

static_f void midorator_add_tab_foreach_cb (MidoriView* view, MidoriBrowser* browser, MidoriExtension* extension) {
	midorator_add_tab_cb (browser, view, extension);
}

static_f void midorator_del_browser_cb (MidoriExtension* extension, MidoriBrowser* browser) {
	MidoriApp* app = midori_extension_get_app (extension);
	g_signal_handlers_disconnect_by_func (
		browser, midorator_add_tab_cb, extension);
	g_signal_handlers_disconnect_by_func (
		app, midorator_add_browser_cb, extension);
	g_signal_handlers_disconnect_by_func (
		browser, midorator_del_browser_cb, browser);
	midori_browser_foreach (browser,
		(GtkCallback)midorator_del_tab_cb, extension);
}

static_f void midorator_tab_switched_cb (MidoriBrowser* browser) {
	GtkWidget* w;
	midorator_findwidget_macro(GTK_WIDGET(browser), w, GTK_IS_NOTEBOOK(w));
	if (!w)
		return;
	w = gtk_notebook_get_nth_page(GTK_NOTEBOOK(w), gtk_notebook_get_current_page(GTK_NOTEBOOK(w)));
	if (!w)
		return;
	midorator_findwidget_macro(w, w, WEBKIT_IS_WEB_VIEW(w));
	if (!w)
		return;
	midorator_process_command(w, "js_fix_mode");
}

static_f void midorator_add_browser_cb (MidoriApp* app, MidoriBrowser* browser, MidoriExtension* extension) {
	g_object_set_data(G_OBJECT(browser), "midori-app", app);
	midori_browser_foreach (browser,
		(GtkCallback)midorator_add_tab_foreach_cb, extension);
	g_signal_connect (browser, "add-tab",
		G_CALLBACK (midorator_add_tab_cb), extension);
	g_signal_connect (extension, "deactivate",
		G_CALLBACK (midorator_del_browser_cb), browser);
	g_signal_connect (browser, "notify::tab",
		G_CALLBACK (midorator_tab_switched_cb), extension);
}

static_f void midorator_activate_cb (MidoriExtension* extension, MidoriApp* app) {
	int i;
	KatzeArray* browsers = katze_object_get_object (app, "browsers");

	MidoriBrowser* browser;
	for (i=0; (browser = katze_array_get_nth_item (browsers, i)); i++)
		midorator_add_browser_cb (app, browser, extension);

	g_signal_connect (app, "add-browser",
		G_CALLBACK (midorator_add_browser_cb), extension);

	g_object_unref (browsers);
}

MidoriExtension* extension_init() {
	midorator_init_keycodes();
	MidoriExtension* extension = g_object_new (
		MIDORI_TYPE_EXTENSION,
		"name", _("Midorator"),
		"description", _("Vimperator-like extension for Midori"),
		"version", MIDORATOR_VERSION,
		"authors", "/dev/random <dev-random@dev-random.ru>",
		NULL);
	midori_extension_install_string_list (extension, "filters", NULL, G_MAXSIZE);

	g_signal_connect (extension, "activate",
		G_CALLBACK (midorator_activate_cb), NULL);

	return extension;
}

