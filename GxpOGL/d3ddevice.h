
// The following ifdef block is the standard way of creating macros which make exporting 
// from a DLL simpler. All files within this DLL are compiled with the DIRECTHACK_EXPORTS
// symbol defined on the command line. this symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see 
// DIRECTHACK_API functions as being imported from a DLL, wheras this DLL sees symbols
// defined with this macro as being exported.

#pragma once

// {6EE47AAA-9880-43b0-A249-BD896891BC25}
DEFINE_GUID(IID_IDirectHackDevice, 0x6ee47aaa, 0x9880, 0x43b0, 0xa2, 0x49, 0xbd, 0x89, 0x68, 0x91, 0xbc, 0x25);

//#define ASM_BREAKPOINT __asm { __asm int 3 }
#define ASM_BREAKPOINT

interface IDirectHackDevice : public IDirect3DDevice8
{
private:
	DWORD			m_cRef; // Object reference count 
	
	/*** New and delete methods ***/
	//static void *operator new (size_t s);
	//static void operator delete (void *b);

	IDirectHackDevice();

public:

	static IDirect3DDevice8 * DirectHackDeviceCreate8();

    /*** IUnknown methods ***/
    STDMETHOD(QueryInterface)(THIS_ REFIID riid, void** ppvObj);
    STDMETHOD_(ULONG,AddRef)(THIS);
    STDMETHOD_(ULONG,Release)(THIS);

    /*** IDirect3DDevice8 methods ***/
    STDMETHOD(TestCooperativeLevel)(THIS) { ASM_BREAKPOINT return D3D_OK; }
    STDMETHOD_(UINT, GetAvailableTextureMem)(THIS) { ASM_BREAKPOINT return 1; }
    STDMETHOD(ResourceManagerDiscardBytes)(THIS_ DWORD Bytes) { ASM_BREAKPOINT return D3D_OK; }

    STDMETHOD(GetDirect3D)(THIS_ IDirect3D8** ppD3D8)
	{ 
		ASM_BREAKPOINT 
		lpD3D->AddRef(); 
		*ppD3D8 = lpD3D; 
		return D3D_OK; 
	}

    STDMETHOD(GetDeviceCaps)(THIS_ D3DCAPS8* pCaps) { ASM_BREAKPOINT return D3D_OK; }
    STDMETHOD(GetDisplayMode)(THIS_ D3DDISPLAYMODE* pMode) { ASM_BREAKPOINT return D3D_OK; }
    STDMETHOD(GetCreationParameters)(THIS_ D3DDEVICE_CREATION_PARAMETERS *pParameters) { ASM_BREAKPOINT return D3D_OK; }
    STDMETHOD(SetCursorProperties)(THIS_ UINT XHotSpot,UINT YHotSpot,IDirect3DSurface8* pCursorBitmap) { ASM_BREAKPOINT return D3D_OK; }
    STDMETHOD_(void, SetCursorPosition)(THIS_ int X,int Y,DWORD Flags) { ASM_BREAKPOINT return; }
    STDMETHOD_(BOOL, ShowCursor)(THIS_ BOOL bShow) { ASM_BREAKPOINT return FALSE; }
    STDMETHOD(CreateAdditionalSwapChain)(THIS_ D3DPRESENT_PARAMETERS* pPresentationParameters,IDirect3DSwapChain8** pSwapChain) { ASM_BREAKPOINT return D3D_OK; }
    STDMETHOD(Reset)(THIS_ D3DPRESENT_PARAMETERS* pPresentationParameters) { ASM_BREAKPOINT return D3D_OK; }
    STDMETHOD(Present)(THIS_ CONST RECT* pSourceRect,CONST RECT* pDestRect,HWND hDestWindowOverride,CONST RGNDATA* pDirtyRegion) { ASM_BREAKPOINT return D3D_OK; }
    STDMETHOD(GetBackBuffer)(THIS_ UINT BackBuffer,D3DBACKBUFFER_TYPE Type,IDirect3DSurface8** ppBackBuffer) { ASM_BREAKPOINT return D3D_OK; }
    STDMETHOD(GetRasterStatus)(THIS_ D3DRASTER_STATUS* pRasterStatus) { ASM_BREAKPOINT return D3D_OK; }
    STDMETHOD_(void, SetGammaRamp)(THIS_ DWORD Flags,CONST D3DGAMMARAMP* pRamp) { ASM_BREAKPOINT return; }
    STDMETHOD_(void, GetGammaRamp)(THIS_ D3DGAMMARAMP* pRamp) { ASM_BREAKPOINT return; }
    STDMETHOD(CreateTexture)(THIS_ UINT Width,UINT Height,UINT Levels,DWORD Usage,D3DFORMAT Format,D3DPOOL Pool,IDirect3DTexture8** ppTexture) { ASM_BREAKPOINT return D3D_OK; }
    STDMETHOD(CreateVolumeTexture)(THIS_ UINT Width,UINT Height,UINT Depth,UINT Levels,DWORD Usage,D3DFORMAT Format,D3DPOOL Pool,IDirect3DVolumeTexture8** ppVolumeTexture) { ASM_BREAKPOINT return D3D_OK; }
    STDMETHOD(CreateCubeTexture)(THIS_ UINT EdgeLength,UINT Levels,DWORD Usage,D3DFORMAT Format,D3DPOOL Pool,IDirect3DCubeTexture8** ppCubeTexture) { ASM_BREAKPOINT return D3D_OK; }
    STDMETHOD(CreateVertexBuffer)(THIS_ UINT Length,DWORD Usage,DWORD FVF,D3DPOOL Pool,IDirect3DVertexBuffer8** ppVertexBuffer) { ASM_BREAKPOINT return D3D_OK; }
    STDMETHOD(CreateIndexBuffer)(THIS_ UINT Length,DWORD Usage,D3DFORMAT Format,D3DPOOL Pool,IDirect3DIndexBuffer8** ppIndexBuffer) { ASM_BREAKPOINT return D3D_OK; }
    STDMETHOD(CreateRenderTarget)(THIS_ UINT Width,UINT Height,D3DFORMAT Format,D3DMULTISAMPLE_TYPE MultiSample,BOOL Lockable,IDirect3DSurface8** ppSurface) { ASM_BREAKPOINT return D3D_OK; }
    STDMETHOD(CreateDepthStencilSurface)(THIS_ UINT Width,UINT Height,D3DFORMAT Format,D3DMULTISAMPLE_TYPE MultiSample,IDirect3DSurface8** ppSurface) { ASM_BREAKPOINT return D3D_OK; }
    STDMETHOD(CreateImageSurface)(THIS_ UINT Width,UINT Height,D3DFORMAT Format,IDirect3DSurface8** ppSurface) { ASM_BREAKPOINT return D3D_OK; }
    STDMETHOD(CopyRects)(THIS_ IDirect3DSurface8* pSourceSurface,CONST RECT* pSourceRectsArray,UINT cRects,IDirect3DSurface8* pDestinationSurface,CONST POINT* pDestPointsArray) { ASM_BREAKPOINT return  D3DERR_INVALIDCALL; }
    STDMETHOD(UpdateTexture)(THIS_ IDirect3DBaseTexture8* pSourceTexture,IDirect3DBaseTexture8* pDestinationTexture) { ASM_BREAKPOINT return D3D_OK; }
    STDMETHOD(GetFrontBuffer)(THIS_ IDirect3DSurface8* pDestSurface) { ASM_BREAKPOINT return D3D_OK; }
    STDMETHOD(SetRenderTarget)(THIS_ IDirect3DSurface8* pRenderTarget,IDirect3DSurface8* pNewZStencil) { ASM_BREAKPOINT return D3D_OK; }
    STDMETHOD(GetRenderTarget)(THIS_ IDirect3DSurface8** ppRenderTarget) { ASM_BREAKPOINT return D3D_OK; }
    STDMETHOD(GetDepthStencilSurface)(THIS_ IDirect3DSurface8** ppZStencilSurface) { ASM_BREAKPOINT return D3D_OK; }
    STDMETHOD(BeginScene)(THIS) { ASM_BREAKPOINT return D3D_OK; }
    STDMETHOD(EndScene)(THIS) { ASM_BREAKPOINT return D3D_OK; }
    STDMETHOD(Clear)(THIS_ DWORD Count,CONST D3DRECT* pRects,DWORD Flags,D3DCOLOR Color,float Z,DWORD Stencil) { ASM_BREAKPOINT return D3D_OK; }
    STDMETHOD(SetTransform)(THIS_ D3DTRANSFORMSTATETYPE State,CONST D3DMATRIX* pMatrix) { ASM_BREAKPOINT return D3D_OK; }
    STDMETHOD(GetTransform)(THIS_ D3DTRANSFORMSTATETYPE State,D3DMATRIX* pMatrix) { ASM_BREAKPOINT return D3D_OK; }
    STDMETHOD(MultiplyTransform)(THIS_ D3DTRANSFORMSTATETYPE,CONST D3DMATRIX*) { ASM_BREAKPOINT return D3D_OK; }
    STDMETHOD(SetViewport)(THIS_ CONST D3DVIEWPORT8* pViewport) { ASM_BREAKPOINT return D3D_OK; }
    STDMETHOD(GetViewport)(THIS_ D3DVIEWPORT8* pViewport) { ASM_BREAKPOINT return D3D_OK; }
    STDMETHOD(SetMaterial)(THIS_ CONST D3DMATERIAL8* pMaterial) { ASM_BREAKPOINT return D3D_OK; }
    STDMETHOD(GetMaterial)(THIS_ D3DMATERIAL8* pMaterial) { ASM_BREAKPOINT return D3D_OK; }
    STDMETHOD(SetLight)(THIS_ DWORD Index,CONST D3DLIGHT8*) { ASM_BREAKPOINT return D3D_OK; }
    STDMETHOD(GetLight)(THIS_ DWORD Index,D3DLIGHT8*) { ASM_BREAKPOINT return D3D_OK; }
    STDMETHOD(LightEnable)(THIS_ DWORD Index,BOOL Enable) { ASM_BREAKPOINT return D3D_OK; }
    STDMETHOD(GetLightEnable)(THIS_ DWORD Index,BOOL* pEnable) { ASM_BREAKPOINT return D3D_OK; }
    STDMETHOD(SetClipPlane)(THIS_ DWORD Index,CONST float* pPlane) { ASM_BREAKPOINT return D3D_OK; }
    STDMETHOD(GetClipPlane)(THIS_ DWORD Index,float* pPlane) { ASM_BREAKPOINT return D3D_OK; }
    STDMETHOD(SetRenderState)(THIS_ D3DRENDERSTATETYPE State,DWORD Value) { ASM_BREAKPOINT return D3D_OK; }
    STDMETHOD(GetRenderState)(THIS_ D3DRENDERSTATETYPE State,DWORD* pValue) { ASM_BREAKPOINT return D3D_OK; }
    STDMETHOD(BeginStateBlock)(THIS) { ASM_BREAKPOINT return D3D_OK; }
    STDMETHOD(EndStateBlock)(THIS_ DWORD* pToken) { ASM_BREAKPOINT return D3D_OK; }
    STDMETHOD(ApplyStateBlock)(THIS_ DWORD Token) { ASM_BREAKPOINT return D3D_OK; }
    STDMETHOD(CaptureStateBlock)(THIS_ DWORD Token) { ASM_BREAKPOINT return D3D_OK; }
    STDMETHOD(DeleteStateBlock)(THIS_ DWORD Token) { ASM_BREAKPOINT return D3D_OK; }
    STDMETHOD(CreateStateBlock)(THIS_ D3DSTATEBLOCKTYPE Type,DWORD* pToken) { ASM_BREAKPOINT return D3D_OK; }
    STDMETHOD(SetClipStatus)(THIS_ CONST D3DCLIPSTATUS8* pClipStatus) { ASM_BREAKPOINT return D3D_OK; }
    STDMETHOD(GetClipStatus)(THIS_ D3DCLIPSTATUS8* pClipStatus) { ASM_BREAKPOINT return D3D_OK; }
    STDMETHOD(GetTexture)(THIS_ DWORD Stage,IDirect3DBaseTexture8** ppTexture) { ASM_BREAKPOINT return D3D_OK; }
    STDMETHOD(SetTexture)(THIS_ DWORD Stage,IDirect3DBaseTexture8* pTexture) { ASM_BREAKPOINT return D3D_OK; }
    STDMETHOD(GetTextureStageState)(THIS_ DWORD Stage,D3DTEXTURESTAGESTATETYPE Type,DWORD* pValue) { ASM_BREAKPOINT return D3D_OK; }
    STDMETHOD(SetTextureStageState)(THIS_ DWORD Stage,D3DTEXTURESTAGESTATETYPE Type,DWORD Value) { ASM_BREAKPOINT return D3D_OK; }
    STDMETHOD(ValidateDevice)(THIS_ DWORD* pNumPasses) { ASM_BREAKPOINT return D3D_OK; }
    STDMETHOD(GetInfo)(THIS_ DWORD DevInfoID,void* pDevInfoStruct,DWORD DevInfoStructSize) { ASM_BREAKPOINT return D3D_OK; }
    STDMETHOD(SetPaletteEntries)(THIS_ UINT PaletteNumber,CONST PALETTEENTRY* pEntries) { ASM_BREAKPOINT return D3D_OK; }
    STDMETHOD(GetPaletteEntries)(THIS_ UINT PaletteNumber,PALETTEENTRY* pEntries) { ASM_BREAKPOINT return D3D_OK; }
    STDMETHOD(SetCurrentTexturePalette)(THIS_ UINT PaletteNumber) { ASM_BREAKPOINT return D3D_OK; }
    STDMETHOD(GetCurrentTexturePalette)(THIS_ UINT *PaletteNumber) { ASM_BREAKPOINT return D3D_OK; }
    STDMETHOD(DrawPrimitive)(THIS_ D3DPRIMITIVETYPE PrimitiveType,UINT StartVertex,UINT PrimitiveCount) { ASM_BREAKPOINT return D3D_OK; }
    STDMETHOD(DrawIndexedPrimitive)(THIS_ D3DPRIMITIVETYPE,UINT minIndex,UINT NumVertices,UINT startIndex,UINT primCount) { ASM_BREAKPOINT return D3D_OK; }
    STDMETHOD(DrawPrimitiveUP)(THIS_ D3DPRIMITIVETYPE PrimitiveType,UINT PrimitiveCount,CONST void* pVertexStreamZeroData,UINT VertexStreamZeroStride) { ASM_BREAKPOINT return D3D_OK; }
    STDMETHOD(DrawIndexedPrimitiveUP)(THIS_ D3DPRIMITIVETYPE PrimitiveType,UINT MinVertexIndex,UINT NumVertexIndices,UINT PrimitiveCount,CONST void* pIndexData,D3DFORMAT IndexDataFormat,CONST void* pVertexStreamZeroData,UINT VertexStreamZeroStride) { ASM_BREAKPOINT return D3D_OK; }
    STDMETHOD(ProcessVertices)(THIS_ UINT SrcStartIndex,UINT DestIndex,UINT VertexCount,IDirect3DVertexBuffer8* pDestBuffer,DWORD Flags) { ASM_BREAKPOINT return D3D_OK; }
    STDMETHOD(CreateVertexShader)(THIS_ CONST DWORD* pDeclaration,CONST DWORD* pFunction,DWORD* pHandle,DWORD Usage) { ASM_BREAKPOINT return D3D_OK; }
    STDMETHOD(SetVertexShader)(THIS_ DWORD Handle) { ASM_BREAKPOINT return D3D_OK; }
    STDMETHOD(GetVertexShader)(THIS_ DWORD* pHandle) { ASM_BREAKPOINT return D3D_OK; }
    STDMETHOD(DeleteVertexShader)(THIS_ DWORD Handle) { ASM_BREAKPOINT return D3D_OK; }
    STDMETHOD(SetVertexShaderConstant)(THIS_ DWORD Register,CONST void* pConstantData,DWORD ConstantCount) { ASM_BREAKPOINT return D3D_OK; }
    STDMETHOD(GetVertexShaderConstant)(THIS_ DWORD Register,void* pConstantData,DWORD ConstantCount) { ASM_BREAKPOINT return D3D_OK; }
    STDMETHOD(GetVertexShaderDeclaration)(THIS_ DWORD Handle,void* pData,DWORD* pSizeOfData) { ASM_BREAKPOINT return D3D_OK; }
    STDMETHOD(GetVertexShaderFunction)(THIS_ DWORD Handle,void* pData,DWORD* pSizeOfData) { ASM_BREAKPOINT return D3D_OK; }
    STDMETHOD(SetStreamSource)(THIS_ UINT StreamNumber,IDirect3DVertexBuffer8* pStreamData,UINT Stride) { ASM_BREAKPOINT return D3D_OK; }
    STDMETHOD(GetStreamSource)(THIS_ UINT StreamNumber,IDirect3DVertexBuffer8** ppStreamData,UINT* pStride) { ASM_BREAKPOINT return D3D_OK; }
    STDMETHOD(SetIndices)(THIS_ IDirect3DIndexBuffer8* pIndexData,UINT BaseVertexIndex) { ASM_BREAKPOINT return D3D_OK; }
    STDMETHOD(GetIndices)(THIS_ IDirect3DIndexBuffer8** ppIndexData,UINT* pBaseVertexIndex) { ASM_BREAKPOINT return D3D_OK; }
    STDMETHOD(CreatePixelShader)(THIS_ CONST DWORD* pFunction,DWORD* pHandle) { ASM_BREAKPOINT return D3D_OK; }
    STDMETHOD(SetPixelShader)(THIS_ DWORD Handle) { ASM_BREAKPOINT return D3D_OK; }
    STDMETHOD(GetPixelShader)(THIS_ DWORD* pHandle) { ASM_BREAKPOINT return D3D_OK; }
    STDMETHOD(DeletePixelShader)(THIS_ DWORD Handle) { ASM_BREAKPOINT return D3D_OK; }
    STDMETHOD(SetPixelShaderConstant)(THIS_ DWORD Register,CONST void* pConstantData,DWORD ConstantCount) { ASM_BREAKPOINT return D3D_OK; }
    STDMETHOD(GetPixelShaderConstant)(THIS_ DWORD Register,void* pConstantData,DWORD ConstantCount) { ASM_BREAKPOINT return D3D_OK; }
    STDMETHOD(GetPixelShaderFunction)(THIS_ DWORD Handle,void* pData,DWORD* pSizeOfData) { ASM_BREAKPOINT return D3D_OK; }
    STDMETHOD(DrawRectPatch)(THIS_ UINT Handle,CONST float* pNumSegs,CONST D3DRECTPATCH_INFO* pRectPatchInfo) { ASM_BREAKPOINT return D3D_OK; }
    STDMETHOD(DrawTriPatch)(THIS_ UINT Handle,CONST float* pNumSegs,CONST D3DTRIPATCH_INFO* pTriPatchInfo) { ASM_BREAKPOINT return D3D_OK; }
    STDMETHOD(DeletePatch)(THIS_ UINT Handle) { ASM_BREAKPOINT return D3D_OK; }

    STDMETHOD_(void, SetCursorPosition)(THIS_ UINT XScreenSpace,UINT YScreenSpace,DWORD Flags)  { ASM_BREAKPOINT; }

};

typedef struct IDirectHackDevice *LPDIRECTHACKDEVICE, *PDIRECTHACKDEVICE;

extern LPDIRECT3DDEVICE8	lpD3DDevice;

#undef ASM_BREAKPOINT