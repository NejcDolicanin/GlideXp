//
// Glide 2 to Glide 3 Translator for GlideXP
//
// g2clip.cpp : Glide 2 Utility Clipping Functions
//
// Copyright (c) 2002 Ryan Nunn
//
// Just a note that this is written in C++ so I can use namespaces.
// No other C++ features are used.
//

#include "g2pch.h"

/* Moved per GMT cleanup */
#define GU_PRIM_MAX_VERTICES 100

inline void calcParams(const GrVertex *a, const GrVertex *b, GrVertex *isect, float d)
{
	if (1) { //if (theState.paramIndex & STATE_REQUIRES_IT_DRGB) {
		isect->r = a->r + d * ( b->r - a->r );
		isect->g = a->g + d * ( b->g - a->g );
		isect->b = a->b + d * ( b->b - a->b );
	}
    
	if (1) { //if (theState.paramIndex & STATE_REQUIRES_IT_ALPHA) {
		isect->a        = a->a        + d * ( b->a - a->a );
	}
	
	if (1) { //if (theState.paramIndex & STATE_REQUIRES_OOZ) {
		isect->ooz = a->ooz + d * ( b->ooz - a->ooz );
	}
	
	if (1) { //if (theState.paramIndex & STATE_REQUIRES_OOW_FBI) {
		isect->oow = a->oow + d * ( b->oow - a->oow);
	}
	
	if (1) { //if (theState.paramIndex & STATE_REQUIRES_ST_TMU0) {
		isect->tmuvtx[0].oow =
			a->tmuvtx[0].oow + d * ( b->tmuvtx[0].oow - a->tmuvtx[0].oow );
		isect->tmuvtx[0].sow =
			a->tmuvtx[0].sow + d * ( b->tmuvtx[0].sow - a->tmuvtx[0].sow );
		isect->tmuvtx[0].tow =
			a->tmuvtx[0].tow + d * ( b->tmuvtx[0].tow - a->tmuvtx[0].tow );
	}
	
	if (1) { //if (theState.paramIndex & STATE_REQUIRES_ST_TMU1) {
		isect->tmuvtx[1].oow =
			a->tmuvtx[1].oow + d * ( b->tmuvtx[1].oow - a->tmuvtx[1].oow );
		isect->tmuvtx[1].sow =
			a->tmuvtx[1].sow + d * ( b->tmuvtx[1].sow - a->tmuvtx[1].sow );
		isect->tmuvtx[1].tow =
			a->tmuvtx[1].tow + d * ( b->tmuvtx[1].tow - a->tmuvtx[1].tow );
	}
	
#if (GLIDE_NUM_TMU > 2)
	if (1) { //if (theState.paramIndex & STATE_REQUIRES_ST_TMU2) {
		isect->tmuvtx[2].oow =
			a->tmuvtx[2].oow + d * ( b->tmuvtx[2].oow - a->tmuvtx[2].oow );
		isect->tmuvtx[2].sow =
			a->tmuvtx[2].sow + d * ( b->tmuvtx[2].sow - a->tmuvtx[2].sow );
		isect->tmuvtx[2].tow =
			a->tmuvtx[2].tow + d * ( b->tmuvtx[2].tow - a->tmuvtx[2].tow );
	}
#endif
	
} /* calcParams */

inline void intersectTop( const GrVertex *a, const GrVertex *b, GrVertex *intersect )
{
	float
		d = ( theState.clipwindowf_ymin - a->y ) / ( b->y - a->y );
	
	intersect->x        = a->x        + d * ( b->x - a->x );
	intersect->y        = theState.clipwindowf_ymin; 
	
	calcParams(a, b, intersect, d);
	
} /* intersectTop */

inline void
intersectBottom( const GrVertex *a, const GrVertex *b, GrVertex *intersect )
{
	float
		d = ( theState.clipwindowf_ymax - a->y ) / ( b->y - a->y );
	
	intersect->x        = a->x        + d * ( b->x - a->x );
	intersect->y        = theState.clipwindowf_ymax; 
	
	calcParams(a, b, intersect, d);
	
} /* intersectBottom */

inline void
intersectRight( const GrVertex *a, const GrVertex *b, GrVertex *intersect )
{
	float
		d = ( theState.clipwindowf_xmax - a->x ) / ( b->x - a->x );
	
	intersect->x        = theState.clipwindowf_xmax; 
	intersect->y        = a->y        + d * ( b->y - a->y );
	
	calcParams(a, b, intersect, d);
	
} /* intersectRight */

inline void
intersectLeft( const GrVertex *a, const GrVertex *b, GrVertex *intersect )
{
	float
		d = ( theState.clipwindowf_xmin - a->x ) / ( b->x - a->x );
	
	intersect->x        = theState.clipwindowf_xmin;
	intersect->y        = a->y        + d * ( b->y - a->y );
	
	calcParams(a, b, intersect, d);
	
} /* intersectLeft */

inline FxBool
aboveYMin(const GrVertex *p)
{
	return (( p->y > theState.clipwindowf_ymin ) ? FXTRUE : FXFALSE);
} /* aboveYMin */


inline FxBool
belowYMax(const GrVertex *p)
{
	return (( p->y < theState.clipwindowf_ymax ) ? FXTRUE : FXFALSE);
} /* belowYMax */

inline FxBool
aboveXMin(const GrVertex *p)
{
	return (( p->x > theState.clipwindowf_xmin ) ? FXTRUE : FXFALSE );
} /* aboveXMin */

inline FxBool
belowXMax(const GrVertex *p)
{
	return (( p->x < theState.clipwindowf_xmax ) ? FXTRUE : FXFALSE );
} /* belowXMax */

  /*
  ** shClipPolygon
*/
inline void
shClipPolygon(
              const GrVertex invertexarray[],
              GrVertex outvertexarray[],
              int inlength, int *outlength,
              FxBool (*inside)(const GrVertex *p),
              void (*intersect)(
			  const GrVertex *a,
			  const GrVertex *b,
			  GrVertex *intersect )
              )
{
	GrVertex
		s, p /*, intersection */;
	int
		j;
	
	*outlength = 0;
	
	s = invertexarray[inlength-1];
	for ( j = 0; j < inlength; j++ ) {
		p = invertexarray[j];
		if ( inside( &p ) ) {
			if ( inside( &s ) ) {
				outvertexarray[*outlength] = p;
				(*outlength)++;
			}else {
#if 0
				intersect( &s, &p, &intersection );
				outvertexarray[*outlength] = intersection;
#else
				intersect( &s, &p, &outvertexarray[*outlength] );
#endif
				(*outlength)++;
				outvertexarray[*outlength] = p;
				(*outlength)++;
			}
		} else {
			if ( inside( &s ) ) {
#if 0
				intersect( &s, &p, &intersection );
				outvertexarray[*outlength] = intersection;
#else
				intersect( &s, &p, &outvertexarray[*outlength] );
#endif
				(*outlength)++;
			}
		}
		s = p;
	}
} /* shClipPolygon */


/*---------------------------------------------------------------------------
** guDrawTriangleWithClip
**
** NOTE:  This routine snaps vertices by adding a large number then
** subtracting that same number again.  In order for this to work
** you MUST set up the FPU to work in single precision mode.  Code
** to perform this is listed in the Appendix to the Glide Programmer's
** Guide.
*/
static const float vertex_snap_constant = ( float ) ( 1L << 19 );

void FX_CALL guDrawTriangleWithClip (const GrVertex *a, const GrVertex *b, const GrVertex *c )
{
	GrVertex
		output_array[8],
		output_array2[8],
		input_array[3];
	int
		i,
		outlength;
	
	GDBG_INFO_MORE(99,"guDrawTriangleWithClip(0x%x,0x%x,0x%x)\n",a,b,c);
	
	/*
	** perform trivial accept
	*/
	if (
		( a->x >= theState.clipwindowf_xmin) &&
		( a->x < theState.clipwindowf_xmax ) &&
		( a->y >= theState.clipwindowf_ymin ) &&
		( a->y < theState.clipwindowf_ymax ) &&
		( b->x >= theState.clipwindowf_xmin ) &&
		( b->x < theState.clipwindowf_xmax ) &&
		( b->y >= theState.clipwindowf_ymin ) &&
		( b->y < theState.clipwindowf_ymax ) &&
		( c->x >= theState.clipwindowf_xmin ) &&
		( c->x < theState.clipwindowf_xmax ) &&
		( c->y >= theState.clipwindowf_ymin ) &&
		( c->y < theState.clipwindowf_ymax )
		)
	{
		
		Glide3::grDrawTriangle( a, b, c );
		return;
	}
	
	/*
	** go ahead and clip and render
	*/
	input_array[0] = *a;
	input_array[1] = *b;
	input_array[2] = *c;
	
	shClipPolygon( input_array,   output_array,  3,         &outlength, belowXMax, intersectRight );
	shClipPolygon( output_array,  output_array2, outlength, &outlength, belowYMax, intersectBottom );
	shClipPolygon( output_array2, output_array,  outlength, &outlength, aboveXMin, intersectLeft );
	shClipPolygon( output_array,  output_array2, outlength, &outlength, aboveYMin, intersectTop );
	
	/*
	** snap vertices then decompose the n-gon into triangles
	*/
#if defined ( __WATCOMC__ ) || defined ( __MSC__ ) || defined ( __DJGPP__ ) || defined(__GNUC__) || defined(__MWERKS__)
	for ( i = 0; i < outlength; i++ ) {
		output_array2[i].x += vertex_snap_constant;
		output_array2[i].x -= vertex_snap_constant;
		output_array2[i].y += vertex_snap_constant;
		output_array2[i].y -= vertex_snap_constant;
	}
#else
#  error VERTEX SNAPPING MUST BE IMPLEMENTED FOR THIS COMPILER
#endif

	Glide3::grDrawVertexArrayContiguous (GR_POLYGON, outlength, output_array2, sizeof(GrVertex));
}

  /*---------------------------------------------------------------------------
  **  guAADrawTriangleWithClip
*/
void FX_CALL guAADrawTriangleWithClip (const GrVertex *a, const GrVertex *b, const GrVertex *c )
{
	GrVertex
		output_array[8],
		output_array2[8],
		input_array[3];
	int
		i,
		ilist[10],
		outlength;
	
	GDBG_INFO_MORE(99,"guAADrawTriangleWithClip(0x%x,0x%x,0x%x)\n",a,b,c);
	
	/*
    ** perform trivial accept
    */
	if (
		( a->x >= theState.clipwindowf_xmin) &&
		( a->x < theState.clipwindowf_xmax ) &&
		( a->y >= theState.clipwindowf_ymin ) &&
		( a->y < theState.clipwindowf_ymax ) &&
		( b->x >= theState.clipwindowf_xmin ) &&
		( b->x < theState.clipwindowf_xmax ) &&
		( b->y >= theState.clipwindowf_ymin ) &&
		( b->y < theState.clipwindowf_ymax ) &&
		( c->x >= theState.clipwindowf_xmin ) &&
		( c->x < theState.clipwindowf_xmax ) &&
		( c->y >= theState.clipwindowf_ymin ) &&
		( c->y < theState.clipwindowf_ymax )
		)
	{
		Glide3::grAADrawTriangle( a, b, c, FXTRUE, FXTRUE, FXTRUE );
		return;
	}
	
	/*
	** go ahead and clip and render
	*/
	input_array[0] = *a;
	input_array[1] = *b;
	input_array[2] = *c;
	
	shClipPolygon( input_array,   output_array,  3,         &outlength, belowXMax, intersectRight );
	shClipPolygon( output_array,  output_array2, outlength, &outlength, belowYMax, intersectBottom );
	shClipPolygon( output_array2, output_array,  outlength, &outlength, aboveXMin, intersectLeft );
	shClipPolygon( output_array,  output_array2, outlength, &outlength, aboveYMin, intersectTop );
	
	/*
	** snap vertices then decompose the n-gon into triangles
	*/
#if defined ( __WATCOMC__ ) || defined ( __MSC__ ) || defined ( __DJGPP__ ) || defined(__GNUC__) || defined(__MWERKS__)
	for ( i = 0; i < outlength; i++ ) {
		output_array2[i].x += vertex_snap_constant;
		output_array2[i].x -= vertex_snap_constant;
		output_array2[i].y += vertex_snap_constant;
		output_array2[i].y -= vertex_snap_constant;
		ilist[i] = i;
	}
	
	ilist[outlength] = 0;
#else
#  error VERTEX SNAPPING MUST BE IMPLEMENTED FOR THIS COMPILER
#endif
	grAADrawPolygon( outlength, ilist, output_array2 );
}


  /*---------------------------------------------------------------------------
  **  guDrawPolygonVertexListWithClip
*/
void FX_CALL guDrawPolygonVertexListWithClip ( int nverts, const GrVertex vlist[] )
{
	GrVertex
		output_array[GU_PRIM_MAX_VERTICES+8],
		output_array2[GU_PRIM_MAX_VERTICES+8];
	int
		i,
		outlength;
	
	GDBG_INFO_MORE(99,"guDrawPolygonVertexListWithClip(%d,0x%x)\n",nverts,vlist);
	/*
	** go ahead and clip and render
	*/
	shClipPolygon( vlist, output_array,  nverts, &outlength, belowXMax, intersectRight );
	shClipPolygon( output_array,  output_array2, outlength, &outlength, belowYMax, intersectBottom );
	shClipPolygon( output_array2, output_array,  outlength, &outlength, aboveXMin, intersectLeft );
	shClipPolygon( output_array,  output_array2, outlength, &outlength, aboveYMin, intersectTop );
	
	/*
	** snap vertices then decompose the n-gon into triangles
	*/
#if defined ( __WATCOMC__ ) || defined ( __MSC__ ) || defined ( __DJGPP__ ) || defined(__GNUC__) || defined(__MWERKS__)
	for ( i = 0; i < outlength; i++ ) {
		output_array2[i].x += vertex_snap_constant;
		output_array2[i].x -= vertex_snap_constant;
		output_array2[i].y += vertex_snap_constant;
		output_array2[i].y -= vertex_snap_constant;
	}
#else
#  error VERTEX SNAPPING MUST BE IMPLEMENTED FOR THIS COMPILER
#endif
	Glide3::grDrawVertexArrayContiguous (GR_POLYGON, outlength, output_array2, sizeof(GrVertex));
}

