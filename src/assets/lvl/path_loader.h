#pragma once

#include "core/types.h"

#include <cstdint>
#include <string>
#include <vector>

namespace swbf {

class ChunkReader;

// ---------------------------------------------------------------------------
// Path / AI navigation data structures
//
// SWBF uses a node-based AI navigation system. The world editor defines
// planning paths that the AI uses for movement, attack routes, and patrol.
//
// In munged .lvl files, path data appears in plan and PATH chunks:
//
//   plan
//     INFO  — hub/node count
//     NODE (repeated) — per-node data
//       INFO  — node metadata (position, radius)
//       ARCS  — connections to other nodes (edges)
//
//   PATH (less common, simpler waypoint path)
//     INFO  — node count and path properties
//     NODE (repeated) — position + properties
//
// Each node stores:
//   - A 3D position
//   - A radius (for area coverage)
//   - Connection indices to other nodes
//   - Property flags (e.g., hub type, access flags)
// ---------------------------------------------------------------------------

/// Flags describing what types of AI can use a path node.
enum class PathNodeFlags : uint32_t {
    None       = 0,
    Infantry   = (1 << 0),
    Light      = (1 << 1),   // Light vehicles
    Medium     = (1 << 2),   // Medium vehicles
    Heavy      = (1 << 3),   // Heavy vehicles
    Flyer      = (1 << 4),   // Air vehicles
    Hover      = (1 << 5),   // Hover vehicles
    All        = 0xFFFFFFFF
};

/// An edge (arc) connecting two path nodes.
struct PathArc {
    uint32_t target_node = 0;     // Index of the target node
    uint32_t flags       = 0;     // Access flags for this connection
    float    weight      = 1.0f;  // Traversal cost / weight
};

/// A single AI path node (hub).
struct PathNode {
    std::string name;
    float    position[3] = {0.0f, 0.0f, 0.0f};
    float    radius       = 5.0f;  // Area of influence
    uint32_t flags        = 0;      // PathNodeFlags combination
    uint32_t type         = 0;      // Hub type (0=normal, 1=command post, etc.)

    std::vector<PathArc> arcs;     // Connections to other nodes
};

/// A fully parsed path/planning network.
struct PathNetwork {
    std::string            name;   // Network/plan name
    std::vector<PathNode>  nodes;  // All nodes in the network

    /// Find a node by name. Returns -1 if not found.
    int find_node(const std::string& name) const;
};

// ---------------------------------------------------------------------------
// PathLoader — parses plan and PATH UCFB chunks from munged .lvl files.
//
// Usage:
//   PathLoader loader;
//   PathNetwork network = loader.load(plan_chunk);
// ---------------------------------------------------------------------------

class PathLoader {
public:
    PathLoader() = default;

    /// Parse a plan or PATH chunk and return the decoded path network.
    PathNetwork load(ChunkReader& chunk);

private:
    /// Parse a single NODE sub-chunk.
    PathNode parse_node(ChunkReader& node_chunk);
};

} // namespace swbf
