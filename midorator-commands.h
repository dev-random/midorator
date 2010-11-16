
#ifndef __MIDORATOR_COMMANDS_H
#define __MIDORATOR_COMMANDS_H

#include <midori/midori.h>
#include <katze/katze-array.h>
#include <gtk/gtk.h>

#define midorator_process_command(web_view, ...) __midorator_process_command(web_view, __VA_ARGS__, NULL)
gboolean __midorator_process_command(GtkWidget *web_view, const char *fmt, ...);
gboolean midorator_process_command_v(GtkWidget *web_view, char **cmd, size_t cmdlen);
char* midorator_process_request(GtkWidget *web_view, char *args[], int arglen);
KatzeArray* midorator_commands_list();


#endif

