#if 0
# /*
	sed -i 's:\(#[[:space:]]*define[[:space:]]\+MIDORATOR_VERSION[[:space:]]\+\).*:\1"0.0'"$(date -r "$0" '+%Y%m%d')"'":g' midorator.h
	make CFLAGS='-g -DDEBUG -O0 -rdynamic' || exit 1
	midori
	exit $?
# */
#endif

#include <gdk/gdkkeysyms.h>
#include <midori/midori.h>
#include <JavaScriptCore/JavaScript.h>
#include <stdio.h>
#include <stdlib.h>
#include "midorator.h"

#ifdef DEBUG
#	include <execinfo.h>
#	define static_f
#else
#	define static_f static
#endif



static_f void midorator_del_browser_cb (MidoriExtension* extension, MidoriBrowser* browser);
static_f void midorator_add_browser_cb (MidoriApp* app, MidoriBrowser* browser, MidoriExtension* extension);
static_f GtkWidget *midorator_entry(GtkWidget* web_view, const char *text);
static_f bool midorator_process_command(GtkWidget *web_view, const char *fmt, ...);
static_f gboolean midorator_key_press_event_cb (GtkWidget* web_view, GdkEventKey* event, MidoriView* view);
static_f GtkStatusbar* midorator_find_sb(GtkWidget *w);
static_f char midorator_mode(GtkWidget* web_view, char mode);
static_f void midorator_message(GtkWidget* web_view, const char *message, const char *bg, const char *fg);
const char* midorator_options(const char *group, const char *name, const char *value);
static_f GtkWidget* midorator_js_get_wv(JSContextRef ctx);
static_f char* midorator_js_getattr(JSContextRef ctx, JSObjectRef obj, const char *name);
static_f char* midorator_html_decode(char *str);

#include "keycodes.h"

#undef g_signal_handlers_disconnect_by_func
#define g_signal_handlers_disconnect_by_func(i, f, d) (g_signal_handlers_disconnect_matched((i), G_SIGNAL_MATCH_FUNC, 0, 0, NULL, (f), NULL))


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

// Sets option (if "value" is non-NULL) and returns its value.
// If value is supplied, returned value is not original one, but a table's internal copy.
const char* midorator_options(const char *group, const char *name, const char *value) {
	static GHashTable *table = NULL;
	if (!table)
		table = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_hash_table_destroy);
	GHashTable *settings = (GHashTable*)g_hash_table_lookup(table, group);
	if (!settings) {
		settings = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
		g_hash_table_insert(table, g_strdup(group), settings);
	}
	if (value)
		g_hash_table_insert(settings, g_strdup(name), g_strdup(value));
	return (const char *)g_hash_table_lookup(settings, name);
}

GtkWidget *midorator_findwidget(GtkWidget *web_view, const char *name) {
	GtkWidget *w;
	if (strcmp(name, "view") == 0)
		return web_view;
	else if (strcmp(name, "tab") == 0) {
		for (w = web_view; w && !GTK_IS_NOTEBOOK(gtk_widget_get_parent(w)); w = gtk_widget_get_parent(w));
		return w;
	} else if (strcmp(name, "tablabel") == 0) {
		for (w = web_view; w && !GTK_IS_NOTEBOOK(gtk_widget_get_parent(w)); w = gtk_widget_get_parent(w));
		return gtk_notebook_get_tab_label(GTK_NOTEBOOK(gtk_widget_get_parent(w)), w);
	} else if (strcmp(name, "tabs") == 0) {
		for (w = web_view; w && !GTK_IS_NOTEBOOK(w); w = gtk_widget_get_parent(w));
		return w;
	} else if (strcmp(name, "scrollbox") == 0) {
		for (w = web_view; w && !GTK_IS_SCROLLED_WINDOW(w); w = gtk_widget_get_parent(w));
		return w;
	} else if (strcmp(name, "hscroll") == 0) {
		for (w = web_view; w && !GTK_IS_SCROLLED_WINDOW(w); w = gtk_widget_get_parent(w));
		if (!w)
			return NULL;
		return GTK_WIDGET(gtk_scrolled_window_get_hscrollbar(GTK_SCROLLED_WINDOW(w)));
	} else if (strcmp(name, "vscroll") == 0) {
		for (w = web_view; w && !GTK_IS_SCROLLED_WINDOW(w); w = gtk_widget_get_parent(w));
		if (!w)
			return NULL;
		return GTK_WIDGET(gtk_scrolled_window_get_vscrollbar(GTK_SCROLLED_WINDOW(w)));
	} else if (strcmp(name, "statusbar") == 0) {
		return GTK_WIDGET(midorator_find_sb(web_view));
	} else
		return NULL;
}

void midorator_setprop(GtkWidget *web_view, const char *widget, const char *name, const char *value) {
	GtkWidget *w = midorator_findwidget(web_view, widget);
	if (!w) {
		midorator_error(web_view, "Widget not found: %s", widget);
		return;
	}
	GParamSpec *sp = g_object_class_find_property(G_OBJECT_GET_CLASS(w), name);
	if (!sp) {
		midorator_error(web_view, "Property for widget '%s' not found: %s", widget, name);
		return;
	}
	int num = atoi(value);
	double d = strtod(value, NULL);
	switch (sp->value_type) {
		case G_TYPE_STRING:
			g_object_set(G_OBJECT(w), name, value, NULL);
			break;
		case G_TYPE_ENUM:
		case G_TYPE_INT:
			g_object_set(G_OBJECT(w), name, num, NULL);
			break;
		case G_TYPE_UINT:
			g_object_set(G_OBJECT(w), name, (unsigned int)num, NULL);
			break;
		case G_TYPE_LONG:
			g_object_set(G_OBJECT(w), name, (long)num, NULL);
			break;
		case G_TYPE_ULONG:
			g_object_set(G_OBJECT(w), name, (unsigned long)num, NULL);
			break;
		case G_TYPE_INT64:
			g_object_set(G_OBJECT(w), name, (int64_t)num, NULL);
			break;
		case G_TYPE_UINT64:
			g_object_set(G_OBJECT(w), name, (uint64_t)num, NULL);
			break;
		case G_TYPE_BOOLEAN:
			g_object_set(G_OBJECT(w), name, (gboolean)num, NULL);
			break;
		case G_TYPE_FLOAT:
			g_object_set(G_OBJECT(w), name, (float)d, NULL);
			break;
		case G_TYPE_DOUBLE:
			g_object_set(G_OBJECT(w), name, d, NULL);
			break;
	}
}

void midorator_setclipboard(GdkAtom atom, const char *str) {
	if (atom == GDK_NONE)
		atom = GDK_SELECTION_PRIMARY;
	GtkClipboard *c = gtk_clipboard_get(atom);
	if (!c)
		return;
	gtk_clipboard_set_text(c, str, -1);
}

char* midorator_getclipboard(GdkAtom atom) {
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
		return NULL;
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

static_f JSObjectRef midorator_js_make_event(JSContextRef ctx, JSObjectRef obj) {
	JSObjectRef ret = JSObjectMake(ctx, NULL, NULL);

	JSStringRef stopPropagation_n = JSStringCreateWithUTF8CString("stopPropagation");
	JSStringRef stopPropagation_s = JSStringCreateWithUTF8CString("this.cancelBubble = true;");
	JSObjectRef stopPropagation = JSObjectMakeFunction(ctx, stopPropagation_n, 0, NULL, stopPropagation_s, NULL, 0, NULL);
	JSObjectSetProperty(ctx, ret, stopPropagation_n, stopPropagation, kJSPropertyAttributeReadOnly, NULL);
	JSStringRelease(stopPropagation_s);
	JSStringRelease(stopPropagation_n);

	JSStringRef preventDefault_n = JSStringCreateWithUTF8CString("preventDefault");
	JSStringRef preventDefault_s = JSStringCreateWithUTF8CString("this.returnValue = true;");
	JSObjectRef preventDefault = JSObjectMakeFunction(ctx, preventDefault_n, 0, NULL, preventDefault_s, NULL, 0, NULL);
	JSObjectSetProperty(ctx, ret, preventDefault_n, preventDefault, kJSPropertyAttributeReadOnly, NULL);
	JSStringRelease(preventDefault_s);
	JSStringRelease(preventDefault_n);

	if (obj) {
		JSStringRef target = JSStringCreateWithUTF8CString("target");
		JSObjectSetProperty(ctx, ret, target, obj, kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(target);
	}

	return ret;
}

static_f bool midorator_js_is_js_enabled(JSContextRef ctx) {
	GtkWidget *web_view = midorator_js_get_wv(ctx);
	if (!web_view)
		return false;
	GtkWidget *view = gtk_widget_get_parent(gtk_widget_get_parent(web_view));
	MidoriWebSettings* settings = katze_object_get_object (web_view, "settings");
	if (!settings) {
		MidoriBrowser* browser = midori_browser_get_for_widget(web_view);
		MidoriWebSettings* settings = katze_object_get_object (browser, "settings");
		if (!settings)
			return false;
	}
	return katze_object_get_boolean(settings, "enable-scripts");
}

static_f JSValueRef midorator_js_callprop(JSContextRef ctx, JSObjectRef obj, const char *name, int argc, const JSValueRef argv[]) {
	JSValueRef prop = midorator_js_getprop(ctx, obj, name);
	if (!prop || !JSValueIsObject(ctx, prop)) {
		//midorator_error(web_view, "No such method: %s", name);
		return NULL;
	}
	JSObjectRef handler = JSValueToObject(ctx, prop, NULL);
	if (!handler)
		return NULL;
	if (!JSObjectIsFunction(ctx, handler)) {
		midorator_error(midorator_js_get_wv(ctx), "Not a function: %s", name);
		return NULL;
	}
	JSValueRef ex = NULL;
	JSValueRef ret = JSObjectCallAsFunction(ctx, handler, obj, argc, argv, &ex);
	if (midorator_js_error(ctx, ex, "Error while calling JS method"))
		return NULL;
	return ret;
}

static_f bool midorator_js_handle(JSContextRef ctx, JSObjectRef obj, const char *name, int argc, const JSValueRef argv[]) {
	if (!midorator_js_is_js_enabled(ctx))
		return true;
	if (argc) {
		JSObjectRef window = JSValueToObject(ctx, midorator_js_getprop(ctx, NULL, "window"), NULL);
		JSStringRef sw = JSStringCreateWithUTF8CString("event");
		JSObjectSetProperty(ctx, window, sw, argv[0], kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(sw);
	}
	for (/*obj=obj*/; obj; obj = JSValueToObject(ctx, midorator_js_getprop(ctx, obj, "parentNode"), NULL)) {
		JSValueRef ret = midorator_js_callprop(ctx, obj, name, argc, argv);
		if (ret) {
			if (argc && midorator_js_hasprop(ctx, JSValueToObject(ctx, argv[0], NULL), "cancelBubble"))
				return false;
			if (JSValueIsBoolean(ctx, ret) && !JSValueToBoolean(ctx, ret))
				return false;
		}
	}
	if (argc && midorator_js_hasprop(ctx, JSValueToObject(ctx, argv[0], NULL), "returnValue"))
		return false;
	return true;
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
	return midorator_js_array_nth(ctx, arr, 1);
}

static_f midorator_js_array_iter midorator_js_array_last(JSContextRef ctx, JSObjectRef arr) {
	midorator_js_array_iter ret = {NULL, ctx, arr, -1, 0};
	JSValueRef len = midorator_js_getprop(ctx, arr, "length");
	if (!len || !JSValueIsNumber(ctx, len))
		return ret;
	ret.index = ret.length = (int)JSValueToNumber(ctx, len, NULL);
	return midorator_js_array_prev(ret);
}

static_f struct _rect { int l, t, w, h; } midorator_js_getpos(JSContextRef ctx, JSObjectRef el, bool onscreen) {
	struct _rect ret = { -1, -1, -1, -1 };
	JSValueRef e = NULL;
	double wd = JSValueToNumber(ctx, midorator_js_getprop(ctx, el, "clientWidth"), &e);
	double hd = JSValueToNumber(ctx, midorator_js_getprop(ctx, el, "clientHeight"), &e);
	if (midorator_js_error(ctx, e, "JS Error: Calculating element position"))
		return ret;
	int l = 0, t = 0, w = (int)wd, h = (int)hd;
	JSObjectRef i;
	for (i = el; i; i = JSValueToObject(ctx, midorator_js_getprop(ctx, i, "offsetParent"), NULL)) {
		if (!onscreen && !JSValueIsObject(ctx, midorator_js_getprop(ctx, i, "offsetParent")))
			break;
		JSValueRef e = NULL;
		double wd = JSValueToNumber(ctx, midorator_js_getprop(ctx, i, "clientWidth"), &e);
		double hd = JSValueToNumber(ctx, midorator_js_getprop(ctx, i, "clientHeight"), &e);
		double ld = JSValueToNumber(ctx, midorator_js_getprop(ctx, i, "scrollLeft"), &e);
		double td = JSValueToNumber(ctx, midorator_js_getprop(ctx, i, "scrollTop"), &e);
		if (midorator_js_error(ctx, e, "JS Error: Calculating element position"))
			continue;

		l -= (int)ld;
		t -= (int)td;

		if (l < 0) {
			w += l;
			l = 0;
		}
		if (t < 0) {
			h += t;
			t = 0;
		}
		if (l + w > (int)wd)
			w = (int)wd - l;
		if (t + h > (int)hd)
			h = (int)hd - t;

		if (w < 0 || h < 0)
			return ret;

		e = NULL;
		ld = JSValueToNumber(ctx, midorator_js_getprop(ctx, i, "offsetLeft"), &e);
		td = JSValueToNumber(ctx, midorator_js_getprop(ctx, i, "offsetTop"), &e);
		if (e)
			continue;
		l += (int)ld;
		t += (int)td;
	}
	struct _rect ret2 = {l, t, w, h};
	return ret2;
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

	struct _rect p = midorator_js_getpos(ctx, el, true);

	JSValueRef e = NULL;
	JSObjectRef doc = JSValueToObject(ctx, midorator_js_getprop(ctx, el, "ownerDocument"), &e);
	if (midorator_js_error(ctx, e, "JS Error: Calculating element position: ownerDocument"))
		return false;
	JSObjectRef w = JSValueToObject(ctx, midorator_js_getprop(ctx, doc, "defaultView"), &e);
	if (midorator_js_error(ctx, e, "JS Error: Calculating element position: defaultView"))
		return false;
	double wd = JSValueToNumber(ctx, midorator_js_getprop(ctx, w, "innerWidth"), &e);
	double hd = JSValueToNumber(ctx, midorator_js_getprop(ctx, w, "innerHeight"), &e);
	//double ld = JSValueToNumber(ctx, midorator_js_getprop(ctx, w, "pageXOffset"), &e);
	//double td = JSValueToNumber(ctx, midorator_js_getprop(ctx, w, "pageYOffset"), &e);
	if (midorator_js_error(ctx, e, "JS Error: Calculating window scroll position"))
		return false;

	if (p.l + p.w < 0 || p.t + p.h < 0 || p.l > (int)wd || p.t > (int)hd || p.w < 0 || p.h < 0)
		return false;

	return true;
}

static_f bool midorator_js_is_selected(JSContextRef ctx, JSObjectRef el, const char *selector) {
	if (selector[0] != '.') {
		int len = strcspn(selector, ".");
		char *ename = midorator_js_value_to_string(ctx, midorator_js_getprop(ctx, el, "tagName"));
		char *name = g_strndup(selector, len);
		if (g_ascii_strcasecmp(name, ename) != 0) {
			g_free(name);
			free(ename);
			return false;
		}
		g_free(name);
		free(ename);

		selector += len;
	}

	while (selector[0]) {
		selector++;
		int len = strcspn(selector, ".");
		char *attr = g_strndup(selector, len);
		selector += len;
		char *eattr = midorator_js_getattr(ctx, el, attr);
		g_free(attr);
		if (!eattr || !eattr[0]) {
			free(eattr);
			return false;
		}
		free(eattr);
	}
	return true;
}

static_f JSValueRef midorator_js_do_find_elements(JSContextRef ctx, JSObjectRef w, char **sel) {
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
	JSObjectRef all = JSValueToObject(ctx, midorator_js_getprop(ctx, doc, "all"), NULL);
	for (i = midorator_js_array_first(ctx, all); i.val; i = midorator_js_array_next(i)) {
		int j;
		for (j=0; sel[j]; j++)
			if (midorator_js_is_selected(ctx, JSValueToObject(ctx, i.val, NULL), sel[j]) &&
					midorator_js_is_visible(ctx, JSValueToObject(ctx, i.val, NULL))) {
				midorator_js_callprop(ctx, JSValueToObject(ctx, ret, NULL), "push", 1, &i.val);
				if (!ret || !JSValueToObject(ctx, ret, NULL))
					*((char*)NULL) = 1;
				break;
			}
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
	char** strs = g_strsplit_set(selectors, " \t\n\r", -1);
	JSValueRef ret = midorator_js_do_find_elements(ctx, JSContextGetGlobalObject(ctx), strs);
	g_strfreev(strs);
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
	JSStringRef s = JSStringCreateWithUTF8CString("midorator_hint");
	JSValueRef vs = JSValueMakeString(ctx, s);
	JSStringRelease(s);
	JSObjectRef hints = JSValueToObject(ctx, midorator_js_callprop(ctx, doc, "getElementsByName", 1, &vs), NULL);

	midorator_js_array_iter iter;
	for (iter = midorator_js_array_last(ctx, hints); iter.val; iter = midorator_js_array_prev(iter)) {
		JSObjectRef parent = midorator_js_v2o(ctx, midorator_js_getprop(ctx, midorator_js_v2o(ctx, iter.val), "parentNode"));
		midorator_js_callprop(ctx, parent, "removeChild", 1, &iter.val);
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
	JSValueRef ret = midorator_js_callprop(ctx, doc, "createElement", 1, &str);
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
	if (g_ascii_strcasecmp(ename, "td") == 0 || g_ascii_strcasecmp(ename, "li") == 0) {
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
		//JSObjectRef body = midorator_js_v2o(ctx, midorator_js_getprop(ctx, doc, "body"));
		//midorator_js_callprop(ctx, body, "appendChild", 1, &ret);
	}
	midorator_js_setprop(ctx, JSValueToObject(ctx, ret, NULL), "near", near);
	return JSValueToObject(ctx, ret, NULL);
}

static_f void midorator_js_setattr(JSContextRef ctx, JSObjectRef obj, const char *name, const char *value) {
	JSStringRef sn = JSStringCreateWithUTF8CString(name);
	JSStringRef sv = JSStringCreateWithUTF8CString(value);
	JSValueRef args[] = { JSValueMakeString(ctx, sn), JSValueMakeString(ctx, sv) };
	midorator_js_callprop(ctx, obj, "setAttribute", 2, args);
	JSStringRelease(sn);
	JSStringRelease(sv);
}

static_f char* midorator_js_getattr(JSContextRef ctx, JSObjectRef obj, const char *name) {
	JSStringRef sn = JSStringCreateWithUTF8CString(name);
	JSValueRef args[] = { JSValueMakeString(ctx, sn) };
	JSValueRef v = midorator_js_callprop(ctx, obj, "getAttribute", 1, args);
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
		midorator_js_callprop(ctx, item, "focus", 0, NULL);
		midorator_mode(midorator_js_get_wv(ctx), 'i');
		if (!force_submit) {
			midorator_js_callprop(ctx, item, "click", 0, NULL);
			return;
		}
		/*char *type = midorator_js_getattr(ctx, item, "type");
		if (g_ascii_strcasecmp(type, "submit") == 0)
			submit = true;
		else if (g_ascii_strcasecmp(type, "reset") == 0)
			midorator_js_callprop(ctx, form, "reset", 0, NULL);
		free(type);*/
	}

	if (form) {
		if (!force_submit && JSValueToObject(ctx, midorator_js_getprop(ctx, form, "onsubmit"), NULL)) {
			JSValueRef ev = midorator_js_make_event(ctx, form);
			if (midorator_js_handle(ctx, form, "onsubmit", 1, &ev))
				return;
		}
		// TODO: do something with "name=value" of button
		midorator_js_callprop(ctx, form, "submit", 0, NULL);
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

static_f void midorator_js_click(JSContextRef ctx, JSObjectRef item) {
	if (JSValueToObject(ctx, midorator_js_getprop(ctx, item, "form"), NULL)) {
		midorator_js_form_click(ctx, item, false);
		return;
	}
	midorator_js_callprop(ctx, item, "focus", 0, NULL);
	JSValueRef ev = midorator_js_make_event(ctx, item);
	bool r = midorator_js_handle(ctx, item, "onclick", 1, &ev);
	if (!r)
		return;
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

static_f JSValueRef midorator_js_genhint(JSContextRef ctx, JSObjectRef el, const char *text) {
	JSObjectRef hint = midorator_js_create_element(ctx, "div", el);
	if (!hint)
		return JSValueMakeNull(ctx);
	midorator_js_setstrprop(ctx, hint, "innerText", text);
	midorator_js_setattr(ctx, hint, "name", "midorator_hint");
	const char *css = midorator_options("option", "hintstyle", NULL);
	struct _rect p = midorator_js_getpos(ctx, el, false);
	char *pos = g_strdup_printf("left: %ipx !important; top: %ipx; !important; ", p.l, p.t);
	char *fullstyle = g_strconcat(
			"width:auto; height:auto; text-decoration:none; font-weight:normal; margin:0; padding:0; border:none; font-style:normal; font-family: Terminus monospace fixed; ",
			css, pos,
			"display:inline-block !important; position:absolute !important;",
			NULL);
	g_free(pos);
	midorator_js_setattr(ctx, hint, "style", fullstyle);
	g_free(fullstyle);
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
		midorator_js_click(ctx, obj);
		return JSValueMakeNull(ctx);
	} else if (JSStringIsEqualToUTF8CString(param, "tabnew")) {
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
					midorator_process_command(web_view, "tabnew %s", href);
					free(href);
				}
			}
			free(tagname);
		}
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
		JSValueRef hint = midorator_js_genhint(ctx, el, text);
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

	JSObjectRef elems = midorator_js_v2o(ctx, midorator_js_find_elements(ctx, cmd));
	if (!elems)
		return;

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
		//midorator_js_click(ctx, midorator_js_v2o(ctx, iter.val));
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
//fprintf(stderr, "%s\n", text);
		midorator_js_genhint(ctx, midorator_js_v2o(ctx, iter.val), text + strlen(follow));
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
			midorator_js_click(ctx, midorator_js_v2o(ctx, iter.val));
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
	for (i=0; res[i]; i++) {
		for (rule = res[i]; strchr(" \t\n\r", rule[0]); rule++);
		JSObjectRef links = midorator_js_v2o(ctx, midorator_js_getprop(ctx, doc, "all"));
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
				midorator_js_click(ctx, midorator_js_v2o(ctx, iter.val));
				return;
			}
			g_free(text);
		}
		g_regex_unref(re);
	}
	g_regex_unref(tagre);
	g_strfreev(res);
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

static_f void midorator_hints(GtkWidget* web_view, const char *charset, const char *follow, const char *cmd) {
	WebKitWebFrame *frame = webkit_web_view_get_main_frame(WEBKIT_WEB_VIEW(web_view));
	if (!frame)
		return;
	JSGlobalContextRef ctx = webkit_web_frame_get_global_context(frame);
	if (!ctx)
		return;
	midorator_make_js_callback(ctx, GTK_WIDGET(web_view));
	midorator_js_hints(ctx, charset, follow, cmd);
}

static_f void midorator_go(GtkWidget* web_view, const char *dir) {
	WebKitWebFrame *frame = webkit_web_view_get_main_frame(WEBKIT_WEB_VIEW(web_view));
	if (!frame)
		return;
	JSGlobalContextRef ctx = webkit_web_frame_get_global_context(frame);
	if (!ctx)
		return;
	midorator_make_js_callback(ctx, GTK_WIDGET(web_view));
	midorator_js_go(ctx, dir);
}

static_f void midorator_submit_form(GtkWidget* web_view) {
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

static_f GtkStatusbar* midorator_find_sb(GtkWidget *w) {
	for (/*w=w*/; w; w = gtk_widget_get_parent(w))
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
		}
	return NULL;
}

static_f void midorator_message(GtkWidget* web_view, const char *message, const char *bg, const char *fg) {
	GtkWidget *w;
	for (w = web_view; w && !GTK_IS_VBOX(w); w = gtk_widget_get_parent(w));
	if (!w)
		return;

	GList *l = gtk_container_get_children(GTK_CONTAINER(w));
	GList *li;
	for (li = l; li; li = li->next)
		if (GTK_IS_SCROLLED_WINDOW(li->data) && strcmp(gtk_widget_get_name(GTK_WIDGET(li->data)), "midorator_message_area") == 0)
			break;

	GtkScrolledWindow *sw;
	GtkLabel *lab;
	if (li) {
		sw = GTK_SCROLLED_WINDOW(li->data);
		li = gtk_container_get_children(GTK_CONTAINER(sw));
		GtkContainer *vp = GTK_CONTAINER(li->data);
		g_list_free(li);
		li = gtk_container_get_children(vp);
		lab = GTK_LABEL(li->data);
		g_list_free(li);
	} else {
		sw = GTK_SCROLLED_WINDOW(gtk_scrolled_window_new(NULL, NULL));
		gtk_widget_set_name(GTK_WIDGET(sw), "midorator_message_area");
		gtk_widget_show(GTK_WIDGET(sw));
		gtk_box_pack_start(GTK_BOX(w), GTK_WIDGET(sw), true, true, 0);

		lab = GTK_LABEL(gtk_label_new(NULL));
		gtk_scrolled_window_add_with_viewport(sw, GTK_WIDGET(lab));
		gtk_widget_show(GTK_WIDGET(lab));
		gtk_label_set_markup(lab, "");
		gtk_label_set_selectable(lab, true);
		gtk_misc_set_alignment(GTK_MISC(lab), 0, 0);
	}
	g_list_free(l);

	if (message) {
		char *text;
		if (bg) {
			if (fg)
				text = g_markup_printf_escaped("<span color=\"%s\" bgcolor=\"%s\">  %s  \n</span>", fg, bg, message);
			else
				text = g_markup_printf_escaped("<span bgcolor=\"%s\">  %s  \n</span>", bg, message);
		} else {
			if (fg)
				text = g_markup_printf_escaped("<span color=\"%s\">  %s  \n</span>", fg, message);
			else
				text = g_markup_printf_escaped("  %s  \n", message);
		}

		const char *oldtext = gtk_label_get_label(lab);
		char *fulltext = g_strconcat(oldtext, text, NULL);
		g_free(text);
		gtk_label_set_markup(lab, fulltext);
		g_free(fulltext);
		gtk_widget_show(GTK_WIDGET(sw));
	} else {
		gtk_widget_hide(GTK_WIDGET(sw));
		gtk_label_set_markup(lab, "");
	}
}

static_f void midorator_search(GtkWidget* web_view, const char *match, bool forward, bool remember) {
	static char *lastmatch = NULL;
	if (match && remember) {
		if (lastmatch)
			free(lastmatch);
		lastmatch = strdup(match);
	}
	if (!match && !lastmatch)
		return;
	if (match && remember) {
		if (lastmatch[0])
			webkit_web_view_mark_text_matches(web_view, match, false, -1);
		else
			webkit_web_view_unmark_text_matches(web_view);
	}
	webkit_web_view_search_text(web_view, match ? match : lastmatch, false, forward, true);
	webkit_web_view_set_highlight_text_matches(web_view, true);
}

static_f void midorator_entry_edited_cb(GtkEntry *e, GtkWidget* web_view) {
	const char *t = gtk_entry_get_text(e);
	if (!t)
		return;
	else if (t[0] == 0)
		midorator_entry(web_view, NULL);
	else if (t[0] == ';')
		midorator_process_command(web_view, "hint %s", t + 1);
	else if (t[0] == '/')
		midorator_search(web_view, t + 1, true, false);
	else if (t[0] == '?')
		midorator_search(web_view, t + 1, false, false);
}

static_f gboolean midorator_entry_key_press_event_cb (GtkEntry* e, GdkEventKey* event, GtkWidget* web_view) {
	if (event->keyval == GDK_Escape) {
		midorator_entry(web_view, NULL);
		return true;
	} else if (event->keyval == GDK_Return) {
		char *t = g_strdup(gtk_entry_get_text(e));
		midorator_entry(web_view, NULL);
		if (t[0] == ':')
			midorator_process_command(web_view, "%s", t + 1);
		else if (t[0] == ';')
			midorator_process_command(web_view, "hint %s", t + 1);
		else if (t[0] == '/')
			midorator_search(web_view, t + 1, true, true);
		else if (t[0] == '?')
			midorator_search(web_view, t + 1, false, true);
		g_free(t);
		return true;
	} else
		return false;
}

static_f bool midorator_entry_restore_focus(GtkEntry* e) {
	int p = gtk_editable_get_position(GTK_EDITABLE(e));
	gtk_widget_grab_focus(GTK_WIDGET(e));
	gtk_editable_set_position(GTK_EDITABLE(e), p);
	return false;
}

// Used to fix focus. Sometimes after switching tabs, input focus remains on old tab
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

static_f GtkWidget *midorator_entry(GtkWidget* web_view, const char *text) {
	// Find box
	GtkStatusbar *sb = midorator_find_sb(web_view);
	if (!sb)
		return;
	GtkWidget *w = gtk_statusbar_get_message_area(sb);

	// Remove existing entry from box
	GList *l = gtk_container_get_children(GTK_CONTAINER(w));
	GList *li;
	for (li = l; li; li = li->next)
		if (GTK_IS_ENTRY(li->data))
			gtk_container_remove(GTK_CONTAINER(w), GTK_WIDGET(li->data));
	g_list_free(l);

	// Add new entry
	if (text && text[0]) {
		GtkWidget *e = gtk_entry_new();
		gtk_entry_set_has_frame(GTK_ENTRY(e), false);
		g_object_set(gtk_settings_get_default(), "gtk-entry-select-on-focus", FALSE, NULL);
		gtk_box_pack_start(GTK_BOX(w), e, true, true, 0);
		gtk_widget_show(e);
		gtk_entry_set_text(GTK_ENTRY(e), text);
		gtk_widget_grab_focus(e);
		gtk_editable_set_position(GTK_EDITABLE(e), -1);
		gtk_box_reorder_child(GTK_BOX(w), e, 0);
		gtk_widget_modify_base(e, GTK_STATE_NORMAL, &w->style->bg[0]);
		gtk_widget_modify_base(e, GTK_STATE_ACTIVE, &w->style->bg[0]);
		gtk_widget_modify_bg(e, GTK_STATE_NORMAL, &w->style->bg[0]);
		gtk_widget_modify_bg(e, GTK_STATE_ACTIVE, &w->style->bg[0]);
		g_signal_connect (e, "changed",
			G_CALLBACK (midorator_entry_edited_cb), web_view);
		g_signal_connect (e, "key-press-event",
			G_CALLBACK (midorator_entry_key_press_event_cb), web_view);
		g_signal_connect (e, "focus-out-event",
			G_CALLBACK (midorator_entry_restore_focus), web_view);
		midorator_entry_edited_cb(GTK_ENTRY(e), web_view);
		return e;
	} else {
		midorator_process_command(web_view, "unhint");
		gtk_widget_grab_focus(web_view);
	}
	return NULL;
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

static_f char* midorator_make_uri(char **args) {
	if (!args[0] || !args[0][0])
		return strdup("about:blank");
	if (strchr(args[0], ':')) {
		char *ret = strdup(args[0]);
		int i;
		for (i=1; args[i]; i++) {
			ret = (char*)realloc(ret, strlen(ret) + strlen(args[i]) + 4);
			strcat(ret, "%20");
			strcat(ret, args[i]);
		}
		return ret;
	}
	if (!args[1]) {
		if (strchr(args[0], '.')) {
			char *ret = malloc(strlen("http://") + strlen(args[0]) + 1);
			strcpy(ret, "http://");
			strcat(ret, args[0]);
			return ret;
		} else {
			const char *ret = midorator_options("bookmark", args[0], NULL);
			if (ret)
				return strdup(ret);
		}
	}

	const char *search = midorator_options("search", args[0], NULL);
	if (search)
		args++;
	if (!search)
		search = midorator_options("search", "default", NULL);
	if (!search)
		search = midorator_options("search", "google", NULL);
	if (!search)
		return strdup("about:blank");

	char *arg = NULL;
	int i;
	for (i=0; args[i]; i++) {
		if (i == 0)
			arg = strdup(args[0]);
		else {
			arg = (char*)realloc(arg, strlen(arg) + strlen(args[i]) + 4);
			strcat(arg, "%20");
			strcat(arg, args[i]);
		}
	}

	int len = snprintf(NULL, 0, search, arg);
	char *ret = malloc(len + 1);
	sprintf(ret, search, arg);
	free(arg);
	return ret;
}

static_f char midorator_mode(GtkWidget* web_view, char mode) {
	static char oldmode = 'n';
	if (mode)
		oldmode = mode;
	const char *modestring = "-- UNKNOWN MODE --";
	if (oldmode == 'n')
		modestring = "";
	else if (oldmode == 'i')
		modestring = "-- INSERT --";
	if (web_view)
		midorator_show_mode(web_view, modestring);
	return oldmode;
}

static_f gboolean midorator_key_press_event_cb (GtkWidget* web_view, GdkEventKey* event, MidoriView* view) {
	midorator_message(web_view, NULL, NULL, NULL);
	midorator_current_view(&web_view);
	gtk_widget_grab_focus(web_view);

	bool insert = midorator_mode(web_view, 0) == 'i';
	if (insert) {
		if (event->keyval == GDK_Escape) {
			midorator_mode(web_view, 'n');
			return true;
		} else
			return false;
	}

	static int numprefix = 0;
	static char *sequence = NULL;
	if (event->keyval >= GDK_0 && event->keyval <= GDK_9) {
		numprefix = numprefix * 10 + event->keyval - GDK_0;
		return true;
	} else if (event->keyval == GDK_Shift_L || event->keyval == GDK_Shift_R ||
			event->keyval == GDK_Alt_L || event->keyval == GDK_Alt_R ||
			event->keyval == GDK_Control_L || event->keyval == GDK_Control_R) {
		// Do nothing
	} else {
		if (!sequence)
			sequence = g_strdup_printf("%03x;", event->keyval);
		else {
			char *os = sequence;
			sequence = g_strdup_printf("%s%03x;", os, event->keyval);
			g_free(os);
		}
		const char *meaning = midorator_options("map", sequence, NULL);
		if (!meaning || !meaning[0]) {
			numprefix = 0;
			g_free(sequence);
			sequence = NULL;
			return true;
		} else if (strcmp(meaning, "wait") == 0) {
			return true;
		} else if (strcmp(meaning, "pass") == 0) {
			return false;
		} else {
			int pr = numprefix;
			numprefix = 0;
			g_free(sequence);
			sequence = NULL;
			midorator_process_command(web_view, pr ? "%2$i%1$s" : "%s", meaning, pr);
			return true;
		}
	}
}

static_f bool midorator_process_command(GtkWidget *web_view, const char *fmt, ...) {
	va_list l;
	va_start(l, fmt);
	int len = vsnprintf(NULL, 0, fmt, l);
	va_end(l);
	if (len <= 0)
		return false;
	char *buf = (char*)malloc(len + 1);
	if (!buf)
		return false;
	va_start(l, fmt);
	vsnprintf(buf, len + 1, fmt, l);
	va_end(l);
	char **cmd = (char**)malloc(sizeof(char*)*256);
	int i, n = 0;
	cmd[0] = buf;
	cmd[1] = NULL;
	bool inquote = false;
	for (i=0; buf[i]; i++) {
		if (buf[i] == '"' && strcmp(cmd[0], "js") && strcmp(cmd[0], "cmdmap")) {
			inquote = !inquote;
			memmove(&buf[i], &buf[i+1], strlen(&buf[i]));
		} else if (buf[i] == '\\' && buf[i+1]) {
			memmove(&buf[i], &buf[i+1], strlen(&buf[i]));
		} else if (&buf[i] == cmd[n] && buf[i] == ' ')
			cmd[n]++;
		else if (buf[i] == ' ' && !inquote) {
			buf[i] = 0;
			n++;
			if (n > 254)
				break;
			cmd[n] = &buf[i+1];
			cmd[n+1] = NULL;
			if (n == 1 && strcmp(cmd[0], "js") == 0)
				break;
			if (n == 2 && (strcmp(cmd[0], "cmdmap") == 0 || strcmp(cmd[0], "set") == 0))
				break;
		}
	}
	while (cmd[0][0] == ':')
		cmd[0]++;

	if (cmd[0][0] == '#' || cmd[0][0] == '"') {
		// Do nothing, it's a comment

	} else if (strcmp(cmd[0], "widget") == 0 && cmd[1] && cmd[2] && cmd[3]) {
		midorator_setprop(web_view, cmd[1], cmd[2], cmd[3]);

	} else if (strcmp(cmd[0], "insert") == 0) {
		midorator_mode(web_view, 'i');

	} else if (strcmp(cmd[0], "tabnew") == 0) {
		char *uri = midorator_make_uri(cmd + 1);
		g_signal_emit_by_name(gtk_widget_get_parent(gtk_widget_get_parent(web_view)), "new-tab", uri, false, NULL);
		free(uri);

	} else if (strcmp(cmd[0], "open") == 0) {
		char *uri = midorator_make_uri(cmd + 1);
		if (strncmp(uri, "javascript:", strlen("javascript:")) == 0) {
			char *js = g_uri_unescape_string(uri + strlen("javascript:"), NULL);
			midorator_process_command(web_view, "js %s", js);
			g_free(js);
		} else
			webkit_web_view_open(WEBKIT_WEB_VIEW(web_view), uri);
		//midorator_process_command(web_view, "js document.location = '%s';", uri);
		free(uri);

	} else if (strcmp(cmd[0], "paste") == 0) {
		char *uri = midorator_getclipboard(GDK_SELECTION_PRIMARY);
		midorator_process_command(web_view, "open %s", uri);
		free(uri);

	} else if (strcmp(cmd[0], "tabpaste") == 0) {
		char *uri = midorator_getclipboard(GDK_SELECTION_PRIMARY);
		midorator_process_command(web_view, "tabnew %s", uri);
		free(uri);

	} else if (strcmp(cmd[0], "yank") == 0) {
		midorator_setclipboard(GDK_SELECTION_PRIMARY, webkit_web_view_get_uri(WEBKIT_WEB_VIEW(web_view)));

	} else if (strcmp(cmd[0], "search") == 0 && cmd[1] && cmd[2]) {
		midorator_options("search", cmd[1], cmd[2]);

	} else if (strcmp(cmd[0], "bookmark") == 0 && cmd[1] && cmd[2]) {
		midorator_options("bookmark", cmd[1], cmd[2]);

	} else if (strcmp(cmd[0], "set") == 0 && cmd[1] && cmd[2]) {
		midorator_options("option", cmd[1], cmd[2]);

	} else if (strcmp(cmd[0], "cmdmap") == 0 && cmd[1] && cmd[2]) {
		char *buf = g_strdup("");
		int i;
		for (i=0; cmd[1][i]; i++) {
			if (cmd[1][i] == '<') {
				char *end = strchr(cmd[1] + i, '>');
				if (!end) {
					midorator_error(web_view, "Unfinished '<...>' in command 'cmdmap'");
					g_free(buf);
					free(cmd);
					free(buf);
					return false;
				}
				*end = 0;
				const char *val = midorator_options("keycode", cmd[1] + i + 1, NULL);
				if (!val) {
					midorator_error(web_view, "Unknown key: <%s>", cmd[1] + i + 1);
					g_free(buf);
					free(cmd);
					free(buf);
					return false;
				}
				char *nbuf = g_strdup_printf("%s%03x;", buf, (int)strtol(val, NULL, 16));
				g_free(buf);
				buf = nbuf;
				i = end - cmd[1];
			} else {
				char *nbuf = g_strdup_printf("%s%03x;", buf, cmd[1][i]);
				g_free(buf);
				buf = nbuf;
			}
			midorator_options("map", buf, "wait");
		}
		midorator_options("map", buf, cmd[2]);
		g_free(buf);

	} else if (strcmp(cmd[0], "next") == 0) {
		midorator_search(web_view, NULL, true, false);

	} else if (strcmp(cmd[0], "next!") == 0) {
		midorator_search(web_view, NULL, false, false);

	} else if (strcmp(cmd[0], "entry") == 0 && cmd[1]) {
		midorator_entry(web_view, cmd[1]);

	} else if (strcmp(cmd[0], "source") == 0 && cmd[1]) {
		FILE *f = fopen(cmd[1], "r");
		if (!f && cmd[1][0] == '~') {
			const char *home = getenv("HOME");
			if (home) {
				char buf[strlen(home) + strlen(cmd[1])];
				strcpy(buf, home);
				strcat(buf, cmd[1] + 1);
				f = fopen(buf, "r");
			}
		}
		if (f) {
			char buf[512];
			while (fscanf(f, " %511[^\n\r]", buf) == 1)
				midorator_process_command(web_view, "%s", buf);
			fclose(f);
		}

	} else if (strcmp(cmd[0], "submit") == 0) {
		midorator_submit_form(web_view);

	} else if (strcmp(cmd[0], "scroll") == 0) {
		if (!cmd[1][0] || !cmd[2][0] || !cmd[3][0]) {
			free(cmd);
			free(buf);
			return false;
		}

		GtkWidget *s = gtk_widget_get_parent(web_view);
		GtkAdjustment *a;
		if (cmd[1][0] == 'h')
			a = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(s));
		else
			a = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(s));

		double pos = atoi(cmd[3]);
		if (cmd[4] && cmd[4][0] == 'p')
			pos *= gtk_adjustment_get_page_increment(a);
		else
			pos *= gtk_adjustment_get_step_increment(a);

		if (cmd[2][0] == '+')
			pos += gtk_adjustment_get_value(a);
		else if (cmd[2][0] == '-')
			pos = gtk_adjustment_get_value(a) - pos;

		if (pos < gtk_adjustment_get_lower(a))
			pos = gtk_adjustment_get_lower(a);
		if (pos > gtk_adjustment_get_upper(a))
			pos = gtk_adjustment_get_upper(a);
		
		gtk_adjustment_set_value(a, pos);

	} else if (strcmp(cmd[0], "wq") == 0) {
		GtkWidget *w;
		for (w = web_view; w && !GTK_IS_WINDOW(w); w = gtk_widget_get_parent(w));
		if (w)
			gtk_widget_destroy(w);

	} else if (strcmp(cmd[0], "js") == 0) {
		for (i=1; cmd[i]; i++) {
			webkit_web_view_execute_script(WEBKIT_WEB_VIEW(web_view), cmd[i]);
		}

	} else if (strcmp(cmd[0], "hint") == 0) {
		if (cmd[1] && cmd[1][0]) {
			const char *hintchars = midorator_options("option", "hintchars", NULL);
			if (!hintchars)
				hintchars = "0123456789";
			midorator_hints(web_view, hintchars, cmd[1] + 1, (cmd[1][0] == 'F') ? "tabnew" : (cmd[1][0] == 'y') ? "yank" : "click");
		}

	} else if (strcmp(cmd[0], "unhint") == 0) {
		midorator_hints(web_view, NULL, NULL, NULL);

	} else if (strcmp(cmd[0], "reload") == 0) {
		webkit_web_view_reload(WEBKIT_WEB_VIEW(web_view));

	} else if (strcmp(cmd[0], "reload!") == 0) {
		webkit_web_view_reload_bypass_cache(WEBKIT_WEB_VIEW(web_view));

	} else if (strcmp(cmd[0], "go") == 0 && cmd[1] && cmd[1][0]) {
		if (strcmp(cmd[1], "back") == 0) {
			webkit_web_view_go_back(WEBKIT_WEB_VIEW(web_view));
		} else if (strcmp(cmd[1], "forth") == 0) {
			webkit_web_view_go_forward(WEBKIT_WEB_VIEW(web_view));
		} else {
			midorator_go(web_view, cmd[1]);
		}

	} else if (strcmp(cmd[0], "undo") == 0) {
		MidoriBrowser *browser = midori_browser_get_for_widget(web_view);
		GtkActionGroup *actions = midori_browser_get_action_group(browser);
		GtkAction *action = gtk_action_group_get_action(actions, "UndoTabClose");
		gtk_action_activate(action);

	} else if (strcmp(cmd[0], "q") == 0) {
		MidoriBrowser *browser = midori_browser_get_for_widget(web_view);
		GtkActionGroup *actions = midori_browser_get_action_group(browser);
		GtkAction *action = gtk_action_group_get_action(actions, "TabClose");
		gtk_action_activate(action);

	} else if (strcmp(cmd[0], "action") == 0 && cmd[1]) {
		MidoriBrowser *browser = midori_browser_get_for_widget(web_view);
		GtkActionGroup *actions = midori_browser_get_action_group(browser);
		GtkAction *action = gtk_action_group_get_action(actions, cmd[1]);
		if (action)
			gtk_action_activate(action);
		else
			midorator_error(web_view, "No such action: '%s'", cmd[1]);

	} else if (strcmp(cmd[0], "actions") == 0) {
		MidoriBrowser *browser = midori_browser_get_for_widget(web_view);
		GtkActionGroup *actions = midori_browser_get_action_group(browser);
		midorator_message(web_view, "Known actions are:", NULL, NULL);
		int sid = g_signal_lookup("activate", GTK_TYPE_ACTION);
		GList *l = gtk_action_group_list_actions(actions);
		GList *li;
		for (li = l; li; li = li->next)
			if (gtk_action_is_sensitive(GTK_ACTION(li->data))) {
				char *msg = g_strdup_printf("%s - %s", gtk_action_get_name(GTK_ACTION(li->data)), gtk_action_get_label(GTK_ACTION(li->data)));
				midorator_message(web_view, msg, NULL, NULL);
				g_free(msg);
			}
		g_list_free(l);

	} else {
		midorator_error(web_view, "Invalid command or parameters: %s", cmd[0]);
		free(cmd);
		free(buf);
		return false;
	}
	free(cmd);
	free(buf);
	return true;
}

static_f void midorator_context_ready_cb (WebKitWebView* web_view, WebKitWebFrame* web_frame, JSContextRef js_context, JSObjectRef js_window, MidoriExtension* extension) {
	midorator_make_js_callback(js_context, GTK_WIDGET(web_view));
}

static_f gboolean midorator_navrequest_cb (WebKitWebView *web_view, WebKitWebFrame *frame, WebKitNetworkRequest *request, WebKitWebNavigationAction *navigation_action, WebKitWebPolicyDecision *policy_decision, MidoriExtension* extension) {
//	midorator_entry(GTK_WIDGET(web_view), NULL);

	GdkEventKey e = {};
	e.keyval = GDK_Escape;
	midorator_key_press_event_cb(GTK_WIDGET(web_view), &e, NULL);

	return false;
}

static_f void midorator_del_tab_cb (MidoriView* view, MidoriBrowser* browser) {
	GtkWidget* web_view = midori_view_get_web_view (view);

	g_signal_handlers_disconnect_by_func (
		web_view, midorator_key_press_event_cb, browser);
	g_signal_handlers_disconnect_by_func (
		web_view, midorator_context_ready_cb, browser);
}

static_f void midorator_default_config (GtkWidget* web_view) {
#	include "default.h"
}

static_f void midorator_add_tab_cb (MidoriBrowser* browser, MidoriView* view, MidoriExtension* extension) {
	GtkWidget* web_view = midori_view_get_web_view (view);

	g_signal_connect (web_view, "key-press-event",
		G_CALLBACK (midorator_key_press_event_cb), browser);
	g_signal_connect (web_view, "window-object-cleared",
		G_CALLBACK (midorator_context_ready_cb), extension);
	g_signal_connect (web_view, "navigation-policy-decision-requested",
		G_CALLBACK (midorator_navrequest_cb), extension);
	
	static processed = false;
	if (!processed) {
		processed = true;
		midorator_default_config(web_view);
	}

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

static_f void midorator_add_browser_cb (MidoriApp* app, MidoriBrowser* browser, MidoriExtension* extension) {
	midori_browser_foreach (browser,
		(GtkCallback)midorator_add_tab_foreach_cb, extension);
	g_signal_connect (browser, "add-tab",
		G_CALLBACK (midorator_add_tab_cb), extension);
	g_signal_connect (extension, "deactivate",
		G_CALLBACK (midorator_del_browser_cb), browser);
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

