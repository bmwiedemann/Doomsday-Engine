/** @file rend_model.cpp  3D Model Rendering.
 *
 * @note Light vectors and triangle normals are in an entirely independent,
 *       right-handed coordinate system.
 *
 * There is some more confusion with Y and Z axes as the game uses Z as the
 * vertical axis and the rendering code and model definitions use the Y axis.
 *
 * @authors Copyright &copy; 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2006-2013 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include "clientapp.h"
#include "render/rend_model.h"

#include "render/rend_main.h"
#include "render/vlight.h"

#include "gl/gl_main.h"
#include "gl/gl_texmanager.h"

#include "network/net_main.h" // gametic

#include "MaterialSnapshot"
#include "MaterialVariantSpec"
#include "Texture"

#include "con_main.h"
#include "dd_def.h"
#include "dd_main.h" // App_World()

#include <de/Log>
#include <de/binangle.h>
#include <de/memory.h>
#include <cstdlib>
#include <cmath>
#include <cstring>

using namespace de;

#define QATAN2(y,x)         qatan2(y,x)
#define QASIN(x)            asin(x) // @todo Precalculate arcsin.

#define MAX_ARRAYS  (2 + MAX_TEX_UNITS)

typedef enum rendcmd_e {
    RC_COMMAND_COORDS,
    RC_OTHER_COORDS,
    RC_BOTH_COORDS
} rendcmd_t;

typedef struct array_s {
    bool enabled;
    void *data;
} array_t;

int modelLight         = 4;
int frameInter         = true;
int mirrorHudModels;
int modelShinyMultitex = true;
float modelShinyFactor = 1.0f;
float modelSpinSpeed   = 1;
int maxModelDistance   = 1500;
float rend_model_lod   = 256;

static bool inited;

static array_t arrays[MAX_ARRAYS];

// The global vertex render buffer.
static Vector3f *modelPosCoords;
static Vector3f *modelNormCoords;
static Vector4ub *modelColorCoords;
static Vector2f *modelTexCoords;

// Global variables for ease of use. (Egads!)
static Vector3f modelCenter;
static int activeLod;
static QBitArray *vertexUsage;

static uint vertexBufferMax; ///< Maximum number of vertices we'll be required to render per submodel.
static uint vertexBufferSize; ///< Current number of vertices supported by the render buffer.
#if _DEBUG
static bool announcedVertexBufferMaxBreach; ///< @c true if an attempt has been made to expand beyond our capability.
#endif

void Rend_ModelRegister()
{
    C_VAR_BYTE ("rend-model",                &useModels,            0, 0, 1);
    C_VAR_INT  ("rend-model-lights",         &modelLight,           0, 0, 10);
    C_VAR_INT  ("rend-model-inter",          &frameInter,           0, 0, 1);
    C_VAR_FLOAT("rend-model-aspect",         &rModelAspectMod,      CVF_NO_MAX | CVF_NO_MIN, 0, 0);
    C_VAR_INT  ("rend-model-distance",       &maxModelDistance,     CVF_NO_MAX, 0, 0);
    C_VAR_BYTE ("rend-model-precache",       &precacheSkins,        0, 0, 1);
    C_VAR_FLOAT("rend-model-lod",            &rend_model_lod,       CVF_NO_MAX, 0, 0);
    C_VAR_INT  ("rend-model-mirror-hud",     &mirrorHudModels,      0, 0, 1);
    C_VAR_FLOAT("rend-model-spin-speed",     &modelSpinSpeed,       CVF_NO_MAX | CVF_NO_MIN, 0, 0);
    C_VAR_INT  ("rend-model-shiny-multitex", &modelShinyMultitex,   0, 0, 1);
    C_VAR_FLOAT("rend-model-shiny-strength", &modelShinyFactor,     0, 0, 10);
}

boolean Rend_ModelExpandVertexBuffers(uint numVertices)
{
    DENG2_ASSERT(inited);

    LOG_AS("Rend_ModelExpandVertexBuffers");

    if(numVertices <= vertexBufferMax) return true;

    // Sanity check a sane maximum...
    if(numVertices >= RENDER_MAX_MODEL_VERTS)
    {
#ifdef DENG_DEBUG
        if(!announcedVertexBufferMaxBreach)
        {
            LOG_WARNING("Attempted to expand to %u vertices (max %u), ignoring.")
                << numVertices << RENDER_MAX_MODEL_VERTS;
            announcedVertexBufferMaxBreach = true;
        }
#endif
        return false;
    }

    // Defer resizing of the render buffer until draw time as it may be repeatedly expanded.
    vertexBufferMax = numVertices;
    return true;
}

void Rend_ModelSetFrame(modeldef_t *modef, int frame)
{
    if(!modef) return;

    for(uint i = 0; i < modef->subCount(); ++i)
    {
        submodeldef_t &subdef = modef->subModelDef(i);
        Model *mdl;
        if(subdef.modelId == NOMODELID) continue;

        // Modify the modeldef itself: set the current frame.
        mdl = Models_Model(subdef.modelId);
        DENG2_ASSERT(mdl != 0);
        subdef.frame = frame % mdl->frameCount();
    }
}

/// @return  @c true= Vertex buffer is large enough to handle @a numVertices.
static bool Mod_ExpandVertexBuffer(uint numVertices)
{
    // Mark the vertex buffer if a resize is necessary.
    Rend_ModelExpandVertexBuffers(numVertices);

    // Do we need to resize the buffers?
    if(vertexBufferMax != vertexBufferSize)
    {
        /// @todo Align access to this memory along a 4-byte boundary?
        modelPosCoords   =  (Vector3f *) M_Realloc(modelPosCoords,   sizeof(*modelPosCoords)   * vertexBufferMax);
        modelNormCoords  =  (Vector3f *) M_Realloc(modelNormCoords,  sizeof(*modelNormCoords)  * vertexBufferMax);
        modelColorCoords = (Vector4ub *) M_Realloc(modelColorCoords, sizeof(*modelColorCoords) * vertexBufferMax);
        modelTexCoords   =  (Vector2f *) M_Realloc(modelTexCoords,   sizeof(*modelTexCoords)   * vertexBufferMax);

        vertexBufferSize = vertexBufferMax;
    }

    // Is the buffer large enough?
    return vertexBufferSize >= numVertices;
}

static void Mod_InitArrays()
{
    if(!GL_state.features.elementArrays) return;
    de::zap(arrays);
}

#if 0
static void Mod_EnableArrays(int vertices, int colors, int coords)
{
    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    if(vertices)
    {
        if(!GL_state.features.elementArrays)
            arrays[AR_VERTEX].enabled = true;
        else
            glEnableClientState(GL_VERTEX_ARRAY);
    }

    if(colors)
    {
        if(!GL_state.features.elementArrays)
            arrays[AR_COLOR].enabled = true;
        else
            glEnableClientState(GL_COLOR_ARRAY);
    }

    for(int i = 0; i < numTexUnits; ++i)
    {
        if(coords & (1 << i))
        {
            if(!GL_state.features.elementArrays)
            {
                arrays[AR_TEXCOORD0 + i].enabled = true;
            }
            else
            {
                glClientActiveTexture(GL_TEXTURE0 + i);
                glEnableClientState(GL_TEXTURE_COORD_ARRAY);
            }
        }
    }

    DENG_ASSERT(!Sys_GLCheckError());
}
#endif

static void Mod_DisableArrays(int vertices, int colors, int coords)
{
    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    if(vertices)
    {
        if(!GL_state.features.elementArrays)
            arrays[AR_VERTEX].enabled = false;
        else
            glDisableClientState(GL_VERTEX_ARRAY);
    }

    if(colors)
    {
        if(!GL_state.features.elementArrays)
            arrays[AR_COLOR].enabled = false;
        else
            glDisableClientState(GL_COLOR_ARRAY);
    }

    for(int i = 0; i < numTexUnits; ++i)
    {
        if(coords & (1 << i))
        {
            if(!GL_state.features.elementArrays)
            {
                arrays[AR_TEXCOORD0 + i].enabled = false;
            }
            else
            {
                glClientActiveTexture(GL_TEXTURE0 + i);
                glDisableClientState(GL_TEXTURE_COORD_ARRAY);
                glTexCoordPointer(2, GL_FLOAT, 0, NULL);
            }
        }
    }

    DENG_ASSERT(!Sys_GLCheckError());
}

static inline void enableTexUnit(byte id)
{
    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    glActiveTexture(GL_TEXTURE0 + id);
    glEnable(GL_TEXTURE_2D);
}

static inline void disableTexUnit(byte id)
{
    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    glActiveTexture(GL_TEXTURE0 + id);
    glDisable(GL_TEXTURE_2D);

    // Implicit disabling of texcoord array.
    if(!GL_state.features.elementArrays)
    {
        Mod_DisableArrays(0, 0, 1 << id);
    }
}

/**
 * The first selected unit is active after this call.
 */
static void Mod_SelectTexUnits(int count)
{
    for(int i = numTexUnits - 1; i >= count; i--)
    {
        disableTexUnit(i);
    }

    // Enable the selected units.
    for(int i = count - 1; i >= 0; i--)
    {
        if(i >= numTexUnits) continue;
        enableTexUnit(i);
    }
}

/**
 * Enable, set and optionally lock all enabled arrays.
 */
static void Mod_Arrays(void *vertices, void *colors, int numCoords = 0,
    void **coords = 0, int lock = 0)
{
    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    if(vertices)
    {
        if(!GL_state.features.elementArrays)
        {
            arrays[AR_VERTEX].enabled = true;
            arrays[AR_VERTEX].data = vertices;
        }
        else
        {
            glEnableClientState(GL_VERTEX_ARRAY);
            glVertexPointer(3, GL_FLOAT, 0, vertices);
        }
    }

    if(colors)
    {
        if(!GL_state.features.elementArrays)
        {
            arrays[AR_COLOR].enabled = true;
            arrays[AR_COLOR].data = colors;
        }
        else
        {
            glEnableClientState(GL_COLOR_ARRAY);
            glColorPointer(4, GL_UNSIGNED_BYTE, 0, colors);
        }
    }

    for(int i = 0; i < numCoords && i < MAX_TEX_UNITS; ++i)
    {
        if(coords[i])
        {
            if(!GL_state.features.elementArrays)
            {
                arrays[AR_TEXCOORD0 + i].enabled = true;
                arrays[AR_TEXCOORD0 + i].data = coords[i];
            }
            else
            {
                glClientActiveTexture(GL_TEXTURE0 + i);
                glEnableClientState(GL_TEXTURE_COORD_ARRAY);
                glTexCoordPointer(2, GL_FLOAT, 0, coords[i]);
            }
        }
    }

    if(GL_state.features.elementArrays && lock > 0)
    {
        // 'lock' is the number of vertices to lock.
        glLockArraysEXT(0, lock);
    }

    DENG_ASSERT(!Sys_GLCheckError());
}

#if 0
static void Mod_UnlockArrays()
{
    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    if(!GL_state.features.elementArrays) return;

    glUnlockArraysEXT();
    DENG_ASSERT(!Sys_GLCheckError());
}
#endif

static void Mod_ArrayElement(int index)
{
    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    if(GL_state.features.elementArrays)
    {
        glArrayElement(index);
    }
    else
    {
        for(int i = 0; i < numTexUnits; ++i)
        {
            if(!arrays[AR_TEXCOORD0 + i].enabled) continue;

            Vector2f const &texCoord = ((Vector2f const *)arrays[AR_TEXCOORD0 + i].data)[index];
            glMultiTexCoord2f(GL_TEXTURE0 + i, texCoord.x, texCoord.y);
        }

        if(arrays[AR_COLOR].enabled)
        {
            Vector4ub const &colorCoord = ((Vector4ub const *) arrays[AR_COLOR].data)[index];
            glColor4ub(colorCoord.x, colorCoord.y, colorCoord.z, colorCoord.w);
        }

        if(arrays[AR_VERTEX].enabled)
        {
            Vector3f const &posCoord = ((Vector3f const *) arrays[AR_VERTEX].data)[index];
            glVertex3f(posCoord.x, posCoord.y, posCoord.z);
        }
    }
}

static inline float qatan2(float y, float x)
{
    float ang = BANG2RAD(bamsAtan2(y * 512, x * 512));
    if(ang > PI) ang -= 2 * (float) PI;
    return ang;
}

/**
 * Return a pointer to the visible model frame.
 */
static ModelFrame *Mod_GetVisibleFrame(modeldef_t *mf, int subnumber, int mobjId)
{
    if(subnumber >= int(mf->subCount()))
    {
        throw Error("Mod_GetVisibleFrame",
                    QString("Model has %1 submodels, but submodel #%2 was requested")
                    .arg(mf->subCount()).arg(subnumber));
    }
    submodeldef_t const &sub = mf->subModelDef(subnumber);

    int curFrame = sub.frame;
    if(mf->flags & MFF_IDFRAME)
    {
        curFrame += mobjId % sub.frameRange;
    }

    return &Models_Model(sub.modelId)->frame(curFrame);
}

/**
 * Render a set of 3D model primitives using the given data.
 */
static void Mod_RenderPrimitives(rendcmd_t mode, Model::DetailLevel::Primitives const &primitives,
    Vector3f *posCoords, Vector4ub *colorCoords, Vector2f *texCoords = 0)
{
    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    // Disable all vertex arrays.
    Mod_DisableArrays(true, true, DDMAXINT);

    // Load the vertex array.
    void *coords[2];
    switch(mode)
    {
    case RC_OTHER_COORDS:
        coords[0] = texCoords;
        Mod_Arrays(posCoords, colorCoords, 1, coords);
        break;

    case RC_BOTH_COORDS:
        coords[0] = NULL;
        coords[1] = texCoords;
        Mod_Arrays(posCoords, colorCoords, 2, coords);
        break;

    default:
        Mod_Arrays(posCoords, colorCoords);
        break;
    }

    foreach(ModelDetailLevel::Primitive const &prim, primitives)
    {
        // The type of primitive depends on the sign.
        glBegin(prim.triFan? GL_TRIANGLE_FAN : GL_TRIANGLE_STRIP);

        foreach(ModelDetailLevel::Primitive::Element const &elem, prim.elements)
        {
            if(mode != RC_OTHER_COORDS)
            {
                glTexCoord2f(elem.texCoord.x, elem.texCoord.y);
            }

            Mod_ArrayElement(elem.index);
        }

        // The primitive is complete.
        glEnd();
    }
}

/**
 * Linear interpolation between two values.
 */
static inline float Mod_Lerp(float start, float end, float pos)
{
    return end * pos + start * (1 - pos);
}

/**
 * Interpolate linearly between two sets of vertices.
 */
static void Mod_LerpVertices(float inter, int count, ModelFrame const &from,
    ModelFrame const &to, Vector3f *posOut, Vector3f *normOut)
{
    DENG2_ASSERT(&from.model == &to.model); // sanity check.
    DENG2_ASSERT(from.vertices.count() == to.vertices.count()); // sanity check.

    ModelFrame::VertexBuf::const_iterator startIt = from.vertices.begin();
    ModelFrame::VertexBuf::const_iterator endIt   = to.vertices.begin();

    if(&from == &to || de::fequal(inter, 0))
    {
        for(int i = 0; i < count; ++i, startIt++, posOut++, normOut++)
        {
            *posOut  = startIt->pos;
            *normOut = startIt->norm;
        }
        return;
    }

    float const invInter = 1 - inter;

    if(vertexUsage)
    {
        int const modelLodCount = from.model.lodCount();
        for(int i = 0; i < count; ++i, startIt++, endIt++, posOut++, normOut++)
        {
            if(vertexUsage->testBit(i * modelLodCount + activeLod))
            {
                *posOut  = startIt->pos  * invInter + endIt->pos  * inter;
                *normOut = startIt->norm * invInter + endIt->norm * inter;
            }
        }
    }
    else
    {
        for(int i = 0; i < count; ++i, startIt++, endIt++, posOut++, normOut++)
        {
            *posOut  = startIt->pos  * invInter + endIt->pos  * inter;
            *normOut = startIt->norm * invInter + endIt->norm * inter;
        }
    }
}

/**
 * Negate all Z coordinates.
 */
static void Mod_MirrorCoords(int count, Vector3f *coords, int axis)
{
    for(; count-- > 0; coords++)
    {
        (*coords)[axis] = -(*coords)[axis];
    }
}

struct lightmodelvertexworker_params_t
{
    Vector3f color, extra;
    float rotateYaw, rotatePitch;
    Vector3f normal;
    uint numProcessed, max;
    bool invert;
};

static void lightModelVertex(VectorLight const &vlight, lightmodelvertexworker_params_t &parms)
{
    // We must transform the light vector to model space.
    float vlightDirection[3] = { vlight.direction.x, vlight.direction.y, vlight.direction.z };
    M_RotateVector(vlightDirection, parms.rotateYaw, parms.rotatePitch);

    // Quick hack: Flip light normal if model inverted.
    if(parms.invert)
    {
        vlightDirection[VX] = -vlightDirection[VX];
        vlightDirection[VY] = -vlightDirection[VY];
    }

    float strength = Vector3f(vlightDirection).dot(parms.normal)
                   + vlight.offset; // Shift a bit towards the light.

    // Ability to both light and shade.
    if(strength > 0)
    {
        strength *= vlight.lightSide;
    }
    else
    {
        strength *= vlight.darkSide;
    }

    Vector3f &dest = vlight.affectedByAmbient? parms.color : parms.extra;
    dest += vlight.color * de::clamp(-1.f, strength, 1.f);
}

static int lightModelVertexWorker(VectorLight const *vlight, void *context)
{
    lightmodelvertexworker_params_t &parms = *static_cast<lightmodelvertexworker_params_t *>(context);

    lightModelVertex(*vlight, parms);
    parms.numProcessed += 1;

    // Time to stop?
    return parms.max && parms.numProcessed == parms.max;
}

/**
 * Calculate vertex lighting.
 * @todo construct a rotation matrix once and use it for all vertices.
 */
static void Mod_VertexColors(Vector4ub *out, int count, int modelLodCount,
    Vector3f const *normCoords, uint vLightListIdx, uint maxLights,
    Vector4f const &ambient, bool invert, float rotateYaw, float rotatePitch)
{
    Vector4f const saturated(1, 1, 1, 1);
    lightmodelvertexworker_params_t parms;

    for(int i = 0; i < count; ++i, out++, normCoords++)
    {
        if(vertexUsage)
        {
            if(!vertexUsage->testBit(i * modelLodCount + activeLod))
                continue;
        }

        // Begin with total darkness.
        parms.color        = Vector3f();
        parms.extra        = Vector3f();
        parms.normal       = *normCoords;
        parms.invert       = invert;
        parms.rotateYaw    = rotateYaw;
        parms.rotatePitch  = rotatePitch;
        parms.max          = maxLights;
        parms.numProcessed = 0;

        // Add light from each source.
        VL_ListIterator(vLightListIdx, lightModelVertexWorker, &parms);

        // Check for ambient and convert to ubyte.
        Vector4f color(parms.color.max(ambient) + parms.extra, ambient[3]);

        *out = (color.min(saturated) * 255).toVector4ub();
    }
}

/**
 * Set all the colors in the array to bright white.
 */
static void Mod_FullBrightVertexColors(int count, Vector4ub *colorCoords, float alpha)
{
    DENG2_ASSERT(colorCoords != 0);
    for(; count-- > 0; colorCoords++)
    {
        *colorCoords = Vector4ub(255, 255, 255, 255 * alpha);
    }
}

/**
 * Set all the colors into the array to the same values.
 */
static void Mod_FixedVertexColors(int count, Vector4ub *colorCoords, Vector4ub const &color)
{
    DENG2_ASSERT(colorCoords != 0);
    for(; count-- > 0; colorCoords++)
    {
        *colorCoords = color;
    }
}

/**
 * Calculate cylindrically mapped, shiny texture coordinates.
 */
static void Mod_ShinyCoords(Vector2f *out, int count, int modelLodCount,
    Vector3f const *normCoords, float normYaw, float normPitch, float shinyAng,
    float shinyPnt, float reactSpeed)
{
    for(int i = 0; i < count; ++i, out++, normCoords++)
    {
        if(vertexUsage)
        {
            if(!vertexUsage->testBit(i * modelLodCount + activeLod))
                continue;
        }

        float rotatedNormal[3] = { normCoords->x, normCoords->y, normCoords->z };

        // Rotate the normal vector so that it approximates the
        // model's orientation compared to the viewer.
        M_RotateVector(rotatedNormal,
                       (shinyPnt + normYaw) * 360 * reactSpeed,
                       (shinyAng + normPitch - .5f) * 180 * reactSpeed);

        *out = Vector2f(rotatedNormal[0] + 1, rotatedNormal[2]);
    }
}

static int chooseSelSkin(modeldef_t *mf, int submodel, int selector)
{
    if(mf->def->hasSub(submodel))
    {
        int i = (selector >> DDMOBJ_SELECTOR_SHIFT) &
                mf->def->sub(submodel).selSkinBits[0]; // Selskin mask
        int c = mf->def->sub(submodel).selSkinBits[1]; // Selskin shift

        if(c > 0) i >>= c;
        else      i <<= -c;

        if(i > 7) i = 7; // Maximum number of skins for selskin.
        if(i < 0) i = 0; // Improbable (impossible?), but doesn't hurt.

        return mf->def->sub(submodel).selSkins[i];
    }
    return 0;
}

static int chooseSkin(modeldef_t *mf, int submodel, int id, int selector, int tmap)
{
    if(submodel >= int(mf->subCount()))
    {
        return 0;
    }

    submodeldef_t &smf = mf->subModelDef(submodel);
    Model *mdl = Models_Model(smf.modelId);
    int skin = smf.skin;

    // Selskin overrides the skin range.
    if(smf.testFlag(MFF_SELSKIN))
    {
        skin = chooseSelSkin(mf, submodel, selector);
    }

    // Is there a skin range for this frame?
    // (During model setup skintics and skinrange are set to >0.)
    if(smf.skinRange > 1)
    {
        // What rule to use for determining the skin?
        int offset;
        if(smf.testFlag(MFF_IDSKIN))
        {
            offset = id;
        }
        else
        {
            offset = SECONDS_TO_TICKS(App_World().time()) / mf->skinTics;
        }

        skin += offset % smf.skinRange;
    }

    // Need translation?
    if(smf.testFlag(MFF_SKINTRANS))
    {
        skin = tmap;
    }

    DENG2_ASSERT(mdl != 0);
    if(skin < 0 || skin >= mdl->skinCount())
    {
        skin = 0;
    }

    return skin;
}

TextureVariantSpec &Rend_ModelDiffuseTextureSpec(bool noCompression)
{
    return ClientApp::resourceSystem().textureSpec(TC_MODELSKIN_DIFFUSE,
        (noCompression? TSF_NO_COMPRESSION : 0), 0, 0, 0, GL_REPEAT, GL_REPEAT,
        1, -2, -1, true, true, false, false);
}

TextureVariantSpec &Rend_ModelShinyTextureSpec()
{
    return ClientApp::resourceSystem().textureSpec(TC_MODELSKIN_REFLECTION,
        TSF_NO_COMPRESSION, 0, 0, 0, GL_REPEAT, GL_REPEAT, 1, -2, -1, false,
        false, false, false);
}

/**
 * Render a submodel from the vissprite.
 */
static void Mod_RenderSubModel(uint number, rendmodelparams_t const *parm)
{
    modeldef_t *mf = parm->mf, *mfNext = parm->nextMF;
    submodeldef_t *smf = &mf->subModelDef(number);
    Model *mdl = Models_Model(smf->modelId);
    ModelFrame *frame = Mod_GetVisibleFrame(mf, number, parm->id);
    ModelFrame *nextFrame = 0;

    int numVerts, useSkin;
    float endPos, offset, alpha;
    Vector3f delta;
    Vector4f color, ambient;
    float shininess;
    Vector3f shinyColor;
    float normYaw, normPitch, shinyAng, shinyPnt;
    float inter = parm->inter;
    blendmode_t blending;
    TextureVariant *skinTexture = 0, *shinyTexture = 0;
    int zSign = (parm->mirror? -1 : 1);

    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    // Do not bother with infinitely small models...
    if(mf->scale[VX] == 0 && (int)mf->scale[VY] == 0 && mf->scale[VZ] == 0) return;

    alpha = parm->ambientColor[CA];
    // Is the submodel-defined alpha multiplier in effect?
    if(!(parm->flags & (DDMF_BRIGHTSHADOW|DDMF_SHADOW|DDMF_ALTSHADOW)))
    {
        alpha *= smf->alpha * reciprocal255;
    }

    // Would this be visible?
    if(alpha <= 0) return;

    // Is the submodel-defined blend mode in effect?
    if(parm->flags & DDMF_BRIGHTSHADOW)
    {
        blending = BM_ADD;
    }
    else
    {
        blending = smf->blendMode;
    }

    useSkin = chooseSkin(mf, number, parm->id, parm->selector, parm->tmap);

    // Scale interpos. Intermark becomes zero and endmark becomes one.
    // (Full sub-interpolation!) But only do it for the standard
    // interrange. If a custom one is defined, don't touch interpos.
    if((mf->interRange[0] == 0 && mf->interRange[1] == 1) || smf->testFlag(MFF_WORLD_TIME_ANIM))
    {
        endPos = (mf->interNext ? mf->interNext->interMark : 1);
        inter = (parm->inter - mf->interMark) / (endPos - mf->interMark);
    }

    // Do we have a sky/particle model here?
    if(parm->alwaysInterpolate)
    {
        // Always interpolate, if there's animation.
        // Used with sky and particle models.
        nextFrame = &mdl->frame((smf->frame + 1) % mdl->frameCount());
        mfNext = mf;
    }
    else
    {
        // Check for possible interpolation.
        if(frameInter && mfNext && !smf->testFlag(MFF_DONT_INTERPOLATE))
        {
            if(mfNext->hasSub(number) && mfNext->subModelId(number) == smf->modelId)
            {
                nextFrame = Mod_GetVisibleFrame(mfNext, number, parm->id);
            }
        }
    }

    // Clamp interpolation.
    inter = de::clamp(0.f, inter, 1.f);

    if(!nextFrame)
    {
        // If not interpolating, use the same frame as interpolation target.
        // The lerp routines will recognize this special case.
        nextFrame = frame;
        mfNext = mf;
    }

    // Determine the total number of vertices we have.
    numVerts = mdl->vertexCount();

    // Ensure our vertex render buffers can accommodate this.
    if(!Mod_ExpandVertexBuffer(numVerts))
    {
        // No can do, we aint got the power!
        return;
    }

    // Setup transformation.
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();

    // Model space => World space
    glTranslatef(parm->origin[VX] + parm->srvo[VX] +
                   Mod_Lerp(mf->offset.x, mfNext->offset.x, inter),
                 parm->origin[VZ] + parm->srvo[VZ] +
                   Mod_Lerp(mf->offset.y, mfNext->offset.y, inter),
                 parm->origin[VY] + parm->srvo[VY] + zSign *
                   Mod_Lerp(mf->offset.z, mfNext->offset.z, inter));

    if(parm->extraYawAngle || parm->extraPitchAngle)
    {
        // Sky models have an extra rotation.
        glScalef(1, 200 / 240.0f, 1);
        glRotatef(parm->extraYawAngle, 1, 0, 0);
        glRotatef(parm->extraPitchAngle, 0, 0, 1);
        glScalef(1, 240 / 200.0f, 1);
    }

    // Model rotation.
    glRotatef(parm->viewAlign ? parm->yawAngleOffset : parm->yaw,
              0, 1, 0);
    glRotatef(parm->viewAlign ? parm->pitchAngleOffset : parm->pitch,
              0, 0, 1);

    // Scaling and model space offset.
    glScalef(Mod_Lerp(mf->scale.x, mfNext->scale.x, inter),
             Mod_Lerp(mf->scale.y, mfNext->scale.y, inter),
             Mod_Lerp(mf->scale.z, mfNext->scale.z, inter));
    if(parm->extraScale)
    {
        // Particle models have an extra scale.
        glScalef(parm->extraScale, parm->extraScale, parm->extraScale);
    }
    glTranslatef(smf->offset.x, smf->offset.y, smf->offset.z);

    /**
     * Now we can draw.
     */

    // Determine the suitable LOD.
    if(mdl->lodCount() > 1 && rend_model_lod != 0)
    {
        float lodFactor = rend_model_lod * DENG_GAMEVIEW_WIDTH / 640.0f / (Rend_FieldOfView() / 90.0f);
        if(!de::fequal(lodFactor, 0))
        {
            lodFactor = 1 / lodFactor;
        }

        // Determine the LOD we will be using.
        activeLod = de::clamp<int>(0, lodFactor * parm->distance, mdl->lodCount() - 1);
        vertexUsage = &mdl->_vertexUsage;
    }
    else
    {
        activeLod   = 0;
        vertexUsage = 0;
    }

    // Interpolate vertices and normals.
    Mod_LerpVertices(inter, numVerts, *frame, *nextFrame,
                     modelPosCoords, modelNormCoords);

    if(zSign < 0)
    {
        Mod_MirrorCoords(numVerts, modelPosCoords, 2);
        Mod_MirrorCoords(numVerts, modelNormCoords, 1);
    }

    // Coordinates to the center of the model (game coords).
    modelCenter = Vector3f(parm->origin[VX], parm->origin[VY], (parm->origin[VZ] + parm->gzt) * 2)
            + Vector3d(parm->srvo) + Vector3f(mf->offset.x, mf->offset.z, mf->offset.y);

    // Calculate lighting.
    if(smf->testFlag(MFF_FULLBRIGHT) && !smf->testFlag(MFF_DIM))
    {
        // Submodel-specific lighting override.
        ambient = Vector4f(1, 1, 1, 1);
        Mod_FullBrightVertexColors(numVerts, modelColorCoords, alpha);
    }
    else if(!parm->vLightListIdx)
    {
        // Lit uniformly.
        ambient = Vector4f(parm->ambientColor, alpha);
        Mod_FixedVertexColors(numVerts, modelColorCoords,
                              (ambient * 255).toVector4ub());
    }
    else
    {
        // Lit normally.
        ambient = Vector4f(parm->ambientColor, alpha);

        Mod_VertexColors(modelColorCoords, numVerts, frame->model.lodCount(),
                         modelNormCoords, parm->vLightListIdx, modelLight + 1,
                         ambient, (mf->scale[VY] < 0), -parm->yaw, -parm->pitch);
    }

    shininess = 0;
    if(mf->def->hasSub(number))
    {
        shininess = de::clamp(0.f, mf->def->sub(number).shiny * modelShinyFactor, 1.f);
        // Ensure we've prepared the shiny skin.
        if(shininess > 0)
        {
            if(Texture *tex = mf->subModelDef(number).shinySkin)
            {
                shinyTexture = tex->prepareVariant(Rend_ModelShinyTextureSpec());
            }
            else
            {
                shininess = 0;
            }
        }
    }

    if(shininess > 0)
    {
        // Calculate shiny coordinates.
        shinyColor = mf->def->sub(number).shinyColor;

        // With psprites, add the view angle/pitch.
        offset = parm->shineYawOffset;

        // Calculate normalized (0,1) model yaw and pitch.
        normYaw = M_CycleIntoRange(((parm->viewAlign ? parm->yawAngleOffset
                                                     : parm->yaw) + offset) / 360, 1);

        offset = parm->shinePitchOffset;

        normPitch = M_CycleIntoRange(((parm->viewAlign ? parm->pitchAngleOffset
                                                       : parm->pitch) + offset) / 360, 1);

        if(parm->shinepspriteCoordSpace)
        {
            // This is a hack to accommodate the psprite coordinate space.
            shinyAng = 0;
            shinyPnt = 0.5;
        }
        else
        {
            delta = modelCenter - Vector3f(vOrigin[VX], vOrigin[VZ], vOrigin[VY]);

            if(parm->shineTranslateWithViewerPos)
            {
                delta += Vector3f(vOrigin[VX], vOrigin[VZ], vOrigin[VY]);
            }

            shinyAng = QATAN2(delta.z, M_ApproxDistancef(delta.x, delta.y)) / PI + 0.5f; // shinyAng is [0,1]

            shinyPnt = QATAN2(delta.y, delta.x) / (2 * PI);
        }

        Mod_ShinyCoords(modelTexCoords, numVerts, frame->model.lodCount(),
                        modelNormCoords, normYaw, normPitch, shinyAng, shinyPnt,
                        mf->def->sub(number).shinyReact);

        // Shiny color.
        if(smf->testFlag(MFF_SHINY_LIT))
        {
            color = Vector4f(ambient * shinyColor, shininess);
        }
        else
        {
            color = Vector4f(shinyColor, shininess);
        }
    }

    if(renderTextures == 2)
    {
        // For lighting debug, render all surfaces using the gray texture.
        MaterialVariantSpec const &spec =
            ClientApp::resourceSystem()
                .materialSpec(ModelSkinContext, 0, 0, 0, 0, GL_REPEAT, GL_REPEAT,
                              1, -2, -1, true, true, false, false);
        MaterialSnapshot const &ms =
            ClientApp::resourceSystem()
                .findMaterial(de::Uri("System", Path("gray"))).material().prepare(spec);

        skinTexture = &ms.texture(MTU_PRIMARY);
    }
    else
    {
        skinTexture = 0;
        if(Texture *tex = mdl->skin(useSkin).texture)
        {
            skinTexture = tex->prepareVariant(Rend_ModelDiffuseTextureSpec(mdl->flags().testFlag(Model::NoTextureCompression)));
        }
    }

    // If we mirror the model, triangles have a different orientation.
    if(zSign < 0)
    {
        glFrontFace(GL_CCW);
    }

    // Twosided models won't use backface culling.
    if(smf->testFlag(MFF_TWO_SIDED))
    {
        glDisable(GL_CULL_FACE);
    }
    glEnable(GL_TEXTURE_2D);

    // Render using multiple passes?
    if(!modelShinyMultitex || shininess <= 0 || alpha < 1 ||
       blending != BM_NORMAL || !smf->testFlag(MFF_SHINY_SPECULAR) ||
       numTexUnits < 2 || !envModAdd)
    {
        // The first pass can be skipped if it won't be visible.
        if(shininess < 1 || smf->testFlag(MFF_SHINY_SPECULAR))
        {
            Mod_SelectTexUnits(1);
            GL_BlendMode(blending);
            GL_BindTexture(renderTextures? skinTexture : 0);

            Mod_RenderPrimitives(RC_COMMAND_COORDS,
                                 mdl->lod(activeLod).primitives,
                                 modelPosCoords, modelColorCoords);
        }

        if(shininess > 0)
        {
            glDepthFunc(GL_LEQUAL);

            // Set blending mode, two choices: reflected and specular.
            if(smf->testFlag(MFF_SHINY_SPECULAR))
                GL_BlendMode(BM_ADD);
            else
                GL_BlendMode(BM_NORMAL);

            // Shiny color.
            Mod_FixedVertexColors(numVerts, modelColorCoords,
                                  (color * 255).toVector4ub());

            if(numTexUnits > 1 && modelShinyMultitex)
            {
                // We'll use multitexturing to clear out empty spots in
                // the primary texture.
                Mod_SelectTexUnits(2);
                GL_ModulateTexture(11);

                glActiveTexture(GL_TEXTURE1);
                GL_BindTexture(renderTextures? shinyTexture : 0);

                glActiveTexture(GL_TEXTURE0);
                GL_BindTexture(renderTextures? skinTexture : 0);

                Mod_RenderPrimitives(RC_BOTH_COORDS, mdl->lod(activeLod).primitives,
                                     modelPosCoords, modelColorCoords, modelTexCoords);

                Mod_SelectTexUnits(1);
                GL_ModulateTexture(1);
            }
            else
            {
                // Empty spots will get shine, too.
                Mod_SelectTexUnits(1);
                GL_BindTexture(renderTextures? shinyTexture : 0);

                Mod_RenderPrimitives(RC_OTHER_COORDS, mdl->lod(activeLod).primitives,
                                     modelPosCoords, modelColorCoords, modelTexCoords);
            }
        }
    }
    else
    {
        // A special case: specular shininess on an opaque object.
        // Multitextured shininess with the normal blending.
        GL_BlendMode(blending);
        Mod_SelectTexUnits(2);

        // Tex1*Color + Tex2RGB*ConstRGB
        GL_ModulateTexture(10);

        glActiveTexture(GL_TEXTURE1);
        GL_BindTexture(renderTextures? shinyTexture : 0);

        // Multiply by shininess.
        float colorv1[] = { color.x * color.w, color.y * color.w, color.z * color.w, color.w };
        glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, colorv1);

        glActiveTexture(GL_TEXTURE0);
        GL_BindTexture(renderTextures? skinTexture : 0);

        Mod_RenderPrimitives(RC_BOTH_COORDS, mdl->lod(activeLod).primitives,
                             modelPosCoords, modelColorCoords, modelTexCoords);

        Mod_SelectTexUnits(1);
        GL_ModulateTexture(1);
    }

    // We're done!
    glDisable(GL_TEXTURE_2D);
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();

    // Normally culling is always enabled.
    if(smf->testFlag(MFF_TWO_SIDED))
    {
        glEnable(GL_CULL_FACE);
    }

    if(zSign < 0)
    {
        glFrontFace(GL_CW);
    }
    glDepthFunc(GL_LESS);

    GL_BlendMode(BM_NORMAL);
}

void Rend_ModelInit()
{
    if(inited) return; // Already been here.

    modelPosCoords   = 0;
    modelNormCoords  = 0;
    modelColorCoords = 0;
    modelTexCoords   = 0;

    vertexBufferMax = vertexBufferSize = 0;
#ifdef DENG_DEBUG
    announcedVertexBufferMaxBreach = false;
#endif

    Mod_InitArrays();

    inited = true;
}

void Rend_ModelShutdown()
{
    if(!inited) return;

    M_Free(modelPosCoords); modelPosCoords = 0;
    M_Free(modelNormCoords); modelNormCoords = 0;
    M_Free(modelColorCoords); modelColorCoords = 0;
    M_Free(modelTexCoords); modelTexCoords = 0;

    vertexBufferMax = vertexBufferSize = 0;
#ifdef DENG_DEBUG
    announcedVertexBufferMaxBreach = false;
#endif

    inited = false;
}

static int drawLightVectorWorker(VectorLight const *vlight, void *context)
{
    coord_t distFromViewer = *static_cast<coord_t *>(context);
    if(distFromViewer < 1600 - 8)
    {
        Rend_DrawVectorLight(vlight, 1 - distFromViewer / 1600);
    }
    return false; // Continue iteration.
}

void Rend_DrawModel(rendmodelparams_t const &parms)
{
    DENG_ASSERT(inited);
    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    if(!parms.mf) return;

    // Render all the submodels of this model.
    for(uint i = 0; i < parms.mf->subCount(); ++i)
    {
        if(parms.mf->subModelId(i))
        {
            bool disableZ = (parms.mf->flags & MFF_DISABLE_Z_WRITE ||
                             parms.mf->testSubFlag(i, MFF_DISABLE_Z_WRITE));

            if(disableZ)
            {
                glDepthMask(GL_FALSE);
            }

            Mod_RenderSubModel(i, &parms);

            if(disableZ)
            {
                glDepthMask(GL_TRUE);
            }
        }
    }

    if(devMobjVLights && parms.vLightListIdx)
    {
        // Draw the vlight vectors, for debug.
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);

        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();

        glTranslatef(parms.origin[VX], parms.origin[VZ], parms.origin[VY]);

        coord_t distFromViewer = de::abs(parms.distance);
        VL_ListIterator(parms.vLightListIdx, drawLightVectorWorker, &distFromViewer);

        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();

        glEnable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);
    }
}
