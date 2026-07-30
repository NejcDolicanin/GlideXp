//
// An implementation of the IDirect3DSurface8 interface so the D3DX Functions 
// can be used to compress textures
//
#pragma once

// {1F2069EB-6D07-4f75-9409-AFEABA279E6A}
DEFINE_GUID(IID_IGXP_D3DSurface8, 
0x1f2069eb, 0x6d07, 0x4f75, 0x94, 0x9, 0xaf, 0xea, 0xba, 0x27, 0x9e, 0x6a);

interface IGXP_D3DSurface8 : public IDirect3DSurface8
{
private:
	DWORD				m_cRef; // Object reference count 

	BYTE				*buffer;	// Surface buffer
	DWORD				lock_count;	// Lock count
	D3DSURFACE_DESC		surf_desc;	// surface desc
	DWORD				bpp;
	BOOL				nodel;

	BYTE				*allocated;	// Allocated buffer
	DWORD				allocated_size;

	/*** New and delete methods ***/
	//static void *operator new (size_t s);
	//static void operator delete (void *b);

	/*** Constructor ***/
	IGXP_D3DSurface8 (D3DFORMAT format, int w, int h, BYTE *_buffer);

	~IGXP_D3DSurface8();

public:

	static int GetSizeForFormat(D3DFORMAT format);

    /*** Static method to create a surface ***/
	inline static IGXP_D3DSurface8 *Create(D3DFORMAT format, int w, int h, BYTE *_buffer = 0)
	{
		return new IGXP_D3DSurface8(format, w, h, _buffer);
	}

    /*** IUnknown methods ***/
    STDMETHOD(QueryInterface)(THIS_ REFIID riid, void** ppvObj);
    STDMETHOD_(ULONG,AddRef)(THIS);
    STDMETHOD_(ULONG,Release)(THIS);

    /*** IDirect3DSurface8 methods ***/
    STDMETHOD(GetDevice)(THIS_ IDirect3DDevice8** ppDevice);
    STDMETHOD(SetPrivateData)(THIS_ REFGUID refguid,CONST void* pData,DWORD SizeOfData,DWORD Flags);
    STDMETHOD(GetPrivateData)(THIS_ REFGUID refguid,void* pData,DWORD* pSizeOfData);
    STDMETHOD(FreePrivateData)(THIS_ REFGUID refguid);
    STDMETHOD(GetContainer)(THIS_ REFIID riid,void** ppContainer);
    STDMETHOD(GetDesc)(THIS_ D3DSURFACE_DESC *pDesc);
    STDMETHOD(LockRect)(THIS_ D3DLOCKED_RECT* pLockedRect,CONST RECT* pRect,DWORD Flags);
    STDMETHOD(UnlockRect)(THIS);


	/*** IGXP_D3DSurface8 ***/
	STDMETHOD(CopyFromOpenGL)(THIS_ GLenum format, const GLvoid *pixels);
	STDMETHOD(ReallocSurface)(D3DFORMAT format, int w, int h, BYTE *_buffer = 0);
};
