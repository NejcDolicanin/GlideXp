//
// Glide 2 to Glide 3 Translator for GlideXP
//
// g2draw.h : Glide 2 Color and Alpha Functions
//
// Copyright (c) 2002 Ryan Nunn
//
// Just a note that this is written in C++ so I can use namespaces.
// No other C++ features are used.
//

namespace Glide3 {

	bool	SetupColorFunctions();
	bool	FreeColorFunctions();

	typedef	void (FX_CALL * grAlphaBlendFunction_proc) ( GrAlphaBlendFnc_t, GrAlphaBlendFnc_t, GrAlphaBlendFnc_t, GrAlphaBlendFnc_t );
	typedef	void (FX_CALL * grAlphaCombine_proc) ( GrCombineFunction_t, GrCombineFactor_t, GrCombineLocal_t, GrCombineOther_t, FxBool );
	typedef	void (FX_CALL * grAlphaControlsITRGBLighting_proc) ( FxBool enable );
	typedef	void (FX_CALL * grAlphaTestFunction_proc) ( GrCmpFnc_t function );
	typedef	void (FX_CALL * grAlphaTestReferenceValue_proc) ( GrAlpha_t value );
	typedef	void (FX_CALL * grColorCombine_proc) ( GrCombineFunction_t, GrCombineFactor_t, GrCombineLocal_t, GrCombineOther_t, FxBool );
	typedef	void (FX_CALL * grColorMask_proc) ( FxBool rgb, FxBool a );
	typedef	void (FX_CALL * grConstantColorValue_proc) ( GrColor_t value );

	extern	grAlphaBlendFunction_proc		grAlphaBlendFunction;
	extern	grAlphaCombine_proc				grAlphaCombine;
	extern	grAlphaControlsITRGBLighting_proc grAlphaControlsITRGBLighting;
	extern	grAlphaTestFunction_proc		grAlphaTestFunction;
	extern	grAlphaTestReferenceValue_proc	grAlphaTestReferenceValue;
	extern	grColorCombine_proc				grColorCombine;
	extern	grColorMask_proc				grColorMask;
	extern	grConstantColorValue_proc		grConstantColorValue;

};

extern "C" FX_ENTRY void FX_CALL grConstantColorValue4( float a, float r, float g, float b );
