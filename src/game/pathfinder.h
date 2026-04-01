#pragma once

#include "core/types.h"
#include "physics/physics_world.h"

#include <glm/vec3.hpp>

#include <cstddef>
#include <functional>
#include <limits>
#include <string>
#include <unordered_map>
#include <vector>

namespace swbf {

// ============================================================================
// Navigation graph — nodes and edges for pathfinding
// ============================================================================

/// Unique identifier for a navigation node.
using NavNodeId = u32;

/// A single node in the navigation graph.
struct NavNode {
    NavNodeId     id       = 0;
    glm::vec3     position = {0.0f, 0.0f, 0.0f};
    float         radius   = 1.0f;  ///< Walkable radius around the node.

    /// Indices into NavGraph::m_edges for outgoing connections.
    std::vector<u32> edge_indices;
};

/// A directed edge between two navigation nodes.
struct NavEdge {
    NavNodeId from    = 0;
    NavNodeId to      = 0;
    float     cost    = 1.0f;  ///< Traversal cost (typically Euclidean distance).
};

/// Navigation graph that stores the topology AI uses for pathfinding.
///
/// Nodes can be loaded from .lvl planning data or auto-generated from the
/// terrain height grid.  The graph supports both directed and undirected
/// edges (undirected edges are stored as two directed edges).
class NavGraph {
public:
    // -- Construction --------------------------------------------------------

    /// Remove all nodes and edges.
    void clear();

    /// Add a node at the given position.  Returns its id.
    NavNodeId add_node(const glm::vec3& position, float radius = 1.0f);

    /// Add a bidirectional edge between two existing nodes.
    /// Cost defaults to Euclidean distance if <= 0.
    void add_edge(NavNodeId a, NavNodeId b, float cost = -1.0f);

    /// Add a directed edge from @p from to @p to.
    void add_directed_edge(NavNodeId from, NavNodeId to, float cost = -1.0f);

    // -- Queries -------------------------------------------------------------

    /// Number of nodes in the graph.
    size_t node_count() const { return m_nodes.size(); }

    /// Get a node by id.  Returns nullptr if not found.
    const NavNode* get_node(NavNodeId id) const;

    /// Find the nearest node to a world-space position.
    /// Returns invalid id (0xFFFFFFFF) if the graph is empty.
    NavNodeId find_nearest_node(const glm::vec3& position) const;

    /// Access the edge list (read-only).
    const std::vector<NavEdge>& edges() const { return m_edges; }

    /// Access all nodes (read-only).
    const std::unordered_map<NavNodeId, NavNode>& nodes() const { return m_nodes; }

    // -- Generation ----------------------------------------------------------

    /// Auto-generate a grid-based navigation graph from terrain data.
    ///
    /// @param physics       Physics world with terrain heights loaded.
    /// @param world_min     Minimum corner of the area to cover (x, z).
    /// @param world_max     Maximum corner of the area to cover (x, z).
    /// @param spacing       Distance between adjacent nodes in the grid.
    /// @param max_slope     Maximum walkable slope in degrees (0-90).
    ///
    /// Nodes are placed at grid intersections where the terrain slope is
    /// below max_slope.  Edges connect horizontally, vertically, and
    /// diagonally adjacent walkable nodes.
    void generate_from_terrain(const PhysicsWorld& physics,
                               const glm::vec3& world_min,
                               const glm::vec3& world_max,
                               float spacing   = 4.0f,
                               float max_slope  = 45.0f);

private:
    std::unordered_map<NavNodeId, NavNode> m_nodes;
    std::vector<NavEdge>                   m_edges;
    NavNodeId                              m_next_id = 0;
};

// ============================================================================
// A* pathfinding
// ============================================================================

/// Result of a pathfinding query.
struct PathResult {
    bool                   found = false;
    std::vector<glm::vec3> waypoints;      ///< Ordered list of positions to follow.
    float                  total_cost = 0.0f;
};

/// Run A* on a NavGraph from @p start to @p goal.
///
/// Returns a PathResult with waypoints from start to goal (inclusive).
/// If no path exists, result.found is false.
PathResult find_path(const NavGraph& graph,
                     const glm::vec3& start,
                     const glm::vec3& goal);

/// Overload taking node ids directly (avoids nearest-node lookup).
PathResult find_path(const NavGraph& graph,
                     NavNodeId start_node,
                     NavNodeId goal_node);

// ============================================================================
// Terrain walkability
// ============================================================================

/// Query whether a world-space position is walkable on the terrain.
///
/// A position is walkable if:
///   1. It lies within the terrain bounds.
///   2. The local slope does not exceed @p max_slope_degrees.
///
/// @param physics           Physics world with terrain data.
/// @param position          World-space position to test.
/// @param max_slope_degrees Maximum slope in degrees (default 45).
/// @param sample_radius     Distance for finite-difference slope estimation.
bool is_walkable(const PhysicsWorld& physics,
                 const glm::vec3& position,
                 float max_slope_degrees = 45.0f,
                 float sample_radius     = 0.5f);

/// Compute the terrain slope (in degrees) at a world-space position.
/// Uses finite-difference sampling of get_terrain_height.
float terrain_slope_at(const PhysicsWorld& physics,
                       float x, float z,
                       float sample_radius = 0.5f);

// ============================================================================
// Path follower — smooth steering along a waypoint sequence
// ============================================================================

/// Tracks an entity's progress along a path and produces steering output.
class PathFollower {
public:
    /// Set a new path to follow.  Resets progress to the first waypoint.
    void set_path(const std::vector<glm::vec3>& waypoints);

    /// Clear the current path.
    void clear();

    /// Returns true if the follower has a path and has not yet reached the end.
    bool is_active() const;

    /// Returns true if the follower reached the final waypoint.
    bool is_finished() const;

    /// Update the follower and compute a desired velocity vector.
    ///
    /// @param current_pos   Entity's current world-space position.
    /// @param max_speed     Maximum movement speed (units/sec).
    /// @param dt            Time step in seconds.
    /// @param arrival_dist  Distance at which a waypoint is considered reached.
    /// @return Desired velocity vector (not normalized — magnitude <= max_speed).
    glm::vec3 compute_steering(const glm::vec3& current_pos,
                               float max_speed,
                               float dt,
                               float arrival_dist = 1.5f);

    /// Get the current target waypoint position, or zero vec if inactive.
    glm::vec3 current_waypoint() const;

    /// Get remaining waypoint count (including the current target).
    size_t remaining_waypoints() const;

private:
    std::vector<glm::vec3> m_waypoints;
    size_t                 m_current_index = 0;
    bool                   m_active        = false;
};

// ============================================================================
// Dynamic obstacle avoidance
// ============================================================================

/// Simple obstacle representation for avoidance steering.
struct Obstacle {
    glm::vec3 position = {0.0f, 0.0f, 0.0f};
    float     radius   = 1.0f;
};

/// Compute an avoidance steering vector that pushes away from nearby obstacles.
///
/// @param entity_pos     Entity's current position.
/// @param desired_vel    The velocity the entity wants to move at.
/// @param obstacles      List of nearby obstacles.
/// @param avoidance_dist Distance within which obstacles exert repulsion.
/// @return An adjustment vector to add to the desired velocity.
glm::vec3 compute_avoidance(const glm::vec3& entity_pos,
                            const glm::vec3& desired_vel,
                            const std::vector<Obstacle>& obstacles,
                            float avoidance_dist = 4.0f);

// ============================================================================
// Pathfinder — high-level façade combining graph, search, and following
// ============================================================================

/// High-level pathfinding system.
///
/// Owns the navigation graph and provides a simple interface for AI:
///   1. Build or load the nav graph.
///   2. Request a path from A to B.
///   3. Tick path followers each frame to get steering vectors.
class Pathfinder {
public:
    bool init();
    void shutdown();

    /// Access the underlying nav graph for manual construction.
    NavGraph&       nav_graph()       { return m_graph; }
    const NavGraph& nav_graph() const { return m_graph; }

    /// Build the nav graph from terrain data.
    void build_from_terrain(const PhysicsWorld& physics,
                            const glm::vec3& world_min,
                            const glm::vec3& world_max,
                            float spacing   = 4.0f,
                            float max_slope  = 45.0f);

    /// Request a path between two world-space positions.
    PathResult request_path(const glm::vec3& from, const glm::vec3& to) const;

    /// Set the physics world pointer (used for walkability queries).
    void set_physics_world(const PhysicsWorld* physics) { m_physics = physics; }
    const PhysicsWorld* physics_world() const { return m_physics; }

private:
    NavGraph             m_graph;
    const PhysicsWorld*  m_physics = nullptr;
};

} // namespace swbf
