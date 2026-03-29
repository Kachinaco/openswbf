#include "renderer/camera.h"

#include <cmath>

namespace swbf {

// -------------------------------------------------------------------------
// Constants
// -------------------------------------------------------------------------

// +/- 89 degrees in radians — keeps the view from flipping at the poles.
static constexpr float k_max_pitch =  1.5533f;  // 89 * pi / 180
static constexpr float k_min_pitch = -1.5533f;

// -------------------------------------------------------------------------
// Construction
// -------------------------------------------------------------------------

Camera::Camera() = default;

// -------------------------------------------------------------------------
// Absolute setters
// -------------------------------------------------------------------------

void Camera::set_position(float x, float y, float z) {
    m_position[0] = x;
    m_position[1] = y;
    m_position[2] = z;
    m_view_dirty = true;
}

void Camera::set_rotation(float pitch, float yaw) {
    m_pitch = pitch;
    m_yaw   = yaw;

    // Clamp pitch.
    if (m_pitch > k_max_pitch) m_pitch = k_max_pitch;
    if (m_pitch < k_min_pitch) m_pitch = k_min_pitch;

    m_view_dirty = true;
}

// -------------------------------------------------------------------------
// Relative movement
// -------------------------------------------------------------------------

void Camera::move_forward(float amount) {
    // Forward vector derived from yaw and pitch.
    float cp = std::cos(m_pitch);
    float sp = std::sin(m_pitch);
    float cy = std::cos(m_yaw);
    float sy = std::sin(m_yaw);

    m_position[0] += sy * cp * amount;
    m_position[1] += sp * amount;
    m_position[2] -= cy * cp * amount;  // -Z is forward in right-handed GL

    m_view_dirty = true;
}

void Camera::move_right(float amount) {
    // Right vector lies on the XZ plane, perpendicular to the yaw direction.
    float cy = std::cos(m_yaw);
    float sy = std::sin(m_yaw);

    m_position[0] += cy * amount;
    m_position[2] += sy * amount;

    m_view_dirty = true;
}

void Camera::move_up(float amount) {
    m_position[1] += amount;
    m_view_dirty = true;
}

void Camera::rotate(float delta_pitch, float delta_yaw) {
    m_pitch += delta_pitch;
    m_yaw   += delta_yaw;

    // Clamp pitch.
    if (m_pitch > k_max_pitch) m_pitch = k_max_pitch;
    if (m_pitch < k_min_pitch) m_pitch = k_min_pitch;

    m_view_dirty = true;
}

// -------------------------------------------------------------------------
// update
// -------------------------------------------------------------------------

void Camera::update(float /*dt*/) {
    // Currently a no-op — placeholder for future input smoothing or
    // acceleration curves.
}

// -------------------------------------------------------------------------
// Matrix accessors
// -------------------------------------------------------------------------

const float* Camera::view_matrix() const {
    if (m_view_dirty) {
        recalc_view();
        m_view_dirty = false;
    }
    return m_view;
}

const float* Camera::projection_matrix() const {
    if (m_proj_dirty) {
        recalc_proj();
        m_proj_dirty = false;
    }
    return m_proj;
}

// -------------------------------------------------------------------------
// Projection configuration
// -------------------------------------------------------------------------

void Camera::set_perspective(float fov_y_radians, float aspect,
                             float near_plane, float far_plane) {
    m_fov    = fov_y_radians;
    m_aspect = aspect;
    m_near   = near_plane;
    m_far    = far_plane;
    m_proj_dirty = true;
}

// -------------------------------------------------------------------------
// Internal — view matrix (lookAt)
// -------------------------------------------------------------------------
//
// Standard right-handed lookAt construction:
//   f = normalised forward direction (from yaw + pitch)
//   s = normalise(f x world_up)          — camera right
//   u = s x f                            — camera up
//
// The view matrix rows are (s, u, -f) because eye-space looks along -Z.
// Column-major layout: column i occupies m_view[i*4 .. i*4+3].

void Camera::recalc_view() const {
    float cp = std::cos(m_pitch);
    float sp = std::sin(m_pitch);
    float cy = std::cos(m_yaw);
    float sy = std::sin(m_yaw);

    // Forward direction (unit length).
    float fx =  sy * cp;
    float fy =  sp;
    float fz = -cy * cp;

    // s = normalise(f x (0,1,0)) = normalise(-fz, 0, fx).
    // Length = sqrt(fz^2 + fx^2) = |cos(pitch)| = cp  (positive, pitch in (-89,+89)).
    float sx = -fz / cp;   // = cy
    float sy_v = 0.0f;
    float sz =  fx / cp;   // = sy

    // u = s x f  (camera-local up vector).
    float ux = sy_v * fz - sz * fy;
    float uy = sz   * fx - sx * fz;
    float uz = sx   * fy - sy_v * fx;

    // Translation: -dot(basis, eye) for each row.
    float tx = -(sx   * m_position[0] + sy_v * m_position[1] + sz * m_position[2]);
    float ty = -(ux   * m_position[0] + uy   * m_position[1] + uz * m_position[2]);
    float tz = -(-fx  * m_position[0] - fy   * m_position[1] - fz * m_position[2]);

    // Column 0
    m_view[0]  = sx;
    m_view[1]  = ux;
    m_view[2]  = -fx;
    m_view[3]  = 0.0f;

    // Column 1
    m_view[4]  = sy_v;
    m_view[5]  = uy;
    m_view[6]  = -fy;
    m_view[7]  = 0.0f;

    // Column 2
    m_view[8]  = sz;
    m_view[9]  = uz;
    m_view[10] = -fz;
    m_view[11] = 0.0f;

    // Column 3
    m_view[12] = tx;
    m_view[13] = ty;
    m_view[14] = tz;
    m_view[15] = 1.0f;
}

// -------------------------------------------------------------------------
// Internal — projection matrix (perspective)
// -------------------------------------------------------------------------
//
// Standard symmetric perspective projection:
//
//   | f/a   0    0              0            |
//   |  0    f    0              0            |
//   |  0    0   (far+near)/(near-far)  2*far*near/(near-far) |
//   |  0    0   -1              0            |
//
// where f = 1 / tan(fov_y / 2)  and  a = aspect ratio (w/h).
//
// Stored column-major.

void Camera::recalc_proj() const {
    float f = 1.0f / std::tan(m_fov * 0.5f);

    float nf_diff = m_near - m_far;   // (near - far), always negative

    // Column 0
    m_proj[0]  = f / m_aspect;
    m_proj[1]  = 0.0f;
    m_proj[2]  = 0.0f;
    m_proj[3]  = 0.0f;

    // Column 1
    m_proj[4]  = 0.0f;
    m_proj[5]  = f;
    m_proj[6]  = 0.0f;
    m_proj[7]  = 0.0f;

    // Column 2
    m_proj[8]  = 0.0f;
    m_proj[9]  = 0.0f;
    m_proj[10] = (m_far + m_near) / nf_diff;
    m_proj[11] = -1.0f;

    // Column 3
    m_proj[12] = 0.0f;
    m_proj[13] = 0.0f;
    m_proj[14] = (2.0f * m_far * m_near) / nf_diff;
    m_proj[15] = 0.0f;
}

} // namespace swbf
