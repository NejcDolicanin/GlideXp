//
// Glide 2 to Glide 3 Translator for GlideXP
//
// g2impfuncs.h : Header for imported functions from Glide3
//
// Copyright (c) 2002 Ryan Nunn
//
// Just a note that this is written in C++ so I can use namespaces.
// No other C++ features are used.
//

// loads a function "name", that has a parameter list of "parmsize" bytes.
#define LoadFunc(name,parmsize)	(NULL != ((name) = (name##_proc) GetProcAddress(hDllGlide3,"_" #name "@" #parmsize)))

namespace Glide3 {

	// Handle to the DLL
	extern	HMODULE hDllGlide3;

	// Setup All the functions
	bool	SetupAllFunctions();
	bool	FreeAllFunctions();

};

