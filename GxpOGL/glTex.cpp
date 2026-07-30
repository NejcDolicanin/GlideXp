//
// glTex.cpp : glTex* function overloading
//

#include "stdafx.h"
#include "GxpOGL.h"
#include "d3dsurf.h"
#include <stdio.h>

//
// Overloaded glTexImage2D
//
// To allow me to fix DXT5 Support
//
// Export: glTexImage2D

// Setup CompressedSurface
static IGXP_D3DSurface8 *surf = 0;

// Hacky source surface
static IGXP_D3DSurface8 *source = 0;


void APIENTRY Overload_glTexImage2D (GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels)
{
	//OutputDebugString("glTexImage2D\n");

	// Translation from GL_S3_s3tc to GL_EXT_texture_compression_s3tc
	if (internalformat == GL_RGB_S3TC || internalformat == GL_RGB4_S3TC) internalformat = GL_COMPRESSED_RGB_S3TC_DXT1_EXT;

	// Translation from GL_S3_s3tc to GL_EXT_texture_compression_s3tc
	if (internalformat == GL_RGBA_S3TC || internalformat == GL_RGBA4_S3TC) internalformat = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;

	// We'll compress DXTC ourselves
	if (internalformat >= GL_COMPRESSED_RGB_S3TC_DXT1_EXT && internalformat <= GL_COMPRESSED_RGBA_S3TC_DXT5_EXT &&
		(format == GL_RGBA || format == GL_RGB || format == GL_BGR_EXT || format == GL_BGRA_EXT ) &&
		type == GL_UNSIGNED_BYTE)
	{
		D3DFORMAT d3dFormat;

		// Setup the Direct3D Stuff
		if (!lpD3D) lpD3D = IDirectHack::DirectHackCreate8();
		if (!lpD3DDevice) lpD3D->CreateDevice(0, D3DDEVTYPE_HAL, 0, 0, 0, & lpD3DDevice);

		// Make sure we have the correct opengl Extensions
		SetupExtensions();

		// Get the D3DFORMAT for DXT1
		if (internalformat == GL_COMPRESSED_RGB_S3TC_DXT1_EXT || internalformat == GL_COMPRESSED_RGBA_S3TC_DXT1_EXT)
			d3dFormat = D3DFMT_DXT1;
		// Get the D3DFORMAT for DXT3
		else if (internalformat == GL_COMPRESSED_RGBA_S3TC_DXT3_EXT)
			d3dFormat = D3DFMT_DXT3;
		// Get the D3DFORMAT for DXT5
		else if (internalformat == GL_COMPRESSED_RGBA_S3TC_DXT5_EXT)
			d3dFormat = D3DFMT_DXT5;

		// Setup CompressedSurface
		if (!surf) surf = IGXP_D3DSurface8::Create(d3dFormat, width, height);
		else surf->ReallocSurface(d3dFormat, width, height);

		// GL_RGBA - Needs Conversion
		if (format == GL_RGBA)
		{
			if (!source) source = IGXP_D3DSurface8::Create(D3DFMT_A8R8G8B8, width, height);
			else source->ReallocSurface(D3DFMT_A8R8G8B8, width, height);
			source->CopyFromOpenGL(format, pixels);
		}
		// GL_RGB - Needs Conversion
		else if (format == GL_RGB)
		{
			if (!source) source = IGXP_D3DSurface8::Create(D3DFMT_R8G8B8, width, height);
			else source->ReallocSurface(D3DFMT_R8G8B8, width, height);
			source->CopyFromOpenGL(format, pixels);
		}
		// GL_BGRA_EXT - No Conversion Required
		if (format == GL_BGRA_EXT)
		{
			if (!source) source = IGXP_D3DSurface8::Create(D3DFMT_A8R8G8B8, width, height, (BYTE*)pixels);
			else source->ReallocSurface(D3DFMT_A8R8G8B8, width, height, (BYTE*)pixels);
		}
		// GL_BGR_EXT - No Conversion Required
		else if (format == GL_BGR_EXT)
		{
			if (!source) source = IGXP_D3DSurface8::Create(D3DFMT_R8G8B8, width, height, (BYTE*)pixels);
			else source->ReallocSurface(D3DFMT_R8G8B8, width, height, (BYTE*)pixels);
		}

		// Compress it
		HRESULT res = D3DXLoadSurfaceFromSurface(
					surf,				// pDestSurface 
					0,					// pDestPalette
					0,					// pDestRect
					source,				// pSrcSurface
					0,					// SrcPalette
					0,					// pSrcRect
					D3DX_DEFAULT,		// Filter 
					0);					// ColorKey


		// Information about the compressed image
		D3DSURFACE_DESC Desc;
	    surf->GetDesc(&Desc);

		// Lock it
		D3DLOCKED_RECT locked;
		surf->LockRect(&locked,0,0);

		// Upload it!
		imp_glCompressedTexImage2DARB(target, level, internalformat, width, height, border, Desc.Size, locked.pBits);

		// Unlock
		surf->UnlockRect();

		return;
	}

	// Pass it onto the original function
	icd_funcs[table_num].glTexImage2D(target, level, internalformat, width, height, border, format, type, pixels);
}

