//
// Glide 2 to Glide 3 Translator for GlideXP
//
// gxpver.h : Version Info Header
//
// Copyright (c 2002 Ryan Nunn
//

#ifndef GXPVER_H_INCLUDED
#define GXPVER_H_INCLUDED

#define _TRADEMARK_  "(tm)"
#define _REGISTERED_ "\256"
#define _COPYRIGHT_  "\251"

// Glide2x Translator
#ifdef G2_TRANS

	#define VERSIONNAME             "glide2x.dll\0"
	#define GLIDEXP                 "Glide2 to Glide3 Translator"

	// Glide2x translator int version
	#define MAJVERSION				1
	#define MAJREVISION				00
	#define MINREVISION				01
	#define BUILD_NUMBER			107

	// Glide2x translator str version
	#define MAJVERSIONSTR			"1"
	#define MAJREVISIONSTR			"00"
	#define MINREVISIONSTR			"01"
	#define BUILD_NUMBERSTR			"0107"

	// Glide2x version page strings
	#define HWSTR   "Voodoo Graphics" _REGISTERED_ ", Voodoo Rush" _REGISTERED_ ", Voodoo^2" _REGISTERED_ ", Voodoo Banshee" _REGISTERED_ ", Voodoo3" _REGISTERED_ ", Velocity" _TRADEMARK_ " 100/200" ", Voodoo4" _REGISTERED_ ", & Voodoo5" _REGISTERED_ ""
	#define PRODNAME "Glide" _REGISTERED_ " for " HWSTR "; Windows 95" _REGISTERED_ ", Windows 98" _REGISTERED_ ", & Windows NT" _REGISTERED_ ""

// Glide2x Direct3D Wrapper
#elif defined(DIRECT_G2XP)

	#define VERSIONNAME             "glide2x.dll\0"
	#define GLIDEXP                 "GlideXP Glide 2 to Direct3D Wrapper"

	// Glide2x translator int version
	#define MAJVERSION				1
	#define MAJREVISION				00
	#define MINREVISION				01
	#define BUILD_NUMBER			0106

	// Glide2x translator str version
	#define MAJVERSIONSTR			"1"
	#define MAJREVISIONSTR			"00"
	#define MINREVISIONSTR			"01"
	#define BUILD_NUMBERSTR			"0106"

	// Glide2x version page strings
	#define HWSTR   "Direct3D9"
	#define PRODNAME "DirectG2XP"

// GxpOGL ICD Interceptor
#elif (GXPOGL)

	#define VERSIONNAME             "3dfxogl.dll\0"
	#define GLIDEXP                 "GxpOGL ICD Interceptor"

	// GxpOGL ICD Interceptor int version
	#define MAJVERSION				1
	#define MAJREVISION				00
	#define MINREVISION				01
	#define BUILD_NUMBER			0106

	// GxpOGL ICD Interceptor str version
	#define MAJVERSIONSTR			"1"
	#define MAJREVISIONSTR			"00"
	#define MINREVISIONSTR			"01"
	#define BUILD_NUMBERSTR			"0106"

	// Glide2x version page strings
	#define HWSTR   " Voodoo Banshee" _REGISTERED_ ", Voodoo3" _REGISTERED_ ", & Velocity" _TRADEMARK_ " 100/200"
	#define PRODNAME "GxpOGL for" HWSTR "; Windows 95" _REGISTERED_ ", Windows 98" _REGISTERED_ ", & Windows NT" _REGISTERED_ ""

// GlideXP
#else				

	#include "winglide3.h"

	#if WINXP_ALT_TAB_FIX
	#define SAFE " Safe"
	#elif WINXP_FASTER_ALT_TAB_FIX
	#define SAFE " Fast"
	#else
	#define SAFE " Unsafe"
	#endif

	#if GLIDE_USE_C_TRISETUP
	#define NOASM " No Asm"
	#else
	#define NOASM
	#endif

	#define VERSIONNAME             "glide3x.dll\0"
	#define GLIDEXP                 "GlideXP" SAFE NOASM

	// GlideXP int version
	#define MAJVERSION              3
	#define MAJREVISION             20
	#define MINREVISION				00
	#define BUILD_NUMBER            1016

	// GlideXP str version
	#define MAJVERSIONSTR			"3"
	#define MAJREVISIONSTR			"20"
	#define MINREVISIONSTR			"00"
	#define BUILD_NUMBERSTR			"1016"

	#define HWSTR   " Voodoo Banshee" _REGISTERED_ ", Voodoo3" _REGISTERED_ ", & Velocity" _TRADEMARK_ " 100/200"
	#define PRODNAME "Glide" _REGISTERED_ " for" HWSTR "; Windows 95" _REGISTERED_ ", Windows 98" _REGISTERED_ ", & Windows NT" _REGISTERED_ ""

#endif

#ifdef _DEBUG
	#define DEBUGSTR " Debug Build"
#else
	#define DEBUGSTR ""
#endif

#define VERSIONSTR MAJVERSIONSTR "." MAJREVISIONSTR "." MINREVISIONSTR "." BUILD_NUMBERSTR

#endif //GXPVER_H_INCLUDED
