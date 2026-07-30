/*
** THIS SOFTWARE IS SUBJECT TO COPYRIGHT PROTECTION AND IS OFFERED ONLY
** PURSUANT TO THE 3DFX GLIDE GENERAL PUBLIC LICENSE. THERE IS NO RIGHT
** TO USE THE GLIDE TRADEMARK WITHOUT PRIOR WRITTEN PERMISSION OF 3DFX
** INTERACTIVE, INC. A COPY OF THIS LICENSE MAY BE OBTAINED FROM THE 
** DISTRIBUTOR OR BY CONTACTING 3DFX INTERACTIVE INC(info@3dfx.com). 
** THIS PROGRAM IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND, EITHER 
** EXPRESSED OR IMPLIED. SEE THE 3DFX GLIDE GENERAL PUBLIC LICENSE FOR A
** FULL TEXT OF THE NON-WARRANTY PROVISIONS.  
** 
** USE, DUPLICATION OR DISCLOSURE BY THE GOVERNMENT IS SUBJECT TO
** RESTRICTIONS AS SET FORTH IN SUBDIVISION (C)(1)(II) OF THE RIGHTS IN
** TECHNICAL DATA AND COMPUTER SOFTWARE CLAUSE AT DFARS 252.227-7013,
** AND/OR IN SIMILAR OR SUCCESSOR CLAUSES IN THE FAR, DOD OR NASA FAR
** SUPPLEMENT. UNPUBLISHED RIGHTS RESERVED UNDER THE COPYRIGHT LAWS OF
** THE UNITED STATES.  
** 
** COPYRIGHT 3DFX INTERACTIVE, INC. 1999, ALL RIGHTS RESERVED
**
** $Header: /cvsroot/glide/glide3x/h5/glide3/src/diglide.c,v 1.3 2000/11/15 23:32:52 joseph Exp $
** $Log: 
**  7    GlideXP   1.1.4       03/16/02 Ryan Nunn       Fixing error in Glide2 state 
**       hack. Stops crashing on start up for Ultima IX and probably other games too.
**  6    GlideXP   1.1.3       12/20/01 Ryan Nunn       Fixing potential tiny memory
**       leak in the Glide2 state hack.
**  5    GlideXP   1.1.2       12/20/01 Ryan Nunn       Fixing potential problems in 
**       the glide2 State Hack
**  4    GlideXP   1.1.1       12/18/01 Ryan Nunn       Glide2 State Hack
**
**  3    3dfx      1.0.1.0.1.0 10/11/00 Brent           Forced check in to enforce
**       branching.
**  2    3dfx      1.0.1.0     06/20/00 Joseph Kain     Changes to support the
**       Napalm Glide open source release.  Changes include cleaned up offensive
**       comments and new legal headers.
**  1    3dfx      1.0         09/11/99 StarTeam VTS Administrator 
** $
** 
** 11    3/02/99 2:02p Peter
** cleaned up implicit select for grGlideinit
** 
** 10    2/18/99 3:39p Kcd
** Made grGetState definition match prototype.
** 
** 9     8/29/98 2:28p Peter
** tls allocation if hw exists
** 
** 8     8/03/98 6:37a Jdt
** multi-thread changes
** 
** 7     7/16/98 8:14a Jdt
** fxcmd.h
** 
** 6     6/09/98 11:59a Atai
** 1. update glide header
** 2. fix cull mode
** 3. fix tri stats
** 
** 4     4/22/98 4:57p Peter
** glide2x merge
** 
** 3     1/22/98 10:35a Atai
** 1. introduce GLIDE_VERSION, g3\glide.h, g3\glideutl.h, g2\glide.h,
** g2\glideutl.h
** 2. fixed grChromaRange, grSstOrigin, and grGetProcAddress
** 
** 2     1/19/98 8:00p Atai
** validate state in grGlideGetState()
 * 
 * 1     1/16/98 4:29p Atai
 * create glide 3 src
 * 
 * 33    1/07/98 10:22a Peter
 * lod dithering env var
 * 
 * 32    1/06/98 3:53p Atai
 * remove grHint, modify grLfbWriteRegion and grGet
 * 
 * 31    12/17/97 4:05p Atai
 * added grChromaRange(), grGammaCorrecionRGB(), grRest(), and grGet()
 * functions
 * 
 * 30    12/09/97 12:20p Peter
 * mac glide port
 * 
 * 29    12/01/97 5:46p Peter
 * fixed variable names in swizzle
 * 
 * 28    12/01/97 5:17p Peter
 * h3 simulator happiness
 * 
 * 27    11/18/97 4:36p Peter
 * chipfield stuff cleanup and w/ direct writes
 * 
 * 26    11/14/97 5:02p Peter
 * more comdex stuff
 * 
 * 25    11/14/97 12:09a Peter
 * comdex thing and some other stuff
 * 
 * 24    11/12/97 2:27p Peter
 * simulator happiness w/o fifo
 * 
 * 23    11/12/97 11:39a Dow
 * H3 Stuff
 * 
 * 22    11/12/97 9:21a Dow
 * Changed CVG_FIFO to USE_PACKET_FIFO
 * 
 * 21    11/04/97 4:00p Dow
 * Banshee Mods
 * 
 * 20    11/03/97 3:43p Peter
 * h3/cvg cataclysm
 * 
 * 19    10/16/97 3:40p Peter
 * packed rgb
 * 
 * 18    9/20/97 10:53a Peter
 * keep track of palette stats
 * 
 * 17    9/15/97 7:31p Peter
 * more cmdfifo cleanup, fixed normal buffer clear, banner in the right
 * place, lfb's are on, Hmmmm.. probably more
 * 
 * 16    9/10/97 10:13p Peter
 * fifo logic from GaryT, non-normalized fp first cut
 * 
 * 15    7/25/97 11:40a Peter
 * removed dHalf, change field name to match real use for cvg
 * 
 * 14    7/08/97 2:48p Peter
 * started playing w/ h3 sim
 * 
 * 13    6/30/97 3:20p Peter
 * error callback
 * 
 * 12    6/23/97 4:43p Peter
 * cleaned up #defines etc for a nicer tree
 * 
**
*/

#include <string.h>
#include <3dfx.h>
#include <glidesys.h>

#define FX_DLL_DEFINITION
#include <fxdll.h>
#include <glide.h>

#include "fxglide.h"
#include "fxcmd.h"

#include "gxpver.h"
static char glideIdent[] = "@#%" VERSIONSTR ;

/* the root of all EVIL */
struct _GlideRoot_s GR_CDECL _GlideRoot;
/* This is global to speed up the function call wrappers */

/*---------------------------------------------------------------------------
**
*/
void
_grDisplayStats(void)
{
  GR_DCL_GC;

  if (gc != NULL) {
    int frames = gc->stats.bufferSwaps;
    
    if (frames <= 0) frames = 1;
    gdbg_info(80,"GLIDE STATISTICS:\n");
    gdbg_info(80,"     triangles processed: %7d       tris drawn: %7d\n",
              gc->stats.trisProcessed,
              gc->stats.trisDrawn);
    gdbg_info(80,"            buffer swaps: %7d       tris/frame: %7d , %d\n",
              gc->stats.bufferSwaps,
              gc->stats.trisProcessed/frames,
              gc->stats.trisDrawn/frames);
    gdbg_info(80,"                  points: %7d       pnts/frame: %7d\n",
              gc->stats.pointsDrawn,
              gc->stats.pointsDrawn/frames);
    gdbg_info(80,"                   lines: %7d      lines/frame: %7d\n",
              gc->stats.linesDrawn,
              gc->stats.linesDrawn/frames);
    gdbg_info(80,"       texture downloads: %7d    texture bytes: %7d\n",
              gc->stats.texDownloads,  gc->stats.texBytes);
    gdbg_info(80,"       palette downloads: %7d    palette bytes: %7d\n",
              gc->stats.palDownloads, gc->stats.palBytes);
    gdbg_info(80,"           NCC downloads: %7d        NCC bytes: %7d\n",
              gc->stats.nccDownloads, gc->stats.nccBytes);
    
#if USE_PACKET_FIFO
    gdbg_info(80,"\tCommandFifo:\n");
    gdbg_info(80,"\t\tWraps: %ld\n", gc->stats.fifoWraps);
    if (gc->stats.fifoWraps > 0) {
      gdbg_info(80,"\t\tAvg Drain Depth: %g\n", 
                (double)gc->stats.fifoWrapDepth / gc->stats.fifoWraps);
    }
    gdbg_info(80,"\t\tStalls: %ld\n", gc->stats.fifoStalls);
    if (gc->stats.fifoStalls > 0) {
      gdbg_info(80,"\t\tAvg Stall Depth: %g\n", 
                (double)gc->stats.fifoStallDepth / gc->stats.fifoStalls);
    }
#endif /* USE_PACKET_FIFO */
  }
}

#if !USE_PACKET_FIFO
/*
** fifoFree is kept in bytes, each fifo entry is 8 bytes, but since there
** are headers involved, we assume an average of 2 registers per 8 bytes
** or 4 bytes of registers stored in every fifo entry
*/
void
_grReCacheFifo(FxI32 n)
{
#if !(GLIDE_PLATFORM & GLIDE_HW_H3)
  GR_DCL_GC;
  gc->state.fifoFree = ((grSstStatus() >> SST_MEMFIFOLEVEL_SHIFT) & 0xffff)<<2;

#if 0
  gc->state.fifoFree -= gc->hwDep.sst1Dep.swFifoLWM + n;
#endif
#endif
}

FxI32 GR_CDECL
_grSpinFifo(FxI32 n)
{
  GR_DCL_GC;
  do {
    _grReCacheFifo(n);
  } while (gc->state.fifoFree < 0);
  return gc->state.fifoFree;
}
#endif /* !USE_PACKET_FIFO */

/*---------------------------------------------------------------------------
**
*/
void 
_grSwizzleColor(GrColor_t *color)
{
  GR_DCL_GC;
  FxU32 red, green, blue, alpha;
  
  switch(gc->state.color_format) {
  case GR_COLORFORMAT_ARGB:
    break;

  case GR_COLORFORMAT_ABGR:
    red     = *color & 0x00ff;
    blue    = (*color >> 16) & 0xff;
    *color &= 0xff00ff00;
    *color |= ((red << 16) | blue);
    break;

  case GR_COLORFORMAT_RGBA:
    blue   = (*color & 0x0000ff00) >> 8;
    green  = (*color & 0x00ff0000) >> 16;
    red    = (*color & 0xff000000) >> 24;
    alpha  = (*color & 0x000000ff);
    *color = (alpha << 24) | (red << 16) | (green << 8) | blue;
    break;

  case GR_COLORFORMAT_BGRA:
    blue   = (*color & 0xff000000) >> 24;
    green  = (*color & 0x00ff0000) >> 16;
    red    = (*color & 0x0000ff00) >> 8;
    alpha  = (*color & 0x000000ff);
    *color = (alpha << 24) | (red << 16) | (green << 8) | blue;
    break;

  default:
    GR_ASSERT(0);
    break;
  }
} /* _grSwizzleColor */

/*---------------------------------------------------------------------------
** grGlideGetVersion
** NOTE: allow this to be called before grGlideInit()
*/
GR_DIENTRY(grGlideGetVersion, void, (char version[80]))
{
  GDBG_INFO(87,"grGlideGetVersion(0x%x) => \"%s\"\n",version,glideIdent+3);
  GR_ASSERT(version != NULL);
  strcpy(version,glideIdent+3);
} /* grGlideGetVersion */

/*---------------------------------------------------------------------------
** grGlideGetState
*/
GR_DIENTRY(grGlideGetState, void, (void *state))
{
  int state_size;
  GR_BEGIN_NOFIFOCHECK("grGlideGetState",87);
  GDBG_INFO_MORE(gc->myLevel,"(0x%x)\n",state);
  GR_ASSERT(state != NULL);
  state_size = sizeof( GrState);
  GDBG_INFO(120, "grGlideGetState:  %i bytes\n", state_size); //3016
  /* GR_ASSERT(sizeof(struct _GrState_s) == GLIDE_STATE_PAD_SIZE); */
#if defined(GLIDE3) && defined(GLIDE3_ALPHA)
  _grValidateState();
#endif
#ifndef GLIDE2
  *((GrState *)state) = gc->state;
#else
  /* Glide2 state hack */
   _G2SetStatePointer(state);
#endif
  GR_END();
} /* grGlideGetState */

#ifdef GLIDE2

/*---------------------------------------------------------------------------
**  _grSetupGrVertex - Setup the Glide3 GrVertex Emulation
*/
/*
typedef struct {
	float sow;
	float tow;
	float oow;
} GrTmuVertex;

/*
typedef struct {
	float x, y, z;
	float r, g, b;
	float ooz;
	float a;
	float oow;
	GrTmuVertex  tmuvtx[GLIDE_NUM_TMU];
} GrVertex;
*/

#define FOFS(a) ((FxI32)&(((GrVertex *)0)->a))
#define FOFS_TMU(tmu, a) ((FxI32)&(((GrVertex *)0)->tmuvtx[tmu].a))

void _grSetupGrVertex()
{
#define FN_NAME "_grSetupGrVertex"
  GR_BEGIN_NOFIFOCHECK(FN_NAME,85);

  grCoordinateSpace(GR_WINDOW_COORDS); 
  grVertexLayout(GR_PARAM_XY, 0, GR_PARAM_ENABLE); 
  grVertexLayout(GR_PARAM_RGB, FOFS(r), GR_PARAM_ENABLE);
  grVertexLayout(GR_PARAM_Z, FOFS(ooz), GR_PARAM_ENABLE); // Needed????
  grVertexLayout(GR_PARAM_A, FOFS(a), GR_PARAM_ENABLE);
  grVertexLayout(GR_PARAM_W, FOFS(oow), GR_PARAM_ENABLE);
  grVertexLayout(GR_PARAM_Q, FOFS(oow), GR_PARAM_ENABLE);

  /* TMU0 */
  grVertexLayout(GR_PARAM_ST0, FOFS_TMU(0,sow), GR_PARAM_ENABLE);
  grVertexLayout(GR_PARAM_Q0, FOFS_TMU(0,oow), GR_PARAM_ENABLE);

  /* TMU1 */
  grVertexLayout(GR_PARAM_ST1, FOFS_TMU(1,sow), GR_PARAM_ENABLE);
  grVertexLayout(GR_PARAM_Q1, FOFS_TMU(1,oow), GR_PARAM_ENABLE);

#undef FN_NAME
}

/*---------------------------------------------------------------------------
** grHints
*/
GR_DIENTRY(grHints, void, (GrHint_t hintType, FxU32 hints))
{
  GR_BEGIN_NOFIFOCHECK("grHints",85);
  GDBG_INFO_MORE(gc->myLevel,"(%d,0x%x)\n",hintType,hints);

  switch (hintType) {
  case GR_HINT_STWHINT:
    if (gc->state.paramHints != hints) {
      gc->state.paramHints = hints;
      _grUpdateParamIndex();
    }
    break;

  case GR_HINT_FIFOCHECKHINT:
    /* swFifoLWM is kept internally in bytes, hints are in fifo entries */ 
    //gc->state.checkFifo = hints;
    break;

  case GR_HINT_FPUPRECISION:
    //hints ? double_precision_asm() : single_precision_asm();
    break;

  case GR_HINT_ALLOW_MIPMAP_DITHER:
    /* Regardless of the game hint, force the user selection */
//    gc->state.allowLODdither = ((_GlideRoot.environment.texLodDither != 0) || 
//                                hints);
      if (hints) grEnable(GR_ALLOW_MIPMAP_DITHER);
      else grDisable(GR_ALLOW_MIPMAP_DITHER);
    break;
  
  default:
    GR_CHECK_F(myName, 1, "invalid hints type");
  }
  GR_END();
} /* grHints */
#endif

/*---------------------------------------------------------------------------
** grGlideInit
*/
GR_DIENTRY(grGlideInit, void, (void))
{
  GDBG_INIT();
  
  GDBG_INFO(80,"grGlideInit()\n");
  _GlideInitEnvironment();                      /* the main init code */
  FXUNUSED(*glideIdent);

#if GDBG_INFO_ON
  gdbg_error_set_callback(_grErrorCallback);
#endif

  if (_GlideRoot.initialized) {
    initThreadStorage();
    initCriticalSection();
    
    /* NB: We need to select the default device here so that grGetXXX
     * routines work before grSstWinOpen or the surface attachment
     * routines are called.  
     */
    grSstSelect(0);
  }

  _grResetTriStats();
  GDBG_INFO(281,"grGlideInit --done---------------------------------------\n");
} /* grGlideInit */


/*---------------------------------------------------------------------------
**  grGlideShamelessPlug - grGlideShamelessPlug
**
**  Returns:
**
**  Notes:
**
*/
#ifdef GLIDE2
GR_DIENTRY(grGlideShamelessPlug, void, (const FxBool mode))
{
  GDBG_INFO(80,"grGlideShamelessPlug(%d)\n",mode);
  _GlideRoot.environment.shamelessPlug = mode;
} /* grGlideShamelessPlug */
#endif


/*---------------------------------------------------------------------------
**  grResetTriStats - Set triangle counters to zero.
*/
void FX_CSTYLE
_grResetTriStats(void)
{
  GR_DCL_GC;
  GDBG_INFO(80,"grResetTriStats()\n");

  gc->stats.bufferSwaps = 0;
  gc->stats.linesDrawn = 0;
  gc->stats.trisProcessed = 0;
  gc->stats.trisDrawn = 0;
  gc->stats.texDownloads = 0;
  gc->stats.texBytes = 0;
  gc->stats.palDownloads = 0;
  gc->stats.palBytes = 0;
} /* grResetTriStats */

#ifdef GLIDE2
GR_DIENTRY(grResetTriStats, void, (void))
{
	_grResetTriStats();
}
#endif

/*---------------------------------------------------------------------------
**  grResetTriStats - Set triangle counters to zero.
*/
GR_DIENTRY(grTriStats, void, (FxU32 *trisProcessed, FxU32 *trisDrawn))
{
  GR_DCL_GC;
  GDBG_INFO(80,"grTriStats() => %d %d\n",
                gc->stats.trisProcessed,
                gc->stats.trisDrawn);
  *trisProcessed = gc->stats.trisProcessed;
  *trisDrawn = gc->stats.trisDrawn;
} /* grTriStats */

void GR_CDECL
_grFence(void)
{
  GDBG_INFO(120,"\t\t\t\t\t\t\tFENCE\n");
  P6FENCE;
}

