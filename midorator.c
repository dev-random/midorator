#if 0
# /*
	sed 's/"/\\"/g;s/%[^s]/%&/g;s/.*/"&\\n"/' uzbl-follow.js > uzbl-follow.h
	sed 's/"/\\"/g;s/%[^s]/%&/g;s/.*/"&\\n"/' go-next.js > go-next.h
	sed -i 's:\(#[[:space:]]*define[[:space:]]\+MIDORATOR_VERSION[[:space:]]\+\).*:\1"0.0'"$(date -r "$0" '+%Y%m%d')"'":g' midorator.h
	gcc "$0" -Iincludes -o "$(basename "$0" .c).so" -shared $(pkg-config gtk+-2.0 webkit-1.0 --cflags) || exit $?
	midori
	exit 0
# */
#endif

#include <gdk/gdkkeysyms.h>
#include <midori/midori.h>
#include <JavaScriptCore/JavaScript.h>
#include <stdio.h>
#include <stdlib.h>
#include "midorator.h"



static void midorator_del_browser_cb (MidoriExtension* extension, MidoriBrowser* browser);
static void midorator_add_browser_cb (MidoriApp* app, MidoriBrowser* browser, MidoriExtension* extension);
static GtkWidget *midorator_entry(GtkWidget* web_view, const char *text);
static bool midorator_process_command(GtkWidget *web_view, const char *fmt, ...);
static gboolean midorator_key_press_event_cb (GtkWidget* web_view, GdkEventKey* event, MidoriView* view);
static GtkStatusbar* midorator_find_sb(GtkWidget *w);

#undef g_signal_handlers_disconnect_by_func
#define g_signal_handlers_disconnect_by_func(i, f, d) (g_signal_handlers_disconnect_matched((i), G_SIGNAL_MATCH_FUNC, 0, 0, NULL, (f), NULL))


void midorator_error(GtkWidget *web_view, char *fmt, ...) {
	va_list l;
	va_start(l, fmt);
	vfprintf(stderr, fmt, l);
	fprintf(stderr, "\n");
	va_end(l);
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

static JSValueRef midorator_js_callback(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception) {
	if (argumentCount < 1 || !JSValueIsString(ctx, arguments[0]))
		return JSValueMakeNull(ctx);
	
	// Get preperty 'midorator_internal' (we stored pointer to 'web_view' there)
	JSStringRef sw = JSStringCreateWithUTF8CString("midorator_internal");
	JSObjectRef global = JSContextGetGlobalObject(ctx);
	JSValueRef v = JSObjectGetProperty(ctx, global, sw, NULL);
	JSStringRelease(sw);
	
	if (!JSValueIsString(ctx, v))
		return JSValueMakeNull(ctx);
	JSStringRef param = JSValueToStringCopy(ctx, arguments[0], NULL);

	// Extract actual 'web_view' pointer from property
	JSStringRef view_s = JSValueToStringCopy(ctx, v, NULL);
	char buf[32];
	JSStringGetUTF8CString(view_s, buf, sizeof(buf));
	GtkWidget *web_view;
	sscanf(buf, "%p", &web_view);
	JSStringRelease(view_s);

	// Process commands
	if (JSStringIsEqualToUTF8CString(param, "hide entry"))
		midorator_entry(web_view, NULL);
	else if (JSStringIsEqualToUTF8CString(param, "insert mode")) {
		GdkEventKey e = {};
		e.keyval = GDK_i;
		midorator_key_press_event_cb(web_view, &e, NULL);
	}
	// TODO other commands

	JSStringRelease(param);
	return JSValueMakeNull(ctx);
}

static void midorator_make_js_callback(JSContextRef ctx, GtkWidget *web_view) {
	JSObjectRef global = JSContextGetGlobalObject(ctx);

	JSStringRef s = JSStringCreateWithUTF8CString("midorator_command");
	JSStringRef sw = JSStringCreateWithUTF8CString("midorator_internal");

	if (!JSObjectHasProperty(ctx, global, s)) {
		JSObjectRef func = JSObjectMakeFunctionWithCallback(ctx, s, midorator_js_callback);
		JSObjectSetProperty(ctx, global, s, func, kJSPropertyAttributeReadOnly, NULL);
		char buf[32] = {};
		snprintf(buf, 31, "%p", web_view);
		JSStringRef sv = JSStringCreateWithUTF8CString(buf);
		JSObjectSetProperty(ctx, global, sw, JSValueMakeString(ctx, sv), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(sv);
	}

	JSStringRelease(s);
	JSStringRelease(sw);
}

static GtkStatusbar* midorator_find_sb(GtkWidget *w) {
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

static void midorator_search(GtkWidget* web_view, const char *match, bool forward, bool remember) {
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

static void midorator_entry_edited_cb(GtkEntry *e, GtkWidget* web_view) {
	const char *t = gtk_entry_get_text(e);
	if (!t)
		return;
	else if (t[0] == 0)
		midorator_entry(web_view, NULL);
	else if (t[0] == ';')
		midorator_process_command(web_view, "hint %s", t + 1);
	else if (t[0] == '/')
		midorator_search(web_view, t + 1, true, false);
}

static gboolean midorator_entry_key_press_event_cb (GtkEntry* e, GdkEventKey* event, GtkWidget* web_view) {
	if (event->keyval == GDK_Escape) {
		midorator_entry(web_view, NULL);
		return true;
	} else if (event->keyval == GDK_Return) {
		const char *t = gtk_entry_get_text(e);
		if (t[0] == ':')
			midorator_process_command(web_view, "%s", t + 1);
		else if (t[0] == ';')
			midorator_process_command(web_view, "hint %s", t + 1);
		else if (t[0] == '/')
			midorator_search(web_view, t + 1, true, true);
		midorator_entry(web_view, NULL);
		return true;
	} else
		return false;
}

static bool midorator_entry_restore_focus(GtkEntry* e) {
	int p = gtk_editable_get_position(GTK_EDITABLE(e));
	gtk_widget_grab_focus(GTK_WIDGET(e));
	gtk_editable_set_position(GTK_EDITABLE(e), p);
	return true;
}

// Used to fix focus. Sometimes after switching tabs, input focus remains on old tab
// (now background). This function replaces background web_view with foreground.
static void midorator_current_view(GtkWidget **web_view) {
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

static GtkWidget *midorator_entry(GtkWidget* web_view, const char *text) {
	// Find vbox
	GtkWidget *w;
	for (w = web_view; w && !GTK_IS_NOTEBOOK(w); w = gtk_widget_get_parent(w));
	GtkNotebook *n = GTK_NOTEBOOK(w);
	for (/*w = w*/; w && !GTK_IS_VBOX(w); w = gtk_widget_get_parent(w));
	if (!w)
		return NULL;

	// Remove existing entry from vbox
	GList *l = gtk_container_get_children(GTK_CONTAINER(w));
	GList *li;
	for (li = l; li; li = li->next)
		if (GTK_IS_ENTRY(li->data))
			gtk_container_remove(GTK_CONTAINER(w), GTK_WIDGET(li->data));
	g_list_free(l);

	// Add new entry
	if (text) {
		GtkWidget *e = gtk_entry_new();
		g_object_set(gtk_settings_get_default(), "gtk-entry-select-on-focus", FALSE, NULL);
		gtk_box_pack_end(GTK_BOX(w), e, 0, 0, 0);
		gtk_widget_show(e);
		gtk_entry_set_text(GTK_ENTRY(e), text);
		gtk_widget_grab_focus(e);
		gtk_editable_set_position(GTK_EDITABLE(e), -1);
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

static void midorator_show_mode(GtkWidget* web_view, const char *str) {
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

static char* midorator_make_uri(char **args) {
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

static gboolean midorator_key_press_event_cb (GtkWidget* web_view, GdkEventKey* event, MidoriView* view) {
	midorator_current_view(&web_view);
	gtk_widget_grab_focus(web_view);
	static bool insert = false;
	if (insert) {
		if (event->keyval == GDK_Escape) {
			insert = false;
			midorator_show_mode(web_view, "");
			return true;
		} else
			return false;
	}
	switch (event->keyval) {
		case GDK_Escape:
			webkit_web_view_execute_script(WEBKIT_WEB_VIEW(web_view), "document.activeElement.blur();");
			return true;
		case GDK_Tab:
			return false;
		case GDK_semicolon:
			midorator_entry(web_view, ";");
			return true;
		case GDK_colon:
			midorator_entry(web_view, ":");
			return true;
		case GDK_slash:
			midorator_entry(web_view, "/");
			return true;
		case GDK_question:
			midorator_entry(web_view, "?");
			return true;
		case GDK_bracketleft:
			return midorator_process_command(web_view, "go prev");
		case GDK_bracketright:
			return midorator_process_command(web_view, "go next");
		case GDK_r:
			return midorator_process_command(web_view, "reload");
		case GDK_R:
			return midorator_process_command(web_view, "reload!");
		case GDK_space:
		case GDK_Page_Down:
			return midorator_process_command(web_view, "scroll v + 1 p");
		case GDK_Page_Up:
			return midorator_process_command(web_view, "scroll v - 1 p");
		case GDK_Up:
		case GDK_k:
			return midorator_process_command(web_view, "scroll v - 1");
		case GDK_Down:
		case GDK_j:
			return midorator_process_command(web_view, "scroll v + 1");
		case GDK_Home:
			return midorator_process_command(web_view, "scroll v = 0");
		case GDK_End:
		case GDK_G:
			return midorator_process_command(web_view, "scroll v = 32768 p");
		case GDK_BackSpace:
		case GDK_H:
			return midorator_process_command(web_view, "go back");
		case GDK_L:
			return midorator_process_command(web_view, "go forth");
		case GDK_p:
			return midorator_process_command(web_view, "paste");
		case GDK_P:
			return midorator_process_command(web_view, "tabpaste");
		case GDK_n:
			midorator_search(web_view, NULL, true, false);
			return true;
		case GDK_N:
			midorator_search(web_view, NULL, false, false);
			return true;
		case GDK_t:
			midorator_entry(web_view, ":tabnew ");
			return true;
		case GDK_o:
			midorator_entry(web_view, ":open ");
			return true;
		case GDK_f:
			midorator_entry(web_view, ";f");
			return true;
		case GDK_F:
			midorator_entry(web_view, ";F");
			return true;
		case GDK_i:
			insert = true;
			midorator_show_mode(web_view, _("-- INSERT --"));
			return true;
		default:
			//printf("%i\n", event->keyval);
			return true;
	}
	return false;
}

static bool midorator_process_command(GtkWidget *web_view, const char *fmt, ...) {
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
	for (i=0; buf[i]; i++) {
		if (&buf[i] == cmd[n] && buf[i] == ' ')
			cmd[n]++;
		else if (buf[i] == ' ') {
			buf[i] = 0;
			n++;
			if (n > 254)
				break;
			cmd[n] = &buf[i+1];
			cmd[n+1] = NULL;
			if (n == 1 && strcmp(cmd[0], "js") == 0)
				break;
		}
	}
	while (cmd[0][0] == ':')
		cmd[0]++;

	if (cmd[0][0] == '#' || cmd[0][0] == '"') {
		// Do nothing, it's a comment

	} else if (strcmp(cmd[0], "widget") == 0 && cmd[1] && cmd[2] && cmd[3]) {
		midorator_setprop(web_view, cmd[1], cmd[2], cmd[3]);

	} else if (strcmp(cmd[0], "tabnew") == 0) {
		char *uri = midorator_make_uri(cmd + 1);
		midorator_process_command(web_view, "js document.defaultView.open('%s', '_blank', 'toolbar=0');", uri);
		free(uri);

	} else if (strcmp(cmd[0], "open") == 0) {
		char *uri = midorator_make_uri(cmd + 1);
		midorator_process_command(web_view, "js document.location = '%s';", uri);
		free(uri);

	} else if (strcmp(cmd[0], "paste") == 0) {
		char *uri = midorator_getclipboard(GDK_SELECTION_PRIMARY);
		midorator_process_command(web_view, "open %s", uri);
		free(uri);

	} else if (strcmp(cmd[0], "tabpaste") == 0) {
		char *uri = midorator_getclipboard(GDK_SELECTION_PRIMARY);
		midorator_process_command(web_view, "tabnew %s", uri);
		free(uri);

	} else if (strcmp(cmd[0], "search") == 0 && cmd[1] && cmd[2]) {
		midorator_options("search", cmd[1], cmd[2]);

	} else if (strcmp(cmd[0], "bookmark") == 0 && cmd[1] && cmd[2]) {
		midorator_options("bookmark", cmd[1], cmd[2]);

	} else if (strcmp(cmd[0], "set") == 0 && cmd[1] && cmd[2] && strcmp(cmd[2], "=") == 0) {
		if (cmd[3])
			midorator_options("option", cmd[1], cmd[3]);
		else
			midorator_options("option", cmd[1], "");

	} else if (strcmp(cmd[0], "set") == 0 && cmd[1] && cmd[2]) {
		midorator_options("option", cmd[1], cmd[2]);

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

	} else if (strcmp(cmd[0], "hint") == 0 && cmd[1] && cmd[1][0]) {
		const char *hintchars = midorator_options("option", "hintchars", NULL);
		if (!hintchars)
			hintchars = "0123456789";
		midorator_process_command(web_view, "js "
#				include "uzbl-follow.h"
				"", hintchars, cmd[1] + 1, (cmd[1][0] == 'F') ? "true" : "false");

	} else if (strcmp(cmd[0], "unhint") == 0) {
		midorator_process_command(web_view, "js "
#				include "uzbl-follow.h"
				"", "01", "a", "false");

	} else if (strcmp(cmd[0], "reload") == 0) {
		webkit_web_view_reload(WEBKIT_WEB_VIEW(web_view));

	} else if (strcmp(cmd[0], "reload!") == 0) {
		webkit_web_view_reload_bypass_cache(WEBKIT_WEB_VIEW(web_view));

	} else if (strcmp(cmd[0], "go") == 0 && cmd[1] && cmd[1][0]) {
		if (strcmp(cmd[1], "next") == 0) {
			midorator_process_command(web_view, "js "
#				include "go-next.h"
					"", "next");
		} else if (strcmp(cmd[1], "prev") == 0) {
			midorator_process_command(web_view, "js "
#				include "go-next.h"
					"", "prev");
		} else if (strcmp(cmd[1], "back") == 0) {
			webkit_web_view_go_back(WEBKIT_WEB_VIEW(web_view));
		} else if (strcmp(cmd[1], "forth") == 0) {
			webkit_web_view_go_forward(WEBKIT_WEB_VIEW(web_view));
		}

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

static void midorator_context_ready_cb (WebKitWebView* web_view, WebKitWebFrame* web_frame, JSContextRef js_context, JSObjectRef js_window, MidoriExtension* extension) {
	midorator_make_js_callback(js_context, web_view);
}

static gboolean midorator_navrequest_cb (WebKitWebView *web_view, WebKitWebFrame *frame, WebKitNetworkRequest *request, WebKitWebNavigationAction *navigation_action, WebKitWebPolicyDecision *policy_decision, MidoriExtension* extension) {
//	midorator_entry(GTK_WIDGET(web_view), NULL);

	GdkEventKey e = {};
	e.keyval = GDK_Escape;
	midorator_key_press_event_cb(web_view, &e, NULL);

	return false;
}

static void midorator_del_tab_cb (MidoriView* view, MidoriBrowser* browser) {
	GtkWidget* web_view = midori_view_get_web_view (view);

	g_signal_handlers_disconnect_by_func (
		web_view, midorator_key_press_event_cb, browser);
	g_signal_handlers_disconnect_by_func (
		web_view, midorator_context_ready_cb, browser);
}

static void midorator_add_tab_cb (MidoriBrowser* browser, MidoriView* view, MidoriExtension* extension) {
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
		midorator_process_command(web_view, "source ~/.midoratorrc");
	}

	gtk_widget_grab_focus(web_view);
}

static void midorator_add_tab_foreach_cb (MidoriView* view, MidoriBrowser* browser, MidoriExtension* extension) {
	midorator_add_tab_cb (browser, view, extension);
}

static void midorator_del_browser_cb (MidoriExtension* extension, MidoriBrowser* browser) {
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

static void midorator_add_browser_cb (MidoriApp* app, MidoriBrowser* browser, MidoriExtension* extension) {
	midori_browser_foreach (browser,
		(GtkCallback)midorator_add_tab_foreach_cb, extension);
	g_signal_connect (browser, "add-tab",
		G_CALLBACK (midorator_add_tab_cb), extension);
	g_signal_connect (extension, "deactivate",
		G_CALLBACK (midorator_del_browser_cb), browser);
}

static void midorator_activate_cb (MidoriExtension* extension, MidoriApp* app) {
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

