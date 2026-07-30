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
** $Log: 
**  1    GlideXP   1.0         12/14/01 Ryan Nunn       Hacked up macglide3.h
**       to get the sources to compile under Windows MSVC6.0
 */

#ifndef _WIN_GLIDE_H_
#define _WIN_GLIDE_H_

/* Note that GLIDE2 support is kindof a hack. I've basically just 
 * produced an internal wrapper from Glide2 calls into Glide3 calls
 *
 * As such, everything for normal Glide3 also needs to be defined.
 */

/* Enable the function translation */
//#define GLIDE2						1

/* Glide Global options */
#define GLIDE_LIB						1
#define GLIDE_PLUG						1
#define GLIDE_SPLASH					1
#define GLIDE3							1
#define GLIDE3_ALPHA					1
#define GLIDE3_VERTEX_LAYOUT			1
#define PCI_BUMP_N_GRIND				1
#define FIFO_WRITE_64					0

/* Required Glide 2 stuff */
#ifdef GLIDE2
#define GLIDE_POINTCAST_PALETTE			1
#endif

//#define GLIDE_HW_H3					1
#define HWC_EXT_INIT					1
#define H3								1
#define H4								1
#define FX_GLIDE_NAPALM					1

/* Glide Platform Options */
#define HAL_HW							1 
#define INIT_DOS						1
#define HAS_CONSOLE_IO					1
#define FX_DLL_ENABLE					1

#if !(GLIDE_USE_C_TRISETUP)
#define GL_AMD3D						1
#endif

/* Glide Debugging options */
#ifndef DEBUG
#define DEBUG							0
#endif
//#define GDBG_INFO_ON					1

#if DEBUG
#define GLIDE_DEBUG						1
#define GDBG_INFO_ON					1
#define GLIDE_USE_DEBUG_FIFO			0

//#define FIFO_ASSERT_FULL				0
//#define GLIDE_SANITY_SIZE				1	
//#define GLIDE_SANITY_ASSERT			1
#else /* !DEBUG */
#endif /* !DEBUG */

//#define GLIDE_USE_C_TRISETUP			1
#define GLIDE_FP_CLAMP					0
#define GLIDE_FP_CLAMP_TEX				0

/* Glide HW Options */
#define GLIDE_CHIP_BROADCAST			1
#define GLIDE_HW_TRI_SETUP				1
#define GLIDE_PACKET3_TRI_SETUP			1
//#define GLIDE_TRI_CULLING				1
#define GLIDE_PACKED_RGB				1
#define USE_PACKET_FIFO					1
#define GLIDE_INIT_HWC					1
#define HWC_ACCESS_DDRAW				1
#define HMONITOR_DECLARED				1

#define GLIDE_DEFAULT_GAMMA				1.3f

#define ENABLE_V3_W2K_GLIDE_CHANGES		1

// Remove this line to disable Alt Tab Support. There will be a speed up
#define GLIDE_ALT_TAB					1

// Remove this line to disable the WinXP Alt Tab fix. There will be a speed up
// Shouldn't use this anymore. Faster generally works better, in particular for SDL
// app. Doing it this way doesn't always detect vid mode switches
//#define WINXP_ALT_TAB_FIX				1

// Remove this line to disable the faster WinXP Alt Tab fix. There will be a minor speed up
#define WINXP_FASTER_ALT_TAB_FIX		1

// OpenGL dies if I don't do this
#define USE_OPENGL_HACK					1

//#define NEED_MSGFILE_ASSIGN				1

#endif /* _WIN_GLIDE_H_ */
