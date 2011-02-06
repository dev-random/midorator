
#ifndef __MIDORATOR_HOOKS_H
#define __MIDORATOR_HOOKS_H

#include <midori/midori.h>
#include <gtk/gtk.h>
#include "midorator-webkit.h"

void midorator_hooks_add_view(WebKitWebView *web_view);
void midorator_hooks_add(const char *name, const char *code);
void midorator_hooks_call_argv(_MWT obj, const char *name, const char **argv);
#define midorator_hooks_call(o, n) midorator_hooks_call_argv(o, n, NULL)
void __midorator_hooks_call_args(_MWT obj, const char *name, ...);
#define midorator_hooks_call_args(...) __midorator_hooks_call_args(__VA_ARGS__, NULL)


#endif

