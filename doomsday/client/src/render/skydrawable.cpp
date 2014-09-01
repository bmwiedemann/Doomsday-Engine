/** @file skydrawble.cpp  Drawable specialized for the sky.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2014 Daniel Swanson <danij@dengine.net>
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

#include "de_base.h"
#include "render/skydrawable.h"

#include <cmath>
#include <doomsday/console/var.h>
#include <doomsday/console/exec.h>
#include <de/Log>
#include <de/timer.h>
#include <de/concurrency.h>

#include "clientapp.h"
#include "client/cl_def.h"

#include "gl/gl_main.h"
#include "gl/gl_tex.h"

#include "MaterialSnapshot"
#include "MaterialVariantSpec"

#include "render/rend_main.h"
#include "render/rend_model.h"
#include "render/vissprite.h"

#include "Texture"
#include "world/sky.h"

using namespace de;

namespace internal {

static int skySphereColumns = 4 * 6;

/// Console variables:
static float skyDistance   = 1600; ///< Map units.
static int skySphereDetail = 6;
static int skySphereRows   = 3;

/**
 * Geometry used with the sky sphere.
 */
struct Hemisphere
{
    enum SphereComponent {
        UpperHemisphere,
        LowerHemisphere
    };

    enum hemispherecap_t
    {
        HC_NONE = 0,
        HC_TOP,
        HC_BOTTOM
    };

    bool needRebuild    = true;
    float height        = 0;
    float horizonOffset = 0;

    typedef QVector<Vector3f> VBuf;
    VBuf verts; // Crest is up.

    /**
     * Parameters for drawing.
     * @todo SkyDrawable should pass these.
     */
    struct DrawArgs
    {
        bool fadeout    = false;
        bool texXFlip   = false;
        float texOffset = 0;
        Vector2i texSize;
        Vector3f capColor;
    };
    DrawArgs ds;

    // Look up the precalculated vertex.
    Vector3f &vertex(int r, int c)
    {
        return verts[r * skySphereColumns + c % skySphereColumns];
    }

    void configureDrawState(Sky const &sky, int layerIndex, hemispherecap_t setupCap)
    {
        // Default state is no texture and no fadeout.
        ds.texSize = Vector2i();
        if(setupCap != HC_NONE)
            ds.fadeout = false;
        ds.texXFlip = true;

        if(renderTextures != 0)
        {
            Material *mat;

            if(renderTextures == 2)
            {
                mat = ClientApp::resourceSystem().materialPtr(de::Uri("System", Path("gray")));
            }
            else
            {
                mat = sky.layer(layerIndex).material();
                if(!mat)
                {
                    mat = ClientApp::resourceSystem().materialPtr(de::Uri("System", Path("missing")));
                    ds.texXFlip = false;
                }
            }
            DENG2_ASSERT(mat != 0);

            MaterialSnapshot const &ms =
                mat->prepare(SkyDrawable::layerMaterialSpec(sky.layer(layerIndex).isMasked()));

            ds.texSize = ms.texture(MTU_PRIMARY).generalCase().dimensions();
            if(ds.texSize != Vector2i(0, 0))
            {
                ds.texOffset = sky.layer(layerIndex).offset();
                GL_BindTexture(&ms.texture(MTU_PRIMARY));
            }
            else
            {
                // Disable texturing.
                ds.texSize = Vector2i();
                GL_SetNoTexture();
            }

            if(setupCap != HC_NONE)
            {
                averagecolor_analysis_t const *avgLineColor = reinterpret_cast<averagecolor_analysis_t const *>
                        (ms.texture(MTU_PRIMARY).generalCase().analysisDataPointer((setupCap == HC_TOP? Texture::AverageTopColorAnalysis : Texture::AverageBottomColorAnalysis)));
                float const fadeoutLimit = sky.layer(layerIndex).fadeoutLimit();
                if(!avgLineColor) throw Error("configureDrawHemisphereState", QString("Texture \"%1\" has no %2").arg(ms.texture(MTU_PRIMARY).generalCase().manifest().composeUri()).arg(setupCap == HC_TOP? "AverageTopColorAnalysis" : "AverageBottomColorAnalysis"));

                ds.capColor = Vector3f(avgLineColor->color.rgb);
                // Is the colored fadeout in use?
                ds.fadeout = (ds.capColor.x >= fadeoutLimit ||
                              ds.capColor.y >= fadeoutLimit ||
                              ds.capColor.z >= fadeoutLimit);;
            }
        }
        else
        {
            GL_SetNoTexture();
        }

        if(setupCap != HC_NONE && !ds.fadeout)
        {
            // Default color is black.
            ds.capColor = Vector3f();
        }
    }

    void drawCap()
    {
        // Use the appropriate color.
        glColor3f(ds.capColor.x, ds.capColor.y, ds.capColor.z);

        // Draw the cap.
        glBegin(GL_TRIANGLE_FAN);
        for(int c = 0; c < skySphereColumns; ++c)
        {
            Vector3f const &vtx = vertex(0, c);
            glVertex3f(vtx.x, vtx.y, vtx.z);
        }
        glEnd();

        // Are we doing a colored fadeout?
        if(!ds.fadeout) return;

        // We must fill the background for the top row since it'll be translucent.
        glBegin(GL_TRIANGLE_STRIP);
        Vector3f const *vtx = &vertex(0, 0);
        glVertex3f(vtx->x, vtx->y, vtx->z);
        int c = 0;
        for(; c < skySphereColumns; ++c)
        {
            // One step down.
            vtx = &vertex(1, c);
            glVertex3f(vtx->x, vtx->y, vtx->z);
            // And one step right.
            vtx = &vertex(0, c + 1);
            glVertex3f(vtx->x, vtx->y, vtx->z);
        }
        vtx = &vertex(1, c);
        glVertex3f(vtx->x, vtx->y, vtx->z);
        glEnd();
    }

    void draw(SphereComponent hemisphere, Sky const &sky)
    {
        int const firstLayer = sky.firstActiveLayer();
        DENG2_ASSERT(firstLayer >= 0);

        bool const yflip    = (hemisphere == LowerHemisphere);
        hemispherecap_t cap = (hemisphere == LowerHemisphere? HC_BOTTOM : HC_TOP);

        // Rebuild the geometry if necessary.
        rebuildIfNeeded(sky);

        if(yflip)
        {
            // The lower hemisphere must be flipped.
            glMatrixMode(GL_MODELVIEW);
            glPushMatrix();
            glScalef(1.0f, -1.0f, 1.0f);
        }

        // First render the cap and the background for fadeouts, if needed.
        configureDrawState(sky, firstLayer, cap);
        drawCap();

        for(int i = firstLayer; i < MAX_SKY_LAYERS; ++i)
        {
            if(!sky.layer(i).isActive()) continue;

            if(i != firstLayer)
            {
                configureDrawState(sky, i, HC_NONE);
            }

            if(ds.texSize.x != 0)
            {
                glEnable(GL_TEXTURE_2D);
                glMatrixMode(GL_TEXTURE);
                glPushMatrix();
                glLoadIdentity();
                glTranslatef(ds.texOffset / ds.texSize.x, 0, 0);
                glScalef(1024.f / ds.texSize.x * (ds.texXFlip? 1 : -1), yflip? -1 : 1, 1);
                if(yflip) glTranslatef(0, -1, 0);
            }

#define WRITESKYVERTEX(r_, c_) { \
    svtx = &vertex(r_, c_); \
    if(ds.texSize.x != 0) \
       glTexCoord2f((c_) / float(skySphereColumns), (r_) / float(skySphereRows)); \
    if(ds.fadeout) \
    { \
        if((r_) == 0) glColor4f(1, 1, 1, 0); \
        else          glColor3f(1, 1, 1); \
    } \
    else \
    { \
        if((r_) == 0) glColor3f(0, 0, 0); \
        else          glColor3f(1, 1, 1); \
    } \
    glVertex3f(svtx->x, svtx->y, svtx->z); \
}

            Vector3f const *svtx;
            for(int r = 0; r < skySphereRows; ++r)
            {
                glBegin(GL_TRIANGLE_STRIP);
                WRITESKYVERTEX(r, 0);
                WRITESKYVERTEX(r + 1, 0);
                for(int c = 1; c <= skySphereColumns; ++c)
                {
                    WRITESKYVERTEX(r, c);
                    WRITESKYVERTEX(r + 1, c);
                }
                glEnd();
            }

            if(ds.texSize.x != 0)
            {
                glMatrixMode(GL_TEXTURE);
                glPopMatrix();
                glDisable(GL_TEXTURE_2D);
            }

#undef WRITESKYVERTEX
        }

        if(yflip)
        {
            glMatrixMode(GL_MODELVIEW);
            glPopMatrix();
        }
    }

    /**
     * The top row (row 0) is the one that's faded out.
     * There must be at least 4 columns. The preferable number is 4n, where
     * n is 1, 2, 3... There should be at least two rows because the first
     * one is always faded.
     *
     * The total number of triangles per hemisphere can be calculated thus:
     *
     * Sum: rows * columns * 2 + (hemisphere)
     *      rows * 2 + (fadeout)
     *      rows - 2 (cap)
     */
    void makeVertices(float height, float horizonOffset)
    {
        float const maxSideAngle = float(de::PI / 2 * height);

        horizonOffset = float(de::PI / 2 * horizonOffset);

        if(skySphereDetail < 1) skySphereDetail = 1;
        if(skySphereRows < 1) skySphereRows = 1;

        skySphereColumns = 4 * skySphereDetail;

        verts.resize(skySphereColumns * (skySphereRows + 1));

        // Calculate the vertices.
        for(int r = 0; r < skySphereRows + 1; ++r)
        for(int c = 0; c < skySphereColumns; ++c)
        {
            Vector3f &svtx = vertex(r, c);

            float topAngle = ((c / float(skySphereColumns)) *2) * PI;
            float sideAngle = horizonOffset + maxSideAngle * (skySphereRows - r) / float(skySphereRows);
            float realRadius = cos(sideAngle);

            svtx = Vector3f(realRadius * cos(topAngle),
                            sin(sideAngle), // The height.
                            realRadius * sin(topAngle));
        }
    }

    void rebuildIfNeeded(Sky const &sky)
    {
        // Rebuild our model if any parameters have changed.
        if(verts.isEmpty() || horizonOffset != sky.horizonOffset())
        {
            horizonOffset = sky.horizonOffset();
            needRebuild   = true;
        }
        if(verts.isEmpty() || height != sky.height())
        {
            height      = sky.height();
            needRebuild = true;
        }

        // Any work to do?
        if(!needRebuild) return;
        needRebuild = false;

        makeVertices(sky.height(), sky.horizonOffset());
    }
};

static Hemisphere hemisphere;

} // namespace internal
using namespace ::internal;

DENG2_PIMPL(SkyDrawable)
{
    ModelData models[MAX_SKY_MODELS];
    bool haveModels       = false;
    bool alwaysDrawSphere = false;

    Instance(Public *i) : Base(i)
    {
        de::zap(models);
    }

    static inline ResourceSystem &resSys() {
        return ClientApp::resourceSystem();
    }

    void drawSphere(Sky const &sky) const
    {
        if(haveModels && !alwaysDrawSphere) return;

        // We don't want anything written in the depth buffer.
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);

        // Disable culling, all triangles face the viewer.
        glDisable(GL_CULL_FACE);

        // Setup a proper matrix.
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glTranslatef(vOrigin.x, vOrigin.y, vOrigin.z);
        glScalef(skyDistance, skyDistance, skyDistance);

        // Always draw both hemispheres.
        hemisphere.draw(Hemisphere::LowerHemisphere, sky);
        hemisphere.draw(Hemisphere::UpperHemisphere, sky);

        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();

        // Restore assumed default GL state.
        glEnable(GL_CULL_FACE);

        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);
    }

    void drawModels(Sky const &sky) const
    {
        if(!haveModels) return;

        // We don't want anything written in the depth buffer.
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);

        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();

        // Setup basic translation.
        glTranslatef(vOrigin.x, vOrigin.y, vOrigin.z);

        for(int i = 0; i < MAX_SKY_MODELS; ++i)
        {
            ModelData const &mdata    = models[i];
            Record const *skyModelDef = mdata.def;

            if(!skyModelDef) continue;

            // If the associated sky layer is not active then the model won't be drawn.
            if(!sky.layer(skyModelDef->geti("layer")).isActive())
            {
                continue;
            }

            vissprite_t vis; de::zap(vis);

            vis.pose.origin          = vOrigin.xzy() * -Vector3f(skyModelDef->get("originOffset")).xzy();
            vis.pose.topZ            = vis.pose.origin.z;
            vis.pose.distance        = 1;

            Vector2f rotate(skyModelDef->get("rotate"));
            vis.pose.yaw             = mdata.yaw;
            vis.pose.extraYawAngle   = vis.pose.yawAngleOffset   = rotate.x;
            vis.pose.extraPitchAngle = vis.pose.pitchAngleOffset = rotate.y;

            drawmodelparams_t &visModel = *VS_MODEL(&vis);
            visModel.inter                       = (mdata.maxTimer > 0 ? mdata.timer / float(mdata.maxTimer) : 0);
            visModel.mf                          = mdata.model;
            visModel.alwaysInterpolate           = true;
            visModel.shineTranslateWithViewerPos = true;
            resSys().setModelDefFrame(*mdata.model, mdata.frame);

            vis.light.ambientColor = Vector4f(skyModelDef->get("color"), 1);

            Rend_DrawModel(vis);
        }

        // We don't want that anything interferes with what was drawn.
        //glClear(GL_DEPTH_BUFFER_BIT);

        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();

        // Restore assumed default GL state.
        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);
    }
};

SkyDrawable::SkyDrawable() : d(new Instance(this))
{}

void SkyDrawable::setupModels(defn::Sky const &def)
{
    // Normally the sky sphere is not drawn if models are in use.
    d->alwaysDrawSphere = (def.geti("flags") & SIF_DRAW_SPHERE) != 0;

    // The normal sphere is used if no models will be set up.
    d->haveModels = false;
    de::zap(d->models);

    for(int i = 0; i < def.modelCount(); ++i)
    {
        Record const &modef = def.model(i);
        ModelData *mdata = &d->models[i];

        // Is the model ID set?
        try
        {
            mdata->model = &d->resSys().modelDef(modef.gets("id"));
            if(!mdata->model->subCount())
            {
                continue;
            }

            // There is a model here.
            d->haveModels = true;

            mdata->def      = modef.accessedRecordPtr();
            mdata->maxTimer = int(TICSPERSEC * modef.getf("frameInterval"));
            mdata->yaw      = modef.getf("yaw");
            mdata->frame    = mdata->model->subModelDef(0).frame;
        }
        catch(ResourceSystem::MissingModelDefError const &)
        {} // Ignore this error.
    }
}

bool SkyDrawable::hasModel(int index) const
{
    return (index >= 0 && index < MAX_SKY_MODELS);
}

SkyDrawable::ModelData &SkyDrawable::model(int index)
{
    if(hasModel(index))
    {
        return d->models[index];
    }
    /// @throw MissingModelError An invalid model index was specified.
    throw MissingModelError("SkyDrawable::model", "Invalid model index #" + String::number(index) + ".");
}

SkyDrawable::ModelData const &SkyDrawable::model(int index) const
{
    return const_cast<ModelData const &>(const_cast<SkyDrawable *>(this)->model(index));
}

void SkyDrawable::cacheDrawableAssets(Sky const *sky)
{
    DENG2_ASSERT(sky);

    for(int i = 0; i < MAX_SKY_LAYERS; ++i)
    {
        Sky::Layer const &lyr = sky->layer(i);
        if(Material *mat = lyr.material())
        {
            d->resSys().cache(*mat, layerMaterialSpec(lyr.isMasked()));
        }
    }

    if(d->haveModels)
    {
        for(int i = 0; i < MAX_SKY_MODELS; ++i)
        {
            ModelData &mdata = d->models[i];
            if(!mdata.def) continue;

            d->resSys().cache(mdata.model);
        }
    }
}

void SkyDrawable::draw(Sky const *sky) const
{
    DENG2_ASSERT(sky);
    DENG2_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    // The sky is only drawn when at least one layer is active.
    if(sky->firstActiveLayer() < 0) return;

    if(usingFog) glEnable(GL_FOG);

    d->drawSphere(*sky);
    d->drawModels(*sky);

    if(usingFog) glDisable(GL_FOG);
}

MaterialVariantSpec const &SkyDrawable::layerMaterialSpec(bool masked) // static
{
    return Instance::resSys()
                .materialSpec(SkySphereContext, TSF_NO_COMPRESSION | (masked? TSF_ZEROMASK : 0),
                              0, 0, 0, GL_REPEAT, GL_CLAMP_TO_EDGE,
                              0, -1, -1, false, true, false, false);
}

namespace {
void markSkySphereForRebuild()
{
    // Defer this task until render time, when we can be sure we are in correct thread.
    hemisphere.needRebuild = true;
}
}

void SkyDrawable::consoleRegister() // static
{
    C_VAR_INT2 ("rend-sky-detail",   &skySphereDetail,  0,          3, 7, markSkySphereForRebuild);
    C_VAR_INT2 ("rend-sky-rows",     &skySphereRows,    0,          1, 8, markSkySphereForRebuild);
    C_VAR_FLOAT("rend-sky-distance", &skyDistance,      CVF_NO_MAX, 1, 0);
}

//---------------------------------------------------------------------------------------

DENG2_PIMPL_NOREF(SkyDrawable::Animator)
{
    SkyDrawable *sky;
    Instance(SkyDrawable *sky = 0) : sky(sky) {}
};

SkyDrawable::Animator::Animator() : d(new Instance)
{}

SkyDrawable::Animator::Animator(SkyDrawable &sky) : d(new Instance(&sky))
{}

SkyDrawable::Animator::~Animator()
{}

void SkyDrawable::Animator::setSky(SkyDrawable *sky)
{
    d->sky = sky;
}

SkyDrawable &SkyDrawable::Animator::sky() const
{
    DENG2_ASSERT(d->sky != 0);
    return *d->sky;
}

void SkyDrawable::Animator::advanceTime(timespan_t /*elapsed*/)
{
    LOG_AS("SkyDrawable::Animator");

    if(!d->sky) return;

    if(clientPaused) return;
    if(!DD_IsSharpTick()) return;

    // Animate layers.
    /*for(int i = 0; i < MAX_SKY_LAYERS; ++i)
    {
        Sky::Layer &lyr = sky().layer(i);
    }*/

    // Animate models.
    for(int i = 0; i < MAX_SKY_MODELS; ++i)
    {
        ModelData &mdata = sky().model(i);
        if(!mdata.def) continue;

        // Rotate the model.
        mdata.yaw += mdata.def->getf("yawSpeed") / TICSPERSEC;

        // Is it time to advance to the next frame?
        if(mdata.maxTimer > 0 && ++mdata.timer >= mdata.maxTimer)
        {
            mdata.frame++;
            mdata.timer = 0;

            // Execute a console command?
            String const execute = mdata.def->gets("execute");
            if(!execute.isEmpty())
            {
                Con_Execute(CMDS_SCRIPT, execute.toUtf8().constData(), true, false);
            }
        }
    }
}
