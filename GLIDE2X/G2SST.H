//
// Glide 2 to Glide 3 Translator for GlideXP
//
// g2sst.h : Glide 2 SST Functions
//
// Copyright (c) 2002 Ryan Nunn
//
// Just a note that this is written in C++ so I can use namespaces.
// No other C++ features are used.
//

namespace Glide3 {

	bool	SetupSSTFunctions();
	bool	FreeSSTFunctions();

	typedef void (FX_CALL * grFinish_proc) (void);
	typedef void (FX_CALL * grSstOrigin_proc) (GrOriginLocation_t origin);
	typedef FxBool (FX_CALL * grSstQueryBoards_proc) ( GrHwConfiguration *hwc );
	typedef FxBool (FX_CALL * grSstQueryHardware_proc) ( GrHwConfiguration *hwc );
	typedef void (FX_CALL * grSstSelect_proc) ( int which_sst );
	typedef GrContext_t (FX_CALL * grSstWinOpen_proc) ( FxU32, GrScreenResolution_t, GrScreenRefresh_t, GrColorFormat_t, GrOriginLocation_t, int, int );
	typedef FxBool (FX_CALL * grSstWinClose_proc) ( GrContext_t context );
	typedef GrContext_t (FX_CALL * grSstWinOpenExt_proc) ( FxU32, GrScreenResolution_t, GrScreenRefresh_t, GrColorFormat_t, GrOriginLocation_t, GrPixelFormat_t, int, int );

	extern	grFinish_proc					grFinish;
	extern	grSstOrigin_proc				grSstOrigin;
	extern	grSstQueryBoards_proc			grSstQueryBoards;
	extern	grSstQueryHardware_proc			grSstQueryHardware;
	extern	grSstSelect_proc				grSstSelect;
	extern	grSstWinOpen_proc				grSstWinOpen;
	extern	grSstWinClose_proc				grSstWinClose;
	extern	grSstWinOpenExt_proc			grSstWinOpenExt;
};

FxBool FX_CALL grSstWinOpenExt(
                FxU32                   hWnd,
                GrScreenResolution_t    resolution, 
                GrScreenRefresh_t       refresh, 
                GrColorFormat_t         format, 
                GrOriginLocation_t      origin, 
                GrPixelFormat_t         pixelformat,
                int                     nColBuffers,
                int                     nAuxBuffers);
