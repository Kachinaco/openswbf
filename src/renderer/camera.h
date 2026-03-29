#pragma once

namespace swbf {

/// Free-fly debug camera producing view and projection matrices suitable for
/// OpenGL (column-major layout, right-handed coordinate system).
///
/// The camera uses a yaw/pitch Euler-angle model.  Yaw rotates around the
/// world Y axis; pitch tilts around the camera's local right axis.  Pitch is
/// clamped to +/- 89 degrees to avoid gimbal lock at the poles.
///
/// Movement helpers (`move_forward`, `move_right`, `move_up`) operate in
/// world-space so the camera behaves like a standard fly-through controller.
class Camera {
public:
    Camera();

    // ---- Absolute setters ------------------------------------------------

    void set_position(float x, float y, float z);
    void set_rotation(float pitch, float yaw); // in radians

    // ---- Relative movement / rotation ------------------------------------

    /// Move along the forward/back axis derived from yaw (and pitch).
    void move_forward(float amount);

    /// Strafe left/right — perpendicular to the forward vector on the XZ plane.
    void move_right(float amount);

    /// Move along the world-up (Y) axis.
    void move_up(float amount);

    /// Add incremental pitch and yaw (in radians).
    void rotate(float delta_pitch, float delta_yaw);

    /// Per-frame tick — reserved for smoothing / interpolation.
    void update(float dt);

    // ---- Matrix access ---------------------------------------------------

    /// Returns a pointer to 16 floats arranged as a 4x4 column-major view
    /// matrix, suitable for passing directly to glUniformMatrix4fv.
    const float* view_matrix() const;

    /// Returns a pointer to 16 floats arranged as a 4x4 column-major
    /// perspective projection matrix.
    const float* projection_matrix() const;

    // ---- Projection parameters -------------------------------------------

    /// Reconfigure the perspective projection.
    /// @param fov_y_radians  Vertical field of view.
    /// @param aspect         Width / height ratio.
    /// @param near_plane     Distance to the near clipping plane (> 0).
    /// @param far_plane      Distance to the far clipping plane (> near).
    void set_perspective(float fov_y_radians, float aspect,
                         float near_plane, float far_plane);

    // ---- Accessors -------------------------------------------------------

    float x() const { return m_position[0]; }
    float y() const { return m_position[1]; }
    float z() const { return m_position[2]; }

    float pitch() const { return m_pitch; }
    float yaw()   const { return m_yaw; }

private:
    // Position in world space.
    float m_position[3] = {0.0f, 50.0f, 0.0f};

    // Euler angles (radians).  Yaw around world-Y, pitch around local-right.
    float m_pitch = 0.0f;
    float m_yaw   = 0.0f;

    // Perspective projection parameters.
    float m_fov    = 1.0472f;       // ~60 degrees
    float m_aspect = 16.0f / 9.0f;
    float m_near   = 0.1f;
    float m_far    = 2000.0f;

    // Cached matrices — recomputed lazily when dirty.
    mutable float m_view[16];
    mutable float m_proj[16];
    mutable bool  m_view_dirty = true;
    mutable bool  m_proj_dirty = true;

    void recalc_view() const;
    void recalc_proj() const;
};

} // namespace swbf
