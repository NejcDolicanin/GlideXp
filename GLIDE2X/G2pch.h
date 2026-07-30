//
// Glide 2 to Glide 3 Translator for GlideXP
//
// g2pch.h : Precompiled Header
//
// Copyright (c) 2002 Ryan Nunn
//
// Just a note that this is written in C++ so I can use namespaces.
// No other C++ features are used.
//

#pragma once

// No pointcasting of palettes and ncc tables
#define ALLOW_POINTCASTS		0

#ifdef _DEBUG
#define GDBG_INFO_ON			1
#define NAKED_CALL
#define ASM_JMP(func, args) return func args
#define VOID_ASM_JMP(func, args) func args
#else
#define NAKED_CALL	__declspec(naked)
#define ASM_JMP(func, args) __asm jmp func
#define VOID_ASM_JMP(func, args) __asm jmp func
#endif

#define FX_DLL_DEFINITION
#define NO_TRANSLATION			1
#define GLIDE2					1

#ifdef _MSC_VER
#define __MSC__					1
#endif

#ifdef WIN32
#define __WIN32__				1
#endif

#define DO_Z_CLEAR_FIX			1

#include <windows.h>
#include "winglide3.h"
#include "3dfx.h"
#include "fxdll.h"
#include "glide.h"
#include "fxglide.h"
#include "../gxpver.h"

//#ifdef GDBG_INFO
//#undef GDBG_INFO
//#define GDBG_INFO      0 && (unsigned long)
//#endif

#ifdef grSstWinClose 
#undef grSstWinClose 
#endif

#ifdef grSstWinOpen
#undef grSstWinOpen 
#endif

#include "g2impfuncs.h"
#include "g2debug.h"
#include "g2glide.h"
#include "g2misc.h"
#include "g2buffer.h"
#include "g2sst.h"
#include "g2tex.h"
#include "g2draw.h"
#include "g2color.h"
#include "g2utils.h"
#include "g2fog.h"
#include "g2state.h"
#include "g2clip.h"

// These are just required so Glide2 code wont complain
#define smallLod smallLodLog2
#define largeLod largeLodLog2
#define aspectRatio aspectRatioLog2
