//
// Glide 2 to Glide 3 Translator for GlideXP
//
// g2sst.cpp : Glide 2 SST Functions
//
// Copyright (c) 2002 Ryan Nunn
//
// Just a note that this is written in C++ so I can use namespaces.
// No other C++ features are used.
//

#include "g2pch.h"

// The state structure
G2State theState;

// The non state info
NonStateInfo nsInfo;

namespace Glide3 {

grFinish_proc					grFinish = 0;
grSstOrigin_proc				grSstOrigin = 0;
grSstQueryBoards_proc			grSstQueryBoards = 0;
grSstQueryHardware_proc			grSstQueryHardware = 0;
grSstSelect_proc				grSstSelect = 0;
grSstWinOpen_proc				grSstWinOpen = 0;
grSstWinClose_proc				grSstWinClose = 0;
grSstWinOpenExt_proc			grSstWinOpenExt = 0;

bool	SetupSSTFunctions()
{
#define FN_NAME "SetupSSTFunctions"
    GDBG_INFO(80, "%s\n", FN_NAME);

	bool ret = true;


	if (ret) ret = LoadFunc(grFinish,0);
	if (ret) ret = LoadFunc(grSstOrigin,4);
	if (ret) ret = LoadFunc(grSstSelect,4);
	if (ret) ret = LoadFunc(grSstWinOpen,28);
	if (ret) ret = LoadFunc(grSstWinClose,4);

	// These funcs aren't normally exported by Glide 3. 
	// They are only available as extensions. At the moment with GlideXP
	// QUERY isn't properly supported, so the functions are not exported.
	// However, if I fix QUERY sometime in the future, we are ready to use it

	if (ret) {

		// Get Extensions
		//const char *extensions = grGetString(GR_EXTENSION);

		// These functions are exported if the 'QUERY' extension is supported
		//if (strstr(extensions, "QUERY"))
		{
			grSstQueryBoards = (grSstQueryBoards_proc) 
					Glide3::grGetProcAddress("grSstQueryBoards");

			grSstQueryHardware = (grSstQueryHardware_proc)
					Glide3::grGetProcAddress("grSstQueryHardware");
		}

		// Check for PIXEXT
		//if (strstr(extensions, "PIXEXT")) 
			grSstWinOpenExt = (grSstWinOpenExt_proc) Glide3::grGetProcAddress("grSstWinOpenExt");
	}

	return ret;
#undef FN_NAME
}

bool	FreeSSTFunctions()
{
#define FN_NAME "FreeSSTFunctions"
	//GDBG_INFO(80, "%s\n", FN_NAME);

	grFinish = 0;
	grSstSelect = 0;
	grSstWinOpen = 0;
	grSstWinClose = 0;
	grSstQueryBoards = 0;
	grSstQueryHardware = 0;

	return true;
#undef FN_NAME
}

};

//
// grSstOrigin
// 
NAKED_CALL void FX_CALL grSstOrigin (GrOriginLocation_t origin)
{
#define FN_NAME "grSstOrigin"
    GDBG_INFO(80, "%s\n", FN_NAME);

	using Glide3::grSstOrigin;
	VOID_ASM_JMP (grSstOrigin,(origin));

#undef FN_NAME
}

//
// grSstQueryBoards
// 
FxBool FX_CALL grSstQueryBoards ( GrHwConfiguration *hwc )
{
#define FN_NAME "grSstQueryBoards"
    GDBG_INFO(80, "%s\n", FN_NAME);

	// If we have QueryBoards just use it
	if (Glide3::grSstQueryBoards) return Glide3::grSstQueryBoards(hwc);

	// Otherwise, attempt to emulate the function here.
	FxI32 t = 0;
	if (Glide3::grGet) Glide3::grGet(GR_NUM_BOARDS, 4, &t);
	hwc->num_sst = t;
	return FXTRUE;

#undef FN_NAME
}

//
// grSstQueryHardware
// 
FxBool FX_CALL grSstQueryHardware ( GrHwConfiguration *hwc )
{
#define FN_NAME "grSstQueryHardware"
    GDBG_INFO(80, "%s\n", FN_NAME);

	// If we have the function, just use it
	if (Glide3::grSstQueryHardware) return Glide3::grSstQueryHardware(hwc);

	// Clear the entire buffer
	//memset (hwc, 0, sizeof (GrHwConfiguration));		

	// Ok, we don't have the function, so we need to make an implementation
	// outselves. First thing is to check the number of boards. If there were
	// none, fail.
	FxI32 t = 0;
	if (Glide3::grGet) Glide3::grGet(GR_NUM_BOARDS, 4, &t);


	// Set board count
	hwc->num_sst = t;									

	// No boards, no go
	if (!t) return FXFALSE;

	// The following code will make a dummy Voodoo 1 for each board.
	for (int board = 0; board < hwc->num_sst && board < MAX_NUM_SST; board++) {

		// To Save some typing
		GrVoodooConfig_t	&config = hwc->SSTs[board].sstBoard.VoodooConfig;

		// This is a little hack, we need to select the board to get it's info,
		// but it shouldn't matter much
		Glide3::grSstSelect(board);

		hwc->SSTs[board].type = GR_SSTTYPE_VOODOO;		// Set the board to a Voodoo

		// Number of TMUs
		Glide3::grGet(GR_NUM_TMU,4, &t);				// Get Number of TMUs
		if (nsInfo.environment.no_multitexture) t = 1;	// If no multitexture, set tmus to 1
		config.nTexelfx = t;							// Set the number of TMUs

		// FBI Revision
		Glide3::grGet(GR_REVISION_FB,4, &t);			// Get FBI Revision
		config.fbiRev = t;								// Set FBI Revision

		// FB Memory 
		Glide3::grGet(GR_MEMORY_FB,4, &t);				// Get FB Memory
		config.fbRam = t;								// Set FB Memory

		// No SLI
		config.sliDetect = FXFALSE;

		// TMUs
		FxI32 tmuRev, tmuMem;
		Glide3::grGet(GR_MEMORY_TMU,4, &tmuMem);		// Get TMU Memory
		Glide3::grGet(GR_REVISION_TMU,4, &tmuRev);		// Get TMU Revision

		// Loop through number of TMUS
		for (int tmu = 0; tmu < config.nTexelfx; tmu++) {
			config.tmuConfig[tmu].tmuRam = tmuMem;
			config.tmuConfig[tmu].tmuRev = tmuRev;
		}
	}

	// Make sure we leave this func with the proper sst selected
	Glide3::grSstSelect(nsInfo.current_sst);

	return FXTRUE;

#undef FN_NAME
}

//
// grSstSelect
// 
void FX_CALL grSstSelect( int which_sst )
{
#define FN_NAME "grSstSelect"
    GDBG_INFO(80, "%s (%i)\n", FN_NAME, which_sst);

	Glide3::grSstSelect(which_sst);
	nsInfo.current_sst = which_sst;

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
// grSstWinOpen
// 
FxBool FX_CALL grSstWinOpen(
          FxU32                hWnd,
          GrScreenResolution_t screen_resolution,
          GrScreenRefresh_t    refresh_rate,
          GrColorFormat_t      color_format,
          GrOriginLocation_t   origin_location,
          int                  nColBuffers,
          int                  nAuxBuffers)
{
#define FN_NAME "grSstWinOpen"
    GDBG_INFO(80, "%s\n", FN_NAME);

	// Now return fxtrue
	return grSstWinOpenExt(hWnd,
						screen_resolution,
          				refresh_rate,
          				color_format,
          				origin_location,
						GR_PIXFMT_RGB_565,
          				nColBuffers,
          				nAuxBuffers);
#undef FN_NAME
}

//
// grSstWinOpenExt
// 
FxBool FX_CALL grSstWinOpenExt(
		FxU32					hWnd,
		GrScreenResolution_t	screen_resolution,
		GrScreenRefresh_t		refresh_rate,
		GrColorFormat_t			color_format,
		GrOriginLocation_t		origin_location,
		GrPixelFormat_t			pixelformat,
		int						nColBuffers,
		int						nAuxBuffers)
{
#define FN_NAME "grSstWinOpenExt"
    GDBG_INFO(80, "%s\n", FN_NAME);

	// First reset the state
	theState.Reset();

	if (Glide3::grSstWinOpenExt)
	{
		theState.context = Glide3::grSstWinOpenExt(hWnd,
										screen_resolution,
          								refresh_rate,
          								color_format,
          								origin_location,
										pixelformat,
          								nColBuffers,
          								nAuxBuffers);
	}
	else
	{
		theState.context = Glide3::grSstWinOpen(hWnd,
										screen_resolution,
          								refresh_rate,
          								color_format,
          								origin_location,
          								nColBuffers,
          								nAuxBuffers);
	}

	// If unable to open, return FXFALSE
	if (!theState.context) return FXFALSE;

	// Set the correct Coordinate Space
	Glide3::grCoordinateSpace(GR_WINDOW_COORDS); 

	// Setup the Depth Buffer Extents for grBufferClear
	g2SetupDepthBufferExtents();

	// Setup the base grVertex info
	g2SetupGrVertex();

	// Setup the grVertex TMU info from the Hints
	g2SetupGrVertexTMUs();

	// Setup thte State Hack
	g2SetupStateHack();

	// Reset the NonStateInfo
	nsInfo.Reset();

	// Viewport size
	FxI32	viewport[4];
	Glide3::grGet(GR_VIEWPORT, 16, viewport);

	theState.clipwindowf_xmin = 0;
	theState.clipwindowf_ymin = 0;
	theState.clipwindowf_xmax = (float) viewport[2];
	theState.clipwindowf_ymax = (float) viewport[3];

	// Cached for grSstVRetraceOn.  Note this is the resolution the DRIVER
	// actually opened, so it already accounts for any override.
	nsInfo.screen_height = (FxU32) viewport[3];

	// grSstVRetraceOn has no Glide3 entry point to forward to -- Glide3 keeps
	// _grSstVRetraceOn as an internal FX_CSTYLE routine and does not export it
	// -- so it is synthesised from the beam position.  That query is optional,
	// so probe it once here rather than per call: grGet answers with the length
	// it wrote, i.e. 4 on success and 0 when it declines.
	{
		FxI32 probe = 0;
		nsInfo.have_video_position =
			(Glide3::grGet(GR_VIDEO_POSITION, 4, &probe) == 4) ? FXTRUE : FXFALSE;
	}

	// Get Number of TMUs
	if (!nsInfo.environment.no_multitexture)
		Glide3::grGet(GR_NUM_TMU,4, &nsInfo.num_tmus);				
	else {
		// If we have multitexture disabled, don't waste texture memory. Enable UMA mode
		Glide3::grEnable(GR_TEXTURE_UMA_EXT);
		nsInfo.num_tmus = 1;
	}


	// Now return fxtrue
	return FXTRUE;
#undef FN_NAME
}

//
// grSstWinClose
//
FxBool FX_CALL grSstWinClose( void )
{
#define FN_NAME "grSstWinClose"
    GDBG_INFO(80, "%s\n", FN_NAME);

	// Close the window
	FxBool ret = Glide3::grSstWinClose(theState.context);

	// Reset the internal state
	theState.Reset();

	// Return the result
	return ret;
#undef FN_NAME
}

//
// grSstControl
//
FxBool FX_CALL grSstControl( FxU32 code )
{
#define FN_NAME "grSstControl"
    GDBG_INFO(80, "%s\n", FN_NAME);

	if (code == GR_CONTROL_DEACTIVATE) {
		Glide3::grDisable(GR_PASSTHRU);
		return FXTRUE;
	}
	else if (code == GR_CONTROL_ACTIVATE ) {
		Glide3::grEnable(GR_PASSTHRU);
		return FXTRUE;
	}
	
	return FXTRUE;
#undef FN_NAME
}

//
// grSstControlMode
//
NAKED_CALL FxBool FX_CALL grSstControlMode( FxU32 code )
{
#define FN_NAME "grSstControlMode"
    GDBG_INFO(80, "%s\n", FN_NAME);

	ASM_JMP (grSstControl,(code));

#undef FN_NAME
}

//
// grSstIdle
//
NAKED_CALL void FX_CALL grSstIdle(void)
{
#define FN_NAME "grSstIdle"
    GDBG_INFO(80, "%s\n", FN_NAME);

	// grFinish is just grSstIdle renamed
	using Glide3::grFinish;
	VOID_ASM_JMP (grFinish,());

#undef FN_NAME
}

//
// grSstIsBusy
//
FxBool FX_CALL grSstIsBusy()
{
#define FN_NAME "grSstIdle"
    GDBG_INFO(80, "%s\n", FN_NAME);

	FxI32	ret;
	Glide3::grGet(GR_IS_BUSY,4,&ret);
	return ret==0?FXFALSE:FXTRUE;

#undef FN_NAME
}

#if 0
typedef struct GrSstPerfStats_s {
  FxU32  pixelsIn;              /* # pixels processed (minus buffer clears) */
  FxU32  chromaFail;            /* # pixels not drawn due to chroma key */ 
  FxU32  zFuncFail;             /* # pixels not drawn due to Z comparison */
  FxU32  aFuncFail;             /* # pixels not drawn due to alpha comparison */
  FxU32  pixelsOut;             /* # pixels drawn (including buffer clears) */
} GrSstPerfStats_t;
#endif

//
// grSstPerfStats
//
void FX_CALL grSstPerfStats (GrSstPerfStats_t *pStats)
{
#define FN_NAME "grSstPerfStats"
    GDBG_INFO(80, "%s\n", FN_NAME);

	Glide3::grGet(GR_STATS_PIXELS_IN, 4, &(pStats->pixelsIn));
	Glide3::grGet(GR_STATS_PIXELS_CHROMA_FAIL, 4, &(pStats->chromaFail));
	Glide3::grGet(GR_STATS_PIXELS_DEPTHFUNC_FAIL, 4, &(pStats->zFuncFail));
	Glide3::grGet(GR_STATS_PIXELS_AFUNC_FAIL, 4, &(pStats->aFuncFail));
	Glide3::grGet(GR_STATS_PIXELS_OUT, 4, &(pStats->pixelsOut));

#undef FN_NAME
}

//
// grSstResetPerfStats
//
void FX_CALL grSstResetPerfStats(void)
{
#define FN_NAME "grSstResetPerfStats"
    GDBG_INFO(80, "%s\n", FN_NAME);

	Glide3::grReset(GR_STATS_PIXELS);

#undef FN_NAME

}

//
// grSstScreenHeight
//
int FX_CALL grSstScreenHeight( void )
{
#define FN_NAME "grSstScreenHeight"
    GDBG_INFO(80, "%s\n", FN_NAME);

	// Viewport size
	FxI32	viewport[4];
	Glide3::grGet(GR_VIEWPORT, 16, viewport);
	return viewport[3];

#undef FN_NAME
}

//
// grSstScreenWidth
//
int FX_CALL grSstScreenWidth( void )
{
#define FN_NAME "grSstScreenWidth"
    GDBG_INFO(80, "%s\n", FN_NAME);

	// Viewport size
	FxI32	viewport[4];
	Glide3::grGet(GR_VIEWPORT, 16, viewport);
	return viewport[2];

#undef FN_NAME
}

//
// grSstStatus
//
FxU32 FX_CALL grSstStatus( void )
{
#define FN_NAME "grSstStatus"
    GDBG_INFO(80, "%s\n", FN_NAME);

	FxU32	ret = 0;

	// 5:0		PCI FIFO free space (0x3F=FIFO empty) 

	// 6		Vertical retrace (0=vertical retrace active; 1=vertical retrace inactive).
	//
	// Note the inversion, which this used to get backwards: the bit is CLEAR
	// while retrace is active, which is why 3dfx's own Glide2 answers
	// grSstVRetraceOn with `(status & SST_VRETRACE) == 0`
	// (H5/GLIDE/SRC/GSST.C:2759, and the shipped glide2x.dll does `test $0x40`
	// / `sete`).  So the bit is set when retrace is NOT on.
	if(!grSstVRetraceOn()) ret |= 1 << 6;

	// 7		Pixelfx graphics engine busy (0=engine idle; 1=engine busy)
	// 8		TMU busy (0=engine idle; 1=engine busy)
	// 9		Voodoo Graphics busy (0=idle; 1=busy)
	if (grSstIsBusy()) {
		ret |= 1 << 7;
		ret |= 1 << 8;
		ret |= 1 << 9;
	}

	// 11:10	Displayed buffer (0=buffer 0; 1=buffer 1; 2=auxiliary buffer; 3=reserved)

	// 27:12	Memory FIFO free space (0xFFFF=FIFO empty)

	// 30:28	Number of swap buffer commands pending
	ret |= grBufferNumPending() << 28;

	return ret;

#undef FN_NAME
}

//
// grSstVideoLine
//
FxU32 FX_CALL grSstVideoLine()
{
#define FN_NAME "grSstVideoLine"
    GDBG_INFO(80, "%s\n", FN_NAME);

	FxU32	ret;
	Glide3::grGet(GR_VIDEO_POSITION,4,&ret);
	return ret;

#undef FN_NAME
}

//
// grSstVRetraceOn
//
// "Is the beam in vertical retrace?"  3dfx's own Glide2 answers it from a
// status bit that stays asserted for the whole retrace interval:
//
//     H5/GLIDE/SRC/GSST.C:2751
//     return ((grSstStatus() & SST_VRETRACE) == 0);
//
// This used to compare grGet(GR_VIDEO_POSITION) against 0, which is true for
// exactly one scanline out of the vertical total -- the same question, a
// completely different duty cycle.
//
// It matters because games use this to pace themselves against the display.
// Ignition does exactly that: on its 2D-only screens it does not swap buffers
// at all, it spins here until retrace begins and then writes straight into the
// visible buffer (Ign_3dfx.exe 0x405E50, six callers).  With a one-scanline
// window that sync lands in the wrong place, and hand-drawn 2D tears while
// geometry, which goes through the FIFO, does not.
//
// The window is the retrace interval plus a small margin at the bottom of the
// visible area.  The margin is there for safety as much as fidelity: the
// caller is a spin loop, so a window the scanline counter could step over
// without us observing it would hang the game.  For the same reason an unknown
// screen height answers FXTRUE rather than risking a wait that never ends.
//
// Answering FXTRUE is always the safe answer here, because the caller is a spin
// loop -- see the three guards below.
FxBool FX_CALL grSstVRetraceOn()
{
#define FN_NAME "grSstVRetraceOn"
    GDBG_INFO(80, "%s\n", FN_NAME);

	static FxU32	lastPos = 0xFFFFFFFFu;
	static FxU32	stuck   = 0;

	FxI32	pos = 0;

	// 1. Nothing to compare against yet.
	if (!nsInfo.screen_height || !nsInfo.have_video_position) return FXTRUE;

	// 2. GR_VIDEO_POSITION is not a required Glide3 query and it is NOT
	//    answered by a stock glide3x: 3dfx's own code accepts plength 8 only
	//    and reports a constant 0,0 without ever reading the hardware, so a
	//    beam wait built on it spins forever.  grGet returns 0 when it declines
	//    to answer, and `pos` is then left untouched -- taking the return value
	//    on trust would be reading an uninitialised local.
	if (Glide3::grGet(GR_VIDEO_POSITION, 4, &pos) != 4) return FXTRUE;

	// 3. Belt and braces for a driver that answers but reports a frozen beam.
	//    A scanline lasts ~15us and this call costs an uncached register read,
	//    so a genuine wait never sees the same line this many times; a driver
	//    that is not tracking the beam degrades to "always in retrace" instead
	//    of hanging the game.
	if ((FxU32)pos == lastPos) {
		if (++stuck > 200000u) return FXTRUE;
	} else {
		lastPos = (FxU32)pos;
		stuck   = 0;
	}

	return ((FxU32)pos >= nsInfo.screen_height - (nsInfo.screen_height >> 4))
	           ? FXTRUE : FXFALSE;

#undef FN_NAME
}
