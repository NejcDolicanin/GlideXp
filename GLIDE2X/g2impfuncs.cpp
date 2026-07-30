//
// Glide 2 to Glide 3 Translator for GlideXP
//
// g2impfuncs.cpp : Imported functions from Glide3
//
// Copyright (c) 2002 Ryan Nunn
//
// Just a note that this is written in C++ so I can use namespaces.
// No other C++ features are used.
//

#include "g2pch.h"

namespace Glide3 {

HMODULE	hDllGlide3 = NULL;

bool	SetupAllFunctions()
{
	// Already got it
	if (hDllGlide3) return true;

	// First try GlideXP.dll
	hDllGlide3 = LoadLibrary(TEXT("GlideXP.dll"));

	// If it's not found, try Glide3x.dll
	if (!hDllGlide3) hDllGlide3 = LoadLibrary(TEXT("glide3x.dll"));

	if (!hDllGlide3) {
		//MessageBox(NULL, TEXT("Unable to load glide3x.dll"), TEXT("Glide2x Error!"), MB_ICONERROR|MB_OK);
		return false;
	}

	bool ret = true;

	// Set up individual functions now
	if (ret) ret = SetupDebugFunctions();		// Must be first!
	if (ret) ret = SetupMiscFunctions();		// Must be second!
	if (ret) ret = SetupGlideFunctions();
	if (ret) ret = SetupBufferFunctions();
	if (ret) ret = SetupSSTFunctions();
	if (ret) ret = SetupTexFunctions();
	if (ret) ret = SetupDrawFunctions();
	if (ret) ret = SetupColorFunctions();
	if (ret) ret = SetupUtilsFunctions();
	if (ret) ret = SetupFogFunctions();

	if (ret) SetupTuples();

	return ret;
}

bool	FreeAllFunctions()
{
	if (!hDllGlide3) return true;

	bool ret = true;

	if (ret) ret = FreeFogFunctions();
	if (ret) ret = FreeUtilsFunctions();
	if (ret) ret = FreeColorFunctions();
	if (ret) ret = FreeDrawFunctions();
	if (ret) ret = FreeTexFunctions();
	if (ret) ret = FreeSSTFunctions();
	if (ret) ret = FreeBufferFunctions();
	if (ret) ret = FreeGlideFunctions();
	if (ret) ret = FreeMiscFunctions();
	if (ret) ret = FreeDebugFunctions();		// Must be last!

	FreeLibrary(hDllGlide3);
	hDllGlide3 = NULL;

	return ret;
}


};