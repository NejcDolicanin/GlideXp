//
// glfuncs.inl
//
// What is this file?
//
// This file lists all of the functions that need to be imported and exported
//
// When used with macros, it can automate lots of annoying crap
//


//
// Define Function Pointers and Exports
//
#ifdef DEFINE_FUNCS

#define PT_FUNC(name)  \
	PFN##name dll_##name = 0; \
	__declspec(naked) DWORD APIENTRY _##name (void) \
	{												\
		/*OutputDebugString(#name "\n");*/			\
		__asm { __asm jmp dll_##name };				\
	}

#define PT_FUNC_EX(name,ret,args)  \
	PFN##name dll_##name = 0; \
	__declspec(naked) ret APIENTRY _##name args \
	{												\
		/*OutputDebugString(#name "\n");*/			\
		__asm { __asm jmp dll_##name };				\
	}

#define OVERLOADED_FUNC(name,ret,args)  \
	PFN##name dll_##name = 0;

#define GL_OVERLOADED_FUNC(name,ret,args)  \
	PFN##name dll_##name = 0; \
	__declspec(naked) ret APIENTRY _##name args		\
	{												\
		/*OutputDebugString(#name "\n");*/			\
		__asm { __asm jmp dll_##name };				\
	}												\
	__declspec(naked) ret APIENTRY _##name##_0 args	\
	{												\
		/*OutputDebugString(#name "\n");*/			\
		table_num = 0;								\
		__asm { __asm jmp Overload_##name };		\
	}												\
	__declspec(naked) ret APIENTRY _##name##_1 args	\
	{												\
		/*OutputDebugString(#name "\n");*/			\
		table_num = 1;								\
		__asm { __asm jmp Overload_##name };		\
	}

//
// Genterate the Function import code
//
#elif defined(IMPORT_FUNCS)		

#define PT_FUNC(name)	\
	( dll_##name ) = (PFN##name) GetProcAddress(hImported,#name );	\
	if (!( dll_##name ))	\
	{	\
		/*MessageBox(NULL,"Error loading symbol \"" #name "\" from 3dfxogl.dll", "Error in GlideXP OGL", MB_OK|MB_ICONSTOP);*/		\
		return false;	\
	}

#define PT_FUNC_EX(name,ret,args)	\
	( dll_##name ) = (PFN##name) GetProcAddress(hImported,#name );	\
	if (!( dll_##name ))	\
	{	\
		/*MessageBox(NULL,"Error loading symbol \"" #name "\" from 3dfxogl.dll", "Error in GlideXP OGL", MB_OK|MB_ICONSTOP);*/		\
		return false;	\
	}

#define OVERLOADED_FUNC(name,ret,args)	\
	( dll_##name ) = (PFN##name) GetProcAddress(hImported,#name );	\
	if (!( dll_##name ))	\
	{	\
		/*MessageBox(NULL,"Error loading symbol \"" #name "\" from 3dfxogl.dll", "Error in GlideXP OGL", MB_OK|MB_ICONSTOP);*/		\
		return false;	\
	}

#define GL_OVERLOADED_FUNC(name,ret,args)	\
	( dll_##name ) = (PFN##name) GetProcAddress(hImported,#name );	\
	if (!( dll_##name ))	\
	{	\
		/*MessageBox(NULL,"Error loading symbol \"" #name "\" from 3dfxogl.dll", "Error in GlideXP OGL", MB_OK|MB_ICONSTOP);*/		\
		return false;	\
	}

//
// Generate the DEF File
//
#elif defined(GENERATE_DEF)	
EXPORTS
    	
#define PT_FUNC(name) name = _##name
#define PT_FUNC_EX(name,ret,args) name = _##name
#define OVERLOADED_FUNC(name,ret,args) name = _##name
#define GL_OVERLOADED_FUNC(name,ret,args) name = _##name

//
// Create the proc_address_overrides array entires
//
#elif defined(GET_PROC_ADDR_OVERRIDES)

#define PT_FUNC(name) 

#define PT_FUNC_EX(name,ret,args)

#define OVERLOADED_FUNC(name,ret,args) {  #name, (PROC) _##name },

#define GL_OVERLOADED_FUNC(name,ret,args) 
//{  #name, (PROC) _##name },

//
// Create the proc_address_overrides array entires
//
#elif defined(SET_CONTEXT_CALLBACK_REPLACEMENTS)

#define PT_FUNC(name) 

#define PT_FUNC_EX(name,ret,args)

#define OVERLOADED_FUNC(name,ret,args)

#define GL_OVERLOADED_FUNC(name,ret,args)	\
	if (_##name != s->##name)				\
	{										\
		dll_##name = s->##name;				\
		s->##name = _##name;				\
	};

//
// Create the proc_address_overrides array entires
//
#elif defined(ICD_TABLE_OVERLOAD)

#define PT_FUNC(name) 

#define PT_FUNC_EX(name,ret,args)

#define OVERLOADED_FUNC(name,ret,args)

#define GL_OVERLOADED_FUNC(name,ret,args)	\
	table[0]->##name = _##name##_0;			\
	table[1]->##name = _##name##_1;

//
// Just create Declarations of the pointers and exports
//
#else

#define PT_FUNC(name) \
	typedef void (APIENTRY * PFN##name) (void); \
	extern PFN##name dll_##name; \
	extern "C" DWORD APIENTRY _##name (void);

#define PT_FUNC_EX(name,ret,args) \
	typedef ret (APIENTRY * PFN##name) args; \
	extern PFN##name dll_##name; \
	extern "C" ret APIENTRY _##name args;

#define OVERLOADED_FUNC(name,ret,args) \
	typedef ret (APIENTRY * PFN##name) args; \
	extern PFN##name dll_##name; \
	extern "C" ret APIENTRY _##name args;

#define GL_OVERLOADED_FUNC(name,ret,args) \
	typedef ret (APIENTRY * PFN##name) args; \
	extern PFN##name dll_##name; \
	extern "C" ret APIENTRY _##name args; \
	extern "C" ret APIENTRY Overload_##name args; \
	extern "C" ret APIENTRY _##name##_0 args; \
	extern "C" ret APIENTRY _##name##_1 args;

#define CALL_IMP(name,args) dll_##name args

#endif


PT_FUNC(DrvReleaseContext)
PT_FUNC(DrvValidateVersion)

PT_FUNC_EX(DrvCopyContext, BOOL  ,(HGLRC, HGLRC, UINT))
PT_FUNC_EX(DrvCreateContext, HGLRC ,(HDC))
PT_FUNC_EX(DrvCreateLayerContext, HGLRC ,(HDC, int))
PT_FUNC_EX(DrvDeleteContext, BOOL  ,(HGLRC))
PT_FUNC_EX(DrvDescribeLayerPlane, BOOL  ,(HDC, int, int, UINT, LPLAYERPLANEDESCRIPTOR))
PT_FUNC_EX(DrvDescribePixelFormat, int  ,( IN HDC, IN int, IN UINT, OUT LPPIXELFORMATDESCRIPTOR))
PT_FUNC_EX(DrvGetLayerPaletteEntries, int   ,(HDC, int, int, int, COLORREF *))
PT_FUNC_EX(DrvRealizeLayerPalette, BOOL  ,(HDC, int, BOOL))
PT_FUNC_EX(DrvSetLayerPaletteEntries, int   ,(HDC, int, int, int, CONST COLORREF *))
PT_FUNC_EX(DrvSetPixelFormat, BOOL  ,(IN HDC, IN int, IN CONST PIXELFORMATDESCRIPTOR *))
PT_FUNC_EX(DrvShareLists, BOOL  ,(HGLRC, HGLRC))
PT_FUNC_EX(DrvSwapBuffers, BOOL  ,(HDC))
PT_FUNC_EX(DrvSwapLayerBuffers, BOOL  ,(HDC, UINT))

PT_FUNC_EX(glAccum , void ,(GLenum op, GLfloat value))
PT_FUNC_EX(glAlphaFunc , void ,(GLenum func, GLclampf ref))
PT_FUNC_EX(glAreTexturesResident , GLboolean ,(GLsizei n, const GLuint *textures, GLboolean *residences))
PT_FUNC_EX(glArrayElement , void ,(GLint i))
PT_FUNC_EX(glBegin , void ,(GLenum mode))
PT_FUNC_EX(glBindTexture , void ,(GLenum target, GLuint texture))
PT_FUNC_EX(glBitmap , void ,(GLsizei width, GLsizei height, GLfloat xorig, GLfloat yorig, GLfloat xmove, GLfloat ymove, const GLubyte *bitmap))
PT_FUNC_EX(glBlendFunc , void ,(GLenum sfactor, GLenum dfactor))
PT_FUNC_EX(glCallList , void ,(GLuint list))
PT_FUNC_EX(glCallLists , void ,(GLsizei n, GLenum type, const GLvoid *lists))
PT_FUNC_EX(glClear , void ,(GLbitfield mask))
PT_FUNC_EX(glClearAccum , void ,(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha))
PT_FUNC_EX(glClearColor , void ,(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha))
PT_FUNC_EX(glClearDepth , void ,(GLclampd depth))
PT_FUNC_EX(glClearIndex , void ,(GLfloat c))
PT_FUNC_EX(glClearStencil , void ,(GLint s))
PT_FUNC_EX(glClipPlane , void ,(GLenum plane, const GLdouble *equation))
PT_FUNC_EX(glColor3b , void ,(GLbyte red, GLbyte green, GLbyte blue))
PT_FUNC_EX(glColor3bv , void ,(const GLbyte *v))
PT_FUNC_EX(glColor3d , void ,(GLdouble red, GLdouble green, GLdouble blue))
PT_FUNC_EX(glColor3dv , void ,(const GLdouble *v))
PT_FUNC_EX(glColor3f , void ,(GLfloat red, GLfloat green, GLfloat blue))
PT_FUNC_EX(glColor3fv , void ,(const GLfloat *v))
PT_FUNC_EX(glColor3i , void ,(GLint red, GLint green, GLint blue))
PT_FUNC_EX(glColor3iv , void ,(const GLint *v))
PT_FUNC_EX(glColor3s , void ,(GLshort red, GLshort green, GLshort blue))
PT_FUNC_EX(glColor3sv , void ,(const GLshort *v))
PT_FUNC_EX(glColor3ub , void ,(GLubyte red, GLubyte green, GLubyte blue))
PT_FUNC_EX(glColor3ubv , void ,(const GLubyte *v))
PT_FUNC_EX(glColor3ui , void ,(GLuint red, GLuint green, GLuint blue))
PT_FUNC_EX(glColor3uiv , void ,(const GLuint *v))
PT_FUNC_EX(glColor3us , void ,(GLushort red, GLushort green, GLushort blue))
PT_FUNC_EX(glColor3usv , void ,(const GLushort *v))
PT_FUNC_EX(glColor4b , void ,(GLbyte red, GLbyte green, GLbyte blue, GLbyte alpha))
PT_FUNC_EX(glColor4bv , void ,(const GLbyte *v))
PT_FUNC_EX(glColor4d , void ,(GLdouble red, GLdouble green, GLdouble blue, GLdouble alpha))
PT_FUNC_EX(glColor4dv , void ,(const GLdouble *v))
PT_FUNC_EX(glColor4f , void ,(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha))
PT_FUNC_EX(glColor4fv , void ,(const GLfloat *v))
PT_FUNC_EX(glColor4i , void ,(GLint red, GLint green, GLint blue, GLint alpha))
PT_FUNC_EX(glColor4iv , void ,(const GLint *v))
PT_FUNC_EX(glColor4s , void ,(GLshort red, GLshort green, GLshort blue, GLshort alpha))
PT_FUNC_EX(glColor4sv , void ,(const GLshort *v))
PT_FUNC_EX(glColor4ub , void ,(GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha))
PT_FUNC_EX(glColor4ubv , void ,(const GLubyte *v))
PT_FUNC_EX(glColor4ui , void ,(GLuint red, GLuint green, GLuint blue, GLuint alpha))
PT_FUNC_EX(glColor4uiv , void ,(const GLuint *v))
PT_FUNC_EX(glColor4us , void ,(GLushort red, GLushort green, GLushort blue, GLushort alpha))
PT_FUNC_EX(glColor4usv , void ,(const GLushort *v))
PT_FUNC_EX(glColorMask , void ,(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha))
PT_FUNC_EX(glColorMaterial , void ,(GLenum face, GLenum mode))
PT_FUNC_EX(glColorPointer , void ,(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer))
PT_FUNC_EX(glCopyPixels , void ,(GLint x, GLint y, GLsizei width, GLsizei height, GLenum type))
PT_FUNC_EX(glCopyTexImage1D , void ,(GLenum target, GLint level, GLenum internalFormat, GLint x, GLint y, GLsizei width, GLint border))
PT_FUNC_EX(glCopyTexImage2D , void ,(GLenum target, GLint level, GLenum internalFormat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border))
PT_FUNC_EX(glCopyTexSubImage1D , void ,(GLenum target, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width))
PT_FUNC_EX(glCopyTexSubImage2D , void ,(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height))
PT_FUNC_EX(glCullFace , void ,(GLenum mode))
PT_FUNC_EX(glDeleteLists , void ,(GLuint list, GLsizei range))
PT_FUNC_EX(glDeleteTextures , void ,(GLsizei n, const GLuint *textures))
PT_FUNC_EX(glDepthFunc , void ,(GLenum func))
PT_FUNC_EX(glDepthMask , void ,(GLboolean flag))
PT_FUNC_EX(glDepthRange , void ,(GLclampd zNear, GLclampd zFar))
PT_FUNC_EX(glDisable , void ,(GLenum cap))
PT_FUNC_EX(glDisableClientState , void ,(GLenum array))
PT_FUNC_EX(glDrawArrays , void ,(GLenum mode, GLint first, GLsizei count))
PT_FUNC_EX(glDrawBuffer , void ,(GLenum mode))
PT_FUNC_EX(glDrawElements , void ,(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices))
PT_FUNC_EX(glDrawPixels , void ,(GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels))
PT_FUNC_EX(glEdgeFlag , void ,(GLboolean flag))
PT_FUNC_EX(glEdgeFlagPointer , void ,(GLsizei stride, const GLvoid *pointer))
PT_FUNC_EX(glEdgeFlagv , void ,(const GLboolean *flag))
PT_FUNC_EX(glEnable , void ,(GLenum cap))
PT_FUNC_EX(glEnableClientState , void ,(GLenum array))
PT_FUNC_EX(glEnd , void ,(void))
PT_FUNC_EX(glEndList , void ,(void))
PT_FUNC_EX(glEvalCoord1d , void ,(GLdouble u))
PT_FUNC_EX(glEvalCoord1dv , void ,(const GLdouble *u))
PT_FUNC_EX(glEvalCoord1f , void ,(GLfloat u))
PT_FUNC_EX(glEvalCoord1fv , void ,(const GLfloat *u))
PT_FUNC_EX(glEvalCoord2d , void ,(GLdouble u, GLdouble v))
PT_FUNC_EX(glEvalCoord2dv , void ,(const GLdouble *u))
PT_FUNC_EX(glEvalCoord2f , void ,(GLfloat u, GLfloat v))
PT_FUNC_EX(glEvalCoord2fv , void ,(const GLfloat *u))
PT_FUNC_EX(glEvalMesh1 , void ,(GLenum mode, GLint i1, GLint i2))
PT_FUNC_EX(glEvalMesh2 , void ,(GLenum mode, GLint i1, GLint i2, GLint j1, GLint j2))
PT_FUNC_EX(glEvalPoint1 , void ,(GLint i))
PT_FUNC_EX(glEvalPoint2 , void ,(GLint i, GLint j))
PT_FUNC_EX(glFeedbackBuffer , void ,(GLsizei size, GLenum type, GLfloat *buffer))
PT_FUNC_EX(glFinish , void ,(void))
PT_FUNC_EX(glFlush , void ,(void))
PT_FUNC_EX(glFogf , void ,(GLenum pname, GLfloat param))
PT_FUNC_EX(glFogfv , void ,(GLenum pname, const GLfloat *params))
PT_FUNC_EX(glFogi , void ,(GLenum pname, GLint param))
PT_FUNC_EX(glFogiv , void ,(GLenum pname, const GLint *params))
PT_FUNC_EX(glFrontFace , void ,(GLenum mode))
PT_FUNC_EX(glFrustum , void ,(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar))
PT_FUNC_EX(glGenLists , GLuint ,(GLsizei range))
PT_FUNC_EX(glGenTextures , void ,(GLsizei n, GLuint *textures))
PT_FUNC_EX(glGetBooleanv , void ,(GLenum pname, GLboolean *params))
PT_FUNC_EX(glGetClipPlane , void ,(GLenum plane, GLdouble *equation))
PT_FUNC_EX(glGetDoublev , void ,(GLenum pname, GLdouble *params))
PT_FUNC_EX(glGetError , GLenum ,(void))
PT_FUNC_EX(glGetFloatv , void ,(GLenum pname, GLfloat *params))
PT_FUNC_EX(glGetIntegerv , void ,(GLenum pname, GLint *params))
PT_FUNC_EX(glGetLightfv , void ,(GLenum light, GLenum pname, GLfloat *params))
PT_FUNC_EX(glGetLightiv , void ,(GLenum light, GLenum pname, GLint *params))
PT_FUNC_EX(glGetMapdv , void ,(GLenum target, GLenum query, GLdouble *v))
PT_FUNC_EX(glGetMapfv , void ,(GLenum target, GLenum query, GLfloat *v))
PT_FUNC_EX(glGetMapiv , void ,(GLenum target, GLenum query, GLint *v))
PT_FUNC_EX(glGetMaterialfv , void ,(GLenum face, GLenum pname, GLfloat *params))
PT_FUNC_EX(glGetMaterialiv , void ,(GLenum face, GLenum pname, GLint *params))
PT_FUNC_EX(glGetPixelMapfv , void ,(GLenum map, GLfloat *values))
PT_FUNC_EX(glGetPixelMapuiv , void ,(GLenum map, GLuint *values))
PT_FUNC_EX(glGetPixelMapusv , void ,(GLenum map, GLushort *values))
PT_FUNC_EX(glGetPointerv , void ,(GLenum pname, GLvoid* *params))
PT_FUNC_EX(glGetPolygonStipple , void ,(GLubyte *mask))
PT_FUNC_EX(glGetTexEnvfv , void ,(GLenum target, GLenum pname, GLfloat *params))
PT_FUNC_EX(glGetTexEnviv , void ,(GLenum target, GLenum pname, GLint *params))
PT_FUNC_EX(glGetTexGendv , void ,(GLenum coord, GLenum pname, GLdouble *params))
PT_FUNC_EX(glGetTexGenfv , void ,(GLenum coord, GLenum pname, GLfloat *params))
PT_FUNC_EX(glGetTexGeniv , void ,(GLenum coord, GLenum pname, GLint *params))
PT_FUNC_EX(glGetTexImage , void ,(GLenum target, GLint level, GLenum format, GLenum type, GLvoid *pixels))
PT_FUNC_EX(glGetTexLevelParameterfv , void ,(GLenum target, GLint level, GLenum pname, GLfloat *params))
PT_FUNC_EX(glGetTexLevelParameteriv , void ,(GLenum target, GLint level, GLenum pname, GLint *params))
PT_FUNC_EX(glGetTexParameterfv , void ,(GLenum target, GLenum pname, GLfloat *params))
PT_FUNC_EX(glGetTexParameteriv , void ,(GLenum target, GLenum pname, GLint *params))
PT_FUNC_EX(glHint , void ,(GLenum target, GLenum mode))
PT_FUNC_EX(glIndexMask , void ,(GLuint mask))
PT_FUNC_EX(glIndexPointer , void ,(GLenum type, GLsizei stride, const GLvoid *pointer))
PT_FUNC_EX(glIndexd , void ,(GLdouble c))
PT_FUNC_EX(glIndexdv , void ,(const GLdouble *c))
PT_FUNC_EX(glIndexf , void ,(GLfloat c))
PT_FUNC_EX(glIndexfv , void ,(const GLfloat *c))
PT_FUNC_EX(glIndexi , void ,(GLint c))
PT_FUNC_EX(glIndexiv , void ,(const GLint *c))
PT_FUNC_EX(glIndexs , void ,(GLshort c))
PT_FUNC_EX(glIndexsv , void ,(const GLshort *c))
PT_FUNC_EX(glIndexub , void ,(GLubyte c))
PT_FUNC_EX(glIndexubv , void ,(const GLubyte *c))
PT_FUNC_EX(glInitNames , void ,(void))
PT_FUNC_EX(glInterleavedArrays , void ,(GLenum format, GLsizei stride, const GLvoid *pointer))
PT_FUNC_EX(glIsEnabled , GLboolean ,(GLenum cap))
PT_FUNC_EX(glIsList , GLboolean ,(GLuint list))
PT_FUNC_EX(glIsTexture , GLboolean ,(GLuint texture))
PT_FUNC_EX(glLightModelf , void ,(GLenum pname, GLfloat param))
PT_FUNC_EX(glLightModelfv , void ,(GLenum pname, const GLfloat *params))
PT_FUNC_EX(glLightModeli , void ,(GLenum pname, GLint param))
PT_FUNC_EX(glLightModeliv , void ,(GLenum pname, const GLint *params))
PT_FUNC_EX(glLightf , void ,(GLenum light, GLenum pname, GLfloat param))
PT_FUNC_EX(glLightfv , void ,(GLenum light, GLenum pname, const GLfloat *params))
PT_FUNC_EX(glLighti , void ,(GLenum light, GLenum pname, GLint param))
PT_FUNC_EX(glLightiv , void ,(GLenum light, GLenum pname, const GLint *params))
PT_FUNC_EX(glLineStipple , void ,(GLint factor, GLushort pattern))
PT_FUNC_EX(glLineWidth , void ,(GLfloat width))
PT_FUNC_EX(glListBase , void ,(GLuint base))
PT_FUNC_EX(glLoadIdentity , void ,(void))
PT_FUNC_EX(glLoadMatrixd , void ,(const GLdouble *m))
PT_FUNC_EX(glLoadMatrixf , void ,(const GLfloat *m))
PT_FUNC_EX(glLoadName , void ,(GLuint name))
PT_FUNC_EX(glLogicOp , void ,(GLenum opcode))
PT_FUNC_EX(glMap1d , void ,(GLenum target, GLdouble u1, GLdouble u2, GLint stride, GLint order, const GLdouble *points))
PT_FUNC_EX(glMap1f , void ,(GLenum target, GLfloat u1, GLfloat u2, GLint stride, GLint order, const GLfloat *points))
PT_FUNC_EX(glMap2d , void ,(GLenum target, GLdouble u1, GLdouble u2, GLint ustride, GLint uorder, GLdouble v1, GLdouble v2, GLint vstride, GLint vorder, const GLdouble *points))
PT_FUNC_EX(glMap2f , void ,(GLenum target, GLfloat u1, GLfloat u2, GLint ustride, GLint uorder, GLfloat v1, GLfloat v2, GLint vstride, GLint vorder, const GLfloat *points))
PT_FUNC_EX(glMapGrid1d , void ,(GLint un, GLdouble u1, GLdouble u2))
PT_FUNC_EX(glMapGrid1f , void ,(GLint un, GLfloat u1, GLfloat u2))
PT_FUNC_EX(glMapGrid2d , void ,(GLint un, GLdouble u1, GLdouble u2, GLint vn, GLdouble v1, GLdouble v2))
PT_FUNC_EX(glMapGrid2f , void ,(GLint un, GLfloat u1, GLfloat u2, GLint vn, GLfloat v1, GLfloat v2))
PT_FUNC_EX(glMaterialf , void ,(GLenum face, GLenum pname, GLfloat param))
PT_FUNC_EX(glMaterialfv , void ,(GLenum face, GLenum pname, const GLfloat *params))
PT_FUNC_EX(glMateriali , void ,(GLenum face, GLenum pname, GLint param))
PT_FUNC_EX(glMaterialiv , void ,(GLenum face, GLenum pname, const GLint *params))
PT_FUNC_EX(glMatrixMode , void ,(GLenum mode))
PT_FUNC_EX(glMultMatrixd , void ,(const GLdouble *m))
PT_FUNC_EX(glMultMatrixf , void ,(const GLfloat *m))
PT_FUNC_EX(glNewList , void ,(GLuint list, GLenum mode))
PT_FUNC_EX(glNormal3b , void ,(GLbyte nx, GLbyte ny, GLbyte nz))
PT_FUNC_EX(glNormal3bv , void ,(const GLbyte *v))
PT_FUNC_EX(glNormal3d , void ,(GLdouble nx, GLdouble ny, GLdouble nz))
PT_FUNC_EX(glNormal3dv , void ,(const GLdouble *v))
PT_FUNC_EX(glNormal3f , void ,(GLfloat nx, GLfloat ny, GLfloat nz))
PT_FUNC_EX(glNormal3fv , void ,(const GLfloat *v))
PT_FUNC_EX(glNormal3i , void ,(GLint nx, GLint ny, GLint nz))
PT_FUNC_EX(glNormal3iv , void ,(const GLint *v))
PT_FUNC_EX(glNormal3s , void ,(GLshort nx, GLshort ny, GLshort nz))
PT_FUNC_EX(glNormal3sv , void ,(const GLshort *v))
PT_FUNC_EX(glNormalPointer , void ,(GLenum type, GLsizei stride, const GLvoid *pointer))
PT_FUNC_EX(glOrtho , void ,(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar))
PT_FUNC_EX(glPassThrough , void ,(GLfloat token))
PT_FUNC_EX(glPixelMapfv , void ,(GLenum map, GLsizei mapsize, const GLfloat *values))
PT_FUNC_EX(glPixelMapuiv , void ,(GLenum map, GLsizei mapsize, const GLuint *values))
PT_FUNC_EX(glPixelMapusv , void ,(GLenum map, GLsizei mapsize, const GLushort *values))
PT_FUNC_EX(glPixelStoref , void ,(GLenum pname, GLfloat param))
PT_FUNC_EX(glPixelStorei , void ,(GLenum pname, GLint param))
PT_FUNC_EX(glPixelTransferf , void ,(GLenum pname, GLfloat param))
PT_FUNC_EX(glPixelTransferi , void ,(GLenum pname, GLint param))
PT_FUNC_EX(glPixelZoom , void ,(GLfloat xfactor, GLfloat yfactor))
PT_FUNC_EX(glPointSize , void ,(GLfloat size))
PT_FUNC_EX(glPolygonMode , void ,(GLenum face, GLenum mode))
PT_FUNC_EX(glPolygonOffset , void ,(GLfloat factor, GLfloat units))
PT_FUNC_EX(glPolygonStipple , void ,(const GLubyte *mask))
PT_FUNC_EX(glPopAttrib , void ,(void))
PT_FUNC_EX(glPopClientAttrib , void ,(void))
PT_FUNC_EX(glPopMatrix , void ,(void))
PT_FUNC_EX(glPopName , void ,(void))
PT_FUNC_EX(glPrioritizeTextures , void ,(GLsizei n, const GLuint *textures, const GLclampf *priorities))
PT_FUNC_EX(glPushAttrib , void ,(GLbitfield mask))
PT_FUNC_EX(glPushClientAttrib , void ,(GLbitfield mask))
PT_FUNC_EX(glPushMatrix , void ,(void))
PT_FUNC_EX(glPushName , void ,(GLuint name))
PT_FUNC_EX(glRasterPos2d , void ,(GLdouble x, GLdouble y))
PT_FUNC_EX(glRasterPos2dv , void ,(const GLdouble *v))
PT_FUNC_EX(glRasterPos2f , void ,(GLfloat x, GLfloat y))
PT_FUNC_EX(glRasterPos2fv , void ,(const GLfloat *v))
PT_FUNC_EX(glRasterPos2i , void ,(GLint x, GLint y))
PT_FUNC_EX(glRasterPos2iv , void ,(const GLint *v))
PT_FUNC_EX(glRasterPos2s , void ,(GLshort x, GLshort y))
PT_FUNC_EX(glRasterPos2sv , void ,(const GLshort *v))
PT_FUNC_EX(glRasterPos3d , void ,(GLdouble x, GLdouble y, GLdouble z))
PT_FUNC_EX(glRasterPos3dv , void ,(const GLdouble *v))
PT_FUNC_EX(glRasterPos3f , void ,(GLfloat x, GLfloat y, GLfloat z))
PT_FUNC_EX(glRasterPos3fv , void ,(const GLfloat *v))
PT_FUNC_EX(glRasterPos3i , void ,(GLint x, GLint y, GLint z))
PT_FUNC_EX(glRasterPos3iv , void ,(const GLint *v))
PT_FUNC_EX(glRasterPos3s , void ,(GLshort x, GLshort y, GLshort z))
PT_FUNC_EX(glRasterPos3sv , void ,(const GLshort *v))
PT_FUNC_EX(glRasterPos4d , void ,(GLdouble x, GLdouble y, GLdouble z, GLdouble w))
PT_FUNC_EX(glRasterPos4dv , void ,(const GLdouble *v))
PT_FUNC_EX(glRasterPos4f , void ,(GLfloat x, GLfloat y, GLfloat z, GLfloat w))
PT_FUNC_EX(glRasterPos4fv , void ,(const GLfloat *v))
PT_FUNC_EX(glRasterPos4i , void ,(GLint x, GLint y, GLint z, GLint w))
PT_FUNC_EX(glRasterPos4iv , void ,(const GLint *v))
PT_FUNC_EX(glRasterPos4s , void ,(GLshort x, GLshort y, GLshort z, GLshort w))
PT_FUNC_EX(glRasterPos4sv , void ,(const GLshort *v))
PT_FUNC_EX(glReadBuffer , void ,(GLenum mode))
PT_FUNC_EX(glReadPixels , void ,(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *pixels))
PT_FUNC_EX(glRectd , void ,(GLdouble x1, GLdouble y1, GLdouble x2, GLdouble y2))
PT_FUNC_EX(glRectdv , void ,(const GLdouble *v1, const GLdouble *v2))
PT_FUNC_EX(glRectf , void ,(GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2))
PT_FUNC_EX(glRectfv , void ,(const GLfloat *v1, const GLfloat *v2))
PT_FUNC_EX(glRecti , void ,(GLint x1, GLint y1, GLint x2, GLint y2))
PT_FUNC_EX(glRectiv , void ,(const GLint *v1, const GLint *v2))
PT_FUNC_EX(glRects , void ,(GLshort x1, GLshort y1, GLshort x2, GLshort y2))
PT_FUNC_EX(glRectsv , void ,(const GLshort *v1, const GLshort *v2))
PT_FUNC_EX(glRenderMode , GLint ,(GLenum mode))
PT_FUNC_EX(glRotated , void ,(GLdouble angle, GLdouble x, GLdouble y, GLdouble z))
PT_FUNC_EX(glRotatef , void ,(GLfloat angle, GLfloat x, GLfloat y, GLfloat z))
PT_FUNC_EX(glScaled , void ,(GLdouble x, GLdouble y, GLdouble z))
PT_FUNC_EX(glScalef , void ,(GLfloat x, GLfloat y, GLfloat z))
PT_FUNC_EX(glScissor , void ,(GLint x, GLint y, GLsizei width, GLsizei height))
PT_FUNC_EX(glSelectBuffer , void ,(GLsizei size, GLuint *buffer))
PT_FUNC_EX(glShadeModel , void ,(GLenum mode))
PT_FUNC_EX(glStencilFunc , void ,(GLenum func, GLint ref, GLuint mask))
PT_FUNC_EX(glStencilMask , void ,(GLuint mask))
PT_FUNC_EX(glStencilOp , void ,(GLenum fail, GLenum zfail, GLenum zpass))
PT_FUNC_EX(glTexCoord1d , void ,(GLdouble s))
PT_FUNC_EX(glTexCoord1dv , void ,(const GLdouble *v))
PT_FUNC_EX(glTexCoord1f , void ,(GLfloat s))
PT_FUNC_EX(glTexCoord1fv , void ,(const GLfloat *v))
PT_FUNC_EX(glTexCoord1i , void ,(GLint s))
PT_FUNC_EX(glTexCoord1iv , void ,(const GLint *v))
PT_FUNC_EX(glTexCoord1s , void ,(GLshort s))
PT_FUNC_EX(glTexCoord1sv , void ,(const GLshort *v))
PT_FUNC_EX(glTexCoord2d , void ,(GLdouble s, GLdouble t))
PT_FUNC_EX(glTexCoord2dv , void ,(const GLdouble *v))
PT_FUNC_EX(glTexCoord2f , void ,(GLfloat s, GLfloat t))
PT_FUNC_EX(glTexCoord2fv , void ,(const GLfloat *v))
PT_FUNC_EX(glTexCoord2i , void ,(GLint s, GLint t))
PT_FUNC_EX(glTexCoord2iv , void ,(const GLint *v))
PT_FUNC_EX(glTexCoord2s , void ,(GLshort s, GLshort t))
PT_FUNC_EX(glTexCoord2sv , void ,(const GLshort *v))
PT_FUNC_EX(glTexCoord3d , void ,(GLdouble s, GLdouble t, GLdouble r))
PT_FUNC_EX(glTexCoord3dv , void ,(const GLdouble *v))
PT_FUNC_EX(glTexCoord3f , void ,(GLfloat s, GLfloat t, GLfloat r))
PT_FUNC_EX(glTexCoord3fv , void ,(const GLfloat *v))
PT_FUNC_EX(glTexCoord3i , void ,(GLint s, GLint t, GLint r))
PT_FUNC_EX(glTexCoord3iv , void ,(const GLint *v))
PT_FUNC_EX(glTexCoord3s , void ,(GLshort s, GLshort t, GLshort r))
PT_FUNC_EX(glTexCoord3sv , void ,(const GLshort *v))
PT_FUNC_EX(glTexCoord4d , void ,(GLdouble s, GLdouble t, GLdouble r, GLdouble q))
PT_FUNC_EX(glTexCoord4dv , void ,(const GLdouble *v))
PT_FUNC_EX(glTexCoord4f , void ,(GLfloat s, GLfloat t, GLfloat r, GLfloat q))
PT_FUNC_EX(glTexCoord4fv , void ,(const GLfloat *v))
PT_FUNC_EX(glTexCoord4i , void ,(GLint s, GLint t, GLint r, GLint q))
PT_FUNC_EX(glTexCoord4iv , void ,(const GLint *v))
PT_FUNC_EX(glTexCoord4s , void ,(GLshort s, GLshort t, GLshort r, GLshort q))
PT_FUNC_EX(glTexCoord4sv , void ,(const GLshort *v))
PT_FUNC_EX(glTexCoordPointer , void ,(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer))
PT_FUNC_EX(glTexEnvf , void ,(GLenum target, GLenum pname, GLfloat param))
PT_FUNC_EX(glTexEnvfv , void ,(GLenum target, GLenum pname, const GLfloat *params))
PT_FUNC_EX(glTexEnvi , void ,(GLenum target, GLenum pname, GLint param))
PT_FUNC_EX(glTexEnviv , void ,(GLenum target, GLenum pname, const GLint *params))
PT_FUNC_EX(glTexGend , void ,(GLenum coord, GLenum pname, GLdouble param))
PT_FUNC_EX(glTexGendv , void ,(GLenum coord, GLenum pname, const GLdouble *params))
PT_FUNC_EX(glTexGenf , void ,(GLenum coord, GLenum pname, GLfloat param))
PT_FUNC_EX(glTexGenfv , void ,(GLenum coord, GLenum pname, const GLfloat *params))
PT_FUNC_EX(glTexGeni , void ,(GLenum coord, GLenum pname, GLint param))
PT_FUNC_EX(glTexGeniv , void ,(GLenum coord, GLenum pname, const GLint *params))
PT_FUNC_EX(glTexImage1D , void ,(GLenum target, GLint level, GLint internalformat, GLsizei width, GLint border, GLenum format, GLenum type, const GLvoid *pixels))
PT_FUNC_EX(glTexParameterf , void ,(GLenum target, GLenum pname, GLfloat param))
PT_FUNC_EX(glTexParameterfv , void ,(GLenum target, GLenum pname, const GLfloat *params))
PT_FUNC_EX(glTexParameteri , void ,(GLenum target, GLenum pname, GLint param))
PT_FUNC_EX(glTexParameteriv , void ,(GLenum target, GLenum pname, const GLint *params))
PT_FUNC_EX(glTexSubImage1D , void ,(GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const GLvoid *pixels))
PT_FUNC_EX(glTexSubImage2D , void ,(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels))
PT_FUNC_EX(glTranslated , void ,(GLdouble x, GLdouble y, GLdouble z))
PT_FUNC_EX(glTranslatef , void ,(GLfloat x, GLfloat y, GLfloat z))
PT_FUNC_EX(glVertex2d , void ,(GLdouble x, GLdouble y))
PT_FUNC_EX(glVertex2dv , void ,(const GLdouble *v))
PT_FUNC_EX(glVertex2f , void ,(GLfloat x, GLfloat y))
PT_FUNC_EX(glVertex2fv , void ,(const GLfloat *v))
PT_FUNC_EX(glVertex2i , void ,(GLint x, GLint y))
PT_FUNC_EX(glVertex2iv , void ,(const GLint *v))
PT_FUNC_EX(glVertex2s , void ,(GLshort x, GLshort y))
PT_FUNC_EX(glVertex2sv , void ,(const GLshort *v))
PT_FUNC_EX(glVertex3d , void ,(GLdouble x, GLdouble y, GLdouble z))
PT_FUNC_EX(glVertex3dv , void ,(const GLdouble *v))
PT_FUNC_EX(glVertex3f , void ,(GLfloat x, GLfloat y, GLfloat z))
PT_FUNC_EX(glVertex3fv , void ,(const GLfloat *v))
PT_FUNC_EX(glVertex3i , void ,(GLint x, GLint y, GLint z))
PT_FUNC_EX(glVertex3iv , void ,(const GLint *v))
PT_FUNC_EX(glVertex3s , void ,(GLshort x, GLshort y, GLshort z))
PT_FUNC_EX(glVertex3sv , void ,(const GLshort *v))
PT_FUNC_EX(glVertex4d , void ,(GLdouble x, GLdouble y, GLdouble z, GLdouble w))
PT_FUNC_EX(glVertex4dv , void ,(const GLdouble *v))
PT_FUNC_EX(glVertex4f , void ,(GLfloat x, GLfloat y, GLfloat z, GLfloat w))
PT_FUNC_EX(glVertex4fv , void ,(const GLfloat *v))
PT_FUNC_EX(glVertex4i , void ,(GLint x, GLint y, GLint z, GLint w))
PT_FUNC_EX(glVertex4iv , void ,(const GLint *v))
PT_FUNC_EX(glVertex4s , void ,(GLshort x, GLshort y, GLshort z, GLshort w))
PT_FUNC_EX(glVertex4sv , void ,(const GLshort *v))
PT_FUNC_EX(glVertexPointer , void ,(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer))
PT_FUNC_EX(glViewport , void ,(GLint x, GLint y, GLsizei width, GLsizei height))

PT_FUNC_EX(wglChoosePixelFormat, int   ,( IN HDC, IN CONST PIXELFORMATDESCRIPTOR *))
PT_FUNC_EX(wglCopyContext, BOOL  ,(HGLRC, HGLRC, UINT))
PT_FUNC_EX(wglCreateContext, HGLRC ,(HDC))
PT_FUNC_EX(wglCreateLayerContext, HGLRC ,(HDC, int))
PT_FUNC_EX(wglDeleteContext, BOOL  ,(HGLRC))
PT_FUNC_EX(wglDescribeLayerPlane, BOOL  ,(HDC, int, int, UINT, LPLAYERPLANEDESCRIPTOR))
PT_FUNC_EX(wglDescribePixelFormat, int  ,( IN HDC, IN int, IN UINT, OUT LPPIXELFORMATDESCRIPTOR))
PT_FUNC_EX(wglGetCurrentContext, HGLRC ,(VOID))
PT_FUNC_EX(wglGetCurrentDC, HDC   ,(VOID))
PT_FUNC(wglGetDefaultProcAddress)
PT_FUNC_EX(wglGetLayerPaletteEntries, int   ,(HDC, int, int, int, COLORREF *))
PT_FUNC_EX(wglGetPixelFormat, int   ,( IN HDC))
PT_FUNC_EX(wglMakeCurrent, BOOL  ,(HDC, HGLRC))
PT_FUNC_EX(wglRealizeLayerPalette, BOOL  ,(HDC, int, BOOL))
PT_FUNC_EX(wglSetLayerPaletteEntries, int   ,(HDC, int, int, int, CONST COLORREF *))
PT_FUNC_EX(wglSetPixelFormat, BOOL  ,(IN HDC, IN int, IN CONST PIXELFORMATDESCRIPTOR *))
PT_FUNC_EX(wglShareLists, BOOL  ,(HGLRC, HGLRC))
PT_FUNC_EX(wglSwapBuffers, BOOL  ,(HDC))
PT_FUNC_EX(wglSwapLayerBuffers, BOOL  ,(HDC, UINT))
PT_FUNC_EX(wglUseFontBitmapsA, BOOL  ,(HDC, DWORD, DWORD, DWORD))
PT_FUNC_EX(wglUseFontBitmapsW, BOOL  ,(HDC, DWORD, DWORD, DWORD))
PT_FUNC_EX(wglUseFontOutlinesA, BOOL  ,(HDC, DWORD, DWORD, DWORD, FLOAT, FLOAT, int, LPGLYPHMETRICSFLOAT))
PT_FUNC_EX(wglUseFontOutlinesW, BOOL  ,(HDC, DWORD, DWORD, DWORD, FLOAT, FLOAT, int, LPGLYPHMETRICSFLOAT))

//
// Overloaded Functions
//

OVERLOADED_FUNC(DrvGetProcAddress,PROC,(LPCSTR))
OVERLOADED_FUNC(DrvSetContext, int, (HDC hdc, HGLRC hglrc, SetContextCallBack callback))

GL_OVERLOADED_FUNC(glGetString, const GLubyte * ,  (GLenum name))
GL_OVERLOADED_FUNC(glTexImage2D,void,(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels))

OVERLOADED_FUNC(wglGetProcAddress,PROC,(LPCSTR))

#undef PT_FUNC
#undef PT_FUNC_EX
#undef OVERLOADED_FUNC
#undef GL_OVERLOADED_FUNC
