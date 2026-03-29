#pragma once

#include <string>
#include <vector>

namespace swbf {

/// A single node in the scene graph.
///
/// Each node carries a 4x4 model-to-world transform (column-major, matching
/// OpenGL conventions) and an optional reference into the model array.
struct SceneNode {
    std::string    name;
    float          transform[16]; // 4x4 column-major model matrix
    int            model_index = -1; // index into model array, -1 = no model
    std::vector<int> children;       // indices into the node array
};

/// Flat-array scene graph.
///
/// Nodes are stored in a contiguous vector and reference each other by index.
/// Parent-child relationships are expressed through the children list in each
/// node.  There is no single root — top-level nodes simply have no parent that
/// lists them.
class SceneGraph {
public:
    /// Create a new node and return its index.
    /// @p transform  Pointer to 16 floats (column-major 4x4 matrix).
    ///               If nullptr, the node gets the identity matrix.
    /// @p model_index  Index into the model array, or -1 for a grouping node.
    int add_node(const std::string& name, const float* transform, int model_index = -1);

    /// Make @p child a child of @p parent.
    void set_parent(int child, int parent);

    /// Read-only access to a node.
    const SceneNode& get_node(int index) const;

    /// Mutable access to a node (for updating transforms, etc.).
    SceneNode& get_node_mut(int index);

    /// Number of nodes currently in the graph.
    size_t node_count() const;

    /// Remove all nodes.
    void clear();

private:
    std::vector<SceneNode> m_nodes;
};

} // namespace swbf
