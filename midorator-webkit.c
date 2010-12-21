
#include "midorator.h"
#include "midorator-webkit.h"
#include "midorator-commands.h"
#include <stdlib.h>
#include <string.h>
#include <glib.h>


_MWT midorator_webkit_error(_MWT issuer, const char *fmt, ...) {
	va_list va;
	va_start(va, fmt);
	vsnprintf(issuer.error, sizeof(issuer.error) - 1, fmt, va);
	va_end(va);
	issuer.error[sizeof(issuer.error) - 1] = 0;
	return issuer;
}

_MWT midorator_webkit_jserror(_MWT issuer, JSValueRef err) {
	JSStringRef str = JSValueToStringCopy(issuer.ctx, err, NULL);
	if (!str)
		return midorator_webkit_error(issuer, "Unknown JS error");
	const char *pre = "JS error: ";
	strcpy(issuer.error, pre);
	JSStringGetUTF8CString(str, issuer.error + strlen(pre), sizeof(issuer.error) - strlen(pre));
	issuer.error[sizeof(issuer.error) - 1] = 0;
	JSStringRelease(str);
	return issuer;
}

_MWAT midorator_webkit_aerror(_MWT error) {
	_MWAT ret = { error.web_view };
	memcpy(ret.error, error.error, sizeof(ret.error));
	return ret;
}

#define _TEST(o) midorator_webkit_assert(o, o)
#define _TEST_NZR(o) midorator_webkit_assert_nz(o, o)
#define _TEST_NZ(o, ...) midorator_webkit_assert_nz(o, (o).error[0] ? o : midorator_webkit_error(o, __VA_ARGS__))
#define _TEST_A(o) midorator_webkit_assert_array(o, o)

_MWT midorator_webkit_getframe(WebKitWebView *web_view, WebKitWebFrame *web_frame) {
	_MWT ret = { web_view, web_frame };
	if (!web_frame)
		return midorator_webkit_error(ret, "No frame");
	ret.ctx = webkit_web_frame_get_global_context(web_frame);
	if (!ret.ctx)
		return midorator_webkit_error(ret, "No JS context at frame");
	ret.elem = JSContextGetGlobalObject(ret.ctx);
	if (!ret.elem)
		return midorator_webkit_error(ret, "No window object at frame");
	ret._this = (JSObjectRef)ret.elem;
	return ret;
}

_MWT midorator_webkit_getroot(WebKitWebView *web_view) {
	WebKitWebFrame *web_frame = webkit_web_view_get_main_frame(web_view);
	return midorator_webkit_getframe(web_view, web_frame);
}

_MWT midorator_webkit_from_string(_MWT base, const char *str) {
	JSStringRef sr = JSStringCreateWithUTF8CString(str);
	base.elem = JSValueMakeString(base.ctx, sr);
	JSStringRelease(sr);
	return base;
}

_MWT midorator_webkit_from_number(_MWT base, double number) {
	base.elem = JSValueMakeNumber(base.ctx, number);
	return base;
}

char *midorator_webkit_to_string(_MWT val) {
	JSStringRef s = JSValueToStringCopy(val.ctx, val.elem, NULL);
	if (!s)
		return NULL;
	size_t len = JSStringGetMaximumUTF8CStringSize(s);
	char *ret = g_malloc(len + 1);
	JSStringGetUTF8CString(s, ret, len + 1);
	return ret;
}

double midorator_webkit_to_number(_MWT val) {
	return JSValueToNumber(val.ctx, val.elem, NULL);
}

_MWT midorator_webkit_from_value(_MWT base, JSValueRef val) {
	base.elem = val;
	base._this = JSContextGetGlobalObject(base.ctx);
	return base;
}

_MWT midorator_webkit_setprop(_MWT obj, const char *name, _MWT val) {
	_TEST(obj);
	_TEST(val);
	JSValueRef err = NULL;
	obj._this = JSValueToObject(obj.ctx, obj.elem, &err);
	if (err)
		return midorator_webkit_jserror(obj, err);
	JSStringRef sr = JSStringCreateWithUTF8CString(name);
	JSObjectSetProperty(obj.ctx, obj._this, sr, val.elem, 0, &err);
	JSStringRelease(sr);
	if (err)
		return midorator_webkit_jserror(obj, err);
	return val;
}

_MWT midorator_webkit_getprop(_MWT val, const char *name) {
	_TEST_NZ(val, "Getting property '%s' of null object", name);
	JSValueRef err = NULL;
	val._this = JSValueToObject(val.ctx, val.elem, &err);
	if (err)
		return midorator_webkit_jserror(val, err);
	if (!val._this)
		return midorator_webkit_error(val, "Value is not an object");
	JSStringRef sr = JSStringCreateWithUTF8CString(name);
	val.elem = JSObjectGetProperty(val.ctx, val._this, sr, &err);
	JSStringRelease(sr);
	if (err)
		return midorator_webkit_jserror(val, err);
	return val;
}

_MWT midorator_webkit_getarray(_MWT val, size_t index) {
	char buf[128];
	sprintf(buf, "%zu", index);
	return midorator_webkit_getprop(val, buf);
}

_MWAT midorator_webkit_unpack(_MWT arr) {
	midorator_webkit_assert(arr, midorator_webkit_aerror(arr));
	midorator_webkit_assert_nz(arr, midorator_webkit_aerror(midorator_webkit_error(arr, "Can't unpack object: not an array")));
	_MWT len_j = midorator_webkit_getprop(arr, "length");
	midorator_webkit_assert(len_j, midorator_webkit_aerror(len_j));
	midorator_webkit_assert_nz(len_j, midorator_webkit_aerror(midorator_webkit_error(len_j, "Can't unpack object: not an array")));
	_MWAT ret = { arr.web_view };
	ret.len = (size_t) midorator_webkit_to_number(len_j);
	ret.arr = g_new(_MWT, ret.len);
	size_t i;
	for (i = 0; i < ret.len; i++) {
		ret.arr[i] = midorator_webkit_getarray(arr, i);
		JSValueProtect(ret.arr[i].ctx, ret.arr[i].elem);
	}
	return ret;
}

void midorator_webkit_array_free(_MWAT arr) {
	size_t i;
	for (i = 0; i < arr.len; i++)
		JSValueUnprotect(arr.arr[i].ctx, arr.arr[i].elem);
	g_free(arr.arr);
}

_MWAT midorator_webkit_array_merge(_MWAT first, _MWAT second, bool free) {
	midorator_webkit_assert(first, first);
	midorator_webkit_assert(second, second);
	_MWAT ret = { first.web_view };
	ret.len = first.len + second.len;
	ret.arr = g_new(_MWT, ret.len);
	size_t i;
	for (i = 0; i < first.len; i++)
		ret.arr[i] = first.arr[i];
	for (i = 0; i < second.len; i++)
		ret.arr[i + first.len] = second.arr[i];
	for (i = 0; i < ret.len; i++)
		JSValueProtect(ret.arr[i].ctx, ret.arr[i].elem);
	if (free) {
		midorator_webkit_array_free(first);
		midorator_webkit_array_free(second);
	}
	return ret;
}

_MWT midorator_webkit_vcall(_MWT obj, int argc, const _MWT *argv) {
	_TEST(obj);
	if (!midorator_webkit_isok_nz(obj))
		return midorator_webkit_error(obj, "calling empty value");
	int i;
	for (i = 0; i < argc; i++)
		_TEST(argv[i]);
	JSValueRef *argv_j = g_new(JSValueRef, argc);
	for (i = 0; i < argc; i++)
		argv_j[i] = argv[i].elem;
	JSValueRef err = NULL;
	JSValueRef ret = JSObjectCallAsFunction(obj.ctx, JSValueToObject(obj.ctx, obj.elem, NULL), obj._this, argc, argv_j, &err);
	if (err)
		return midorator_webkit_jserror(obj, err);
	return midorator_webkit_from_value(obj, ret);
}

_MWT _midorator_webkit_call(_MWT obj, ...) {
	_TEST(obj);
	va_list va;
	va_start(va, obj);
	int argc;
	for (argc = 0; va_arg(va, _MWT).ctx; argc++);
	va_end(va);
	_MWT *argv = g_new0(_MWT, argc + 1);
	va_start(va, obj);
	int i;
	for (i = 0; i < argc; i++)
		argv[i] = va_arg(va, _MWT);
	va_end(va);
	_MWT ret = midorator_webkit_vcall(obj, argc, argv);
	g_free(argv);
	return ret;
}

_MWAT midorator_webkit_items(_MWT obj, const char *selector, bool subframes) {
	midorator_webkit_assert(obj, midorator_webkit_aerror(obj));
	_MWT qsa = midorator_webkit_getprop(obj, "querySelectorAll");
	if (!midorator_webkit_isok_nz(qsa)) {
		_MWT doc = midorator_webkit_getprop(obj, "document");
		midorator_webkit_assert_nz(doc, midorator_webkit_aerror(midorator_webkit_error(obj, "Object doesn't support selectors")));
		qsa = midorator_webkit_getprop(doc, "querySelectorAll");
		midorator_webkit_assert_nz(qsa, midorator_webkit_aerror(midorator_webkit_error(obj, "Object doesn't support selectors")));
	}
	_MWAT ret = midorator_webkit_unpack(midorator_webkit_call(qsa, midorator_webkit_from_string(obj, selector)));
	if (subframes) {
		_MWAT frames = midorator_webkit_get_subframes(obj, true);
		_TEST(frames);
		size_t i;
		for (i = 0; i < frames.len; i++)
			if (frames.arr[i].web_frame != obj.web_frame) {
				_MWAT new = midorator_webkit_items(frames.arr[i], selector, false);
				ret = midorator_webkit_array_merge(ret, new, true);
			}
		midorator_webkit_array_free(frames);
	}
	return ret;
}












static _MWT midorator_webkit_cmd_to_frame(JSContextRef ctx, JSObjectRef func) {
	_MWT tmp = {};
	tmp.ctx = ctx;
	tmp.elem = func;
	tmp = midorator_webkit_getprop(tmp, "web_frame");
	char *str = midorator_webkit_to_string(tmp);
	sscanf(str, "%p", &tmp.web_frame);
	g_free(str);
	tmp.web_view = webkit_web_frame_get_web_view(tmp.web_frame);
	return midorator_webkit_getframe(tmp.web_view, tmp.web_frame);
}

static JSValueRef midorator_webkit_command_cb(JSContextRef ctx, JSObjectRef func, JSObjectRef _this, size_t argc, const JSValueRef arguments[], JSValueRef* ex) {
	_MWT _frame = midorator_webkit_cmd_to_frame(ctx, func);
	char **argv = g_new0(char*, argc + 1);
	size_t i;
	for (i = 0; i < argc; i++)
		argv[i] = midorator_webkit_to_string(midorator_webkit_from_value(_frame, arguments[i]));
	if (argv[0] && strcmp(argv[0], "get") == 0) {
		char *result = midorator_process_request(GTK_WIDGET(_frame.web_view), argv + 1, argc - 1);
		g_strfreev(argv);
		_MWT ret = midorator_webkit_from_string(_frame, result);
		g_free(result);
		return ret.elem;
	} else {
		midorator_process_command_v(GTK_WIDGET(_frame.web_view), argv, argc);
		g_strfreev(argv);
		return NULL;
	}
}

static _MWT midorator_webkit_make_callback(_MWT _frame, JSObjectCallAsFunctionCallback cb) {
	JSObjectRef func = JSObjectMakeFunctionWithCallback(_frame.ctx, NULL, cb);
	_MWT ret = midorator_webkit_from_value(_frame, func);
	char buf[256];
	sprintf(buf, "%p", _frame.web_frame);
	midorator_webkit_setprop(ret, "web_frame", midorator_webkit_from_string(_frame, buf));
	return ret;
}

static _MWT midorator_webkit_make_script(_MWT _frame, const char *script, const char *argnames[]) {
	JSStringRef script_j = JSStringCreateWithUTF8CString(script);
	int argc;
	for (argc = 0; argnames[argc]; argc++);
	JSStringRef *names_j = g_new0(JSStringRef, argc);
	int i;
	for (i = 0; i < argc; i++)
		names_j[i] = JSStringCreateWithUTF8CString(argnames[i]);
	JSValueRef err = NULL;
	JSObjectRef ret = JSObjectMakeFunction(_frame.ctx, NULL, argc, names_j, script_j, NULL, 0, &err);
	JSStringRelease(script_j);
	for (i = 0; i < argc; i++)
		JSStringRelease(names_j[i]);
	g_free(names_j);
	if (err)
		return midorator_webkit_jserror(_frame, err);
	return midorator_webkit_from_value(_frame, ret);
}

void midorator_webkit_run_script_argv(_MWT obj, const char *script, const char *argv[]) {
	const char *tmp = NULL;
	if (!argv)
		argv = &tmp;
	_MWT win = midorator_webkit_getframe(obj.web_view, obj.web_frame);
	if (win.error[0]) {
		midorator_error(GTK_WIDGET(win.web_view), "%s", win.error);
		return;
	}
	_MWT arr_c = midorator_webkit_getprop(win, "Array");
	if (arr_c.error[0]) {
		midorator_error(GTK_WIDGET(arr_c.web_view), "%s", arr_c.error);
		return;
	}
	int argc;
	for (argc = 0; argv[argc]; argc++);
	_MWT *mwt_argv = g_new0(_MWT, argc);
	int i;
	for (i = 0; i < argc; i++)
		mwt_argv[i] = midorator_webkit_from_string(obj, argv[i]);
	_MWT arr = midorator_webkit_vcall(arr_c, argc, mwt_argv);
	g_free(mwt_argv);
	_MWT cb = midorator_webkit_make_callback(win, midorator_webkit_command_cb);
	const char *argnames[] = { "args", "command", "object", NULL };
	_MWT fn = midorator_webkit_make_script(win, script, argnames);
	_MWT callargs[] = { arr, cb, obj };
	_MWT ret = midorator_webkit_vcall(fn, 3, callargs);
	if (ret.error[0])
		midorator_error(GTK_WIDGET(ret.web_view), "%s", ret.error);
}

void _midorator_webkit_run_script_args(_MWT obj, const char *script, ...) {
	va_list va;
	va_start(va, script);
	int i;
	for (i = 0; va_arg(va, const char*); i++);
	va_end(va);
	const char **args = g_new0(const char*, i + 1);
	va_start(va, script);
	for (i = 0; (args[i] = va_arg(va, const char*)); i++);
	va_end(va);
	midorator_webkit_run_script_argv(obj, script, args);
	g_free(args);
}

void midorator_webkit_run_script_printf(_MWT obj, const char *fmt, ...) {
	va_list va;
	va_start(va, fmt);
	char *script = g_strdup_vprintf(fmt, va);
	va_end(va);
	midorator_webkit_run_script_argv(obj, script, NULL);
	g_free(script);
}









void midorator_webkit_go(_MWT root, const char *direction) {
	char *selector = g_strdup_printf("link[href][rel=\"%1$s\"], a[href][rel=\"%1$s\"]", direction);
	_MWAT links = midorator_webkit_items(root, selector, true);
	g_free(selector);

	if (links.len) {
		char *href = midorator_webkit_to_string(midorator_webkit_getprop(links.arr[0], "href"));
		midorator_webkit_array_free(links);
		midorator_process_command(GTK_WIDGET(root.web_view), NULL, "open", href);
		g_free(href);
		return;
	}

	midorator_webkit_array_free(links);

	char *o = g_strconcat("go_", direction, NULL);
	direction = midorator_options("option", o, NULL);
	g_free(o);
	if (!direction)
		return;

	GError *e = NULL;
	GRegex *re = g_regex_new(direction, G_REGEX_CASELESS | G_REGEX_DOTALL | G_REGEX_EXTENDED | G_REGEX_OPTIMIZE, G_REGEX_MATCH_NOTEMPTY, &e);
	if (e) {
		midorator_error(GTK_WIDGET(root.web_view), "Regular expression in config file: %s", e->message);
		return;
	}

	links = midorator_webkit_items(root, "a[href]", true);

	size_t i;
	for (i = 0; i < links.len; i++) {
		char *text = midorator_webkit_to_string(midorator_webkit_getprop(links.arr[i], "innerText"));
		if (g_regex_match(re, text, 0, NULL)) {
			g_free(text);
			g_regex_unref(re);
			char *href = midorator_webkit_to_string(midorator_webkit_getprop(links.arr[i], "href"));
			midorator_webkit_array_free(links);
			midorator_process_command(GTK_WIDGET(root.web_view), NULL, "open", href);
			g_free(href);
			return;
		}
		g_free(text);
	}

	midorator_webkit_array_free(links);
	g_regex_unref(re);
}







_MWT midorator_webkit_overlap_rects(_MWT rect1, _MWT rect2) {
	_TEST_NZR(rect1);
	_TEST_NZR(rect2);

	double
		l1 = midorator_webkit_to_number(midorator_webkit_getprop(rect1, "left")),
		l2 = midorator_webkit_to_number(midorator_webkit_getprop(rect2, "left")),
		r1 = midorator_webkit_to_number(midorator_webkit_getprop(rect1, "right")),
		r2 = midorator_webkit_to_number(midorator_webkit_getprop(rect2, "right")),
		t1 = midorator_webkit_to_number(midorator_webkit_getprop(rect1, "top")),
		t2 = midorator_webkit_to_number(midorator_webkit_getprop(rect2, "top")),
		b1 = midorator_webkit_to_number(midorator_webkit_getprop(rect1, "bottom")),
		b2 = midorator_webkit_to_number(midorator_webkit_getprop(rect2, "bottom")),
		w1 = midorator_webkit_to_number(midorator_webkit_getprop(rect1, "width")),
		w2 = midorator_webkit_to_number(midorator_webkit_getprop(rect2, "width")),
		h1 = midorator_webkit_to_number(midorator_webkit_getprop(rect1, "height")),
		h2 = midorator_webkit_to_number(midorator_webkit_getprop(rect2, "height"));

	double
		l = l1 > l2 ? l1 : l2,
		r = r1 < r2 ? r1 : r2,
		t = t1 > t2 ? t1 : t2,
		b = b1 < b2 ? b1 : b2;

	if (l > r || t > b || w1 == 0 || h1 == 0 || w2 == 0 || h2 == 0)
		return midorator_webkit_from_value(rect1, NULL);

	_MWT ret = midorator_webkit_from_value(rect1, JSObjectMake(rect1.ctx, NULL, NULL));

	midorator_webkit_setprop(ret, "left",	midorator_webkit_from_number(ret, l));
	midorator_webkit_setprop(ret, "right",	midorator_webkit_from_number(ret, r));
	midorator_webkit_setprop(ret, "top",	midorator_webkit_from_number(ret, t));
	midorator_webkit_setprop(ret, "bottom",	midorator_webkit_from_number(ret, b));
	midorator_webkit_setprop(ret, "width",	midorator_webkit_from_number(ret, r - l));
	midorator_webkit_setprop(ret, "height",	midorator_webkit_from_number(ret, b - t));

	return ret;
}

_MWT midorator_webkit_visible_rect(_MWT rect, _MWT item) {
	_TEST(item);
	_TEST_NZR(rect);
	bool go_on = true;
	_MWT win = midorator_webkit_getframe(item.web_view, item.web_frame);

	double
		iw = midorator_webkit_to_number(midorator_webkit_getprop(win, "innerWidth")),
		ih = midorator_webkit_to_number(midorator_webkit_getprop(win, "innerHeight"));
	_MWT scrollrect = midorator_webkit_from_value(item, JSObjectMake(item.ctx, NULL, NULL));
	midorator_webkit_setprop(scrollrect, "left",   midorator_webkit_from_number(scrollrect, 0));
	midorator_webkit_setprop(scrollrect, "top",    midorator_webkit_from_number(scrollrect, 0));
	midorator_webkit_setprop(scrollrect, "right",  midorator_webkit_from_number(scrollrect, iw));
	midorator_webkit_setprop(scrollrect, "width",  midorator_webkit_from_number(scrollrect, iw));
	midorator_webkit_setprop(scrollrect, "bottom", midorator_webkit_from_number(scrollrect, ih));
	midorator_webkit_setprop(scrollrect, "height", midorator_webkit_from_number(scrollrect, ih));
	rect = midorator_webkit_overlap_rects(rect, scrollrect);
	_TEST_NZR(rect);

	_MWT gcs = midorator_webkit_getprop(win, "getComputedStyle");
	for (; go_on && midorator_webkit_isok_nz(item); item = midorator_webkit_getprop(item, "parentElement")) {
		_MWT itemstyle = midorator_webkit_call(gcs, item);
		if (!midorator_webkit_isok_nz(itemstyle))
			itemstyle = midorator_webkit_getprop(item, "style");

		char *display = midorator_webkit_to_string(midorator_webkit_getprop(itemstyle, "display"));
		bool hidden = (display == NULL) || (strcmp(display, "none") == 0);
		g_free(display);

		char *visibility = midorator_webkit_to_string(midorator_webkit_getprop(itemstyle, "visibility"));
		hidden = hidden || (visibility == NULL) || (strcmp(visibility, "hidden") == 0);
		g_free(visibility);

		if (hidden)
			return midorator_webkit_from_value(rect, NULL);

		// FIXME: overflowX and overflowY may differ
		char *overflow = midorator_webkit_to_string(midorator_webkit_getprop(itemstyle, "overflow"));
		bool need_check_overlap = (overflow == NULL) || (strcmp(overflow, "visible") != 0);
		g_free(overflow);

		/*char *tagName = midorator_webkit_to_string(midorator_webkit_getprop(item, "tagName"));
		need_check_overlap = need_check_overlap && (g_ascii_strcasecmp(tagName, "details"));
		g_free(tagName);*/

		char *position = midorator_webkit_to_string(midorator_webkit_getprop(itemstyle, "position"));
		go_on = (position != NULL) && (strcmp(position, "absolute") != 0) && (strcmp(position, "fixed") != 0);
		g_free(position);

		if (need_check_overlap) {
			_MWT gbcr = midorator_webkit_getprop(item, "getBoundingClientRect");
			_MWT itemrect = midorator_webkit_call(gbcr);
			_TEST(itemrect);
			if (!midorator_webkit_isok_nz(itemrect))
				continue;
			rect = midorator_webkit_overlap_rects(rect, itemrect);
			_TEST_NZR(rect);
		}
	}
	return rect;
}

void midorator_webkit_unhint(_MWT root) {
	_MWAT items = midorator_webkit_items(root, "div.midorator_hint", true);
	size_t i;
	for (i = 0; i < items.len; i++) {
		_MWT item = items.arr[i];
		_MWT parent = midorator_webkit_getprop(item, "parentNode");
		_MWT rc = midorator_webkit_getprop(parent, "removeChild");
		midorator_webkit_call(rc, item);
	}
	midorator_webkit_array_free(items);
}

void midorator_webkit_hints(_MWT root, const char *selector, const char *hintchars, const char *entered, const char *script) {
	midorator_webkit_unhint(root);
	if (!selector)
		return;
	if (!entered)
		entered = "";
	int enlen = strlen(entered);
	int base = hintchars ? strlen(hintchars) : 0;
	if (base < 2) {
		base = 10;
		hintchars = "0123456789";
	}
	if (strspn(entered, hintchars) != strlen(entered)) {
		midorator_error(GTK_WIDGET(root.web_view), "Entered invalid hint character");
		return;
	}

	_MWAT items = midorator_webkit_items(root, selector, true);
	if (items.error[0]) {
		midorator_error(GTK_WIDGET(items.web_view), "%s", items.error);
		return;
	}
	_MWAT approved = items;
	approved.arr = g_new0(_MWT, items.len);
	approved.len = 0;
	size_t i;
	for (i = 0; i < items.len; i++) {
		_MWT item = items.arr[i];
		_MWT gcr = midorator_webkit_getprop(item, "getClientRects");
		_MWT rects = midorator_webkit_call(gcr);
		if (rects.error[0]) {
			midorator_error(GTK_WIDGET(rects.web_view), "%s", rects.error);
			continue;
		}
		size_t rectcount = (size_t)midorator_webkit_to_number(midorator_webkit_getprop(rects, "length"));
		size_t j;
		for (j = 0; j < rectcount; j++) {
			_MWT rect = midorator_webkit_getarray(rects, j);
			rect = midorator_webkit_visible_rect(rect, item);
			if (midorator_webkit_isok_nz(rect)) {
				midorator_webkit_setprop(rect, "item", item);
				approved.arr[approved.len++] = rect;
				JSValueProtect(rect.ctx, rect.elem);
				break;
			}
		}
	}

	midorator_webkit_array_free(items);

	if (!approved.len) {
		midorator_error(GTK_WIDGET(approved.web_view), "No selectable elements found");
		midorator_webkit_array_free(approved);
		return;
	}

	int hlen, max;
	for (hlen = 0, max = 1; max < approved.len; hlen++, max *= base);
	if (hlen == 0)
		hlen = 1;

	for (i = 0; i < approved.len; i++) {
		char hint[128];
		int val = i;
		int j;
		for (j = hlen - 1; j >= 0; j--) {
			int mod = val % base;
			hint[j] = hintchars[mod];
			val = (val - mod) / base;
			if (j < enlen && entered[j] != hint[j])
				break;
		}
		if (j >= 0)
			continue;

		_MWT rect = approved.arr[i];
		_MWT item = midorator_webkit_getprop(rect, "item");
		if (hlen == enlen) {
			midorator_entry(GTK_WIDGET(item.web_view), NULL);
			if (script)
				midorator_webkit_run_script_args(item, script);
			break;
		} else {
			_MWT text = midorator_webkit_from_string(rect, hint + enlen);
			_MWT doc = midorator_webkit_getprop(item, "ownerDocument");
			_MWT ce = midorator_webkit_getprop(doc, "createElement");
			_MWT hint = midorator_webkit_call(ce, midorator_webkit_from_string(ce, "div"));
			_MWT style = midorator_webkit_getprop(hint, "style");
			if (!midorator_webkit_isok_nz(style)) {
				if (style.error[0])
					midorator_error(GTK_WIDGET(approved.web_view), style.error);
				break;
			}

			_MWT body = midorator_webkit_getprop(doc, "body");
			_MWT ac = midorator_webkit_getprop(body, "appendChild");
			if (!midorator_webkit_isok_nz(ac)) {
				if (ac.error[0])
					midorator_error(GTK_WIDGET(approved.web_view), ac.error);
				break;
			}

			midorator_webkit_setprop(hint, "item", item);
			midorator_webkit_setprop(hint, "innerText", text);
			midorator_webkit_setprop(hint, "className", midorator_webkit_from_string(hint, "midorator_hint"));
			midorator_webkit_setprop(style, "position", midorator_webkit_from_string(style, "fixed"));
			midorator_webkit_setprop(style, "display", midorator_webkit_from_string(style, "inline-block"));
			char buf[128];
			sprintf(buf, "%ipx", (int)midorator_webkit_to_number(midorator_webkit_getprop(rect, "left")));
			midorator_webkit_setprop(style, "left", midorator_webkit_from_string(style, buf));
			sprintf(buf, "%ipx", (int)midorator_webkit_to_number(midorator_webkit_getprop(rect, "top")));
			midorator_webkit_setprop(style, "top",  midorator_webkit_from_string(style, buf));
			midorator_webkit_call(ac, hint);
		}
	}

	midorator_webkit_array_free(approved);
}








// Hack to maintain list of frames for each view. Please let me know when that
// list will become accessible via webkitgtk-api and this hack will become
// unnessessary.
// {{{
typedef struct midorator_webkit_framelist {
	size_t alloc;
	WebKitWebFrame **list;
} midorator_webkit_framelist;

static gboolean midorator_webkit_add_frame(WebKitWebView *web_view, WebKitWebFrame *web_frame) {
	if (!web_view || !web_frame)
		return false;

	midorator_webkit_framelist *list = g_object_get_data(G_OBJECT(web_view), "midorator_webkit_framelist");
	if (!list) {
		size_t start_count = 8;
		list = g_malloc0(sizeof(midorator_webkit_framelist) + sizeof(WebKitWebFrame*) * start_count);
		list->alloc = start_count;
		list->list = (WebKitWebFrame **)(list + 1);
		g_object_set_data(G_OBJECT(web_view), "midorator_webkit_framelist", list);
	}

	size_t i;
	for (i = 0; i < list->alloc; i++)
		if (list->list[i] == web_frame)
			return false;

	for (i = 0; i < list->alloc && list->list[i]; i++);
	if (i == list->alloc) {
		// Grow
		size_t new_size = list->alloc << 1;
		midorator_webkit_framelist *newlist = g_malloc0(sizeof(midorator_webkit_framelist) + sizeof(WebKitWebFrame*) * new_size);
		newlist->list = (WebKitWebFrame **)(newlist + 1);
		size_t j;
		for (j = 0; j < i; j++) {
			newlist->list[j] = list->list[j];
			g_object_add_weak_pointer(G_OBJECT(list->list[j]), (void**)&newlist->list[j]);
			g_object_remove_weak_pointer(G_OBJECT(list->list[j]), (void**)&list->list[j]);
		}
		g_object_set_data(G_OBJECT(web_view), "midorator_webkit_framelist", newlist);
		g_free(list);
		list = newlist;
	}

	list->list[i] = web_frame;
	g_object_add_weak_pointer(G_OBJECT(web_frame), (void**)&list->list[i]);
	return false;
}

void midorator_webkit_add_view(WebKitWebView *web_view) {
	midorator_webkit_add_frame(web_view, webkit_web_view_get_main_frame(web_view));
	g_signal_connect (web_view, "navigation-policy-decision-requested",
		G_CALLBACK (midorator_webkit_add_frame), NULL);
}

void midorator_webkit_remove_view(WebKitWebView *web_view) {
	g_signal_handlers_disconnect_by_func (
		web_view, midorator_webkit_add_frame, NULL);
	midorator_webkit_framelist *list = g_object_get_data(G_OBJECT(web_view), "midorator_webkit_framelist");
	if (list) {
		g_free(list);
		g_object_set_data(G_OBJECT(web_view), "midorator_webkit_framelist", NULL);
	}
}

_MWAT midorator_webkit_get_subframes(_MWT _frame, bool tree) {
	_MWAT ret = { _frame.web_view };
	midorator_webkit_assert_nz(_frame, ret);
	midorator_webkit_framelist *list = g_object_get_data(G_OBJECT(_frame.web_view), "midorator_webkit_framelist");
	if (!list)
		return ret;
	ret.len = 0;
	ret.arr = g_new0(_MWT, list->alloc);
	int i;
	for (i = 0; i < list->alloc; i++)
		if (list->list[i]) {
			if (tree) {
				WebKitWebFrame *f;
				for (f = list->list[i]; f; f = webkit_web_frame_get_parent(f))
					if (f == _frame.web_frame) {
						ret.arr[ret.len++] = midorator_webkit_getframe(_frame.web_view, list->list[i]);
						JSValueProtect(ret.arr[ret.len-1].ctx, ret.arr[ret.len-1].elem);
						break;
					}
			} else {
				if (webkit_web_frame_get_parent(list->list[i]) == _frame.web_frame) {
					ret.arr[ret.len++] = midorator_webkit_getframe(_frame.web_view, list->list[i]);
					JSValueProtect(ret.arr[ret.len-1].ctx, ret.arr[ret.len-1].elem);
				}
			}
		}
	return ret;
}
// }}}

