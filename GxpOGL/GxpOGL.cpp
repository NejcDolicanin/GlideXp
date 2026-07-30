//
// GxpOGL.cpp : Defines the entry point for the DLL application.
//

#include "stdafx.h"
#include "GxpOGL.h"

static HINSTANCE	hImported = 0;
static HINSTANCE	hThis = 0;

int table_num = 0;
GLFunctionStruct		icd_funcs[2];

static bool LoadImportDLL();

// Define Function Pointers and Exports
#define DEFINE_FUNCS
#include "glfuncs.inl"
#undef DEFINE_FUNCS

//
// Load the Imports from the DLL
//
static bool LoadImportDLL()
{
	// Don't do it if we already have loaded
	if (hImported) return true;

	char str[512];
	char *our_name = 0;
	char *icd_name = 0;

#if 0
	// Try opengl32.dll
	GetSystemDirectory(str,sizeof(str)-1);
	strncat(str,"\\opengl32.dll", sizeof(str)-1);
	hImported = LoadLibrary("opengl32.dll");

	if (hThis == hImported)
	{
		OutputDebugString("hThis == hImported -> opengl32.dll\n");
		FreeLibrary(hImported);
		hImported = 0;
	}

	// Try wopengl32.dll
	if (!hImported) 
	{
		//GetSystemDirectory(str,sizeof(str)-1);
		//strncat(str,"\\wopengl32.dll", sizeof(str)-1);
		hImported = LoadLibrary("wopengl32.dll");
	}
	if (hThis == hImported)
	{
		OutputDebugString("hThis == hImported -> wopengl32.dll\n");
		FreeLibrary(hImported);
		hImported = 0;
	}
#endif 

	// First thing, we attempt to check to see if 3dfxogl is us
	if (our_name == 0)
	{
		// Try 3dfxicd.dll
		hImported = LoadLibrary(our_name = "3dfxogl.dll");
		if (hThis != hImported) our_name = 0;
		FreeLibrary(hImported);
		hImported = 0;
	}

	// Now attempt to see if opengl32.dll is us
	if (our_name == 0)
	{
		// Try opengl32.dll
		hImported = LoadLibrary(our_name = "opengl32.dll");
		if (hThis != hImported) our_name = 0;
		FreeLibrary(hImported);
		hImported = 0;
	}

	// Try 3dfxicd.dll
	if (!hImported) 
	{
		//GetSystemDirectory(str,sizeof(str)-1);
		//strncat(str,"\\3dfxogl.dll", sizeof(str)-1);
		hImported = LoadLibrary(icd_name = "3dfxicd.dll");
	}
	if (hThis == hImported)
	{
		OutputDebugString("hThis == hImported -> 3dfxicd.dll\n");
		FreeLibrary(hImported);
		hImported = 0;
		icd_name = 0;
	}
	// Try 3dfxogl.dll
	if (!hImported) 
	{
		//GetSystemDirectory(str,sizeof(str)-1);
		//strncat(str,"\\3dfxogl.dll", sizeof(str)-1);
		hImported = LoadLibrary(icd_name = "3dfxogl.dll");
	}
	if (hThis == hImported)
	{
		OutputDebugString("hThis == hImported -> 3dfxogl.dll\n");
		FreeLibrary(hImported);
		hImported = 0;
		icd_name = 0;
	}
	// Try %SYSTEM_DIR%\3dfxogl.dll
	if (!hImported) 
	{
		GetSystemDirectory(str,sizeof(str)-1);
		strncat(str,"\\3dfxogl.dll", sizeof(str)-1);
		hImported = LoadLibrary(icd_name = str);
	}
	if (hThis == hImported)
	{
		OutputDebugString("hThis == hImported -> %SYSTEM_DIR%\\3dfxogl.dll\n");
		FreeLibrary(hImported);
		hImported = 0;
		icd_name = 0;
	}

	if (!hImported) 
	{
		OutputDebugString("3dfxicd/3dfxogl not found.\n");
		return false;
	}

	#define IMPORT_FUNCS
	#include "glfuncs.inl"
	#undef IMPORT_FUNCS

	// Finally, duplicate the handle to us and the icd
	if (icd_name) LoadLibrary(icd_name);

	// Finally, duplicate the handle to us and the icd
	if (our_name) LoadLibrary(our_name);

	// Now do some 'nasty' stuff and modify the code in the DLL
	// Firstly though verify if the DLL is actually the one we want

	DWORD dll_base;					// The dlls base

	// Choose something we know
	dll_base = ((DWORD) dll_glNewList) - 0x64D0;

	struct { 
		char		*name;
		DWORD		offset;
	} CheckOffsets[] = {
		{ "DrvSwapBuffers", 0x0000B6D0 },
		{ "DrvSwapLayerBuffers", 0x0000B730 },
		{ "DrvSetPixelFormat", 0x0000B750 },
		{ "DrvRealizeLayerPalette", 0x0000B877 },
		{ "DrvGetLayerPaletteEntries", 0x0000B8E6 },
		{ "DrvSetLayerPaletteEntries", 0x0000B956 },
		{ "DrvGetProcAddress", 0x0000B9B0 },
		{ "DrvShareLists", 0x0000B9EE },
		{ "DrvValidateVersion", 0x0000BA80 },
		{ "DrvReleaseContext", 0x0000BAA0 },
		{ "DrvDescribePixelFormat", 0x0000BB30 },
		{ "DrvCopyContext", 0x0000BBDF },
		{ "DrvSetContext", 0x0000BD34 },
		{ "DrvDeleteContext", 0x0000BFF5 },
		{ "DrvDescribeLayerPlane", 0x0000C0D6 },
		{ "DrvCreateLayerContext", 0x0000C1A5 },
		{ "DrvCreateContext", 0x0000C410 },
		{ "wglChoosePixelFormat", 0x0000C460 },
		{ "wglDescribePixelFormat", 0x0000C470 },
		{ "wglGetPixelFormat", 0x0000C480 },
		{ "wglSetPixelFormat", 0x0000C490 },
		{ "wglSwapBuffers", 0x0000C4A0 },
		{ "wglCreateContext", 0x0000C4B0 },
		{ "wglDeleteContext", 0x0000C4C0 },
		{ "wglGetCurrentContext", 0x0000C4D0 },
		{ "wglGetCurrentDC", 0x0000C4E0 },
		{ "wglGetProcAddress", 0x0000C4F0 },
		{ "wglMakeCurrent", 0x0000C500 },
		{ "wglShareLists", 0x0000C520 },
		{ "wglUseFontBitmapsA", 0x0000C530 },
		{ "wglUseFontBitmapsW", 0x0000C540 },
		{ "wglUseFontOutlinesA", 0x0000C550 },
		{ "wglUseFontOutlinesW", 0x0000C560 },
		{ "wglCopyContext", 0x0000C570 },
		{ "wglCreateLayerContext", 0x0000C580 },
		{ "wglDescribeLayerPlane", 0x0000C590 },
		{ "wglGetDefaultProcAddress", 0x0000C5A0 },
		{ "wglGetLayerPaletteEntries", 0x0000C5B0 },
		{ 0, 0x00 }
	};

	for (int i = 0; CheckOffsets[i].name; i++)
	{
		DWORD func_offset = (DWORD) GetProcAddress(hImported, CheckOffsets[i].name);
		func_offset -= dll_base;

		if (CheckOffsets[i].offset != func_offset)
		{
			OutputDebugString("DLL Function Check failed.\n");

			// Not 'our' dll
			return true;
		}

	}

	// Ok, we are reasonable sure it's our dll.
	// But just to be sure, we'll do exception handling

    __try
    {

		// so lets check the date
		const char *date = (char *) (dll_base + 0x2A1C8);

		if (strcmp(date, "/ICD (Jan 16 2001)")) 
		{
			OutputDebugString("DLL Date Check failed.\n");
			return true;
		}

		GLFunctionStruct *table[2];

		table[0] = (GLFunctionStruct *) (dll_base + 0x000F70C0);
		table[1] = (GLFunctionStruct *) (dll_base + 0x000F7728);

		if (table[0]->num_funcs != 0x150 || table[1]->num_funcs != 0x150)
		{
			OutputDebugString("DLL Function Count Check failed.\n");
			return true;
		}

		if (table[0]->glGetString == _glGetString_0)
		{
			//OutputDebugString("DLL Check passed. Already loaded.\n");
			return true;
		}

		icd_funcs[0] = *(table[0]);
		icd_funcs[1] = *(table[1]);

		#define ICD_TABLE_OVERLOAD
		#include "glfuncs.inl"
		#undef ICD_TABLE_OVERLOAD

		//OutputDebugString("DLL Check passed.\n");
    }
	__except ( EXCEPTION_EXECUTE_HANDLER) 
	{
		OutputDebugString("DLL Check failed. Exception Caught.\n");
    }


	return true;
}

BOOL APIENTRY DllMain( HANDLE hModule, 
                       DWORD  ul_reason_for_call, 
                       LPVOID lpReserved
					 )
{
	hThis = (HINSTANCE) hModule;

	switch( ul_reason_for_call ) {
	case DLL_PROCESS_DETACH:
		if (hImported) FreeLibrary(hImported);
		break;
	case DLL_PROCESS_ATTACH:
		if (!LoadImportDLL()) return FALSE;
		break;
	case DLL_THREAD_ATTACH:
		break;
	case DLL_THREAD_DETACH:
		break;
	default:
		break;
	}
	
    return TRUE;
}


