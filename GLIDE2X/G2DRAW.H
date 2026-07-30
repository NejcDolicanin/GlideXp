//
// Glide 2 to Glide 3 Translator for GlideXP
//
// g2draw.h : Glide 2 Drawing and related Functions
//
// Copyright (c) 2002 Ryan Nunn
//
// Just a note that this is written in C++ so I can use namespaces.
// No other C++ features are used.
//

namespace Glide3 {

	bool	SetupDrawFunctions();
	bool	FreeDrawFunctions();

	typedef void (FX_CALL * grAADrawTriangle_proc) ( const void *, const void *, const void *, FxBool, FxBool, FxBool );
	typedef	void (FX_CALL * grCullMode_proc) ( GrCullMode_t mode );
	typedef	void (FX_CALL * grDrawPoint_proc) ( const void *pt );
	typedef	void (FX_CALL * grDrawLine_proc) ( const void *v1, const void *v2 );
	typedef	void (FX_CALL * grDrawTriangle_proc) ( const void *a, const void *b, const void *c );
	typedef	void (FX_CALL * grDrawVertexArray_proc) (FxU32 mode, FxU32 Count, void *pointers);
	typedef	void (FX_CALL * grDrawVertexArrayContiguous_proc) (FxU32 mode, FxU32 Count, void *pointers, FxU32 stride);
	typedef	void (FX_CALL * grVertexLayout_proc) (FxU32 param, FxI32 offset, FxU32 mode);

	extern	grAADrawTriangle_proc			grAADrawTriangle;
	extern	grCullMode_proc					grCullMode;
	extern	grDrawPoint_proc				grDrawPoint;
	extern	grDrawLine_proc					grDrawLine;
	extern	grDrawTriangle_proc				grDrawTriangle;
	extern	grDrawVertexArray_proc			grDrawVertexArray;
	extern	grDrawVertexArrayContiguous_proc grDrawVertexArrayContiguous;
	extern	grVertexLayout_proc				grVertexLayout;
};
