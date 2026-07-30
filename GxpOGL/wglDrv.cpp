//
// wglDrv.cpp : wgl* and Drv* function overloading
//

#include "stdafx.h"
#include "GxpOGL.h"

//
// Overloaded wglGetProcAddress and DrvGetProcAddress
//
// This is just so I can return pointers to my functions
//

struct GetProcAddressOverride
{
	const char	*name;
	PROC		proc;
};

GetProcAddressOverride	proc_address_overrides[] =
{
#define GET_PROC_ADDR_OVERRIDES
#include "glfuncs.inl"
#undef GET_PROC_ADDR_OVERRIDES

	{ 0, 0 }
};

// Export: wglGetProcAddress
PROC WINAPI _wglGetProcAddress (LPCSTR lpszProc)
{
	//OutputDebugString("wglGetProcAddress: ");
	//OutputDebugString(lpszProc);
	//OutputDebugString("\n");
	
	GetProcAddressOverride	*over = proc_address_overrides;

	while (over->name && over->proc)
	{
		if (!strcmp(lpszProc,over->name)) return over->proc;
		over++;
	}

	return CALL_IMP(wglGetProcAddress,(lpszProc));
}

// Export: DrvGetProcAddress
PROC WINAPI _DrvGetProcAddress (LPCSTR lpszProc)
{
	//OutputDebugString("DrvGetProcAddress: ");
	//OutputDebugString(lpszProc);
	//OutputDebugString("\n");
	
	GetProcAddressOverride	*over = proc_address_overrides;

	while (over->name && over->proc)
	{
		if (!strcmp(lpszProc,over->name)) return over->proc;
		over++;
	}

	return CALL_IMP(DrvGetProcAddress,(lpszProc));
}


//
// Overloaded DrvSetContext
// 
// This isn't very nice at all, but it works. The callback function passed to
// DrvSetContext is passed a pointer to a rather large structure containing all
// the various OpenGL functions supplied by the ICD. Now, what I can do is
// modify this structure so my functions will always be called. This is a hack
// and quite possibly only works with the 3dfx icd. The 3dfx ICD itself uses 
// this structure to call internally.
//
// TODO: Work out why Elite Force doesn't work as expected but Quake 3 does.
//

// Callback passed to the function
static SetContextCallBack sccb = 0;

// My new callback
static DWORD WINAPI SetContextCallBackReplacement (GLFunctionStruct *s)
{
	// Replace glTexImage2D
#define SET_CONTEXT_CALLBACK_REPLACEMENTS
//#include "glfuncs.inl"
#undef SET_CONTEXT_CALLBACK_REPLACEMENTS

	return sccb(s);
}

// Export: DrvSetContext 
int WINAPI _DrvSetContext (HDC hdc, HGLRC hglrc, SetContextCallBack callback)
{
	sccb = callback;
//	int ret = CALL_IMP(DrvSetContext ,(hdc,hglrc,SetContextCallBackReplacement));
	int ret = CALL_IMP(DrvSetContext ,(hdc,hglrc,callback));
	return ret;
}

