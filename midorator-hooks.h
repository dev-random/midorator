
#ifndef __MIDORATOR_HOOKS_H
#define __MIDORATOR_HOOKS_H

#include <midori/midori.h>
#include <gtk/gtk.h>
#include "midorator-webkit.h"

void midorator_hooks_add_view(WebKitWebView *web_view);
void midorator_hooks_add(const char *name, const char *code);
void midorator_hooks_call(_MWT obj, const char *name);


#endif

