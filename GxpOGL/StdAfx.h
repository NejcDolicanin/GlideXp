// stdafx.h : include file for standard system include files,
//  or project specific include files that are used frequently, but
//      are changed infrequently
//

#if !defined(AFX_STDAFX_H__46F9E05F_E7DF_4E76_8ACC_6E21AD50538C__INCLUDED_)
#define AFX_STDAFX_H__46F9E05F_E7DF_4E76_8ACC_6E21AD50538C__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000


// Insert your headers here
#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers
#define _GDI32_

#include <windows.h>

// TODO: reference additional headers your program requires here
#include <gl/GL.h>

#include "glext.h"
#include "wglext.h"

#include <d3d8.h>
#include <d3dx8.h>
#include "d3dobject.h"
#include "d3ddevice.h"
#include "d3dsurf.h"

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_STDAFX_H__46F9E05F_E7DF_4E76_8ACC_6E21AD50538C__INCLUDED_)
