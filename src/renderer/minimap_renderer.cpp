#include "renderer/minimap_renderer.h"

#include <cmath>

namespace swbf {

void MiniMapRenderer::set_position(float x, float y, float size) {
    m_screen_x = x;
    m_screen_y = y;
    m_size     = size;
}

bool MiniMapRenderer::world_to_minimap(const float* world_pos,
                                        const float* player_pos,
                                        float player_yaw,
                                        float& out_sx, float& out_sy) const {
    // Offset in world space relative to player.
    float dx = world_pos[0] - player_pos[0];
    float dz = world_pos[2] - player_pos[2];

    // Rotate so that the player's forward direction points "up" on the map.
    float cos_y = std::cos(-player_yaw);
    float sin_y = std::sin(-player_yaw);
    float rx =  dx * cos_y - dz * sin_y;
    float ry = -dx * sin_y - dz * cos_y;

    // Normalize to [-1, 1] within the world radius.
    float nx = rx / m_world_radius;
    float ny = ry / m_world_radius;

    // Check if within the circular minimap.
    if (nx * nx + ny * ny > 1.0f) return false;

    float half = m_size * 0.5f;
    float cx = m_screen_x + half;
    float cy = m_screen_y + half;

    out_sx = cx + nx * half;
    out_sy = cy + ny * half;
    return true;
}

void MiniMapRenderer::render(UIRenderer& ui,
                              const float* player_pos, float player_yaw,
                              Team player_team,
                              const CommandPostSystem& cps,
                              const EntityManager& entities) {
    float half = m_size * 0.5f;
    float cx = m_screen_x + half;
    float cy = m_screen_y + half;

    // -- Background circle (dark semi-transparent) --
    ui.draw_circle(cx, cy, half, 0.0f, 0.0f, 0.0f, 0.5f, 32);

    // -- Border circle --
    // Draw a ring by drawing a slightly larger circle then a smaller dark one.
    // Actually we just draw a border outline using line segments.
    {
        constexpr int SEGS = 32;
        constexpr float PI2 = 6.28318530718f;
        for (int i = 0; i < SEGS; ++i) {
            float a0 = PI2 * static_cast<float>(i) / static_cast<float>(SEGS);
            float a1 = PI2 * static_cast<float>(i + 1) / static_cast<float>(SEGS);
            float x0 = cx + half * std::cos(a0);
            float y0 = cy + half * std::sin(a0);
            float x1 = cx + half * std::cos(a1);
            float y1 = cy + half * std::sin(a1);
            ui.draw_line(x0, y0, x1, y1, 0.4f, 0.6f, 0.4f, 0.8f, 2.0f);
        }
    }

    // -- Command posts --
    for (const auto& post : cps.posts()) {
        float sx, sy;
        if (world_to_minimap(post.position, player_pos, player_yaw, sx, sy)) {
            float pr = 0.5f, pg = 0.5f, pb = 0.5f; // neutral = grey
            switch (post.owner()) {
                case Team::REPUBLIC: pr = 0.2f; pg = 0.5f; pb = 1.0f; break;
                case Team::CIS:     pr = 1.0f; pg = 0.2f; pb = 0.2f; break;
                default: break;
            }
            // Draw a small diamond for command posts.
            float d = 5.0f;
            ui.draw_triangle(sx, sy - d, sx - d, sy, sx + d, sy,
                             pr, pg, pb, 1.0f);
            ui.draw_triangle(sx, sy + d, sx - d, sy, sx + d, sy,
                             pr, pg, pb, 1.0f);

            // If being captured, show a pulsing outline.
            if (post.capture_progress > 0.0f) {
                float cr = 1.0f, cg = 1.0f, cb = 0.0f;
                ui.draw_circle(sx, sy, d + 2.0f, cr, cg, cb, 0.5f, 8);
            }
        }
    }

    // -- Friendly units --
    {
        int pt = static_cast<int>(player_team);
        auto friendlies = entities.alive_on_team(pt);
        for (const auto* ent : friendlies) {
            if (!ent->alive) continue;
            if (ent->is_player) continue; // player drawn separately

            float sx, sy;
            if (world_to_minimap(ent->position, player_pos, player_yaw, sx, sy)) {
                // Green dots for friendlies.
                ui.draw_circle(sx, sy, 2.5f, 0.2f, 0.9f, 0.2f, 0.9f, 6);
            }
        }
    }

    // -- Player indicator (always in the center) --
    // Draw a small upward-pointing triangle at the center.
    {
        float d = 6.0f;
        // Player is always at center, facing "up" since we rotated the map.
        ui.draw_triangle(cx, cy - d,
                         cx - d * 0.6f, cy + d * 0.5f,
                         cx + d * 0.6f, cy + d * 0.5f,
                         1.0f, 1.0f, 1.0f, 1.0f);
    }

    // -- Compass labels (N/S/E/W) at the edges --
    {
        // Calculate where the cardinal directions map to on the minimap edge.
        // "North" in world space is -Z.  We need to rotate by player_yaw.
        struct DirLabel {
            const char* label;
            float world_angle; // angle from +X axis in world XZ plane
        };
        DirLabel dirs[] = {
            {"N",  1.5708f},  //  pi/2 = -Z direction
            {"S", -1.5708f},  // -pi/2 = +Z direction
            {"E",  0.0f},     //  0    = +X direction
            {"W",  3.14159f}, //  pi   = -X direction
        };

        for (const auto& d : dirs) {
            float angle = d.world_angle - player_yaw;
            float lx = cx + (half - 12.0f) * std::cos(angle);
            float ly = cy - (half - 12.0f) * std::sin(angle);
            ui.draw_text(d.label, lx - 3.0f, ly - 5.0f,
                         0.8f, 0.8f, 0.8f, 0.6f, 1.5f);
        }
    }
}

} // namespace swbf
