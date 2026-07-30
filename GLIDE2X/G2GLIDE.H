//
// Glide 2 to Glide 3 Translator for GlideXP
//
// g2glide.h : Glide 2 Initializtion Functions
//
// Copyright (c) 2002 Ryan Nunn
//
// Just a note that this is written in C++ so I can use namespaces.
// No other C++ features are used.
//

namespace Glide3 {

	bool	SetupGlideFunctions();
	bool	FreeGlideFunctions();

	typedef void (FX_CALL * grGlideGetState_proc) ( void *state );
	typedef void (FX_CALL * grGlideInit_proc) ( void );
	typedef void (FX_CALL * grGlideShutdown_proc) ( void );
	typedef void (FX_CALL * grGlideSetState_proc) ( const void *state );

	extern	grGlideGetState_proc			grGlideGetState;
	extern	grGlideInit_proc				grGlideInit;
	extern	grGlideShutdown_proc			grGlideShutdown;
	extern	grGlideSetState_proc			grGlideSetState;
}
