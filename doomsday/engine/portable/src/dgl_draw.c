/**\file dgl_draw.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2007-2012 Daniel Swanson <danij@dengine.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/**
 * Drawing Operations and Vertex Arrays.
 *
 * Get OpenGL header files from:
 * http://oss.sgi.com/projects/ogl-sample/
 */

// HEADER FILES ------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>

#include "de_base.h"
#include "de_console.h"
#include "de_graphics.h"
#include "de_misc.h"

#include "sys_opengl.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// FUNCTION PROTOTYPES -----------------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int primLevel = 0;
static DGLuint inList = 0;
#ifdef _DEBUG
static boolean inPrim = false;
#endif

// CODE --------------------------------------------------------------------

boolean GL_NewList(DGLuint list, int mode)
{
    // We enter a New/End list section.
#ifdef _DEBUG
if(inList)
    Con_Error("GL_NewList: Already in list");
Sys_GLCheckError();
#endif

    if(list)
    {   // A specific list id was requested. Is it free?
        if(glIsList(list))
        {
#if _DEBUG
Con_Error("GL_NewList: List %u already in use.", (unsigned int) list);
#endif
            return false;
        }
    }
    else
    {   // Just get a new list id, it doesn't matter.
        list = glGenLists(1);
    }

    glNewList(list, mode == DGL_COMPILE? GL_COMPILE : GL_COMPILE_AND_EXECUTE);
    inList = list;
    return true;
}

DGLuint GL_EndList(void)
{
    DGLuint currentList = inList;

    glEndList();
#ifdef _DEBUG
    inList = 0;
    Sys_GLCheckError();
#endif

    return currentList;
}

void GL_CallList(DGLuint list)
{
    if(!list) return; // We do not consider zero a valid list id.
    glCallList(list);
}

void GL_DeleteLists(DGLuint list, int range)
{
    glDeleteLists(list, range);
}

void DGL_Color3ub(DGLubyte r, DGLubyte g, DGLubyte b)
{
    glColor3ub(r, g, b);
}

void DGL_Color3ubv(const DGLubyte* vec)
{
    glColor3ubv(vec);
}

void DGL_Color4ub(DGLubyte r, DGLubyte g, DGLubyte b, DGLubyte a)
{
    glColor4ub(r, g, b, a);
}

void DGL_Color4ubv(const DGLubyte* vec)
{
    glColor4ubv(vec);
}

void DGL_Color3f(float r, float g, float b)
{
    glColor3f(r, g, b);
}

void DGL_Color3fv(const float* vec)
{
    glColor3fv(vec);
}

void DGL_Color4f(float r, float g, float b, float a)
{
    glColor4f(r, g, b, a);
}

void DGL_Color4fv(const float* vec)
{
    glColor4fv(vec);
}

void DGL_TexCoord2f(byte target, float s, float t)
{
    glMultiTexCoord2f(GL_TEXTURE0 + target, s, t);
}

void DGL_TexCoord2fv(byte target, float* vec)
{
    glMultiTexCoord2fv(GL_TEXTURE0 + target, vec);
}

void DGL_Vertex2f(float x, float y)
{
    glVertex2f(x, y);
}

void DGL_Vertex2fv(const float* vec)
{
    glVertex2fv(vec);
}

void DGL_Vertex3f(float x, float y, float z)
{
    glVertex3f(x, y, z);
}

void DGL_Vertex3fv(const float* vec)
{
    glVertex3fv(vec);
}

void DGL_Vertices2ftv(int num, const dgl_ft2vertex_t* vec)
{
    for(; num > 0; num--, vec++)
    {
        glTexCoord2fv(vec->tex);
        glVertex2fv(vec->pos);
    }
}

void DGL_Vertices3ftv(int num, const dgl_ft3vertex_t* vec)
{
    for(; num > 0; num--, vec++)
    {
        glTexCoord2fv(vec->tex);
        glVertex3fv(vec->pos);
    }
}

void DGL_Vertices3fctv(int num, const dgl_fct3vertex_t* vec)
{
    for(; num > 0; num--, vec++)
    {
        glColor4fv(vec->color);
        glTexCoord2fv(vec->tex);
        glVertex3fv(vec->pos);
    }
}

void DGL_Begin(dglprimtype_t mode)
{
    if(novideo)
        return;

    // We enter a Begin/End section.
    primLevel++;

#ifdef _DEBUG
    if(inPrim)
        Con_Error("OpenGL: already inPrim");
    inPrim = true;
    Sys_GLCheckError();
#endif

    glBegin(mode == DGL_POINTS ? GL_POINTS : mode ==
            DGL_LINES ? GL_LINES : mode ==
            DGL_TRIANGLES ? GL_TRIANGLES : mode ==
            DGL_TRIANGLE_FAN ? GL_TRIANGLE_FAN : mode ==
            DGL_TRIANGLE_STRIP ? GL_TRIANGLE_STRIP : mode ==
            DGL_QUAD_STRIP ? GL_QUAD_STRIP : GL_QUADS);
}

void DGL_End(void)
{
    if(novideo)
        return;

    if(primLevel > 0)
    {
        primLevel--;
        glEnd();
    }

#ifdef _DEBUG
    inPrim = false;
    Sys_GLCheckError();
#endif
}

boolean DGL_NewList(DGLuint list, int mode)
{
    return GL_NewList(list, mode);
}

DGLuint DGL_EndList(void)
{
    return GL_EndList();
}

void DGL_CallList(DGLuint list)
{
    GL_CallList(list);
}

void DGL_DeleteLists(DGLuint list, int range)
{
    GL_DeleteLists(list, range);
}

void DGL_DrawLine(float x1, float y1, float x2, float y2, float r, float g, float b, float a)
{
    GL_DrawLine(x1, y1, x2, y2, r, g, b, a);
}

void DGL_DrawRect(const RectRaw* rect)
{
    GL_DrawRect(rect);
}

void DGL_DrawRect2(int x, int y, int w, int h)
{
    GL_DrawRect2(x, y, w, h);
}

void DGL_DrawRectf(const RectRawf* rect)
{
    GL_DrawRectf(rect);
}

void DGL_DrawRectf2(double x, double y, double w, double h)
{
    GL_DrawRectf2(x, y, w, h);
}

void DGL_DrawRectf2Color(double x, double y, double w, double h, float r, float g, float b, float a)
{
    glColor4f(r, g, b, a);
    GL_DrawRectf2(x, y, w, h);
}

void DGL_DrawRectf2Tiled(double x, double y, double w, double h, int tw, int th)
{
    // Make sure the current texture will be tiled.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    GL_DrawRectf2Tiled(x, y, w, h, tw, th);
}

void DGL_DrawCutRectf2Tiled(double x, double y, double w, double h, int tw, int th,
    int txoff, int tyoff, double cx, double cy, double cw, double ch)
{
    GL_DrawCutRectf2Tiled(x, y, w, h, tw, th, txoff, tyoff, cx, cy, cw, ch);
}
