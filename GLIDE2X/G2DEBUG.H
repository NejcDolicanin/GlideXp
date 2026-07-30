//
// Glide 2 to Glide 3 Translator for GlideXP
//
// g2debug.h: Imported Debug Functions
//
// Copyright (c) 2002 Ryan Nunn
//
// Just a note that this is written in C++ so I can use namespaces.
// No other C++ features are used.
//

namespace Glide3 {

	bool	SetupDebugFunctions();
	bool	FreeDebugFunctions();

	//
	// DEBUG FUNCTIONS
	//
	typedef void (FX_CALL * gdbg_error_proc)(const char *name, const char *format, ...);
	typedef void (FX_CALL * gdbg_error_clear_callback_proc)(GDBGErrorProc p);
	typedef int  (FX_CALL * gdbg_error_set_callback_proc)(GDBGErrorProc p);
	typedef int  (FX_CALL * gdbg_get_debuglevel_proc)(const int level);
	typedef int  (FX_CALL * gdbg_get_errors_proc)(void);
	typedef int  (FX_CALL * gdbg_info_proc)(const int level, const char *format, ...);
	typedef int  (FX_CALL * gdbg_info_more_proc)(const int level, const char *format, ...);
	typedef void (FX_CALL * gdbg_init_proc)(void);
	typedef void (FX_CALL * gdbg_parse_proc)(const char *env);
	typedef void (FX_CALL * gdbg_printf_proc)(const char *format, ...);
	typedef void (FX_CALL * gdbg_set_debuglevel_proc)(const int level, const int value);
	typedef int  (FX_CALL * gdbg_set_file_proc)(const char *name);
	typedef void (FX_CALL * gdbg_set_keepalive_proc)(GDBGKeepAliveProc p);
	typedef void (FX_CALL * gdbg_shutdown_proc)(void);
	typedef void (FX_CALL * gdbg_vprintf_proc)(const char *format, va_list);

	extern	gdbg_error_proc					gdbg_error;
	extern	gdbg_error_clear_callback_proc	gdbg_error_clear_callback;
	extern	gdbg_error_set_callback_proc	gdbg_error_set_callback;
	extern	gdbg_get_debuglevel_proc		gdbg_get_debuglevel;
	extern	gdbg_get_errors_proc			gdbg_get_errors;
	extern	gdbg_info_proc					gdbg_info;
	extern	gdbg_info_more_proc				gdbg_info_more;
	extern	gdbg_init_proc					gdbg_init;
	extern	gdbg_parse_proc					gdbg_parse;
	extern	gdbg_printf_proc				gdbg_printf;
	extern	gdbg_set_debuglevel_proc		gdbg_set_debuglevel;
	extern	gdbg_set_file_proc				gdbg_set_file;
	extern	gdbg_set_keepalive_proc			gdbg_set_keepalive;
	extern	gdbg_shutdown_proc				gdbg_shutdown;
	extern	gdbg_vprintf_proc				gdbg_vprintf;
};

// These are not exported, but we might use them here in the dll instead
using Glide3::gdbg_init;
using Glide3::gdbg_parse;
using Glide3::gdbg_shutdown;
using Glide3::gdbg_vprintf;
using Glide3::gdbg_printf;
using Glide3::gdbg_info;
using Glide3::gdbg_info_more;
using Glide3::gdbg_error;
using Glide3::gdbg_get_errors;
using Glide3::gdbg_set_file;
using Glide3::gdbg_get_debuglevel;
using Glide3::gdbg_set_debuglevel;
using Glide3::gdbg_error_set_callback;
using Glide3::gdbg_error_clear_callback;
using Glide3::gdbg_set_keepalive;
