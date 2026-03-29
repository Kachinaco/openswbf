#include "physics/physics_world.h"
#include "core/log.h"

#include <cmath>
#include <cstring>

namespace swbf {

bool PhysicsWorld::init() {
    LOG_INFO("PhysicsWorld::init — stub (Bullet not yet integrated)");
    return true;
}

void PhysicsWorld::shutdown() {
    m_heights.clear();
    m_grid_size = 0;
    LOG_INFO("PhysicsWorld::shutdown");
}

void PhysicsWorld::step(float /*dt*/) {
    // No dynamic bodies yet — nothing to simulate.
}

bool PhysicsWorld::raycast(float /*ox*/, float /*oy*/, float /*oz*/,
                           float /*dx*/, float /*dy*/, float /*dz*/,
                           float /*max_dist*/,
                           float* /*hit_pos*/, float* /*hit_normal*/) const {
    // Stub — always misses until Bullet is integrated.
    return false;
}

void PhysicsWorld::set_terrain_heights(const float* heights,
                                       int grid_size,
                                       float grid_scale) {
    if (!heights || grid_size <= 0 || grid_scale <= 0.0f) {
        LOG_WARN("PhysicsWorld::set_terrain_heights — invalid parameters");
        return;
    }

    const int count = grid_size * grid_size;
    m_heights.resize(static_cast<size_t>(count));
    std::memcpy(m_heights.data(), heights, static_cast<size_t>(count) * sizeof(float));
    m_grid_size  = grid_size;
    m_grid_scale = grid_scale;

    LOG_INFO("PhysicsWorld — terrain heights loaded (%dx%d, scale %.2f)",
             grid_size, grid_size, static_cast<double>(grid_scale));
}

float PhysicsWorld::get_terrain_height(float x, float z) const {
    if (m_heights.empty() || m_grid_size <= 0) {
        return 0.0f;
    }

    // Convert world coords to fractional grid coords.
    // Grid layout: world x = col * grid_scale, world z = row * grid_scale
    const float col_f = x / m_grid_scale;
    const float row_f = z / m_grid_scale;

    // Clamp to valid grid range [0, grid_size-1].
    const float max_idx = static_cast<float>(m_grid_size - 1);
    const float col_clamped = std::fmax(0.0f, std::fmin(col_f, max_idx));
    const float row_clamped = std::fmax(0.0f, std::fmin(row_f, max_idx));

    // Integer grid cell and fractional part within the cell.
    const int col0 = static_cast<int>(std::floor(col_clamped));
    const int row0 = static_cast<int>(std::floor(row_clamped));
    const int col1 = (col0 < m_grid_size - 1) ? col0 + 1 : col0;
    const int row1 = (row0 < m_grid_size - 1) ? row0 + 1 : row0;

    const float frac_col = col_clamped - static_cast<float>(col0);
    const float frac_row = row_clamped - static_cast<float>(row0);

    // Fetch the four corner heights.
    const float h00 = m_heights[static_cast<size_t>(row0 * m_grid_size + col0)];
    const float h10 = m_heights[static_cast<size_t>(row0 * m_grid_size + col1)];
    const float h01 = m_heights[static_cast<size_t>(row1 * m_grid_size + col0)];
    const float h11 = m_heights[static_cast<size_t>(row1 * m_grid_size + col1)];

    // Bilinear interpolation.
    const float top    = h00 + (h10 - h00) * frac_col;
    const float bottom = h01 + (h11 - h01) * frac_col;
    return top + (bottom - top) * frac_row;
}

} // namespace swbf
