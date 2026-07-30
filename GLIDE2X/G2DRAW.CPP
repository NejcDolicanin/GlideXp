//
// Glide 2 to Glide 3 Translator for GlideXP
//
// g2draw.cpp : Glide 2 Drawing and related Functions
//
// Copyright (c) 2002 Ryan Nunn
//
// Just a note that this is written in C++ so I can use namespaces.
// No other C++ features are used.
//

#include "g2pch.h"
#include <malloc.h>

namespace Glide3 {

grAADrawTriangle_proc			grAADrawTriangle = 0;
grCullMode_proc					grCullMode = 0;
grDrawPoint_proc				grDrawPoint = 0;
grDrawLine_proc					grDrawLine = 0;
grDrawTriangle_proc				grDrawTriangle = 0;
grDrawVertexArray_proc			grDrawVertexArray = 0;
grDrawVertexArrayContiguous_proc grDrawVertexArrayContiguous = 0;
grVertexLayout_proc				grVertexLayout = 0;

bool	SetupDrawFunctions()
{
#define FN_NAME "SetupDrawFunctions"
    GDBG_INFO(80, "%s\n", FN_NAME);

	bool ret = true;

	if (ret) ret = LoadFunc(grAADrawTriangle,24);
	if (ret) ret = LoadFunc(grCullMode,4);
	if (ret) ret = LoadFunc(grDrawPoint,4);
	if (ret) ret = LoadFunc(grDrawLine,8);
	if (ret) ret = LoadFunc(grDrawTriangle,12);
	if (ret) ret = LoadFunc(grDrawVertexArray,12);
	if (ret) ret = LoadFunc(grDrawVertexArrayContiguous,16);
	if (ret) ret = LoadFunc(grVertexLayout,12);

	return ret;
#undef FN_NAME
}

bool	FreeDrawFunctions()
{
#define FN_NAME "FreeDrawFunctions"
	//GDBG_INFO(80, "%s\n", FN_NAME);

	grAADrawTriangle = 0;
	grCullMode = 0;
	grDrawPoint = 0;
	grDrawLine = 0;
	grDrawTriangle = 0;
	grDrawVertexArray = 0;
	grDrawVertexArrayContiguous = 0;
	grVertexLayout = 0;

	return true;
#undef FN_NAME
}

};

#if 0
typedef struct {
	float sow0;	/* s/w texture coordinate */
	float tow4;	/* t/w texture coordinate */ 
	float oow8;	/* 1/w */
} GrTmuVertex;

typedef struct {
	float x0, y4, z8;                /* X, Y, and Z of scrn space -- Z is ignored */
	float r12, g16, b20;                /* R, G, B, ([0..255.0]) */
	float ooz24;                    /* 65535/Z (used for Z-buffering) */
	float a28;                      /* Alpha [0..255.0] */
	float oow32;                    /* 1/W (used for W-buffering, texturing) */
	GrTmuVertex  tmuvtx36[GLIDE_NUM_TMU];
} GrVertex;
#endif

//
// grCullMode
//
NAKED_CALL void FX_CALL grCullMode( GrCullMode_t mode )
{
#define FN_NAME "grCullMode"
    GDBG_INFO(80, "%s\n", FN_NAME);

	using Glide3::grCullMode;
	VOID_ASM_JMP (grCullMode,(mode));

#undef FN_NAME
}

//
// grAADrawLine
//
void FX_CALL grAADrawLine ( const GrVertex *v1, const GrVertex *v2 )
{
#define FN_NAME "grAADrawLine"
    GDBG_INFO(80, "%s\n", FN_NAME);

	Glide3::grEnable(GR_AA_ORDERED); 
	Glide3::grDrawLine(v1, v2);
	Glide3::grDisable(GR_AA_ORDERED); 

#undef FN_NAME
}

//
// grAADrawPoint
//
void FX_CALL grAADrawPoint ( const GrVertex *pt )
{
#define FN_NAME "grAADrawPoint"
    GDBG_INFO(80, "%s\n", FN_NAME);

	Glide3::grEnable(GR_AA_ORDERED); 
	Glide3::grDrawPoint(pt);
	Glide3::grDisable(GR_AA_ORDERED); 

#undef FN_NAME
}

//
// grAADrawPolygon
//
void FX_CALL grAADrawPolygon( int nVerts, const int ilist[], GrVertex vlist[] )
{
#define FN_NAME "grAADrawPolygon"
    GDBG_INFO(80, "%s\n", FN_NAME);

	Glide3::grEnable(GR_AA_ORDERED); 
	grDrawPolygon(nVerts, (int *) ilist, vlist);
	Glide3::grDisable(GR_AA_ORDERED); 

#undef FN_NAME
}

//
// grAADrawPolygonVertexList
//
void FX_CALL grAADrawPolygonVertexList( int nVerts, GrVertex vlist[] )
{
#define FN_NAME "grAADrawPolygonVertexList"
    GDBG_INFO(80, "%s\n", FN_NAME);

	Glide3::grEnable(GR_AA_ORDERED); 
	Glide3::grDrawVertexArrayContiguous(GR_POLYGON, nVerts, vlist, sizeof(GrVertex));
	Glide3::grDisable(GR_AA_ORDERED); 

#undef FN_NAME
}

//
// grAADrawTriangle
//
NAKED_CALL void FX_CALL grAADrawTriangle(const void *a, const void *b, const void *c,
							FxBool ab_antialias, FxBool bc_antialias, FxBool ca_antialias )
{
#define FN_NAME "grAADrawTriangle"
    GDBG_INFO(80, "%s\n", FN_NAME);

	using Glide3::grAADrawTriangle;
	VOID_ASM_JMP (grAADrawTriangle,(a,b,c,ab_antialias,bc_antialias,ca_antialias));

#undef FN_NAME
}


//
// grDrawPlanarPolygon
//
NAKED_CALL void FX_CALL grDrawPlanarPolygon ( int nVerts, int ilist[], GrVertex vlist[] )
{
#define FN_NAME "grDrawPlanarPolygon"
    GDBG_INFO(80, "%s\n", FN_NAME);

	VOID_ASM_JMP (grDrawPolygon,(nVerts, ilist,vlist));

#undef FN_NAME
}

//
// grDrawPlanarPolygonVertexList
//
void FX_CALL grDrawPlanarPolygonVertexList ( int nVerts, GrVertex vlist[] )
{
#define FN_NAME "grDrawPlanarPolygonVertexList"
    GDBG_INFO(80, "%s\n", FN_NAME);

	Glide3::grDrawVertexArrayContiguous(GR_POLYGON, nVerts, vlist, sizeof(GrVertex));

#undef FN_NAME
}

//
// grDrawPoint
//
NAKED_CALL void FX_CALL grDrawPoint( const void *pt )
{
#define FN_NAME "grDrawPoint"
    GDBG_INFO(80, "%s\n", FN_NAME);

	using Glide3::grDrawPoint;
	VOID_ASM_JMP (grDrawPoint,(pt));

#undef FN_NAME
}

//
// grDrawPolygon
//
void FX_CALL grDrawPolygon ( int nVerts, int ilist[], GrVertex vlist[] )
{
#define FN_NAME "grDrawPolygon"

	// We need to sort the list. Note that alloca allocates the memory on the stack
	// so it doesn't need to be freed
	GrVertex **sorted = (GrVertex **) alloca(sizeof(GrVertex*)*nVerts);
	const int *end = ilist + nVerts;

	while (ilist != end) {
		*sorted = vlist + *ilist;
		ilist++;
		sorted++;
	}

	Glide3::grDrawVertexArray(GR_POLYGON, nVerts, sorted-nVerts);
		
#undef FN_NAME

}

//
// grDrawPolygonVertexList
//
void FX_CALL grDrawPolygonVertexList ( int nVerts, GrVertex vlist[] )
{
#define FN_NAME "grDrawPolygonVertexList"
    GDBG_INFO(80, "%s\n", FN_NAME);

	Glide3::grDrawVertexArrayContiguous(GR_POLYGON, nVerts, vlist, sizeof(GrVertex));

#undef FN_NAME
}

//
// grDrawLine
//
NAKED_CALL void FX_CALL grDrawLine( const void *v1, const void *v2 )
{
#define FN_NAME "grDrawLine"
    GDBG_INFO(80, "%s\n", FN_NAME);

	using Glide3::grDrawLine;
	VOID_ASM_JMP(grDrawLine,(v1,v2));

#undef FN_NAME
}

//
// grDrawTriangle
//
NAKED_CALL void FX_CALL grDrawTriangle( const void *a, const void *b, const void *c )
{
#define FN_NAME "grDrawTriangle"
    GDBG_INFO(80, "%s\n", FN_NAME);

	using Glide3::grDrawTriangle;
	VOID_ASM_JMP (grDrawTriangle,(a,b,c));

#undef FN_NAME
}
