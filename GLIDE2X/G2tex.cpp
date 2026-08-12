//
// Glide 2 to Glide 3 Translator for GlideXP
//
// g2tex.cpp : Glide 2 Texturing Functions
//
// Copyright (c) 2002 Ryan Nunn
//
// Just a note that this is written in C++ so I can use namespaces.
// No other C++ features are used.
//

#include "g2pch.h"

namespace Glide3 {

grTexCalcMemRequired_proc		grTexCalcMemRequired = 0;
grTexClampMode_proc				grTexClampMode = 0;
grTexCombine_proc				grTexCombine = 0;
grTexDetailControl_proc			grTexDetailControl = 0;
grTexDownloadMipMap_proc		grTexDownloadMipMap = 0;
grTexDownloadMipMapLevel_proc	grTexDownloadMipMapLevel = 0;
grTexDownloadMipMapLevelPartial_proc	grTexDownloadMipMapLevelPartial = 0;
grTexDownloadTable_proc			grTexDownloadTable = 0;
grTexDownloadTableExt_proc		grTexDownloadTableExt = 0;
grTexDownloadTablePartial_proc	grTexDownloadTablePartial = 0;
grTexDownloadTablePartialExt_proc	grTexDownloadTablePartialExt = 0;
grTexFilterMode_proc			grTexFilterMode = 0;
grTexLodBiasValue_proc			grTexLodBiasValue = 0;
grTexMaxAddress_proc			grTexMaxAddress = 0;
grTexMinAddress_proc			grTexMinAddress = 0;
grTexMipMapMode_proc			grTexMipMapMode = 0;
grTexMultibase_proc				grTexMultibase = 0;
grTexMultibaseAddress_proc		grTexMultibaseAddress = 0;
grTexNCCTable_proc				grTexNCCTable = 0;
grTexNCCTableExt_proc			grTexNCCTableExt = 0;
grTexSource_proc				grTexSource = 0;
grTexTextureMemRequired_proc	grTexTextureMemRequired = 0;
grTextureBuffer_proc			grTextureBufferExt = 0;
grTextureAuxBuffer_proc			grTextureAuxBufferExt = 0;
grTexDownloadMipMapLevelPartialRowExt_proc	grTexDownloadMipMapLevelPartialRowExt = 0;

bool	SetupTexFunctions()
{
#define FN_NAME "SetupTexFunctions"
    GDBG_INFO(80, "%s\n", FN_NAME);

	bool ret = true;

	if (ret) ret = LoadFunc(grTexCalcMemRequired,16);
	if (ret) ret = LoadFunc(grTexClampMode,12);
	if (ret) ret = LoadFunc(grTexCombine,28);
	if (ret) ret = LoadFunc(grTexDetailControl,16);
	if (ret) ret = LoadFunc(grTexDownloadMipMap,16);
	if (ret) ret = LoadFunc(grTexDownloadMipMapLevel,32);
	if (ret) ret = LoadFunc(grTexDownloadMipMapLevelPartial,40);
	if (ret) ret = LoadFunc(grTexDownloadTable,8);
	if (ret) ret = LoadFunc(grTexDownloadTablePartial,16);
	if (ret) ret = LoadFunc(grTexFilterMode,12);
	if (ret) ret = LoadFunc(grTexLodBiasValue,8);
	if (ret) ret = LoadFunc(grTexMaxAddress,4);
	if (ret) ret = LoadFunc(grTexMinAddress,4);
	if (ret) ret = LoadFunc(grTexMipMapMode,12);
	if (ret) ret = LoadFunc(grTexMultibase,8);
	if (ret) ret = LoadFunc(grTexMultibaseAddress,20);
	if (ret) ret = LoadFunc(grTexNCCTable,4);
	if (ret) ret = LoadFunc(grTexSource,16);
	if (ret) ret = LoadFunc(grTexTextureMemRequired,8);

	// These funcs aren't normally exported by Glide 3. 
	// They are only available as extensions.
	if (ret) {

		// These functions are exported if the 'POINTCAST' extension is supported
		grTexDownloadTableExt = (grTexDownloadTableExt_proc)
					Glide3::grGetProcAddress("grTexDownloadTableExt");

		grTexDownloadTablePartialExt = (grTexDownloadTablePartialExt_proc)
					Glide3::grGetProcAddress("grTexDownloadTablePartialExt");

		grTexNCCTableExt = (grTexNCCTableExt_proc) Glide3::grGetProcAddress("grTexNCCTableExt");

		// If we don't support all, we can't use the extension
		if (!grTexDownloadTableExt || !grTexDownloadTablePartialExt || !grTexNCCTableExt) {
			grTexDownloadTableExt = 0;
			grTexDownloadTablePartialExt = 0;
			grTexNCCTableExt = 0;
		    GDBG_INFO(80, "%s: Warning Glide3 POINTCAST extension not supported\n", FN_NAME);
		}

		// TEXTUREBUFFER extension
		grTextureBufferExt = (grTextureBuffer_proc)
				Glide3::grGetProcAddress("grTextureBufferExt");

		grTextureAuxBufferExt = (grTextureAuxBuffer_proc)
				Glide3::grGetProcAddress("grTextureAuxBufferExt");

		grTexDownloadMipMapLevelPartialRowExt = (grTexDownloadMipMapLevelPartialRowExt_proc)
				Glide3::grGetProcAddress("grTexDownloadMipMapLevelPartialRowExt");
	}

	return ret;
#undef FN_NAME
}

bool	FreeTexFunctions()
{
#define FN_NAME "FreeTexFunctions"
	//GDBG_INFO(80, "%s\n", FN_NAME);

	grTexCalcMemRequired = 0;
	grTexClampMode = 0;
	grTexCombine = 0;
	grTexDetailControl = 0;
	grTexDownloadMipMap = 0;
	grTexDownloadMipMapLevel = 0;
	grTexDownloadMipMapLevelPartial = 0;
	grTexDownloadTable = 0;
	grTexDownloadTableExt = 0;
	grTexDownloadTablePartial = 0;
	grTexDownloadTablePartialExt = 0;
	grTexFilterMode = 0;
	grTexLodBiasValue = 0;
	grTexMaxAddress = 0;
	grTexMinAddress = 0;
	grTexMipMapMode = 0;
	grTexMultibase = 0;
	grTexMultibaseAddress = 0;
	grTexNCCTable = 0;
	grTexNCCTableExt = 0;
	grTexSource = 0;
	grTexTextureMemRequired = 0;
	grTextureBufferExt = 0;
	grTextureAuxBufferExt = 0;

	return true;
#undef FN_NAME
}

};

#if 0
#define GR_LOD_LOG2_256			0x8
#define GR_LOD_LOG2_128			0x7
#define GR_LOD_LOG2_64			0x6
#define GR_LOD_LOG2_32			0x5
#define GR_LOD_LOG2_16			0x4
#define GR_LOD_LOG2_8			0x3
#define GR_LOD_LOG2_4			0x2
#define GR_LOD_LOG2_2			0x1
#define GR_LOD_LOG2_1			0x0

#define GR_LOD_256				0x0
#define GR_LOD_128				0x1
#define GR_LOD_64				0x2
#define GR_LOD_32				0x3
#define GR_LOD_16				0x4
#define GR_LOD_8				0x5
#define GR_LOD_4				0x6
#define GR_LOD_2				0x7
#define GR_LOD_1				0x8

#define GR_ASPECT_LOG2_8x1		3		/* 8W x 1H */
#define GR_ASPECT_LOG2_4x1		2		/* 4W x 1H */
#define GR_ASPECT_LOG2_2x1		1		/* 2W x 1H */
#define GR_ASPECT_LOG2_1x1		0		/* 1W x 1H */
#define GR_ASPECT_LOG2_1x2		-1		/* 1W x 2H */
#define GR_ASPECT_LOG2_1x4		-2		/* 1W x 4H */
#define GR_ASPECT_LOG2_1x8		-3		/* 1W x 8H */

#define GR_ASPECT_8x1			0x0		/* 8W x 1H */
#define GR_ASPECT_4x1			0x1		/* 4W x 1H */
#define GR_ASPECT_2x1			0x2		/* 2W x 1H */
#define GR_ASPECT_1x1			0x3		/* 1W x 1H */
#define GR_ASPECT_1x2			0x4		/* 1W x 2H */
#define GR_ASPECT_1x4			0x5		/* 1W x 4H */
#define GR_ASPECT_1x8			0x6		/* 1W x 8H */

#endif

//
// grTexCalcMemRequired
//
FxU32 FX_CALL grTexCalcMemRequired (GrLOD_t lodmin, GrLOD_t lodmax,
									GrAspectRatio_t aspect, GrTextureFormat_t fmt)
{
#define FN_NAME "grTexCalcMemRequired"
    GDBG_INFO(80, "%s: g2 min %i max %i aspect %i fmt %i\n", FN_NAME, lodmin, lodmax, aspect, fmt);
    GDBG_INFO(80, "%s: g3 min %i max %i aspect %i fmt %i\n", FN_NAME, ConLod(lodmin), ConLod(lodmax), ConAspect(aspect), fmt);

	FxU32 ret = Glide3::grTexCalcMemRequired(ConLod(lodmin),
										ConLod(lodmax),
										ConAspect(aspect),
										fmt);

    GDBG_INFO(80, "%s: %i bytes\n", FN_NAME, ret);

	return ret;

#undef FN_NAME
}

//
// grTexClampMode
//
NAKED_CALL void FX_CALL grTexClampMode( GrChipID_t tmu,
								   GrTextureClampMode_t s_clampmode,
								   GrTextureClampMode_t t_clampmode )
{
#define FN_NAME "grTexClampMode"
    GDBG_INFO(80, "%s\n", FN_NAME);

	using Glide3::grTexClampMode;
	VOID_ASM_JMP (grTexClampMode,(tmu,s_clampmode,t_clampmode));

#undef FN_NAME
}

//
// grTexCombine
//
void FX_CALL grTexCombine( GrChipID_t tmu,
							GrCombineFunction_t rgb_function,
							GrCombineFactor_t rgb_factor, 
							GrCombineFunction_t alpha_function,
							GrCombineFactor_t alpha_factor,
							FxBool rgb_invert,
							FxBool alpha_invert)
{
#define FN_NAME "grTexCombine"
    GDBG_INFO(80, "%s\n", FN_NAME);

	FxU32 newMask = theState.tmuMask;
	FxU32 tmuMask = GR_TMUMASK_TMU0 << tmu;
	
	// Check for passthrough case
	if((rgb_function == GR_COMBINE_FUNCTION_SCALE_OTHER) && (rgb_factor == GR_COMBINE_FACTOR_ONE) &&
		(alpha_function == GR_COMBINE_FUNCTION_SCALE_OTHER) && (alpha_factor == GR_COMBINE_FACTOR_ONE)) {
		newMask &= ~tmuMask;
	    GDBG_INFO(80, "%s: Passthrough on TMU%i\n", FN_NAME, tmu);
	}
	else {
		newMask |= tmuMask;
	}

	Glide3::grTexCombine(tmu, rgb_function, rgb_factor, alpha_function, alpha_factor, rgb_invert, alpha_invert);

	if (newMask != theState.tmuMask) {
		theState.tmuMask = newMask;
	    GDBG_INFO(80, "%s: Need to resetup GrVertex TMU info\n", FN_NAME);
		g2SetupGrVertexTMUs();
	}

#undef FN_NAME
}

//
// grTexDetailControl
//
NAKED_CALL void FX_CALL  grTexDetailControl( GrChipID_t tmu, int lod_bias,
											FxU8 detail_scale, float detail_max )
{
#define FN_NAME "grTexDetailControl"
    GDBG_INFO(80, "%s\n", FN_NAME);

	using Glide3::grTexDetailControl;
	VOID_ASM_JMP (grTexDetailControl,(tmu,lod_bias,detail_scale,detail_max));

#undef FN_NAME
}


//
// grTexDownloadMipMap
//
void FX_CALL grTexDownloadMipMap( GrChipID_t tmu,
                     FxU32      startAddress,
                     FxU32      evenOdd,
                     GrTexInfo  *info )
{
#define FN_NAME "grTexDownloadMipMap"
    GDBG_INFO(80, "%s: TMU%i %08x %i %08X\n", FN_NAME, tmu, startAddress, evenOdd, info);
    GDBG_INFO(80, "%s: %i %i %i %i\n", FN_NAME, info->aspectRatioLog2, info->smallLodLog2, info->smallLodLog2, info->format);

	GrTexInfo	newInfo;
	ConGrTexInfo(newInfo, info);

	Glide3::grTexDownloadMipMap(tmu, startAddress, evenOdd, &newInfo);

#undef FN_NAME
}

//
// grTexDownloadMipMapLevelPartial
//
FxBool FX_CALL grTexDownloadMipMapLevelPartial( GrChipID_t        tmu,
                                 FxU32             startAddress,
                                 GrLOD_t           thisLod,
                                 GrLOD_t           largeLod,
                                 GrAspectRatio_t   aspectRatio,
                                 GrTextureFormat_t format,
                                 FxU32             evenOdd,
                                 void              *data,
                                 int               start,
                                 int               end )
{
#define FN_NAME "grTexDownloadMipMapLevelPartial"
    GDBG_INFO(80, "%s: TMU%i\n", FN_NAME, tmu);

	return Glide3::grTexDownloadMipMapLevelPartial(tmu, startAddress,
				ConLod(thisLod), ConLod(largeLod), 
				ConAspect(aspectRatio), format, evenOdd, data,
				start, end );

#undef FN_NAME
}

//
// grTexDownloadMipMapLevelPartialWidthExt
//
FxBool FX_CALL grTexDownloadMipMapLevelPartialRowExt( GrChipID_t        tmu,
                                 FxU32             startAddress,
                                 GrLOD_t           thisLod,
                                 GrLOD_t           largeLod,
                                 GrAspectRatio_t   aspectRatio,
                                 GrTextureFormat_t format,
                                 FxU32             evenOdd,
                                 void              *data,
                                 int               row,
								 int			   min_s,
								 int			   max_s)
{
#define FN_NAME "grTexDownloadMipMapLevelPartialRowExt"
    GDBG_INFO(80, "%s: TMU%i\n", FN_NAME, tmu);

	return Glide3::grTexDownloadMipMapLevelPartialRowExt(tmu, startAddress,
				ConLod(thisLod), ConLod(largeLod), 
				ConAspect(aspectRatio), format, evenOdd, data,
				row, min_s, max_s );

#undef FN_NAME
}

//
// grTexDownloadMipMapLevel
//
void FX_CALL  grTexDownloadMipMapLevel( GrChipID_t        tmu,
									FxU32             startAddress,
									GrLOD_t           thisLod,
									GrLOD_t           largeLod,
									GrAspectRatio_t   aspectRatio,
									GrTextureFormat_t format,
									FxU32             evenOdd,
									void              *data )
{
#define FN_NAME "grTexDownloadMipMapLevel"
    GDBG_INFO(80, "%s: TMU%i\n", FN_NAME, tmu);

	Glide3::grTexDownloadMipMapLevel(tmu, startAddress, ConLod(thisLod), ConLod(largeLod), 
		ConAspect(aspectRatio), format, evenOdd, data);
#undef FN_NAME
}

//
// grTexDownloadTable
//
void FX_CALL grTexDownloadTable( GrChipID_t tmu, GrTexTable_t type, void *data )
{
#define FN_NAME "grTexDownloadTable"
    GDBG_INFO(80, "%s: TMU%i %i %08X\n", FN_NAME, tmu, type, data);

	//
	// Uh oh, problems Jim
	//
	// Due to a bug in Avenger and Napalm, Glide 3 doesn't allow pointcasting
	// palettes by default. However, it is still possible to do it on those
	// cards, although it's buggy. So what I've done is create an extension
	// that will allow me to pointcast on Glide3. 
	// 

	// Glide supports the POINTCAST extension. Yay!
	if (Glide3::grTexDownloadTableExt) {
		Glide3::grTexDownloadTableExt(tmu, type, data);
	}
	// Oh no, glide doesn't support the POINTCAST extenstion. Could be messy
	else {
		// Download to the Hardware
		Glide3::grTexDownloadTable(type, data);

		// If we only have 1 TMU, just return
		if (nsInfo.num_tmus == 1) return;

		// Copy the table to the correct buffer in the state
		if (type == GR_TEXTABLE_PALETTE) {
			theState.tmuTables[tmu].texPalette = * (GuTexPalette*) data;
			theState.texPaletteActive = tmu;
		}
		else if (type == GR_TEXTABLE_NCC0 || type == GR_TEXTABLE_NCC1) {

			// Here's a slight trick. TMU1 will have it's NCC table inverted.
			if (tmu == GR_TMU1) {
				if (type == GR_TEXTABLE_NCC1) type = GR_TEXTABLE_NCC0;
				else if (type == GR_TEXTABLE_NCC0) type = GR_TEXTABLE_NCC1;
			}

			theState.tmuTables[tmu].nccTable[type]= * (GuNccTable*) data;
			theState.nccTableActive[type] = tmu;
		}
	}

#undef FN_NAME
}

//
// grTexDownloadTablePartial
//
void FX_CALL grTexDownloadTablePartial( GrChipID_t tmu, GrTexTable_t type, 
			                           void *data, int start, int end )
{
#define FN_NAME "grTexDownloadTablePartial"
    GDBG_INFO(80, "%s: TMU%i %i %08X %i %i\n", FN_NAME, tmu, type, data, start, end);

	//
	// Uh oh, problems Jim.
	//
	// See grTexDownloadTable
	//

	// Only palette supported by this func. If someone attempts to use NCC
	// tables, they are a fool, SDK says NCC not supported so we will just fail
	// quietly
	if (type != GR_TEXTABLE_PALETTE) return;

	// Glide supports the POINTCAST extension. Yay!
	if (Glide3::grTexDownloadTablePartialExt) {
		Glide3::grTexDownloadTablePartialExt(tmu, type, data, start, end);
	} 
	// Oh no, glide doesn't support the POINTCAST extenstion. Could be messy
	else {

		// If we only have 1 TMU, just do it and return
		if (nsInfo.num_tmus == 1) {
			Glide3::grTexDownloadTablePartial(type, data, start, end);
			return;
		}

		// Otherwise do all the hacky stuff

		// Just use memcpy. It's replaced by an intrinsic in msvc anyway
		// Nejc: start and end are both INCLUSIVE, so the run is end-start+1
		// entries.  Copying end-start dropped the last entry of every range,
		// which then stayed at whatever the shadow was initialised to (zero,
		// i.e. black) and got pushed to the hardware by the upload below.
		memcpy (start + theState.tmuTables[tmu].texPalette.data,
					start + (FxU32 *) data, (end-start+1)*sizeof(FxU32));

		// If we aren't the active palette, we will need to upload the entire thing
		if (theState.texPaletteActive != tmu) {
			Glide3::grTexDownloadTable(type, &theState.tmuTables[tmu].texPalette);
			theState.texPaletteActive = tmu;
		}
		// But is we are active we only need to update
		else {
			Glide3::grTexDownloadTablePartial(type, data, start, end);
		}
	}

#undef FN_NAME
}

//
// grTexFilterMode
//
NAKED_CALL void FX_CALL grTexFilterMode( GrChipID_t tmu,
						GrTextureFilterMode_t minfilter_mode,
						GrTextureFilterMode_t magfilter_mode)
{
#define FN_NAME "grTexFilterMode"
    GDBG_INFO(80, "%s\n", FN_NAME);

	using Glide3::grTexFilterMode;
	VOID_ASM_JMP (grTexFilterMode,(tmu,minfilter_mode,magfilter_mode));

#undef FN_NAME
}

//
// grTexLodBiasValue
//
NAKED_CALL void FX_CALL grTexLodBiasValue(GrChipID_t tmu, float bias )
{
#define FN_NAME "grTexLodBiasValue"
    GDBG_INFO(80, "%s\n", FN_NAME);

	using Glide3::grTexLodBiasValue;
	VOID_ASM_JMP (grTexLodBiasValue,(tmu,bias));

#undef FN_NAME
}

//
// grTexMaxAddress
//
//#ifdef _DEBUG
#if 1
FxU32 FX_CALL grTexMaxAddress( GrChipID_t tmu )
{
#define FN_NAME "grTexMaxAddress"
    GDBG_INFO(80, "%s: TMU%i\n", FN_NAME, tmu);

	FxU32 ret = Glide3::grTexMaxAddress(tmu);
	if (!ret) {
		ret =  0x200000;
		GDBG_INFO(80, "%s: Couldn't get max address\n", FN_NAME);
	}
    GDBG_INFO(80, "%s: %i bytes\n", FN_NAME, ret);
	return ret;

#undef FN_NAME
}
#else
NAKED_CALL FxU32 FX_CALL grTexMaxAddress( GrChipID_t tmu )
{
#define FN_NAME "grTexMaxAddress"
    GDBG_INFO(80, "%s\n", FN_NAME);

	using Glide3::grTexMaxAddress;
	ASM_JMP (grTexMaxAddress,(tmu));

#undef FN_NAME
}
#endif

//
// grTexMinAddress
//
NAKED_CALL FxU32 FX_CALL grTexMinAddress( GrChipID_t tmu )
{
#define FN_NAME "grTexMinAddress"
    GDBG_INFO(80, "%s\n", FN_NAME);

	using Glide3::grTexMinAddress;
	ASM_JMP (grTexMinAddress,(tmu));

#undef FN_NAME
}

//
// grTexMipMapMode
//
NAKED_CALL void FX_CALL grTexMipMapMode( GrChipID_t     tmu, 
						GrMipMapMode_t mode,
						FxBool         lodBlend )
{
#define FN_NAME "grTexMipMapMode"
    GDBG_INFO(80, "%s\n", FN_NAME);

	using Glide3::grTexMipMapMode;
	VOID_ASM_JMP (grTexMipMapMode,(tmu,mode,lodBlend));

#undef FN_NAME
}

//
// grTexMipMapMode
//
NAKED_CALL void FX_CALL grTexMultibase( GrChipID_t tmu, FxBool enable )
{
#define FN_NAME "grTexMultibase"
    GDBG_INFO(80, "%s\n", FN_NAME);

	using Glide3::grTexMultibase;
	VOID_ASM_JMP (grTexMultibase,(tmu,enable));

#undef FN_NAME
}

//
// grTexMultibaseAddress
//
void FX_CALL grTexMultibaseAddress( GrChipID_t       tmu,
                       GrTexBaseRange_t range,
                       FxU32            startAddress,
                       FxU32            evenOdd,
                       GrTexInfo        *info )
{
#define FN_NAME "grTexMultibaseAddress"
    GDBG_INFO(80, "%s (%i,%i,%i,%i,0x%X)\n", FN_NAME, tmu, range, startAddress,evenOdd,info);

	GrTexInfo	newInfo;
	ConGrTexInfo(newInfo, info);
	Glide3::grTexMultibaseAddress(tmu,range,startAddress,evenOdd, &newInfo);

#undef FN_NAME
}

//
// grTexNCCTable
//
void FX_CALL grTexNCCTable( GrChipID_t tmu, GrNCCTable_t table )
{
#define FN_NAME "grTexNCCTable"
    GDBG_INFO(80, "%s: TMU%i %i\n", FN_NAME, tmu, table);

	//
	// Uh oh, problems Jim.
	//
	// See grTexDownloadTable
	//

	// Glide supports the POINTCAST extension. Yay!
	if (Glide3::grTexNCCTableExt) {
		Glide3::grTexNCCTableExt(tmu, table);
	}
	// Oh no, glide doesn't support the POINTCAST extenstion. Could be messy
	else {

		// Here's a slight trick. TMU1 will have it's NCC table inverted.
		if (tmu == GR_TMU1) {
			if (table == GR_TEXTABLE_NCC1) table = GR_TEXTABLE_NCC0;
			else if (table == GR_TEXTABLE_NCC0) table = GR_TEXTABLE_NCC1;
		}

		// First check to make sure this tmu's ncctable is the active one. If it's not,
		// download it, and set this tmu active, only if we are multitexturing
		if (nsInfo.num_tmus != 1 && theState.nccTableActive[table] != tmu) {
			Glide3::grTexDownloadTable(table, theState.tmuTables[tmu].nccTable+table);
			theState.nccTableActive[table] = tmu;
			theState.tmuTables[tmu].nccSelected = table;
		}

		Glide3::grTexNCCTable(table);
	}

#undef FN_NAME
}

//
// grTexSource
//
void FX_CALL grTexSource( GrChipID_t tmu,
						FxU32      startAddress,
						FxU32      evenOdd,
						GrTexInfo  *info )
{
#define FN_NAME "grTexSource"
    GDBG_INFO(80, "%s: TMU%i %08x %i %08X\n", FN_NAME, tmu, startAddress, evenOdd, info);

	// Ok, Hack time. If POINTCAST isn't supported, we need to be sure that the
	// correct table is loaded before setting the texture source. In all 
	// ordinary cases this shouldn't be a problem. Few games are going to do
	// multitexture with different tables in both TMUs. If the games have a problem,
	// there is the disable multitexture registry setting

	if (nsInfo.num_tmus != 1 && !Glide3::grTexDownloadTableExt) {

		// First handle paletted cause they are easy
		if (info->format == GR_TEXFMT_P_8 || info->format == GR_TEXFMT_AP_88) {

			// It's not the active palette, so download it.
			// Nejc: the test is on texPaletteActive itself.  A stray ! made this
			// (texPaletteActive == 0) != tmu, which is true for every TMU0
			// bind once texPaletteActive has settled at 0 -- so the shadow
			// palette was re-uploaded over the real one on every single
			// palettised grTexSource, undoing the game's partial downloads.
			if (theState.texPaletteActive != tmu) {
				Glide3::grTexDownloadTable(GR_TEXTABLE_PALETTE, &theState.tmuTables[tmu].texPalette);
				theState.texPaletteActive = tmu;
			}
		}
		else if (info->format == GR_TEXFMT_YIQ_422 || info->format == GR_TEXFMT_AYIQ_8422) {
		
			// NCC is slightly more complex since both TMUs can have 2 tables
			GrNCCTable_t table = theState.tmuTables[tmu].nccSelected;

			// Make sure the selected NCC table is actually loaded
			if (theState.nccTableActive[table] != tmu) {
				Glide3::grTexDownloadTable(table, theState.tmuTables[tmu].nccTable+table);
				theState.nccTableActive[table] = tmu;
			}

			// Always make sure that the ncc table we want is set active
			Glide3::grTexNCCTable(table);
		}
	}

	// Now convert the grTexInfo
	GrTexInfo	newInfo;
	ConGrTexInfo(newInfo, info);

	// And set the texture active
	Glide3::grTexSource(tmu, startAddress, evenOdd, &newInfo);

#undef FN_NAME
}

//
// grTexTextureMemRequired
//
FxU32 FX_CALL grTexTextureMemRequired( FxU32 evenOdd, GrTexInfo *info )

{
#define FN_NAME "grTexTextureMemRequired"
    GDBG_INFO(80, "%s: %i %08X\n", FN_NAME, evenOdd, info);

	GrTexInfo	newInfo;
	ConGrTexInfo(newInfo, info);

	FxU32 ret = Glide3::grTexTextureMemRequired(evenOdd, &newInfo);

    GDBG_INFO(80, "%s: %i bytes\n", FN_NAME, ret);
	return ret;
#undef FN_NAME
}

//
// Todo
//

//
// ConvertAndDownloadRle
//
void FX_CALL ConvertAndDownloadRle(
						GrChipID_t        tmu,
						FxU32             startAddress,
						GrLOD_t           thisLod,
						GrLOD_t           largeLod,
						GrAspectRatio_t   aspectRatio,
						GrTextureFormat_t format,
						FxU32             evenOdd,
						FxU8              *bm_data,
						long              bm_h,
						FxU32             u0,
						FxU32             v0,
						FxU32             width,
						FxU32             height,
						FxU32             dest_width,
						FxU32             dest_height,
						FxU16             *tlut)
{
#define FN_NAME "ConvertAndDownloadRle"
    GDBG_INFO(80, "%s: TMU%i\n", FN_NAME, tmu);
#undef FN_NAME
}

