//
// Glide 2 to Glide 3 Translator for GlideXP
//
// g2buffer.cpp : Buffer related functions
//
// Copyright (c) 2002 Ryan Nunn
//
// Just a note that this is written in C++ so I can use namespaces.
// No other C++ features are used.
//

#include "g2pch.h"

// The values for the extemes on the depth buffer
static FxU32 zMinMax[2];
static FxU32 wMinMax[2];

namespace Glide3 {

grBufferClear_proc				grBufferClear = 0;
grBufferSwap_proc				grBufferSwap = 0;
grDepthBiasLevel_proc			grDepthBiasLevel = 0;
grDepthBufferFunction_proc		grDepthBufferFunction = 0;
grDepthBufferMode_proc			grDepthBufferMode = 0;
grDepthMask_proc				grDepthMask = 0;
grDisableAllEffects_proc		grDisableAllEffects = 0;
grLfbConstantAlpha_proc			grLfbConstantAlpha = 0;
grLfbConstantDepth_proc			grLfbConstantDepth = 0;
grLfbLock_proc					grLfbLock = 0;
grLfbReadRegion_proc			grLfbReadRegion = 0;
grLfbUnlock_proc				grLfbUnlock = 0;
grLfbWriteRegion_proc			grLfbWriteRegion = 0;
grRenderBuffer_proc				grRenderBuffer = 0;
grAuxBuffer_proc				grAuxBufferExt = 0;

bool	SetupBufferFunctions()
{
#define FN_NAME "SetupBufferFunctions"
    GDBG_INFO(80, "%s\n", FN_NAME);

	bool ret = true;

	if (ret) ret = LoadFunc(grBufferClear,12);
	if (ret) ret = LoadFunc(grBufferSwap,4);
	if (ret) ret = LoadFunc(grDepthBiasLevel,4);
	if (ret) ret = LoadFunc(grDepthBufferFunction,4);
	if (ret) ret = LoadFunc(grDepthBufferMode,4);
	if (ret) ret = LoadFunc(grDepthMask,4);
	if (ret) ret = LoadFunc(grDisableAllEffects,0);
	if (ret) ret = LoadFunc(grLfbConstantAlpha,4);
	if (ret) ret = LoadFunc(grLfbConstantDepth ,4);
	if (ret) ret = LoadFunc(grLfbLock,24);
	if (ret) ret = LoadFunc(grLfbReadRegion,28);
	if (ret) ret = LoadFunc(grLfbUnlock,8);
	if (ret) ret = LoadFunc(grLfbWriteRegion,36);
	if (ret) ret = LoadFunc(grRenderBuffer,4);

	// These funcs aren't normally exported by Glide 3. 
	// They are only available as extensions.
	if (ret) {
		// TEXTUREBUFFER extension
		grAuxBufferExt = (grAuxBuffer_proc) 
				Glide3::grGetProcAddress("grAuxBufferExt");

	}

	return ret;
#undef FN_NAME
}

bool	FreeBufferFunctions()
{
#define FN_NAME "FreeBufferFunctions"
	//GDBG_INFO(80, "%s\n", FN_NAME);

	grBufferClear = 0;
	grBufferSwap = 0;
	grDepthBiasLevel = 0;
	grDepthBufferFunction = 0;
	grDepthBufferMode = 0;
	grDepthMask = 0;
	grDisableAllEffects = 0;
	grLfbConstantAlpha = 0;
	grLfbConstantDepth = 0;
	grLfbLock = 0;
	grLfbReadRegion = 0;
	grLfbUnlock = 0;
	grLfbWriteRegion = 0;
	grRenderBuffer = 0;
	grAuxBufferExt = 0;

	return true;
#undef FN_NAME
}

};

//
// grBufferSwap
//
// In theory doing this 'should' be ok, but just to be sure, I'll allow for a
// compile time override. The Glide3 func is FxU32 while the Glide2 func is int
//
#ifndef SAFER_grBufferSwap

NAKED_CALL void FX_CALL grBufferSwap( int swap_interval )
{
#define FN_NAME "grBufferSwap"
    GDBG_INFO(80, "%s\n", FN_NAME);

	using Glide3::grBufferSwap;
	VOID_ASM_JMP(grBufferSwap, (swap_interval));

#undef FN_NAME
}

#else

void FX_CALL grBufferSwap( int swap_interval )
{
#define FN_NAME "grBufferSwap"
    GDBG_INFO(80, "%s\n", FN_NAME);

	Glide3::grBufferSwap(swap_interval);

#undef FN_NAME
}

#endif

//
// grBufferClear
//
void FX_CALL grBufferClear( GrColor_t color, GrAlpha_t alpha, FxU16 depth )
{
#define FN_NAME "grBufferClear"
    GDBG_INFO(80, "%s\n", FN_NAME);

	// We need to hack around the fact that Glide3 doesn't use constants for 
	// clearing the depth buffer
#ifdef DO_Z_CLEAR_FIX
	if (theState.wBuffer) {
		if (wMinMax[1] == 0xFFFFFF && wMinMax[0] == 0) {
			Glide3::grBufferClear (color, alpha, (((FxU32) depth)<<8)|depth>>8);
		}
		else if (wMinMax[1] == 0xFFFF && wMinMax[0] == 0) {
			Glide3::grBufferClear (color, alpha, depth);
		}
		else {
			float depthf = depth / 65535.0F;
			depthf *= wMinMax[1];
			Glide3::grBufferClear (color, alpha, (FxU32) depthf);
		}
	}
	else if (!theState.wBuffer) {
		if (zMinMax[0] == 0xFFFFFF && zMinMax[1] == 0) {
			Glide3::grBufferClear (color, alpha, (((FxU32) depth)<<8)|depth>>8);
		}
		else if (zMinMax[0] == 0xFFFF && zMinMax[1] == 0) {
			Glide3::grBufferClear (color, alpha, depth);
		}
		else {
			float depthf = depth / 65535.0F;
			depthf *= zMinMax[0];
			Glide3::grBufferClear (color, alpha, (FxU32) depthf);
		}
	}
	else
#endif
		Glide3::grBufferClear (color, alpha, depth);

#undef FN_NAME
}

//
// grBufferNumPending
//
int FX_CALL grBufferNumPending( void )
{
#define FN_NAME "grBufferNumPending"
    GDBG_INFO(80, "%s\n", FN_NAME);

	FxI32 ret;
	Glide3::grGet(GR_PENDING_BUFFERSWAPS,4,&ret);

    GDBG_INFO(81, "%s: %i pending\n", FN_NAME, ret);
	return ret;

#undef FN_NAME
}

//
// grDepthBiasLevel
//
void FX_CALL grDepthBiasLevel( FxI16 level )
{
#define FN_NAME "grDepthBiasLevel"
    GDBG_INFO(80, "%s\n", FN_NAME);

	// It's unsafe to attempt to use NAKED_CALL with this.
	// Glide2 uses FxI16 while Glide 3 uses FxI32

	Glide3::grDepthBiasLevel(level);

#undef FN_NAME
}

//
// grDepthBufferFunction
//
NAKED_CALL void FX_CALL grDepthBufferFunction( GrCmpFnc_t function )
{
#define FN_NAME "grDepthBufferFunction"
    GDBG_INFO(80, "%s\n", FN_NAME);

	using Glide3::grDepthBufferFunction;
	VOID_ASM_JMP (grDepthBufferFunction, (function));

#undef FN_NAME
}

//
// grDepthBufferMode
//
void FX_CALL grDepthBufferMode ( GrDepthBufferMode_t mode )
{
#define FN_NAME "grDepthBufferMode"
    GDBG_INFO(80, "%s\n", FN_NAME);

	Glide3::grDepthBufferMode(mode);

	if (mode == GR_DEPTHBUFFER_WBUFFER || mode == GR_DEPTHBUFFER_WBUFFER_COMPARE_TO_BIAS)
		theState.wBuffer = FXTRUE;
	else 
		theState.wBuffer = FXFALSE;

	// Just do it for now
//	g2SetupGrVertex();
//	g2SetupGrVertexTMUs();

#undef FN_NAME
}

//
// grDepthMask
//
NAKED_CALL void FX_CALL grDepthMask( FxBool mask )
{
#define FN_NAME "grDepthMask"
    GDBG_INFO(80, "%s\n", FN_NAME);

	using Glide3::grDepthMask;
	VOID_ASM_JMP (grDepthMask, (mask));

#undef FN_NAME
}

//
// grDisableAllEffects
//
NAKED_CALL void FX_CALL grDisableAllEffects( void )
{
#define FN_NAME "grDisableAllEffects"
    GDBG_INFO(80, "%s\n", FN_NAME);

	using Glide3::grDisableAllEffects;
	VOID_ASM_JMP (grDisableAllEffects,());

#undef FN_NAME
}

//
// grDepthBufferFunction
//
NAKED_CALL void FX_CALL grLfbConstantAlpha( GrAlpha_t alpha )
{
#define FN_NAME "grLfbConstantAlpha"
    GDBG_INFO(80, "%s\n", FN_NAME);

	using Glide3::grLfbConstantAlpha;
	VOID_ASM_JMP (grLfbConstantAlpha,(alpha));

#undef FN_NAME
}

//
// grLfbConstantDepth
//
void FX_CALL grLfbConstantDepth( FxU16 depth )
{
#define FN_NAME "grLfbConstantDepth"
    GDBG_INFO(80, "%s\n", FN_NAME);

	// It's unsafe to attempt to use NAKED_CALL with this.
	// Glide2 uses FxI16 while Glide 3 uses FxI32

	Glide3::grLfbConstantDepth(depth);

#undef FN_NAME
}

//
// grLfbLock
//
NAKED_CALL FxBool FX_CALL grLfbLock( GrLock_t type, GrBuffer_t buffer, GrLfbWriteMode_t writeMode,
           GrOriginLocation_t origin, FxBool pixelPipeline, 
           GrLfbInfo_t *info )
{
#define FN_NAME "grLfbLock"
    GDBG_INFO(80, "%s\n", FN_NAME);

	using Glide3::grLfbLock;
	ASM_JMP (grLfbLock, (type,buffer,writeMode,origin,pixelPipeline,info));

#undef FN_NAME
}



//
// grLfbReadRegion
//
NAKED_CALL FxBool FX_CALL grLfbReadRegion( GrBuffer_t src_buffer,
                 FxU32 src_x, FxU32 src_y,
                 FxU32 src_width, FxU32 src_height,
                 FxU32 dst_stride, void *dst_data )
{
#define FN_NAME "grLfbReadRegion"
    GDBG_INFO(80, "%s\n", FN_NAME);

	using Glide3::grLfbReadRegion;
	ASM_JMP (grLfbReadRegion, (src_buffer, src_x, src_y, src_width, src_height, dst_stride, dst_data));

#undef FN_NAME
}

//
// grLfbUnlock
//
NAKED_CALL FxBool FX_CALL grLfbUnlock( GrLock_t type, GrBuffer_t buffer )
{
#define FN_NAME "grLfbUnlock"
    GDBG_INFO(80, "%s\n", FN_NAME);

	using Glide3::grLfbUnlock;
	ASM_JMP (grLfbUnlock,(type,buffer));

#undef FN_NAME
}


//
// grLfbWriteRegion
//
FxBool FX_CALL grLfbWriteRegion( GrBuffer_t dst_buffer, 
						  FxU32 dst_x, FxU32 dst_y, 
						  GrLfbSrcFmt_t src_format, 
						  FxU32 src_width, FxU32 src_height, 
						  FxI32 src_stride, void *src_data )
{
#define FN_NAME "grLfbWriteRegion"
    GDBG_INFO(80, "%s\n", FN_NAME);

	// We won't put it through the pixel pipe.
	// But I Don't know if this is the correct behaviour

	return Glide3::grLfbWriteRegion(dst_buffer,
							dst_x, dst_y, 
							src_format,
							src_width, src_height,
							FXFALSE,
							src_stride, src_data);
#undef FN_NAME
}

//
// grRenderBuffer
//
NAKED_CALL void FX_CALL grRenderBuffer( GrBuffer_t buffer )
{
#define FN_NAME "grRenderBuffer"
    GDBG_INFO(80, "%s\n", FN_NAME);

	using Glide3::grRenderBuffer;
	VOID_ASM_JMP (grRenderBuffer,(buffer));

#undef FN_NAME
}


//
// This sets up the extents of the depth buffer
//
void g2SetupDepthBufferExtents()
{
#define FN_NAME "g2SetupDepthBufferExtents"
    GDBG_INFO(80, "%s\n", FN_NAME);

	// Get the Limits
	Glide3::grGet( GR_ZDEPTH_MIN_MAX, 8, (FxI32 *) zMinMax );
	Glide3::grGet( GR_WDEPTH_MIN_MAX, 8, (FxI32 *) wMinMax );

    GDBG_INFO(80, "%s:  w:%06X W:%06X z:%06X Z:%06X \n", FN_NAME, wMinMax[0], wMinMax[1], zMinMax[0], zMinMax[1]);

#undef FN_NAME
}

//
// TODO
//

//
// grLfbWriteColorFormat
//
void FX_CALL grLfbWriteColorFormat (GrColorFormat_t colorFormat)
{
#define FN_NAME "grLfbWriteColorFormat"
    GDBG_INFO(80, "%s\n", FN_NAME);
#undef FN_NAME
} /* grLfbWriteColorFormat */

//
// grLfbWriteColorSwizzle
// 
void FX_CALL grLfbWriteColorSwizzle (FxBool swizzleBytes, FxBool swapWords)
{
#define FN_NAME "grLfbWriteColorSwizzle"
    GDBG_INFO(80, "%s\n", FN_NAME);
#undef FN_NAME
} /* grLfbWriteColorSwizzle */

