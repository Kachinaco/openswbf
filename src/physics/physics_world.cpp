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

bool PhysicsWorld::raycast(float ox, float oy, float oz,
                           float dx, float dy, float dz,
                           float max_dist,
                           float* hit_pos, float* hit_normal) const {
    // Normalise direction.
    float len = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (len < 1e-8f) return false;
    dx /= len;
    dy /= len;
    dz /= len;

    // March along the ray in fixed steps, checking against terrain height.
    // Use small steps for accuracy near the origin, larger steps further out.
    constexpr float STEP_SIZE = 0.5f;
    const int max_steps = static_cast<int>(max_dist / STEP_SIZE);

    float prev_h = get_terrain_height(ox, oz);
    bool prev_above = (oy >= prev_h);

    for (int i = 1; i <= max_steps; ++i) {
        float t = static_cast<float>(i) * STEP_SIZE;
        float cx = ox + dx * t;
        float cy = oy + dy * t;
        float cz = oz + dz * t;

        float h = get_terrain_height(cx, cz);
        bool above = (cy >= h);

        // Crossed terrain surface: ray went from above to below (or hit exactly).
        if (prev_above && !above) {
            // Binary search to refine the hit point.
            float t0 = static_cast<float>(i - 1) * STEP_SIZE;
            float t1 = t;
            for (int j = 0; j < 8; ++j) {
                float tm = (t0 + t1) * 0.5f;
                float mx = ox + dx * tm;
                float my = oy + dy * tm;
                float mz = oz + dz * tm;
                float mh = get_terrain_height(mx, mz);
                if (my >= mh) {
                    t0 = tm;
                } else {
                    t1 = tm;
                }
            }
            float tf = (t0 + t1) * 0.5f;
            float hx = ox + dx * tf;
            float hz = oz + dz * tf;

            if (hit_pos) {
                hit_pos[0] = hx;
                hit_pos[1] = get_terrain_height(hx, hz);
                hit_pos[2] = hz;
            }
            if (hit_normal) {
                get_terrain_normal(hx, hz, &hit_normal[0], &hit_normal[1], &hit_normal[2]);
            }
            return true;
        }

        prev_h = h;
        prev_above = above;
    }

    return false;
}

void PhysicsWorld::get_terrain_normal(float x, float z,
                                       float* nx, float* ny, float* nz) const {
    constexpr float eps = 0.5f;
    float hL = get_terrain_height(x - eps, z);
    float hR = get_terrain_height(x + eps, z);
    float hD = get_terrain_height(x, z - eps);
    float hU = get_terrain_height(x, z + eps);

    float gx = hL - hR;
    float gy = 2.0f * eps;
    float gz = hD - hU;

    float len = std::sqrt(gx * gx + gy * gy + gz * gz);
    if (len > 1e-6f) {
        *nx = gx / len;
        *ny = gy / len;
        *nz = gz / len;
    } else {
        *nx = 0.0f;
        *ny = 1.0f;
        *nz = 0.0f;
    }
}

void PhysicsWorld::set_terrain_heights(const float* heights,
                                       int grid_size,
                                       float grid_scale,
                                       float origin_x,
                                       float origin_z) {
    if (!heights || grid_size <= 0 || grid_scale <= 0.0f) {
        LOG_WARN("PhysicsWorld::set_terrain_heights — invalid parameters");
        return;
    }

    const int count = grid_size * grid_size;
    m_heights.resize(static_cast<size_t>(count));
    std::memcpy(m_heights.data(), heights, static_cast<size_t>(count) * sizeof(float));
    m_grid_size  = grid_size;
    m_grid_scale = grid_scale;
    m_origin_x   = origin_x;
    m_origin_z   = origin_z;

    LOG_INFO("PhysicsWorld — terrain heights loaded (%dx%d, scale %.2f, origin %.1f,%.1f)",
             grid_size, grid_size, static_cast<double>(grid_scale),
             static_cast<double>(origin_x), static_cast<double>(origin_z));
}

float PhysicsWorld::get_terrain_height(float x, float z) const {
    if (m_heights.empty() || m_grid_size <= 0) {
        return 0.0f;
    }

    // Convert world coords to fractional grid coords.
    // Grid layout: world x = origin_x + col * grid_scale,
    //              world z = origin_z + row * grid_scale
    const float col_f = (x - m_origin_x) / m_grid_scale;
    const float row_f = (z - m_origin_z) / m_grid_scale;

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
