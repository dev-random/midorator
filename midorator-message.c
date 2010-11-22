
#include "midorator-message.h"
#include "midorator.h"

#include <stdbool.h>

static void midorator_message_scroll(GtkLabel* label) {
	GtkBox *box = GTK_BOX(gtk_widget_get_parent(GTK_WIDGET(label)));
	if (!box)
		return;
	GtkScrolledWindow *sw = GTK_SCROLLED_WINDOW(gtk_scrolled_window_new(NULL, NULL));
	gtk_box_pack_start(box, GTK_WIDGET(sw), true, true, 0);

	g_object_ref(label);
	gtk_container_remove(GTK_CONTAINER(box), GTK_WIDGET(label));
	gtk_scrolled_window_add_with_viewport(sw, GTK_WIDGET(label));
	g_object_unref(label);

	gtk_widget_show(GTK_WIDGET(sw));
}

static void midorator_message_unscroll(GtkLabel* label) {
	GtkViewport *vp = GTK_VIEWPORT(gtk_widget_get_parent(GTK_WIDGET(label)));
	if (!vp)
		return;
	GtkScrolledWindow *sw = GTK_SCROLLED_WINDOW(gtk_widget_get_parent(GTK_WIDGET(vp)));
	if (!sw)
		return;
	GtkBox *box = GTK_BOX(gtk_widget_get_parent(GTK_WIDGET(sw)));
	if (!box)
		return;

	g_object_ref(label);
	gtk_container_remove(GTK_CONTAINER(vp), GTK_WIDGET(label));
	gtk_container_remove(GTK_CONTAINER(box), GTK_WIDGET(sw));
	gtk_box_pack_start(box, GTK_WIDGET(label), false, true, 0);
	g_object_unref(label);
}

static void midorator_message_notify_label_cb(GtkLabel* label) {
	const char *text = gtk_label_get_label(label);
	if (!text || !text[0]) {
		midorator_message_unscroll(label);
		gtk_widget_hide(GTK_WIDGET(label));
		return;
	}
	GtkContainer *p = GTK_CONTAINER(gtk_widget_get_parent(GTK_WIDGET(label)));
	GtkWidget *t = gtk_widget_get_toplevel(GTK_WIDGET(label));
	GtkRequisition lr;
	GtkAllocation ta;
	gtk_widget_size_request(GTK_WIDGET(label), &lr);
	gtk_widget_get_allocation(t, &ta);
	if (GTK_IS_BOX(p) && lr.height > ta.height / 2)
		midorator_message_scroll(label);
	else if (!GTK_IS_BOX(p) && lr.height < ta.height / 2)
		midorator_message_unscroll(label);
	gtk_widget_show(GTK_WIDGET(label));
}

GtkLabel* midorator_message_new(GtkWidget *web_view) {
	GtkWidget *w;
	midorator_findwidget_up_macro(web_view, w, GTK_IS_NOTEBOOK(w));
	if (!w)
		return NULL;
	midorator_findwidget_up_macro(w, w, GTK_IS_VBOX(w));
	if (!w)
		return NULL;
	GtkWidget *ret = gtk_label_new(NULL);
	gtk_widget_set_name(ret, "midorator_message");
	gtk_box_pack_start(GTK_BOX(w), ret, false, true, 0);
	gtk_label_set_selectable(GTK_LABEL(ret), true);
	gtk_misc_set_alignment(GTK_MISC(ret), 0, 0);
	
	g_signal_connect (ret, "notify::label",
		G_CALLBACK (midorator_message_notify_label_cb), NULL);

	return GTK_LABEL(ret);
}

GtkLabel* midorator_message_get(GtkWidget *web_view) {
	GtkLabel *label = GTK_LABEL(midorator_findwidget(web_view, "midorator_message"));
	if (!label)
		label = midorator_message_new(web_view);
	return label;
}

void midorator_message(GtkWidget* web_view, const char *message, const char *bg, const char *fg) {
	GtkLabel *label = midorator_message_get(web_view);
	if (!label)
		return;

	if (message) {
		char *text;
		if (bg) {
			if (fg)
				text = g_markup_printf_escaped("<span color=\"%s\" bgcolor=\"%s\">  %s  </span>", fg, bg, message);
			else
				text = g_markup_printf_escaped("<span bgcolor=\"%s\">  %s  </span>", bg, message);
		} else {
			if (fg)
				text = g_markup_printf_escaped("<span color=\"%s\">  %s  </span>", fg, message);
			else
				text = g_markup_printf_escaped("  %s  ", message);
		}

		const char *oldtext = gtk_label_get_label(label);
		char *fulltext;
		if (oldtext && oldtext[0]) {
			fulltext = g_strconcat(oldtext, "\n", text, NULL);
			g_free(text);
		} else
			fulltext = text;
		gtk_label_set_markup(label, fulltext);
		g_free(fulltext);
	} else
		gtk_label_set_markup(label, "");
}

