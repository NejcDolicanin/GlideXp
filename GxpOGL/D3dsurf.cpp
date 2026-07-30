//
// d3dsurf.cpp : You *really* don't want to know
//

#include "stdafx.h"
#include "d3dsurf.h"
#include <malloc.h>
#include <stdio.h>

#define INITGUID
#include <Guiddef.h>

// {1F2069EB-6D07-4f75-9409-AFEABA279E6A}
DEFINE_GUID(IID_IGXP_D3DSurface8, 
0x1f2069eb, 0x6d07, 0x4f75, 0x94, 0x9, 0xaf, 0xea, 0xba, 0x27, 0x9e, 0x6a);

#define PRINTF while(0) printf

int IGXP_D3DSurface8::GetSizeForFormat(D3DFORMAT format)
{
	switch (format)
	{
	case D3DFMT_A8R8G8B8:
	case D3DFMT_X8R8G8B8:
		return 32;

	case D3DFMT_R8G8B8:
		return 24;

	case D3DFMT_R5G6B5:
    case D3DFMT_X1R5G5B5:
    case D3DFMT_A1R5G5B5:
    case D3DFMT_A4R4G4B4:
    case D3DFMT_A8R3G3B2:
    case D3DFMT_X4R4G4B4:
    case D3DFMT_A8P8:
    case D3DFMT_A8L8:
		return 16;

    case D3DFMT_R3G3B2:
    case D3DFMT_A8:
    case D3DFMT_P8:
    case D3DFMT_L8:
    case D3DFMT_A4L4:
    case D3DFMT_DXT2:
    case D3DFMT_DXT3:
    case D3DFMT_DXT4:
    case D3DFMT_DXT5:
		return 8;

    case D3DFMT_DXT1:
		return 4;

	default:
		return 0;
	}
}

//
// operator new 
//
//void *IGXP_D3DSurface8::operator new (size_t s)
//{
//	PRINTF("void *IGXP_D3DSurface8::operator new (size_t s)\n");
//	return CoTaskMemAlloc(s);
//}

//
// operator delete
//
//void IGXP_D3DSurface8::operator delete (void *b)
//{
//	PRINTF("void IGXP_D3DSurface8::operator delete (void *b)\n");
//	CoTaskMemFree(b);
//}

//
// Constructor
//
IGXP_D3DSurface8::IGXP_D3DSurface8(D3DFORMAT format, int w, int h, BYTE *_buffer) : m_cRef(1), lock_count(0)
{
	PRINTF("IGXP_D3DSurface8::IGXP_D3DSurface8(D3DFORMAT format = %i, int w = %i, int h = %i)\n", format, w, h);

	bpp = GetSizeForFormat(format);

	// Do these first
	surf_desc.Format = format;
	surf_desc.Type = D3DRTYPE_SURFACE;
	surf_desc.Usage = 0;
	surf_desc.Pool = D3DPOOL_MANAGED;
	surf_desc.MultiSampleType = D3DMULTISAMPLE_NONE;
	surf_desc.Width = w;
	surf_desc.Height = h;

	// Now what we need to do is clamp the w and h values if we are using S3TC
	if (surf_desc.Format >= D3DFMT_DXT1 && surf_desc.Format <= D3DFMT_DXT5)
	{
		w = max(w,4);
		h = max(h,4);
	}

	surf_desc.Size = (w * h * bpp) / 8;

	if (_buffer)
	{
		nodel = TRUE;
		buffer = _buffer;
		allocated = 0;
		allocated_size = 0;

	}
	else
	{
		nodel = FALSE;
		allocated = buffer = (BYTE*) _aligned_malloc(allocated_size = max(surf_desc.Size,16), 8);
	}

}

//
// Destructor
//
IGXP_D3DSurface8::~IGXP_D3DSurface8()
{
	PRINTF("IGXP_D3DSurface8::~IGXP_D3DSurface8()\n");
	if (allocated) _aligned_free(allocated);
}

//
// IUnknown::AddRef implementation
//
ULONG IGXP_D3DSurface8::AddRef(void)
{
	PRINTF("ULONG IGXP_D3DSurface8::AddRef(void)\n");
	return ++m_cRef;
}

//
// IUnknown::Release implementation
//
ULONG IGXP_D3DSurface8::Release(void)
{
	PRINTF("ULONG IGXP_D3DSurface8::Release(void)\n");
	--m_cRef;

	if (!m_cRef)
	{
		delete this;
		return 0;
	}

	return m_cRef;
}

//
// IUnknown::QueryInterface implementation
//
HRESULT IGXP_D3DSurface8::QueryInterface(REFIID riid, void** ppvObj)
{
	PRINTF("HRESULT IGXP_D3DSurface8::QueryInterface(REFIID riid, void** ppvObj)\n");

	if (IsEqualGUID(riid, IID_IUnknown)) {
		*ppvObj = (IUnknown*)this;
		return S_OK;
	}
	if (IsEqualGUID(riid, IID_IDirect3DSurface8)) {
		*ppvObj = (IDirect3DSurface8*)this;
		return S_OK;
	}
	if (IsEqualGUID(riid, IID_IGXP_D3DSurface8)) {
		*ppvObj = (IGXP_D3DSurface8*)this;
		return S_OK;
	}

	*ppvObj = 0;
	return E_NOINTERFACE;
}

//
// IDirect3DSurface8::GetDevice implementation
//
HRESULT IGXP_D3DSurface8::GetDevice(IDirect3DDevice8** ppDevice)
{
	PRINTF("HRESULT IGXP_D3DSurface8::GetDevice(IDirect3DDevice8** ppDevice)\n");
	lpD3DDevice->AddRef();
	*ppDevice = lpD3DDevice;
	return D3D_OK;
}

//
// IDirect3DSurface8::SetPrivateData implementation
//
HRESULT IGXP_D3DSurface8::SetPrivateData(REFGUID refguid,CONST void* pData,DWORD SizeOfData,DWORD Flags)
{
	PRINTF("HRESULT IGXP_D3DSurface8::SetPrivateData(REFGUID refguid,CONST void* pData,DWORD SizeOfData,DWORD Flags)\n");
	return D3DERR_INVALIDCALL;
}


//
// IDirect3DSurface8::GetPrivateData implementation
//
HRESULT IGXP_D3DSurface8::GetPrivateData(REFGUID refguid,void* pData,DWORD* pSizeOfData)
{
	PRINTF("HRESULT IGXP_D3DSurface8::GetPrivateData(REFGUID refguid,void* pData,DWORD* pSizeOfData)\n");
	return D3DERR_INVALIDCALL;
}


//
// IDirect3DSurface8::FreePrivateData implementation
//
HRESULT IGXP_D3DSurface8::FreePrivateData(REFGUID refguid)
{
	PRINTF("HRESULT IGXP_D3DSurface8::FreePrivateData(REFGUID refguid)\n");
	return D3DERR_INVALIDCALL;
}


//
// IDirect3DSurface8::GetContainer implementation
//
HRESULT IGXP_D3DSurface8::GetContainer(REFIID riid,void** ppContainer)
{
	PRINTF("HRESULT IGXP_D3DSurface8::GetContainer(REFIID riid,void** ppContainer)\n");
	return D3DERR_INVALIDCALL;
}

//
// IDirect3DSurface8::GetDesc implementation
//
HRESULT IGXP_D3DSurface8::GetDesc(D3DSURFACE_DESC *pDesc)
{
	PRINTF("HRESULT IGXP_D3DSurface8::GetDesc(D3DSURFACE_DESC *pDesc = %X)\n", pDesc);
	*pDesc = surf_desc;
	return D3D_OK;
}


//
// IDirect3DSurface8::LockRect implementation
//
HRESULT IGXP_D3DSurface8::LockRect(D3DLOCKED_RECT* pLockedRect,CONST RECT* pRect,DWORD Flags)
{
	PRINTF("HRESULT IGXP_D3DSurface8::LockRect(D3DLOCKED_RECT* pLockedRect = %X,CONST RECT* pRect = %X,DWORD Flags = %X)\n", pLockedRect, pRect, Flags);
	lock_count++;

	pLockedRect->pBits = buffer;

	if (surf_desc.Format >= D3DFMT_DXT1 && surf_desc.Format <= D3DFMT_DXT5)
	{
		// Pitch will 'screw up' on surfaces smaller than 4 pixels
		if (surf_desc.Width <= 4) pLockedRect->Pitch = bpp*2;
		else pLockedRect->Pitch = (surf_desc.Width*bpp)/2;

		// If we have a rectangle and we are using S3TC, then we must be 
		// aligned to the 4x4 blocks
		if (pRect)
		{
			// Never Valid
			if ((pRect->left&3) || (pRect->top&3))
				return D3DERR_INVALIDCALL;

			// It's valid to lock non multiples of 4, IF the surface is smaller than 4 pixels
			if ((pRect->right != (LONG) surf_desc.Width && (pRect->right&3)) ||
				(pRect->bottom != (LONG) surf_desc.Height && (pRect->bottom&3)))
			{
				return D3DERR_INVALIDCALL;
			}

			int x = pRect->left/4;
			int y = pRect->top /4;
		
			pLockedRect->pBits = buffer + (pLockedRect->Pitch * y) + (x * bpp)/2;
		}

	}
	else
	{
		pLockedRect->Pitch = (surf_desc.Width*bpp)/8;
		if (pRect) pLockedRect->pBits = buffer + (surf_desc.Width*pRect->top+pRect->left)*bpp/8;
	}

	return D3D_OK;

}


//
// IDirect3DSurface8::UnlockRect implementation
//
HRESULT IGXP_D3DSurface8::UnlockRect()
{
	PRINTF("HRESULT IGXP_D3DSurface8::UnlockRect()\n");
	if (lock_count == 0) return D3DERR_INVALIDCALL;

	lock_count--;
	return D3D_OK;
}


//
// IGXP_D3DSurface8::CopyFromOpenGL implementation
//
HRESULT IGXP_D3DSurface8::CopyFromOpenGL (GLenum format, const GLvoid *pixels)
{
	BYTE *dest = buffer;
	BYTE *end  = dest+surf_desc.Size;
	BYTE *source = (BYTE *)pixels;

	if (format == GL_RGBA)
	{
		if (bpp == 32) while (dest < end)
		{
			*(dest+0) = *(source+2);
			*(dest+1) = *(source+1);
			*(dest+2) = *(source+0);
			*(dest+3) = *(source+3);
			dest+= 4;
			source+= 4;
		}
		else if (bpp == 24) while (dest < end)
		{
			*(dest+0) = *(source+2);
			*(dest+1) = *(source+1);
			*(dest+2) = *(source+0);
			dest+= 3;
			source+= 4;
		}
	}
	else if (format == GL_RGB)
	{
		if (bpp == 32) while (dest < end)
		{
			*(dest+0) = *(source+2);
			*(dest+1) = *(source+1);
			*(dest+2) = *(source+0);
			*(dest+3) = 0xFF;
			dest+= 4;
			source+= 3;
		}
		else if (bpp == 24) while (dest < end)
		{
			*(dest+0) = *(source+2);
			*(dest+1) = *(source+1);
			*(dest+2) = *(source+0);
			dest+= 3;
			source+= 3;
		}
	}
	else if (format == GL_BGRA_EXT)
	{
		if (bpp == 32) while (dest < end)
		{
			*(DWORD*)dest = *(DWORD*)source;
			dest+= 4;
			source+= 4;
		}
		else if (bpp == 24) while (dest < end)
		{
			*(dest+0) = *(source+0);
			*(dest+1) = *(source+1);
			*(dest+2) = *(source+2);
			dest+= 3;
			source+= 4;
		}
	}
	else if (format == GL_BGR_EXT)
	{
		if (bpp == 32) while (dest < end)
		{
			*(dest+0) = *(source+0);
			*(dest+1) = *(source+1);
			*(dest+2) = *(source+2);
			*(dest+3) = 0xFF;
			dest+= 4;
			source+= 3;
		}
		else if (bpp == 24) while (dest < end)
		{
			*(dest+0) = *(source+0);
			*(dest+1) = *(source+1);
			*(dest+2) = *(source+2);
			dest+= 3;
			source+= 3;
		}
	}
	return D3D_OK;
}

//
// IGXP_D3DSurface8::ReallocSurface implementation
//
HRESULT IGXP_D3DSurface8::ReallocSurface(D3DFORMAT format, int w, int h, BYTE *_buffer)
{
	PRINTF("IGXP_D3DSurface8::ReallocSurface(D3DFORMAT format = %i, int w = %i, int h = %i)\n", format, w, h);

	if (lock_count) D3DERR_INVALIDCALL;

	bpp = GetSizeForFormat(format);

	// Do these first
	surf_desc.Format = format;
	surf_desc.Type = D3DRTYPE_SURFACE;
	surf_desc.Usage = 0;
	surf_desc.Pool = D3DPOOL_MANAGED;
	surf_desc.MultiSampleType = D3DMULTISAMPLE_NONE;
	surf_desc.Width = w;
	surf_desc.Height = h;

	// Now what we need to do is clamp the w and h values if we are using S3TC
	if (surf_desc.Format >= D3DFMT_DXT1 && surf_desc.Format <= D3DFMT_DXT5)
	{
		w = max(w,4);
		h = max(h,4);
	}

	surf_desc.Size = (w * h * bpp) / 8;

	if (_buffer)
	{
		// Don't care about the allocated surface
		nodel = TRUE;
		buffer = _buffer;
	}
	else
	{
		// Only realloc IF real_size is less than the size we want
		if (allocated_size < surf_desc.Size) 
		{
			if (allocated) _aligned_free(allocated);
			allocated = (BYTE*) _aligned_malloc(allocated_size = max(surf_desc.Size,16), 8);
		}

		buffer = allocated;

		nodel = FALSE;
	}
	return D3D_OK;
}

