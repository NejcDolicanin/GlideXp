//
// Glide 2 to Glide 3 Translator for GlideXP
//
// g2fog.cpp : Fog related functions
//
// Copyright (c) 2002 Ryan Nunn
//
// Just a note that this is written in C++ so I can use namespaces.
// No other C++ features are used.
//

#include "g2pch.h"

namespace Glide3 {

grFogColorValue_proc			grFogColorValue = 0;
grFogMode_proc					grFogMode = 0;
grFogTable_proc					grFogTable = 0;
guFogTableIndexToW_proc			guFogTableIndexToW = 0;
guFogGenerateExp_proc			guFogGenerateExp = 0;
guFogGenerateExp2_proc			guFogGenerateExp2 = 0;
guFogGenerateLinear_proc		guFogGenerateLinear = 0;

bool	SetupFogFunctions()
{
#define FN_NAME "SetupFogFunctions"
    GDBG_INFO(80, "%s\n", FN_NAME);

	bool ret = true;

	if (ret) ret = LoadFunc(grFogColorValue,4);
	if (ret) ret = LoadFunc(grFogMode,4);
	if (ret) ret = LoadFunc(grFogTable,4);
	if (ret) ret = LoadFunc(guFogTableIndexToW,4);
	if (ret) ret = LoadFunc(guFogGenerateExp,8);
	if (ret) ret = LoadFunc(guFogGenerateExp2,8);
	if (ret) ret = LoadFunc(guFogGenerateLinear,12);

	return ret;
#undef FN_NAME
}

bool	FreeFogFunctions()
{
#define FN_NAME "FreeFogFunctions"
//    GDBG_INFO(80, "%s\n", FN_NAME);

	grFogColorValue = 0;
	grFogMode = 0;
	grFogTable = 0;
	guFogTableIndexToW = 0;
	guFogGenerateExp = 0;
	guFogGenerateExp2 = 0;
	guFogGenerateLinear = 0;

	return true;
#undef FN_NAME
}

};

//
// grFogColorValue
//
NAKED_CALL void FX_CALL grFogColorValue( GrColor_t fogcolor )
{
#define FN_NAME "grFogColorValue"
    GDBG_INFO(69, "%s (0x%08X)\n", FN_NAME, fogcolor);

	using Glide3::grFogColorValue;
	VOID_ASM_JMP (grFogColorValue, (fogcolor));

#undef FN_NAME
}

//
// grFogMode
//
#define G2_FOG_DISABLE             0x0
#define G2_FOG_WITH_ITERATED_ALPHA 0x1
#define G2_FOG_WITH_TABLE          0x2
#define G2_FOG_WITH_ITERATED_Z     0x3          /* Bug 735 */

NAKED_CALL void FX_CALL grFogMode( GrFogMode_t mode )
{
#define FN_NAME "grFogMode"
    GDBG_INFO(69, "%s (%i)\n", FN_NAME, mode);

#ifdef _DEBUG
	if ((mode & 0xFF) == G2_FOG_WITH_ITERATED_ALPHA) mode += 3;

	Glide3::grFogMode(mode);
#else
	using Glide3::grFogMode;
	__asm {
		mov eax, [esp+4]
		cmp al, G2_FOG_WITH_ITERATED_ALPHA	// See if we are using iterated z
		jne not_it_a;						// If not, just call the normal function
		add [esp+4], 3						// Need this to be 4 instead of 1.

not_it_a:
		jmp grFogMode;
	};
#endif
#undef FN_NAME
}

//
// grFogTable
//
NAKED_CALL void FX_CALL grFogTable( const GrFog_t ft[] )
{
#define FN_NAME "grFogTable"
    GDBG_INFO(69, "%s\n", FN_NAME);

	using Glide3::grFogTable;
	VOID_ASM_JMP (grFogTable,(ft));

#undef FN_NAME
}

//
// guFogTableIndexToW
//
NAKED_CALL float FX_CALL guFogTableIndexToW( int i )
{
#define FN_NAME "guFogTableIndexToW"
    GDBG_INFO(69, "%s\n", FN_NAME);

	using Glide3::guFogTableIndexToW;
	ASM_JMP (guFogTableIndexToW,(i));

#undef FN_NAME
}

//
// guFogGenerateExp
//
NAKED_CALL void FX_CALL guFogGenerateExp( GrFog_t *fogtable, float density )
{
#define FN_NAME "guFogGenerateExp"
    GDBG_INFO(69, "%s\n", FN_NAME);

	using Glide3::guFogGenerateExp;
	VOID_ASM_JMP (guFogGenerateExp,(fogtable,density));

#undef FN_NAME
}

//
// guFogGenerateExp2
//
NAKED_CALL void FX_CALL guFogGenerateExp2( GrFog_t *fogtable, float density )
{
#define FN_NAME "guFogGenerateExp2"
    GDBG_INFO(69, "%s\n", FN_NAME);

	using Glide3::guFogGenerateExp2;
	VOID_ASM_JMP (guFogGenerateExp2,(fogtable,density));

#undef FN_NAME
}

//
// guFogGenerateLinear
//
NAKED_CALL void FX_CALL guFogGenerateLinear(GrFog_t *fogtable, float nearZ, float farZ )
{
#define FN_NAME "guFogGenerateLinear"
    GDBG_INFO(69, "%s\n", FN_NAME);

	using Glide3::guFogGenerateLinear;
	VOID_ASM_JMP (guFogGenerateLinear,(fogtable,nearZ,farZ));

#undef FN_NAME
}

