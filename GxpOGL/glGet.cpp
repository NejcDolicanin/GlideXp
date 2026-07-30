//
// glGet.cpp : glGet* function overloading
//

#include "stdafx.h"
#include "GxpOGL.h"

//
// Overloaded glGetString
//

const GLubyte * APIENTRY Overload_glGetString (GLenum name)
{
	const char * called_gl_string = (const char*) icd_funcs[table_num].glGetString(name);

	// Add " and Colourless Dragon" to the GL_VENDOR string
	if (name == GL_VENDOR)
	{
		static GLubyte * gl_vendor = 0;
		static const char * old_gl_vendor = 0;
		static const char col_dragon[] = " and Colourless Dragon";

		if (gl_vendor && old_gl_vendor != called_gl_string)
		{
			free(gl_vendor);
			gl_vendor = 0;
		}
		old_gl_vendor = called_gl_string;

		if (!gl_vendor)
		{
			gl_vendor = (GLubyte*) malloc (strlen(old_gl_vendor) + sizeof(col_dragon));
			strcpy((char*)gl_vendor, old_gl_vendor);
			strcat((char*)gl_vendor, col_dragon);
		}
		return gl_vendor;
	}
	// add " GxpOGL" to the GL_VERSION string
	else if (name == GL_VERSION)
	{
		static GLubyte * gl_version = 0;
		static const char * old_gl_version = 0;
		static const char gxpogl_ver[] = " GxpOGL";

		if (gl_version && old_gl_version != called_gl_string)
		{
			free(gl_version);
			gl_version = 0;
		}
		old_gl_version = called_gl_string;

		if (!gl_version)
		{
			gl_version = (GLubyte*) malloc (strlen(old_gl_version) + sizeof(gxpogl_ver));
			strcpy((char*)gl_version, old_gl_version);
			strcat((char*)gl_version, gxpogl_ver);
		}
		return gl_version;
	}
	// add "/GxpOGL Build ????" to the GL_RENDERER string
	else if (name == GL_RENDERER)
	{
		static GLubyte * gl_renderer= 0;
		static const char * old_gl_renderer= 0;
		static const char gxpogl_build[] = "/GxpOGL Build " BUILD_NUMBERSTR;

		if (gl_renderer && old_gl_renderer != called_gl_string)
		{
			free(gl_renderer);
			gl_renderer = 0;
		}
		old_gl_renderer = called_gl_string;

		if (!gl_renderer)
		{
			gl_renderer = (GLubyte*) malloc (strlen(old_gl_renderer) + sizeof(gxpogl_build));
			strcpy((char*)gl_renderer, old_gl_renderer);
			strcat((char*)gl_renderer, gxpogl_build);
		}
		return gl_renderer;
	}
	// add the paletted extension strings to to the GL_EXTENSION string, if they don't exist
	else if (name == GL_EXTENSIONS)
	{
		static GLubyte * gl_extensions = 0;
		static const char * old_gl_extensions = 0;
		static const char gxpogl_ext[] = "GL_EXT_paletted_texture GL_EXT_shared_texture_palette ";

		if (gl_extensions && old_gl_extensions != called_gl_string)
		{
			free(gl_extensions);
			gl_extensions = 0;
		}
		old_gl_extensions = called_gl_string;

		if (!gl_extensions)
		{
			gl_extensions = (GLubyte*) old_gl_extensions;

			if (!strstr((const char *)gl_extensions, "GL_EXT_paletted_texture"))
			{
				gl_extensions= (GLubyte*) malloc (strlen(old_gl_extensions) + sizeof(gxpogl_ext));
				strcpy((char*)gl_extensions, old_gl_extensions);
				strcat((char*)gl_extensions, gxpogl_ext);
			}
		}
		return gl_extensions;
	}

	return (GLubyte *) called_gl_string;
}
