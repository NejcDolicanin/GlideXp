//
// Glide 2 to Glide 3 Translator for GlideXP
//
// g2misc.h : Misc Functions
//
// Copyright (c) 2002 Ryan Nunn
//
// Just a note that this is written in C++ so I can use namespaces.
// No other C++ features are used.
//

namespace Glide3 {

	bool	SetupMiscFunctions();
	bool	FreeMiscFunctions();

	typedef void (FX_CALL * grChromakeyMode_proc) ( GrChromakeyMode_t mode );
	typedef void (FX_CALL * grChromakeyValue_proc)( GrColor_t value );
	typedef void (FX_CALL * grClipWindow_proc) ( FxU32 minx, FxU32 miny, FxU32 maxx, FxU32 maxy );
	typedef void (FX_CALL * grCoordinateSpace_proc) ( GrCoordinateSpaceMode_t mode );
	typedef void (FX_CALL * grDisable_proc) ( GrEnableMode_t mode );
	typedef void (FX_CALL * grDitherMode_proc) ( GrDitherMode_t mode );
	typedef void (FX_CALL * grEnable_proc) ( GrEnableMode_t mode );
	typedef FxU32 (FX_CALL * grGet_proc) ( FxU32 pname, FxU32 plength, void *params );
	typedef	GrProc (FX_CALL * grGetProcAddress_proc) ( char *procName );
	typedef	char* (FX_CALL * grGetRegistryOrEnvironmentStringExt_proc) (char* theEntry);
	typedef const char * (FX_CALL * grGetString_proc) ( FxU32 pname );
	typedef FxBool (FX_CALL * grReset_proc) ( FxU32 what );
	typedef void (FX_CALL * grErrorSetCallback_proc) ( GrErrorCallbackFnc_t fnc );
	typedef void (FX_CALL * grSplash_proc) (float x, float y, float width, float height, FxU32 frame);
	typedef void (FX_CALL * grLoadGammaTable_proc)( FxU32 nentries, FxU32 *red, FxU32 *green, FxU32 *blue);

	extern	grChromakeyMode_proc			grChromakeyMode;
	extern	grChromakeyValue_proc			grChromakeyValue;
	extern	grClipWindow_proc				grClipWindow;
	extern	grDitherMode_proc				grDitherMode;
	extern	grCoordinateSpace_proc			grCoordinateSpace;
	extern	grDisable_proc					grDisable;
	extern	grEnable_proc					grEnable;
	extern	grGet_proc						grGet;
	extern	grGetProcAddress_proc			grGetProcAddress;
	extern	grGetRegistryOrEnvironmentStringExt_proc	grGetRegistryOrEnvironmentStringExt;
	extern	grGetString_proc				grGetString;
	extern	grReset_proc					grReset;
	extern	grErrorSetCallback_proc			grErrorSetCallback;
	extern	grSplash_proc					grSplash;
	extern	grLoadGammaTable_proc			grLoadGammaTable;

};

void g2SetupGrVertex();
void g2SetupGrVertexTMUs();
void SetupTuples();

extern "C" FX_ENTRY void FX_CALL grCheckForRoom (FxI32 n);
extern "C" FX_ENTRY GrProc FX_CALL grGetProcAddressExtXP (char *procName);
extern "C" FX_ENTRY void FX_CALL grDitherMode( GrDitherMode_t mode );

void FX_CALL grGammaCorrectionRGBExt (float red, float green, float blue);
void FX_CALL grLoadGammaTableExt( FxU32 red[256], FxU32 green[256], FxU32 blue[256]);

// GlideXP Extension
#define GXP_HINT_AA_MULTI_SAMPLE				32
#define GXP_HINT_TEXTURE_UMA					33

