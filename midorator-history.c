
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
	sqlite3 *db = g_object_get_data(bookmarks, "db");
	if (!db)
		return g_object_ref(bookmarks);
	sqlite3_stmt* stmt;
	sqlite3_prepare_v2(db, "SELECT title, uri, desc FROM bookmarks;", -1, &stmt, NULL);
	while (sqlite3_step(stmt) == SQLITE_ROW) {
		const char *title = sqlite3_column_text(stmt, 0);
		const char *uri = sqlite3_column_text(stmt, 1);
		const char *desc = sqlite3_column_text(stmt, 2);
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
	char *cmd = g_strdup_printf("SELECT %s FROM %s ORDER BY id;", field, table);
	sqlite3_prepare_v2(db, cmd, -1, &stmt, NULL);
	g_free(cmd);
	while (sqlite3_step(stmt) == SQLITE_ROW) {
		char *cmd = g_strdup(sqlite3_column_text(stmt, 0));
		katze_array_add_item(a, cmd);
	}
	sqlite3_finalize(stmt);
}

static void midorator_history_add_item_cb(KatzeArray *a, const char *item, sqlite3 *db) {
	int index = 0;
	sqlite3_stmt* stmt;
	sqlite3_prepare_v2(db, "SELECT id FROM midorator_command_history ORDER BY id DESC LIMIT 1;", -1, &stmt, NULL);
	if (sqlite3_step(stmt) == SQLITE_ROW)
		index = sqlite3_column_int(stmt, 0) + 1;
	sqlite3_finalize(stmt);
	char *cmd = sqlite3_mprintf("INSERT INTO midorator_command_history (id, command) VALUES (%i, '%q');", index, item);
	sqlite3_exec(db, cmd, NULL, NULL, NULL);
	sqlite3_free(cmd);
}

static void midorator_history_remove_item_cb(KatzeArray *a, const char *item, sqlite3 *db) {
	char *cmd = sqlite3_mprintf("DELETE FROM midorator_command_history WHERE command = '%q';", item);
	sqlite3_exec(db, cmd, NULL, NULL, NULL);
	sqlite3_free(cmd);
}

static void midorator_history_move_item_cb(KatzeArray *a, const char *item, int index, sqlite3 *db) {
	sqlite3_stmt* stmt;
	char *cmd = sqlite3_mprintf("SELECT command FROM midorator_command_history WHERE id = %i;", index);
	sqlite3_prepare_v2(db, cmd, -1, &stmt, NULL);
	sqlite3_free(cmd);
	if (sqlite3_step(stmt) == SQLITE_ROW) {
		const char *cmd = sqlite3_column_text(stmt, 0);
		midorator_history_move_item_cb(a, cmd, index + 1, db);
	}
	cmd = sqlite3_mprintf("UPDATE midorator_command_history SET id = %i WHERE command = '%q';", index, item);
	sqlite3_exec(db, cmd, NULL, NULL, NULL);
	sqlite3_free(cmd);
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
	g_signal_connect (ret, "add-item", G_CALLBACK (midorator_history_add_item_cb), db);
	g_signal_connect (ret, "remove-item", G_CALLBACK (midorator_history_remove_item_cb), db);
	g_signal_connect (ret, "move-item", G_CALLBACK (midorator_history_move_item_cb), db);
	g_signal_connect (ret, "clear", G_CALLBACK (midorator_history_clear_cb), db);
	
	g_object_set_data(G_OBJECT(app), "midorator_command_history", ret);

	return ret;
}


