#include "midorator-entry.h"
#include "midorator.h"

#include <stdbool.h>
#include <string.h>

#include <gdk/gdkkeysyms.h>

struct _MidoratorEntry {
	GtkEntry parent_instance;

	KatzeArray *completion_array;
	KatzeArray *command_history;
	GtkLabel *message_label;
	GtkWidget *current_browser;
};

struct _MidoratorEntryClass {
	GtkEntryClass parent_class;

	void (*completion_cb)(MidoratorEntry *e, const char *str);
	void (*execute_cb)(MidoratorEntry *e, const char *str);
	void (*cancel_cb)(MidoratorEntry *e);
	void (*old_grab_focus)(GtkWidget *w);
};

enum {
	SIG_EXECUTE,
	SIG_CANCEL,
	SIG_COMPLETION_REQUEST,

	SIG_COUNT
};

enum {
	PROP_0,

	PROP_COMMAND_HISTORY,
	PROP_COMPLETION_ARRAY,
	PROP_MESSAGE_LABEL,
	PROP_CURRENT_BROWSER,
};

static guint signals[SIG_COUNT];

G_DEFINE_TYPE (MidoratorEntry, midorator_entry, GTK_TYPE_ENTRY);

static void midorator_entry_set_property (GObject *o, guint pid, const GValue *val, GParamSpec *psp) {
	MidoratorEntry* e = MIDORATOR_ENTRY(o);
	if (!e)
		return;
	switch(pid) {
		case PROP_COMMAND_HISTORY:
			if (e->command_history)
				g_object_unref(e->command_history);
			e->command_history = g_object_ref(g_value_get_object(val));
			break;

		case PROP_COMPLETION_ARRAY:
			if (e->completion_array)
				g_object_unref(e->completion_array);
			e->completion_array = g_object_ref(g_value_get_object(val));
			break;

		case PROP_MESSAGE_LABEL:
			if (e->message_label)
				g_object_unref(e->message_label);
			e->message_label = g_object_ref(g_value_get_object(val));
			break;

		case PROP_CURRENT_BROWSER:
			if (e->current_browser)
				g_object_unref(e->current_browser);
			e->current_browser = g_object_ref(g_value_get_object(val));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(o, pid, psp);
	}
}

static void midorator_entry_get_property (GObject *o, guint pid, GValue *val, GParamSpec *psp) {
	MidoratorEntry* e = MIDORATOR_ENTRY(o);
	if (!e)
		return;
	switch(pid) {
		case PROP_COMMAND_HISTORY:
			g_value_set_object(val, e->command_history);
			break;

		case PROP_COMPLETION_ARRAY:
			g_value_set_object(val, e->completion_array);
			break;

		case PROP_MESSAGE_LABEL:
			g_value_set_object(val, e->message_label);
			break;

		case PROP_CURRENT_BROWSER:
			g_value_set_object(val, e->current_browser);
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(o, pid, psp);
	}
}

const char* midorator_options(const char *group, const char *name, const char *value);
static gboolean midorator_string_to_bool(const char *string) {
	return
		g_ascii_strcasecmp(string, "true") ||
		g_ascii_strcasecmp(string, "yes") ||
		g_ascii_strcasecmp(string, "on") ||
		g_ascii_strcasecmp(string, "+") ||
		g_ascii_strcasecmp(string, "1");
}

static GList* midorator_entry_get_comp_list(MidoratorEntry* e, const char* str) {
	if (!str)
		return NULL;
	size_t sl = strlen(str);
	KatzeArray *c = e->completion_array;
	if (c == NULL)
		return NULL;
	if (!katze_array_is_a(c, G_TYPE_STRING) && !katze_array_is_a(c, KATZE_TYPE_ITEM))
		return NULL;
	guint l = katze_array_get_length(c);
	guint i;
	GList *list = 0;
	guint count = 0;
	for (i = 0; i < l && count < 256; i++) {
		const char *s;
		if (katze_array_is_a(c, G_TYPE_STRING))
			s = katze_array_get_nth_item(c, i);
		else
			s = katze_item_get_token(katze_array_get_nth_item(c, i));
		if (strncmp(s, str, sl) == 0) {
			list = g_list_append(list, (gpointer)s);
			count++;
		}
	}
	return list;
}

static void midorator_entry_find_completion_cb(MidoratorEntry* e, const char* str) {
	GtkLabel *l = e->message_label;
	if (!l)
		return;
	GList *list = midorator_entry_get_comp_list(e, str);
	if (!list)
		return;
	char *c = g_strdup(list->data);
	GList *i;
	for (i = list->next; i; i = i->next) {
		char *n = g_strconcat(c, "\n", i->data, NULL);
		g_free(c);
		c = n;
	}
	g_list_free(list);
	gtk_label_set_text(l, c);
	g_free(c);
}

static void midorator_entry_perform_completion(MidoratorEntry* e) {
	char *str = g_strdup(gtk_entry_get_text(GTK_ENTRY(e)));
	int prelen = 0;
	if (str[0]) {
		prelen = g_utf8_offset_to_pointer(str, gtk_editable_get_position(GTK_EDITABLE(e))) - str;
		str[prelen] = 0;
	}
	GList *list = midorator_entry_get_comp_list(e, str);
	g_free(str);
	if (!list)
		return;
	str = g_strdup(list->data);
	GList *i;
	bool crop = false;
	for (i = list->next; i; i = i->next) {
		const char *s = i->data;
		int j;
		for (j = 0; s[j] && s[j] == str[j]; j++);
		if (str[j]) {
			crop = true;
			str[j] = 0;
		}
	}
	int pos = gtk_editable_get_position(GTK_EDITABLE(e));
	gtk_editable_insert_text(GTK_EDITABLE(e), str + prelen, -1, &pos);
	if (!crop)
		gtk_editable_insert_text(GTK_EDITABLE(e), " ", -1, &pos);
	gtk_editable_set_position(GTK_EDITABLE(e), pos);
	g_free(str);
}

static void midorator_entry_execute_default_cb(MidoratorEntry* e, const char* str) {
	(void)str;
	gtk_entry_set_text(GTK_ENTRY(e), "");
	gtk_widget_hide(GTK_WIDGET(e));
	if (e->current_browser)
		gtk_widget_grab_focus(e->current_browser);
}

static void midorator_entry_cancel_default_cb(MidoratorEntry* e) {
	gtk_entry_set_text(GTK_ENTRY(e), "");
	gtk_widget_hide(GTK_WIDGET(e));
	if (e->current_browser)
		gtk_widget_grab_focus(e->current_browser);
}

// We don't need that stupid 'gtk-entry-select-on-focus' stuff.
// So we redefine 'grab_focus'.
static void midorator_entry_grab_focus_cb(GtkWidget *w) {
	gboolean old;
	g_object_get(gtk_settings_get_default(), "gtk-entry-select-on-focus", &old, NULL);
	g_object_set(gtk_settings_get_default(), "gtk-entry-select-on-focus", FALSE, NULL);
	MIDORATOR_ENTRY_GET_CLASS(w)->old_grab_focus(w);
	g_object_set(gtk_settings_get_default(), "gtk-entry-select-on-focus", old, NULL);
}

static void midorator_entry_class_init(MidoratorEntryClass *_class) {
	_class->completion_cb = midorator_entry_find_completion_cb;
	_class->execute_cb = midorator_entry_execute_default_cb;
	_class->cancel_cb = midorator_entry_cancel_default_cb;
	GObjectClass *gc = (GObjectClass*)_class;
	gc->get_property = midorator_entry_get_property;
	gc->set_property = midorator_entry_set_property;
	GtkWidgetClass *wc = (GtkWidgetClass*)_class;
	_class->old_grab_focus = wc->grab_focus;
	wc->grab_focus = midorator_entry_grab_focus_cb;
	g_object_class_install_property(G_OBJECT_CLASS(_class),
			PROP_COMMAND_HISTORY,
			g_param_spec_object("command-history",
				"Array of commands history",
				"Katze array for storing history of commands",
				KATZE_TYPE_ARRAY,
				G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
	g_object_class_install_property(G_OBJECT_CLASS(_class),
			PROP_COMPLETION_ARRAY,
			g_param_spec_object("completion-array",
				"Array of command completions",
				"Katze array with strings for auto-completion",
				KATZE_TYPE_ARRAY,
				G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
	g_object_class_install_property(G_OBJECT_CLASS(_class),
			PROP_MESSAGE_LABEL,
			g_param_spec_object("message-label",
				"Label for messages",
				"Large multi-line label to display messages in",
				GTK_TYPE_LABEL,
				G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
	g_object_class_install_property(G_OBJECT_CLASS(_class),
			PROP_CURRENT_BROWSER,
			g_param_spec_object("current-browser",
				"Current browser widget",
				"A browser widget for which commands are entered",
				GTK_TYPE_WIDGET,
				G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	signals[SIG_EXECUTE] = g_signal_new("execute",
			G_TYPE_FROM_CLASS(_class),
			G_SIGNAL_RUN_FIRST,
			G_STRUCT_OFFSET(MidoratorEntryClass, execute_cb),
			NULL,
			NULL,
			g_cclosure_marshal_VOID__STRING,
			G_TYPE_NONE,
			1,
			G_TYPE_STRING);
	signals[SIG_CANCEL] = g_signal_new("cancel",
			G_TYPE_FROM_CLASS(_class),
			G_SIGNAL_RUN_FIRST,
			G_STRUCT_OFFSET(MidoratorEntryClass, cancel_cb),
			NULL,
			NULL,
			g_cclosure_marshal_VOID__VOID,
			G_TYPE_NONE,
			0);
	signals[SIG_COMPLETION_REQUEST] = g_signal_new("completion-request",
			G_TYPE_FROM_CLASS(_class),
			G_SIGNAL_RUN_LAST,
			G_STRUCT_OFFSET(MidoratorEntryClass, completion_cb),
			NULL,
			NULL,
			g_cclosure_marshal_VOID__STRING,
			G_TYPE_NONE,
			1,
			G_TYPE_STRING);
}

static gboolean midorator_entry_is_visible(MidoratorEntry* e) {
	gboolean v;
	g_object_get(e, "visible", &v, NULL);
	return v;
}

static gboolean midorator_entry_restore_focus(MidoratorEntry* e) {
	if (midorator_entry_is_visible(e)) {
		int p = gtk_editable_get_position(GTK_EDITABLE(e));
		gtk_widget_grab_focus(GTK_WIDGET(e));
		gtk_editable_set_position(GTK_EDITABLE(e), p);
	}
	return false;
}

static void midorator_entry_edited_cb(MidoratorEntry* e) {
	if (!midorator_entry_is_visible(e)) {
		gtk_widget_show(GTK_WIDGET(e));
		gtk_widget_grab_focus(GTK_WIDGET(e));
	}
	char *t = g_strdup(gtk_entry_get_text(GTK_ENTRY(e)));
	if (t && t[0]) {
		int l = g_utf8_strlen(t, -1);
		int p = gtk_editable_get_position(GTK_EDITABLE(e));
		if (p < l)
			g_utf8_offset_to_pointer(t, p)[0] = 0;
		g_signal_emit(e, signals[SIG_COMPLETION_REQUEST], 0, t);
	} else
		g_signal_emit(e, signals[SIG_CANCEL], 0);
	g_free(t);
}

static void midorator_entry_history_add(MidoratorEntry* e, const char *str) {
	if (!str)
		str = gtk_entry_get_text(GTK_ENTRY(e));
	KatzeArray *c = e->command_history;
	if (!c)
		return;
	if (!katze_array_is_a(c, G_TYPE_STRING))
		return;
	int i, l = katze_array_get_length(c);
	for (i = 0; i < l; i++)
		if (strcmp(katze_array_get_nth_item(c, i), str) == 0) {
			katze_array_move_item(c, katze_array_get_nth_item(c, i), l);
			return;
		}
	katze_array_add_item(c, g_strdup(str));
	while (katze_array_get_length(c) > 256) {
		char *i = katze_array_get_nth_item(c, 0);
		g_free(i);
		katze_array_remove_item(c, i);
	}
}

static void midorator_entry_history_up(MidoratorEntry* e) {
	const char *full = gtk_entry_get_text(GTK_ENTRY(e));
	guint pos = gtk_editable_get_position(GTK_EDITABLE(e));
	char *prefix = g_strndup(full, g_utf8_offset_to_pointer(full, pos) - full);
	bool pre = strlen(prefix) == strlen(full);
	KatzeArray *c = e->command_history;
	if (!c)
		return;
	if (!katze_array_is_a(c, G_TYPE_STRING))
		return;
	int i, l = katze_array_get_length(c);
	char *found = NULL;
	for (i = 0; i < l; i++) {
		char *item = katze_array_get_nth_item(c, i);
		if (!pre && strcmp(item, full) == 0)
			break;
		else if (g_str_has_prefix(item, prefix) && strlen(item) != strlen(prefix))
			found = item;
	}
	if (found) {
		gtk_entry_set_text(GTK_ENTRY(e), found);
		gtk_editable_set_position(GTK_EDITABLE(e), pos);
	}
}

static void midorator_entry_history_down(MidoratorEntry* e) {
	const char *full = gtk_entry_get_text(GTK_ENTRY(e));
	guint pos = gtk_editable_get_position(GTK_EDITABLE(e));
	char *prefix = g_strndup(full, g_utf8_offset_to_pointer(full, pos) - full);
	bool pre = strlen(prefix) == strlen(full);
	KatzeArray *c = e->command_history;
	if (!c)
		return;
	if (!katze_array_is_a(c, G_TYPE_STRING))
		return;
	int i, l = katze_array_get_length(c);
	char *found = NULL;
	for (i = l - 1; i >= 0; i--) {
		char *item = katze_array_get_nth_item(c, i);
		if (!pre && strcmp(item, full) == 0)
			break;
		else if (g_str_has_prefix(item, prefix) && strlen(item) != strlen(prefix))
			found = item;
	}
	if (found) {
		gtk_entry_set_text(GTK_ENTRY(e), found);
		gtk_editable_set_position(GTK_EDITABLE(e), pos);
	}
}

static gboolean midorator_entry_key_press_event_cb (MidoratorEntry* e, GdkEventKey* event) {
	if ((event->state & GDK_CONTROL_MASK) && event->keyval == GDK_a) {
		gtk_editable_set_position(GTK_EDITABLE(e), 0);
	} else if ((event->state & GDK_CONTROL_MASK) && event->keyval == GDK_e) {
		gtk_editable_set_position(GTK_EDITABLE(e), -1);
	} else if ((event->state & GDK_CONTROL_MASK) && event->keyval == GDK_f) {
		gtk_editable_set_position(GTK_EDITABLE(e), gtk_editable_get_position(GTK_EDITABLE(e)) + 1);
	} else if ((event->state & GDK_CONTROL_MASK) && event->keyval == GDK_b) {
		gtk_editable_set_position(GTK_EDITABLE(e), gtk_editable_get_position(GTK_EDITABLE(e)) - 1);
	} else if ((event->state & GDK_CONTROL_MASK) && event->keyval == GDK_p) {
		gtk_editable_set_position(GTK_EDITABLE(e), 0);
		midorator_entry_history_up(e);
	} else if ((event->state & GDK_CONTROL_MASK) && event->keyval == GDK_n) {
		gtk_editable_set_position(GTK_EDITABLE(e), 0);
		midorator_entry_history_down(e);
	} else if ((event->state & GDK_CONTROL_MASK) && event->keyval == GDK_w) {
		const char *t = gtk_entry_get_text(GTK_ENTRY(e));
		int pos = gtk_editable_get_position(GTK_EDITABLE(e));
		char *i;
		for (i = g_utf8_offset_to_pointer(t, pos) - 1; i >= t && *i == ' '; i--);
		for (; i >= t && *i != ' '; i--);
		i++;
		int newpos = g_utf8_pointer_to_offset(t, i);
		gtk_editable_delete_text(GTK_EDITABLE(e), newpos, pos);
		gtk_editable_set_position(GTK_EDITABLE(e), newpos);
	} else if (event->keyval == GDK_Escape) {
		g_signal_emit(e, signals[SIG_CANCEL], 0);
	} else if (event->keyval == GDK_Tab) {
		midorator_entry_perform_completion(e);
	} else if (event->keyval == GDK_Up) {
		midorator_entry_history_up(e);
	} else if (event->keyval == GDK_Down) {
		midorator_entry_history_down(e);
	} else if (event->keyval == GDK_Page_Up) {
		gtk_editable_set_position(GTK_EDITABLE(e), 0);
		midorator_entry_history_up(e);
	} else if (event->keyval == GDK_Page_Down) {
		gtk_editable_set_position(GTK_EDITABLE(e), 0);
		midorator_entry_history_down(e);
	} else if (event->keyval == GDK_Return) {
		char *t = g_strdup(gtk_entry_get_text(GTK_ENTRY(e)));
		g_signal_emit(e, signals[SIG_EXECUTE], 0, t);
		midorator_entry_history_add(e, t);
		g_free(t);
	} else
		return false;
	return true;
}

static gboolean midorator_entry_paste_clipboard_cb (MidoratorEntry* e, GtkWidget* web_view) {
	if (!midorator_string_to_bool(midorator_options("option", "paste_primary", NULL)))
		return false;
	g_signal_stop_emission_by_name(e, "paste-clipboard");
	char *text = gtk_clipboard_wait_for_text(gtk_clipboard_get(GDK_SELECTION_PRIMARY));
	if (!text)
		return true;
	gtk_editable_delete_selection(GTK_EDITABLE(e));
	int pos = gtk_editable_get_position(GTK_EDITABLE(e));
	gtk_editable_insert_text(GTK_EDITABLE(e), text, -1, &pos);
	gtk_editable_set_position(GTK_EDITABLE(e), pos);
	g_free(text);
	return true;
}

static void midorator_entry_init(MidoratorEntry *e) {
	g_signal_connect (e, "notify::text",
		G_CALLBACK (midorator_entry_edited_cb), NULL);
	g_signal_connect (e, "key-press-event",
		G_CALLBACK (midorator_entry_key_press_event_cb), NULL);
	g_signal_connect (e, "focus-out-event",
		G_CALLBACK (midorator_entry_restore_focus), NULL);
	g_signal_connect (e, "paste-clipboard",
		G_CALLBACK (midorator_entry_paste_clipboard_cb), NULL);
}

MidoratorEntry* midorator_entry_new(GtkWidget *parent) {
	GtkWidget *e = g_object_new(MIDORATOR_TYPE_ENTRY, NULL);
	gtk_entry_set_has_frame(GTK_ENTRY(e), false);
	if (parent) {
		gtk_widget_modify_base(e, GTK_STATE_NORMAL, &parent->style->bg[0]);
		gtk_widget_modify_base(e, GTK_STATE_ACTIVE, &parent->style->bg[0]);
		gtk_widget_modify_bg(e, GTK_STATE_NORMAL, &parent->style->bg[0]);
		gtk_widget_modify_bg(e, GTK_STATE_ACTIVE, &parent->style->bg[0]);
	}
	if (GTK_IS_BOX(parent)) {
		gtk_box_pack_start(GTK_BOX(parent), e, true, true, 0);
		gtk_widget_show(e);
		gtk_widget_grab_focus(e);
		gtk_box_reorder_child(GTK_BOX(parent), e, 0);
	} else if (GTK_IS_CONTAINER(parent)) {
		gtk_container_add(GTK_CONTAINER(parent), e);
		gtk_widget_show(e);
	}
	return MIDORATOR_ENTRY(e);
	//return (MidoratorEntry*)(e);
}

