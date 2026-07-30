//
// Glide 2 to Glide 3 Translator for GlideXP
//
// g2draw.cpp : Glide 2 Color and Alpha Functions
//
// Copyright (c) 2002 Ryan Nunn
//
// Just a note that this is written in C++ so I can use namespaces.
// No other C++ features are used.
//

#include "g2pch.h"

namespace Glide3 {

grAlphaBlendFunction_proc		grAlphaBlendFunction = 0;
grAlphaCombine_proc				grAlphaCombine = 0;
grAlphaControlsITRGBLighting_proc grAlphaControlsITRGBLighting = 0;
grAlphaTestFunction_proc		grAlphaTestFunction = 0;
grAlphaTestReferenceValue_proc	grAlphaTestReferenceValue = 0;
grColorCombine_proc				grColorCombine = 0;
grColorMask_proc				grColorMask = 0;
grConstantColorValue_proc		grConstantColorValue = 0;

bool	SetupColorFunctions()
{
#define FN_NAME "SetupColorFunctions"
    GDBG_INFO(80, "%s\n", FN_NAME);

	bool ret = true;

	if (ret) ret = LoadFunc(grAlphaBlendFunction,16);
	if (ret) ret = LoadFunc(grAlphaCombine,20);
	if (ret) ret = LoadFunc(grAlphaControlsITRGBLighting,4);
	if (ret) ret = LoadFunc(grAlphaTestFunction,4);
	if (ret) ret = LoadFunc(grAlphaTestReferenceValue,4);
	if (ret) ret = LoadFunc(grColorCombine,20);
	if (ret) ret = LoadFunc(grColorMask,8);
	if (ret) ret = LoadFunc(grConstantColorValue,4);

	return ret;
#undef FN_NAME
}

bool	FreeColorFunctions()
{
#define FN_NAME "FreeColorFunctions"
	//GDBG_INFO(80, "%s\n", FN_NAME);

	grAlphaBlendFunction = 0;
	grAlphaCombine = 0;
	grAlphaControlsITRGBLighting = 0;
	grAlphaTestFunction = 0;
	grAlphaTestReferenceValue = 0;
	grColorCombine = 0;
	grColorMask = 0;
	grConstantColorValue = 0;

	return true;
#undef FN_NAME
}

};

//
// grAlphaBlendFunction
//
NAKED_CALL void FX_CALL grAlphaBlendFunction (
                 GrAlphaBlendFnc_t rgb_sf,   GrAlphaBlendFnc_t rgb_df,
                 GrAlphaBlendFnc_t alpha_sf, GrAlphaBlendFnc_t alpha_df
                 )
{
#define FN_NAME "grAlphaBlendFunction"
    GDBG_INFO(80, "%s\n", FN_NAME);

	using Glide3::grAlphaBlendFunction;
	VOID_ASM_JMP (grAlphaBlendFunction, (rgb_sf, rgb_df, alpha_sf, alpha_df));

#undef FN_NAME
}

//
// grAlphaCombine 
//
NAKED_CALL void FX_CALL grAlphaCombine (
           GrCombineFunction_t function, GrCombineFactor_t factor,
           GrCombineLocal_t local, GrCombineOther_t other,
           FxBool invert
           )
{
#define FN_NAME "grAlphaCombine"
    GDBG_INFO(80, "%s\n", FN_NAME);

	using Glide3::grAlphaCombine;
	VOID_ASM_JMP (grAlphaCombine, (function, factor, local, other, invert));

#undef FN_NAME
}

//
// grAlphaControlsITRGBLighting
//
NAKED_CALL void FX_CALL grAlphaControlsITRGBLighting( FxBool enable )
{
#define FN_NAME "grAlphaControlsITRGBLighting"
    GDBG_INFO(80, "%s\n", FN_NAME);

	using Glide3::grAlphaControlsITRGBLighting;
	VOID_ASM_JMP (grAlphaControlsITRGBLighting, (enable));

#undef FN_NAME
}

//
// grAlphaTestFunction 
//
NAKED_CALL void FX_CALL grAlphaTestFunction ( GrCmpFnc_t function )
{
#define FN_NAME "grAlphaTestFunction"
    GDBG_INFO(80, "%s\n", FN_NAME);

	using Glide3::grAlphaTestFunction;
	VOID_ASM_JMP (grAlphaTestFunction, (function));

#undef FN_NAME
}

//
// grAlphaTestReferenceValue
//
NAKED_CALL void FX_CALL grAlphaTestReferenceValue( GrAlpha_t value )
{
#define FN_NAME "grAlphaTestReferenceValue"
    GDBG_INFO(80, "%s\n", FN_NAME);

	using Glide3::grAlphaTestReferenceValue;
	VOID_ASM_JMP (grAlphaTestReferenceValue, (value));

#undef FN_NAME
}

//
// grColorCombine 
//
NAKED_CALL void FX_CALL grColorCombine (
           GrCombineFunction_t function, GrCombineFactor_t factor,
           GrCombineLocal_t local, GrCombineOther_t other,
           FxBool invert )
{
#define FN_NAME "grColorCombine"
    GDBG_INFO(80, "%s\n", FN_NAME);

	using Glide3::grColorCombine;
	VOID_ASM_JMP (grColorCombine, (function,factor,local,other,invert));

#undef FN_NAME
}

//
// grColorMask
//
NAKED_CALL void FX_CALL grColorMask( FxBool rgb, FxBool a )
{
#define FN_NAME "grColorMask"
    GDBG_INFO(80, "%s\n", FN_NAME);

	using Glide3::grColorMask;
	VOID_ASM_JMP (grColorMask,(rgb,a));

#undef FN_NAME
}

//
// grConstantColorValue 
//
NAKED_CALL void FX_CALL grConstantColorValue ( GrColor_t value )
{
#define FN_NAME "grConstantColorValue"
    GDBG_INFO(80, "%s\n", FN_NAME);

	using Glide3::grConstantColorValue;
	VOID_ASM_JMP (grConstantColorValue,(value));

#undef FN_NAME
}

void FX_CALL grConstantColorValue4( float a, float r, float g, float b )
{
#define FN_NAME "grConstantColorValue4"
    GDBG_INFO(80, "%s (%f,%f,%f,%f)\n", FN_NAME, a, r, g,b);

	// TODO Make sure this works correctly (which i know it wont)
	FxU32 red = (FxU32) (r);
	FxU32 green = (FxU32) (g);
	FxU32 blue = (FxU32) (b);
	FxU32 alpha = (FxU32) (a);

	Glide3::grConstantColorValue((alpha << 24) | (red << 16) | (green << 8) | blue);
#undef FN_NAME
}
