#pragma once

#include "renderer/backend/gl_context.h"
#include "core/types.h"

#include <vector>
#include <cstdint>

namespace swbf {

/// Low-level 2D overlay renderer for the HUD, menus, minimap, and scoreboard.
///
/// All coordinates are in screen-space pixels: (0,0) is top-left,
/// (width, height) is bottom-right.  The renderer batches colored quads
/// and bitmap-font text into a single draw call per flush.
///
/// Usage each frame:
///   ui.begin(screen_w, screen_h);
///   ui.draw_rect(...);
///   ui.draw_text(...);
///   ui.flush();   // issues the actual GL draw call
class UIRenderer {
public:
    UIRenderer() = default;
    ~UIRenderer() = default;

    UIRenderer(const UIRenderer&) = delete;
    UIRenderer& operator=(const UIRenderer&) = delete;

    /// Compile the 2D overlay shader.  Returns false on failure.
    bool init();

    /// Release GPU resources.
    void destroy();

    /// Begin a new frame of 2D drawing.  Must be called before any draw_*
    /// calls.  Sets the screen dimensions used for the orthographic projection.
    void begin(int screen_w, int screen_h);

    /// Submit all batched geometry to the GPU and reset the vertex buffer.
    void flush();

    // ----- Primitive drawing ------------------------------------------------

    /// Draw a filled rectangle.
    void draw_rect(float x, float y, float w, float h,
                   float r, float g, float b, float a = 1.0f);

    /// Draw a rectangle outline (unfilled) with the given line thickness.
    void draw_rect_outline(float x, float y, float w, float h,
                           float r, float g, float b, float a = 1.0f,
                           float thickness = 1.0f);

    /// Draw a filled circle approximated with line segments.
    void draw_circle(float cx, float cy, float radius,
                     float r, float g, float b, float a = 1.0f,
                     int segments = 16);

    /// Draw a line between two points.
    void draw_line(float x0, float y0, float x1, float y1,
                   float r, float g, float b, float a = 1.0f,
                   float thickness = 1.0f);

    /// Draw a triangle (filled).
    void draw_triangle(float x0, float y0, float x1, float y1,
                       float x2, float y2,
                       float r, float g, float b, float a = 1.0f);

    // ----- Text drawing -----------------------------------------------------

    /// Draw a string of text at the given pixel position.
    /// @p scale  Pixel size of each "dot" in the 5x7 bitmap font (2.0 = default).
    void draw_text(const char* text, float px, float py,
                   float r, float g, float b, float a = 1.0f,
                   float scale = 2.0f);

    /// Draw text with a dark drop shadow for readability.
    void draw_text_shadow(const char* text, float px, float py,
                          float r, float g, float b, float a = 1.0f,
                          float scale = 2.0f);

    /// Measure the width of a text string in pixels at the given scale.
    float text_width(const char* text, float scale = 2.0f) const;

    /// Returns the line height in pixels at the given scale.
    float text_height(float scale = 2.0f) const;

private:
    struct Vertex {
        float x, y;
        float r, g, b, a;
    };

    void push_quad(float x0, float y0, float x1, float y1,
                   float r, float g, float b, float a);

    std::vector<Vertex> m_vertices;

    u32 m_program  = 0;
    int m_screen_w = 1280;
    int m_screen_h = 720;
};

} // namespace swbf
