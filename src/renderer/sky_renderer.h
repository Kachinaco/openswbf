#pragma once

#include "renderer/backend/gl_context.h"
#include "core/types.h"

namespace swbf {

/// Renders a procedural sky dome as a fallback when no game skybox assets are
/// loaded.  The dome is a hemisphere (upper half of a UV sphere) with per-vertex
/// color interpolation from a warm horizon color to a cool zenith color.
///
/// Usage:
///   sky.init();
///   // each frame, before other geometry:
///   sky.render(view, projection);
///
/// The sky is drawn with depth writes and depth testing disabled so that it
/// always sits behind all scene geometry.  Translation is stripped from the view
/// matrix so the dome appears infinitely far away.
class SkyRenderer {
public:
    SkyRenderer() = default;
    ~SkyRenderer() = default;

    SkyRenderer(const SkyRenderer&) = delete;
    SkyRenderer& operator=(const SkyRenderer&) = delete;

    /// Compile the sky shader and generate the dome geometry.
    /// Returns false if shader compilation or buffer creation fails.
    bool init();

    /// Render the sky dome.  @p view_matrix and @p proj_matrix are column-major
    /// 4x4 float arrays (compatible with GLM / raw float[16]).
    void render(const float* view_matrix, const float* proj_matrix);

    /// Override the sky gradient colors.  Top is the zenith; bottom is the
    /// horizon.
    void set_colors(float top_r, float top_g, float top_b,
                    float bottom_r, float bottom_g, float bottom_b);

    /// Release GPU resources.
    void destroy();

private:
    bool compile_shader();
    void generate_dome(u32 longitude_segments, u32 latitude_segments);

    // Shader program
    u32 m_program = 0;

    // Uniform locations
    i32 m_loc_view       = -1;
    i32 m_loc_projection = -1;
    i32 m_loc_top_color  = -1;
    i32 m_loc_bot_color  = -1;

    // Geometry
    u32 m_vao         = 0;
    u32 m_vbo         = 0;
    u32 m_ibo         = 0;
    u32 m_index_count = 0;

    // Sky gradient colors
    float m_top_color[3]    = {0.2f, 0.4f, 0.8f};   // zenith — sky blue
    float m_bottom_color[3] = {0.8f, 0.7f, 0.5f};   // horizon — warm
};

} // namespace swbf
