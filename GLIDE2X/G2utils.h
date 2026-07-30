//
// Glide 2 to Glide 3 Translator for GlideXP
//
// g2utils.h : Glide 2 Utility Functions
//
// Copyright (c) 2002 Ryan Nunn
//
// Just a note that this is written in C++ so I can use namespaces.
// No other C++ features are used.
//

namespace Glide3 {

	bool	SetupUtilsFunctions();
	bool	FreeUtilsFunctions();

	typedef FxBool (FX_CALL * gu3dfGetInfo_proc) ( const char *filename, Gu3dfInfo *info );
	typedef FxBool (FX_CALL * gu3dfLoad_proc) ( const char *filename, Gu3dfInfo *data );
	typedef void (FX_CALL * guGammaCorrectionRGB_proc) ( FxFloat red, FxFloat green, FxFloat blue );

	extern	gu3dfGetInfo_proc				gu3dfGetInfo;
	extern	gu3dfLoad_proc					gu3dfLoad;
	extern	guGammaCorrectionRGB_proc		guGammaCorrectionRGB;
};

extern "C" FX_EXPORT FxU16 * FX_CALL guTexCreateColorMipMap ( void );
extern "C" FX_EXPORT int FX_CALL guEncodeRLE16 ( void *dst, void *src, FxU32 width, FxU32 height );
extern "C" FX_EXPORT FxU32 FX_CALL guEndianSwapWords ( FxU32 value );
extern "C" FX_EXPORT FxU16 FX_CALL guEndianSwapBytes ( FxU16 value );
