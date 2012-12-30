/** @file materialvariant.h Logical Material Variant.
 *
 * @author Copyright &copy; 2011-2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_RESOURCE_MATERIALVARIANT_H
#define LIBDENG_RESOURCE_MATERIALVARIANT_H

/// Material (Usage) Context identifiers.
typedef enum materialcontext_e {
    MC_UNKNOWN = -1,
    MATERIALCONTEXT_FIRST = 0,
    MC_UI = MATERIALCONTEXT_FIRST,
    MC_MAPSURFACE,
    MC_SPRITE,
    MC_MODELSKIN,
    MC_PSPRITE,
    MC_SKYSPHERE,
    MATERIALCONTEXT_LAST = MC_SKYSPHERE
} materialcontext_t;

#define MATERIALCONTEXT_COUNT (MATERIALCONTEXT_LAST + 1 - MATERIALCONTEXT_FIRST )

/// @c true= val can be interpreted as a valid material context identifier.
#define VALID_MATERIALCONTEXT(val) ((val) >= MATERIALCONTEXT_FIRST && (val) <= MATERIALCONTEXT_LAST)

struct texturevariantspecification_s;

typedef struct materialvariantspecification_s {
    enum materialcontext_e context;
    struct texturevariantspecification_s *primarySpec;
} materialvariantspecification_t;

#ifdef __cplusplus

#include "def_data.h"
#include "resource/materialsnapshot.h"
#include "resource/texture.h"

struct material_s;

namespace de {

/**
 * @ingroup resource
 */
class MaterialVariant
{
public:
    /// Maximum number of layers a material supports.
    static int const max_layers = DDMAX_MATERIAL_LAYERS;

    struct Layer
    {
        /// @c -1 => layer not in use.
        int stage;

        /// Texture of the layer.
        Texture *texture;

        /// Origin of the layer in material-space.
        float texOrigin[2];

        /// Glow strength for the layer.
        float glow;

        /// Current frame?
        short tics;
    };

public:
    /// The requested layer does not exist. @ingroup errors
    DENG2_ERROR(InvalidLayerError);

public:
    MaterialVariant(struct material_s &generalCase, materialvariantspecification_t const &spec,
                    ded_material_t const &def);
    ~MaterialVariant();

    /**
     * Process a system tick event.
     * @param time  Length of the tick in seconds.
     */
    void ticker(timespan_t time);

    /**
     * Reset the staged animation point for this Material.
     */
    void resetAnim();

    /// @return  Material from which this variant is derived.
    struct material_s &generalCase() const;

    /// @return  MaterialVariantSpecification from which this variant is derived.
    materialvariantspecification_t const &spec() const;

    /**
     * Retrieve a handle for a staged animation layer form this variant.
     * @param layer  Index of the layer to retrieve.
     * @return  MaterialVariantLayer for the specified layer index.
     */
    Layer const &layer(int layer);

    /**
     * Attach MaterialSnapshot data to this. MaterialVariant is given ownership of @a materialSnapshot.
     * @return  Same as @a materialSnapshot for caller convenience.
     */
    MaterialSnapshot &attachSnapshot(MaterialSnapshot &snapshot);

    /**
     * Detach MaterialSnapshot data from this. Ownership of the data is relinquished to the caller.
     */
    MaterialSnapshot *detachSnapshot();

    /// @return  MaterialSnapshot data associated with this.
    MaterialSnapshot *snapshot() const;

    /// @return  Frame count when the snapshot was last prepared/updated.
    int snapshotPrepareFrame() const;

    /**
     * Change the frame when the snapshot was last prepared/updated.
     * @param frame  Frame to mark the snapshot with.
     */
    void setSnapshotPrepareFrame(int frame);

    /// @return  Translated 'next' (or target) MaterialVariant if set, else this.
    MaterialVariant *translationNext();

    /// @return  Translated 'current' MaterialVariant if set, else this.
    MaterialVariant *translationCurrent();

    /// @return  Translation position [0..1]
    float translationPoint();

    /**
     * Change the translation target for this variant.
     *
     * @param current  Translated 'current' MaterialVariant.
     * @param next  Translated 'next' (or target) MaterialVariant.
     */
    void setTranslation(MaterialVariant *current, MaterialVariant *next);

    /**
     * Change the translation point for this variant.
     * @param inter  Translation point.
     */
    void setTranslationPoint(float inter);

private:
    struct Instance;
    Instance *d;
};

} // namespace de

#endif // __cplusplus

struct materialvariant_s; // The materialvariant instance (opaque).

#endif /* LIBDENG_RESOURCE_MATERIALVARIANT_H */
