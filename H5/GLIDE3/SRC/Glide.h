/*
** THIS SOFTWARE IS SUBJECT TO COPYRIGHT PROTECTION AND IS OFFERED ONLY
** PURSUANT TO THE 3DFX GLIDE GENERAL PUBLIC LICENSE. THERE IS NO RIGHT
** TO USE THE GLIDE TRADEMARK WITHOUT PRIOR WRITTEN PERMISSION OF 3DFX
** INTERACTIVE, INC. A COPY OF THIS LICENSE MAY BE OBTAINED FROM THE 
** DISTRIBUTOR OR BY CONTACTING 3DFX INTERACTIVE INC(info@3dfx.com). 
** THIS PROGRAM IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND, EITHER 
** EXPRESSED OR IMPLIED. SEE THE 3DFX GLIDE GENERAL PUBLIC LICENSE FOR A
** FULL TEXT OF THE NON-WARRANTY PROVISIONS.  
** 
** USE, DUPLICATION OR DISCLOSURE BY THE GOVERNMENT IS SUBJECT TO
** RESTRICTIONS AS SET FORTH IN SUBDIVISION (C)(1)(II) OF THE RIGHTS IN
** TECHNICAL DATA AND COMPUTER SOFTWARE CLAUSE AT DFARS 252.227-7013,
** AND/OR IN SIMILAR OR SUCCESSOR CLAUSES IN THE FAR, DOD OR NASA FAR
** SUPPLEMENT. UNPUBLISHED RIGHTS RESERVED UNDER THE COPYRIGHT LAWS OF
** THE UNITED STATES.  
** 
** COPYRIGHT 3DFX INTERACTIVE, INC. 1999, ALL RIGHTS RESERVED
*/

/*
** GLIDE.H
**
** The following #defines are relevant when using Glide:
**
** One of the following "platform constants" must be defined during
** compilation:
**
**            __DOS__           Defined for 32-bit DOS applications
**            __WIN32__         Defined for 32-bit Windows applications
**            __sparc__         Defined for Sun Solaris/SunOS
**            __linux__         Defined for Linux applications
**            __IRIX__          Defined for SGI Irix applications
**
*/
#ifndef __GLIDE_H__
#define __GLIDE_H__

#include <3dfx.h>
#include <glidesys.h>
#include <sst1vid.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
** -----------------------------------------------------------------------
** TYPE DEFINITIONS
** -----------------------------------------------------------------------
*/
typedef FxU32 GrColor_t;
typedef FxU8  GrAlpha_t;
typedef FxU32 GrMipMapId_t;
#if defined(__linux__) || defined(_WIN32)
typedef FxU32 GrStipplePattern_t;
#endif /* __linux__ */
typedef FxU8  GrFog_t;
typedef FxU32 GrContext_t;
typedef int (FX_CALL *GrProc)();

/*
** -----------------------------------------------------------------------
** CONSTANTS AND TYPES
** -----------------------------------------------------------------------
*/
#define GR_NULL_MIPMAP_HANDLE  ((GrMipMapId_t) -1)

#define GR_MIPMAPLEVELMASK_EVEN  FXBIT(0)
#define GR_MIPMAPLEVELMASK_ODD  FXBIT(1)
#define GR_MIPMAPLEVELMASK_BOTH (GR_MIPMAPLEVELMASK_EVEN | GR_MIPMAPLEVELMASK_ODD )

#define GR_LODBIAS_BILINEAR     0.5
#define GR_LODBIAS_TRILINEAR    0.0

#define MAX_NUM_SST                  4

typedef FxI32 GrChipID_t;
#define GR_TMU0         0x0
#define GR_TMU1         0x1
#define GR_TMU2         0x2

#define GR_FBI          0x0

typedef FxI32 GrCombineFunction_t;
#define GR_COMBINE_FUNCTION_ZERO        0x0
#define GR_COMBINE_FUNCTION_NONE        GR_COMBINE_FUNCTION_ZERO
#define GR_COMBINE_FUNCTION_LOCAL       0x1
#define GR_COMBINE_FUNCTION_LOCAL_ALPHA 0x2
#define GR_COMBINE_FUNCTION_SCALE_OTHER 0x3
#define GR_COMBINE_FUNCTION_BLEND_OTHER GR_COMBINE_FUNCTION_SCALE_OTHER
#define GR_COMBINE_FUNCTION_SCALE_OTHER_ADD_LOCAL 0x4
#define GR_COMBINE_FUNCTION_SCALE_OTHER_ADD_LOCAL_ALPHA 0x5 
#define GR_COMBINE_FUNCTION_SCALE_OTHER_MINUS_LOCAL 0x6
#define GR_COMBINE_FUNCTION_SCALE_OTHER_MINUS_LOCAL_ADD_LOCAL 0x7
#define GR_COMBINE_FUNCTION_BLEND GR_COMBINE_FUNCTION_SCALE_OTHER_MINUS_LOCAL_ADD_LOCAL
#define GR_COMBINE_FUNCTION_SCALE_OTHER_MINUS_LOCAL_ADD_LOCAL_ALPHA 0x8
#define GR_COMBINE_FUNCTION_SCALE_MINUS_LOCAL_ADD_LOCAL 0x9
#define GR_COMBINE_FUNCTION_BLEND_LOCAL GR_COMBINE_FUNCTION_SCALE_MINUS_LOCAL_ADD_LOCAL
#define GR_COMBINE_FUNCTION_SCALE_MINUS_LOCAL_ADD_LOCAL_ALPHA 0x10

typedef FxI32 GrCombineFactor_t;
#define GR_COMBINE_FACTOR_ZERO          0x0
#define GR_COMBINE_FACTOR_NONE          GR_COMBINE_FACTOR_ZERO
#define GR_COMBINE_FACTOR_LOCAL         0x1
#define GR_COMBINE_FACTOR_OTHER_ALPHA   0x2
#define GR_COMBINE_FACTOR_LOCAL_ALPHA   0x3
#define GR_COMBINE_FACTOR_TEXTURE_ALPHA 0x4
#define GR_COMBINE_FACTOR_TEXTURE_RGB   0x5
#define GR_COMBINE_FACTOR_DETAIL_FACTOR GR_COMBINE_FACTOR_TEXTURE_ALPHA
#define GR_COMBINE_FACTOR_LOD_FRACTION  0x5
#define GR_COMBINE_FACTOR_ONE           0x8
#define GR_COMBINE_FACTOR_ONE_MINUS_LOCAL 0x9
#define GR_COMBINE_FACTOR_ONE_MINUS_OTHER_ALPHA 0xa
#define GR_COMBINE_FACTOR_ONE_MINUS_LOCAL_ALPHA 0xb
#define GR_COMBINE_FACTOR_ONE_MINUS_TEXTURE_ALPHA 0xc
#define GR_COMBINE_FACTOR_ONE_MINUS_DETAIL_FACTOR GR_COMBINE_FACTOR_ONE_MINUS_TEXTURE_ALPHA
#define GR_COMBINE_FACTOR_ONE_MINUS_LOD_FRACTION 0xd


typedef FxI32 GrCombineLocal_t;
#define GR_COMBINE_LOCAL_ITERATED 0x0
#define GR_COMBINE_LOCAL_CONSTANT 0x1
#define GR_COMBINE_LOCAL_NONE GR_COMBINE_LOCAL_CONSTANT
#define GR_COMBINE_LOCAL_DEPTH  0x2

typedef FxI32 GrCombineOther_t;
#define GR_COMBINE_OTHER_ITERATED 0x0
#define GR_COMBINE_OTHER_TEXTURE 0x1
#define GR_COMBINE_OTHER_CONSTANT 0x2
#define GR_COMBINE_OTHER_NONE GR_COMBINE_OTHER_CONSTANT


typedef FxI32 GrAlphaSource_t;
#define GR_ALPHASOURCE_CC_ALPHA 0x0
#define GR_ALPHASOURCE_ITERATED_ALPHA 0x1
#define GR_ALPHASOURCE_TEXTURE_ALPHA 0x2
#define GR_ALPHASOURCE_TEXTURE_ALPHA_TIMES_ITERATED_ALPHA 0x3


typedef FxI32 GrColorCombineFnc_t;
#define GR_COLORCOMBINE_ZERO 0x0
#define GR_COLORCOMBINE_CCRGB 0x1
#define GR_COLORCOMBINE_ITRGB 0x2
#define GR_COLORCOMBINE_ITRGB_DELTA0 0x3
#define GR_COLORCOMBINE_DECAL_TEXTURE 0x4
#define GR_COLORCOMBINE_TEXTURE_TIMES_CCRGB 0x5
#define GR_COLORCOMBINE_TEXTURE_TIMES_ITRGB 0x6
#define GR_COLORCOMBINE_TEXTURE_TIMES_ITRGB_DELTA0 0x7
#define GR_COLORCOMBINE_TEXTURE_TIMES_ITRGB_ADD_ALPHA 0x8
#define GR_COLORCOMBINE_TEXTURE_TIMES_ALPHA 0x9
#define GR_COLORCOMBINE_TEXTURE_TIMES_ALPHA_ADD_ITRGB 0xa
#define GR_COLORCOMBINE_TEXTURE_ADD_ITRGB 0xb
#define GR_COLORCOMBINE_TEXTURE_SUB_ITRGB 0xc
#define GR_COLORCOMBINE_CCRGB_BLEND_ITRGB_ON_TEXALPHA 0xd
#define GR_COLORCOMBINE_DIFF_SPEC_A 0xe
#define GR_COLORCOMBINE_DIFF_SPEC_B 0xf
#define GR_COLORCOMBINE_ONE 0x10

typedef FxI32 GrAlphaBlendFnc_t;
#define GR_BLEND_ZERO 0x0
#define GR_BLEND_SRC_ALPHA 0x1
#define GR_BLEND_SRC_COLOR 0x2
#define GR_BLEND_DST_COLOR GR_BLEND_SRC_COLOR
#define GR_BLEND_DST_ALPHA 0x3 
#define GR_BLEND_ONE 0x4
#define GR_BLEND_ONE_MINUS_SRC_ALPHA 0x5
#define GR_BLEND_ONE_MINUS_SRC_COLOR 0x6
#define GR_BLEND_ONE_MINUS_DST_COLOR GR_BLEND_ONE_MINUS_SRC_COLOR 
#define GR_BLEND_ONE_MINUS_DST_ALPHA 0x7
#define GR_BLEND_RESERVED_8 0x8
#define GR_BLEND_RESERVED_9 0x9
#define GR_BLEND_RESERVED_A 0xa
#define GR_BLEND_RESERVED_B 0xb
#define GR_BLEND_RESERVED_C 0xc
#define GR_BLEND_RESERVED_D 0xd
#define GR_BLEND_RESERVED_E 0xe
#define GR_BLEND_ALPHA_SATURATE 0xf
#define GR_BLEND_PREFOG_COLOR GR_BLEND_ALPHA_SATURATE

typedef FxI32 GrAspectRatio_t;
#define GR_ASPECT_LOG2_8x1        3       /* 8W x 1H */
#define GR_ASPECT_LOG2_4x1        2       /* 4W x 1H */
#define GR_ASPECT_LOG2_2x1        1       /* 2W x 1H */
#define GR_ASPECT_LOG2_1x1        0       /* 1W x 1H */
#define GR_ASPECT_LOG2_1x2       -1       /* 1W x 2H */
#define GR_ASPECT_LOG2_1x4       -2       /* 1W x 4H */
#define GR_ASPECT_LOG2_1x8       -3       /* 1W x 8H */

#ifdef GLIDE2
#define GR_ASPECT_8x1 0x0       /* 8W x 1H */
#define GR_ASPECT_4x1 0x1       /* 4W x 1H */
#define GR_ASPECT_2x1 0x2       /* 2W x 1H */
#define GR_ASPECT_1x1 0x3       /* 1W x 1H */
#define GR_ASPECT_1x2 0x4       /* 1W x 2H */
#define GR_ASPECT_1x4 0x5       /* 1W x 4H */
#define GR_ASPECT_1x8 0x6       /* 1W x 8H */
extern GrAspectRatio_t TOG3AspectTable[];	// Put in GR_ASPECT and get out GR_ASPECT_LOG2
extern GrAspectRatio_t TOG2AspectTable[]; // Put in GR_ASPECT_LOG2 and get out GR_ASPECT
#define TO_GLIDE3_ASPECT(a) (a) = TOG3AspectTable[a];
#define TO_GLIDE2_ASPECT(a) (a) = TOG2AspectTable[a+3];
#define AS_GLIDE3_ASPECT(a) (TOG3AspectTable[a])
#define AS_GLIDE2_ASPECT(a) (TOG2AspectTable[a+3])
#endif

typedef FxI32 GrBuffer_t;
#define GR_BUFFER_FRONTBUFFER   0x0
#define GR_BUFFER_BACKBUFFER    0x1
#define GR_BUFFER_AUXBUFFER     0x2
#define GR_BUFFER_DEPTHBUFFER   0x3
#define GR_BUFFER_ALPHABUFFER   0x4
#define GR_BUFFER_TRIPLEBUFFER  0x5

typedef FxI32 GrChromakeyMode_t;
#define GR_CHROMAKEY_DISABLE    0x0
#define GR_CHROMAKEY_ENABLE     0x1

typedef FxI32 GrChromaRangeMode_t;
#define GR_CHROMARANGE_RGB_ALL_EXT  0x0

#define GR_CHROMARANGE_DISABLE_EXT  0x00
#define GR_CHROMARANGE_ENABLE_EXT   0x01

typedef FxI32 GrTexChromakeyMode_t;
#define GR_TEXCHROMA_DISABLE_EXT               0x0
#define GR_TEXCHROMA_ENABLE_EXT                0x1

#define GR_TEXCHROMARANGE_RGB_ALL_EXT  0x0

typedef FxI32 GrCmpFnc_t;
#define GR_CMP_NEVER    0x0
#define GR_CMP_LESS     0x1
#define GR_CMP_EQUAL    0x2
#define GR_CMP_LEQUAL   0x3
#define GR_CMP_GREATER  0x4
#define GR_CMP_NOTEQUAL 0x5
#define GR_CMP_GEQUAL   0x6
#define GR_CMP_ALWAYS   0x7

typedef FxI32 GrColorFormat_t;
#define GR_COLORFORMAT_ARGB     0x0
#define GR_COLORFORMAT_ABGR     0x1

#define GR_COLORFORMAT_RGBA     0x2
#define GR_COLORFORMAT_BGRA     0x3

typedef FxI32 GrCullMode_t;
#define GR_CULL_DISABLE         0x0
#define GR_CULL_NEGATIVE        0x1
#define GR_CULL_POSITIVE        0x2

typedef FxI32 GrDepthBufferMode_t;
#define GR_DEPTHBUFFER_DISABLE                  0x0
#define GR_DEPTHBUFFER_ZBUFFER                  0x1
#define GR_DEPTHBUFFER_WBUFFER                  0x2
#define GR_DEPTHBUFFER_ZBUFFER_COMPARE_TO_BIAS  0x3
#define GR_DEPTHBUFFER_WBUFFER_COMPARE_TO_BIAS  0x4

typedef FxI32 GrDitherMode_t;
#define GR_DITHER_DISABLE       0x0
#define GR_DITHER_2x2           0x1
#define GR_DITHER_4x4           0x2

#if defined(__linux__) || defined(_WIN32)
typedef FxI32 GrStippleMode_t;
#define GR_STIPPLE_DISABLE	0x0
#define GR_STIPPLE_PATTERN	0x1
#define GR_STIPPLE_ROTATE	0x2
#endif /* __linux__ */

typedef FxI32 GrFogMode_t;
#define GR_FOG_DISABLE                     0x0
#define GR_FOG_WITH_TABLE_ON_FOGCOORD_EXT  0x1
#define GR_FOG_WITH_TABLE_ON_Q             0x2
#define GR_FOG_WITH_TABLE_ON_W             GR_FOG_WITH_TABLE_ON_Q
#define GR_FOG_WITH_ITERATED_Z             0x3
#define GR_FOG_WITH_ITERATED_ALPHA_EXT     0x4
#define GR_FOG_MULT2                       0x100
#define GR_FOG_ADD2                        0x200

typedef FxU32 GrLock_t;
#define GR_LFB_READ_ONLY				0x00
#define GR_LFB_WRITE_ONLY				0x01
#define GR_LFB_IDLE						0x00
#define GR_LFB_NOIDLE					0x10

// Special Glide XP extension that will explicitly not allow reading from the lfb pointer
#define GR_LFB_WRITE_ONLY_EXPLICIT_EXT	0x02	

typedef FxI32 GrLfbBypassMode_t;
#define GR_LFBBYPASS_DISABLE    0x0
#define GR_LFBBYPASS_ENABLE     0x1

typedef FxI32 GrLfbWriteMode_t;
#define GR_LFBWRITEMODE_565        0x0 /* RGB:RGB */
#define GR_LFBWRITEMODE_555        0x1 /* RGB:RGB */
#define GR_LFBWRITEMODE_1555       0x2 /* ARGB:ARGB */
#define GR_LFBWRITEMODE_RESERVED1  0x3
#define GR_LFBWRITEMODE_888        0x4 /* RGB */
#define GR_LFBWRITEMODE_8888       0x5 /* ARGB */
#define GR_LFBWRITEMODE_RESERVED2  0x6
#define GR_LFBWRITEMODE_RESERVED3  0x7
#define GR_LFBWRITEMODE_RESERVED4  0x8
#define GR_LFBWRITEMODE_RESERVED5  0x9
#define GR_LFBWRITEMODE_RESERVED6  0xa
#define GR_LFBWRITEMODE_RESERVED7  0xb
#define GR_LFBWRITEMODE_565_DEPTH  0xc /* RGB:DEPTH */
#define GR_LFBWRITEMODE_555_DEPTH  0xd /* RGB:DEPTH */
#define GR_LFBWRITEMODE_1555_DEPTH 0xe /* ARGB:DEPTH */
#define GR_LFBWRITEMODE_ZA16       0xf /* DEPTH:DEPTH */
#define GR_LFBWRITEMODE_ANY        0xFF

typedef FxI32 GrOriginLocation_t;
#define GR_ORIGIN_UPPER_LEFT    0x0
#define GR_ORIGIN_LOWER_LEFT    0x1
#define GR_ORIGIN_ANY           0xFF

typedef struct {
    int                size;
    void               *lfbPtr;
    FxU32              strideInBytes;        
    GrLfbWriteMode_t   writeMode;
    GrOriginLocation_t origin;
} GrLfbInfo_t;

typedef FxI32 GrLOD_t;
#define GR_LOD_LOG2_256         0x8
#define GR_LOD_LOG2_128         0x7
#define GR_LOD_LOG2_64          0x6
#define GR_LOD_LOG2_32          0x5
#define GR_LOD_LOG2_16          0x4
#define GR_LOD_LOG2_8           0x3
#define GR_LOD_LOG2_4           0x2
#define GR_LOD_LOG2_2           0x1
#define GR_LOD_LOG2_1           0x0

#if GLIDE2
#define GR_LOD_2048             -0x3
#define GR_LOD_1024             -0x2
#define GR_LOD_512              -0x1
#define GR_LOD_256              0x0
#define GR_LOD_128              0x1
#define GR_LOD_64               0x2
#define GR_LOD_32               0x3
#define GR_LOD_16               0x4
#define GR_LOD_8                0x5
#define GR_LOD_4                0x6
#define GR_LOD_2                0x7
#define GR_LOD_1                0x8
extern GrLOD_t TOG3LODTable[];	// Put in GR_LOD and get out GR_LOD_LOG2
extern GrLOD_t TOG2LODTable[];	// Put in GR_LOD_LOG2 and get out GR_LOD
#define TO_GLIDE3_LOD(a) (a) = TOG3LODTable[a+3];
#define TO_GLIDE2_LOD(a) (a) = TOG2LODTable[a];
#define AS_GLIDE3_LOD(a) (TOG3LODTable[a+3])
#define AS_GLIDE2_LOD(a) (TOG2LODTable[a])
#endif

typedef FxI32 GrMipMapMode_t;
#define GR_MIPMAP_DISABLE               0x0 /* no mip mapping  */
#define GR_MIPMAP_NEAREST               0x1 /* use nearest mipmap */
#define GR_MIPMAP_NEAREST_DITHER        0x2 /* GR_MIPMAP_NEAREST + LOD dith */

typedef FxI32 GrSmoothingMode_t;
#define GR_SMOOTHING_DISABLE    0x0
#define GR_SMOOTHING_ENABLE     0x1

typedef FxI32 GrTextureClampMode_t;
#define GR_TEXTURECLAMP_WRAP        0x0
#define GR_TEXTURECLAMP_CLAMP       0x1
#define GR_TEXTURECLAMP_MIRROR_EXT  0x2

typedef FxI32 GrTextureCombineFnc_t;
#define GR_TEXTURECOMBINE_ZERO          0x0 /* texout = 0 */
#define GR_TEXTURECOMBINE_DECAL         0x1 /* texout = texthis */
#define GR_TEXTURECOMBINE_OTHER         0x2 /* this TMU in passthru mode */
#define GR_TEXTURECOMBINE_ADD           0x3 /* tout = tthis + t(this+1) */
#define GR_TEXTURECOMBINE_MULTIPLY      0x4 /* texout = tthis * t(this+1) */
#define GR_TEXTURECOMBINE_SUBTRACT      0x5 /* Sutract from upstream TMU */
#define GR_TEXTURECOMBINE_DETAIL        0x6 /* detail--detail on tthis */
#define GR_TEXTURECOMBINE_DETAIL_OTHER  0x7 /* detail--detail on tthis+1 */
#define GR_TEXTURECOMBINE_TRILINEAR_ODD 0x8 /* trilinear--odd levels tthis*/
#define GR_TEXTURECOMBINE_TRILINEAR_EVEN 0x9 /*trilinear--even levels tthis*/
#define GR_TEXTURECOMBINE_ONE           0xa /* texout = 0xFFFFFFFF */

typedef FxI32 GrTextureFilterMode_t;
#define GR_TEXTUREFILTER_POINT_SAMPLED  0x0
#define GR_TEXTUREFILTER_BILINEAR       0x1

typedef FxI32 GrTextureFormat_t;
#define GR_TEXFMT_8BIT                  0x0
#define GR_TEXFMT_RGB_332 GR_TEXFMT_8BIT
#define GR_TEXFMT_YIQ_422               0x1
#define GR_TEXFMT_ALPHA_8               0x2 /* (0..0xFF) alpha     */
#define GR_TEXFMT_INTENSITY_8           0x3 /* (0..0xFF) intensity */
#define GR_TEXFMT_ALPHA_INTENSITY_44    0x4
#define GR_TEXFMT_P_8                   0x5 /* 8-bit palette */
#define GR_TEXFMT_RSVD0                 0x6
#define GR_TEXFMT_RSVD1                 0x7
#define GR_TEXFMT_16BIT                 0x8
#define GR_TEXFMT_ARGB_8332 GR_TEXFMT_16BIT
#define GR_TEXFMT_AYIQ_8422             0x9
#define GR_TEXFMT_RGB_565               0xa
#define GR_TEXFMT_ARGB_1555             0xb
#define GR_TEXFMT_ARGB_4444             0xc
#define GR_TEXFMT_ALPHA_INTENSITY_88    0xd
#define GR_TEXFMT_AP_88                 0xe /* 8-bit alpha 8-bit palette */
#define GR_TEXFMT_RSVD2                 0xf

typedef FxU32 GrTexTable_t;
#define GR_TEXTABLE_NCC0                 0x0
#define GR_TEXTABLE_NCC1                 0x1
#define GR_TEXTABLE_PALETTE              0x2
#define GR_TEXTABLE_PALETTE_6666_EXT     0x3

typedef FxU32 GrNCCTable_t;
#define GR_NCCTABLE_NCC0    0x0
#define GR_NCCTABLE_NCC1    0x1

typedef FxU32 GrTexBaseRange_t;
#define GR_TEXBASE_256      0x3
#define GR_TEXBASE_128      0x2
#define GR_TEXBASE_64       0x1
#define GR_TEXBASE_32_TO_1  0x0

#if GLIDE2
#define GR_G2_TEXBASE_256      0x0
#define GR_G2_TEXBASE_128      0x1
#define GR_G2_TEXBASE_64       0x2
#define GR_G2_TEXBASE_32_TO_1  0x3
extern GrTexBaseRange_t TOG3TexBaseTable[];	// Put in GR_G2_TEXBASE and get out GR_TEXBASE
extern GrTexBaseRange_t TOG2TexBaseTable[];	// Put in GR_TEXBASE and get out GR_G2_TEXBASE
#define TO_GLIDE3_TEXBASE(a) (a) = TOG3TexBaseTable[a];
#define TO_GLIDE2_TEXBASE(a) (a) = TOG2TexBaseTable[a];
#define AS_GLIDE3_TEXBASE(a) (TOG3TexBaseTable[a])
#define AS_GLIDE2_TEXBASE(a) (TOG2TexBaseTable[a])
#endif

typedef FxU32 GrEnableMode_t;
#define GR_MODE_DISABLE     0x0
#define GR_MODE_ENABLE      0x1

#define GR_AA_ORDERED            0x01
#define GR_ALLOW_MIPMAP_DITHER   0x02
#define GR_PASSTHRU              0x03
#define GR_SHAMELESS_PLUG        0x04
#define GR_VIDEO_SMOOTHING       0x05

typedef FxU32 GrCoordinateSpaceMode_t;
#define GR_WINDOW_COORDS    0x00
#define GR_CLIP_COORDS      0x01

/* Types of data in strips */
#define GR_FLOAT        0
#define GR_U8           1

/* Parameters for strips */
#define GR_PARAM_XY       0x01
#define GR_PARAM_Z        0x02
#define GR_PARAM_W        0x03
#define GR_PARAM_Q        0x04
#define GR_PARAM_FOG_EXT  0x05

#define GR_PARAM_A        0x10

#define GR_PARAM_RGB      0x20

#define GR_PARAM_PARGB    0x30

#define GR_PARAM_ST0      0x40
#define GR_PARAM_ST1      GR_PARAM_ST0+1
#define GR_PARAM_ST2      GR_PARAM_ST0+2

#define GR_PARAM_Q0       0x50
#define GR_PARAM_Q1       GR_PARAM_Q0+1
#define GR_PARAM_Q2       GR_PARAM_Q0+2

#define GR_PARAM_DISABLE  0x00
#define GR_PARAM_ENABLE   0x01

/*
** grDrawVertexArray/grDrawVertexArrayContiguous primitive type
*/
#define GR_POINTS                        0
#define GR_LINE_STRIP                    1
#define GR_LINES                         2
#define GR_POLYGON                       3
#define GR_TRIANGLE_STRIP                4
#define GR_TRIANGLE_FAN                  5
#define GR_TRIANGLES                     6
#define GR_TRIANGLE_STRIP_CONTINUE       7
#define GR_TRIANGLE_FAN_CONTINUE         8

/* 
** grGet/grReset types
*/
#define GR_BITS_DEPTH                   0x01
#define GR_BITS_RGBA                    0x02
#define GR_FIFO_FULLNESS                0x03
#define GR_FOG_TABLE_ENTRIES            0x04
#define GR_GAMMA_TABLE_ENTRIES          0x05
#define GR_GLIDE_STATE_SIZE             0x06
#define GR_GLIDE_VERTEXLAYOUT_SIZE      0x07
#define GR_IS_BUSY                      0x08
#define GR_LFB_PIXEL_PIPE               0x09
#define GR_MAX_TEXTURE_SIZE             0x0a
#define GR_MAX_TEXTURE_ASPECT_RATIO     0x0b
#define GR_MEMORY_FB                    0x0c
#define GR_MEMORY_TMU                   0x0d
#define GR_MEMORY_UMA                   0x0e
#define GR_NUM_BOARDS                   0x0f
#define GR_NON_POWER_OF_TWO_TEXTURES    0x10
#define GR_NUM_FB                       0x11
#define GR_NUM_SWAP_HISTORY_BUFFER      0x12
#define GR_NUM_TMU                      0x13
#define GR_PENDING_BUFFERSWAPS          0x14
#define GR_REVISION_FB                  0x15
#define GR_REVISION_TMU                 0x16
#define GR_STATS_LINES                  0x17  /* grGet/grReset */
#define GR_STATS_PIXELS_AFUNC_FAIL      0x18
#define GR_STATS_PIXELS_CHROMA_FAIL     0x19
#define GR_STATS_PIXELS_DEPTHFUNC_FAIL  0x1a
#define GR_STATS_PIXELS_IN              0x1b
#define GR_STATS_PIXELS_OUT             0x1c
#define GR_STATS_PIXELS                 0x1d  /* grReset */
#define GR_STATS_POINTS                 0x1e  /* grGet/grReset */
#define GR_STATS_TRIANGLES_IN           0x1f
#define GR_STATS_TRIANGLES_OUT          0x20
#define GR_STATS_TRIANGLES              0x21  /* grReset */
#define GR_SWAP_HISTORY                 0x22
#define GR_SUPPORTS_PASSTHRU            0x23
#define GR_TEXTURE_ALIGN                0x24
#define GR_VIDEO_POSITION               0x25
#define GR_VIEWPORT                     0x26
#define GR_WDEPTH_MIN_MAX               0x27
#define GR_ZDEPTH_MIN_MAX               0x28
#define GR_VERTEX_PARAMETER             0x29
#define GR_BITS_GAMMA                   0x2a
#define GR_GET_RESERVED_1               0x1000

/*
** grGetString types
*/
#define GR_EXTENSION                    0xa0
#define GR_HARDWARE                     0xa1
#define GR_RENDERER                     0xa2
#define GR_VENDOR                       0xa3
#define GR_VERSION                      0xa4

typedef int GrSstType;
#define GR_SSTTYPE_VOODOO    0
#define GR_SSTTYPE_SST96     1
#define GR_SSTTYPE_AT3D      2
#define GR_SSTTYPE_Voodoo2   3
#define GR_SSTTYPE_Banshee   4
#define GR_SSTTYPE_Voodoo3   5
#define GR_SSTTYPE_Voodoo4   6


/*
** -----------------------------------------------------------------------
** STRUCTURES
** -----------------------------------------------------------------------
*/

typedef struct {
    GrLOD_t           smallLodLog2;
    GrLOD_t           largeLodLog2;
    GrAspectRatio_t   aspectRatioLog2;
    GrTextureFormat_t format;
    void              *data;
} GrTexInfo;

typedef struct GrTMUConfig_St {
  int    tmuRev;                /* Rev of Texelfx chip */
  int    tmuRam;                /* 1, 2, or 4 MB */
} GrTMUConfig_t;

typedef struct GrVoodooConfig_St {
  int    fbRam;                         /* 1, 2, or 4 MB */
  int    fbiRev;                        /* Rev of Pixelfx chip */
  int    nTexelfx;                      /* How many texelFX chips are there? */
  FxBool sliDetect;                     /* Is it a scan-line interleaved board? */
  GrTMUConfig_t tmuConfig[GLIDE_NUM_TMU];   /* Configuration of the Texelfx chips */
} GrVoodooConfig_t;

typedef struct GrSst96Config_St {
  int   fbRam;                  /* How much? */
  int   nTexelfx;
  GrTMUConfig_t tmuConfig;
} GrSst96Config_t;

typedef GrVoodooConfig_t GrVoodoo2Config_t;

typedef struct GrAT3DConfig_St {
  int   rev;
} GrAT3DConfig_t;

typedef struct {
  int num_sst;                  /* # of HW units in the system */
  struct {
    GrSstType type;             /* Which hardware is it? */
    union SstBoard_u {
      GrVoodooConfig_t  VoodooConfig;
      GrSst96Config_t   SST96Config;
      GrAT3DConfig_t    AT3DConfig;
      GrVoodoo2Config_t Voodoo2Config;
    } sstBoard;
  } SSTs[MAX_NUM_SST];          /* configuration for each board */
} GrHwConfiguration;


typedef struct GrSstPerfStats_s {
  FxU32  pixelsIn;              /* # pixels processed (minus buffer clears) */
  FxU32  chromaFail;            /* # pixels not drawn due to chroma key */ 
  FxU32  zFuncFail;             /* # pixels not drawn due to Z comparison */
  FxU32  aFuncFail;             /* # pixels not drawn due to alpha comparison */
  FxU32  pixelsOut;             /* # pixels drawn (including buffer clears) */
} GrSstPerfStats_t;

typedef struct {
  GrScreenResolution_t resolution;
  GrScreenRefresh_t    refresh;
  int                  numColorBuffers;
  int                  numAuxBuffers;
} GrResolution;

typedef GrResolution GlideResolution;

/* In Glide 2 we need GrVertex defined */
#ifdef GLIDE2

typedef struct {
	float sow;	/* s/w texture coordinate */
	float tow;	/* t/w texture coordinate */ 
	float oow;	/* 1/w */
} GrTmuVertex;

typedef struct {
	float x, y, z;                /* X, Y, and Z of scrn space -- Z is ignored */
	float r, g, b;                /* R, G, B, ([0..255.0]) */
	float ooz;                    /* 65535/Z (used for Z-buffering) */
	float a;                      /* Alpha [0..255.0] */
	float oow;                    /* 1/W (used for W-buffering, texturing) */
	GrTmuVertex  tmuvtx[GLIDE_NUM_TMU];
} GrVertex;

/*
** 3DF texture file structs
*/

typedef struct
{
  FxU32               width, height;
  int                 small_lod, large_lod;
  GrAspectRatio_t     aspect_ratio;
  GrTextureFormat_t   format;
} Gu3dfHeader;

typedef struct
{
  FxU8  yRGB[16];
  FxI16 iRGB[4][3];
  FxI16 qRGB[4][3];
  FxU32 packed_data[12];
} GuNccTable;

typedef struct {
    FxU32 data[256];
} GuTexPalette;

typedef union {
    GuNccTable   nccTable;
    GuTexPalette palette;
} GuTexTable;

typedef struct
{
  Gu3dfHeader  header;
  GuTexTable   table;
  void        *data;
  FxU32        mem_required;    /* memory required for mip map in bytes. */
} Gu3dfInfo;

typedef struct
{
  int           sst;                    /* SST where this texture map was stored  */
  FxBool        valid;                  /* set when this table entry is allocated*/
  int           width, height;
  GrAspectRatio_t aspect_ratio;         /* aspect ratio of the mip map.  */
  void          *data;                  /* actual texture data  */

  GrTextureFormat_t  format;                    /* format of the texture table */
  GrMipMapMode_t     mipmap_mode;               /* mip map mode for this texture */
  GrTextureFilterMode_t   magfilter_mode;       /* filtering to be used when magnified */
  GrTextureFilterMode_t   minfilter_mode;       /* filtering to be used with minified  */
  GrTextureClampMode_t    s_clamp_mode;         /* how this texture should be clamped in s */
  GrTextureClampMode_t    t_clamp_mode;         /* how this texture should be clamped in t */
  FxU32         tLOD;                   /* Register value for tLOD register */ 
  FxU32         tTextureMode;           /* Register value for tTextureMode register
                                           not including non-texture specific bits */
  FxU32         lod_bias;               /* LOD bias of the mip map in preshifted 4.2*/
  FxFloat       float_lod_bias;         /* LOD bias of the mip map in preshifted 4.2*/
  GrLOD_t       lod_min, lod_max;       /* largest and smallest levels of detail  */
  int           tmu;                    /* tmu on which this texture resides */
  FxU32         odd_even_mask;          /* mask specifying levels on this tmu  */
  FxU32         tmu_base_address;       /* base addr (in TMU mem) of this texture */
  FxBool        trilinear;              /* should we blend by lod? */

  GuNccTable    ncc_table;              /* NCC compression table (optional) */
} GrMipMapInfo;

#endif

#define GR_QUERY_ANY  ((FxU32)(~0))

typedef FxU32 GrLfbSrcFmt_t;
#define GR_LFB_SRC_FMT_565          0x00
#define GR_LFB_SRC_FMT_555          0x01
#define GR_LFB_SRC_FMT_1555         0x02
#define GR_LFB_SRC_FMT_888          0x04
#define GR_LFB_SRC_FMT_8888         0x05
#define GR_LFB_SRC_FMT_565_DEPTH    0x0c
#define GR_LFB_SRC_FMT_555_DEPTH    0x0d
#define GR_LFB_SRC_FMT_1555_DEPTH   0x0e
#define GR_LFB_SRC_FMT_ZA16         0x0f
#define GR_LFB_SRC_FMT_RLE16        0x80

#ifndef GLIDE2
#ifdef H3D
#define GR_HINT_H3DENABLE               4
#undef  GR_HINTTYPE_MAX
#define GR_HINTTYPE_MAX 4
#endif

#else
typedef FxU32 GrHint_t;
#define GR_HINTTYPE_MIN                 0
#define GR_HINT_STWHINT                 0
#define GR_HINT_FIFOCHECKHINT           1
#define GR_HINT_FPUPRECISION            2
#define GR_HINT_ALLOW_MIPMAP_DITHER     3
#define GR_HINT_LFB_WRITE               4
#define GR_HINT_LFB_PROTECT             5
#define GR_HINT_LFB_RESET               6
#define GR_HINTTYPE_MAX                 GR_HINT_LFB_RESET
#ifdef H3D
#define GR_HINT_H3DENABLE               GR_HINTTYPE_MAX
#undef  GR_HINTTYPE_MAX
#define GR_HINTTYPE_MAX                 GR_HIT_H3DENABLE
#endif

typedef FxU32 GrSTWHint_t;
#define GR_STWHINT_W_DIFF_FBI   FXBIT(0)
#define GR_STWHINT_W_DIFF_TMU0  FXBIT(1)
#define GR_STWHINT_ST_DIFF_TMU0 FXBIT(2)
#define GR_STWHINT_W_DIFF_TMU1  FXBIT(3)
#define GR_STWHINT_ST_DIFF_TMU1 FXBIT(4)
#define GR_STWHINT_W_DIFF_TMU2  FXBIT(5)
#define GR_STWHINT_ST_DIFF_TMU2 FXBIT(6)

typedef FxU32 GrControl_t;
#define GR_CONTROL_ACTIVATE   0x1
#define GR_CONTROL_DEACTIVATE 0x2
#define GR_CONTROL_RESIZE     0x3
#define GR_CONTROL_MOVE       0x4
#endif

/*
** -----------------------------------------------------------------------
** FUNCTION PROTOTYPES
** -----------------------------------------------------------------------
*/
#ifndef FX_GLIDE_NO_FUNC_PROTO
/*
** rendering functions
*/

#ifdef GLIDE2

FX_ENTRY_G2 void FX_CALL
grDrawPlanarPolygon( int nVerts, int ilist[], GrVertex vlist[] );

FX_ENTRY_G2 void FX_CALL
grDrawPolygon( int nVerts, int ilist[], GrVertex vlist[] );

FX_ENTRY_G2 void FX_CALL
grDrawPlanarPolygonVertexList( int nVerts, GrVertex vlist[] );

FX_ENTRY_G2 void FX_CALL
grDrawPolygonVertexList( int nVerts, GrVertex vlist[] );

#endif

FX_ENTRY void FX_CALL
grDrawPoint( const void *pt );

FX_ENTRY void FX_CALL
grDrawLine( const void *v1, const void *v2 );

FX_ENTRY void FX_CALL
grDrawTriangle( const void *a, const void *b, const void *c );
FX_ENTRY_G3 void FX_CALL
grVertexLayout(FxU32 param, FxI32 offset, FxU32 mode);

FX_ENTRY_G3 void FX_CALL 
grDrawVertexArray(FxU32 mode, FxU32 Count, void *pointers);

FX_ENTRY_G3 void FX_CALL 
grDrawVertexArrayContiguous(FxU32 mode, FxU32 Count, void *pointers, FxU32 stride);

/*
** buffer management
*/
#ifndef GLIDE2

FX_ENTRY void FX_CALL
grBufferClear( GrColor_t color, GrAlpha_t alpha, FxU32 depth );

#else

FX_ENTRY_G2 void FX_CALL
grBufferClear( GrColor_t color, GrAlpha_t alpha, FxU16 depth );

FX_ENTRY_G2 int FX_CALL
grBufferNumPending( void );

#endif

#ifndef GLIDE2

FX_ENTRY void FX_CALL
grBufferSwap( FxU32 swap_interval );

#else

FX_ENTRY void FX_CALL
grBufferSwap( int swap_interval );

#endif

FX_ENTRY void FX_CALL
grRenderBuffer( GrBuffer_t buffer );

/*
** error management
*/
typedef void (*GrErrorCallbackFnc_t)( const char *string, FxBool fatal );

FX_ENTRY void FX_CALL 
grErrorSetCallback( GrErrorCallbackFnc_t fnc );

/*
** SST routines
*/
FX_ENTRY_G3 void FX_CALL 
grFinish(void);

FX_ENTRY_G3 void FX_CALL 
grFlush(void);

#ifdef GLIDE2
FX_ENTRY_G2 void FX_CALL 
grSstIdle(void);

FX_ENTRY_G2 FxU32 FX_CALL 
grSstVideoLine( void );

FX_ENTRY_G2 FxBool FX_CALL 
grSstVRetraceOn( void );

FX_ENTRY_G2 FxBool FX_CALL 
grSstIsBusy( void );
#endif /* GLIDE2*/

#ifndef GLIDE2
FX_ENTRY GrContext_t FX_CALL 
grSstWinOpen(
          FxU32                hWnd,
          GrScreenResolution_t screen_resolution,
          GrScreenRefresh_t    refresh_rate,
          GrColorFormat_t      color_format,
          GrOriginLocation_t   origin_location,
          int                  nColBuffers,
          int                  nAuxBuffers);

FX_ENTRY FxBool FX_CALL
grSstWinClose( GrContext_t context );

#else
FX_ENTRY_G2 FxBool FX_CALL 
grSstWinOpen(
          FxU32                hWnd,
          GrScreenResolution_t screen_resolution,
          GrScreenRefresh_t    refresh_rate,
          GrColorFormat_t      color_format,
          GrOriginLocation_t   origin_location,
          int                  nColBuffers,
          int                  nAuxBuffers);

FX_ENTRY_G2 FxBool FX_CALL
grSstWinClose( void );

#ifndef NO_TRANSLATION
FxBool _G3grSstWinClose( GrContext_t context );

#define grSstWinClose _G3grSstWinClose
#endif

FX_ENTRY_G2 FxBool FX_CALL
grSstControl( FxU32 code );

FX_ENTRY_G2 FxBool FX_CALL
grSstControlMode( FxU32 code );

FX_ENTRY_G2 int FX_CALL
grSstScreenWidth (void);

FX_ENTRY_G2 int FX_CALL
grSstScreenHeight (void);

FX_ENTRY_G2 FxU32 FX_CALL
grSstStatus (void);

#endif

FX_ENTRY_G2 FxBool FX_CALL
grSstQueryBoards ( GrHwConfiguration *hwc );

FX_ENTRY_G2 FxBool FX_CALL
grSstQueryHardware ( GrHwConfiguration *hwc );

FX_ENTRY_G3 void FX_CALL
grSetNumPendingBuffers(FxI32 NumPendingBuffers);

FX_ENTRY_G3 FxBool FX_CALL
grSelectContext( GrContext_t context );

FX_ENTRY void FX_CALL
grSstOrigin(GrOriginLocation_t  origin);

FX_ENTRY void FX_CALL 
grSstSelect( int which_sst );

/*
**  Drawing Statistics
*/

#ifdef GLIDE2
FX_ENTRY_G2 void FX_CALL
grSstPerfStats (GrSstPerfStats_t *pStats);

FX_ENTRY_G2 void FX_CALL
grSstResetPerfStats(void);

FX_ENTRY_G2 void FX_CALL
grResetTriStats();

FX_ENTRY_G2 void FX_CALL
grTriStats(FxU32 *trisProcessed, FxU32 *trisDrawn);
#endif

/*
** Glide configuration and special effect maintenance functions
*/
FX_ENTRY void FX_CALL
grAlphaBlendFunction(
                     GrAlphaBlendFnc_t rgb_sf,   GrAlphaBlendFnc_t rgb_df,
                     GrAlphaBlendFnc_t alpha_sf, GrAlphaBlendFnc_t alpha_df
                     );

FX_ENTRY void FX_CALL
grAlphaCombine(
               GrCombineFunction_t function, GrCombineFactor_t factor,
               GrCombineLocal_t local, GrCombineOther_t other,
               FxBool invert
               );

FX_ENTRY void FX_CALL
grAlphaControlsITRGBLighting( FxBool enable );

FX_ENTRY void FX_CALL
grAlphaTestFunction( GrCmpFnc_t function );

FX_ENTRY void FX_CALL
grAlphaTestReferenceValue( GrAlpha_t value );

FX_ENTRY void FX_CALL 
grChromakeyMode( GrChromakeyMode_t mode );

FX_ENTRY void FX_CALL 
grChromakeyValue( GrColor_t value );

FX_ENTRY void FX_CALL 
grClipWindow( FxU32 minx, FxU32 miny, FxU32 maxx, FxU32 maxy );

FX_ENTRY void FX_CALL 
grColorCombine(
               GrCombineFunction_t function, GrCombineFactor_t factor,
               GrCombineLocal_t local, GrCombineOther_t other,
               FxBool invert );

FX_ENTRY void FX_CALL
grColorMask( FxBool rgb, FxBool a );

FX_ENTRY void FX_CALL 
grCullMode( GrCullMode_t mode );

FX_ENTRY void FX_CALL 
grConstantColorValue( GrColor_t value );

#ifndef GLIDE2
FX_ENTRY void FX_CALL 
grDepthBiasLevel( FxI32 level );
#else
FX_ENTRY void FX_CALL 
grDepthBiasLevel( FxI16 level );
#endif

FX_ENTRY void FX_CALL 
grDepthBufferFunction( GrCmpFnc_t function );

FX_ENTRY void FX_CALL 
grDepthBufferMode( GrDepthBufferMode_t mode );

FX_ENTRY void FX_CALL 
grDepthMask( FxBool mask );

FX_ENTRY void FX_CALL 
grDisableAllEffects( void );

FX_ENTRY void FX_CALL 
grDitherMode( GrDitherMode_t mode );

FX_ENTRY void FX_CALL 
grFogColorValue( GrColor_t fogcolor );

FX_ENTRY void FX_CALL 
grFogMode( GrFogMode_t mode );

FX_ENTRY void FX_CALL 
grFogTable( const GrFog_t ft[] );

#ifdef GLIDE2
FX_ENTRY_G2 void FX_CALL
grGammaCorrectionValue (float gamma);
#endif

FX_ENTRY_G3 void FX_CALL 
grLoadGammaTable( FxU32 nentries, FxU32 *red, FxU32 *green, FxU32 *blue);

FX_ENTRY void FX_CALL
grSplash(float x, float y, float width, float height, FxU32 frame);

FX_ENTRY_G3 FxU32 FX_CALL 
grGet( FxU32 pname, FxU32 plength, FxI32 *params );

FX_ENTRY_G3 const char * FX_CALL 
grGetString( FxU32 pname );

FX_ENTRY_G3 FxI32 FX_CALL 
grQueryResolutions( const GrResolution *resTemplate, GrResolution *output );

FX_ENTRY_G3 FxBool FX_CALL 
grReset( FxU32 what );

FX_ENTRY_G3 GrProc FX_CALL
grGetProcAddress( char *procName );

FX_ENTRY_G3 void FX_CALL 
grEnable( GrEnableMode_t mode );

FX_ENTRY_G3 void FX_CALL 
grDisable( GrEnableMode_t mode );

FX_ENTRY_G3 void FX_CALL 
grCoordinateSpace( GrCoordinateSpaceMode_t mode );

FX_ENTRY_G3 void FX_CALL 
grDepthRange( FxFloat n, FxFloat f );

#if defined(__linux__) || defined(_WIN32)
FX_ENTRY void FX_CALL 
grStippleMode( GrStippleMode_t mode );

FX_ENTRY void FX_CALL 
grStipplePattern( GrStipplePattern_t mode );
#endif /* __linux__ */

FX_ENTRY_G3 void FX_CALL 
grViewport( FxI32 x, FxI32 y, FxI32 width, FxI32 height );

/*
** texture mapping control functions
*/

FX_ENTRY FxU32 FX_CALL 
grTexCalcMemRequired(
                     GrLOD_t lodmin, GrLOD_t lodmax,
                     GrAspectRatio_t aspect, GrTextureFormat_t fmt);

FX_ENTRY FxU32 FX_CALL 
grTexTextureMemRequired( FxU32     evenOdd,
                                 GrTexInfo *info   );

FX_ENTRY void FX_CALL 
grTexSource( GrChipID_t tmu,
             FxU32      startAddress,
             FxU32      evenOdd,
             GrTexInfo  *info );


#if defined(GLIDE2) && !defined(NO_TRANSLATION)
FxU32 _G3grTexCalcMemRequired( GrLOD_t lodmin, GrLOD_t lodmax,
                     GrAspectRatio_t aspect, GrTextureFormat_t fmt);

#define grTexCalcMemRequired _G3grTexCalcMemRequired

FxU32 _G3grTexTextureMemRequired( FxU32 evenOdd, GrTexInfo *info);

#define grTexTextureMemRequired _G3grTexTextureMemRequired

void _G3grTexSource( GrChipID_t tmu,
             FxU32      startAddress,
             FxU32      evenOdd,
             GrTexInfo  *info );

#define grTexSource _G3grTexSource

#endif

FX_ENTRY FxU32 FX_CALL 
grTexMinAddress( GrChipID_t tmu );

FX_ENTRY FxU32 FX_CALL 
grTexMaxAddress( GrChipID_t tmu );

#ifndef GLIDE2
FX_ENTRY void FX_CALL 
grTexNCCTable( GrNCCTable_t table );
#else
FX_ENTRY void FX_CALL 
grTexNCCTable( GrChipID_t tmu, GrNCCTable_t table );
#endif

#if GLIDE_POINTCAST_PALETTE
void FX_CALL 
grTexNCCTableExt( GrChipID_t tmu, GrNCCTable_t table );
#endif

FX_ENTRY void FX_CALL 
grTexClampMode(
               GrChipID_t tmu,
               GrTextureClampMode_t s_clampmode,
               GrTextureClampMode_t t_clampmode
               );

FX_ENTRY void FX_CALL 
grTexCombine(
             GrChipID_t tmu,
             GrCombineFunction_t rgb_function,
             GrCombineFactor_t rgb_factor, 
             GrCombineFunction_t alpha_function,
             GrCombineFactor_t alpha_factor,
             FxBool rgb_invert,
             FxBool alpha_invert
             );

FX_ENTRY void FX_CALL 
grTexDetailControl(
                   GrChipID_t tmu,
                   int lod_bias,
                   FxU8 detail_scale,
                   float detail_max
                   );

FX_ENTRY void FX_CALL 
grTexFilterMode(
                GrChipID_t tmu,
                GrTextureFilterMode_t minfilter_mode,
                GrTextureFilterMode_t magfilter_mode
                );


FX_ENTRY void FX_CALL 
grTexLodBiasValue(GrChipID_t tmu, float bias );

FX_ENTRY void FX_CALL 
grTexDownloadMipMap( GrChipID_t tmu,
                     FxU32      startAddress,
                     FxU32      evenOdd,
                     GrTexInfo  *info );

FX_ENTRY void FX_CALL 
grTexDownloadMipMapLevel( GrChipID_t        tmu,
                          FxU32             startAddress,
                          GrLOD_t           thisLod,
                          GrLOD_t           largeLod,
                          GrAspectRatio_t   aspectRatio,
                          GrTextureFormat_t format,
                          FxU32             evenOdd,
                          void              *data );

FX_ENTRY FxBool FX_CALL 
grTexDownloadMipMapLevelPartial( GrChipID_t        tmu,
                                 FxU32             startAddress,
                                 GrLOD_t           thisLod,
                                 GrLOD_t           largeLod,
                                 GrAspectRatio_t   aspectRatio,
                                 GrTextureFormat_t format,
                                 FxU32             evenOdd,
                                 void              *data,
                                 int               start,
                                 int               end );

FxBool FX_CALL 
grTexDownloadMipMapLevelPartialRowExt(GrChipID_t        tmu,
										FxU32             startAddress,
										GrLOD_t           thisLod,
										GrLOD_t           largeLod,
										GrAspectRatio_t   aspectRatio,
										GrTextureFormat_t format,
										FxU32             evenOdd,
										void              *data,
										int               row,
										int               min_s,
										int               max_s);

FX_ENTRY void FX_CALL
grTexMultibaseAddress( GrChipID_t       tmu,
                       GrTexBaseRange_t range,
                       FxU32            startAddress,
                       FxU32            evenOdd,
                       GrTexInfo        *info );


#if GLIDE_POINTCAST_PALETTE
void FX_CALL
grTexDownloadTableExt( GrChipID_t   tmu,
		    GrTexTable_t type, 
                    void         *data );

void FX_CALL
grTexDownloadTablePartialExt( GrChipID_t   tmu,
			   GrTexTable_t type, 
                           void         *data,
                           int          start,
                           int          end );
#endif

#ifndef GLIDE2 // Is Glide3

/* These calls have a tmu param in Glide2 but don't in GLide3 */
FX_ENTRY void FX_CALL
grTexDownloadTable( GrTexTable_t type, 
                    void         *data );


FX_ENTRY void FX_CALL
grTexDownloadTablePartial( GrTexTable_t type, 
                           void         *data,
                           int          start,
                           int          end );
#else // Is Glide2

FX_ENTRY void FX_CALL
grTexDownloadTable( GrChipID_t   tmu,
		    GrTexTable_t type, 
                    void         *data );

FX_ENTRY void FX_CALL
grTexDownloadTablePartial( GrChipID_t   tmu,
			   GrTexTable_t type, 
                           void         *data,
                           int          start,
                           int          end );

/* Hacks for translating LOD and Aspect from Glide 2 to Glide 3 for exported functions */
#ifndef NO_TRANSLATION
void _G3grTexDownloadMipMap( GrChipID_t tmu,
                     FxU32      startAddress,
                     FxU32      evenOdd,
                     GrTexInfo  *info );

#define grTexDownloadMipMap _G3grTexDownloadMipMap

void _G3grTexDownloadMipMapLevel( GrChipID_t        tmu,
                          FxU32             startAddress,
                          GrLOD_t           thisLod,
                          GrLOD_t           largeLod,
                          GrAspectRatio_t   aspectRatio,
                          GrTextureFormat_t format,
                          FxU32             evenOdd,
                          void              *data );

#define grTexDownloadMipMapLevel _G3grTexDownloadMipMapLevel

FxBool _G3grTexDownloadMipMapLevelPartial( GrChipID_t        tmu,
                                 FxU32             startAddress,
                                 GrLOD_t           thisLod,
                                 GrLOD_t           largeLod,
                                 GrAspectRatio_t   aspectRatio,
                                 GrTextureFormat_t format,
                                 FxU32             evenOdd,
                                 void              *data,
                                 int               start,
                                 int               end );

#define grTexDownloadMipMapLevelPartial _G3grTexDownloadMipMapLevelPartial

void _G3grTexMultibaseAddress( GrChipID_t       tmu,
                       GrTexBaseRange_t range,
                       FxU32            startAddress,
                       FxU32            evenOdd,
                       GrTexInfo        *info );

#define grTexMultibaseAddress _G3grTexMultibaseAddress
#endif

#endif

FX_ENTRY void FX_CALL 
grTexMipMapMode( GrChipID_t     tmu, 
                 GrMipMapMode_t mode,
                 FxBool         lodBlend );

FX_ENTRY void FX_CALL 
grTexMultibase( GrChipID_t tmu,
                FxBool     enable );

/*
** linear frame buffer functions
*/

FX_ENTRY FxBool FX_CALL
grLfbLock( GrLock_t type, GrBuffer_t buffer, GrLfbWriteMode_t writeMode,
           GrOriginLocation_t origin, FxBool pixelPipeline, 
           GrLfbInfo_t *info );

FX_ENTRY FxBool FX_CALL
grLfbUnlock( GrLock_t type, GrBuffer_t buffer );

FX_ENTRY void FX_CALL 
grLfbConstantAlpha( GrAlpha_t alpha );

#ifndef GLIDE2
FX_ENTRY void FX_CALL 
grLfbConstantDepth( FxU32 depth );
#else
FX_ENTRY void FX_CALL 
grLfbConstantDepth( FxU16 depth );
#endif

FX_ENTRY void FX_CALL 
grLfbWriteColorSwizzle(FxBool swizzleBytes, FxBool swapWords);

FX_ENTRY void FX_CALL
grLfbWriteColorFormat(GrColorFormat_t colorFormat);

#ifndef GLIDE2
FX_ENTRY FxBool FX_CALL
grLfbWriteRegion( GrBuffer_t dst_buffer, 
                  FxU32 dst_x, FxU32 dst_y, 
                  GrLfbSrcFmt_t src_format, 
                  FxU32 src_width, FxU32 src_height, 
                  FxBool pixelPipeline,
                  FxI32 src_stride, void *src_data );
#else
FX_ENTRY FxBool FX_CALL
grLfbWriteRegion( GrBuffer_t dst_buffer, 
                  FxU32 dst_x, FxU32 dst_y, 
                  GrLfbSrcFmt_t src_format, 
                  FxU32 src_width, FxU32 src_height, 
                  FxI32 src_stride, void *src_data );
#endif

FX_ENTRY FxBool FX_CALL
grLfbReadRegion( GrBuffer_t src_buffer,
                 FxU32 src_x, FxU32 src_y,
                 FxU32 src_width, FxU32 src_height,
                 FxU32 dst_stride, void *dst_data );

/*
**  Antialiasing Functions
*/

FX_ENTRY void FX_CALL
grAADrawTriangle(
                 const void *a, const void *b, const void *c,
                 FxBool ab_antialias, FxBool bc_antialias, FxBool ca_antialias
                 );
#ifdef GLIDE2
FX_ENTRY_G2 void FX_CALL
grAADrawPoint( const GrVertex *pt );

FX_ENTRY_G2 void FX_CALL
grAADrawLine( const GrVertex *v1, const GrVertex *v2 );

FX_ENTRY_G2 void FX_CALL
grAADrawPolygon( int nVerts, const int ilist[], GrVertex vlist[] ) ;

FX_ENTRY_G2 void FX_CALL
grAADrawPolygonVertexList( int nVerts, GrVertex vlist[] ) ;
#endif

/*
** glide management functions
*/
FX_ENTRY void FX_CALL
grGlideInit( void );

FX_ENTRY void FX_CALL
grGlideShutdown( void );

#ifdef GLIDE2
FX_ENTRY_G2 void FX_CALL
grGlideGetVersion( char version[80] );
#endif

FX_ENTRY void FX_CALL
grGlideGetState( void *state );

FX_ENTRY void FX_CALL
grGlideSetState( const void *state );

#ifdef GLIDE2
FX_ENTRY_G2 void FX_CALL
grGlideShamelessPlug(const FxBool on);

FX_ENTRY void FX_CALL
grHints (GrHint_t hintType, FxU32 hints);
#endif

FX_ENTRY_G3 void FX_CALL
grGlideGetVertexLayout( void *layout );

FX_ENTRY_G3 void FX_CALL
grGlideSetVertexLayout( const void *layout );

#endif /* FX_GLIDE_NO_FUNC_PROTO */

#ifdef __cplusplus
}
#endif

#include <glideutl.h>

#endif /* __GLIDE_H__ */
