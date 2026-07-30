//
// Glide 2 to Glide 3 Translator for GlideXP
//
// g2tex.h : Glide 2 Texturing Functions
//
// Copyright (c) 2002 Ryan Nunn
//
// Just a note that this is written in C++ so I can use namespaces.
// No other C++ features are used.
//

namespace Glide3 {

	bool	SetupTexFunctions();
	bool	FreeTexFunctions();

	typedef	FxU32 (FX_CALL * grTexCalcMemRequired_proc) (GrLOD_t, GrLOD_t, GrAspectRatio_t, GrTextureFormat_t);
	typedef	void (FX_CALL * grTexClampMode_proc) ( GrChipID_t, GrTextureClampMode_t, GrTextureClampMode_t );
	typedef	void (FX_CALL * grTexCombine_proc) ( GrChipID_t, GrCombineFunction_t, GrCombineFactor_t, GrCombineFunction_t, GrCombineFactor_t, FxBool, FxBool);
	typedef	void (FX_CALL * grTexDetailControl_proc) ( GrChipID_t tmu, int lod_bias, FxU8 detail_scale, float detail_max );
	typedef	void (FX_CALL * grTexDownloadMipMap_proc) ( GrChipID_t, FxU32, FxU32, GrTexInfo *);
	typedef	void (FX_CALL * grTexDownloadMipMapLevel_proc) ( GrChipID_t, FxU32, GrLOD_t, GrLOD_t, GrAspectRatio_t, GrTextureFormat_t, FxU32, void* );
	typedef	FxBool (FX_CALL * grTexDownloadMipMapLevelPartial_proc) ( GrChipID_t, FxU32, GrLOD_t, GrLOD_t, GrAspectRatio_t, GrTextureFormat_t, FxU32, void*, int, int );
	typedef	void (FX_CALL * grTexDownloadTable_proc) ( GrTexTable_t type, void *data );
	typedef	void (FX_CALL * grTexDownloadTableExt_proc) ( GrChipID_t tmu, GrTexTable_t type, void *data );
	typedef	void (FX_CALL * grTexDownloadTablePartial_proc) ( GrTexTable_t type, void *data, int start, int end );
	typedef	void (FX_CALL * grTexDownloadTablePartialExt_proc) ( GrChipID_t tmu, GrTexTable_t type, void *data, int start, int end );
	typedef	void (FX_CALL * grTexFilterMode_proc) ( GrChipID_t, GrTextureFilterMode_t, GrTextureFilterMode_t );
	typedef	void (FX_CALL * grTexLodBiasValue_proc) (GrChipID_t tmu, float bias );
	typedef	FxU32 (FX_CALL * grTexMaxAddress_proc) ( GrChipID_t );
	typedef	FxU32 (FX_CALL * grTexMinAddress_proc) ( GrChipID_t );
	typedef	void (FX_CALL * grTexMipMapMode_proc) ( GrChipID_t, GrMipMapMode_t, FxBool );
	typedef	void (FX_CALL * grTexMultibase_proc) ( GrChipID_t tmu, FxBool enable );
	typedef	void (FX_CALL * grTexMultibaseAddress_proc) ( GrChipID_t, GrTexBaseRange_t, FxU32, FxU32, GrTexInfo* );
	typedef	void (FX_CALL * grTexNCCTable_proc) ( GrNCCTable_t table );
	typedef	void (FX_CALL * grTexNCCTableExt_proc) ( GrChipID_t tmu, GrNCCTable_t table );
	typedef	void (FX_CALL * grTexSource_proc) ( GrChipID_t, FxU32, FxU32, GrTexInfo * );
	typedef	FxU32 (FX_CALL * grTexTextureMemRequired_proc) ( FxU32 evenOdd, GrTexInfo *info );
	typedef void (FX_CALL * grTextureBuffer_proc)( GrChipID_t tmu, FxU32 startAddress, GrLOD_t thisLOD, GrLOD_t largeLOD, GrAspectRatio_t aspectRatio, GrTextureFormat_t format, FxU32 odd_even_mask );
	typedef void (FX_CALL * grTextureAuxBuffer_proc)( GrChipID_t tmu, FxU32 startAddress, GrLOD_t thisLOD, GrLOD_t largeLOD, GrAspectRatio_t aspectRatio, GrTextureFormat_t format, FxU32 odd_even_mask );
	typedef FxBool (FX_CALL * grTexDownloadMipMapLevelPartialRowExt_proc) ( GrChipID_t, FxU32, GrLOD_t, GrLOD_t, GrAspectRatio_t, GrTextureFormat_t, FxU32, void*, int, int, int );

	extern	grTexCalcMemRequired_proc		grTexCalcMemRequired;
	extern	grTexClampMode_proc				grTexClampMode;
	extern	grTexCombine_proc				grTexCombine;
	extern	grTexDetailControl_proc			grTexDetailControl;
	extern	grTexDownloadMipMap_proc		grTexDownloadMipMap;
	extern	grTexDownloadMipMapLevel_proc	grTexDownloadMipMapLevel;
	extern	grTexDownloadMipMapLevelPartial_proc	grTexDownloadMipMapLevelPartial;
	extern	grTexDownloadTable_proc			grTexDownloadTable;
	extern	grTexDownloadTableExt_proc		grTexDownloadTableExt;
	extern	grTexDownloadTablePartial_proc	grTexDownloadTablePartial;
	extern	grTexDownloadTablePartialExt_proc	grTexDownloadTablePartialExt;
	extern	grTexLodBiasValue_proc			grTexLodBiasValue;
	extern	grTexFilterMode_proc			grTexFilterMode;
	extern	grTexMaxAddress_proc			grTexMaxAddress;
	extern	grTexMinAddress_proc			grTexMinAddress;
	extern	grTexMipMapMode_proc			grTexMipMapMode;
	extern	grTexMultibase_proc				grTexMultibase;
	extern	grTexMultibaseAddress_proc		grTexMultibaseAddress;
	extern	grTexNCCTable_proc				grTexNCCTable;
	extern	grTexNCCTableExt_proc			grTexNCCTableExt;
	extern	grTexSource_proc				grTexSource;
	extern	grTexTextureMemRequired_proc	grTexTextureMemRequired;
	extern	grTextureBuffer_proc			grTextureBufferExt;
	extern	grTextureAuxBuffer_proc			grTextureAuxBufferExt;
	extern	grTexDownloadMipMapLevelPartialRowExt_proc	grTexDownloadMipMapLevelPartialRowExt;
};

extern "C" FX_ENTRY void FX_CALL ConvertAndDownloadRle(
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
						FxU16             *tlut);

FxBool FX_CALL 
grTexDownloadMipMapLevelPartialRowExt(GrChipID_t        tmu,
										FxU32             startAddress,
										GrLOD_t           thisLod,
										GrLOD_t           largeLod,
										GrAspectRatio_t   aspectRatio,
										GrTextureFormat_t format,
										FxU32             evenOdd,
										void              *data,
										int               row,
										int               min_s,
										int               max_s);

//
// A cupple of helper inline functions
//

// Lod converting is easy. Just subtract the lod from 8 to switch between the 2
inline GrLOD_t _fastcall ConLod (GrLOD_t lod) {
	return 8 - lod;
}

// Aspect converting is just as easy. Substact the lod from 3
inline GrAspectRatio_t _fastcall ConAspect (GrAspectRatio_t aspect) {
	return 3 - aspect;
}

// Convert a GrTexInfo structure
inline void _fastcall ConGrTexInfo(GrTexInfo &out, GrTexInfo *in) {
	if (&out != in) {
		out.format = in->format;
		out.data = in->data;
	}
	out.smallLodLog2 = ConLod (in->smallLodLog2);
	out.largeLodLog2 = ConLod (in->largeLodLog2);
	out.aspectRatioLog2 = ConAspect (in->aspectRatioLog2);
}

