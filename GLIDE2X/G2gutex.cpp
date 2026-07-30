//
// Glide 2 to Glide 3 Translator for GlideXP
//
// g2gutex.cpp : Glide 2 Utility Texture Memory Management Functions
//
// Copyright (c) 2002 Ryan Nunn
//
// Just a note that this is written in C++ so I can use namespaces.
// No other C++ features are used.
//

#include "g2pch.h"

//
// guTexAllocateMemory
//
GrMipMapId_t FX_CALL guTexAllocateMemory ( GrChipID_t tmu,
										  FxU8 odd_even_mask,
										  int width, int height,
										  GrTextureFormat_t format,
										  GrMipMapMode_t mipmap_mode,
										  GrLOD_t small_lod, GrLOD_t large_lod,
										  GrAspectRatio_t aspect_ratio,
										  GrTextureClampMode_t s_clamp_mode,
										  GrTextureClampMode_t t_clamp_mode,
										  GrTextureFilterMode_t minfilter_mode,
										  GrTextureFilterMode_t magfilter_mode,
										  float lod_bias,
										  FxBool trilinear
										  )
{
#define FN_NAME "guTexAllocateMemory"
	GDBG_INFO(80,"%s\n", FN_NAME);
	
	// First thing, Can we actually allocate a new one?
	if (nsInfo.free_mmid >= MAX_MIPMAPS_PER_SST )
		return (GrMipMapId_t) GR_NULL_MIPMAP_HANDLE;
	
	// Get Size of the Texture
	GrTexInfo info;
	info.smallLod = small_lod;
	info.largeLod = large_lod;
	info.aspectRatio = aspect_ratio;
	info.format = format;
	FxU32 memrequired = grTexTextureMemRequired(odd_even_mask, &info);
	
	// Make sure to not cross 2 MByte texture boundry
	if ((nsInfo.tmu_state[tmu].freemem_base < 0x200000) &&(nsInfo.tmu_state[tmu].freemem_base + memrequired > 0x200000))
		nsInfo.tmu_state[tmu].freemem_base = 0x200000;
	
	// If we have enough memory and a free mip map handle then go for it
	if ( guTexMemQueryAvail( tmu ) < memrequired ) return (GrMipMapId_t) GR_NULL_MIPMAP_HANDLE;
	
	// Get the new mmid
	GrMipMapId_t mmid = nsInfo.free_mmid++;
	
	// Put the texture at the free base location
	FxU32 location = nsInfo.tmu_state[tmu].freemem_base;
	GDBG_INFO(81,"%s: location = 0x%x (in bytes)\n",FN_NAME, location);
	
	// Increment the base pointer to past the end of this texture
	nsInfo.tmu_state[tmu].freemem_base += memrequired;
	
	// Fill in the mm_table data for this mip map
	nsInfo.data[mmid].format         = format;
	nsInfo.data[mmid].mipmap_mode    = mipmap_mode;
	nsInfo.data[mmid].magfilter_mode = magfilter_mode;
	nsInfo.data[mmid].minfilter_mode = minfilter_mode;
	nsInfo.data[mmid].s_clamp_mode   = s_clamp_mode;
	nsInfo.data[mmid].t_clamp_mode   = t_clamp_mode;
	nsInfo.data[mmid].float_lod_bias = lod_bias;
	nsInfo.data[mmid].lod_min        = small_lod;
	nsInfo.data[mmid].lod_max        = large_lod;
	nsInfo.data[mmid].tmu            = tmu;
	nsInfo.data[mmid].odd_even_mask  = odd_even_mask;
	nsInfo.data[mmid].tmu_base_address = location;
	nsInfo.data[mmid].trilinear      = trilinear;
	nsInfo.data[mmid].aspect_ratio   = aspect_ratio;
	nsInfo.data[mmid].data           = 0;
	//nsInfo.data[mmid].ncc_table      = 0; */
	nsInfo.data[mmid].sst            = 0;
	nsInfo.data[mmid].valid          = FXTRUE;
	nsInfo.data[mmid].width          = width;
	nsInfo.data[mmid].height         = height;
	
	// Fixme maybe?
	nsInfo.data[mmid].tTextureMode   = 0;
	nsInfo.data[mmid].tLOD           = 0;
	nsInfo.data[mmid].lod_bias       = 0;
	
	return mmid;
#undef FN_NAME
}

//
// guTexChangeAttributes 
//
FxBool FX_CALL guTexChangeAttributes ( GrMipMapId_t mmid,
									  int width, int height,
									  GrTextureFormat_t fmt,
									  GrMipMapMode_t mm_mode,
									  GrLOD_t smallest_lod, GrLOD_t largest_lod,
									  GrAspectRatio_t aspect,
									  GrTextureClampMode_t s_clamp_mode,
									  GrTextureClampMode_t t_clamp_mode,
									  GrTextureFilterMode_t minFilterMode,
									  GrTextureFilterMode_t magFilterMode
									  )
{
#define FN_NAME "guTexChangeAttributes" 
	GDBG_INFO(80,"%s\n", FN_NAME);
	
	// Make sure that mmid is valie
	if ( mmid == GR_NULL_MIPMAP_HANDLE || mmid  >= nsInfo.free_mmid )
		return FXFALSE;
	
	// Get the mminfo
	GrMipMapInfo *mminfo = nsInfo.data+mmid;
	
	// Update the details
	if ( fmt != -1 ) mminfo->format = fmt;
	if ( mm_mode != -1 ) mminfo->mipmap_mode = mm_mode;
	if ( smallest_lod != -1 ) mminfo->lod_min = smallest_lod;
	if ( largest_lod != -1 ) mminfo->lod_max = largest_lod;
	if ( minFilterMode != -1 ) mminfo->minfilter_mode = minFilterMode;
	if ( magFilterMode != -1 ) mminfo->magfilter_mode = magFilterMode;
	if ( s_clamp_mode != -1 ) mminfo->s_clamp_mode = s_clamp_mode;
	if ( t_clamp_mode != -1 ) mminfo->t_clamp_mode = t_clamp_mode;
	if ( aspect != -1 ) mminfo->aspect_ratio = aspect;
	if ( width != -1 ) mminfo->width = width;
	if ( height != -1 ) mminfo->height = height;
	
	return FXTRUE;
	
#undef FN_NAME
}

//
// guTexDownloadMipMap
//
void FX_CALL guTexDownloadMipMap (GrMipMapId_t mmid, const void *src, const GuNccTable *ncc_table )
{
#define FN_NAME "guTexDownloadMipMap"
	GDBG_INFO(80,"%s: (%d,0x%x,0x%x)\n", FN_NAME, mmid, src, ncc_table);
	
	// Check for valid mipmap
	if (src == NULL || mmid == GR_NULL_MIPMAP_HANDLE || mmid >= nsInfo.free_mmid) {
		GDBG_INFO(80,"%s: Invalid mipmap\n", FN_NAME);
		return;
	}
	
	// Get the mminfo
	GrMipMapInfo *mminfo = nsInfo.data+mmid;
	
	// Bind NCC table
	if (mminfo->format == GR_TEXFMT_YIQ_422)
		memcpy ( &mminfo->ncc_table, ncc_table, sizeof(GuNccTable));
	
	// Text info
	GrTexInfo info;
	info.aspectRatio = mminfo->aspect_ratio;
	info.largeLod = mminfo->lod_max;
	info.smallLod = mminfo->lod_min;
	info.format = mminfo->format;
	info.data = mminfo->data = (void *) src;
	
	// Download it
	grTexDownloadMipMap(mminfo->tmu, mminfo->tmu_base_address, mminfo->odd_even_mask, &info);
	
#undef FN_NAME
} 

//
// guTexDownloadMipMapLevel
//
void FX_CALL guTexDownloadMipMapLevel (GrMipMapId_t mmid, GrLOD_t lod, const void **src_base)
{
#define FN_NAME "guTexDownloadMipMapLevel" 
	GDBG_INFO(80,"%s: (%d,%d,0x%x)\n",FN_NAME, mmid, lod, src_base);
	
	// Check for valid mipmap
	if (src_base == NULL || mmid == GR_NULL_MIPMAP_HANDLE || mmid >= nsInfo.free_mmid) {
		GDBG_INFO(80,"%s: Invalid mipmap\n", FN_NAME);
		return;
	}
	
	// Get the mminfo
	const GrMipMapInfo *mminfo = nsInfo.data+mmid;
	
	// Get ourselves an easy to use pointer to the buffer
	FxU8 **src = (FxU8 **) src_base;
	
	// download the level
	grTexDownloadMipMapLevel( mminfo->tmu, mminfo->tmu_base_address,
								lod, mminfo->lod_max, mminfo->aspect_ratio,
								mminfo->format, mminfo->odd_even_mask, *src );
	
	// update src_base to point to next mipmap level
	src += grTexCalcMemRequired(lod,lod,mminfo->aspect_ratio, mminfo->format);
	
#undef FN_NAME
}

//
// guTexGetCurrentMipMap
//
GrMipMapId_t FX_CALL guTexGetCurrentMipMap ( GrChipID_t tmu )
{
#define FN_NAME "guTexGetCurrentMipMap" 
	GDBG_INFO(80,"%s\n", FN_NAME);
	
	return theState.current_mm[tmu];
	
#undef FN_NAME
}

//
// guTexGetMipMapInfo
//
GrMipMapInfo * FX_CALL guTexGetMipMapInfo ( GrMipMapId_t mmid )
{
#define FN_NAME "guTexGetMipMapInfo" 
	GDBG_INFO(80,"%s (%i)\n", FN_NAME, mmid);
	
	return nsInfo.data+mmid;
	
#undef FN_NAME
}

//
// guTexMemQueryAvail
//
FxU32 FX_CALL guTexMemQueryAvail ( GrChipID_t tmu )
{
#define FN_NAME "guTexMemQueryAvail" 
	GDBG_INFO(80,"%s: %i bytes\n", FN_NAME, nsInfo.tmu_state[tmu].total_mem - nsInfo.tmu_state[tmu].freemem_base);
	
	return nsInfo.tmu_state[tmu].total_mem - nsInfo.tmu_state[tmu].freemem_base;
	
#undef FN_NAME
}

//
// guTexMemReset
//
void FX_CALL guTexMemReset ( void )
{
#define FN_NAME "guTexChangeAttributes" 
	GDBG_INFO(80,"%s\n", FN_NAME);
	
	memset( nsInfo.data, 0, sizeof( nsInfo.data ) );
	nsInfo.free_mmid = 0;

	#if !(ALLOW_POINTCASTS)
		nsInfo.next_ncc_table = 0;
		nsInfo.ncc_mmids[0] = nsInfo.ncc_mmids[1] = GR_NULL_MIPMAP_HANDLE;
		nsInfo.ncc_table[0] = nsInfo.ncc_table[1] = 0;
	#endif
	
	FxI32 tmu_mem;
	Glide3::grGet(GR_MEMORY_TMU,4,&tmu_mem);
	
	for ( int i = 0; i < GLIDE_NUM_TMU; i++ ) {
		theState.current_mm[i] = (GrMipMapId_t) GR_NULL_MIPMAP_HANDLE;
		nsInfo.tmu_state[i].freemem_base = 0;
		nsInfo.tmu_state[i].total_mem = tmu_mem;
		#if ALLOW_POINTCASTS
			nsInfo.tmu_state[i].next_ncc_table = 0;
			nsInfo.tmu_state[i].ncc_mmids[0] = nsInfo.tmu_state[i].ncc_mmids[1] = GR_NULL_MIPMAP_HANDLE;
			nsInfo.tmu_state[i].ncc_table[0] = nsInfo.tmu_state[i].ncc_table[1] = 0;
		#endif
	}
	
#undef FN_NAME
}

//
// guTexSource
//
void FX_CALL guTexSource (GrMipMapId_t mmid)
{
#define FN_NAME "guTexSource"
	GDBG_INFO(80,"%s (%i)\n", FN_NAME, mmid);
	
	// Make sure that mmid is valid
	if ( mmid == GR_NULL_MIPMAP_HANDLE || mmid  >= nsInfo.free_mmid )
		return;
	
	// get a pointer to the relevant GrMipMapInfo struct
	const GrMipMapInfo *mminfo = &nsInfo.data[mmid];
	int tmu = mminfo->tmu;
	
	// Setup the info
	GrTexInfo info;
	info.smallLodLog2 = mminfo->lod_min;
	info.largeLodLog2 = mminfo->lod_max;
	info.aspectRatioLog2 = mminfo->aspect_ratio;
	info.format = mminfo->format;
	info.data = mminfo->data;
	
	// Download the NCC table, if needed.
	if ((mminfo->format == GR_TEXFMT_YIQ_422) ||
		(mminfo->format == GR_TEXFMT_AYIQ_8422)) {

		#if ALLOW_POINTCASTS

			// The table we want to use
			int table;
			
			// See if it's already down there 
			if (nsInfo.tmu_state[tmu].ncc_mmids[0] == mmid)
				table = 0;	// Got it
			else if (nsInfo.tmu_state[tmu].ncc_mmids[1] == mmid) {
				table = 1;	// Got it
			} else {
				// We need to upload

				// Which table should we use?
				table = nsInfo.tmu_state[tmu].next_ncc_table;
				
				// Download NCC table (POINTCASTS!)
				grTexDownloadTable(tmu, table==0?GR_NCCTABLE_NCC0:GR_NCCTABLE_NCC1, (void *) &mminfo->ncc_table);
				
				// Set the mmid so we known it's down there
				nsInfo.tmu_state[tmu].ncc_mmids[table] = mmid;
				
				// Set the state to know which table was the LRA
				nsInfo.tmu_state[tmu].next_ncc_table = (table == 0 ? 1 : 0);
			}
			
			// Set the active table
			if (table == 0)	grTexNCCTable( tmu, GR_NCCTABLE_NCC0 );
			else grTexNCCTable( tmu, GR_NCCTABLE_NCC1 );

		#else // Don't ALLOW_POINTCASTS
			// The table we want to use
			int table;
			
			// See if it's already down there 
			if (nsInfo.ncc_mmids[0] == mmid)
				table = 0;	// Got it
			else if (nsInfo.ncc_mmids[1] == mmid) {
				table = 1;	// Got it
			} else {
				// We need to upload

				// Which table should we use?
				table = nsInfo.next_ncc_table;
				
				// Download NCC table
				Glide3::grTexDownloadTable(table==0?GR_NCCTABLE_NCC0:GR_NCCTABLE_NCC1, (void *) &mminfo->ncc_table);
				
				// Set the mmid so we known it's down there
				nsInfo.ncc_mmids[table] = mmid;
				
				// Set the state to know which table was the LRA
				nsInfo.next_ncc_table = (table == 0 ? 1 : 0);
			}
			
			// Set the active table
			if (table == 0)	Glide3::grTexNCCTable( GR_NCCTABLE_NCC0 );
			else Glide3::grTexNCCTable( GR_NCCTABLE_NCC1 );
		#endif
	} 
		
	// Set all the various required state. This isn't exactly efficent,
	// but it needs to be done. Hopefully Glide 3 will check to see if the
	// state has actually changed.

	Glide3::grTexMipMapMode (tmu, mminfo->mipmap_mode, mminfo->trilinear);
	Glide3::grTexFilterMode (tmu, mminfo->minfilter_mode, mminfo->magfilter_mode);
	Glide3::grTexClampMode(tmu, mminfo->s_clamp_mode, mminfo->t_clamp_mode);
	Glide3::grTexLodBiasValue(tmu, mminfo->float_lod_bias );
	
	// Set the source
	grTexSource( tmu, mminfo->tmu_base_address, mminfo->odd_even_mask, &info);
	
	// So we can know it's there
	theState.current_mm[tmu] = mmid;
	
#undef FN_NAME
} /* guTexSource */
