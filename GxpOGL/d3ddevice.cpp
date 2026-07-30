// DirectHack.cpp : Defines the entry point for the DLL application.
//

#include "stdafx.h"
#include "d3ddevice.h"

#define INITGUID
#include <Guiddef.h>

LPDIRECT3DDEVICE8	lpD3DDevice = 0;

// {6EE47AAA-9880-43b0-A249-BD896891BC25}
DEFINE_GUID(IID_IDirectHackDevice, 0x6ee47aaa, 0x9880, 0x43b0, 0xa2, 0x49, 0xbd, 0x89, 0x68, 0x91, 0xbc, 0x25);

//
// Create the DirectHackDevice
//
IDirect3DDevice8 * IDirectHackDevice::DirectHackDeviceCreate8()
{
	// Allocate IDirectHackDevice object
	LPDIRECTHACKDEVICE	lpDHack = new IDirectHackDevice();

	// Return our object
	return lpDHack;
}

//
// operator new 
//
//void *IDirectHackDevice::operator new (size_t s)
//{
//	return CoTaskMemAlloc(s);
//}

//
// operator delete
//
//void IDirectHackDevice::operator delete (void *b)
//{
//	CoTaskMemFree(b);
//}

//
// Constructor
//
IDirectHackDevice::IDirectHackDevice() : m_cRef(1)
{

}

//
// IUnknown::AddRef implementation
//
ULONG IDirectHackDevice::AddRef(void)
{
	return ++m_cRef;
}

//
// IUnknown::Release implementation
//
ULONG IDirectHackDevice::Release(void)
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
HRESULT IDirectHackDevice::QueryInterface(REFIID riid, void** ppvObj)
{
	if (IsEqualGUID(riid, IID_IUnknown)) {
		*ppvObj = (IUnknown*)this;
		return S_OK;
	}
	if (IsEqualGUID(riid, IID_IDirect3DDevice8)) {
		*ppvObj = (IDirect3DDevice8*)this;
		return S_OK;
	}
	if (IsEqualGUID(riid, IID_IDirectHackDevice)) {
		*ppvObj = (IDirect3DDevice8*)this;
		return S_OK;
	}

	*ppvObj = 0;
	return E_NOINTERFACE;
}

