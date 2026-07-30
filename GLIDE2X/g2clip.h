//
// Glide 2 to Glide 3 Translator for GlideXP
//
// g2clip.h : Clipping related functions
//
// Copyright (c) 2002, 2003 Ryan Nunn
//
// Just a note that this is written in C++ so I can use namespaces.
// No other C++ features are used.
//

extern "C" FX_EXPORT void FX_CALL guDrawTriangleWithClip (const GrVertex *a, const GrVertex *b, const GrVertex *c );
extern "C" FX_EXPORT void FX_CALL guAADrawTriangleWithClip (const GrVertex *a, const GrVertex *b, const GrVertex *c );
extern "C" FX_EXPORT void FX_CALL guDrawPolygonVertexListWithClip ( int nverts, const GrVertex vlist[] );

