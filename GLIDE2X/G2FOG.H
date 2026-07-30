//
// Glide 2 to Glide 3 Translator for GlideXP
//
// g2fog.h : Fog related functions
//
// Copyright (c) 2002 Ryan Nunn
//
// Just a note that this is written in C++ so I can use namespaces.
// No other C++ features are used.
//

namespace Glide3 {

	bool	SetupFogFunctions();
	bool	FreeFogFunctions();

	typedef void (FX_CALL * grFogColorValue_proc) ( GrColor_t fogcolor );
	typedef void (FX_CALL * grFogMode_proc) ( GrFogMode_t mode );
	typedef void (FX_CALL * grFogTable_proc) ( const GrFog_t ft[] );
	typedef float (FX_CALL * guFogTableIndexToW_proc) ( int i );
	typedef void (FX_CALL * guFogGenerateExp_proc) ( GrFog_t *fogtable, float density );
	typedef void (FX_CALL * guFogGenerateExp2_proc) ( GrFog_t *fogtable, float density );
	typedef void (FX_CALL * guFogGenerateLinear_proc) (GrFog_t *fogtable, float nearZ, float farZ );

	extern	grFogColorValue_proc			grFogColorValue;
	extern	grFogMode_proc					grFogMode;
	extern	grFogTable_proc					grFogTable;
	extern	guFogTableIndexToW_proc			guFogTableIndexToW;
	extern	guFogGenerateExp_proc			guFogGenerateExp;
	extern	guFogGenerateExp2_proc			guFogGenerateExp2;
	extern	guFogGenerateLinear_proc		guFogGenerateLinear;
};
