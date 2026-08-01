//
// Glide 2 to Glide 3 Translator for GlideXP
//
// g2state.cpp : State Handling
//
// Copyright (c) 2002 Ryan Nunn
//
// Just a note that this is written in C++ so I can use namespaces.
// No other C++ features are used.
//

#include "g2pch.h"
#include "gamefix.h"

const static char glideIdent[] = "@#%" "2.70 " VERSIONSTR "GXP";

namespace Glide3 {

grGlideInit_proc				grGlideInit = 0;
grGlideGetState_proc			grGlideGetState = 0;
grGlideShutdown_proc			grGlideShutdown = 0;
grGlideSetState_proc			grGlideSetState = 0;

bool	SetupGlideFunctions()
{
#define FN_NAME "SetupGlideFunctions"
    GDBG_INFO(80, "%s\n", FN_NAME);

	bool ret = true;

	if (ret) ret = LoadFunc(grGlideInit,0);
	if (ret) ret = LoadFunc(grGlideGetState,4);
	if (ret) ret = LoadFunc(grGlideShutdown,0);
	if (ret) ret = LoadFunc(grGlideSetState,4);

	return ret;
#undef FN_NAME
}

bool	FreeGlideFunctions()
{
#define FN_NAME "FreeGlideFunctions"
    //GDBG_INFO(80, "%s\n", FN_NAME);

	grGlideInit = 0;
	grGlideGetState = 0;
	grGlideShutdown = 0;
	grGlideSetState = 0;

	return true;
#undef FN_NAME
}

};

//
// grGlideInit
// 
void FX_CALL grGlideInit( void )
{
#define FN_NAME "grGlideInit"

	if (!Glide3::SetupAllFunctions()) {
		MessageBox(NULL, TEXT("Unable to load glide3x.dll"), TEXT("Glide2x Error!"), MB_ICONERROR|MB_OK);
		exit(-1);
		return;
	}

    GDBG_INFO(80, "%s\n", FN_NAME);

	// Per-game patches, second chance.
	//
	// DLL_PROCESS_ATTACH is the natural place, but it is not reliable here:
	// GTA2 pulls its video device in with LoadLibrary, and on Win9x
	// GetModuleHandle("DMAGlide.dll") still comes back NULL while that load is
	// in progress -- which is why the in-memory patch did nothing on hardware
	// while the identical on-disk patch worked.
	//
	// grGlideInit is a guaranteed-good hook instead: DMAGlide's Vid_Init_SYS
	// calls it as its very first Glide call, and only afterwards allocates and
	// fills the mode list we need to rewrite. By the time we are here the
	// module is fully loaded and its code is definitely reachable.
	//
	// GameFix_Apply is idempotent, so the DllMain call staying in place costs
	// nothing on platforms where it does work.
	//
	// Before that, tell it which resolution the screen is actually going to be.
	//
	// We ask the DRIVER rather than deciding for ourselves.  glide3x reads
	// FX_GLIDE_OVERRIDE_RESOLUTION in gpci.c:1439 and, when it is > 1, forces
	// that resolution in grSstWinOpen (gsst.c:1558).  So it is the one value
	// that determines the real screen size, and every per-game fix has to agree
	// with it or the projection is computed for the wrong screen.
	//
	// grGetRegistryOrEnvironmentStringExt is glide3x's own lookup, exported for
	// exactly this purpose -- its comment says it exists "so the spooky code for
	// finding the correct registry tweak path in 9x/NT/2K does not have to be
	// duplicated".  It checks the environment first, then HKCU and HKLM under
	// the driver's device key, so we read precisely what the driver will read.
	// Reimplementing that path here would be a second source of truth and would
	// drift.
	//
	// Safe to call now: SetupAllFunctions() above has already resolved it, and
	// G2misc falls back to plain getenv if the extension is missing.
	if (Glide3::grGetRegistryOrEnvironmentStringExt) {
		const char *res =
			Glide3::grGetRegistryOrEnvironmentStringExt("FX_GLIDE_OVERRIDE_RESOLUTION");
		if (res) GameFix_SetResolutionEnum((unsigned int)atoi(res));
	}

	GameFix_Apply();

	Glide3::grGlideInit();

	// Init our environment settings
	nsInfo.environment.Init();

	// Get Number of TMUs
	if (!nsInfo.environment.no_multitexture)
		Glide3::grGet(GR_NUM_TMU,4, &nsInfo.num_tmus);				
	else {
		// If we have multitexture disabled, don't waste texture memory. Enable UMA mode
		Glide3::grEnable(GR_TEXTURE_UMA_EXT);
		nsInfo.num_tmus = 1;
	}


#undef FN_NAME
}

//
// grGlideShutdown
// 
NAKED_CALL void FX_CALL grGlideShutdown( void )
{
#define FN_NAME "grGlideShutdown"
    GDBG_INFO(80, "%s\n", FN_NAME);

	using Glide3::grGlideShutdown;
	VOID_ASM_JMP (grGlideShutdown,());

#undef FN_NAME
}

//
// grGlideGetVersion
// 
void FX_CALL grGlideGetVersion (char version[80])
{
#define FN_NAME "grGlideGetVersion"
    GDBG_INFO(80, "%s\n", FN_NAME);

  GDBG_INFO(87,"grGlideGetVersion(0x%x) => \"%s\"\n",version,glideIdent+3);
  GR_ASSERT(version != NULL);
  strcpy(version,glideIdent+3);

#undef FN_NAME
}

//
// grGlideShamelessPlug
//
void FX_CALL grGlideShamelessPlug (const FxBool mode)
{
#define FN_NAME "grGlideShamelessPlug"
    GDBG_INFO(80, "%s: %i\n", FN_NAME, mode);

  if (mode) Glide3::grEnable(GR_SHAMELESS_PLUG);
  else Glide3::grDisable(GR_SHAMELESS_PLUG);

#undef FN_NAME
}

//
// grGlideGetState
// 
void FX_CALL grGlideGetState (void *state)
{
#define FN_NAME "grGlideGetState"
    GDBG_INFO(80, "%s: %08X\n", FN_NAME, state);

	StateBuffer *s = g2CreateStateBuffer(state);
    GDBG_INFO(80, "%s: shp=%08X\n", FN_NAME, s);

	Glide3::grGlideGetState(s->state);
    GDBG_INFO(80, "%s: g3 done\n", FN_NAME, s);

	s->state2 = theState;
    GDBG_INFO(80, "%s: g2 done\n", FN_NAME, s);

#undef FN_NAME
}

//
// grGlideSetState
// 
void FX_CALL grGlideSetState (const void *state)
{
#define FN_NAME "grGlideSetState"
    GDBG_INFO(80, "%s: %08X\n", FN_NAME, state);

	const StateBuffer *s = g2GetStateBuffer(state);
    GDBG_INFO(80, "%s: shp=%08X\n", FN_NAME, s);

	Glide3::grGlideSetState(s->state);
    GDBG_INFO(80, "%s: g3 done\n", FN_NAME, s);

	theState = s->state2;
    GDBG_INFO(80, "%s: g2 done\n", FN_NAME, s);

	// We need to do this because it's not really possible to know
	// if the correct table is loaded. But only if MultiTexture
	if (nsInfo.num_tmus > 1) {
		theState.nccTableActive[0] = -1;
		theState.nccTableActive[1] = -1;
		theState.texPaletteActive = -1;
	}

#undef FN_NAME
}
