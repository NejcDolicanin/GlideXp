//
// Glide 2 to Glide 3 Translator for GlideXP
//
// g2debug.cpp : Imported Debug Functions
//
// Copyright (c) 2002 Ryan Nunn
//
// Just a note that this is written in C++ so I can use namespaces.
// No other C++ features are used.
//

#include "g2pch.h"

namespace Glide3 {

// Dummys so we can to link to a dll that doesn't export these (but all the dll's should)
static void FX_CALL _gdbg_error (const char *name, const char *format, ...) { }
static void FX_CALL _gdbg_error_clear_callback (GDBGErrorProc p) { }
static int  FX_CALL _gdbg_error_set_callback (GDBGErrorProc p) { return 0; }
static int  FX_CALL _gdbg_get_debuglevel (const int level) { return 0; }
static int  FX_CALL _gdbg_get_errors (void) { return 0; }
static int  FX_CALL _gdbg_info (const int level, const char *format, ...) { return 0; }
static int  FX_CALL _gdbg_info_more (const int level, const char *format, ...) { return 0; }
static void FX_CALL _gdbg_init (void) { }
static void FX_CALL _gdbg_parse (const char *env) { }
static void FX_CALL _gdbg_printf (const char *format, ...) { }
static void FX_CALL _gdbg_set_debuglevel (const int level, const int value) { }
static int  FX_CALL _gdbg_set_file (const char *name) { return 0; }
static void FX_CALL _gdbg_set_keepalive (GDBGKeepAliveProc p) { }
static void FX_CALL _gdbg_shutdown (void) { }
static void FX_CALL _gdbg_vprintf (const char *format, va_list) { }

gdbg_error_proc					gdbg_error = _gdbg_error;
gdbg_error_clear_callback_proc	gdbg_error_clear_callback = _gdbg_error_clear_callback;
gdbg_error_set_callback_proc	gdbg_error_set_callback = _gdbg_error_set_callback;
gdbg_get_debuglevel_proc		gdbg_get_debuglevel = _gdbg_get_debuglevel;
gdbg_get_errors_proc			gdbg_get_errors = _gdbg_get_errors;
gdbg_info_proc					gdbg_info = _gdbg_info;
gdbg_info_more_proc				gdbg_info_more = _gdbg_info_more;
gdbg_init_proc					gdbg_init = _gdbg_init;
gdbg_parse_proc					gdbg_parse = _gdbg_parse;
gdbg_printf_proc				gdbg_printf = _gdbg_printf;
gdbg_set_debuglevel_proc		gdbg_set_debuglevel = _gdbg_set_debuglevel;
gdbg_set_file_proc				gdbg_set_file = _gdbg_set_file;
gdbg_set_keepalive_proc			gdbg_set_keepalive = _gdbg_set_keepalive;
gdbg_shutdown_proc				gdbg_shutdown = _gdbg_shutdown;
gdbg_vprintf_proc				gdbg_vprintf = _gdbg_vprintf;

#define LoadGDBGFunc(name,parmsize) { if (!((name) = (name##_proc) GetProcAddress(hDllGlide3,"_" #name "@" #parmsize))) name = _##name; }
#define LoadGDBGFunc2(name)	         { if (!((name) = (name##_proc) GetProcAddress(hDllGlide3,#name))) name = _##name; }

bool SetupDebugFunctions()
{
	LoadGDBGFunc2(gdbg_error);
	LoadGDBGFunc(gdbg_error_clear_callback, 4);
	LoadGDBGFunc(gdbg_error_set_callback, 4);
	LoadGDBGFunc(gdbg_get_debuglevel, 4);
	LoadGDBGFunc(gdbg_get_errors, 0);
	LoadGDBGFunc2(gdbg_info);
	LoadGDBGFunc2(gdbg_info_more);
	LoadGDBGFunc(gdbg_init, 0);
	LoadGDBGFunc(gdbg_parse, 4);
	LoadGDBGFunc2(gdbg_printf);
	LoadGDBGFunc(gdbg_set_debuglevel, 8);
	LoadGDBGFunc(gdbg_set_file, 4);
	LoadGDBGFunc(gdbg_set_keepalive, 4);
	LoadGDBGFunc(gdbg_shutdown, 0);
	LoadGDBGFunc(gdbg_vprintf, 8);

	// Do it here cause we want it
	GDBG_INIT();

	return true;
}


bool FreeDebugFunctions()
{
	gdbg_error = _gdbg_error;
	gdbg_error_clear_callback = _gdbg_error_clear_callback;
	gdbg_error_set_callback = _gdbg_error_set_callback;
	gdbg_get_debuglevel = _gdbg_get_debuglevel;
	gdbg_get_errors = _gdbg_get_errors;
	gdbg_info = _gdbg_info;
	gdbg_info_more = _gdbg_info_more;
	gdbg_init = _gdbg_init;
	gdbg_parse = _gdbg_parse;
	gdbg_printf = _gdbg_printf;
	gdbg_set_debuglevel = _gdbg_set_debuglevel;
	gdbg_set_file = _gdbg_set_file;
	gdbg_set_keepalive = _gdbg_set_keepalive;
	gdbg_shutdown = _gdbg_shutdown;
	gdbg_vprintf = _gdbg_vprintf;

	return true;
}

};