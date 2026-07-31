//
// Glide 2 to Glide 3 Translator for GlideXP
//
// g2main.cpp : DLL Entry point and function pointer setup
//
// Copyright (c) 2002 Ryan Nunn
//
// Just a note that this is written in C++ so I can use namespaces.
// No other C++ features are used.
//

#include "g2pch.h"
#include "gamefix.h"

BOOL WINAPI
DllMain(HANDLE hInst, ULONG  ul_reason_for_call, LPVOID lpReserved)
{
  switch( ul_reason_for_call ) {
  case DLL_PROCESS_DETACH:
//    GDBG_INFO(80, "DllMain: DLL_PROCESS_DETACH\n");
    //g2FreeStateBuffers();
//    grGlideShutdown();
	Glide3::FreeAllFunctions();
    break;
  case DLL_PROCESS_ATTACH:
	theState.Reset();
	Glide3::SetupAllFunctions();
	//if (!Glide3::SetupAllFunctions()) return FALSE;

	// Per-game binary patches.  For GTA2 this rewrites DMAGlide.dll's
	// hardcoded mode list before the game reads it.  We get here early
	// enough because DMAGlide.dll imports us, so it is already mapped
	// while the loader is still resolving its imports.  Never fails
	// loudly -- an unpatched game at stock resolution beats no game.
	GameFix_Apply();
    break;
  case DLL_THREAD_ATTACH:
    break;
  case DLL_THREAD_DETACH:
    break;
  default:
    break;
  }
  
  return TRUE;

} /* DllMain */
