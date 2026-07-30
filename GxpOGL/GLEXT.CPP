//
// glext.cpp : OpenGL Extensions
//

#include "stdafx.h"
#include "GxpOGL.h"

static bool setup = false;
PFNGLCOMPRESSEDTEXIMAGE2DARBPROC imp_glCompressedTexImage2DARB = 0;

void SetupExtensions() 
{
	if (setup) return;
	setup = true;

	imp_glCompressedTexImage2DARB = (PFNGLCOMPRESSEDTEXIMAGE2DARBPROC) CALL_IMP(wglGetProcAddress,("glCompressedTexImage2DARB"));

}
