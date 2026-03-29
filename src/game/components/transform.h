#pragma once

namespace swbf {

/// Transform component — position, rotation (quaternion), and scale.
///
/// The quaternion is stored in (x, y, z, w) order, matching the convention
/// used by most physics libraries.  The identity rotation is (0, 0, 0, 1).
///
/// to_matrix() produces a column-major 4x4 matrix suitable for OpenGL,
/// computed as T * R * S (translate, then rotate, then scale).
struct Transform {
    float position[3] = {0.0f, 0.0f, 0.0f};
    float rotation[4] = {0.0f, 0.0f, 0.0f, 1.0f}; // quaternion xyzw
    float scale[3]    = {1.0f, 1.0f, 1.0f};

    /// Compute a column-major 4x4 model matrix from position, rotation, scale.
    /// @p out_mat4  Pointer to 16 floats that will receive the result.
    void to_matrix(float* out_mat4) const;
};

} // namespace swbf
