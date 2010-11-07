
#ifndef __MIDORATOR_ENTRY
#define __MIDORATOR_ENTRY

#include <glib-object.h>
#include <gtk/gtk.h>
#include <katze/katze-array.h>

#define MIDORATOR_TYPE_ENTRY			(midorator_entry_get_type())
#define MIDORATOR_ENTRY(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), MIDORATOR_TYPE_ENTRY, MidoratorEntry))
#define MIDORATOR_IS_ENTRY(obj)			(G_TYPE_CHECK_INSTANCE_TYPE ((obj), MIDORATOR_TYPE_ENTRY))
#define MIDORATOR_ENTRY_CLASS(_class)		(G_TYPE_CHECK_CLASS_CAST ((_class), MIDORATOR_TYPE_ENTRY, MidoratorEntryClass))
#define MIDORATOR_IS_ENTRY_CLASS(_class)	(G_TYPE_CHECK_CLASS_TYPE ((_class), MIDORATOR_TYPE_ENTRY))
#define MIDORATOR_ENTRY_GET_CLASS(obj)		(G_TYPE_INSTANCE_GET_CLASS ((obj), MIDORATOR_TYPE_ENTRY, MidoratorEntryClass))

typedef struct _MidoratorEntry MidoratorEntry;
typedef struct _MidoratorEntryClass MidoratorEntryClass;

GType midorator_entry_get_type();

#endif

