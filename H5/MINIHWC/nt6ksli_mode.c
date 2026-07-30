/*
** THIS SOFTWARE IS SUBJECT TO COPYRIGHT PROTECTION AND IS OFFERED ONL
** PURSUANT TO THE 3DFX GLIDE GENERAL PUBLIC LICENSE. THERE IS NO RIGH
** TO USE THE GLIDE TRADEMARK WITHOUT PRIOR WRITTEN PERMISSION OF 3DF
** INTERACTIVE, INC. A COPY OF THIS LICENSE MAY BE OBTAINED FROM THE
** DISTRIBUTOR OR BY CONTACTING 3DFX INTERACTIVE INC(info@3dfx.com).
** THIS PROGRAM IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND, EITHER
** EXPRESSED OR IMPLIED. SEE THE 3DFX GLIDE GENERAL PUBLIC LICENSE FOR 
** FULL TEXT OF THE NON-WARRANTY PROVISIONS. 
**
** USE, DUPLICATION OR DISCLOSURE BY THE GOVERNMENT IS SUBJECT T
** RESTRICTIONS AS SET FORTH IN SUBDIVISION (C)(1)(II) OF THE RIGHTS I
** TECHNICAL DATA AND COMPUTER SOFTWARE CLAUSE AT DFARS 252.227-7013
** AND/OR IN SIMILAR OR SUCCESSOR CLAUSES IN THE FAR, DOD OR NASA FA
** SUPPLEMENT. UNPUBLISHED RIGHTS RESERVED UNDER THE COPYRIGHT LAWS O
** THE UNITED STATES. 
**
** COPYRIGHT 3DFX INTERACTIVE, INC. 1999, ALL RIGHTS RESERVE
**
** $Header: /cvsroot/glide/glide3x/h5/minihwc/dos_mode.c,v 1.3 2000/11/15 23:32:58 joseph Exp $
** $Log: 
**	9 	 3dfx 		 1.4.1.2.1.0 10/11/00 Brent 					Forced check in to enforce
**			 branching.
**	8 	 3dfx 		 1.4.1.2		 06/21/00 Joseph Kain 		Fixed errors introduced by
**			 my previous merge.
**	7 	 3dfx 		 1.4.1.1		 06/20/00 Joseph Kain 		Changes to support the
**			 Napalm Glide open source release.	Changes include cleaned up offensive
**			 comments and new legal headers.
**	6 	 3dfx 		 1.4.1.0		 05/09/00 Kenneth Dyke		Improved startup code for
**			 Glide.  Fixes cold startup problem.
**	5 	 3dfx 		 1.4				 04/25/00 Kenneth Dyke		Brough SLI/AA code for DOS
**			 a little more up to date.
**	4 	 3dfx 		 1.3				 03/28/00 Kenneth Dyke		Fixed missing parameter to
**			 setVideoMode.	C r0x.
**	3 	 3dfx 		 1.2				 03/28/00 Kenneth Dyke		Refinements to memory
**			 layout for DOS glide setup.
**	2 	 3dfx 		 1.1				 03/27/00 Kenneth Dyke		DOS Glide support for
**			 two-chip (and maybe four-chip) Napalm boards.
**	1 	 3dfx 		 1.0				 09/11/99 StarTeam VTS Administrator 
** $
** 
** 4		 10/08/98 10:14a Dow
** Fixes 512x384 sometimes
** 
** 3		 9/02/98 1:34p Peter
** watcom warnings
** 
** 2		 6/25/98 7:40p Dow
** Made it compile
** 
*/

#ifndef GDBG_INFO_ON
#define GDBG_INFO_ON 0
#endif

#if !defined(GDBG_INFO_ON) || (GDBG_INFO_ON == 0)
#if defined(GDBG_INFO_ON)
#undef GDBG_INFO_ON
#endif /* defined(GDBG_INFO_ON) */
#define GDBG_INFO_ON
#endif /* !defined(GDBG_INFO_ON) || (GDBG_INFO_ON == 0) */

#include <stddef.h>
#include <3dfx.h>
#include <gdebug.h>
#include <fxpci.h>
#include <h3cinit.h>
#include <h3regs.h>
#include <minihwc.h>
#include "hwcio.h"

#define CFG_READ(_chip, _offset) \
hwcReadConfigRegister(bInfo, _chip, offsetof(SstPCIConfigRegs, _offset))

#define CFG_WRITE(_chip, _offset, _value) \
hwcWriteConfigRegister(bInfo, _chip, offsetof(SstPCIConfigRegs, _offset), (_value))


/* This assumes that the slave has been mapped in already. */
void
initSlave(hwcBoardInfo *bInfo, FxU32 chipNum)
{
	FxU32 pciInit0, pllCtrl1;
	FxU32 dramInit0, dramInit1;
	FxU32 miscInit0, miscInit1;
	FxU32 tmuGbeInit;
	FxU32 cmdStatus;
	
	GDBG_INFO(80, "InitSlave(%d)\n",chipNum);
		
	/* Copy over pll & memory timings, etc. */
	HWC_IO_LOAD(bInfo->regInfo, pllCtrl1, pllCtrl1);
	HWC_IO_STORE_SLAVE(chipNum, bInfo->regInfo, pllCtrl1, pllCtrl1);
	HWC_IO_LOAD(bInfo->regInfo, dramInit0, dramInit0);
	HWC_IO_STORE_SLAVE(chipNum, bInfo->regInfo, dramInit0, dramInit0);
	HWC_IO_LOAD(bInfo->regInfo, dramInit1, dramInit1);
	HWC_IO_STORE_SLAVE(chipNum, bInfo->regInfo, dramInit1, dramInit1);
	HWC_IO_LOAD(bInfo->regInfo, pciInit0, pciInit0);
	HWC_IO_STORE_SLAVE(chipNum, bInfo->regInfo, pciInit0, pciInit0);
	HWC_IO_LOAD(bInfo->regInfo, miscInit0, miscInit0);
	miscInit0 &= ~BIT(30);
	HWC_IO_STORE(bInfo->regInfo, miscInit0, miscInit0);
	HWC_IO_STORE_SLAVE(chipNum, bInfo->regInfo, miscInit0, miscInit0);
	HWC_IO_LOAD(bInfo->regInfo, miscInit1, miscInit1);
	HWC_IO_STORE_SLAVE(chipNum, bInfo->regInfo, miscInit1, miscInit1);
	HWC_IO_LOAD(bInfo->regInfo, tmuGbeInit, tmuGbeInit);
	HWC_IO_STORE_SLAVE(chipNum, bInfo->regInfo, tmuGbeInit, tmuGbeInit);
	
	/* Init DRAM mode stuff */
	HWC_IO_STORE_SLAVE(chipNum, bInfo->regInfo, dramData, 0x000000037);
	HWC_IO_STORE_SLAVE(chipNum, bInfo->regInfo, dramCommand, 0x10d);
	
	/* Disable master IO */
	cmdStatus = CFG_READ(0, status_command);
	cmdStatus &= ~1;
	CFG_WRITE(0, status_command, cmdStatus);
	/* Enable slave IO */
	cmdStatus = CFG_READ(chipNum, status_command);
	cmdStatus |= 1;
	CFG_WRITE(chipNum, status_command, cmdStatus);
	
	/* Reset everything */
	//h3InitResetAll(bInfo->regInfo.slaveIOBase[chipNum-1]);
	
	/* Init VGA (no legacy decode) */
	// h3InitVga(bInfo->regInfo.slaveIOPortBase[chipNum-1], FXFALSE);
	
	/* Disable slave IO */
	cmdStatus = CFG_READ(chipNum, status_command);
	cmdStatus &= ~1;
	CFG_WRITE(chipNum, status_command, cmdStatus);
	/* Enable master IO */
	cmdStatus = CFG_READ(0, status_command);
	cmdStatus |= 1;
	CFG_WRITE(0, status_command, cmdStatus);
	
	{ 
		FxU32 status, vgaInit0, vgaInit1;
		HWC_IO_LOAD_SLAVE(chipNum, bInfo->regInfo, status, status);
		HWC_IO_LOAD_SLAVE(chipNum, bInfo->regInfo, vgaInit0, vgaInit0);
		HWC_IO_LOAD_SLAVE(chipNum, bInfo->regInfo, vgaInit1, vgaInit1);
		GDBG_INFO(80, "initSlave(%d) done.  slave status: %08lx vgaInit0: %08lx vgaInit1: %08lx\n",chipNum, status, vgaInit0, vgaInit1);
	}
}

static FxU8 vgaattr[] = {0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x01, 0x00, 0x0F, 0x00};

/* The standard mode table has 24 entries, but the first 3 are x, y and refresh. 
* We don't care about those here.	The default values (0-21) are just to serve
* as a reference for the comments... they will be blown away the first time we
* fill out the table. */
static FxU16 modeData[21] =
{ 
	0,			/* CRTC (0xd4) Index 0x00 - Horizontal Total */ 		 
	1,			/* CRTC (0xd4) Index 0x01 - Horizontal Display Enable End */
	2,			/* CRTC (0xd4) Index 0x02 - Start Horizontal Blanking */
	3,			/* CRTC (0xd4) Index 0x03 - End Horizontal Blanking */
	4,			/* CRTC (0xd4) Index 0x04 - Start Horizontal Sync */
	5,			/* CRTC (0xd4) Index 0x05 - End Horzontal Sync */
	6,			/* CRTC (0xd4) Index 0x06 - Vertical Total */
	7,			/* CRTC (0xd4) Index 0x07 - Overflow */
	8,			/* CRTC (0xd4) Index 0x09 - Maximum Scan Line */
	9,			/* CRTC (0xd4) Index 0x10 - Vertical Retrace Start */
	10,			/* CRTC (0xd4) Index 0x11 - Vertical Retrace End */
	11,			/* CRTC (0xd4) Index 0x12 - Vertical Display Enable End */
	12,			/* CRTC (0xd4) Index 0x15 - Start Vertical Blank */
	13,			/* CRTC (0xd4) Index 0x16 - End Vertical Blank */
	14,			/* CRTC (0xd4) Index 0x1a - Horizontal Extension Register */
	15,			/* CRTC (0xd4) Index 0x1b - Vertical Extension Register */
	16,			/* Miscellaneous Output (Written to port 0xc2, but read from 0xcc) */
	17,			/* Sequencer (0xc4) Index 0x01 */
	18,			/* pllCtrl0 - Low byte */
	19,			/* pllCtrl0 - High byte */
	20 			/* 2X Mode */
};


#define GET_CRTC_INDEX(_srcindex, _dstindex) \
	HWC_IO_STORE8(bInfo->regInfo, 0x0c4, _srcindex); \
	HWC_IO_LOAD8(bInfo->regInfo, 0x0d5, modeData[_dstindex]); 

void 
buildVideoModeData(hwcBoardInfo *bInfo)
{
	/* Snarf all VGA data we need from the master */
	FxU32 pllCtrl0, dacMode;
	
	GDBG_INFO(80, "buildVideoModeData()\n");
	
	GDBG_INFO(80, "buildVideoModeData(): %i.\n", __LINE__);
	
	/* CRTC values */
	GET_CRTC_INDEX(0x00, 0);
	GET_CRTC_INDEX(0x01, 1);
	GET_CRTC_INDEX(0x02, 2);
	GET_CRTC_INDEX(0x03, 3);
	GET_CRTC_INDEX(0x04, 4);
	GET_CRTC_INDEX(0x05, 5);
	GET_CRTC_INDEX(0x06, 6);
	GET_CRTC_INDEX(0x07, 7);
	GET_CRTC_INDEX(0x09, 8);
	GET_CRTC_INDEX(0x10, 9);
	GET_CRTC_INDEX(0x11, 10);
	GET_CRTC_INDEX(0x12, 11);
	GET_CRTC_INDEX(0x15, 12);
	GET_CRTC_INDEX(0x16, 13);
	GET_CRTC_INDEX(0x1a, 14);
	GET_CRTC_INDEX(0x1b, 15);
	
	GDBG_INFO(80, "buildVideoModeData(): %i\n", __LINE__);
	
	/* Miscellaneous output */
	HWC_IO_LOAD8(bInfo->regInfo, 0x0cc, modeData[16]);
	
	GDBG_INFO(80, "buildVideoModeData(): %i\n", __LINE__);
	
	/* Sequencer */
	HWC_IO_STORE8(bInfo->regInfo, 0x0c4, 0x01);
	HWC_IO_LOAD8(bInfo->regInfo, 0x0c5, modeData[17]);
	
	GDBG_INFO(80, "buildVideoModeData(): %i\n", __LINE__);
	
	/* pllCtrl0 */
	HWC_IO_LOAD(bInfo->regInfo, pllCtrl0, pllCtrl0);
	modeData[18] = (FxU16)(pllCtrl0 & 0xFF);
	modeData[19] = (FxU16)((pllCtrl0 >> 8) & 0xFF);

	/* 2X Mode */
	HWC_IO_LOAD(bInfo->regInfo, dacMode, dacMode);
	modeData[20] = (FxU16) dacMode;
	
	GDBG_INFO(80, "buildVideoModeData(): %i\n", __LINE__);
	
	{
		FxU32 i;
		for(i = 0; i < 21; i++) {
			GDBG_INFO(80, "modeData[%d]: %02lx\n",i,modeData[i]);
		}
	}  
}  

void
setVideoModeSlave(hwcBoardInfo *bInfo, FxU32 chipNum)
{
	FxU16 i;
	FxU8 garbage;
	FxU32 vidProcCfg, pllCtrl0, dacMode;
	FxU16 *rs = modeData;
	
	//
	// MISC REGISTERS
	// This register gets programmed first since the Mono/ Color
	// selection needs to be made.
	//
	// Sync polarities
	// Also force the programmable clock to be used with bits 3&2
	//
	GDBG_INFO(80, "setVideoModeSlave(%i)\n",chipNum);
	HWC_IO_LOAD8(bInfo->regInfo, 0xc3, garbage);
	GDBG_INFO(80, "motherboard enable: %02lx\n",garbage);
	
	HWC_IO_STORE8_SLAVE(chipNum, bInfo->regInfo, 0xc2, rs[16] | BIT(0));
	
	//
	// CRTC REGISTERS
	// First Unlock CRTC, then program them 
	//
	
	// Mystical VGA Magic
	HWC_IO_STORE16_SLAVE(chipNum, bInfo->regInfo, 0x0d4,  0x11);
	HWC_IO_STORE16_SLAVE(chipNum, bInfo->regInfo, 0x0d4,  (rs[0] << 8) | 0x00);
	HWC_IO_STORE16_SLAVE(chipNum, bInfo->regInfo, 0x0d4,  (rs[1] << 8) | 0x01);
	HWC_IO_STORE16_SLAVE(chipNum, bInfo->regInfo, 0x0d4,  (rs[2] << 8) | 0x02);
	HWC_IO_STORE16_SLAVE(chipNum, bInfo->regInfo, 0x0d4,  (rs[3] << 8) | 0x03);
	HWC_IO_STORE16_SLAVE(chipNum, bInfo->regInfo, 0x0d4,  (rs[4] << 8) | 0x04);
	HWC_IO_STORE16_SLAVE(chipNum, bInfo->regInfo, 0x0d4,  (rs[5] << 8) | 0x05);
	HWC_IO_STORE16_SLAVE(chipNum, bInfo->regInfo, 0x0d4,  (rs[6] << 8) | 0x06);
	HWC_IO_STORE16_SLAVE(chipNum, bInfo->regInfo, 0x0d4,  (rs[7] << 8) | 0x07);
	HWC_IO_STORE16_SLAVE(chipNum, bInfo->regInfo, 0x0d4,  (rs[8] << 8) | 0x09);
	HWC_IO_STORE16_SLAVE(chipNum, bInfo->regInfo, 0x0d4,  (rs[9] << 8) | 0x10);
	HWC_IO_STORE16_SLAVE(chipNum, bInfo->regInfo, 0x0d4,  (rs[10] << 8) | 0x11);
	HWC_IO_STORE16_SLAVE(chipNum, bInfo->regInfo, 0x0d4,  (rs[11] << 8) | 0x12);
	HWC_IO_STORE16_SLAVE(chipNum, bInfo->regInfo, 0x0d4,  (rs[12] << 8) | 0x15);
	HWC_IO_STORE16_SLAVE(chipNum, bInfo->regInfo, 0x0d4,  (rs[13] << 8) | 0x16);
	HWC_IO_STORE16_SLAVE(chipNum, bInfo->regInfo, 0x0d4,  (rs[14] << 8) | 0x1a);
	HWC_IO_STORE16_SLAVE(chipNum, bInfo->regInfo, 0x0d4,  (rs[15] << 8) | 0x1b);
	
	//
	// Enable Sync Outputs
	//
	HWC_IO_STORE16_SLAVE(chipNum, bInfo->regInfo, 0x0d4, (0x80 << 8) | 0x17);
	
	//
	// VIDCLK (32 bit access only!)
	// Set the Video Clock to the correct frequency
	//
	pllCtrl0 = (rs[19] << 8) | rs[18];
	HWC_IO_STORE_SLAVE(chipNum, bInfo->regInfo, pllCtrl0, pllCtrl0);
	
	//
	// dacMode (32 bit access only!)
	// (sets up 1x mode or 2x mode)
	//
	dacMode = rs[20];
	HWC_IO_STORE_SLAVE(chipNum, bInfo->regInfo, dacMode, dacMode);
	
	//
	// the 1x / 2x bit must also be set in vidProcConfig to properly
	// enable 1x / 2x mode
	//
	HWC_IO_LOAD_SLAVE(chipNum, bInfo->regInfo, vidProcCfg, vidProcCfg);
	vidProcCfg &= ~(SST_VIDEO_2X_MODE_EN | SST_HALF_MODE);
	if (rs[20]) vidProcCfg |= SST_VIDEO_2X_MODE_EN;
	HWC_IO_STORE_SLAVE(chipNum, bInfo->regInfo, vidProcCfg, vidProcCfg);
	
	//
	// SEQ REGISTERS
	// set run mode in the sequencer (not reset)
	//
	// make sure bit 5 == 0 (i.e., screen on)
	//
	HWC_IO_STORE16_SLAVE(chipNum, bInfo->regInfo, 0x0c4, ((rs[17] & ((FxU16) ~BIT(5))) << 8) | 0x1 );
	HWC_IO_STORE16_SLAVE(chipNum, bInfo->regInfo, 0x0c4, ( 0x3 << 8) | 0x0 );
	
	//
	// turn off VGA's screen refresh, as this function only sets extended
	// video modes, and the VGA screen refresh eats up performance
	// (10% difference in screen to screen blits!). 	This code is not in
	// the perl, but should stay here unless specifically decided otherwise
	//
	{
		FxU32 vgaInit0;
		HWC_IO_LOAD_SLAVE(chipNum, bInfo->regInfo, vgaInit0, vgaInit0);
		HWC_IO_STORE_SLAVE(chipNum, bInfo->regInfo, vgaInit0, vgaInit0|BIT(12)); 
	}
	
	//
	// Make sure attribute index register is initialized
	//
	HWC_IO_LOAD8_SLAVE(chipNum, bInfo->regInfo, 0x0da, garbage); // return value not used
	
	for (i = 0; i <= 19; i++)
	{
		HWC_IO_STORE8_SLAVE(chipNum, bInfo->regInfo, 0xc0, (FxU8) i);
		HWC_IO_STORE8_SLAVE(chipNum, bInfo->regInfo, 0xc0, (FxU8) vgaattr[i]);
	}
	
	HWC_IO_STORE8_SLAVE(chipNum, bInfo->regInfo, 0xc0, 0x34);
	
	HWC_IO_STORE8_SLAVE(chipNum, bInfo->regInfo, 0xda, 0);
	
	GDBG_INFO(80, "setVideoModeSlave(%i) done\n",chipNum);
	
} // h3InitSetVideoMode

/* This code is mostly based on the Win9x sliaa.c code. */
void hwcSetSLIAAMode(hwcBoardInfo *bInfo,
					 FxU32 sliEnable,
					 FxU32 aaEnable,
					 FxU32 analogSLI,
					 FxU32 sliBandHeight,
					 FxU32 totalMem,
					 FxU32 numChips,
					 FxU32 aaSampleHigh,
					 FxU32 aaColorBuffStart,
					 FxU32 aaDepthBuffStart,
					 FxU32 aaDepthBuffEnd,
					 FxU32 bpp)
{
	FxU32 chipNum, temp;
	FxU32 numChipsLog2;
	//FxU32 memBase0, memBase1;
	FxU32 sliBandHeightLog2, sliRenderMask, sliCompareMask, sliScanMask;
	FxU32 cmdStatus;
	
	GDBG_INFO(80, "hwcSetSLIAAMode() begin\n");
	
	/* Enable AA and/or SLI */
	if(sliEnable || aaEnable) {
		//#if 0
		/* First, init all chips */
		for(chipNum = 1; chipNum < numChips; chipNum++) {
			initSlave(bInfo, chipNum);
		}
		
		/* Grab video mode data from master. */
		buildVideoModeData(bInfo);
		
		/* Now set up video modes */
		for(chipNum = 1; chipNum < numChips; chipNum++) {
			
			/* Disable master IO */
			cmdStatus = CFG_READ(0, status_command);
			cmdStatus &= ~1;
			CFG_WRITE(0, status_command, cmdStatus);
			/* Enable slave IO */
			cmdStatus = CFG_READ(chipNum, status_command);
			cmdStatus |= 1;
			CFG_WRITE(chipNum, status_command, cmdStatus);
			
		//	setVideoModeSlave(bInfo, chipNum);
			
			/* Disable slave IO */
			cmdStatus = CFG_READ(chipNum, status_command);
			cmdStatus &= ~1;
			CFG_WRITE(chipNum, status_command, cmdStatus);
			/* Enable master IO */
			cmdStatus = CFG_READ(0, status_command);
			cmdStatus |= 1;
			CFG_WRITE(0, status_command, cmdStatus);
			
			/* Copy over video processor config registers */
			HWC_IO_LOAD(bInfo->regInfo, vidScreenSize, temp);
			HWC_IO_STORE_SLAVE(chipNum, bInfo->regInfo, vidScreenSize, temp);
			
			/* Make sure video processor is reset so the size change takes effect. */
			HWC_IO_LOAD(bInfo->regInfo, vidProcCfg, temp);
			HWC_IO_STORE_SLAVE(chipNum, bInfo->regInfo, vidProcCfg, temp & ~1);
			HWC_IO_STORE_SLAVE(chipNum, bInfo->regInfo, vidProcCfg, temp | 1);
			
			/* Note: We don't set any start addresses here because it's done as part 
			* of the 3D init stuff... */
			HWC_IO_LOAD(bInfo->regInfo, vidOverlayStartCoords, temp);
			HWC_IO_STORE_SLAVE(chipNum, bInfo->regInfo, vidOverlayStartCoords, temp);
			
			HWC_IO_LOAD(bInfo->regInfo, vidOverlayEndScreenCoord, temp);
			HWC_IO_STORE_SLAVE(chipNum, bInfo->regInfo, vidOverlayEndScreenCoord, temp);
			
			HWC_IO_LOAD(bInfo->regInfo, vidOverlayDudxOffsetSrcWidth, temp);
			HWC_IO_STORE_SLAVE(chipNum, bInfo->regInfo, vidOverlayDudxOffsetSrcWidth, temp);
			
			HWC_IO_LOAD(bInfo->regInfo, vidDesktopOverlayStride, temp);
			HWC_IO_STORE_SLAVE(chipNum, bInfo->regInfo, vidDesktopOverlayStride, temp);
			
			HWC_IO_LOAD(bInfo->regInfo, vidOverlayDudx, temp);
			HWC_IO_STORE_SLAVE(chipNum, bInfo->regInfo, vidOverlayDudx, temp);
			
			HWC_IO_LOAD(bInfo->regInfo, vidMaxRGBDelta, temp);
			HWC_IO_STORE_SLAVE(chipNum, bInfo->regInfo, vidMaxRGBDelta, temp);
		}  
		
		/* Calculate Log2 of the SLI band height */
		switch (sliBandHeight) {
		case	2: sliBandHeightLog2 = 1; 
			break;
		case	4: sliBandHeightLog2 = 2; 
			break;
		case	8: sliBandHeightLog2 = 3; 
			break;
		case 16: sliBandHeightLog2 = 4; 
			break;
		case 32: sliBandHeightLog2 = 5; 
			break;
		case 64: sliBandHeightLog2 = 6; 
			break;
		case 128: sliBandHeightLog2 = 7; 
			break;
		}
		
		GDBG_INFO(80, "sliBandHeight: %d  log2: %d\n",sliBandHeight, sliBandHeightLog2);
		
		/* Calculate Log2 of the number of chips */
		switch(numChips) {
		case 1: numChipsLog2 = 0;
			break;
		case 2: numChipsLog2 = 1;
			break;
		case 4: numChipsLog2 = 2;
			break;
		case 8: numChipsLog2 = 3;
			break;		
		}  
		
		GDBG_INFO(80, "numChips: %d  log2: %d\n",numChips, numChipsLog2);
		
		/* Now on to the MUCH nastier backend stuff... */
		for(chipNum = 0; chipNum < numChips; chipNum++) {
			/* Set up pciInit0 and tmuGbeInit */
			if(chipNum == 0) {
				HWC_IO_LOAD(bInfo->regInfo, pciInit0, temp);
				temp = (temp & ~(SST_PCI_RETRY_INTERVAL|SST_PCI_FORCE_FB_HIGH)) |
					SST_PCI_READ_WS | SST_PCI_WRITE_WS;
				HWC_IO_STORE(bInfo->regInfo, pciInit0, temp);
				
				GDBG_INFO(80, "master pciInit0: %08lx\n",temp);
				HWC_IO_LOAD(bInfo->regInfo, tmuGbeInit, temp);
				temp = (temp & ~(SST_AA_CLK_DELAY | SST_AA_CLK_INVERT)) |
					(0x02 << SST_AA_CLK_DELAY_SHIFT) | SST_AA_CLK_INVERT;
				HWC_IO_STORE(bInfo->regInfo, tmuGbeInit, temp);
				
				GDBG_INFO(80, "master tmuGbeInit: %08lx\n",temp);
				
				/* Dump out any other registers we want to look at... */
				HWC_IO_LOAD(bInfo->regInfo, miscInit0, temp);
				GDBG_INFO(80, "master miscInit0: %08lx\n",temp);
				HWC_IO_LOAD(bInfo->regInfo, miscInit1, temp);
				GDBG_INFO(80, "master miscInit1: %08lx\n",temp);
				
			} else {
				HWC_IO_LOAD_SLAVE(chipNum, bInfo->regInfo, pciInit0, temp);
				temp = (temp & ~(SST_PCI_RETRY_INTERVAL|SST_PCI_FORCE_FB_HIGH)) |
					SST_PCI_READ_WS | SST_PCI_WRITE_WS;
				HWC_IO_STORE_SLAVE(chipNum, bInfo->regInfo, pciInit0, temp);
				GDBG_INFO(80, "slave pciInit0: %08lx\n",temp);
				
				HWC_IO_LOAD_SLAVE(chipNum, bInfo->regInfo, tmuGbeInit, temp);
				temp = (temp & ~(SST_AA_CLK_DELAY | SST_AA_CLK_INVERT)) |
					(0x02 << SST_AA_CLK_DELAY_SHIFT) | SST_AA_CLK_INVERT;
				HWC_IO_STORE_SLAVE(chipNum, bInfo->regInfo, tmuGbeInit, temp);
				GDBG_INFO(80, "slave tmuGbeInit: %08lx\n",temp);
				
				HWC_IO_LOAD_SLAVE(chipNum, bInfo->regInfo, miscInit0, temp);
				GDBG_INFO(80, "slave miscInit0: %08lx\n",temp);
				HWC_IO_LOAD_SLAVE(chipNum, bInfo->regInfo, miscInit1, temp);
				GDBG_INFO(80, "slave miscInit1: %08lx\n",temp);
			}
			
			/* Set up buffer swap control and snooping */
			if(numChips > 1) {
				/* Buffer swapping */
				temp = CFG_READ(chipNum, cfgInitEnable_FabID) >> 8;
				GDBG_INFO(80, "cfgInitEnable rd0: %08lx\n",temp << 8);
				temp &= ~SST_SWAP_MASTER;
				temp |= (SST_SWAPBUFFER_ALGORITHM | (chipNum == 0 ? SST_SWAP_MASTER : 0));
				GDBG_INFO(80, "cfgInitEnable wr0: %08lx\n",temp << 8);
				CFG_WRITE(chipNum, cfgInitEnable_FabID, temp << 8); 			 
				
				/* Enable snooping */
#if 0
				if(chipNum == 0) {
					temp = CFG_READ(chipNum, cfgInitEnable_FabID) >> 8;
					temp |= SST_ADDRESS_SNOOP_ENABLE;
					CFG_WRITE(chipNum, cfgInitEnable_FabID, temp << 8);
				} else {
					memBase0 = bInfo->pciInfo.pciBaseAddr[0] & ~0xf;
					memBase1 = bInfo->pciInfo.pciBaseAddr[1] & ~0xf;
					
					GDBG_INFO(80, "memBase0: %08lx\n",memBase0);
					GDBG_INFO(80, "memBase1: %08lx\n",memBase1);
					
					temp = CFG_READ(chipNum, cfgInitEnable_FabID) >> 8;
					GDBG_INFO(80, "cfgInitEnable rd1: %08lx\n",temp << 8);
					temp &= ~SST_MEMBASE0_SNOOP;
					GDBG_INFO(80, "Snoop cleared: %08lx\n",temp << 8);
					temp |= (SST_ADDRESS_SNOOP_ENABLE | SST_MEMBASE0_SNOOP_ENABLE | SST_MEMBASE1_SNOOP_ENABLE | SST_ADDRESS_SNOOP_SLAVE |
						SST_INIT_REGISTER_SNOOP_ENABLE |
						(((memBase0>>22) & 0x3ff) << SST_MEMBASE0_SNOOP_SHIFT) |
						((numChips > 2) ? SST_QUICK_SAMPLING : 0x0));
					GDBG_INFO(80, "cfgInitEnable wr1: %08lx\n",temp << 8);
					CFG_WRITE(chipNum, cfgInitEnable_FabID, temp << 8);
					
					temp = CFG_READ(chipNum, cfgPciDecode);
					GDBG_INFO(80, "cfgPciDecode rd: %08lx\n",temp);
					temp &= ~SST_MEMBASE1_SNOOP;
					temp |= ((memBase1 >> 22) & 0x3ff) << SST_MEMBASE1_SNOOP_SHIFT;
					GDBG_INFO(80, "cfgPciDecode wr: %08lx\n",temp);
					CFG_WRITE(chipNum, cfgPciDecode, temp);
				} 	 
#endif
			}

			continue;
			
			/* cfgSliLfbCtrl */
			if(sliEnable && (!aaEnable || !aaSampleHigh)) {
				/* No aa or 2 sample AA w/SLI */
				sliRenderMask = (numChips - 1) << sliBandHeightLog2;
				sliCompareMask = chipNum << sliBandHeightLog2;
				sliScanMask = sliBandHeight - 1;
				
				temp = (sliRenderMask << SST_SLI_LFB_RENDERMASK_SHIFT) |
					(sliCompareMask << SST_SLI_LFB_COMPAREMASK_SHIFT) |
					(sliScanMask << SST_SLI_LFB_SCANMASK_SHIFT) |
					(numChipsLog2 << SST_SLI_LFB_NUMCHIPS_LOG2_SHIFT) |
					SST_SLI_LFB_CPU_WRITE_ENABLE | 
					SST_SLI_LFB_DISPATCH_WRITE_ENABLE |
					SST_SLI_LFB_READ_ENABLE;
				CFG_WRITE(chipNum, cfgSliLfbCtrl, temp);
			} else if(!sliEnable && aaEnable) {
				/* SLI disabled, AA enabled */
				CFG_WRITE(chipNum, cfgSliLfbCtrl, 0);
			}  else {
				/* SLI enabled, AA enabled, 4 sample AA enabled */
				sliRenderMask = ((numChips >> 1) - 1) << sliBandHeightLog2;
				sliCompareMask = (chipNum >> 1) << sliBandHeightLog2;
				sliScanMask = sliBandHeight - 1;
				
				temp = (sliRenderMask << SST_SLI_LFB_RENDERMASK_SHIFT) |
					(sliCompareMask << SST_SLI_LFB_COMPAREMASK_SHIFT) |
					(sliScanMask << SST_SLI_LFB_SCANMASK_SHIFT) |
					((numChipsLog2-1) << SST_SLI_LFB_NUMCHIPS_LOG2_SHIFT) |
					SST_SLI_LFB_CPU_WRITE_ENABLE | 
					SST_SLI_LFB_DISPATCH_WRITE_ENABLE |
					SST_SLI_LFB_READ_ENABLE;
				CFG_WRITE(chipNum, cfgSliLfbCtrl, temp);
			}  
			
			/* cfgSliAATiledAperture */
			if(sliEnable && !aaEnable) {
				/* Do nothing */	
			} else {
				/* AA is enabled */
				FxU32 format;
				switch(bpp) {
				case 15: format = SST_AA_LFB_READ_FORMAT_15BPP;
					break;
				case 16: format = SST_AA_LFB_READ_FORMAT_16BPP;
					break;
				case 32: format = SST_AA_LFB_READ_FORMAT_32BPP;
					break;
				}  
				
				temp = (aaColorBuffStart << SST_SECONDARY_BUFFER_BASE_SHIFT) |
					SST_AA_LFB_CPU_WRITE_ENABLE | 
					SST_AA_LFB_DISPATCH_WRITE_ENABLE |
					format |
					(aaSampleHigh ? SST_AA_LFB_RD_DIVIDE_BY_FOUR : 0x0);
				CFG_WRITE(chipNum, cfgAALfbCtrl, temp);
				
				temp = ((aaDepthBuffStart >> 12) << SST_AA_DEPTH_BUFFER_APERTURE_BEGIN_SHIFT) |
					((aaDepthBuffEnd >> 12) << SST_AA_DEPTH_BUFFER_APERTURE_END_SHIFT);
				CFG_WRITE(chipNum, cfgAADepthBufferAperture, temp);
			}  
			
			/* Set up vga_vsync_offset field in cfgSliAAMisc */
			if((numChips > 1) && (chipNum > 0) && (aaEnable || sliEnable)) {
				FxU32 vsyncOffsetPixels, vsyncOffsetChars, vsyncOffsetHXtra;
				
				if(analogSLI ||
					((numChips == 4) && sliEnable && aaEnable && aaSampleHigh && !analogSLI && (chipNum == 2))) {
					
					/* Handle four chips, 2-way analog SLI with digital 4-sample AA...*/
					vsyncOffsetPixels = 7;
					vsyncOffsetChars = 4;
					vsyncOffsetHXtra = 0;
				} else {
					/* Run slave 8 clocks ahead */
					vsyncOffsetPixels = 7;
					vsyncOffsetChars = 5;
					vsyncOffsetHXtra = 0;
				}  
				
				temp = CFG_READ(chipNum, cfgSliAAMisc);
				temp &= ~SST_VGA_VSYNC_OFFSET;
				temp |= (vsyncOffsetPixels << SST_VGA_VSYNC_OFFSET_PIXELS_SHIFT) |
					(vsyncOffsetChars << SST_VGA_VSYNC_OFFSET_CHARS_SHIFT) |
					(vsyncOffsetHXtra << SST_VGA_VSYNC_OFFSET_HXTRA_SHIFT);
				CFG_WRITE(chipNum, cfgSliAAMisc, temp); 			 
			}

		
			/* Macros to save my effing fingers */
#define CFG_VIDEOCTRL0(flags, trueShift, falseShift) \
	temp = (flags) |	\
	((trueShift) << SST_CFG_VIDEO_OTHERMUX_SEL_TRUE_SHIFT) | \
	((falseShift) << SST_CFG_VIDEO_OTHERMUX_SEL_FALSE_SHIFT); \
			CFG_WRITE(chipNum, cfgVideoCtrl0, temp);
#define CFG_VIDEOCTRL1(renderFetch, compareFetch, renderCRT, compareCRT) \
	temp = ((renderFetch) << SST_CFG_SLI_RENDERMASK_FETCH_SHIFT) | \
	((compareFetch) << SST_CFG_SLI_COMPAREMASK_FETCH_SHIFT) | \
	((renderCRT) << SST_CFG_SLI_RENDERMASK_CRT_SHIFT) | \
	((compareCRT) << SST_CFG_SLI_COMPAREMASK_CRT_SHIFT); \
			CFG_WRITE(chipNum, cfgVideoCtrl1, temp);
#define CFG_VIDEOCTRL2(renderMask, compareMask) \
	temp = ((renderMask) << SST_CFG_SLI_RENDERMASK_AAFIFO_SHIFT) | \
	((compareMask) << SST_CFG_SLI_COMPAREMASK_AAFIFO_SHIFT); \
			CFG_WRITE(chipNum, cfgVideoCtrl2, temp);
			
			if(numChips == 1 && aaEnable) {
				/* Single chip, 2-sample AA */
				temp = SST_CFG_ENHANCED_VIDEO_EN |
					SST_CFG_VIDEO_LOCALMUX_DESKTOP_PLUS_OVERLAY |
					(SST_CFG_VIDEO_OTHERMUX_SEL_PIPE << SST_CFG_VIDEO_OTHERMUX_SEL_FALSE_SHIFT) |
					SST_CFG_DIVIDE_VIDEO_BY_2;
				CFG_WRITE(chipNum, cfgVideoCtrl0, temp);					
				temp =	(0x00 << SST_CFG_SLI_RENDERMASK_FETCH_SHIFT) |
					(0x00 << SST_CFG_SLI_COMPAREMASK_FETCH_SHIFT) |
					(0x00 << SST_CFG_SLI_RENDERMASK_CRT_SHIFT) |
					(0x00 << SST_CFG_SLI_COMPAREMASK_CRT_SHIFT);
				CFG_WRITE(chipNum, cfgVideoCtrl1, temp);					
				temp = (0x00 << SST_CFG_SLI_RENDERMASK_AAFIFO_SHIFT) |
					(0xFF << SST_CFG_SLI_COMPAREMASK_AAFIFO_SHIFT);
				CFG_WRITE(chipNum, cfgVideoCtrl2, temp);					
			} else if(numChips == 2 && !sliEnable && aaEnable && aaSampleHigh && !analogSLI) {
				/* Two chips, 4-sample digital AA... */
				if(chipNum == 0) {
					/* First chip */
					temp = SST_CFG_ENHANCED_VIDEO_EN |
						SST_CFG_VIDEO_LOCALMUX_DESKTOP_PLUS_OVERLAY |
						(SST_CFG_VIDEO_OTHERMUX_SEL_PIPE_PLUS_AAFIFO << SST_CFG_VIDEO_OTHERMUX_SEL_TRUE_SHIFT) |
						SST_CFG_DIVIDE_VIDEO_BY_4;
					CFG_WRITE(chipNum, cfgVideoCtrl0, temp);
					temp =	(0x00 << SST_CFG_SLI_RENDERMASK_FETCH_SHIFT) |
						(0x00 << SST_CFG_SLI_COMPAREMASK_FETCH_SHIFT) |
						(0x00 << SST_CFG_SLI_RENDERMASK_CRT_SHIFT) |
						(0x00 << SST_CFG_SLI_COMPAREMASK_CRT_SHIFT);
					CFG_WRITE(chipNum, cfgVideoCtrl1, temp);
					temp = (0x00 << SST_CFG_SLI_RENDERMASK_AAFIFO_SHIFT) |
						(0x00 << SST_CFG_SLI_COMPAREMASK_AAFIFO_SHIFT);
					CFG_WRITE(chipNum, cfgVideoCtrl2, temp);					
				} else {
					/* Second chip */
					temp = SST_CFG_ENHANCED_VIDEO_EN |
						SST_CFG_ENHANCED_VIDEO_SLV |
						SST_CFG_VIDEO_LOCALMUX_DESKTOP_PLUS_OVERLAY |
						(SST_CFG_VIDEO_OTHERMUX_SEL_PIPE << SST_CFG_VIDEO_OTHERMUX_SEL_TRUE_SHIFT) |
						SST_CFG_DIVIDE_VIDEO_BY_1;
					CFG_WRITE(chipNum, cfgVideoCtrl0, temp);
					temp =	(0x00 << SST_CFG_SLI_RENDERMASK_FETCH_SHIFT) |
						(0x00 << SST_CFG_SLI_COMPAREMASK_FETCH_SHIFT) |
						(0x00 << SST_CFG_SLI_RENDERMASK_CRT_SHIFT) |
						(0xFF << SST_CFG_SLI_COMPAREMASK_CRT_SHIFT);
					CFG_WRITE(chipNum, cfgVideoCtrl1, temp);
					temp = (0x00 << SST_CFG_SLI_RENDERMASK_AAFIFO_SHIFT) |
						(0x00 << SST_CFG_SLI_COMPAREMASK_AAFIFO_SHIFT);
					CFG_WRITE(chipNum, cfgVideoCtrl2, temp);					
				}  
			} else if(numChips == 2 && !sliEnable && aaEnable && aaSampleHigh && analogSLI) {
				/* Two chips, 4-sample analog AA... */
				if(chipNum == 0) {
					/* First chip */
					temp = SST_CFG_ENHANCED_VIDEO_EN |
						SST_CFG_VIDEO_LOCALMUX_DESKTOP_PLUS_OVERLAY |
						(SST_CFG_VIDEO_OTHERMUX_SEL_PIPE << SST_CFG_VIDEO_OTHERMUX_SEL_TRUE_SHIFT) |
						SST_CFG_DIVIDE_VIDEO_BY_4;
					CFG_WRITE(chipNum, cfgVideoCtrl0, temp);
				} else {
					/* Second chip */
					temp = SST_CFG_ENHANCED_VIDEO_EN |
						SST_CFG_ENHANCED_VIDEO_SLV |
						SST_CFG_DAC_HSYNC_TRISTATE |
						SST_CFG_VIDEO_LOCALMUX_DESKTOP_PLUS_OVERLAY |
						(SST_CFG_VIDEO_OTHERMUX_SEL_PIPE << SST_CFG_VIDEO_OTHERMUX_SEL_TRUE_SHIFT) |
						SST_CFG_DIVIDE_VIDEO_BY_4;
					CFG_WRITE(chipNum, cfgVideoCtrl0, temp);
					temp =	(0x00 << SST_CFG_SLI_RENDERMASK_FETCH_SHIFT) |
						(0x00 << SST_CFG_SLI_COMPAREMASK_FETCH_SHIFT) |
						(0x00 << SST_CFG_SLI_RENDERMASK_CRT_SHIFT) |
						(0x00 << SST_CFG_SLI_COMPAREMASK_CRT_SHIFT);
					CFG_WRITE(chipNum, cfgVideoCtrl1, temp);
					temp = (0x00 << SST_CFG_SLI_RENDERMASK_AAFIFO_SHIFT) |
						(0x00 << SST_CFG_SLI_COMPAREMASK_AAFIFO_SHIFT);
					CFG_WRITE(chipNum, cfgVideoCtrl2, temp);					
				}  
			} else if(numChips == 2 && sliEnable && !aaEnable && !analogSLI) {
				/* Two chips, 2-way digital SLI */
				if(chipNum == 0) {
					/* First chip */
					temp = SST_CFG_ENHANCED_VIDEO_EN |
						(SST_CFG_VIDEO_OTHERMUX_SEL_AAFIFO << SST_CFG_VIDEO_OTHERMUX_SEL_TRUE_SHIFT) |
						(SST_CFG_VIDEO_OTHERMUX_SEL_PIPE << SST_CFG_VIDEO_OTHERMUX_SEL_FALSE_SHIFT) |
						SST_CFG_DIVIDE_VIDEO_BY_1;
					CFG_WRITE(chipNum, cfgVideoCtrl0, temp);
					temp =	((0x01 <<sliBandHeightLog2) << SST_CFG_SLI_RENDERMASK_FETCH_SHIFT) |
						(0x00 << SST_CFG_SLI_COMPAREMASK_FETCH_SHIFT) |
						(0x00 << SST_CFG_SLI_RENDERMASK_CRT_SHIFT) |
						(0x00 << SST_CFG_SLI_COMPAREMASK_CRT_SHIFT);
					CFG_WRITE(chipNum, cfgVideoCtrl1, temp);
					temp = ((0x01 <<sliBandHeightLog2) << SST_CFG_SLI_RENDERMASK_AAFIFO_SHIFT) |
						((0x01 <<sliBandHeightLog2) << SST_CFG_SLI_COMPAREMASK_AAFIFO_SHIFT);
					CFG_WRITE(chipNum, cfgVideoCtrl2, temp);					
				} else {
					/* Second chip */
					temp = SST_CFG_ENHANCED_VIDEO_EN |
						SST_CFG_ENHANCED_VIDEO_SLV |
						(SST_CFG_VIDEO_OTHERMUX_SEL_PIPE << SST_CFG_VIDEO_OTHERMUX_SEL_TRUE_SHIFT) |
						(SST_CFG_VIDEO_OTHERMUX_SEL_PIPE << SST_CFG_VIDEO_OTHERMUX_SEL_FALSE_SHIFT) |
						SST_CFG_DIVIDE_VIDEO_BY_1;
					CFG_WRITE(chipNum, cfgVideoCtrl0, temp);
					temp = (((numChips - 1) << sliBandHeightLog2) << SST_CFG_SLI_RENDERMASK_FETCH_SHIFT) |
						((chipNum  << sliBandHeightLog2)	<< SST_CFG_SLI_COMPAREMASK_FETCH_SHIFT) |
						(0x00 << SST_CFG_SLI_RENDERMASK_CRT_SHIFT) |
						(0xFF << SST_CFG_SLI_COMPAREMASK_CRT_SHIFT);
					CFG_WRITE(chipNum, cfgVideoCtrl1, temp);
					temp = (((numChips - 1) << sliBandHeightLog2) << SST_CFG_SLI_RENDERMASK_AAFIFO_SHIFT) |
						((chipNum << sliBandHeightLog2) << SST_CFG_SLI_COMPAREMASK_AAFIFO_SHIFT);
					CFG_WRITE(chipNum, cfgVideoCtrl2, temp);
				} 	 
			} else if((numChips == 2 || numChips == 4) && sliEnable && !aaEnable && analogSLI) {
				/* 2 or 4 chips, 2/4-way analog SLI */
				if(chipNum == 0) {
					/* First chip */
					temp = SST_CFG_ENHANCED_VIDEO_EN |
						(SST_CFG_VIDEO_OTHERMUX_SEL_PIPE << SST_CFG_VIDEO_OTHERMUX_SEL_TRUE_SHIFT) |
						(SST_CFG_VIDEO_OTHERMUX_SEL_PIPE << SST_CFG_VIDEO_OTHERMUX_SEL_FALSE_SHIFT) |
						SST_CFG_DIVIDE_VIDEO_BY_1;
					CFG_WRITE(chipNum, cfgVideoCtrl0, temp);
					temp = (((numChips - 1) << sliBandHeightLog2) << SST_CFG_SLI_RENDERMASK_FETCH_SHIFT) |
						(0x00 << SST_CFG_SLI_COMPAREMASK_FETCH_SHIFT) |
						(((numChips - 1) << sliBandHeightLog2) << SST_CFG_SLI_RENDERMASK_CRT_SHIFT) |
						(0x00 << SST_CFG_SLI_COMPAREMASK_CRT_SHIFT);
					CFG_WRITE(chipNum, cfgVideoCtrl1, temp);
					temp = (0x00 << SST_CFG_SLI_RENDERMASK_AAFIFO_SHIFT) |
						(0xFF << SST_CFG_SLI_COMPAREMASK_AAFIFO_SHIFT);
					CFG_WRITE(chipNum, cfgVideoCtrl2, temp);					
				} else {
					/* Remaining chips */
					temp = SST_CFG_ENHANCED_VIDEO_EN |
						SST_CFG_ENHANCED_VIDEO_SLV |
						(SST_CFG_VIDEO_OTHERMUX_SEL_PIPE << SST_CFG_VIDEO_OTHERMUX_SEL_TRUE_SHIFT) |
						(SST_CFG_VIDEO_OTHERMUX_SEL_PIPE << SST_CFG_VIDEO_OTHERMUX_SEL_FALSE_SHIFT) |
						SST_CFG_DIVIDE_VIDEO_BY_1;
					CFG_WRITE(chipNum, cfgVideoCtrl0, temp);
					temp = (((numChips - 1) << sliBandHeightLog2) << SST_CFG_SLI_RENDERMASK_FETCH_SHIFT) |
						((chipNum << sliBandHeightLog2) << SST_CFG_SLI_COMPAREMASK_FETCH_SHIFT) |
						(((numChips - 1) << sliBandHeightLog2) << SST_CFG_SLI_RENDERMASK_CRT_SHIFT) |
						((chipNum << sliBandHeightLog2) << SST_CFG_SLI_COMPAREMASK_CRT_SHIFT);
					CFG_WRITE(chipNum, cfgVideoCtrl1, temp);
					temp = (0x00 << SST_CFG_SLI_RENDERMASK_AAFIFO_SHIFT) |
						(0xFF << SST_CFG_SLI_COMPAREMASK_AAFIFO_SHIFT);
					CFG_WRITE(chipNum, cfgVideoCtrl2, temp);					
				} 	
			} else if(numChips == 2 && sliEnable && aaEnable && !aaSampleHigh && !analogSLI) {
				/* Two chips, 2-sample AA with 2-way digital SLI... */
				if(chipNum == 0) {
					/* First chip */
					temp = SST_CFG_ENHANCED_VIDEO_EN |
						SST_CFG_VIDEO_LOCALMUX_DESKTOP_PLUS_OVERLAY |
						(SST_CFG_VIDEO_OTHERMUX_SEL_AAFIFO << SST_CFG_VIDEO_OTHERMUX_SEL_TRUE_SHIFT) |
						(SST_CFG_VIDEO_OTHERMUX_SEL_PIPE << SST_CFG_VIDEO_OTHERMUX_SEL_FALSE_SHIFT) |
						SST_CFG_DIVIDE_VIDEO_BY_2;
					CFG_WRITE(chipNum, cfgVideoCtrl0, temp);
					temp = ((0x01 << sliBandHeightLog2) << SST_CFG_SLI_RENDERMASK_FETCH_SHIFT) |
						(0x00 << SST_CFG_SLI_COMPAREMASK_FETCH_SHIFT) |
						(0x00 << SST_CFG_SLI_COMPAREMASK_CRT_SHIFT) |
						(0x00 << SST_CFG_SLI_COMPAREMASK_CRT_SHIFT);
					CFG_WRITE(chipNum, cfgVideoCtrl1, temp);
					temp = ((0x01 << sliBandHeightLog2) << SST_CFG_SLI_RENDERMASK_AAFIFO_SHIFT) |
						((0x01 << sliBandHeightLog2) << SST_CFG_SLI_COMPAREMASK_AAFIFO_SHIFT);
					CFG_WRITE(chipNum, cfgVideoCtrl2, temp);
				} else {
					/* Second chip */
					temp = SST_CFG_ENHANCED_VIDEO_EN |
						SST_CFG_ENHANCED_VIDEO_SLV |
						SST_CFG_VIDEO_LOCALMUX_DESKTOP_PLUS_OVERLAY |
						(SST_CFG_VIDEO_OTHERMUX_SEL_PIPE << SST_CFG_VIDEO_OTHERMUX_SEL_TRUE_SHIFT) |
						(SST_CFG_VIDEO_OTHERMUX_SEL_PIPE << SST_CFG_VIDEO_OTHERMUX_SEL_FALSE_SHIFT) |
						SST_CFG_DIVIDE_VIDEO_BY_1;
					CFG_WRITE(chipNum, cfgVideoCtrl0, temp);
					temp = (((numChips - 1) << sliBandHeightLog2) << SST_CFG_SLI_RENDERMASK_FETCH_SHIFT) |
						((chipNum  << sliBandHeightLog2) << SST_CFG_SLI_COMPAREMASK_FETCH_SHIFT) |
						(0x00 << SST_CFG_SLI_RENDERMASK_CRT_SHIFT) |
						(0xFF << SST_CFG_SLI_COMPAREMASK_CRT_SHIFT);
					CFG_WRITE(chipNum, cfgVideoCtrl1, temp);
					temp = (((numChips - 1) << sliBandHeightLog2) << SST_CFG_SLI_RENDERMASK_AAFIFO_SHIFT) |
						((chipNum  << sliBandHeightLog2) << SST_CFG_SLI_COMPAREMASK_AAFIFO_SHIFT);
					CFG_WRITE(chipNum, cfgVideoCtrl2, temp);
				} 	 
			} else if(numChips == 4 && sliEnable && !aaEnable && !analogSLI) {
				/* Four chips, 4-way digital SLI... */
				if(chipNum == 0) {
					/* First chip */
					temp = SST_CFG_ENHANCED_VIDEO_EN |
						(SST_CFG_VIDEO_OTHERMUX_SEL_AAFIFO << SST_CFG_VIDEO_OTHERMUX_SEL_TRUE_SHIFT) |
						(SST_CFG_VIDEO_OTHERMUX_SEL_PIPE << SST_CFG_VIDEO_OTHERMUX_SEL_FALSE_SHIFT) |
						SST_CFG_SLI_AAFIFO_COMPARE_INV |
						SST_CFG_DIVIDE_VIDEO_BY_1;
					CFG_WRITE(chipNum, cfgVideoCtrl0, temp);
					temp = (((numChips - 1) << sliBandHeightLog2) << SST_CFG_SLI_RENDERMASK_FETCH_SHIFT) |
						(0x00 << SST_CFG_SLI_COMPAREMASK_FETCH_SHIFT) |
						(0x00 << SST_CFG_SLI_RENDERMASK_CRT_SHIFT) |
						(0x00 << SST_CFG_SLI_COMPAREMASK_CRT_SHIFT);
					CFG_WRITE(chipNum, cfgVideoCtrl1, temp);
					temp = (((numChips - 1) << sliBandHeightLog2) << SST_CFG_SLI_RENDERMASK_AAFIFO_SHIFT) |
						((0x00 << sliBandHeightLog2) << SST_CFG_SLI_COMPAREMASK_AAFIFO_SHIFT);
					CFG_WRITE(chipNum, cfgVideoCtrl2, temp);
				} else {
					/* Remaining chips */
					temp = SST_CFG_ENHANCED_VIDEO_EN |
						SST_CFG_ENHANCED_VIDEO_SLV |
						(SST_CFG_VIDEO_OTHERMUX_SEL_PIPE << SST_CFG_VIDEO_OTHERMUX_SEL_TRUE_SHIFT) |
						(SST_CFG_VIDEO_OTHERMUX_SEL_PIPE << SST_CFG_VIDEO_OTHERMUX_SEL_FALSE_SHIFT) |
						SST_CFG_DIVIDE_VIDEO_BY_1;
					CFG_WRITE(chipNum, cfgVideoCtrl0, temp);
					temp = (((numChips - 1) << sliBandHeightLog2) << SST_CFG_SLI_RENDERMASK_FETCH_SHIFT) |
						((chipNum << sliBandHeightLog2) << SST_CFG_SLI_COMPAREMASK_FETCH_SHIFT) |
						(0x00 << SST_CFG_SLI_RENDERMASK_CRT_SHIFT) |
						(0xFF << SST_CFG_SLI_COMPAREMASK_CRT_SHIFT);
					CFG_WRITE(chipNum, cfgVideoCtrl1, temp);
					temp = (((numChips - 1) << sliBandHeightLog2) << SST_CFG_SLI_RENDERMASK_AAFIFO_SHIFT) |
						((chipNum << sliBandHeightLog2) << SST_CFG_SLI_COMPAREMASK_AAFIFO_SHIFT);
					CFG_WRITE(chipNum, cfgVideoCtrl2, temp);
				}
				
			} else if(numChips == 2 && sliEnable && aaEnable && !aaSampleHigh && !analogSLI) {
				if(chipNum == 0) {
					/* Four chips, 2-sample AA with 4-way digital SLI */
					CFG_VIDEOCTRL0(SST_CFG_ENHANCED_VIDEO_EN | 
						SST_CFG_VIDEO_LOCALMUX_DESKTOP_PLUS_OVERLAY |
						SST_CFG_SLI_AAFIFO_COMPARE_INV |
						SST_CFG_DIVIDE_VIDEO_BY_2,
						SST_CFG_VIDEO_OTHERMUX_SEL_AAFIFO,
						SST_CFG_VIDEO_OTHERMUX_SEL_PIPE);
					CFG_VIDEOCTRL1((numChips - 1) << sliBandHeightLog2,
						0x00,
						0x00,
						0x00);
					CFG_VIDEOCTRL2((numChips - 1) << sliBandHeightLog2,
						0x00);
				} else {
					CFG_VIDEOCTRL0(SST_CFG_ENHANCED_VIDEO_EN |
						SST_CFG_ENHANCED_VIDEO_SLV |
						SST_CFG_VIDEO_LOCALMUX_DESKTOP_PLUS_OVERLAY |
						SST_CFG_DIVIDE_VIDEO_BY_1,
						SST_CFG_VIDEO_OTHERMUX_SEL_PIPE,
						SST_CFG_VIDEO_OTHERMUX_SEL_PIPE);
					CFG_VIDEOCTRL1((numChips - 1) << sliBandHeightLog2,
						chipNum << sliBandHeightLog2,
						0x00,
						0xff);
					CFG_VIDEOCTRL2((numChips - 1) << sliBandHeightLog2,
						chipNum << sliBandHeightLog2);
				} 																 
			} else if(numChips == 4 && sliEnable && aaEnable && aaSampleHigh && !analogSLI) {
				/* Four chip, 2-way analog SLI with digital 4-sample AA */
				if(chipNum == 0) {
					/* First chip */
					CFG_VIDEOCTRL0(SST_CFG_ENHANCED_VIDEO_EN |
						SST_CFG_VIDEO_LOCALMUX_DESKTOP_PLUS_OVERLAY |
						SST_CFG_DIVIDE_VIDEO_BY_4,
						SST_CFG_VIDEO_OTHERMUX_SEL_PIPE_PLUS_AAFIFO,
						0);
					CFG_VIDEOCTRL1(0x01 << sliBandHeightLog2,
						0x00 << sliBandHeightLog2,
						0x01 << sliBandHeightLog2,
						0x00 << sliBandHeightLog2);
					CFG_VIDEOCTRL2(0x00, 0x00);
				} else if(chipNum == 1 || chipNum == 3) { 
					/* Second and fourth chips */
					CFG_VIDEOCTRL0(SST_CFG_ENHANCED_VIDEO_EN |
						SST_CFG_ENHANCED_VIDEO_SLV |
						SST_CFG_DAC_HSYNC_TRISTATE |
						SST_CFG_VIDEO_LOCALMUX_DESKTOP_PLUS_OVERLAY |
						SST_CFG_DIVIDE_VIDEO_BY_1,
						SST_CFG_VIDEO_OTHERMUX_SEL_PIPE,
						0);
					CFG_VIDEOCTRL1(0x01 << sliBandHeightLog2,
						((chipNum + 1) >> 2) << sliBandHeightLog2,
						0x00 << sliBandHeightLog2,
						0xFF << sliBandHeightLog2);
					CFG_VIDEOCTRL2(0x00, 0x00);
				} else {
					/* Third chip */
					CFG_VIDEOCTRL0(SST_CFG_ENHANCED_VIDEO_EN |
						SST_CFG_ENHANCED_VIDEO_SLV |
						SST_CFG_VIDEO_LOCALMUX_DESKTOP_PLUS_OVERLAY |
						SST_CFG_DIVIDE_VIDEO_BY_4,
						SST_CFG_VIDEO_OTHERMUX_SEL_PIPE_PLUS_AAFIFO,
						0x00);
					CFG_VIDEOCTRL1(0x01 << sliBandHeightLog2,
						0x01 << sliBandHeightLog2,
						0x01 << sliBandHeightLog2,
						0x01 << sliBandHeightLog2);
					CFG_VIDEOCTRL2(0x00, 0xff);
				} 	 
			} else if(numChips == 4 && sliEnable && aaEnable && aaSampleHigh && analogSLI) {
				if(chipNum == 0) {
					/* First chip */
					CFG_VIDEOCTRL0(SST_CFG_ENHANCED_VIDEO_EN |
						SST_CFG_VIDEO_LOCALMUX_DESKTOP_PLUS_OVERLAY |
						SST_CFG_DIVIDE_VIDEO_BY_4,
						SST_CFG_VIDEO_OTHERMUX_SEL_PIPE,
						0);
					CFG_VIDEOCTRL1(0x01 << sliBandHeightLog2,
						0x00 << sliBandHeightLog2,
						0x01 << sliBandHeightLog2,
						0x00 << sliBandHeightLog2);
					CFG_VIDEOCTRL2(0x00, 0x00);
				} else if(chipNum == 1 || chipNum == 3) {
					/* Second and fourth chips */
					CFG_VIDEOCTRL0(SST_CFG_ENHANCED_VIDEO_EN |
						SST_CFG_ENHANCED_VIDEO_SLV |
						SST_CFG_DAC_HSYNC_TRISTATE |
						SST_CFG_VIDEO_LOCALMUX_DESKTOP_PLUS_OVERLAY |
						SST_CFG_DIVIDE_VIDEO_BY_4,
						SST_CFG_VIDEO_OTHERMUX_SEL_PIPE,
						0);
					CFG_VIDEOCTRL1(0x01 << sliBandHeightLog2,
						((chipNum + 1) >> 2) << sliBandHeightLog2,
						0x01 << sliBandHeightLog2,
						((chipNum + 1) >> 2) << sliBandHeightLog2);
					CFG_VIDEOCTRL2(0x00, 0x00);
				} else {
					/* Third chip */
					CFG_VIDEOCTRL0(SST_CFG_ENHANCED_VIDEO_EN |
						SST_CFG_ENHANCED_VIDEO_SLV |
						SST_CFG_VIDEO_LOCALMUX_DESKTOP_PLUS_OVERLAY |
						SST_CFG_DIVIDE_VIDEO_BY_4,
						SST_CFG_VIDEO_OTHERMUX_SEL_PIPE,
						0);
					CFG_VIDEOCTRL1(0x01 << sliBandHeightLog2,
						0x01 << sliBandHeightLog2,
						0x01 << sliBandHeightLog2,
						0x01 << sliBandHeightLog2);
					CFG_VIDEOCTRL2(0x00, 0x00);
				}  
			}  
			
			/* Make sure that last chip properly waits for data to be xfered
			* over the PCI bus before driving... */
			if(numChips == 4 && sliEnable && aaEnable && aaSampleHigh && chipNum == 3) {
				temp = CFG_READ(chipNum, cfgSliAAMisc);
				temp |= SST_CFG_AA_LFB_RD_SLV_WAIT;
				CFG_WRITE(chipNum, cfgSliAAMisc, temp);
			}  
			
			if(chipNum > 0) {
			/* For the slave chips, make the video PLL lock to the Master's 
				* sync_clk_out clock output... */
				temp = CFG_READ(chipNum, cfgVideoCtrl0);
				temp |= SST_CFG_VIDPLL_SEL;
				CFG_WRITE(chipNum, cfgVideoCtrl0, temp);
				
				/* Power down slave(s) RAMDAC */
				HWC_IO_LOAD_SLAVE(chipNum, bInfo->regInfo, miscInit1, temp);
				temp |= SST_POWERDOWN_DAC;
				HWC_IO_STORE_SLAVE(chipNum, bInfo->regInfo, miscInit1, temp);
			}  
			
			GDBG_INFO(80, "cfgInitEnable: %08lx\n",CFG_READ(chipNum, cfgInitEnable_FabID));
			GDBG_INFO(80, "cfgPciDecode:  %08lx\n",CFG_READ(chipNum, cfgPciDecode));
			GDBG_INFO(80, "cfgVideoCtrl0: %08lx\n",CFG_READ(chipNum, cfgVideoCtrl0));
			GDBG_INFO(80, "cfgVideoCtrl1: %08lx\n",CFG_READ(chipNum, cfgVideoCtrl1));
			GDBG_INFO(80, "cfgVideoCtrl2: %08lx\n",CFG_READ(chipNum, cfgVideoCtrl2));
			GDBG_INFO(80, "cfgSliLfbCtrl: %08lx\n",CFG_READ(chipNum, cfgSliLfbCtrl));
			GDBG_INFO(80, "cfgAADepthAp:  %08lx\n",CFG_READ(chipNum, cfgAADepthBufferAperture));
			GDBG_INFO(80, "cfgAALfbCtrl:  %08lx\n",CFG_READ(chipNum, cfgAALfbCtrl));
			GDBG_INFO(80, "cfgSliAAMisc:  %08lx\n",CFG_READ(chipNum, cfgSliAAMisc));
		}
	} else { /* Disable AA and/or SLI */
		
		for(chipNum = 0; chipNum < numChips; chipNum++) {
			temp = CFG_READ(chipNum, cfgInitEnable_FabID) >> 8;
			temp &= ~(SST_ADDRESS_SNOOP_ENABLE | SST_MEMBASE0_SNOOP_ENABLE | SST_MEMBASE1_SNOOP_ENABLE | SST_ADDRESS_SNOOP_SLAVE |
				SST_INIT_REGISTER_SNOOP_ENABLE | SST_MEMBASE0_SNOOP | SST_QUICK_SAMPLING | SST_SWAP_MASTER | SST_SWAPBUFFER_ALGORITHM);
			CFG_WRITE(chipNum, cfgInitEnable_FabID, temp << 8);
			
			CFG_WRITE(chipNum, cfgSliLfbCtrl, 0);
			CFG_WRITE(chipNum, cfgAALfbCtrl, 0);
			CFG_WRITE(chipNum, cfgSliAAMisc, 0);
			// Make sure slave chips don't drive HSYNC & VSYNC
			if(chipNum > 0) {
				CFG_WRITE(chipNum, cfgVideoCtrl0, SST_CFG_DAC_HSYNC_TRISTATE | SST_CFG_DAC_VSYNC_TRISTATE);
			} else {
				CFG_WRITE(chipNum, cfgVideoCtrl0, 0);
			}
			CFG_WRITE(chipNum, cfgVideoCtrl1, 0);
			CFG_WRITE(chipNum, cfgVideoCtrl2, 0);
			CFG_WRITE(chipNum, cfgAADepthBufferAperture, 0);
			
			/* Kill the DAC & video output on the slaves */
			if(chipNum > 0) {
				HWC_IO_STORE_SLAVE(chipNum, bInfo->regInfo, dacMode, SST_DAC_DPMS_ON_VSYNC | SST_DAC_DPMS_ON_HSYNC);
				HWC_IO_LOAD_SLAVE(chipNum, bInfo->regInfo, vidProcCfg, temp);
				temp &= ~SST_VIDEO_PROCESSOR_EN;
				HWC_IO_STORE_SLAVE(chipNum, bInfo->regInfo, vidProcCfg, temp);
			}
		} 		 
	} 	 
} 		 
