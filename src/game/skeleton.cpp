#include "game/skeleton.h"
#include "core/log.h"

#include <cmath>
#include <cstring>

namespace swbf {

// ===========================================================================
// Matrix helpers (local to this file)
// ===========================================================================

/// Build a column-major 4x4 matrix from position and quaternion rotation.
static void compose_mat4(const float pos[3], const float rot[4],
                         float m[16]) {
    const float qx = rot[0];
    const float qy = rot[1];
    const float qz = rot[2];
    const float qw = rot[3];

    const float xx = qx * qx;
    const float yy = qy * qy;
    const float zz = qz * qz;
    const float xy = qx * qy;
    const float xz = qx * qz;
    const float yz = qy * qz;
    const float wx = qw * qx;
    const float wy = qw * qy;
    const float wz = qw * qz;

    // Column 0
    m[0]  = 1.0f - 2.0f * (yy + zz);
    m[1]  =        2.0f * (xy + wz);
    m[2]  =        2.0f * (xz - wy);
    m[3]  = 0.0f;
    // Column 1
    m[4]  =        2.0f * (xy - wz);
    m[5]  = 1.0f - 2.0f * (xx + zz);
    m[6]  =        2.0f * (yz + wx);
    m[7]  = 0.0f;
    // Column 2
    m[8]  =        2.0f * (xz + wy);
    m[9]  =        2.0f * (yz - wx);
    m[10] = 1.0f - 2.0f * (xx + yy);
    m[11] = 0.0f;
    // Column 3 (translation)
    m[12] = pos[0];
    m[13] = pos[1];
    m[14] = pos[2];
    m[15] = 1.0f;
}

/// Multiply two column-major 4x4 matrices: out = A * B.
static void mul_mat4(const float a[16], const float b[16], float out[16]) {
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            out[col * 4 + row] =
                a[0 * 4 + row] * b[col * 4 + 0] +
                a[1 * 4 + row] * b[col * 4 + 1] +
                a[2 * 4 + row] * b[col * 4 + 2] +
                a[3 * 4 + row] * b[col * 4 + 3];
        }
    }
}

/// Invert a column-major 4x4 affine matrix (rotation + translation, no shear).
/// For affine transforms: inverse = transpose(R) with adjusted translation.
static bool invert_affine_mat4(const float m[16], float out[16]) {
    // Extract the 3x3 rotation part (transposed) and translation.
    // For a pure rotation+translation matrix:
    //   R^-1 = R^T
    //   t^-1 = -R^T * t

    // Transpose the 3x3 rotation block.
    out[0]  = m[0];  out[4]  = m[1];  out[8]  = m[2];
    out[1]  = m[4];  out[5]  = m[5];  out[9]  = m[6];
    out[2]  = m[8];  out[6]  = m[9];  out[10] = m[10];

    // Bottom row stays [0, 0, 0, 1].
    out[3]  = 0.0f;
    out[7]  = 0.0f;
    out[11] = 0.0f;
    out[15] = 1.0f;

    // Compute -R^T * t for the translation column.
    float tx = m[12];
    float ty = m[13];
    float tz = m[14];

    out[12] = -(out[0] * tx + out[4] * ty + out[8]  * tz);
    out[13] = -(out[1] * tx + out[5] * ty + out[9]  * tz);
    out[14] = -(out[2] * tx + out[6] * ty + out[10] * tz);

    return true;
}

// ===========================================================================
// Skeleton implementation
// ===========================================================================

void Skeleton::compute_inverse_bind_matrices() {
    if (bones.empty()) return;

    const int bone_count = static_cast<int>(bones.size());

    // First, compute world-space bind pose matrices for all bones
    // by walking the hierarchy (parents are always at lower indices).
    std::vector<float> world_mats(static_cast<std::size_t>(bone_count) * 16);

    for (int i = 0; i < bone_count; ++i) {
        Bone& bone = bones[static_cast<std::size_t>(i)];
        float local_mat[16];
        compose_mat4(bone.bind_position, bone.bind_rotation, local_mat);

        float* world = &world_mats[static_cast<std::size_t>(i) * 16];

        if (bone.parent >= 0 && bone.parent < i) {
            const float* parent_world =
                &world_mats[static_cast<std::size_t>(bone.parent) * 16];
            mul_mat4(parent_world, local_mat, world);
        } else {
            // Root bone -- local is world.
            std::memcpy(world, local_mat, sizeof(float) * 16);
        }

        // Compute inverse bind matrix (world -> bone space).
        invert_affine_mat4(world, bone.inv_bind_matrix);
    }

    LOG_DEBUG("Skeleton '%s': computed inverse bind matrices for %d bones",
              name.c_str(), bone_count);
}

} // namespace swbf
