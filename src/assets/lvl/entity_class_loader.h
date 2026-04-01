#pragma once

#include "assets/odf/odf_parser.h"
#include "core/types.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace swbf {

class ChunkReader;

// ---------------------------------------------------------------------------
// EntityClass — a parsed entity/object class definition from a .lvl chunk.
//
// In SWBF, entity classes are defined by ODF (Object Definition File) data
// that is munged into the .lvl binary as entc/ordc/wpnc/expc chunks.
//
// Each entity class chunk contains:
//   - A class type hash (FNV hash of the class label)
//   - The class name (e.g. "rep_inf_ep3_rifleman")
//   - ODF property data (either embedded text or pre-parsed key-value pairs)
//
// The chunk hierarchy for entity classes in munged .lvl:
//
//   entc / ordc / wpnc / expc
//     TYPE  — uint32 class type hash (FNV hash of ClassLabel)
//     BASE  — parent class name (ClassParent)
//     PROP  — property key-value pairs (NAME + VALUE sub-chunks)
// ---------------------------------------------------------------------------

/// The category of entity class, determined by the chunk FourCC.
enum class EntityClassType : uint8_t {
    Entity    = 0,   // entc — standard game objects (soldiers, vehicles, props)
    Ordnance  = 1,   // ordc — projectiles (bolts, missiles, grenades)
    Weapon    = 2,   // wpnc — weapon definitions
    Explosion = 3,   // expc — explosion effects
};

/// A single key-value property from the entity class definition.
struct EntityProperty {
    std::string key;
    std::string value;
};

/// A fully parsed entity class definition.
struct EntityClass {
    std::string       name;           // Class name (e.g. "rep_inf_ep3_rifleman")
    EntityClassType   type = EntityClassType::Entity;
    uint32_t          type_hash = 0;  // FNV hash of the ClassLabel
    std::string       base_class;     // Parent class name (ClassParent), may be empty

    /// All properties in order of appearance.
    std::vector<EntityProperty> properties;

    /// Look up a property value by key. Returns default_val if not found.
    std::string get(const std::string& key,
                    const std::string& default_val = "") const;

    /// Look up a float property. Returns default_val on missing/parse failure.
    float get_float(const std::string& key, float default_val = 0.0f) const;

    /// Look up an int property. Returns default_val on missing/parse failure.
    int get_int(const std::string& key, int default_val = 0) const;
};

// ---------------------------------------------------------------------------
// EntityClassLoader — parses entc/ordc/wpnc/expc UCFB chunks.
//
// Munged entity class chunk structure:
//
//   entc (or ordc, wpnc, expc)
//     entc (inner container — some formats double-wrap)
//       TYPE (4 bytes)  — uint32 FNV hash of ClassLabel
//       BASE            — null-terminated parent class name
//       PROP (repeated) — property container
//         NAME          — property key (null-terminated string)
//         VALU          — property value (null-terminated string)
//
// Some munged formats store properties as a single DATA blob containing
// the full ODF text, which is then parsed by the ODF parser.
// ---------------------------------------------------------------------------

class EntityClassLoader {
public:
    EntityClassLoader() = default;

    /// Parse an entity class from an entc/ordc/wpnc/expc chunk.
    EntityClass load(ChunkReader& chunk);

private:
    /// Determine the EntityClassType from the chunk FourCC.
    static EntityClassType classify(FourCC id);

    /// Parse a PROP container chunk into a property key-value pair.
    EntityProperty parse_property(ChunkReader& prop);
};

} // namespace swbf
