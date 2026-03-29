#include "game/components/transform.h"

namespace swbf {

void Transform::to_matrix(float* m) const {
    // Quaternion components.
    const float qx = rotation[0];
    const float qy = rotation[1];
    const float qz = rotation[2];
    const float qw = rotation[3];

    // Rotation matrix elements from unit quaternion.
    const float xx = qx * qx;
    const float yy = qy * qy;
    const float zz = qz * qz;
    const float xy = qx * qy;
    const float xz = qx * qz;
    const float yz = qy * qz;
    const float wx = qw * qx;
    const float wy = qw * qy;
    const float wz = qw * qz;

    // Column-major 4x4 = T * R * S
    // Column 0 (right)
    m[0]  = (1.0f - 2.0f * (yy + zz)) * scale[0];
    m[1]  = (       2.0f * (xy + wz))  * scale[0];
    m[2]  = (       2.0f * (xz - wy))  * scale[0];
    m[3]  = 0.0f;

    // Column 1 (up)
    m[4]  = (       2.0f * (xy - wz))  * scale[1];
    m[5]  = (1.0f - 2.0f * (xx + zz)) * scale[1];
    m[6]  = (       2.0f * (yz + wx))  * scale[1];
    m[7]  = 0.0f;

    // Column 2 (forward)
    m[8]  = (       2.0f * (xz + wy))  * scale[2];
    m[9]  = (       2.0f * (yz - wx))  * scale[2];
    m[10] = (1.0f - 2.0f * (xx + yy)) * scale[2];
    m[11] = 0.0f;

    // Column 3 (translation)
    m[12] = position[0];
    m[13] = position[1];
    m[14] = position[2];
    m[15] = 1.0f;
}

} // namespace swbf
