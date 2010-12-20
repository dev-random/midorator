
#ifndef __MIDORATOR_WEBKIT_H
#define __MIDORATOR_WEBKIT_H

#include <JavaScriptCore/JavaScript.h>
#include <webkit/webkit.h>

typedef struct midorator_webkit_t {
	WebKitWebView *web_view;
	WebKitWebFrame *web_frame;
	JSContextRef ctx;
	JSValueRef elem;
	JSObjectRef _this;
	char error[128];
} midorator_webkit_t, _MWT;

typedef struct midorator_webkit_array_t {
	WebKitWebView *web_view;
	size_t len;
	_MWT *arr;
	char error[128];
} midorator_webkit_array_t, _MWAT;


_MWT midorator_webkit_error(_MWT issuer, const char *fmt, ...);
_MWT midorator_webkit_jserror(_MWT issuer, JSValueRef err);
_MWT midorator_webkit_getframe(WebKitWebView *web_view, WebKitWebFrame *web_frame);
_MWT midorator_webkit_getroot(WebKitWebView *web_view);
_MWT midorator_webkit_from_string(_MWT base, const char *str);
_MWT midorator_webkit_from_number(_MWT base, double number);
char *midorator_webkit_to_string(_MWT val);
double midorator_webkit_to_number(_MWT val);
_MWT midorator_webkit_from_value(_MWT base, JSValueRef val);
_MWT midorator_webkit_getprop(_MWT val, const char *name);
_MWT midorator_webkit_getarray(_MWT val, size_t index);
_MWAT midorator_webkit_unpack(_MWT arr);
void midorator_webkit_array_free(_MWAT arr);
_MWAT midorator_webkit_array_merge(_MWAT first, _MWAT second, bool free);
_MWT midorator_webkit_vcall(_MWT obj, int argc, const _MWT *argv);
_MWT _midorator_webkit_call(_MWT obj, ...);
_MWAT midorator_webkit_items(_MWT obj, const char *selector, bool subframes);
_MWAT midorator_webkit_get_subframes(_MWT _frame, bool tree);

void midorator_webkit_add_view(WebKitWebView *web_view);
void midorator_webkit_remove_view(WebKitWebView *web_view);

void midorator_webkit_run_script_argv(_MWT obj, const char *script, const char *argv[]);
void _midorator_webkit_run_script_args(_MWT obj, const char *script, ...);
void midorator_webkit_run_script_printf(_MWT obj, const char *fmt, ...);

void midorator_webkit_hints(_MWT root, const char *selector, const char *hintchars, const char *entered, const char *script);

static _MWT _midorator_webkit_null = {};
static _MWAT _midorator_webkit_empty = {};

#define midorator_webkit_call(...) (_midorator_webkit_call(__VA_ARGS__, _midorator_webkit_null))
#define midorator_webkit_run_script_args(...) (_midorator_webkit_run_script_args(__VA_ARGS__, NULL))

#define midorator_webkit_isok_nz(test) (!(test).error[0] && (test).elem && !JSValueIsUndefined((test).ctx, (test).elem) && !JSValueIsNull((test).ctx, (test).elem))
#define midorator_webkit_assert(test, ...) { if ((test).error[0]) return __VA_ARGS__; }
#define midorator_webkit_assert_nz(test, ...) { if (!midorator_webkit_isok_nz(test)) return __VA_ARGS__; }
#define midorator_webkit_assert_array(test, ...) { if ((test).error[0] || !(test).len) return __VA_ARGS__; }


#endif

