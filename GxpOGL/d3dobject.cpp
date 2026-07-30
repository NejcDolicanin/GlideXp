// DirectHack.cpp : Defines the entry point for the DLL application.
//

#include "stdafx.h"
#include "d3dobject.h"
#include "d3ddevice.h"

#define INITGUID
#include <Guiddef.h>

LPDIRECTHACK	lpD3D = 0;

// {73A3FF2C-424C-4750-8646-5299659B5BF0}
DEFINE_GUID(IID_IDirectHack, 0x73a3ff2c, 0x424c, 0x4750, 0x86, 0x46, 0x52, 0x99, 0x65, 0x9b, 0x5b, 0xf0);

//
// Create the DirectHackDevice
//
IDirectHack * IDirectHack::DirectHackCreate8()
{
	// Allocate IDirectHack object
	LPDIRECTHACK	lpDHack = new IDirectHack();

	// Return our object
	return lpDHack;
}

//
// operator new 
//
//void *IDirectHack::operator new (size_t s)
//{
//	return CoTaskMemAlloc(s);
//}

//
// operator delete
//
//void IDirectHack::operator delete (void *b)
//{
//	CoTaskMemFree(b);
//}

//
// Constructor
//
IDirectHack::IDirectHack() : m_cRef(1)
{
}

//
// IUnknown::AddRef implementation
//
ULONG IDirectHack::AddRef(void)
{
	return ++m_cRef;
}

//
// IUnknown::Release implementation
//
ULONG IDirectHack::Release(void)
{
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
HRESULT IDirectHack::QueryInterface(REFIID riid, void** ppvObj)
{
	if (IsEqualGUID(riid, IID_IUnknown)) {
		*ppvObj = (IUnknown*)this;
		return S_OK;
	}
	if (IsEqualGUID(riid, IID_IDirect3D8)) {
		*ppvObj = (IDirect3D8*)this;
		return S_OK;
	}
	if (IsEqualGUID(riid, IID_IDirectHack)) {
		*ppvObj = (IDirect3D8*)this;
		return S_OK;
	}

	*ppvObj = 0;
	return E_NOINTERFACE;
}


//
// IDirect3D8::RegisterSoftwareDevice implementation
//
HRESULT IDirectHack::RegisterSoftwareDevice(void* pInitializeFunction)
{
	return D3D_OK;
}

//
// IDirect3D8::GetAdapterCount implementation
//
UINT IDirectHack::GetAdapterCount(void)
{
	return 1;
}

//
// IDirect3D8::GetAdapterIdentifier implementation
//
HRESULT IDirectHack::GetAdapterIdentifier(UINT Adapter,DWORD Flags,D3DADAPTER_IDENTIFIER8* pIdentifier)
{
	return D3D_OK;
}

//
// IDirect3D8::GetAdapterModeCount implementation
//
UINT IDirectHack::GetAdapterModeCount(UINT Adapter)
{
	return 1;
}

//
// IDirect3D8::EnumAdapterModes implementation
//
HRESULT IDirectHack::EnumAdapterModes(UINT Adapter,UINT Mode,D3DDISPLAYMODE* pMode)
{
	return D3D_OK;
}

//
// IDirect3D8::GetAdapterDisplayMode implementation
//
HRESULT IDirectHack::GetAdapterDisplayMode(UINT Adapter,D3DDISPLAYMODE* pMode)
{
	return D3D_OK;
}

//
// IDirect3D8::CheckDeviceType implementation
//
HRESULT IDirectHack::CheckDeviceType(UINT Adapter,D3DDEVTYPE CheckType,D3DFORMAT DisplayFormat,D3DFORMAT BackBufferFormat,BOOL Windowed)
{
	return D3D_OK;
}

//
// IDirect3D8::CheckDeviceFormat implementation
//
HRESULT IDirectHack::CheckDeviceFormat(UINT Adapter,D3DDEVTYPE DeviceType,D3DFORMAT AdapterFormat,DWORD Usage,D3DRESOURCETYPE RType,D3DFORMAT CheckFormat)
{
	return D3D_OK;
}

//
// IDirect3D8::CheckDeviceMultiSampleType implementation
//
HRESULT IDirectHack::CheckDeviceMultiSampleType(UINT Adapter,D3DDEVTYPE DeviceType,D3DFORMAT SurfaceFormat,BOOL Windowed,D3DMULTISAMPLE_TYPE MultiSampleType)
{
	return D3D_OK;
}

//
// IDirect3D8::CheckDepthStencilMatch implementation
//
HRESULT IDirectHack::CheckDepthStencilMatch(UINT Adapter,D3DDEVTYPE DeviceType,D3DFORMAT AdapterFormat,D3DFORMAT RenderTargetFormat,D3DFORMAT DepthStencilFormat)
{
	return D3D_OK;
}

//
// IDirect3D8::GetDeviceCaps implementation
//
HRESULT IDirectHack::GetDeviceCaps(UINT Adapter,D3DDEVTYPE DeviceType,D3DCAPS8* pCaps)
{
	return D3D_OK;
}

//
// IDirect3D8::GetAdapterMonitor implementation
//
HMONITOR IDirectHack::GetAdapterMonitor(UINT Adapter)
{
	return 0;
}

//
// IDirect3D8::CreateDevice implementation
//
HRESULT IDirectHack::CreateDevice(UINT Adapter,D3DDEVTYPE DeviceType,HWND hFocusWindow,DWORD BehaviorFlags,D3DPRESENT_PARAMETERS* pPresentationParameters,IDirect3DDevice8** ppReturnedDeviceInterface)
{
	*ppReturnedDeviceInterface = IDirectHackDevice::DirectHackDeviceCreate8();
	return D3D_OK;
}
