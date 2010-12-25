#if 0
# /*
	sed -i 's:\(#[[:space:]]*define[[:space:]]\+MIDORATOR_VERSION[[:space:]]\+\).*:\1"0.1.0'"$(date -r "$0" '+%Y%m%d')"'":g' midorator.h
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
#include "midorator-webkit.h"
#include "midorator-hooks.h"



static_f void midorator_current_view(GtkWidget **web_view);
static_f void midorator_del_browser_cb (MidoriExtension* extension, MidoriBrowser* browser);
static_f void midorator_add_browser_cb (MidoriApp* app, MidoriBrowser* browser, MidoriExtension* extension);
static_f gboolean midorator_key_press_event_cb (GtkWidget* web_view, GdkEventKey* event, MidoriView* view);
static_f GtkStatusbar* midorator_find_sb(GtkWidget *w);
const char* midorator_options(const char *group, const char *name, const char *value);
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
	if (strchr(name, '.')) {
		char **names = g_strsplit(name, ".", 0);
		GObject *w = G_OBJECT(midorator_findwidget(web_view, names[0]));
		int i;
		for (i = 1; names[i] && w; i++) {
			if (names[i][0] == '#' && GTK_IS_CONTAINER(w)) {
				guint num = atoi(names[i] + 1);
				GList *children = gtk_container_get_children(GTK_CONTAINER(w));
				GList *child = g_list_nth(children, num);
				if (child)
					w = G_OBJECT(child->data);
				else
					w = NULL;
				g_list_free(children);
			} else if (names[i][0] == ':' && GTK_IS_CONTAINER(w)) {
				GType t = g_type_from_name(names[i] + 1);
				if (t) {
					GList *children = gtk_container_get_children(GTK_CONTAINER(w));
					GList *child;
					for (child = children; child; child = child->next)
						if (G_TYPE_CHECK_INSTANCE_TYPE(child->data, t))
							break;
					if (child)
						w = G_OBJECT(child->data);
					else
						w = NULL;
					g_list_free(children);
				} else
					w = NULL;
			} else {
				GValue val = {};
				g_value_init(&val, G_TYPE_OBJECT);
				g_object_get_property(w, names[i], &val);
				if (G_VALUE_HOLDS(&val, G_TYPE_OBJECT))
					w = g_value_get_object(&val);
				else
					w = NULL;
				g_value_unset(&val);
			}
		}
		g_strfreev(names);
		return (GtkWidget*)w;
	} else {
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
		} else if (strcmp(name, "browser") == 0) {
			midorator_findwidget_up_macro(web_view, w, MIDORI_IS_BROWSER(w));
			return w;
		} else {
			midorator_findwidget_up_macro(web_view, w, GTK_IS_NOTEBOOK(gtk_widget_get_parent(w)));
			midorator_findwidget_macro(w, w, strcmp(gtk_widget_get_name(w), name) == 0);
			if (!w) {
				midorator_findwidget_macro(gtk_widget_get_toplevel(web_view), w, strcmp(gtk_widget_get_name(w), name) == 0);
			}
			return w;
		}
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
		return NULL;
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
	//const char *oldtext = gtk_entry_get_text(e);
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

char midorator_mode(GtkWidget* web_view, unsigned char mode) {
	static const char* modenames[256] = {};
	if (!modenames['i']) {
		modenames['i'] = "-- INSERT --";
		modenames['k'] = "-- KEY WAIT --";
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

	char mode = midorator_mode(web_view, 0);
	if (mode == 'i') {
		if (event->keyval == GDK_Escape) {
			midorator_mode(web_view, 'n');
			if (midorator_string_to_bool(midorator_options("option", "blur_on_escape", NULL)))
				midorator_process_command(web_view, NULL, "blur");
			return true;
		} else
			return false;
	} else if (mode == 'k') {
		GdkKeymap *km = gdk_keymap_get_default();
		GdkKeymapKey kk = {event->hardware_keycode};
		gdk_keymap_translate_keyboard_state(km, event->hardware_keycode, event->state, event->group, NULL, NULL, &kk.level, NULL);
		guint kv = gdk_keymap_lookup_key(km, &kk);
		if (!kv)
			kv = event->keyval;
		if (kv < 128) {
			midorator_mode(web_view, 'n');
			char c[2] = {kv, 0};
			midorator_process_command(web_view, NULL, "js_keywait_cb", c);
		}
		// TODO else
		return true;
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
		GdkKeymapKey kk = {event->hardware_keycode};
		gdk_keymap_translate_keyboard_state(km, event->hardware_keycode, event->state, event->group, NULL, NULL, &kk.level, NULL);
		guint kv = gdk_keymap_lookup_key(km, &kk);
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
//	if (midorator_is_current_view(GTK_WIDGET(web_view)))
//		midorator_process_command(web_view, NULL, "js_fix_mode");
}

static_f void midorator_loaded_cb (WebKitWebView *web_view, WebKitWebFrame *web_frame, gpointer ud) {
	if (midorator_is_current_view(GTK_WIDGET(web_view)) &&
			webkit_web_view_get_focused_frame(web_view) == web_frame &&
			midorator_string_to_bool(midorator_options("option", "auto_switch_mode", NULL)))
		midorator_process_command(GTK_WIDGET(web_view), NULL, "js_fix_mode");
}

static_f void midorator_del_tab_cb (MidoriView* view, MidoriBrowser* browser) {
	GtkWidget* web_view = midori_view_get_web_view (view);
	midorator_webkit_remove_view(WEBKIT_WEB_VIEW(web_view));

	g_signal_handlers_disconnect_by_func (
		web_view, midorator_key_press_event_cb, browser);
	g_signal_handlers_disconnect_by_func (
		web_view, midorator_context_ready_cb, extension);
	g_signal_handlers_disconnect_by_func (
		web_view, midorator_loaded_cb, NULL);
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
		midorator_process_command(GTK_WIDGET(web_view), "jscmd", js, text);
	g_free(text);
}

static_f void midorator_add_tab_cb (MidoriBrowser* browser, MidoriView* view, MidoriExtension* extension) {
	GtkWidget* web_view = midori_view_get_web_view (view);
	midorator_webkit_add_view(WEBKIT_WEB_VIEW(web_view));
	midorator_hooks_add_view(WEBKIT_WEB_VIEW(web_view));

	g_signal_connect (web_view, "key-press-event",
		G_CALLBACK (midorator_key_press_event_cb), browser);
	g_signal_connect (web_view, "window-object-cleared",
		G_CALLBACK (midorator_context_ready_cb), extension);
	g_signal_connect (web_view, "document-load-finished",
		G_CALLBACK (midorator_loaded_cb), NULL);
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

	static bool processed = false;
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

