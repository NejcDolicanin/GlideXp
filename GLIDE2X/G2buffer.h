//
// Glide 2 to Glide 3 Translator for GlideXP
//
// g2buffer.h : Buffer related functions
//
// Copyright (c) 2002 Ryan Nunn
//
// Just a note that this is written in C++ so I can use namespaces.
// No other C++ features are used.
//

namespace Glide3 {

	bool	SetupBufferFunctions();
	bool	FreeBufferFunctions();

	typedef void (FX_CALL * grBufferClear_proc) ( GrColor_t color, GrAlpha_t alpha, FxU32 depth );
	typedef void (FX_CALL * grBufferSwap_proc) ( FxU32 swap_interval );
	typedef void (FX_CALL * grDepthBiasLevel_proc) ( FxI32 level );
	typedef void (FX_CALL * grDepthBufferFunction_proc) ( GrCmpFnc_t function );
	typedef void (FX_CALL * grDepthBufferMode_proc) ( GrDepthBufferMode_t mode );
	typedef void (FX_CALL * grDepthMask_proc) ( FxBool mask );
	typedef void (FX_CALL * grDisableAllEffects_proc) ( void );
	typedef void (FX_CALL * grLfbConstantAlpha_proc) ( GrAlpha_t alpha );
	typedef void (FX_CALL * grLfbConstantDepth_proc) ( FxU32 depth );
	typedef FxBool (FX_CALL * grLfbLock_proc) ( GrLock_t, GrBuffer_t, GrLfbWriteMode_t, GrOriginLocation_t, FxBool, GrLfbInfo_t * );
	typedef FxBool (FX_CALL * grLfbReadRegion_proc) ( GrBuffer_t src_buffer, FxU32 src_x, FxU32 src_y, FxU32 src_width, FxU32 src_height, FxU32 dst_stride, void *dst_data );
	typedef FxBool (FX_CALL * grLfbUnlock_proc) ( GrLock_t type, GrBuffer_t buffer );
	typedef FxBool (FX_CALL * grLfbWriteRegion_proc) ( GrBuffer_t, FxU32, FxU32, GrLfbSrcFmt_t,  FxU32, FxU32, FxBool, FxI32, void * );
	typedef void (FX_CALL * grRenderBuffer_proc) ( GrBuffer_t buffer );
	typedef void (FX_CALL * grAuxBuffer_proc)( GrBuffer_t buffer );

	extern	grBufferClear_proc				grBufferClear;
	extern	grBufferSwap_proc				grBufferSwap;
	extern	grDepthBiasLevel_proc			grDepthBiasLevel;
	extern	grDepthBufferFunction_proc		grDepthBufferFunction;
	extern	grDepthBufferMode_proc			grDepthBufferMode;
	extern	grDepthMask_proc				grDepthMask;
	extern	grDisableAllEffects_proc		grDisableAllEffects;
	extern	grLfbConstantAlpha_proc			grLfbConstantAlpha;
	extern	grLfbConstantDepth_proc			grLfbConstantDepth;
	extern	grLfbLock_proc					grLfbLock;
	extern	grLfbReadRegion_proc			grLfbReadRegion;
	extern	grLfbUnlock_proc				grLfbUnlock;
	extern	grLfbWriteRegion_proc			grLfbWriteRegion;
	extern	grRenderBuffer_proc				grRenderBuffer;
	extern	grAuxBuffer_proc				grAuxBufferExt;
};

void g2SetupDepthBufferExtents();

extern "C" FX_ENTRY void FX_CALL grLfbWriteColorFormat (GrColorFormat_t colorFormat);
extern "C" FX_ENTRY void FX_CALL grLfbWriteColorSwizzle (FxBool swizzleBytes, FxBool swapWords);
