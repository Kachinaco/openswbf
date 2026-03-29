#pragma once

#include <vector>

namespace swbf {

/// Lightweight physics world providing terrain collision queries.
///
/// This is a stub implementation that supports terrain height lookups via
/// bilinear interpolation of a height grid.  Full rigid-body physics
/// (Bullet integration) will be added in a later milestone.
///
/// The height grid uses the same layout as TerrainData:
///   - grid_size x grid_size vertices in row-major order
///   - index = row * grid_size + col
///   - world-space position: x = col * grid_scale, z = row * grid_scale
class PhysicsWorld {
public:
    bool init();
    void shutdown();

    /// Advance the simulation by @p dt seconds.
    /// Currently a no-op (no dynamic bodies yet).
    void step(float dt);

    // ---- Raycasting ---------------------------------------------------------

    /// Cast a ray from origin (ox,oy,oz) in direction (dx,dy,dz).
    /// Returns true if a hit is found within @p max_dist.
    /// On hit, @p hit_pos and @p hit_normal are filled with 3-float vectors.
    /// Stub: always returns false until Bullet is integrated.
    bool raycast(float ox, float oy, float oz,
                 float dx, float dy, float dz,
                 float max_dist,
                 float* hit_pos, float* hit_normal) const;

    // ---- Terrain collision --------------------------------------------------

    /// Upload a height grid for terrain queries.
    /// @p heights     Row-major array of grid_size*grid_size floats.
    /// @p grid_size   Number of vertices per side.
    /// @p grid_scale  World-space distance between adjacent vertices.
    void set_terrain_heights(const float* heights, int grid_size, float grid_scale);

    /// Query the terrain height at world-space position (x, z) using bilinear
    /// interpolation.  Returns 0 if no terrain data has been set or if the
    /// query point lies outside the grid.
    float get_terrain_height(float x, float z) const;

private:
    std::vector<float> m_heights;
    int   m_grid_size  = 0;
    float m_grid_scale = 8.0f;
};

} // namespace swbf
