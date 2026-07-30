//
// Make sure this file is ALWAYS up to date. What it does is for grGetProcAddressExt
// to support all the functions that we export. IF a G2->G3 function modification
// is done make sure this file reflects it
//

#ifdef ASSIGN_EXPORT_TUPPLES
#define OVERLOADED_FUNCTION(a) \
	_functionTable.a.name = #a; \
	_functionTable.a.proc = (GrProc) a; \

#define OVERLOADED_FUNCTION_EX(a,b) \
	_functionTable.a.name = #a; \
	_functionTable.a.proc = (GrProc) b; \

#define PASSTHROUGH_FUNCTION(a) \
	_functionTable.a.name = #a; \
	_functionTable.a.proc = (GrProc) Glide3::a; \

#define PASSTHROUGH_FUNCTION_EX(a,b) \
	_functionTable.a.name = #a; \
	_functionTable.a.proc = (GrProc) Glide3::b; \

#else

#define OVERLOADED_FUNCTION(a) GrProcAddressTuple a;
#define OVERLOADED_FUNCTION_EX(a,b) GrProcAddressTuple a;
#define PASSTHROUGH_FUNCTION(a) GrProcAddressTuple a;
#define PASSTHROUGH_FUNCTION_EX(a,b) GrProcAddressTuple a;

#endif

// These are the extensions that we support
OVERLOADED_FUNCTION(grSstWinOpenExt)
OVERLOADED_FUNCTION(grGetExt)
OVERLOADED_FUNCTION(grGetStringExt)
OVERLOADED_FUNCTION(grGetProcAddressExtXP)
OVERLOADED_FUNCTION(grTexDownloadMipMapLevelPartialRowExt)
OVERLOADED_FUNCTION(grLoadGammaTableExt)

// There are the passthrough extensions that we support
PASSTHROUGH_FUNCTION(grTextureBufferExt)
PASSTHROUGH_FUNCTION(grTextureAuxBufferExt)
PASSTHROUGH_FUNCTION(grAuxBufferExt)
PASSTHROUGH_FUNCTION_EX(grGammaCorrectionRGBExt,guGammaCorrectionRGB)

// These are all the pass through functions
PASSTHROUGH_FUNCTION(grAlphaBlendFunction)
PASSTHROUGH_FUNCTION(grAlphaCombine)
PASSTHROUGH_FUNCTION(grAlphaControlsITRGBLighting)
PASSTHROUGH_FUNCTION(grAlphaTestFunction)
PASSTHROUGH_FUNCTION(grAlphaTestReferenceValue)
PASSTHROUGH_FUNCTION(grColorCombine)
PASSTHROUGH_FUNCTION(grColorMask)
PASSTHROUGH_FUNCTION(grConstantColorValue)
PASSTHROUGH_FUNCTION(grCullMode)
PASSTHROUGH_FUNCTION(grAADrawTriangle)
PASSTHROUGH_FUNCTION(grDrawPoint)
PASSTHROUGH_FUNCTION(grDrawLine)
PASSTHROUGH_FUNCTION(grDrawTriangle)
PASSTHROUGH_FUNCTION(grFogColorValue)
PASSTHROUGH_FUNCTION(grFogMode)
PASSTHROUGH_FUNCTION(grFogTable)
PASSTHROUGH_FUNCTION(guFogTableIndexToW)
PASSTHROUGH_FUNCTION(guFogGenerateExp)
PASSTHROUGH_FUNCTION(guFogGenerateExp2)
PASSTHROUGH_FUNCTION(guFogGenerateLinear)
PASSTHROUGH_FUNCTION(grGlideShutdown)
PASSTHROUGH_FUNCTION(grSstOrigin)
PASSTHROUGH_FUNCTION_EX(grSstIdle,grFinish)
PASSTHROUGH_FUNCTION(grDepthBufferFunction)
PASSTHROUGH_FUNCTION(grDepthMask)
PASSTHROUGH_FUNCTION(grDisableAllEffects)
PASSTHROUGH_FUNCTION(grLfbConstantAlpha)
PASSTHROUGH_FUNCTION(grLfbLock)
PASSTHROUGH_FUNCTION(grLfbReadRegion)
PASSTHROUGH_FUNCTION(grLfbUnlock)
PASSTHROUGH_FUNCTION(grRenderBuffer)
PASSTHROUGH_FUNCTION(grChromakeyMode)
PASSTHROUGH_FUNCTION(grChromakeyValue)
PASSTHROUGH_FUNCTION(grDitherMode)
PASSTHROUGH_FUNCTION(grErrorSetCallback)
PASSTHROUGH_FUNCTION(grSplash)
PASSTHROUGH_FUNCTION(grTexClampMode)
PASSTHROUGH_FUNCTION(grTexDetailControl)
PASSTHROUGH_FUNCTION(grTexFilterMode)
PASSTHROUGH_FUNCTION(grTexLodBiasValue)
PASSTHROUGH_FUNCTION(grTexMinAddress)
PASSTHROUGH_FUNCTION(grTexMipMapMode)
PASSTHROUGH_FUNCTION(grTexMultibase)

// These are all the overloaded functions
OVERLOADED_FUNCTION(guDrawTriangleWithClip)
OVERLOADED_FUNCTION(guAADrawTriangleWithClip)
OVERLOADED_FUNCTION(guDrawPolygonVertexListWithClip)
OVERLOADED_FUNCTION(grConstantColorValue4)
OVERLOADED_FUNCTION(grAADrawLine)
OVERLOADED_FUNCTION(grAADrawPoint)
OVERLOADED_FUNCTION(grAADrawPolygon)
OVERLOADED_FUNCTION(grAADrawPolygonVertexList)
OVERLOADED_FUNCTION(grDrawPlanarPolygonVertexList)
OVERLOADED_FUNCTION_EX(grDrawPlanarPolygon,grDrawPolygon)
OVERLOADED_FUNCTION(grDrawPolygon)
OVERLOADED_FUNCTION(grDrawPolygonVertexList)
OVERLOADED_FUNCTION(grGlideInit)
OVERLOADED_FUNCTION(grGlideGetState)
OVERLOADED_FUNCTION(grGlideSetState)
OVERLOADED_FUNCTION(guTexAllocateMemory)
OVERLOADED_FUNCTION(guTexChangeAttributes)
OVERLOADED_FUNCTION(guTexDownloadMipMap)
OVERLOADED_FUNCTION(guTexDownloadMipMapLevel)
OVERLOADED_FUNCTION(guTexGetCurrentMipMap)
OVERLOADED_FUNCTION(guTexGetMipMapInfo)
OVERLOADED_FUNCTION(guTexMemQueryAvail)
OVERLOADED_FUNCTION(grSstQueryBoards)
OVERLOADED_FUNCTION(grSstQueryHardware)
OVERLOADED_FUNCTION(grSstSelect)
OVERLOADED_FUNCTION(grSstWinOpen)
OVERLOADED_FUNCTION(grSstWinClose)
OVERLOADED_FUNCTION(grSstControl)
OVERLOADED_FUNCTION_EX(grSstControlMode,grSstControl)
OVERLOADED_FUNCTION(grSstIsBusy)
OVERLOADED_FUNCTION(grSstPerfStats)
OVERLOADED_FUNCTION(grSstResetPerfStats)
OVERLOADED_FUNCTION(grSstScreenHeight)
OVERLOADED_FUNCTION(grSstScreenWidth)
OVERLOADED_FUNCTION(grSstStatus)
OVERLOADED_FUNCTION(grSstVideoLine)
OVERLOADED_FUNCTION(grSstVRetraceOn)
OVERLOADED_FUNCTION(gu3dfGetInfo)
OVERLOADED_FUNCTION(gu3dfLoad)
OVERLOADED_FUNCTION(guAlphaSource)
OVERLOADED_FUNCTION(guTexCombineFunction)
OVERLOADED_FUNCTION(guTexCreateColorMipMap)
OVERLOADED_FUNCTION(guEncodeRLE16)
OVERLOADED_FUNCTION(guEndianSwapWords)
OVERLOADED_FUNCTION(guEndianSwapBytes)
OVERLOADED_FUNCTION(grBufferClear)
OVERLOADED_FUNCTION(grBufferNumPending)
OVERLOADED_FUNCTION(grDepthBiasLevel)
OVERLOADED_FUNCTION(grDepthBufferMode)
OVERLOADED_FUNCTION(grLfbConstantDepth)
OVERLOADED_FUNCTION(grLfbWriteRegion)
OVERLOADED_FUNCTION(grLfbWriteColorFormat)
OVERLOADED_FUNCTION(grLfbWriteColorSwizzle)
OVERLOADED_FUNCTION(grClipWindow)
OVERLOADED_FUNCTION(grGammaCorrectionValue)
OVERLOADED_FUNCTION(grHints)
OVERLOADED_FUNCTION(grResetTriStats)
OVERLOADED_FUNCTION(grTriStats)
OVERLOADED_FUNCTION(grCheckForRoom)
OVERLOADED_FUNCTION(grTexCalcMemRequired)
OVERLOADED_FUNCTION(grTexCombine)
OVERLOADED_FUNCTION_EX(grTexCombineFunction,guTexCombineFunction)
OVERLOADED_FUNCTION(grTexDownloadMipMap)
OVERLOADED_FUNCTION(grTexDownloadMipMapLevelPartial)
OVERLOADED_FUNCTION(grTexDownloadMipMapLevel)
OVERLOADED_FUNCTION(grTexDownloadTable)
OVERLOADED_FUNCTION(grTexDownloadTablePartial)
OVERLOADED_FUNCTION(grTexMaxAddress)
OVERLOADED_FUNCTION(grTexMultibaseAddress)
OVERLOADED_FUNCTION(grTexNCCTable)
OVERLOADED_FUNCTION(grTexSource)
OVERLOADED_FUNCTION(grTexTextureMemRequired)
OVERLOADED_FUNCTION(ConvertAndDownloadRle)
OVERLOADED_FUNCTION(grGlideGetVersion)
OVERLOADED_FUNCTION(grGlideShamelessPlug)
OVERLOADED_FUNCTION(guColorCombineFunction)
OVERLOADED_FUNCTION(guTexMemReset)
OVERLOADED_FUNCTION(guTexSource)

#ifndef SAFER_grBufferSwap
PASSTHROUGH_FUNCTION(grBufferSwap)
#else
OVERLOADED_FUNCTION(grBufferSwap)
#endif

#undef OVERLOADED_FUNCTION
#undef OVERLOADED_FUNCTION_EX
#undef PASSTHROUGH_FUNCTION
#undef PASSTHROUGH_FUNCTION_EX
