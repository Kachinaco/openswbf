#pragma once

#include "assets/lvl/entity_class_loader.h"
#include "core/types.h"

#include <cstdint>
#include <string>
#include <vector>

namespace swbf {

class ChunkReader;

// ---------------------------------------------------------------------------
// World instance data structures
//
// The wrld chunk in a munged .lvl file defines every placed object in the
// map: buildings, props, command posts, spawn points, vehicles, turrets, etc.
//
// Each instance references an entity class (entc/ordc) by name and carries:
//   - A world-space transform (position + rotation, optionally scale)
//   - Instance-specific properties (key-value overrides)
//
// The wrld chunk hierarchy:
//
//   wrld
//     inst (repeated per placed object)
//       TYPE — class name string (references an entc/ordc class)
//       NAME — instance name string (unique identifier for this placement)
//       XFRM — transform data (rotation matrix 3x3 + position vec3, 48 bytes)
//       PROP (repeated) — instance properties
//         NAME — property key
//         VALU — property value
//
// The XFRM chunk stores a 4x3 column-major matrix:
//   [r00 r10 r20 r01 r11 r21 r02 r12 r22 tx ty tz]
//   = 3x3 rotation matrix (columns) followed by translation vector.
// ---------------------------------------------------------------------------

/// Transform for a world instance: 3x3 rotation + position.
struct WorldTransform {
    /// 3x3 rotation matrix, stored column-major:
    ///   rotation[0..2] = column 0 (right)
    ///   rotation[3..5] = column 1 (up)
    ///   rotation[6..8] = column 2 (forward)
    float rotation[9] = {
        1.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 1.0f
    };

    float position[3] = {0.0f, 0.0f, 0.0f};

    /// Build a column-major 4x4 matrix suitable for OpenGL.
    /// Output is 16 floats in column-major order.
    void to_mat4(float out[16]) const {
        // Column 0
        out[0]  = rotation[0];
        out[1]  = rotation[1];
        out[2]  = rotation[2];
        out[3]  = 0.0f;
        // Column 1
        out[4]  = rotation[3];
        out[5]  = rotation[4];
        out[6]  = rotation[5];
        out[7]  = 0.0f;
        // Column 2
        out[8]  = rotation[6];
        out[9]  = rotation[7];
        out[10] = rotation[8];
        out[11] = 0.0f;
        // Column 3 (translation)
        out[12] = position[0];
        out[13] = position[1];
        out[14] = position[2];
        out[15] = 1.0f;
    }
};

/// A single placed object instance in the world.
struct WorldInstance {
    std::string class_name;    // TYPE — references an entc/ordc class name
    std::string instance_name; // NAME — unique instance identifier

    WorldTransform transform;

    /// Instance-specific property overrides (same format as EntityProperty).
    std::vector<EntityProperty> properties;

    /// Look up a property value by key. Returns default_val if not found.
    std::string get(const std::string& key,
                    const std::string& default_val = "") const;
};

/// All world instance data parsed from a single wrld chunk.
struct WorldData {
    std::string name;
    std::vector<WorldInstance> instances;
};

// ---------------------------------------------------------------------------
// WorldLoader — parses wrld UCFB chunks from munged .lvl files.
//
// Usage:
//   WorldLoader loader;
//   WorldData world = loader.load(wrld_chunk);
// ---------------------------------------------------------------------------

class WorldLoader {
public:
    WorldLoader() = default;

    /// Parse a wrld chunk and return all decoded world instances.
    WorldData load(ChunkReader& chunk);

private:
    /// Parse a single inst sub-chunk into a WorldInstance.
    WorldInstance parse_instance(ChunkReader& inst);

    /// Parse a PROP container into an EntityProperty.
    EntityProperty parse_property(ChunkReader& prop);
};

} // namespace swbf
