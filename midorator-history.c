
#include <sqlite3.h>

#include "midorator-history.h"


KatzeArray* midorator_history_get_browser_history(MidoriApp* app) {
	KatzeArray *history = NULL;
	g_object_get(app, "history", &history, NULL);
	return history;
}

KatzeArray* midorator_history_get_bookmarks(MidoriApp* app) {
	KatzeArray *bookmarks = NULL;
	KatzeArray *ret = katze_array_new(KATZE_TYPE_ITEM);
	g_object_get(app, "bookmarks", &bookmarks, NULL);
	sqlite3 *db = g_object_get_data(G_OBJECT(bookmarks), "db");
	if (!db)
		return g_object_ref(bookmarks);
	sqlite3_stmt* stmt;
	sqlite3_prepare_v2(db, "SELECT title, uri, desc FROM bookmarks;", -1, &stmt, NULL);
	while (sqlite3_step(stmt) == SQLITE_ROW) {
		const char *title = (const char *)sqlite3_column_text(stmt, 0);
		const char *uri = (const char *)sqlite3_column_text(stmt, 1);
		const char *desc = (const char *)sqlite3_column_text(stmt, 2);
		KatzeItem *it = katze_item_new();
		katze_item_set_uri(it, uri);
		katze_item_set_text(it, desc);
		katze_item_set_name(it, title);
		katze_array_add_item(ret, it);
		g_object_unref(it);
	}
	sqlite3_finalize(stmt);
	return ret;
}

static void midorator_history_fill(KatzeArray *a, sqlite3 *db, const char *table, const char *field) {
	sqlite3_stmt* stmt;
	char *cmd = g_strdup_printf("SELECT DISTINCT %s FROM %s ORDER BY id;", field, table);
	sqlite3_prepare_v2(db, cmd, -1, &stmt, NULL);
	g_free(cmd);
	while (sqlite3_step(stmt) == SQLITE_ROW) {
		char *cmd = g_strdup((const char *)sqlite3_column_text(stmt, 0));
		katze_array_add_item(a, cmd);
	}
	sqlite3_finalize(stmt);
}

static void midorator_history_update(KatzeArray *a, sqlite3 *db) {
	sqlite3_exec(db, "BEGIN;", NULL, NULL, NULL);
	sqlite3_exec(db, "DELETE FROM midorator_command_history;", NULL, NULL, NULL);
	int i, l;
	l = katze_array_get_length(a);
	for (i = 0; i < l; i++) {
		const char *item = katze_array_get_nth_item(a, i);
		char *cmd = sqlite3_mprintf("INSERT INTO midorator_command_history (id, command) VALUES (%i, '%q');", i, item);
		sqlite3_exec(db, cmd, NULL, NULL, NULL);
		sqlite3_free(cmd);
	}
	sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
}

static void midorator_history_add_item_cb(KatzeArray *a, const char *item, sqlite3 *db) {
	midorator_history_update(a, db);
}

static void midorator_history_remove_item_cb(KatzeArray *a, const char *item, sqlite3 *db) {
	midorator_history_update(a, db);
}

static void midorator_history_move_item_cb(KatzeArray *a, const char *item, int index, sqlite3 *db) {
	midorator_history_update(a, db);
}

static void midorator_history_clear_cb(KatzeArray *a, sqlite3 *db) {
	sqlite3_exec(db, "DELETE FROM midorator_command_history;", NULL, NULL, NULL);
}

KatzeArray* midorator_history_get_command_history(MidoriApp* app) {
	KatzeArray *ret = g_object_get_data(G_OBJECT(app), "midorator_command_history");
	if (ret)
		return ret;
	KatzeArray *br_history = midorator_history_get_browser_history(app);
	sqlite3 *db = g_object_get_data(G_OBJECT(br_history), "db");
	if (!db)
		return NULL;
	if (sqlite3_exec(db, "CREATE TABLE IF NOT EXISTS midorator_command_history (id integer, command text);", NULL, NULL, NULL) != SQLITE_OK)
		return NULL;
	ret = katze_array_new(G_TYPE_STRING);
	midorator_history_fill(ret, db, "midorator_command_history", "command");
	g_signal_connect_after (ret, "add-item", G_CALLBACK (midorator_history_add_item_cb), db);
	g_signal_connect_after (ret, "remove-item", G_CALLBACK (midorator_history_remove_item_cb), db);
	g_signal_connect_after (ret, "move-item", G_CALLBACK (midorator_history_move_item_cb), db);
	g_signal_connect_after (ret, "clear", G_CALLBACK (midorator_history_clear_cb), db);
	
	g_object_set_data(G_OBJECT(app), "midorator_command_history", ret);

	return ret;
}


