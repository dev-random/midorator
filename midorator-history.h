
#ifndef __MIDORATOR_HISTORY
#define __MIDORATOR_HISTORY

#include <midori/midori.h>
#include <katze/katze-array.h>

void midorator_history_add_string(KatzeArray* a, const char *str);
const char* midorator_history_get_string(KatzeArray* a, int i);
KatzeArray* midorator_history_get_browser_history(MidoriApp* app);
KatzeArray* midorator_history_get_command_history(MidoriApp* app);
KatzeArray* midorator_history_get_bookmarks(MidoriApp* app);

#endif

