#include "game/pathfinder.h"
#include "core/log.h"

#include <glm/geometric.hpp>
#include <glm/vec3.hpp>

#include <algorithm>
#include <cmath>
#include <queue>
#include <unordered_set>

namespace swbf {

// ============================================================================
// NavGraph
// ============================================================================

void NavGraph::clear() {
    m_nodes.clear();
    m_edges.clear();
    m_next_id = 0;
}

NavNodeId NavGraph::add_node(const glm::vec3& position, float radius) {
    NavNodeId id = m_next_id++;
    NavNode node;
    node.id       = id;
    node.position = position;
    node.radius   = radius;
    m_nodes[id]   = std::move(node);
    return id;
}

void NavGraph::add_edge(NavNodeId a, NavNodeId b, float cost) {
    add_directed_edge(a, b, cost);
    add_directed_edge(b, a, cost);
}

void NavGraph::add_directed_edge(NavNodeId from, NavNodeId to, float cost) {
    auto it_from = m_nodes.find(from);
    auto it_to   = m_nodes.find(to);
    if (it_from == m_nodes.end() || it_to == m_nodes.end()) {
        LOG_WARN("NavGraph::add_directed_edge — invalid node id(s) %u -> %u",
                 from, to);
        return;
    }

    // Compute cost from distance if not provided.
    if (cost <= 0.0f) {
        cost = glm::distance(it_from->second.position, it_to->second.position);
    }

    u32 edge_index = static_cast<u32>(m_edges.size());

    NavEdge edge;
    edge.from = from;
    edge.to   = to;
    edge.cost = cost;
    m_edges.push_back(edge);

    it_from->second.edge_indices.push_back(edge_index);
}

const NavNode* NavGraph::get_node(NavNodeId id) const {
    auto it = m_nodes.find(id);
    return (it != m_nodes.end()) ? &it->second : nullptr;
}

NavNodeId NavGraph::find_nearest_node(const glm::vec3& position) const {
    if (m_nodes.empty()) {
        return 0xFFFFFFFFu;
    }

    NavNodeId best_id   = 0xFFFFFFFFu;
    float     best_dist = std::numeric_limits<float>::max();

    for (const auto& [id, node] : m_nodes) {
        float d = glm::distance(position, node.position);
        if (d < best_dist) {
            best_dist = d;
            best_id   = id;
        }
    }

    return best_id;
}

void NavGraph::generate_from_terrain(const PhysicsWorld& physics,
                                     const glm::vec3& world_min,
                                     const glm::vec3& world_max,
                                     float spacing,
                                     float max_slope) {
    clear();

    if (spacing <= 0.0f) {
        LOG_WARN("NavGraph::generate_from_terrain — invalid spacing %.2f",
                 static_cast<double>(spacing));
        return;
    }

    const float sample_radius = spacing * 0.25f;

    // Determine grid dimensions.
    int cols = static_cast<int>((world_max.x - world_min.x) / spacing) + 1;
    int rows = static_cast<int>((world_max.z - world_min.z) / spacing) + 1;

    if (cols <= 0 || rows <= 0) {
        LOG_WARN("NavGraph::generate_from_terrain — degenerate area");
        return;
    }

    // Phase 1: Create nodes at walkable grid positions.
    // Store node ids in a 2D grid for easy neighbor lookup.
    // 0xFFFFFFFF means "no node at this cell" (too steep / unwalkable).
    std::vector<NavNodeId> grid(static_cast<size_t>(rows * cols), 0xFFFFFFFFu);

    int nodes_created = 0;

    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            float x = world_min.x + static_cast<float>(c) * spacing;
            float z = world_min.z + static_cast<float>(r) * spacing;
            float y = physics.get_terrain_height(x, z);

            glm::vec3 pos(x, y, z);

            if (is_walkable(physics, pos, max_slope, sample_radius)) {
                NavNodeId id = add_node(pos, spacing * 0.5f);
                grid[static_cast<size_t>(r * cols + c)] = id;
                ++nodes_created;
            }
        }
    }

    // Phase 2: Connect adjacent walkable nodes.
    // 8-connected grid: horizontal, vertical, and diagonal neighbors.
    static const int dx[] = { 1, 1, 0, -1, -1, -1,  0,  1 };
    static const int dz[] = { 0, 1, 1,  1,  0, -1, -1, -1 };

    int edges_created = 0;

    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            NavNodeId from_id = grid[static_cast<size_t>(r * cols + c)];
            if (from_id == 0xFFFFFFFFu) continue;

            for (int d = 0; d < 8; ++d) {
                int nr = r + dz[d];
                int nc = c + dx[d];
                if (nr < 0 || nr >= rows || nc < 0 || nc >= cols) continue;

                NavNodeId to_id = grid[static_cast<size_t>(nr * cols + nc)];
                if (to_id == 0xFFFFFFFFu) continue;

                // Only add forward direction — the reverse will be added
                // when we process the neighbor cell.  Use directed edge to
                // avoid double-adding.
                add_directed_edge(from_id, to_id);
                ++edges_created;
            }
        }
    }

    LOG_INFO("NavGraph — generated %d nodes, %d edges (spacing %.1f, max_slope %.1f)",
             nodes_created, edges_created,
             static_cast<double>(spacing), static_cast<double>(max_slope));
}

// ============================================================================
// A* pathfinding
// ============================================================================

PathResult find_path(const NavGraph& graph,
                     const glm::vec3& start,
                     const glm::vec3& goal) {
    NavNodeId start_node = graph.find_nearest_node(start);
    NavNodeId goal_node  = graph.find_nearest_node(goal);

    if (start_node == 0xFFFFFFFFu || goal_node == 0xFFFFFFFFu) {
        return {};  // No path — graph is empty or positions are unreachable.
    }

    return find_path(graph, start_node, goal_node);
}

PathResult find_path(const NavGraph& graph,
                     NavNodeId start_node,
                     NavNodeId goal_node) {
    PathResult result;

    const NavNode* start_n = graph.get_node(start_node);
    const NavNode* goal_n  = graph.get_node(goal_node);
    if (!start_n || !goal_n) {
        return result;
    }

    // Trivial case: start == goal.
    if (start_node == goal_node) {
        result.found      = true;
        result.total_cost = 0.0f;
        result.waypoints.push_back(start_n->position);
        return result;
    }

    // A* data structures.
    struct OpenEntry {
        float     f_score;
        NavNodeId node_id;
        bool operator>(const OpenEntry& o) const { return f_score > o.f_score; }
    };

    std::priority_queue<OpenEntry, std::vector<OpenEntry>,
                        std::greater<OpenEntry>> open_set;

    // g_score: cost of cheapest known path from start to node.
    std::unordered_map<NavNodeId, float>     g_score;
    // came_from: for path reconstruction.
    std::unordered_map<NavNodeId, NavNodeId> came_from;
    // closed: already fully evaluated.
    std::unordered_set<NavNodeId>            closed;

    const glm::vec3& goal_pos = goal_n->position;

    // Heuristic: Euclidean distance to goal.
    auto heuristic = [&goal_pos, &graph](NavNodeId id) -> float {
        const NavNode* n = graph.get_node(id);
        return n ? glm::distance(n->position, goal_pos) : 0.0f;
    };

    g_score[start_node] = 0.0f;
    open_set.push({ heuristic(start_node), start_node });

    const auto& edges = graph.edges();

    // Safety limit to prevent runaway searches on huge graphs.
    constexpr int MAX_ITERATIONS = 100000;
    int iterations = 0;

    while (!open_set.empty() && iterations < MAX_ITERATIONS) {
        ++iterations;

        OpenEntry current = open_set.top();
        open_set.pop();

        if (current.node_id == goal_node) {
            // Reconstruct path.
            result.found      = true;
            result.total_cost = g_score[goal_node];

            std::vector<NavNodeId> id_path;
            NavNodeId cur = goal_node;
            while (cur != start_node) {
                id_path.push_back(cur);
                cur = came_from[cur];
            }
            id_path.push_back(start_node);
            std::reverse(id_path.begin(), id_path.end());

            result.waypoints.reserve(id_path.size());
            for (NavNodeId id : id_path) {
                const NavNode* n = graph.get_node(id);
                if (n) result.waypoints.push_back(n->position);
            }

            return result;
        }

        if (closed.count(current.node_id)) continue;
        closed.insert(current.node_id);

        const NavNode* cur_node = graph.get_node(current.node_id);
        if (!cur_node) continue;

        float cur_g = g_score[current.node_id];

        for (u32 edge_idx : cur_node->edge_indices) {
            const NavEdge& edge = edges[edge_idx];
            NavNodeId neighbor = edge.to;

            if (closed.count(neighbor)) continue;

            float tentative_g = cur_g + edge.cost;

            auto it = g_score.find(neighbor);
            if (it == g_score.end() || tentative_g < it->second) {
                g_score[neighbor]  = tentative_g;
                came_from[neighbor] = current.node_id;

                float f = tentative_g + heuristic(neighbor);
                open_set.push({ f, neighbor });
            }
        }
    }

    // No path found.
    return result;
}

// ============================================================================
// Terrain walkability
// ============================================================================

float terrain_slope_at(const PhysicsWorld& physics,
                       float x, float z,
                       float sample_radius) {
    // Sample terrain height at 4 points around (x, z) to estimate gradient.
    float h_px = physics.get_terrain_height(x + sample_radius, z);
    float h_mx = physics.get_terrain_height(x - sample_radius, z);
    float h_pz = physics.get_terrain_height(x, z + sample_radius);
    float h_mz = physics.get_terrain_height(x, z - sample_radius);

    // Finite-difference gradient.
    float dh_dx = (h_px - h_mx) / (2.0f * sample_radius);
    float dh_dz = (h_pz - h_mz) / (2.0f * sample_radius);

    // Slope magnitude = atan(gradient magnitude) in degrees.
    float gradient_mag = std::sqrt(dh_dx * dh_dx + dh_dz * dh_dz);
    return std::atan(gradient_mag) * (180.0f / 3.14159265358979f);
}

bool is_walkable(const PhysicsWorld& physics,
                 const glm::vec3& position,
                 float max_slope_degrees,
                 float sample_radius) {
    float slope = terrain_slope_at(physics, position.x, position.z, sample_radius);
    return slope <= max_slope_degrees;
}

// ============================================================================
// PathFollower
// ============================================================================

void PathFollower::set_path(const std::vector<glm::vec3>& waypoints) {
    m_waypoints     = waypoints;
    m_current_index = 0;
    m_active        = !waypoints.empty();
}

void PathFollower::clear() {
    m_waypoints.clear();
    m_current_index = 0;
    m_active        = false;
}

bool PathFollower::is_active() const {
    return m_active;
}

bool PathFollower::is_finished() const {
    return !m_active && !m_waypoints.empty();
}

glm::vec3 PathFollower::current_waypoint() const {
    if (!m_active || m_current_index >= m_waypoints.size()) {
        return glm::vec3(0.0f);
    }
    return m_waypoints[m_current_index];
}

size_t PathFollower::remaining_waypoints() const {
    if (!m_active || m_current_index >= m_waypoints.size()) return 0;
    return m_waypoints.size() - m_current_index;
}

glm::vec3 PathFollower::compute_steering(const glm::vec3& current_pos,
                                          float max_speed,
                                          float dt,
                                          float arrival_dist) {
    (void)dt;  // dt reserved for future smoothing/prediction.

    if (!m_active || m_current_index >= m_waypoints.size()) {
        return glm::vec3(0.0f);
    }

    const glm::vec3& target = m_waypoints[m_current_index];
    glm::vec3 to_target = target - current_pos;

    // Project onto horizontal plane for distance check (y is up).
    float horiz_dist = std::sqrt(to_target.x * to_target.x +
                                 to_target.z * to_target.z);

    // Advance to next waypoint if close enough.
    while (horiz_dist < arrival_dist) {
        ++m_current_index;
        if (m_current_index >= m_waypoints.size()) {
            m_active = false;
            return glm::vec3(0.0f);
        }
        to_target  = m_waypoints[m_current_index] - current_pos;
        horiz_dist = std::sqrt(to_target.x * to_target.x +
                               to_target.z * to_target.z);
    }

    // Compute desired velocity toward the current waypoint.
    glm::vec3 direction = glm::normalize(to_target);

    // Arrival behavior: slow down when approaching the final waypoint.
    float speed = max_speed;
    if (m_current_index == m_waypoints.size() - 1) {
        float slow_radius = arrival_dist * 3.0f;
        if (horiz_dist < slow_radius) {
            speed = max_speed * (horiz_dist / slow_radius);
            // Clamp to a minimum creep speed to avoid stalling.
            float min_speed = max_speed * 0.1f;
            if (speed < min_speed) speed = min_speed;
        }
    }

    return direction * speed;
}

// ============================================================================
// Dynamic obstacle avoidance
// ============================================================================

glm::vec3 compute_avoidance(const glm::vec3& entity_pos,
                            const glm::vec3& desired_vel,
                            const std::vector<Obstacle>& obstacles,
                            float avoidance_dist) {
    glm::vec3 avoidance(0.0f, 0.0f, 0.0f);

    if (obstacles.empty()) return avoidance;

    float speed = glm::length(desired_vel);
    if (speed < 0.001f) return avoidance;

    glm::vec3 forward = desired_vel / speed;

    for (const auto& obs : obstacles) {
        glm::vec3 to_obs = obs.position - entity_pos;

        // Only consider obstacles on the XZ plane (ignore height differences
        // beyond a reasonable threshold).
        to_obs.y = 0.0f;

        float dist = glm::length(to_obs);
        float combined_radius = obs.radius + 0.5f;  // Entity radius ~0.5m.

        // Skip obstacles that are too far away.
        if (dist > avoidance_dist + combined_radius) continue;
        // Skip obstacles that are behind us.
        if (glm::dot(to_obs, forward) < 0.0f) continue;

        // Project obstacle onto the forward axis to check lateral distance.
        float forward_proj = glm::dot(to_obs, forward);
        glm::vec3 closest_on_line = forward * forward_proj;
        glm::vec3 lateral_offset  = to_obs - closest_on_line;
        float lateral_dist = glm::length(lateral_offset);

        // If the obstacle is within the collision corridor, push away.
        if (lateral_dist < combined_radius) {
            // Determine which side to steer to.
            glm::vec3 push_dir;
            if (lateral_dist < 0.01f) {
                // Obstacle is directly ahead — steer to the right.
                push_dir = glm::vec3(-forward.z, 0.0f, forward.x);
            } else {
                push_dir = -glm::normalize(lateral_offset);
            }

            // Strength inversely proportional to distance.
            float strength = 1.0f - (dist / (avoidance_dist + combined_radius));
            strength = std::max(0.0f, strength);

            avoidance += push_dir * strength * speed;
        }
    }

    return avoidance;
}

// ============================================================================
// Pathfinder (high-level façade)
// ============================================================================

bool Pathfinder::init() {
    LOG_INFO("Pathfinder::init");
    return true;
}

void Pathfinder::shutdown() {
    m_graph.clear();
    m_physics = nullptr;
    LOG_INFO("Pathfinder::shutdown");
}

void Pathfinder::build_from_terrain(const PhysicsWorld& physics,
                                    const glm::vec3& world_min,
                                    const glm::vec3& world_max,
                                    float spacing,
                                    float max_slope) {
    m_physics = &physics;
    m_graph.generate_from_terrain(physics, world_min, world_max,
                                  spacing, max_slope);
}

PathResult Pathfinder::request_path(const glm::vec3& from,
                                    const glm::vec3& to) const {
    return find_path(m_graph, from, to);
}

} // namespace swbf
