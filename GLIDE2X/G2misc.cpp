//
// Glide 2 to Glide 3 Translator for GlideXP
//
// g2misc.cpp : Misc Functions
//
// Copyright (c) 2002 Ryan Nunn
//
// Just a note that this is written in C++ so I can use namespaces.
// No other C++ features are used.
//

#include "g2pch.h"

namespace Glide3 {

grChromakeyMode_proc			grChromakeyMode = 0;
grChromakeyValue_proc			grChromakeyValue = 0;
grClipWindow_proc				grClipWindow = 0;
grCoordinateSpace_proc			grCoordinateSpace = 0;
grDisable_proc					grDisable = 0;
grDitherMode_proc				grDitherMode = 0;
grEnable_proc					grEnable = 0;
grGet_proc						grGet = 0;
grGetProcAddress_proc			grGetProcAddress = 0;
grGetRegistryOrEnvironmentStringExt_proc	grGetRegistryOrEnvironmentStringExt = 0;
grGetString_proc				grGetString = 0;
grReset_proc					grReset = 0;
grErrorSetCallback_proc			grErrorSetCallback = 0;
grSplash_proc					grSplash = 0;
grLoadGammaTable_proc			grLoadGammaTable = 0;

//
// grGetRegistryOrEnvironmentString (for if the extension isn't supported)
//
char* FX_CALL _grGetRegistryOrEnvironmentString (char* theEntry)
{
	// Just pass through to getenv
	return getenv(theEntry);
}

bool	SetupMiscFunctions()
{
#define FN_NAME "SetupMiscFunctions"
    GDBG_INFO(80, "%s\n", FN_NAME);

	bool ret = true;

	if (ret) ret = LoadFunc(grChromakeyMode,4);
	if (ret) ret = LoadFunc(grChromakeyValue,4);
	if (ret) ret = LoadFunc(grClipWindow,16);
	if (ret) ret = LoadFunc(grCoordinateSpace,4);
	if (ret) ret = LoadFunc(grDisable,4);
	if (ret) ret = LoadFunc(grDitherMode,4);
	if (ret) ret = LoadFunc(grEnable,4);
	if (ret) ret = LoadFunc(grGet,12);
	if (ret) ret = LoadFunc(grGetProcAddress,4);
	if (ret) ret = LoadFunc(grGetString,4);
	if (ret) ret = LoadFunc(grReset,4);
	if (ret) ret = LoadFunc(grErrorSetCallback,4);
	if (ret) ret = LoadFunc(grSplash,20);
	if (ret) ret = LoadFunc(grLoadGammaTable,16);

	// Extension time
	if (ret) {
		grGetRegistryOrEnvironmentStringExt = (grGetRegistryOrEnvironmentStringExt_proc)
				grGetProcAddress("grGetRegistryOrEnvironmentStringExt");

		// Wasn't found
		if (!grGetRegistryOrEnvironmentStringExt) grGetRegistryOrEnvironmentStringExt = _grGetRegistryOrEnvironmentString;
	}
	return ret;

#undef FN_NAME
}

bool	FreeMiscFunctions()
{
#define FN_NAME "FreeMiscFunctions"
	//GDBG_INFO(80, "%s\n", FN_NAME);

	grChromakeyMode = 0;
	grChromakeyValue = 0;
	grClipWindow = 0;
	grCoordinateSpace = 0;
	grDisable = 0;
	grDitherMode = 0;
	grEnable = 0;
	grGet = 0;
	grGetProcAddress = 0;
	grGetString = 0;
	grReset = 0;
	grErrorSetCallback = 0;
	grSplash = 0;
	grLoadGammaTable = 0;

	return true;

#undef FN_NAME
}

};

//
// grChromakeyMode
//
NAKED_CALL void FX_CALL grChromakeyMode( GrChromakeyMode_t mode )
{
#define FN_NAME "grChromakeyMode"
    GDBG_INFO(80, "%s\n", FN_NAME);

	using Glide3::grChromakeyMode;
	VOID_ASM_JMP (grChromakeyMode,(mode));

#undef FN_NAME
}

//
// grChromakeyValue
//
NAKED_CALL void FX_CALL grChromakeyValue( GrColor_t value )
{
#define FN_NAME "grChromakeyValue"
    GDBG_INFO(80, "%s\n", FN_NAME);

	using Glide3::grChromakeyValue;
	VOID_ASM_JMP (grChromakeyValue,(value));

#undef FN_NAME
}

//
// grClipWindow
//
void FX_CALL grClipWindow ( FxU32 minx, FxU32 miny, FxU32 maxx, FxU32 maxy )
{
#define FN_NAME "grClipWindow"
    GDBG_INFO(80, "%s\n", FN_NAME);

	Glide3::grClipWindow(minx, miny, maxx, maxy);

	theState.clipwindowf_xmin = (float) minx;
	theState.clipwindowf_xmax = (float) maxx;
	theState.clipwindowf_ymin = (float) miny;
	theState.clipwindowf_ymax = (float) maxy;

#undef FN_NAME
}

//
// grDitherMode
//
NAKED_CALL void FX_CALL grDitherMode( GrDitherMode_t mode )
{
#define FN_NAME "grDitherMode"
    GDBG_INFO(80, "%s\n", FN_NAME);

	using Glide3::grDitherMode;
	VOID_ASM_JMP (grDitherMode,(mode));

#undef FN_NAME
}

//
// grGammaCorrectionValue
//
void FX_CALL grGammaCorrectionValue (float gamma)
{
#define FN_NAME "grGammaCorrectionValue "
    GDBG_INFO(80, "%s\n", FN_NAME);

	Glide3::guGammaCorrectionRGB(gamma,gamma,gamma);

#undef FN_NAME
}

//
// grGammaCorrectionRGBExt
//
NAKED_CALL void FX_CALL grGammaCorrectionRGBExt (float red, float green, float blue)
{
#define FN_NAME "grGammaCorrectionRGB"
    GDBG_INFO(80, "%s\n", FN_NAME);

	using Glide3::guGammaCorrectionRGB;
	VOID_ASM_JMP (guGammaCorrectionRGB,(red,green,blue));

#undef FN_NAME
}

//
// grLoadGammaTableExt
//
void FX_CALL grLoadGammaTableExt( FxU32 red[256], FxU32 green[256], FxU32 blue[256])
{
#define FN_NAME "grGammaCorrectionRGB"
	Glide3::grLoadGammaTable(256,red,green,blue);
#undef FN_NAME
}


//
// single_precision_asm
//
// this routine sets the precision to single
// which effects all adds, mults, and divs
//
inline static void single_precision_asm(void)
{
#define FN_NAME "single_precision_asm"
    GDBG_INFO(80, "%s\n", FN_NAME);

  __asm
  {
    push  eax       ; make room
    fnclex          ; clear pending exceptions    
    fstcw WORD PTR [esp]
    mov   eax, DWORD PTR [esp]
    and   eax, 0000fcffh  ; clear bits 9:8
    mov   DWORD PTR [esp], eax
    fldcw WORD PTR [esp]
    pop   eax
  }

#undef FN_NAME
}

//
// double_precision_asm
//
// this routine sets the precision to double
// which effects all adds, mults, and divs
//
inline static void double_precision_asm(void)
{
#define FN_NAME "double_precision_asm"
    GDBG_INFO(80, "%s\n", FN_NAME);

  __asm {
    push  eax       ; make room
    fnclex          ; clear pending exceptions    
    fstcw WORD PTR [esp]
    mov   eax, DWORD PTR [esp]
    and   eax, 0000fcffh  ; clear bits 9:8
    or    eax, 000002ffh  ; set 9:8 to 10
    mov   DWORD PTR [esp], eax
    fldcw WORD PTR [esp]
    pop   eax
    ret   0
  }

#undef FN_NAME
}

//
// grHints
//
void FX_CALL grHints (GrHint_t hintType, FxU32 hints)
{
#define FN_NAME "grHints"
    GDBG_INFO(80, "%s: %i %i\n", FN_NAME, hintType, hints);

	switch (hintType) {
	case GR_HINT_STWHINT:
		if (theState.stwHints != hints) {
			theState.stwHints = hints;
			g2SetupGrVertexTMUs();
		}
		break;

	case GR_HINT_FPUPRECISION:
		hints ? double_precision_asm() : single_precision_asm();
		break;

	case GR_HINT_ALLOW_MIPMAP_DITHER:
		if (hints) Glide3::grEnable(GR_ALLOW_MIPMAP_DITHER);
		else Glide3::grDisable(GR_ALLOW_MIPMAP_DITHER);
		break;

	case GXP_HINT_AA_MULTI_SAMPLE:
		if (hints) Glide3::grEnable(GR_AA_MULTI_SAMPLE);
		else Glide3::grDisable(GR_AA_MULTI_SAMPLE);
		break;

	case GXP_HINT_TEXTURE_UMA:
		if (hints) Glide3::grEnable(GR_TEXTURE_UMA_EXT);
		else Glide3::grDisable(GR_TEXTURE_UMA_EXT);
		break;

	default:
		GR_CHECK_F(myName, 1, "invalid hints type");
	}

#undef FN_NAME
}

//
// grResetTriStats
//
void FX_CALL grResetTriStats(void)
{
#define FN_NAME "grResetTriStats"
    GDBG_INFO(80, "%s\n", FN_NAME);

	Glide3::grReset(GR_STATS_TRIANGLES);

#undef FN_NAME
}

//
// grErrorSetCallback
//
NAKED_CALL void FX_CALL grErrorSetCallback( GrErrorCallbackFnc_t fnc )
{
#define FN_NAME "grErrorSetCallback"
    GDBG_INFO(80, "%s\n", FN_NAME);

	using Glide3::grErrorSetCallback;
	VOID_ASM_JMP (grErrorSetCallback,(fnc));

#undef FN_NAME
}

//
// grSplash
//
NAKED_CALL void FX_CALL grSplash(float x, float y, float width, float height, FxU32 frame)
{
#define FN_NAME "grSplash"
    GDBG_INFO(80, "%s\n", FN_NAME);

	using Glide3::grSplash;
	VOID_ASM_JMP (grSplash,(x,y,width,height,frame));

#undef FN_NAME
}

//
// grTriStats
//
void FX_CALL grTriStats(FxU32 *trisProcessed, FxU32 *trisDrawn)
{
#define FN_NAME "grTriStats"
    GDBG_INFO(80, "%s\n", FN_NAME);

	Glide3::grGet(GR_STATS_TRIANGLES_IN, 4, (FxI32*) trisProcessed);
	Glide3::grGet(GR_STATS_TRIANGLES_OUT, 4, (FxI32*) trisDrawn);

#undef FN_NAME
}

//
// g2SetupGrVertex
//
#define FOFS(a) ((FxI32)&(((GrVertex *)0)->a))
#define FOFS_TMU(tmu, a) ((FxI32)&(((GrVertex *)0)->tmuvtx[tmu].a))

void g2SetupGrVertex()
{
#define FN_NAME "g2SetupGrVertex"
    GDBG_INFO(80, "%s\n", FN_NAME);

	// Base Vertex
	Glide3::grVertexLayout(GR_PARAM_XY,	 0,					GR_PARAM_ENABLE); 
	Glide3::grVertexLayout(GR_PARAM_Z,	 FOFS(ooz),			GR_PARAM_ENABLE); 
	Glide3::grVertexLayout(GR_PARAM_Q,	 FOFS(oow),			GR_PARAM_ENABLE);
	Glide3::grVertexLayout(GR_PARAM_W,	 FOFS(oow),			GR_PARAM_ENABLE);
	Glide3::grVertexLayout(GR_PARAM_RGB, FOFS(r),			GR_PARAM_ENABLE);
	Glide3::grVertexLayout(GR_PARAM_A,	 FOFS(a),			GR_PARAM_ENABLE);

	// Tmus
	Glide3::grVertexLayout(GR_PARAM_ST0, FOFS_TMU(0,sow),	GR_PARAM_ENABLE);
	Glide3::grVertexLayout(GR_PARAM_Q0,	 FOFS_TMU(0,oow),	GR_PARAM_DISABLE);
	Glide3::grVertexLayout(GR_PARAM_ST1, FOFS_TMU(1,sow),	GR_PARAM_DISABLE);
	Glide3::grVertexLayout(GR_PARAM_Q1,	 FOFS_TMU(1,oow),	GR_PARAM_DISABLE);
	Glide3::grVertexLayout(GR_PARAM_ST2, FOFS_TMU(2,sow),	GR_PARAM_DISABLE);
	Glide3::grVertexLayout(GR_PARAM_Q2,	 FOFS_TMU(2,oow),	GR_PARAM_DISABLE);

	theState.stwEnabled = GR_STWHINT_ST_DIFF_TMU0;

#undef FN_NAME
}

//
// SetupParam
//
inline void __fastcall SetupParam(FxU32 newEnabled, FxU32 hint, FxU32 param, FxU32 offset)
{
#define FN_NAME "SetupParam"
    GDBG_INFO(80, "%s: %i (%i %i) %i %i\n", FN_NAME, newEnabled&hint, newEnabled, hint, param, offset);

	if ((newEnabled & hint) != (theState.stwEnabled & hint)) {
		if (newEnabled & hint)
			Glide3::grVertexLayout(param, offset, GR_PARAM_ENABLE);
		else
			Glide3::grVertexLayout(param, offset, GR_PARAM_DISABLE);
	}

#undef FN_NAME
}

//
// g2SetupGrVertexTMUs
//
void g2SetupGrVertexTMUs()
{
#define FN_NAME "g2SetupGrVertexTMUs"
    GDBG_INFO(80, "%s\n", FN_NAME);

	FxU32 newEnabled = theState.stwHints;

	// If TMU0 is enabled force usage of ST0
	if (theState.tmuMask & GR_TMUMASK_TMU0)
		newEnabled |= GR_STWHINT_ST_DIFF_TMU0;

	// If TMU1 enabled force ST1 if there is no hint for TMU0
	if (theState.tmuMask & GR_TMUMASK_TMU1 && !(newEnabled & GR_STWHINT_ST_DIFF_TMU0))
		newEnabled |= GR_STWHINT_ST_DIFF_TMU1;

	// If TMU2 enabled force ST2 if there is no hint for TMU0 or TMU1
	if (theState.tmuMask & GR_TMUMASK_TMU2 && !(newEnabled & (GR_STWHINT_ST_DIFF_TMU0|GR_STWHINT_ST_DIFF_TMU1)))
		newEnabled |= GR_STWHINT_ST_DIFF_TMU2;

	// Need to change the state of ST0
	SetupParam(newEnabled, GR_STWHINT_ST_DIFF_TMU0, GR_PARAM_ST0, FOFS_TMU(0,sow));

	// Need to change the state of ST1
	SetupParam(newEnabled, GR_STWHINT_ST_DIFF_TMU1, GR_PARAM_ST1, FOFS_TMU(1,sow));

	// Need to change the state of ST2
	SetupParam(newEnabled, GR_STWHINT_ST_DIFF_TMU2, GR_PARAM_ST2, FOFS_TMU(2,sow));

	// Need to change the state of Q0
	SetupParam(newEnabled, GR_STWHINT_W_DIFF_TMU0, GR_PARAM_Q0, FOFS_TMU(0,oow));

	// Need to change the state of Q1
	SetupParam(newEnabled, GR_STWHINT_W_DIFF_TMU1, GR_PARAM_Q1, FOFS_TMU(1,oow));

	// Need to change the state of Q2
	SetupParam(newEnabled, GR_STWHINT_W_DIFF_TMU2, GR_PARAM_Q2, FOFS_TMU(2,oow));

	// Save what's been enabled
	theState.stwEnabled = newEnabled;

#undef FN_NAME
}

//
// Todo
//

//
// grCheckForRoom
//
void FX_CALL grCheckForRoom (FxI32 n)
{
#define FN_NAME "grCheckForRoom"
    GDBG_INFO(80, "%s\n", FN_NAME);
//  GR_DCL_GC;

  /* dpc - 13 sep 1997 - FixMe!
   * Setting one packet for now.
   */
//  GR_CHECK_FOR_ROOM(n, 1);
#undef FN_NAME
}


//
// grGetExt
//
FxU32 FX_CALL grGetExt (FxU32 pname, FxU32 plength, FxI32 *params)
{
	FxBool retVal = FXFALSE;

	switch(pname)
	{
	case GR_BITS_DEPTH:
		if (plength == 4)
		{
			retVal = Glide3::grGet(pname, plength, params);
		}
		break;

  case GR_BITS_RGBA:
		if (plength == 16)
		{
			retVal = Glide3::grGet(pname, plength, params);
		}
		break;

  case GR_MAX_TEXTURE_SIZE:
		if (plength == 4)
		{
			retVal = Glide3::grGet(pname, plength, params);
		}
		break;

  case GR_MAX_TEXTURE_ASPECT_RATIO:
		if (plength == 4)
		{
			retVal = Glide3::grGet(pname, plength, params);
		}
		break;

	default:
		retVal = FXFALSE;
		break;
	}

	return retVal;
}

//
// grGetStringExt
//
char * const extensions = " GETEXT GAMMA PIXFMT TEXFMT TEXBUF TEXUMA PARTIALROW ";

const char *FX_CALL grGetStringExt (FxU32 pname)
{
	const char *rv = "ERROR";

	switch(pname)
	{
	case GR_EXTENSION:
		{
			const char *g3_ext = Glide3::grGetString(pname);

			// Always support GETEXT and GAMMA
			strcpy (extensions, " GETEXT GAMMA ");

			// Glide 3 supports TEXFMT, so we can support TEXFMT
			if (strstr(g3_ext,"TEXFMT ")) 
				strcat (extensions, "TEXFMT ");

			// Glide 3 supports PIXEXT, so we can support PIXFMT
			if (strstr(g3_ext,"PIXEXT ")) 
				strcat (extensions, "PIXEXT ");

			// Glide 3 supports TEXTUREBUFFER, so we can support TEXTUREBUFFER
			if (strstr(g3_ext,"TEXTUREBUFFER ")) 
				strcat (extensions, "TEXBUF ");

			// Glide 3 supports TEXUMA, so we can support TEXUMA
			if (strstr(g3_ext,"TEXUMA ")) 
				strcat (extensions, "TEXUMA ");
			
			// Glide 3 supports PARTIALROW so we can support it too
			if (strstr(g3_ext,"PARTIALROW ")) 
				strcat (extensions, "PARTIALROW ");
			
			rv = extensions;
		}
		break;

	default:
		break;
	}

	return rv;
}


//
// grGetProcAddressExt - Extension to Glide2x
//
struct GrProcAddressTuple {
    const char *name;
    GrProc      proc;
};

// Disable the warning
#pragma warning (disable:4200)

// Can be addressed in 2 ways. Either by index, or by function name
// Don't ask me what the indices are, because I don't know
union GrProcAddresses
{
	struct
	{
		#include "g2exports.inl"

		GrProcAddressTuple _terminator; // The 'terminator'
	};
	
	// *** Non-standard extension *** //
	GrProcAddressTuple index[];	

} _functionTable;

// Renable the warning
#pragma warning (default:4200)

GrProc FX_CALL grGetProcAddressExtXP (char *procName)
{
#define FN_NAME "grGetProcAddress"
    GrProcAddressTuple *tuple;
    GrProc           rv;

    tuple = _functionTable.index;
    rv    = 0;

    while( tuple->name ) {
        if ( !strcmp( procName, tuple->name ) ) {
            rv = tuple->proc;
            break;
        }
        tuple++;
    }
    return rv;
#undef FN_NAME
} /* grGetProcAddress */

void SetupTuples()
{
#define ASSIGN_EXPORT_TUPPLES
	#include "g2exports.inl"
#undef ASSIGN_EXPORT_TUPPLES

	_functionTable._terminator.name = 0;
	_functionTable._terminator.proc = 0;
}
