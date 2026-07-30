//
// Glide 2 to Glide 3 Translator for GlideXP
//
// g2utils.cpp : Glide 2 Utility Functions
//
// Copyright (c) 2002 Ryan Nunn
//
// Just a note that this is written in C++ so I can use namespaces.
// No other C++ features are used.
//

#include "g2pch.h"

namespace Glide3 {

gu3dfGetInfo_proc				gu3dfGetInfo = 0;
gu3dfLoad_proc					gu3dfLoad = 0;
guGammaCorrectionRGB_proc		guGammaCorrectionRGB = 0;

bool	SetupUtilsFunctions()
{
#define FN_NAME "SetupUtilsFunctions"
    GDBG_INFO(80, "%s\n", FN_NAME);

	bool ret = true;

	if (ret) ret = LoadFunc(gu3dfGetInfo,8);
	if (ret) ret = LoadFunc(gu3dfLoad,8);
	if (ret) ret = LoadFunc(guGammaCorrectionRGB,12);

	return ret;
#undef FN_NAME
}

bool	FreeUtilsFunctions()
{
#define FN_NAME "FreeUtilsFunctions"
	//GDBG_INFO(80, "%s\n", FN_NAME);

	gu3dfGetInfo = 0;
	gu3dfLoad = 0;
	guGammaCorrectionRGB = 0;

	return true;
#undef FN_NAME
}

};

//
// gu3dfGetInfo
//
FxBool FX_CALL gu3dfGetInfo ( const char *filename, Gu3dfInfo *info )
{
#define FN_NAME "gu3dfGetInfo"
    GDBG_INFO(80, "%s: %s %08X\n", FN_NAME, filename, info);

	// First get the info from Glide3
	FxBool ret = Glide3::gu3dfGetInfo(filename, info);

	// Then Convert it
	info->header.aspect_ratio = ConAspect(info->header.aspect_ratio);
	info->header.large_lod = ConLod(info->header.large_lod);
	info->header.small_lod = ConLod(info->header.small_lod);

    GDBG_INFO(80, "%s: %i %i bytes\n", FN_NAME, ret, info->mem_required);
	return ret;

#undef FN_NAME
}

//
// gu3dfLoad
//
FxBool FX_CALL gu3dfLoad ( const char *filename, Gu3dfInfo *data )
{
#define FN_NAME "SetupUtilsFunctions"
    GDBG_INFO(80, "%s: %s %08X\n", FN_NAME, filename, data);

	// First Convert it
	data->header.aspect_ratio = ConAspect(data->header.aspect_ratio);
	data->header.large_lod = ConLod(data->header.large_lod);
	data->header.small_lod = ConLod(data->header.small_lod);

	// Then Load it with Glide3
	FxBool ret = Glide3::gu3dfLoad(filename, data);

	// Then Convert it back
	data->header.aspect_ratio = ConAspect(data->header.aspect_ratio);
	data->header.large_lod = ConLod(data->header.large_lod);
	data->header.small_lod = ConLod(data->header.small_lod);

    GDBG_INFO(80, "%s: %i\n", FN_NAME, ret);
	return ret;

#undef FN_NAME
}


//
//  guAlphaSource
//
void FX_CALL guAlphaSource ( GrAlphaSource_t mode )
{
#define FN_NAME "guAlphaSource"
    GDBG_INFO(80, "%s (%d)\n", FN_NAME, mode);

	switch ( mode ) {
	case GR_ALPHASOURCE_CC_ALPHA:
		Glide3::grAlphaCombine( GR_COMBINE_FUNCTION_LOCAL, 
			GR_COMBINE_FACTOR_NONE, 
			GR_COMBINE_LOCAL_CONSTANT, 
			GR_COMBINE_OTHER_NONE, 
			FXFALSE );
		break;
		
	case GR_ALPHASOURCE_ITERATED_ALPHA:
		Glide3::grAlphaCombine( GR_COMBINE_FUNCTION_LOCAL, 
			GR_COMBINE_FACTOR_NONE, 
			GR_COMBINE_LOCAL_ITERATED, 
			GR_COMBINE_OTHER_NONE, 
			FXFALSE );
		break;
		
	case GR_ALPHASOURCE_TEXTURE_ALPHA:
		Glide3::grAlphaCombine( GR_COMBINE_FUNCTION_SCALE_OTHER, 
			GR_COMBINE_FACTOR_ONE, 
			GR_COMBINE_LOCAL_NONE, 
			GR_COMBINE_OTHER_TEXTURE, 
			FXFALSE );
		break;
		
	case GR_ALPHASOURCE_TEXTURE_ALPHA_TIMES_ITERATED_ALPHA:
		Glide3::grAlphaCombine( GR_COMBINE_FUNCTION_SCALE_OTHER, 
			GR_COMBINE_FACTOR_LOCAL, 
			GR_COMBINE_LOCAL_ITERATED, 
			GR_COMBINE_OTHER_TEXTURE, 
			FXFALSE );
		break;
		
	default:
		// TODO
		//GR_CHECK_F("guAlphaSource", 1, "unknown alpha source mode");
		break;
	}

#undef FN_NAME
}

//
// guColorCombineFunction
//
void FX_CALL guColorCombineFunction ( GrColorCombineFnc_t fnc )
{
#define FN_NAME "guColorCombineFunction"
    GDBG_INFO(80, "%s (%d)\n", FN_NAME, fnc);
	
	switch ( fnc )
	{
	case GR_COLORCOMBINE_ZERO:
		Glide3::grColorCombine( GR_COMBINE_FUNCTION_ZERO, GR_COMBINE_FACTOR_NONE, GR_COMBINE_LOCAL_NONE, GR_COMBINE_OTHER_NONE, FXFALSE );
		break;
		
	case GR_COLORCOMBINE_CCRGB:
		Glide3::grColorCombine( GR_COMBINE_FUNCTION_LOCAL, GR_COMBINE_FACTOR_NONE, GR_COMBINE_LOCAL_CONSTANT, GR_COMBINE_OTHER_NONE, FXFALSE );
		break;
		
	case GR_COLORCOMBINE_ITRGB_DELTA0:
	case GR_COLORCOMBINE_ITRGB:
		Glide3::grColorCombine( GR_COMBINE_FUNCTION_LOCAL, GR_COMBINE_FACTOR_NONE, GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_NONE, FXFALSE );
		break;
		
	case GR_COLORCOMBINE_DECAL_TEXTURE:
		Glide3::grColorCombine( GR_COMBINE_FUNCTION_SCALE_OTHER, GR_COMBINE_FACTOR_ONE, GR_COMBINE_LOCAL_NONE, GR_COMBINE_OTHER_TEXTURE, FXFALSE );
		break;
		
	case GR_COLORCOMBINE_TEXTURE_TIMES_CCRGB:
		Glide3::grColorCombine( GR_COMBINE_FUNCTION_SCALE_OTHER, GR_COMBINE_FACTOR_LOCAL, GR_COMBINE_LOCAL_CONSTANT, GR_COMBINE_OTHER_TEXTURE, FXFALSE );
		break;
		
	case GR_COLORCOMBINE_TEXTURE_TIMES_ITRGB_DELTA0:
	case GR_COLORCOMBINE_TEXTURE_TIMES_ITRGB:
		Glide3::grColorCombine( GR_COMBINE_FUNCTION_SCALE_OTHER, GR_COMBINE_FACTOR_LOCAL, GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_TEXTURE, FXFALSE );
		break;
		
	case GR_COLORCOMBINE_TEXTURE_TIMES_ITRGB_ADD_ALPHA:
		Glide3::grColorCombine( GR_COMBINE_FUNCTION_SCALE_OTHER_ADD_LOCAL_ALPHA, GR_COMBINE_FACTOR_LOCAL, GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_TEXTURE, FXFALSE );
		break;
		
	case GR_COLORCOMBINE_TEXTURE_TIMES_ALPHA:
		Glide3::grColorCombine( GR_COMBINE_FUNCTION_SCALE_OTHER, GR_COMBINE_FACTOR_LOCAL_ALPHA, GR_COMBINE_LOCAL_NONE, GR_COMBINE_OTHER_TEXTURE, FXFALSE );
		break;
		
	case GR_COLORCOMBINE_TEXTURE_TIMES_ALPHA_ADD_ITRGB:
		Glide3::grColorCombine( GR_COMBINE_FUNCTION_SCALE_OTHER_ADD_LOCAL, GR_COMBINE_FACTOR_LOCAL_ALPHA, GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_TEXTURE, FXFALSE );
		break;
		
	case GR_COLORCOMBINE_TEXTURE_ADD_ITRGB:
		Glide3::grColorCombine( GR_COMBINE_FUNCTION_SCALE_OTHER_ADD_LOCAL, GR_COMBINE_FACTOR_ONE, GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_TEXTURE, FXFALSE );
		break;
		
	case GR_COLORCOMBINE_TEXTURE_SUB_ITRGB:
		Glide3::grColorCombine( GR_COMBINE_FUNCTION_SCALE_OTHER_MINUS_LOCAL, GR_COMBINE_FACTOR_ONE, GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_TEXTURE, FXFALSE );
		break;
		
	case GR_COLORCOMBINE_CCRGB_BLEND_ITRGB_ON_TEXALPHA:
		Glide3::grColorCombine( GR_COMBINE_FUNCTION_BLEND, GR_COMBINE_FACTOR_TEXTURE_ALPHA, GR_COMBINE_LOCAL_CONSTANT, GR_COMBINE_OTHER_ITERATED, FXFALSE );
		break;
		
	case GR_COLORCOMBINE_DIFF_SPEC_A:
		Glide3::grColorCombine( GR_COMBINE_FUNCTION_SCALE_OTHER_ADD_LOCAL, GR_COMBINE_FACTOR_LOCAL_ALPHA, GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_TEXTURE, FXFALSE );
		break;
		
	case GR_COLORCOMBINE_DIFF_SPEC_B:
		Glide3::grColorCombine( GR_COMBINE_FUNCTION_SCALE_OTHER_ADD_LOCAL_ALPHA, GR_COMBINE_FACTOR_LOCAL, GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_TEXTURE, FXFALSE );
		break;
		
	case GR_COLORCOMBINE_ONE:
		Glide3::grColorCombine( GR_COMBINE_FUNCTION_ZERO, GR_COMBINE_FACTOR_NONE, GR_COMBINE_LOCAL_NONE, GR_COMBINE_OTHER_NONE, FXTRUE );
		break;
		
	default:
		// TODO
		//GR_CHECK_F("guColorCombineFunction", 1, "unsupported color combine function");
		break;
	}
#undef FN_NAME
}

//
// guTexCombineFunction
//
void FX_CALL guTexCombineFunction (GrChipID_t tmu, GrTextureCombineFnc_t tc)
{
#define FN_NAME "guTexCombineFunction"
    GDBG_INFO(80, "%s (%d,%d)\n", FN_NAME, tmu, tc);

	switch ( tc )  {
	case GR_TEXTURECOMBINE_ZERO:
		Glide3::grTexCombine( tmu, GR_COMBINE_FUNCTION_ZERO, GR_COMBINE_FACTOR_NONE,
			GR_COMBINE_FUNCTION_ZERO, GR_COMBINE_FACTOR_NONE, FXFALSE, FXFALSE );
		break;
		
	case GR_TEXTURECOMBINE_DECAL:
		Glide3::grTexCombine( tmu, GR_COMBINE_FUNCTION_LOCAL, GR_COMBINE_FACTOR_NONE,
			GR_COMBINE_FUNCTION_LOCAL, GR_COMBINE_FACTOR_NONE, FXFALSE, FXFALSE );
		break;
		
	case GR_TEXTURECOMBINE_ONE:
		Glide3::grTexCombine( tmu, GR_COMBINE_FUNCTION_ZERO, GR_COMBINE_FACTOR_NONE,
			GR_COMBINE_FUNCTION_ZERO, GR_COMBINE_FACTOR_NONE, FXTRUE, FXTRUE );
		break;
		
	case GR_TEXTURECOMBINE_ADD:
		Glide3::grTexCombine( tmu, GR_COMBINE_FUNCTION_SCALE_OTHER_ADD_LOCAL, GR_COMBINE_FACTOR_ONE,
			GR_COMBINE_FUNCTION_SCALE_OTHER_ADD_LOCAL, GR_COMBINE_FACTOR_ONE, FXFALSE, FXFALSE );
		break;
		
	case GR_TEXTURECOMBINE_MULTIPLY:
		Glide3::grTexCombine( tmu, GR_COMBINE_FUNCTION_SCALE_OTHER, GR_COMBINE_FACTOR_LOCAL,
			GR_COMBINE_FUNCTION_SCALE_OTHER, GR_COMBINE_FACTOR_LOCAL, FXFALSE, FXFALSE );
		break;
		
	case GR_TEXTURECOMBINE_DETAIL:
		Glide3::grTexCombine( tmu, GR_COMBINE_FUNCTION_BLEND, GR_COMBINE_FACTOR_ONE_MINUS_DETAIL_FACTOR,
			GR_COMBINE_FUNCTION_BLEND, GR_COMBINE_FACTOR_ONE_MINUS_DETAIL_FACTOR, FXFALSE, FXFALSE );
		break;
		
	case GR_TEXTURECOMBINE_DETAIL_OTHER:
		Glide3::grTexCombine( tmu, GR_COMBINE_FUNCTION_BLEND, GR_COMBINE_FACTOR_DETAIL_FACTOR,
			GR_COMBINE_FUNCTION_BLEND, GR_COMBINE_FACTOR_DETAIL_FACTOR, FXFALSE, FXFALSE );
		break;
		
	case GR_TEXTURECOMBINE_TRILINEAR_ODD:
		Glide3::grTexCombine( tmu, GR_COMBINE_FUNCTION_BLEND, GR_COMBINE_FACTOR_ONE_MINUS_LOD_FRACTION,
			GR_COMBINE_FUNCTION_BLEND, GR_COMBINE_FACTOR_ONE_MINUS_LOD_FRACTION, FXFALSE, FXFALSE );
		break;
		
	case GR_TEXTURECOMBINE_TRILINEAR_EVEN:
		Glide3::grTexCombine( tmu, GR_COMBINE_FUNCTION_BLEND, GR_COMBINE_FACTOR_LOD_FRACTION,
			GR_COMBINE_FUNCTION_BLEND, GR_COMBINE_FACTOR_LOD_FRACTION, FXFALSE, FXFALSE );
		break;
		
	case GR_TEXTURECOMBINE_SUBTRACT:
		Glide3::grTexCombine( tmu, GR_COMBINE_FUNCTION_SCALE_OTHER_MINUS_LOCAL, GR_COMBINE_FACTOR_ONE,
			GR_COMBINE_FUNCTION_SCALE_OTHER_MINUS_LOCAL, GR_COMBINE_FACTOR_ONE, FXFALSE, FXFALSE );
		break;
		
	case GR_TEXTURECOMBINE_OTHER:
		Glide3::grTexCombine( tmu, GR_COMBINE_FUNCTION_SCALE_OTHER, GR_COMBINE_FACTOR_ONE,
			GR_COMBINE_FUNCTION_SCALE_OTHER, GR_COMBINE_FACTOR_ONE, FXFALSE, FXFALSE );
		break;
		
	default:
		// TODO
		//GrErrorCallback( "guTexCombineFunction:  Unsupported function", FXTRUE );
		break;
	}
#undef FN_NAME
}

//
// grTexCombineFunction
//
NAKED_CALL void FX_CALL grTexCombineFunction (GrChipID_t tmu, GrTextureCombineFnc_t tc)
{
#define FN_NAME "grTexCombineFunction"

    VOID_ASM_JMP (guTexCombineFunction,(tmu,tc));

#undef FN_NAME
}

//
// setlevel
//
static inline void setlevel( FxU16 *data, FxU16 color, int width, int height )
{
#define FN_NAME "setLevel"

	for ( int t = 0; t < height; t++ ) {
		for ( int s = 0; s < width; s++ ) {
			*data = color;
			data++;
		}
	}

#undef FN_NAME
}

//
//  guTexCreateColorMipMap
//
FxU16 * FX_CALL guTexCreateColorMipMap ( void )
{
#define FN_NAME "guTexCreateColorMipMap"
    GDBG_INFO(80, "%s\n");

   FxU32 memrequired;
   FxU16 *data;
   FxU16 *start;

   GDBG_INFO(99,"guTexCreateColorMipMap()\n");
   memrequired = 2 * ( 256 * 256 + 128 * 128 + 64 * 64 + 32 * 32 + 16 * 16 + 8 * 8 + 4 * 4 + 2 * 2 + 1 * 1 );
   start = data = (FxU16*) malloc( memrequired );
   if ( !data )
      return 0;

   setlevel( data,            0xF800, 256, 256 );
   setlevel( data += 256*256, 0x07e0, 128, 128 );
   setlevel( data += 128*128, 0x001F, 64, 64 );
   setlevel( data += 64*64,   0xFFFF, 32, 32);
   setlevel( data += 32*32,   0x0000, 16, 16 );
   setlevel( data += 16*16,   0xF800, 8, 8);
   setlevel( data += 8*8,     0x07e0, 4, 4 );
   setlevel( data += 4*4,     0x001f, 2, 2 );
   setlevel( data += 2*2,     0xFFFF, 1, 1 );

   return start;

#undef FN_NAME
}

/*-------------------------------------------------------------------
  Function: guEncodeRle
  Date: 3/5/96
  Implementor(s): jdt
  Library: Glide Utilities
  Description:
  Encode an RGB565 image into RLE16 format
  Arguments:
  dst - destination rle image data ( NULL for bytecount only )
  src - source rgb565 image data
  width - width of source data
  height - height of source data
  Return:
  number of bytes in encoded rle image
  -------------------------------------------------------------------*/
int FX_CALL guEncodeRLE16 ( void *dst, void *src, FxU32 width, FxU32 height )
{
#define FN_NAME "guEncodeRLE16"
    GDBG_INFO(80, "%s (0x%X,0x%X,%d,%d)\n", FN_NAME, dst, src, width,height);

    int byteCount = 0;
    int sourceImageSizeInWords;
    FxU16 *srcPixels;
    FxU32 *dstPixels;

    sourceImageSizeInWords = width * height;

    srcPixels = (FxU16*)src;

    if ( dst ) {
        dstPixels = (FxU32*) dst;
        while( sourceImageSizeInWords-- ) {
            short length    = 1;
            short color     = *srcPixels;
            int   lookAhead = 1;

            while( (sourceImageSizeInWords-length)&&
                   (color == srcPixels[lookAhead]) ) {
                length++;
                lookAhead++;
            }

            *dstPixels = ((((FxU32)length)<<16) | ((FxU32)color));
            dstPixels++;

            byteCount+=4;

            srcPixels+=length;
            sourceImageSizeInWords-=length;            
        }
    } else {
        while( sourceImageSizeInWords-- ) {
            short length    = 1;
            short color     = *srcPixels;
            int   lookAhead = 1;

            while( (sourceImageSizeInWords-length)&&
                   (color == srcPixels[lookAhead]) ) {
                length++;
                lookAhead++;
            }

            byteCount+=4;
            srcPixels+=length;
            sourceImageSizeInWords-=length;            
        }
    }
    return byteCount;

#undef FN_NAME
}

//
// guEndianSwapWords
//
FxU32 FX_CALL guEndianSwapWords ( FxU32 value )
{
#define FN_NAME "guEndianSwapWords"
   return ( ( value & 0xFFFF0000 ) >> 16 ) | ( value << 16 );
#undef FN_NAME
}

//
// guEndianSwapBytes
//
FxU16 FX_CALL guEndianSwapBytes ( FxU16 value )
{
#define FN_NAME "guEndianSwapBytes"
  return ( ( value & 0xFF00 ) >> 8 ) | ( value << 8 );
#undef FN_NAME
}

