
#ifndef MIDORATOR_H
#define MIDORATOR_H

#define MIDORATOR_VERSION "0.020101104"

#ifdef DEBUG
#	include <execinfo.h>
#	define static_f
#	define logline (fprintf(stderr, "%s():%i\n", __func__, __LINE__))
#	define logextra(f, ...) (fprintf(stderr, "%s():%i: " f "\n", __func__, __LINE__, __VA_ARGS__))
#else
#	define static_f static
#	define logline
#	define logextra(f, ...)
#endif

#endif

