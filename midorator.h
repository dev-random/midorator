
#ifndef MIDORATOR_H
#define MIDORATOR_H

#define MIDORATOR_VERSION "0.020101206"

#ifdef DEBUG
#	include <execinfo.h>
#	define static_f
#	define logline (fprintf(stderr, "%s():%i\n", __func__, __LINE__))
#	define logextra(f, ...) (fprintf(stderr, "%s():%i: " f "\n", __func__, __LINE__, __VA_ARGS__))
#else
#	define static_f static
#	define logline
#	define logextra(f, ...)
#endif

// recursively search for widget
#define midorator_findwidget_macro(parent, iter, test) \
{	\
logextra("%s; %s", #parent, #test); \
	GList *__l; \
	GList *__i; \
	__l = gtk_container_get_children(GTK_CONTAINER((parent))); \
	iter = NULL; \
	for (__i = __l; __i; __i = __i->next) { \
		iter = GTK_WIDGET(__i->data); \
logextra("%p: %s", iter, gtk_widget_get_name(iter)); \
		if (test) \
			break; \
		if (GTK_IS_CONTAINER(__i->data)) { \
			GtkContainer *c = GTK_CONTAINER(__i->data); \
			GList *l2 = gtk_container_get_children(c); \
			__l = g_list_concat(__l, l2); \
		} \
	} \
	g_list_free(__l); \
	if (!__i) \
		iter = NULL; \
}

#define midorator_findwidget_up_macro(child, iter, test) \
	for ((iter) = GTK_WIDGET((child)); (iter) && !(test); (iter) = gtk_widget_get_parent(GTK_WIDGET((iter))));


GtkWidget *midorator_findwidget(GtkWidget *web_view, const char *name);


#endif

