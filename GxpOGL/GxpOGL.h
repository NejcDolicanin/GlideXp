//
// GxpOGL.h
//

#ifndef GXPOGL_H_INCLUDED
#define GXPOGL_H_INCLUDED

#include "../gxpver.h"

extern HINSTANCE	hImported;	// Imported DLL
extern HINSTANCE	hThis;		// This DLL

// Forward Declare this because we can't define it without including glfunc.inl first,
// but that needs this
struct GLFunctionStruct;


// Callback for the DrvSetContext function
typedef DWORD (CALLBACK * SetContextCallBack) (GLFunctionStruct *s);


//
// This will declare all the imported and exported funcs
//
#include "glfuncs.inl"

// Helper Macro
#define FUNC_POINTER(name) PFN##name name

//
// Structure passed to SetContextCallBack
//
struct GLFunctionStruct
{
	DWORD		num_funcs;		// Should be 336 (0x150) from the 3dfx ICD

	// Pointers to GL functions passed from the ICD to the 
	union {
		struct
		{
			FUNC_POINTER(glNewList);				//0
			FUNC_POINTER(glEndList);				//1
			FUNC_POINTER(glCallList);				//2
			FUNC_POINTER(glCallLists);				//3
			FUNC_POINTER(glDeleteLists);			//4
			FUNC_POINTER(glGenLists);				//5
			FUNC_POINTER(glListBase);				//6
			FUNC_POINTER(glBegin);					//7
			FUNC_POINTER(glBitmap);					//8
			FUNC_POINTER(glColor3b);				//9
			FUNC_POINTER(glColor3bv);				//10
			FUNC_POINTER(glColor3d);				//11
			FUNC_POINTER(glColor3dv);				//12
			FUNC_POINTER(glColor3f);				//13
			FUNC_POINTER(glColor3fv);				//14
			FUNC_POINTER(glColor3i);				//15
			FUNC_POINTER(glColor3iv);				//16
			FUNC_POINTER(glColor3s);				//17
			FUNC_POINTER(glColor3sv);				//18
			FUNC_POINTER(glColor3ub);				//19
			FUNC_POINTER(glColor3ubv);				//20
			FUNC_POINTER(glColor3ui);				//21
			FUNC_POINTER(glColor3uiv);				//22
			FUNC_POINTER(glColor3us);				//23
			FUNC_POINTER(glColor3usv);				//24
			FUNC_POINTER(glColor4b);				//25
			FUNC_POINTER(glColor4bv);				//26
			FUNC_POINTER(glColor4d);				//27
			FUNC_POINTER(glColor4dv);				//28
			FUNC_POINTER(glColor4f);				//29
			FUNC_POINTER(glColor4fv);				//30
			FUNC_POINTER(glColor4i);				//31
			FUNC_POINTER(glColor4iv);				//32
			FUNC_POINTER(glColor4s);				//33
			FUNC_POINTER(glColor4sv);				//34
			FUNC_POINTER(glColor4ub);				//35
			FUNC_POINTER(glColor4ubv);				//36
			FUNC_POINTER(glColor4ui);				//37
			FUNC_POINTER(glColor4uiv);				//38
			FUNC_POINTER(glColor4us);				//39
			FUNC_POINTER(glColor4usv);				//40
			FUNC_POINTER(glEdgeFlag);				//41
			FUNC_POINTER(glEdgeFlagv);				//42
			FUNC_POINTER(glEnd);					//43
			FUNC_POINTER(glIndexd);					//44
			FUNC_POINTER(glIndexdv);				//45
			FUNC_POINTER(glIndexf);					//46
			FUNC_POINTER(glIndexfv);				//47
			FUNC_POINTER(glIndexi);					//48
			FUNC_POINTER(glIndexiv);				//49
			FUNC_POINTER(glIndexs);					//50
			FUNC_POINTER(glIndexsv);				//51
			FUNC_POINTER(glNormal3b);				//52
			FUNC_POINTER(glNormal3bv);				//53
			FUNC_POINTER(glNormal3d);				//54
			FUNC_POINTER(glNormal3dv);				//55
			FUNC_POINTER(glNormal3f);				//56
			FUNC_POINTER(glNormal3fv);				//57
			FUNC_POINTER(glNormal3i);				//58
			FUNC_POINTER(glNormal3iv);				//59
			FUNC_POINTER(glNormal3s);				//60
			FUNC_POINTER(glNormal3sv);				//61
			FUNC_POINTER(glRasterPos2d);			//62
			FUNC_POINTER(glRasterPos2dv);			//63
			FUNC_POINTER(glRasterPos2f);			//64
			FUNC_POINTER(glRasterPos2fv);			//65
			FUNC_POINTER(glRasterPos2i);			//66
			FUNC_POINTER(glRasterPos2iv);			//67
			FUNC_POINTER(glRasterPos2s);			//68
			FUNC_POINTER(glRasterPos2sv);			//69
			FUNC_POINTER(glRasterPos3d);			//70
			FUNC_POINTER(glRasterPos3dv);			//71
			FUNC_POINTER(glRasterPos3f);			//72
			FUNC_POINTER(glRasterPos3fv);			//73
			FUNC_POINTER(glRasterPos3i);			//74
			FUNC_POINTER(glRasterPos3iv);			//75
			FUNC_POINTER(glRasterPos3s);			//76
			FUNC_POINTER(glRasterPos3sv);			//77
			FUNC_POINTER(glRasterPos4d);			//78
			FUNC_POINTER(glRasterPos4dv);			//79
			FUNC_POINTER(glRasterPos4f);			//80
			FUNC_POINTER(glRasterPos4fv);			//81
			FUNC_POINTER(glRasterPos4i);			//82
			FUNC_POINTER(glRasterPos4iv);			//83
			FUNC_POINTER(glRasterPos4s);			//84
			FUNC_POINTER(glRasterPos4sv);			//85
			FUNC_POINTER(glRectd);					//86
			FUNC_POINTER(glRectdv);					//87
			FUNC_POINTER(glRectf);					//88
			FUNC_POINTER(glRectfv);					//89
			FUNC_POINTER(glRecti);					//90
			FUNC_POINTER(glRectiv);					//91
			FUNC_POINTER(glRects);					//92
			FUNC_POINTER(glRectsv);					//93
			FUNC_POINTER(glTexCoord1d);				//94
			FUNC_POINTER(glTexCoord1dv);			//95
			FUNC_POINTER(glTexCoord1f);				//96
			FUNC_POINTER(glTexCoord1fv);			//97
			FUNC_POINTER(glTexCoord1i);				//98
			FUNC_POINTER(glTexCoord1iv);			//99
			FUNC_POINTER(glTexCoord1s);				//100
			FUNC_POINTER(glTexCoord1sv);			//101
			FUNC_POINTER(glTexCoord2d);				//102
			FUNC_POINTER(glTexCoord2dv);			//103
			FUNC_POINTER(glTexCoord2f);				//104
			FUNC_POINTER(glTexCoord2fv);			//105
			FUNC_POINTER(glTexCoord2i);				//106
			FUNC_POINTER(glTexCoord2iv);			//107
			FUNC_POINTER(glTexCoord2s);				//108
			FUNC_POINTER(glTexCoord2sv);			//109
			FUNC_POINTER(glTexCoord3d);				//110
			FUNC_POINTER(glTexCoord3dv);			//111
			FUNC_POINTER(glTexCoord3f);				//112
			FUNC_POINTER(glTexCoord3fv);			//113
			FUNC_POINTER(glTexCoord3i);				//114
			FUNC_POINTER(glTexCoord3iv);			//115
			FUNC_POINTER(glTexCoord3s);				//116
			FUNC_POINTER(glTexCoord3sv);			//117
			FUNC_POINTER(glTexCoord4d);				//118
			FUNC_POINTER(glTexCoord4dv);			//119
			FUNC_POINTER(glTexCoord4f);				//120
			FUNC_POINTER(glTexCoord4fv);			//121
			FUNC_POINTER(glTexCoord4i);				//122
			FUNC_POINTER(glTexCoord4iv);			//123
			FUNC_POINTER(glTexCoord4s);				//124
			FUNC_POINTER(glTexCoord4sv);			//125
			FUNC_POINTER(glVertex2d);				//126
			FUNC_POINTER(glVertex2dv);				//127
			FUNC_POINTER(glVertex2f);				//128
			FUNC_POINTER(glVertex2fv);				//129
			FUNC_POINTER(glVertex2i);				//130
			FUNC_POINTER(glVertex2iv);				//131
			FUNC_POINTER(glVertex2s);				//132
			FUNC_POINTER(glVertex2sv);				//133
			FUNC_POINTER(glVertex3d);				//134
			FUNC_POINTER(glVertex3dv);				//135
			FUNC_POINTER(glVertex3f);				//136
			FUNC_POINTER(glVertex3fv);				//137
			FUNC_POINTER(glVertex3i);				//138
			FUNC_POINTER(glVertex3iv);				//139
			FUNC_POINTER(glVertex3s);				//140
			FUNC_POINTER(glVertex3sv);				//141
			FUNC_POINTER(glVertex4d);				//142
			FUNC_POINTER(glVertex4dv);				//143
			FUNC_POINTER(glVertex4f);				//144
			FUNC_POINTER(glVertex4fv);				//145
			FUNC_POINTER(glVertex4i);				//146
			FUNC_POINTER(glVertex4iv);				//147
			FUNC_POINTER(glVertex4s);				//148
			FUNC_POINTER(glVertex4sv);				//149
			FUNC_POINTER(glClipPlane);				//150
			FUNC_POINTER(glColorMaterial);			//151
			FUNC_POINTER(glCullFace);				//152
			FUNC_POINTER(glFogf);					//153
			FUNC_POINTER(glFogfv);					//154
			FUNC_POINTER(glFogi);					//155
			FUNC_POINTER(glFogiv);					//156
			FUNC_POINTER(glFrontFace);				//157
			FUNC_POINTER(glHint);					//158
			FUNC_POINTER(glLightf);					//159
			FUNC_POINTER(glLightfv);				//160
			FUNC_POINTER(glLighti);					//161
			FUNC_POINTER(glLightiv);				//162
			FUNC_POINTER(glLightModelf);			//163
			FUNC_POINTER(glLightModelfv);			//164
			FUNC_POINTER(glLightModeli);			//165
			FUNC_POINTER(glLightModeliv);			//166
			FUNC_POINTER(glLineStipple);			//167
			FUNC_POINTER(glLineWidth);				//168
			FUNC_POINTER(glMaterialf);				//169
			FUNC_POINTER(glMaterialfv);				//170
			FUNC_POINTER(glMateriali);				//171
			FUNC_POINTER(glMaterialiv);				//172
			FUNC_POINTER(glPointSize);				//173
			FUNC_POINTER(glPolygonMode);			//174
			FUNC_POINTER(glPolygonStipple);			//175
			FUNC_POINTER(glScissor);				//176
			FUNC_POINTER(glShadeModel);				//177
			FUNC_POINTER(glTexParameterf);			//178
			FUNC_POINTER(glTexParameterfv);			//179
			FUNC_POINTER(glTexParameteri);			//180
			FUNC_POINTER(glTexParameteriv);			//181
			FUNC_POINTER(glTexImage1D);				//182
			FUNC_POINTER(glTexImage2D);				//183
			FUNC_POINTER(glTexEnvf);				//184
			FUNC_POINTER(glTexEnvfv);				//185
			FUNC_POINTER(glTexEnvi);				//186
			FUNC_POINTER(glTexEnviv);				//187
			FUNC_POINTER(glTexGend);				//188
			FUNC_POINTER(glTexGendv);				//189
			FUNC_POINTER(glTexGenf);				//190
			FUNC_POINTER(glTexGenfv);				//191
			FUNC_POINTER(glTexGeni);				//192
			FUNC_POINTER(glTexGeniv);				//193
			FUNC_POINTER(glFeedbackBuffer);			//194
			FUNC_POINTER(glSelectBuffer);			//195
			FUNC_POINTER(glRenderMode);				//196
			FUNC_POINTER(glInitNames);				//197
			FUNC_POINTER(glLoadName);				//198
			FUNC_POINTER(glPassThrough);			//199
			FUNC_POINTER(glPopName);				//200
			FUNC_POINTER(glPushName);				//201
			FUNC_POINTER(glDrawBuffer);				//202
			FUNC_POINTER(glClear);					//203
			FUNC_POINTER(glClearAccum);				//204
			FUNC_POINTER(glClearIndex);				//205
			FUNC_POINTER(glClearColor);				//206
			FUNC_POINTER(glClearStencil);			//207
			FUNC_POINTER(glClearDepth);				//208
			FUNC_POINTER(glStencilMask);			//209
			FUNC_POINTER(glColorMask);				//210
			FUNC_POINTER(glDepthMask);				//211
			FUNC_POINTER(glIndexMask);				//212
			FUNC_POINTER(glAccum);					//213
			FUNC_POINTER(glDisable);				//214
			FUNC_POINTER(glEnable);					//215
			FUNC_POINTER(glFinish);					//216
			FUNC_POINTER(glFlush);					//217
			FUNC_POINTER(glPopAttrib);				//218
			FUNC_POINTER(glPushAttrib);				//219
			FUNC_POINTER(glMap1d);					//220
			FUNC_POINTER(glMap1f);					//221
			FUNC_POINTER(glMap2d);					//222
			FUNC_POINTER(glMap2f);					//223
			FUNC_POINTER(glMapGrid1d);				//224
			FUNC_POINTER(glMapGrid1f);				//225
			FUNC_POINTER(glMapGrid2d);				//226
			FUNC_POINTER(glMapGrid2f);				//227
			FUNC_POINTER(glEvalCoord1d);			//228
			FUNC_POINTER(glEvalCoord1dv);			//229
			FUNC_POINTER(glEvalCoord1f);			//230
			FUNC_POINTER(glEvalCoord1fv);			//231
			FUNC_POINTER(glEvalCoord2d);			//232
			FUNC_POINTER(glEvalCoord2dv);			//233
			FUNC_POINTER(glEvalCoord2f);			//234
			FUNC_POINTER(glEvalCoord2fv);			//235
			FUNC_POINTER(glEvalMesh1);				//236
			FUNC_POINTER(glEvalPoint1);				//237
			FUNC_POINTER(glEvalMesh2);				//238
			FUNC_POINTER(glEvalPoint2);				//239
			FUNC_POINTER(glAlphaFunc);				//240
			FUNC_POINTER(glBlendFunc);				//241
			FUNC_POINTER(glLogicOp);				//242
			FUNC_POINTER(glStencilFunc);			//243
			FUNC_POINTER(glStencilOp);				//244
			FUNC_POINTER(glDepthFunc);				//245
			FUNC_POINTER(glPixelZoom);				//246
			FUNC_POINTER(glPixelTransferf);			//247
			FUNC_POINTER(glPixelTransferi);			//248
			FUNC_POINTER(glPixelStoref);			//249
			FUNC_POINTER(glPixelStorei);			//250
			FUNC_POINTER(glPixelMapfv);				//251
			FUNC_POINTER(glPixelMapuiv);			//252
			FUNC_POINTER(glPixelMapusv);			//253
			FUNC_POINTER(glReadBuffer);				//254
			FUNC_POINTER(glCopyPixels);				//255
			FUNC_POINTER(glReadPixels);				//256
			FUNC_POINTER(glDrawPixels);				//257
			FUNC_POINTER(glGetBooleanv);			//258
			FUNC_POINTER(glGetClipPlane);			//259
			FUNC_POINTER(glGetDoublev);				//260
			FUNC_POINTER(glGetError);				//261
			FUNC_POINTER(glGetFloatv);				//262
			FUNC_POINTER(glGetIntegerv);			//263
			FUNC_POINTER(glGetLightfv);				//264
			FUNC_POINTER(glGetLightiv);				//265
			FUNC_POINTER(glGetMapdv);				//266
			FUNC_POINTER(glGetMapfv);				//267
			FUNC_POINTER(glGetMapiv);				//268
			FUNC_POINTER(glGetMaterialfv);			//269
			FUNC_POINTER(glGetMaterialiv);			//270
			FUNC_POINTER(glGetPixelMapfv);			//271
			FUNC_POINTER(glGetPixelMapuiv);			//272
			FUNC_POINTER(glGetPixelMapusv);			//273
			FUNC_POINTER(glGetPolygonStipple);		//274
			FUNC_POINTER(glGetString);				//275
			FUNC_POINTER(glGetTexEnvfv);			//276
			FUNC_POINTER(glGetTexEnviv);			//277
			FUNC_POINTER(glGetTexGendv);			//278
			FUNC_POINTER(glGetTexGenfv);			//279
			FUNC_POINTER(glGetTexGeniv);			//280
			FUNC_POINTER(glGetTexImage);			//281
			FUNC_POINTER(glGetTexParameterfv);		//282
			FUNC_POINTER(glGetTexParameteriv);		//283
			FUNC_POINTER(glGetTexLevelParameterfv);	//284
			FUNC_POINTER(glGetTexLevelParameteriv);	//285
			FUNC_POINTER(glIsEnabled);				//286
			FUNC_POINTER(glIsList);					//287
			FUNC_POINTER(glDepthRange);				//288
			FUNC_POINTER(glFrustum);				//289
			FUNC_POINTER(glLoadIdentity);			//290
			FUNC_POINTER(glLoadMatrixf);			//291
			FUNC_POINTER(glLoadMatrixd);			//292
			FUNC_POINTER(glMatrixMode);				//293
			FUNC_POINTER(glMultMatrixf);			//294
			FUNC_POINTER(glMultMatrixd);			//295
			FUNC_POINTER(glOrtho);					//296
			FUNC_POINTER(glPopMatrix);				//297
			FUNC_POINTER(glPushMatrix);				//298
			FUNC_POINTER(glRotated);				//299
			FUNC_POINTER(glRotatef);				//300
			FUNC_POINTER(glScaled);					//301
			FUNC_POINTER(glScalef);					//302
			FUNC_POINTER(glTranslated);				//303
			FUNC_POINTER(glTranslatef);				//304
			FUNC_POINTER(glViewport);				//305
			FUNC_POINTER(glArrayElement);			//306
			FUNC_POINTER(glBindTexture);			//307
			FUNC_POINTER(glColorPointer);			//308
			FUNC_POINTER(glDisableClientState);		//309
			FUNC_POINTER(glDrawArrays);				//310
			FUNC_POINTER(glDrawElements);			//311
			FUNC_POINTER(glEdgeFlagPointer);		//312
			FUNC_POINTER(glEnableClientState);		//313
			FUNC_POINTER(glIndexPointer);			//314
			FUNC_POINTER(glIndexub);				//315
			FUNC_POINTER(glIndexubv);				//316
			FUNC_POINTER(glInterleavedArrays);		//317
			FUNC_POINTER(glNormalPointer);			//318
			FUNC_POINTER(glPolygonOffset);			//319
			FUNC_POINTER(glTexCoordPointer);		//320
			FUNC_POINTER(glVertexPointer);			//321
			FUNC_POINTER(glAreTexturesResident);	//322
			FUNC_POINTER(glCopyTexImage1D);			//323
			FUNC_POINTER(glCopyTexImage2D);			//324
			FUNC_POINTER(glCopyTexSubImage1D);		//325
			FUNC_POINTER(glCopyTexSubImage2D);		//326
			FUNC_POINTER(glDeleteTextures);			//327
			FUNC_POINTER(glGenTextures);			//328
			FUNC_POINTER(glGetPointerv);			//329
			FUNC_POINTER(glIsTexture);				//330
			FUNC_POINTER(glPrioritizeTextures);		//331
			FUNC_POINTER(glTexSubImage1D);			//332
			FUNC_POINTER(glTexSubImage2D);			//333
			FUNC_POINTER(glPopClientAttrib);		//334
			FUNC_POINTER(glPushClientAttrib);		//335

			// There are another 70 unknowns. glu???
			PROC	unknown[70];
		};

		// There are another 406 funcs total
		PROC	functions[406];
	};
};

// 2 sets of ICD funcs (real funcs)
extern int table_num;
extern GLFunctionStruct		icd_funcs[2];

// Undef the Helper Macro
#undef FUNC_POINTER

//
// Extensions
//

extern PFNGLCOMPRESSEDTEXIMAGE2DARBPROC imp_glCompressedTexImage2DARB;

void SetupExtensions();


// Used to Define an Overloaded function
#define GL_FUNCTION(name,ret,args) ret APIENTRY _##name args


#endif
