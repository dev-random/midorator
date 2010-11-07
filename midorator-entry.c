#include "midorator-entry.h"

struct _MidoratorEntry {
	GObject parent_instance;
};

struct _MidoratorEntryClass {
	GObjectClass parent_class;
};

G_DEFINE_TYPE (MidoratorEntry, midorator_entry, GTK_TYPE_ENTRY);

static void midorator_entry_class_init(MidoratorEntryClass *_class) {
}

static void midorator_entry_init(MidoratorEntry *entry) {
}

