#include "entity_class_loader.h"

#include "assets/ucfb/chunk_reader.h"
#include "assets/ucfb/chunk_types.h"
#include "core/log.h"

#include <charconv>
#include <cstdlib>
#include <cstring>

namespace swbf {

// ---------------------------------------------------------------------------
// Sub-chunk FourCCs used inside entity class chunks
// ---------------------------------------------------------------------------

namespace {

constexpr FourCC TYPE = make_fourcc('T', 'Y', 'P', 'E');
constexpr FourCC BASE = make_fourcc('B', 'A', 'S', 'E');
constexpr FourCC PROP = make_fourcc('P', 'R', 'O', 'P');
constexpr FourCC NAME = make_fourcc('N', 'A', 'M', 'E');
constexpr FourCC VALU = make_fourcc('V', 'A', 'L', 'U');
constexpr FourCC DATA = make_fourcc('D', 'A', 'T', 'A');
constexpr FourCC INFO = make_fourcc('I', 'N', 'F', 'O');
constexpr FourCC HASH = make_fourcc('H', 'A', 'S', 'H');

} // anonymous namespace

// ---------------------------------------------------------------------------
// EntityClass accessors
// ---------------------------------------------------------------------------

std::string EntityClass::get(const std::string& key,
                              const std::string& default_val) const {
    for (const auto& prop : properties) {
        if (prop.key == key) {
            return prop.value;
        }
    }
    return default_val;
}

float EntityClass::get_float(const std::string& key, float default_val) const {
    std::string val = get(key);
    if (val.empty()) return default_val;

    char* end = nullptr;
    float result = std::strtof(val.c_str(), &end);
    if (end == val.c_str()) {
        return default_val;
    }
    return result;
}

int EntityClass::get_int(const std::string& key, int default_val) const {
    std::string val = get(key);
    if (val.empty()) return default_val;

    int result = default_val;
    auto [ptr, ec] = std::from_chars(val.data(), val.data() + val.size(), result);
    if (ec != std::errc{}) {
        return default_val;
    }
    return result;
}

// ---------------------------------------------------------------------------
// EntityClassLoader
// ---------------------------------------------------------------------------

EntityClassType EntityClassLoader::classify(FourCC id) {
    if (id == chunk_id::entc) return EntityClassType::Entity;
    if (id == chunk_id::ordc) return EntityClassType::Ordnance;
    if (id == chunk_id::wpnc) return EntityClassType::Weapon;
    if (id == chunk_id::expc) return EntityClassType::Explosion;
    return EntityClassType::Entity;
}

EntityProperty EntityClassLoader::parse_property(ChunkReader& prop) {
    EntityProperty result;

    while (prop.has_children()) {
        ChunkReader child = prop.next_child();

        if (child.id() == NAME) {
            result.key = child.read_string();
        } else if (child.id() == VALU) {
            result.value = child.read_string();
        }
    }

    return result;
}

EntityClass EntityClassLoader::load(ChunkReader& chunk) {
    EntityClass entity_class;
    entity_class.type = classify(chunk.id());

    // Munged entity class chunks may be double-wrapped:
    //   entc -> entc (inner) -> children
    // Or single-wrapped:
    //   entc -> children directly
    //
    // We handle both by checking if the first child has the same FourCC
    // as the parent. If so, unwrap one level.

    std::vector<ChunkReader> top_children = chunk.get_children();

    // Check for double-wrapping: if the first child has the same ID as
    // the parent, unwrap into that child's children.
    std::vector<ChunkReader> children;

    if (!top_children.empty() && top_children[0].id() == chunk.id()) {
        children = top_children[0].get_children();
    } else {
        children = std::move(top_children);
    }

    for (auto& child : children) {
        FourCC id = child.id();

        if (id == TYPE || id == HASH) {
            // Class type hash — uint32 FNV hash of the ClassLabel.
            if (child.remaining() >= 4) {
                entity_class.type_hash = child.read<uint32_t>();
            }
        }
        else if (id == NAME) {
            // Entity class name.
            entity_class.name = child.read_string();
        }
        else if (id == BASE) {
            // Parent class name (ClassParent).
            entity_class.base_class = child.read_string();
        }
        else if (id == PROP) {
            // Property key-value pair.
            EntityProperty prop = parse_property(child);
            if (!prop.key.empty()) {
                entity_class.properties.push_back(std::move(prop));
            }
        }
        else if (id == INFO) {
            // INFO chunk may contain the entity name or hash.
            // In some munged formats, INFO stores the name hash followed
            // by a string name.
            if (child.remaining() >= 4) {
                if (entity_class.type_hash == 0) {
                    entity_class.type_hash = child.read<uint32_t>();
                }
                if (child.remaining() > 0 && entity_class.name.empty()) {
                    try {
                        entity_class.name = child.read_string();
                    } catch (...) {
                        // Not a valid string — just skip.
                    }
                }
            }
        }
        else if (id == DATA) {
            // Some munged formats embed the full ODF text as a DATA blob.
            // Parse it using the ODF parser and extract properties.
            if (child.size() > 0) {
                std::vector<uint8_t> odf_data(child.data(),
                                              child.data() + child.size());
                ODFParser parser;
                ODFFile odf = parser.parse(odf_data);

                // Extract class metadata from the ODF.
                if (!odf.class_label.empty() && entity_class.type_hash == 0) {
                    // We don't have the hash, but store the label as a property.
                    entity_class.properties.push_back(
                        {"ClassLabel", odf.class_label});
                }
                if (!odf.class_parent.empty() && entity_class.base_class.empty()) {
                    entity_class.base_class = odf.class_parent;
                }
                if (!odf.geometry_name.empty()) {
                    entity_class.properties.push_back(
                        {"GeometryName", odf.geometry_name});
                }

                // Copy all ODF section properties.
                for (const auto& section : odf.sections) {
                    for (const auto& [key, value] : section.properties) {
                        // Skip the ones we've already extracted.
                        if (key == "ClassLabel" || key == "ClassParent" ||
                            key == "GeometryName") continue;
                        entity_class.properties.push_back({key, value});
                    }
                }
            }
        }
    }

    // If we got a name from ODF data but not from a NAME chunk, try to
    // extract it from a ClassLabel property.
    if (entity_class.name.empty()) {
        std::string label = entity_class.get("ClassLabel");
        if (!label.empty()) {
            entity_class.name = label;
        }
    }

    char type_str[5] = {};
    FourCC chunk_fourcc = chunk.id();
    std::memcpy(type_str, &chunk_fourcc, 4);
    LOG_DEBUG("EntityClassLoader: loaded %s '%s' (hash=0x%08X, base='%s', %zu props)",
              type_str, entity_class.name.c_str(), entity_class.type_hash,
              entity_class.base_class.c_str(), entity_class.properties.size());

    return entity_class;
}

} // namespace swbf
