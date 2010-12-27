
#include "midorator.h"
#include "midorator-hooks.h"
#include "midorator-webkit.h"






static char* strcat_realloc(char *to, const char *from) {
	if (to == NULL)
		return g_strdup(from);
	int len = strlen(to) + strlen(from) + 1;
	to = (char*)g_realloc(to, len);
	strcat(to, from);
	return to;
}

static char* midorator_hooks_make_css() {
	char *ret = NULL;

	const char *hintstyle = midorator_options("option", "hint_style", NULL);
	if (!hintstyle)
		hintstyle =
			"\tbackground: #59FF00 !important;\n"
			"\tborder: 2px solid #4A6600 !important;\n"
			"\tcolor: black !important;\n"
			"\tfont-size: 9px !important;\n"
			"\tfont-weight: bold !important;\n"
			"\tline-height: 9px !important;\n"
			"\tz-index: 1000 !important;\n"
			"\tmargin: 0px !important;\n"
			"\tpadding: 1px !important;\n"
			"\tborder-radius: 6px !important;\n";

	ret = g_strconcat("div.midorator_hint {\n", hintstyle, "}\n", NULL);
	return ret;
}


static _MWT midorator_hooks_onload_event_cb(_MWT root, _MWT func, _MWT _this, _MWT args) {
	char *uri = midorator_webkit_to_string(midorator_webkit_getprop(root, "location"));
	if (strncmp(uri, "about:", 6) == 0)
		midorator_hooks_call(root, uri);
	midorator_hooks_call(root, "load");
	return root;
}

static _MWT midorator_hooks_earlyload_cb(_MWT root, _MWT func, _MWT _this, _MWT args) {
	_MWT doc = midorator_webkit_getprop(root, "document");
	_MWT de = midorator_webkit_getprop(doc, "documentElement");
	if (!midorator_webkit_isok_nz(de)) {
		_MWT sto = midorator_webkit_getprop(root, "setTimeout");
		_MWT tmcb = midorator_webkit_from_callback(root, midorator_hooks_earlyload_cb);
		_MWT ret = midorator_webkit_call(sto, tmcb, midorator_webkit_from_number(root, 10));
		if (ret.error[0])
			midorator_error(GTK_WIDGET(root.web_view), ret.error);
		return root;
	}
	_MWT ce = midorator_webkit_getprop(doc, "createElement");
	_MWT st = midorator_webkit_call(ce, midorator_webkit_from_string(ce, "style"));
	midorator_webkit_setprop(st, "type", midorator_webkit_from_string(ce, "text/css"));
	char *css = midorator_hooks_make_css();
	midorator_webkit_setprop(st, "innerHTML", midorator_webkit_from_string(ce, css));
	g_free(css);
	_MWT head = midorator_webkit_getprop(doc, "head");
	_MWT ac = midorator_webkit_getprop(head, "appendChild");
	_MWT ret = midorator_webkit_call(ac, st);
	if (ret.error[0])
		midorator_error(GTK_WIDGET(root.web_view), ret.error);

	midorator_hooks_call(root, "earlyload");

	return root;
}

static void midorator_hooks_window_object_cleared_cb(WebKitWebView *web_view, WebKitWebFrame *web_frame, gpointer context, gpointer window_object, gpointer user_data) {
	_MWT root = midorator_webkit_getframe(web_view, web_frame);
	_MWT ael = midorator_webkit_getprop(root, "addEventListener");
	_MWT callback = midorator_webkit_from_callback(root, midorator_hooks_onload_event_cb);
	midorator_webkit_call(ael, midorator_webkit_from_string(root, "load"), callback, midorator_webkit_from_boolean(root, true));

	midorator_hooks_earlyload_cb(root, root, root, root);
}

static void midorator_hooks_focus_cb(WebKitWebView *web_view) {
	WebKitWebFrame *web_frame = webkit_web_view_get_focused_frame(web_view);
	_MWT root = midorator_webkit_getframe(web_view, web_frame);
	midorator_hooks_call(root, "tabfocus");
}

void midorator_hooks_add_view(WebKitWebView *web_view) {
	g_signal_connect (web_view, "window-object-cleared",
		G_CALLBACK (midorator_hooks_window_object_cleared_cb), NULL);
	g_signal_connect (web_view, "grab-focus",
		G_CALLBACK (midorator_hooks_focus_cb), NULL);

	/* // This signal is absent on some versions of WebKitGTK, so we need some workaround instead
	g_signal_connect (web_view, "onload-event",
		G_CALLBACK (midorator_hooks_onload_event_cb), NULL); // */
}


void midorator_hooks_add(const char *name, const char *code) {
	char *nname;
	size_t i;
	for (i = 0; ; i++) {
		nname = g_strdup_printf("%s_%zu", name, i);
		const char *ncode = midorator_options("hook", nname, NULL);
		if (!ncode)
			break;
		g_free(nname);
	}
	midorator_options("hook", nname, code);
	g_free(nname);
}

void midorator_hooks_call(_MWT obj, const char *name) {
	size_t i;
	for (i = 0; ; i++) {
		char *nname = g_strdup_printf("%s_%zu", name, i);
		const char *code = midorator_options("hook", nname, NULL);
		g_free(nname);
		if (!code)
			break;
		midorator_webkit_run_script_args(obj, code);
	}
}




