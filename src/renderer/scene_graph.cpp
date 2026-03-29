#include "renderer/scene_graph.h"
#include "core/log.h"

#include <cassert>
#include <cstring>

namespace swbf {

// Column-major 4x4 identity matrix.
static const float s_identity[16] = {
    1, 0, 0, 0,
    0, 1, 0, 0,
    0, 0, 1, 0,
    0, 0, 0, 1
};

int SceneGraph::add_node(const std::string& name,
                         const float* transform,
                         int model_index) {
    SceneNode node;
    node.name = name;
    node.model_index = model_index;

    if (transform) {
        std::memcpy(node.transform, transform, 16 * sizeof(float));
    } else {
        std::memcpy(node.transform, s_identity, 16 * sizeof(float));
    }

    const int index = static_cast<int>(m_nodes.size());
    m_nodes.push_back(std::move(node));
    return index;
}

void SceneGraph::set_parent(int child, int parent) {
    assert(child  >= 0 && child  < static_cast<int>(m_nodes.size()));
    assert(parent >= 0 && parent < static_cast<int>(m_nodes.size()));
    assert(child != parent);

    m_nodes[static_cast<size_t>(parent)].children.push_back(child);
}

const SceneNode& SceneGraph::get_node(int index) const {
    assert(index >= 0 && index < static_cast<int>(m_nodes.size()));
    return m_nodes[static_cast<size_t>(index)];
}

SceneNode& SceneGraph::get_node_mut(int index) {
    assert(index >= 0 && index < static_cast<int>(m_nodes.size()));
    return m_nodes[static_cast<size_t>(index)];
}

size_t SceneGraph::node_count() const {
    return m_nodes.size();
}

void SceneGraph::clear() {
    m_nodes.clear();
}

} // namespace swbf
