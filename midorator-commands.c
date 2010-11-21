
#include <unistd.h>
#include <stdarg.h>
#include <stdlib.h>

#include "midorator-commands.h"
#include "midorator-history.h"


/* This file contains code that executes user commands.
 * Also it provides information about commands to autocompleter.
 */



// Prototypes of some functions from midorator.c
GtkWidget *midori_view_from_web_view(GtkWidget *web_view);
GtkWidget *midorator_findwidget(GtkWidget *web_view, const char *name);
const char* midorator_options(const char *group, const char *name, const char *value);
char ** midorator_options_keylist(const char *group);
void midorator_setclipboard(GdkAtom atom, const char *str);
char* midorator_getclipboard(GdkAtom atom);


// Find a widget by its name ('widget') and set (or get) its property 'name'.
// midorator_setprop sets a property.
// midorator_getprop returns its value.
// midorator_set_get_prop sets a property and returns its previous value.
// Returned value should be freed.
#define midorator_setprop(web_view, widget, name, value) g_free(midorator_set_get_prop((web_view), (widget), (name), (value)))
#define midorator_getprop(web_view, widget, name) (midorator_set_get_prop((web_view), (widget), (name), NULL))
static char* midorator_set_get_prop(GtkWidget *web_view, const char *widget, const char *name, const char *value) {
	GtkWidget *w = midorator_findwidget(web_view, widget);
	if (!w) {
		midorator_error(web_view, "Widget not found: %s", widget);
		return NULL;
	}
	GtkWidget *p = NULL;
	GParamSpec *sp = g_object_class_find_property(G_OBJECT_GET_CLASS(w), name);
	if (!sp) {
		p = gtk_widget_get_parent(w);
		sp = gtk_container_class_find_child_property(G_OBJECT_GET_CLASS(p), name);
	}
	if (!sp) {
		midorator_error(web_view, "Property for widget '%s' not found: %s", widget, name);
		return NULL;
	}
	int num;
	double d;
	if (value) {
		num = atoi(value);
		d = strtod(value, NULL);
	}
	char *ret = NULL;
	GValue v = {};
	g_value_init(&v, sp->value_type);
	if (p)
		gtk_container_child_get_property(GTK_CONTAINER(p), w, name, &v);
	else
		g_object_get_property(G_OBJECT(w), name, &v);
	switch (G_TYPE_FUNDAMENTAL(sp->value_type)) {
		case G_TYPE_STRING:
			ret = g_value_dup_string(&v);
			if (value)
				g_value_set_string(&v, value);
			break;
		case G_TYPE_ENUM:
			ret = g_strdup_printf("%i", (int)g_value_get_enum(&v));
			if (value)
				g_value_set_enum(&v, num);
			break;
		case G_TYPE_INT:
			ret = g_strdup_printf("%i", g_value_get_int(&v));
			if (value)
				g_value_set_int(&v, num);
			break;
		case G_TYPE_UINT:
			ret = g_strdup_printf("%u", g_value_get_uint(&v));
			if (value)
				g_value_set_uint(&v, num);
			break;
		case G_TYPE_LONG:
			ret = g_strdup_printf("%li", g_value_get_long(&v));
			if (value)
				g_value_set_long(&v, num);
			break;
		case G_TYPE_ULONG:
			ret = g_strdup_printf("%lu", g_value_get_ulong(&v));
			if (value)
				g_value_set_ulong(&v, num);
			break;
		case G_TYPE_INT64:
			ret = g_strdup_printf("%lli", (long long)g_value_get_int64(&v));
			if (value)
				g_value_set_int64(&v, num);
			break;
		case G_TYPE_UINT64:
			ret = g_strdup_printf("%llu", (long long unsigned)g_value_get_uint64(&v));
			if (value)
				g_value_set_uint64(&v, num);
			break;
		case G_TYPE_BOOLEAN:
			ret = g_strdup(g_value_get_boolean(&v) ? "true" : "false");
			if (value)
				g_value_set_boolean(&v, (g_ascii_strcasecmp(value, "true") == 0) ? true : (g_ascii_strcasecmp(value, "false") == 0) ? false : num);
			break;
		case G_TYPE_FLOAT:
			ret = g_strdup_printf("%f", g_value_get_float(&v));
			if (value)
				g_value_set_float(&v, d);
			break;
		case G_TYPE_DOUBLE:
			ret = g_strdup_printf("%lf", g_value_get_double(&v));
			if (value)
				g_value_set_double(&v, d);
			break;
		default:
			midorator_error(web_view, "Unknown property type: '%s'", g_type_name(sp->value_type));
			g_value_unset(&v);
			return NULL;
	}
	if (p)
		gtk_container_child_set_property(GTK_CONTAINER(p), w, name, &v);
	else
		g_object_set_property(G_OBJECT(w), name, &v);
	g_value_unset(&v);
	return ret;
}

// Creates an uri from a list of words entered by user.
// They may be complete or incomplete uri, bookmark name or a search string.
static char* midorator_make_uri(MidoriBrowser *browser, char *args[]) {
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

			MidoriApp *app = g_object_get_data(G_OBJECT(browser), "midori-app");
			KatzeArray *a = midorator_history_get_bookmarks(app);
			if (a) {
				char *key = g_strdup_printf("[%s]", args[0]);
				KatzeItem *it;
				int l = katze_array_get_length(a);
				int i;
				for (i = 0; i < l; i++) {
					it = katze_array_get_nth_item(a, i);
					const char *text = katze_item_get_text(it);
					if (text && strstr(text, key)) {
						g_free(key);
						g_object_unref(a);
						return strdup(katze_item_get_uri(it));
					}
				}
				g_free(key);
				g_object_unref(a);
			}
		}
	}

	const char *search = NULL;

	GtkActionGroup *actions = midori_browser_get_action_group(browser);
	GtkAction *action = gtk_action_group_get_action(actions, "Search");
	KatzeArray *arr = midori_search_action_get_search_engines(MIDORI_SEARCH_ACTION(action));
	KatzeItem *item = katze_array_find_token(arr, args[0]);
	if (item)
		args++;
	else
		item = midori_search_action_get_default_item(MIDORI_SEARCH_ACTION(action));
	if (item)
		search = katze_item_get_uri(item);

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

	char *ret;
	if (strstr(search, "%s"))
		ret = g_strdup_printf(search, arg);
	else
		ret = g_strconcat(search, arg, NULL);
	free(arg);
	return ret;
}

// Callback function to be called by atexit() to restart midori
static void midorator_do_restart() {
	int i;
	for (i = 3; i < 64; i++)
		close(i);
	const char *p = g_get_prgname();
	execlp(p, p, NULL);
}

static void midorator_parse_argv(unsigned char *line, size_t *o_len, char ***o_arr, size_t maxlen) {
	if (!o_len && !o_arr)
		return;
	if (!line)
		line = "";
	size_t len, i;
	len = 0;
	for (i = 0; line[i]; i++)
		if (line[i] > 32 && (i == 0 || line[i-1] <= 32))
			len++;
	if (len > maxlen)
		len = maxlen;
	if (o_len)
		*o_len = len;
	if (o_arr) {
		char **arr = g_new(char*, len + 1);
		arr[len] = NULL;
		*o_arr = arr;
		unsigned char *sbeg = line;
		for (i = 0; i < len; i++) {
			for (; sbeg[0] <= 32; sbeg++);
			if (i == len - 1 && i != 0)
				arr[i] = g_strdup(sbeg);
			else {
				size_t n;
				for (n = 0; sbeg[n] > 32; n++);
				arr[i] = g_strndup(sbeg, n);
				sbeg += n;
			}
		}
	}
}









// The following functions execute corresponding commands
// (they are called by midorator_process_command())

static gboolean midorator_command_widget(GtkWidget *web_view, const char *cmd, char *args[]) {
	midorator_setprop(web_view, args[0], args[1], args[2]);
	return true;
}

static gboolean midorator_command_insert(GtkWidget *web_view, const char *cmd, char *args[]) {
	if (args[0])
		midorator_mode(web_view, args[0][0]);
	else
		midorator_mode(web_view, 'i');
	return true;
}

static gboolean midorator_command_tabnew(GtkWidget *web_view, const char *cmd, char *args[]) {
	MidoriBrowser* browser = midori_browser_get_for_widget(web_view);
	char *uri = midorator_make_uri(browser, args);
	g_signal_emit_by_name(midori_view_from_web_view(web_view), "new-tab", uri, cmd[6] == '!', NULL);
	free(uri);
	return true;
}

static gboolean midorator_command_open(GtkWidget *web_view, const char *cmd, char *args[]) {
	MidoriBrowser* browser = midori_browser_get_for_widget(web_view);
	char *uri = midorator_make_uri(browser, args);
	if (strncmp(uri, "javascript:", strlen("javascript:")) == 0) {
		char *js = g_uri_unescape_string(uri + strlen("javascript:"), NULL);
		char *js2 = g_shell_quote(js);
		midorator_process_command(web_view, NULL, "js", js2);
		g_free(js);
		g_free(js2);
	} else
		webkit_web_view_open(WEBKIT_WEB_VIEW(web_view), uri);
	free(uri);
	return true;
}

static gboolean midorator_command_paste(GtkWidget *web_view, const char *cmd, char *args[]) {
	char *uri = midorator_getclipboard(GDK_SELECTION_PRIMARY);
	midorator_process_command(web_view, NULL, strcmp(cmd, "paste") == 0 ? "open" : "tabnew", uri);
	free(uri);
	return true;
}

static gboolean midorator_command_yank(GtkWidget *web_view, const char *cmd, char *args[]) {
	const char *uri = webkit_web_view_get_uri(WEBKIT_WEB_VIEW(web_view));
	midorator_setclipboard(GDK_SELECTION_PRIMARY, uri);
	midorator_setclipboard(GDK_SELECTION_CLIPBOARD, uri);
	return true;
}

static gboolean midorator_command_search(GtkWidget *web_view, const char *cmd, char *args[]) {
	MidoriBrowser *browser = midori_browser_get_for_widget(web_view);
	GtkActionGroup *actions = midori_browser_get_action_group(browser);
	GtkAction *action = gtk_action_group_get_action(actions, "Search");
	KatzeArray *arr = midori_search_action_get_search_engines(MIDORI_SEARCH_ACTION(action));
	gboolean new = false;
	KatzeItem *item = katze_array_find_token(arr, args[0]);
	if (!item) {
		new = true;
		item = katze_item_new();
	}
	g_object_set (item,
		"name", args[0],
		"text", "",
		"uri", args[1],
		"icon", "",
		"token", args[0],
		NULL);
	if (new)
		katze_array_add_item(arr, item);
	return true;
}

static gboolean midorator_command_set(GtkWidget *web_view, const char *cmd, char *args[]) {
	midorator_options(strcmp(cmd, "set") == 0 ? "option" : cmd, args[0], args[1]);
	return true;
}

static gboolean midorator_command_cmdmap(GtkWidget *web_view, const char *cmd, char *args[]) {
	char *buf = g_strdup("");
	int i;
	for (i=0; args[0][i]; i++) {
		if (args[0][i] == '<') {
			char *end = strchr(args[0] + i, '>');
			if (!end) {
				midorator_error(web_view, "Unfinished '<...>' in command '%s'", cmd[0]);
				g_free(buf);
				return false;
			}
			*end = 0;
			const char *key = args[0] + i + 1;
			int mod = 0;
			char *hyph;
			while ((hyph = strchr(key, '-'))) {
				hyph[0] = 0;
				const char *val = midorator_options("modifier", key, NULL);
				if (!val) {
					midorator_error(web_view, "Unknown modifier: <%s->", key);
					g_free(buf);
					return false;
				}
				mod |= strtol(val, NULL, 16);
				key = hyph + 1;
			}
			const char *val = midorator_options("keycode", key, NULL);
			if (!val) {
				midorator_error(web_view, "Unknown key: <%s>", key);
				g_free(buf);
				return false;
			}
			char *nbuf = g_strdup_printf(mod ? "%s%03x;%x;" : "%s%03x;", buf, (int)strtol(val, NULL, 16), mod);
			g_free(buf);
			buf = nbuf;
			i = end - args[0];
		} else {
			char *nbuf = g_strdup_printf("%s%03x;", buf, args[0][i]);
			g_free(buf);
			buf = nbuf;
		}
		midorator_options(cmd+3, buf, "wait");
	}
	midorator_options(cmd+3, buf, args[1]);
	g_free(buf);
	return true;
}

static gboolean midorator_command_next(GtkWidget *web_view, const char *cmd, char *args[]) {
	midorator_search(web_view, NULL, cmd[4] == 0, false);
	return true;
}

static gboolean midorator_command_entry(GtkWidget *web_view, const char *cmd, char *args[]) {
	midorator_entry(web_view, args[0]);
	return true;
}

static gboolean midorator_command_source(GtkWidget *web_view, const char *cmd, char *args[]) {
	FILE *f = fopen(args[0], "r");
	if (!f && args[0][0] == '~') {
		const char *home = getenv("HOME");
		if (home) {
			char buf[strlen(home) + strlen(args[0])];
			strcpy(buf, home);
			strcat(buf, args[0] + 1);
			f = fopen(buf, "r");
		}
	}
	if (!f) {
		if (cmd[6] == 0)
			midorator_error(web_view, "Can't open file");
		return false;
	}
	char buf[512];
	while (fscanf(f, " %511[^\n\r]", buf) == 1) {
		if (strcmp(buf, "{{{") == 0) {
			char *s = NULL;
			for (;;) {
				if (fscanf(f, " %511[^\n\r]", buf) != 1) {
					midorator_error(web_view, "}}} expected, EOF found");
					if (s)
						g_free(s);
					fclose(f);
					return false;
				}
				if (strcmp(buf, "}}}") == 0)
					break;
				char *sn = g_strconcat(s ? s : "", "\n", buf, NULL);
				if (s)
					g_free(s);
				s = sn;
			}
			if (s) {
				midorator_process_command(web_view, "%s", s);
				g_free(s);
			}
		} else
			midorator_process_command(web_view, "%s", buf);
	}
	fclose(f);
	return true;
}

static gboolean midorator_command_submit(GtkWidget *web_view, const char *cmd, char *args[]) {
	midorator_submit_form(web_view);
	return true;
}

static gboolean midorator_command_wq(GtkWidget *web_view, const char *cmd, char *args[]) {
	// TODO: force saving
	GtkWidget *w;
	for (w = web_view; w && !GTK_IS_WINDOW(w); w = gtk_widget_get_parent(w));
	if (w)
		gtk_widget_destroy(w);
	return true;
}

static gboolean midorator_command_js(GtkWidget *web_view, const char *cmd, char *args[]) {
	midorator_execute_user_script(web_view, args[0]);
	return true;
}

static gboolean midorator_command_hint(GtkWidget *web_view, const char *cmd, char *args[]) {
	if (args[0] && args[0][0]) {
		const char *hintchars = midorator_options("option", "hintchars", NULL);
		if (!hintchars)
			hintchars = "0123456789";
		midorator_hints(web_view, hintchars, args[0] + 1, (args[0][0] == 'F') ? "tabnew" : (args[0][0] == 'y') ? "yank" : (args[0][0] == 'b') ? "bgtab" : (args[0][0] == 'm') ? "multitab" : "click");
	}
	return true;
}

static gboolean midorator_command_unhint(GtkWidget *web_view, const char *cmd, char *args[]) {
	midorator_hints(web_view, NULL, NULL, NULL);
	return true;
}

static gboolean midorator_command_reload(GtkWidget *web_view, const char *cmd, char *args[]) {
	if (cmd[6])
		webkit_web_view_reload_bypass_cache(WEBKIT_WEB_VIEW(web_view));
	else
		webkit_web_view_reload(WEBKIT_WEB_VIEW(web_view));
	return true;
}

static gboolean midorator_command_go(GtkWidget *web_view, const char *cmd, char *args[]) {
	if (strcmp(args[0], "back") == 0) {
		webkit_web_view_go_back(WEBKIT_WEB_VIEW(web_view));
	} else if (strcmp(args[0], "forth") == 0) {
		webkit_web_view_go_forward(WEBKIT_WEB_VIEW(web_view));
	} else {
		midorator_go(web_view, args[0]);
	}
	return true;
}

static gboolean midorator_command_action(GtkWidget *web_view, const char *cmd, char *args[]) {
	MidoriBrowser *browser = midori_browser_get_for_widget(web_view);
	GtkActionGroup *actions = midori_browser_get_action_group(browser);
	GtkAction *action = gtk_action_group_get_action(actions, args[0]);
	if (action)
		gtk_action_activate(action);
	else {
		midorator_error(web_view, "No such action: '%s'", args[0]);
		return false;
	}
	return true;
}

static gboolean midorator_command_actions(GtkWidget *web_view, const char *cmd, char *args[]) {
	MidoriBrowser *browser = midori_browser_get_for_widget(web_view);
	GtkActionGroup *actions = midori_browser_get_action_group(browser);
	midorator_message(web_view, "Known actions are:", NULL, NULL);
	//int sid = g_signal_lookup("activate", GTK_TYPE_ACTION);
	GList *l = gtk_action_group_list_actions(actions);
	GList *li;
	for (li = l; li; li = li->next)
		if (gtk_action_is_sensitive(GTK_ACTION(li->data))) {
			char *msg = g_strdup_printf("%s - %s", gtk_action_get_name(GTK_ACTION(li->data)), gtk_action_get_label(GTK_ACTION(li->data)));
			midorator_message(web_view, msg, NULL, NULL);
			g_free(msg);
		}
	g_list_free(l);
	return true;
}

static gboolean midorator_command_killtab(GtkWidget *web_view, const char *cmd, char *args[]) {
	char *end = NULL;
	int n = strtol(args[0], &end, 0);
	if (!end || end[0]) {
		midorator_error(web_view, "killtab: number expected");
		return false;
	}
	GtkNotebook *nb = GTK_NOTEBOOK(midorator_findwidget(web_view, "tabs"));
	GtkWidget *page = gtk_notebook_get_nth_page(nb, n);
	if (!page) {
		midorator_error(web_view, "killtab: no such tab");
		return false;
	}
	gtk_widget_destroy(page);
	return true;
}

static gboolean midorator_command_restart(GtkWidget *web_view, const char *cmd, char *args[]) {
	// FIXME: this deadlocks when Midori is compiled with libunique
	atexit(midorator_do_restart);
	MidoriBrowser *browser = midori_browser_get_for_widget(web_view);
	midori_browser_quit(browser);
	return true;
}

static gboolean midorator_command_get(GtkWidget *web_view, const char *cmd, char *args[]) {
	char *reply = midorator_process_request(web_view, args, -1);
	if (reply) {
		midorator_message(web_view, reply, NULL, NULL);
		g_free(reply);
	}
	return true;
}

static gboolean midorator_command_alias(GtkWidget *web_view, const char *cmd, char *args[]) {
	const char *name = args[0];
	char *code = NULL;
	int i;
	for (i = 1; args[i]; i++) {
		int l = strlen(args[i]);
		if (l > 1024) {
			midorator_error(web_view, "alias: too long argument");
			return false;
		}
		char buf[l * 2 + 3];
		buf[0] = '"';
		int j, k;
		for (j = 0, k = 1; args[i][j]; j++, k++) {
			if (args[i][j] == '"' || args[i][j] == '\\')
				buf[k++] = '\\';
			buf[k] = args[i][j];
		}
		buf[k++] = '"';
		buf[k] = 0;
		char *c2;
		if (code) {
			c2 = g_strconcat(code, ", ", buf, NULL);
			g_free(code);
		} else
			c2 = g_strconcat("var xarr = new Array(", buf, NULL);
		code = c2;
	}
	char *c2 = g_strconcat(code, "); xarr = xarr.concat(args); command.apply(window, xarr);", NULL);
	g_free(code);
	bool r = midorator_process_command(web_view, NULL, "jscmd", name, c2);
	g_free(c2);
	return r;
}










typedef struct midorator_builtin {
	const char *name;
	int min_args, max_args;
	gboolean (*func) (GtkWidget *web_view, const char *cmd, char *args[]);
} midorator_builtin;

midorator_builtin midorator_commands_builtin[] = {
	{ "action", 1, 1, midorator_command_action },
	{ "actions", 0, 0, midorator_command_actions },
	{ "alias", 2, 1024, midorator_command_alias },
	{ "bookmark", 2, 2, midorator_command_set },
	{ "cmdmap", 2, 2, midorator_command_cmdmap },
	{ "cmdnmap", 2, 2, midorator_command_cmdmap },
	{ "entry", 1, 1, midorator_command_entry },
	{ "get", 1, 1024, midorator_command_get },
	{ "go", 1, 1, midorator_command_go },
	{ "hint", 1, 1, midorator_command_hint },
	{ "insert", 0, 1, midorator_command_insert },
	{ "js", 1, 1, midorator_command_js },
	{ "jscmd", 2, 2, midorator_command_set },
	{ "killtab", 1, 1, midorator_command_killtab },
	{ "next!", 0, 0, midorator_command_next },
	{ "next", 0, 0, midorator_command_next },
	{ "open", 0, 1024, midorator_command_open },
	{ "paste", 0, 0, midorator_command_paste },
	{ "reload!", 0, 0, midorator_command_reload },
	{ "reload", 0, 0, midorator_command_reload },
	{ "restart", 0, 0, midorator_command_restart },
	{ "search", 2, 2, midorator_command_search },
	{ "set", 2, 2, midorator_command_set },
	{ "source", 1, 1, midorator_command_source },
	{ "source!", 1, 1, midorator_command_source },
	{ "submit", 0, 0, midorator_command_submit },
	{ "tabnew!", 0, 1024, midorator_command_tabnew },
	{ "tabnew", 0, 1024, midorator_command_tabnew },
	{ "tabpaste", 0, 0, midorator_command_paste },
	{ "unhint", 0, 0, midorator_command_unhint },
	{ "widget", 3, 3, midorator_command_widget },
	{ "wq", 0, 0, midorator_command_wq },
	{ "yank", 0, 0, midorator_command_yank },

	{ NULL }
};


static size_t midorator_command_get_maxlen(const char *cmd) {
	if (!cmd || !cmd[0])
		return -1;
	int i;
	for (i = 0; midorator_commands_builtin[i].name; i++)
		if (strcmp(midorator_commands_builtin[i].name, cmd) == 0)
			return midorator_commands_builtin[i].max_args;
	return -1;
}

gboolean __midorator_process_command(GtkWidget *web_view, const char *fmt, ...) {
	char **cmd = NULL;
	size_t cmdlen;
	if (fmt) {
		va_list l;
		va_start(l, fmt);
		char *line = g_strdup_vprintf(fmt, l);
		va_end(l);
		GError *err = NULL;
		midorator_parse_argv(line, &cmdlen, &cmd, -1);
		size_t maxlen = midorator_command_get_maxlen(cmd[0]);
		if (cmdlen - 1 > maxlen) {
			g_strfreev(cmd);
			midorator_parse_argv(line, &cmdlen, &cmd, maxlen + 1);
		}
		g_free(line);
		if (cmdlen == 0)
			return false;
	} else {
		int i;
		va_list l;
		va_start(l, fmt);
		for (i = 0; va_arg(l, char*); i++);
		va_end(l);
		cmd = g_new0(char*, i+1);
		va_start(l, fmt);
		for (i = 0; (cmd[i] = va_arg(l, char*)); i++)
			cmd[i] = g_strdup(cmd[i]);
		cmdlen = i;
		va_end(l);
	}
	gboolean ret = midorator_process_command_v(web_view, cmd, cmdlen);
	g_strfreev(cmd);
	return ret;
}

gboolean midorator_process_command_v(GtkWidget *web_view, char **cmd, size_t cmdlen) {
	long prefix = 0;
	char *command = cmd[0];
	char **args = cmd + 1;
	if (cmdlen == 0 || command[0] == '#')
		return false;

	if (command[0] >= '0' && command[0] <= '9')
		prefix = strtol(command, &command, 10);

	int i;
	for (i = 0; midorator_commands_builtin[i].name; i++)
		if (strcmp(command, midorator_commands_builtin[i].name) == 0) {
			if (cmdlen - 1 < midorator_commands_builtin[i].min_args || cmdlen - 1 > midorator_commands_builtin[i].max_args) {
				if (midorator_commands_builtin[i].min_args == midorator_commands_builtin[i].max_args)
					midorator_error(web_view, "Command '%s' expects exactly %i arguments; %i found", command, midorator_commands_builtin[i].min_args, cmdlen - 1);
				else
					midorator_error(web_view, "Command '%s' expects %i to %i arguments", command, midorator_commands_builtin[i].min_args, midorator_commands_builtin[i].max_args);
				return false;
			}
			bool r = true;
			if (!prefix)
				prefix = 1;
			int j;
			for (j = 0; j < prefix && r; j++)
				r = midorator_commands_builtin[i].func(web_view, command, args);
			return r;
		}
	const char *js = midorator_options("jscmd", command, NULL);
	if (js) {
		midorator_jscmd(web_view, js, args, cmdlen - 1);
		return true;
	} else {
		midorator_error(web_view, "Invalid command: '%s'", command);
		return false;
	}
}

char* midorator_process_request(GtkWidget *web_view, char *args[], int arglen) {
	if (arglen == 0)
		return NULL;
	if (arglen < 0)
		for (arglen = 0; args[arglen]; arglen++);
	if (strcmp(args[0], "widget") == 0) {
		return midorator_getprop(web_view, arglen > 1 ? args[1] : "", arglen > 2 ? args[2] : "");
	} else if (strcmp(args[0], "option") == 0) {
		return g_strdup(midorator_options(arglen > 2 ? args[1] : "option", arglen > 2 ? args[2] : args[1], NULL));
	}
	return NULL;
}

KatzeArray* midorator_commands_list() {
	char **jscmd = midorator_options_keylist("jscmd");
	int i;
	int l = 0;
	KatzeArray *ret = katze_array_new(KATZE_TYPE_ITEM);
	for (i = 0; midorator_commands_builtin[i].name; i++)
		katze_array_add_item(ret, g_object_new(KATZE_TYPE_ITEM, "token", midorator_commands_builtin[i].name, NULL));
	for (i = 0; jscmd[i]; i++)
		katze_array_add_item(ret, g_object_new(KATZE_TYPE_ITEM, "token", jscmd[i], NULL));
	g_strfreev(jscmd);
	return ret;
}


