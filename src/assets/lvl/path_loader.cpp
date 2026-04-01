#include "path_loader.h"

#include "assets/ucfb/chunk_reader.h"
#include "assets/ucfb/chunk_types.h"
#include "core/log.h"

#include <cstring>

namespace swbf {

// ---------------------------------------------------------------------------
// Sub-chunk FourCCs used inside plan / PATH chunks
// ---------------------------------------------------------------------------

namespace {

constexpr FourCC INFO = make_fourcc('I', 'N', 'F', 'O');
constexpr FourCC NAME = make_fourcc('N', 'A', 'M', 'E');
constexpr FourCC NODE = make_fourcc('N', 'O', 'D', 'E');
constexpr FourCC ARCS = make_fourcc('A', 'R', 'C', 'S');
constexpr FourCC DATA = make_fourcc('D', 'A', 'T', 'A');
constexpr FourCC TYPE = make_fourcc('T', 'Y', 'P', 'E');
constexpr FourCC POSN = make_fourcc('P', 'O', 'S', 'N');
constexpr FourCC PROP = make_fourcc('P', 'R', 'O', 'P');

} // anonymous namespace

// ---------------------------------------------------------------------------
// PathNetwork accessor
// ---------------------------------------------------------------------------

int PathNetwork::find_node(const std::string& name) const {
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        if (nodes[i].name == name) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

// ---------------------------------------------------------------------------
// parse_node — parse a single NODE sub-chunk
//
// NODE chunk layout (inside plan):
//
//   NODE
//     INFO — node metadata:
//       offset 0: float position_x
//       offset 4: float position_y
//       offset 8: float position_z
//       offset 12: float radius
//       offset 16: uint32 flags (AI access flags)
//       offset 20: uint32 node_type (hub type)
//
//     NAME — node name (null-terminated)
//
//     ARCS — connection data:
//       offset 0: uint32 arc_count
//       per arc (12 bytes):
//         offset 0: uint32 target_node_index
//         offset 4: uint32 flags
//         offset 8: float weight
// ---------------------------------------------------------------------------

PathNode PathLoader::parse_node(ChunkReader& node_chunk) {
    PathNode node;

    while (node_chunk.has_children()) {
        ChunkReader child = node_chunk.next_child();
        FourCC id = child.id();

        if (id == INFO) {
            // Node metadata — position, radius, flags, type.
            if (child.remaining() >= 12) {
                node.position[0] = child.read<float>();
                node.position[1] = child.read<float>();
                node.position[2] = child.read<float>();
            }
            if (child.remaining() >= 4) {
                node.radius = child.read<float>();
            }
            if (child.remaining() >= 4) {
                node.flags = child.read<uint32_t>();
            }
            if (child.remaining() >= 4) {
                node.type = child.read<uint32_t>();
            }
        }
        else if (id == NAME) {
            node.name = child.read_string();
        }
        else if (id == ARCS) {
            // Connection data.
            if (child.remaining() >= 4) {
                uint32_t arc_count = child.read<uint32_t>();
                node.arcs.reserve(arc_count);

                for (uint32_t i = 0; i < arc_count; ++i) {
                    if (child.remaining() < 12) break;

                    PathArc arc;
                    arc.target_node = child.read<uint32_t>();
                    arc.flags       = child.read<uint32_t>();
                    arc.weight      = child.read<float>();
                    node.arcs.push_back(arc);
                }
            }
        }
        else if (id == DATA) {
            // Alternate compact format: all node data packed in DATA.
            // offset 0: float pos[3]
            // offset 12: float radius
            if (child.remaining() >= 12) {
                node.position[0] = child.read<float>();
                node.position[1] = child.read<float>();
                node.position[2] = child.read<float>();
            }
            if (child.remaining() >= 4) {
                node.radius = child.read<float>();
            }
        }
        else if (id == PROP) {
            // Property data — may contain additional node metadata.
            // Try to read key-value pairs.
            while (child.has_children()) {
                ChunkReader prop_child = child.next_child();
                if (prop_child.id() == NAME) {
                    std::string key = prop_child.read_string();
                    // Read the value from the next sibling if available.
                    // Some formats pack key and value in separate sub-chunks.
                    (void)key; // Store if needed for extended node properties.
                }
            }
        }
    }

    return node;
}

// ---------------------------------------------------------------------------
// load — main entry point: parse a plan or PATH chunk
//
// plan chunk hierarchy:
//
//   plan
//     INFO (4 bytes) — uint32 node count
//     NODE (repeated) — individual path nodes
//       INFO — position, radius, flags
//       NAME — node name
//       ARCS — connections to other nodes
//
// PATH chunk hierarchy (simpler waypoint format):
//
//   PATH
//     INFO — node count and path properties
//     NAME — path name
//     NODE (repeated) — simple waypoints
//       DATA — position + radius packed
// ---------------------------------------------------------------------------

PathNetwork PathLoader::load(ChunkReader& chunk) {
    PathNetwork network;

    FourCC chunk_id_val = chunk.id();
    if (chunk_id_val != chunk_id::plan && chunk_id_val != chunk_id::PATH) {
        LOG_WARN("PathLoader: expected plan or PATH chunk, got 0x%08X", chunk_id_val);
        return network;
    }

    uint32_t expected_count = 0;

    std::vector<ChunkReader> children = chunk.get_children();

    for (auto& child : children) {
        FourCC id = child.id();

        if (id == INFO) {
            // Node count and optional metadata.
            if (child.remaining() >= 4) {
                expected_count = child.read<uint32_t>();
                network.nodes.reserve(expected_count);
            }
        }
        else if (id == NAME) {
            network.name = child.read_string();
        }
        else if (id == NODE) {
            PathNode node = parse_node(child);
            network.nodes.push_back(std::move(node));
        }
        else if (id == DATA && chunk_id_val == chunk_id::PATH) {
            // Simple PATH format: all nodes packed in a single DATA chunk.
            // Each node is 16 bytes: float pos[3] + float radius.
            while (child.remaining() >= 16) {
                PathNode node;
                node.position[0] = child.read<float>();
                node.position[1] = child.read<float>();
                node.position[2] = child.read<float>();
                node.radius      = child.read<float>();

                // Generate sequential connections (waypoint chain).
                if (!network.nodes.empty()) {
                    PathArc arc;
                    arc.target_node = static_cast<uint32_t>(network.nodes.size());
                    arc.weight = 1.0f;
                    network.nodes.back().arcs.push_back(arc);
                }

                network.nodes.push_back(std::move(node));
            }
        }
    }

    LOG_DEBUG("PathLoader: loaded '%s' with %zu nodes (expected %u)",
              network.name.c_str(), network.nodes.size(), expected_count);

    return network;
}

} // namespace swbf
