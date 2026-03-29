// Unit tests for renderer/camera.h — free-fly debug camera math.

#include "renderer/camera.h"

#include <gtest/gtest.h>

#include <cmath>

using namespace swbf;

// Helper: the default camera starts at (0, 50, 0) with 0 pitch and 0 yaw.
static constexpr float DEFAULT_X = 0.0f;
static constexpr float DEFAULT_Y = 50.0f;
static constexpr float DEFAULT_Z = 0.0f;

static constexpr float PI = 3.14159265358979323846f;
static constexpr float DEG2RAD = PI / 180.0f;
static constexpr float EPSILON = 1e-4f;

// ---------------------------------------------------------------------------
// Initial state
// ---------------------------------------------------------------------------

TEST(Camera, DefaultPosition) {
    Camera cam;
    EXPECT_FLOAT_EQ(cam.x(), DEFAULT_X);
    EXPECT_FLOAT_EQ(cam.y(), DEFAULT_Y);
    EXPECT_FLOAT_EQ(cam.z(), DEFAULT_Z);
}

TEST(Camera, DefaultRotation) {
    Camera cam;
    EXPECT_FLOAT_EQ(cam.pitch(), 0.0f);
    EXPECT_FLOAT_EQ(cam.yaw(), 0.0f);
}

// ---------------------------------------------------------------------------
// Setters
// ---------------------------------------------------------------------------

TEST(Camera, SetPosition) {
    Camera cam;
    cam.set_position(10.0f, 20.0f, 30.0f);
    EXPECT_FLOAT_EQ(cam.x(), 10.0f);
    EXPECT_FLOAT_EQ(cam.y(), 20.0f);
    EXPECT_FLOAT_EQ(cam.z(), 30.0f);
}

TEST(Camera, SetRotation) {
    Camera cam;
    cam.set_rotation(0.5f, 1.0f);
    EXPECT_FLOAT_EQ(cam.pitch(), 0.5f);
    EXPECT_FLOAT_EQ(cam.yaw(), 1.0f);
}

// ---------------------------------------------------------------------------
// View matrix sanity
// ---------------------------------------------------------------------------

TEST(Camera, ViewMatrixIsValid) {
    Camera cam;
    cam.set_position(0.0f, 0.0f, 0.0f);
    cam.set_rotation(0.0f, 0.0f);
    const float* v = cam.view_matrix();

    // The bottom-right element (m[15]) of a valid view matrix is 1.0.
    EXPECT_FLOAT_EQ(v[15], 1.0f);

    // m[3], m[7], m[11] should be 0 (last row of the upper 3x4 block).
    EXPECT_FLOAT_EQ(v[3], 0.0f);
    EXPECT_FLOAT_EQ(v[7], 0.0f);
    EXPECT_FLOAT_EQ(v[11], 0.0f);
}

TEST(Camera, ViewMatrixAtOriginZeroRotation) {
    // At origin with pitch=0, yaw=0:
    //   forward = (0, 0, -1)  (right-handed, -Z is forward)
    //   right   = (1, 0, 0)
    //   up      = (0, 1, 0)
    // View matrix upper-left 3x3 should be the identity (since we're
    // looking along -Z and our axes align with world axes).
    Camera cam;
    cam.set_position(0.0f, 0.0f, 0.0f);
    cam.set_rotation(0.0f, 0.0f);
    const float* v = cam.view_matrix();

    // Column-major:
    // col0 = (right.x, up.x, -fwd.x, 0) => (1, 0, 0, 0)
    // col1 = (right.y, up.y, -fwd.y, 0) => (0, 1, 0, 0)
    // col2 = (right.z, up.z, -fwd.z, 0) => (0, 0, 1, 0)
    // col3 = translation
    // At origin, col0..2 should form an identity.
    // forward: fx=0, fy=0, fz=-1  =>  -fx=0, -fy=0, -fz=1
    // right:   sx=1, sy=0, sz=0
    // up: cross(s, f)

    // col0: s.x, u.x, -f.x, 0
    EXPECT_NEAR(v[0], 1.0f, EPSILON);  // sx = cos(0) = 1
    EXPECT_NEAR(v[3], 0.0f, EPSILON);

    // col1: s.y, u.y, -f.y, 0
    EXPECT_NEAR(v[5], 1.0f, EPSILON);  // uy = sz*fx - sx*fz = 0*0 - 1*(-1) = 1
    EXPECT_NEAR(v[7], 0.0f, EPSILON);

    // col2: s.z, u.z, -f.z, 0
    EXPECT_NEAR(v[10], 1.0f, EPSILON); // -fz = -(-1) = 1
    EXPECT_NEAR(v[11], 0.0f, EPSILON);

    // Translation should be zero at origin.
    EXPECT_NEAR(v[12], 0.0f, EPSILON);
    EXPECT_NEAR(v[13], 0.0f, EPSILON);
    EXPECT_NEAR(v[14], 0.0f, EPSILON);
}

// ---------------------------------------------------------------------------
// Projection matrix
// ---------------------------------------------------------------------------

TEST(Camera, ProjectionMatrixHasCorrectAspectRatio) {
    Camera cam;
    // Default aspect = 16/9, fov = ~60 degrees (1.0472 rad)
    const float* p = cam.projection_matrix();

    // p[0] = f / aspect, p[5] = f, where f = 1/tan(fov/2)
    float f = 1.0f / std::tan(1.0472f * 0.5f);
    float expected_aspect = 16.0f / 9.0f;

    EXPECT_NEAR(p[0], f / expected_aspect, EPSILON);
    EXPECT_NEAR(p[5], f, EPSILON);
}

TEST(Camera, ProjectionMatrixStructure) {
    Camera cam;
    const float* p = cam.projection_matrix();

    // Standard perspective projection has -1 at p[11] and 0 at p[15].
    EXPECT_FLOAT_EQ(p[11], -1.0f);
    EXPECT_FLOAT_EQ(p[15], 0.0f);

    // Off-diagonal elements should be zero.
    EXPECT_FLOAT_EQ(p[1], 0.0f);
    EXPECT_FLOAT_EQ(p[2], 0.0f);
    EXPECT_FLOAT_EQ(p[3], 0.0f);
    EXPECT_FLOAT_EQ(p[4], 0.0f);
    EXPECT_FLOAT_EQ(p[6], 0.0f);
    EXPECT_FLOAT_EQ(p[7], 0.0f);
    EXPECT_FLOAT_EQ(p[8], 0.0f);
    EXPECT_FLOAT_EQ(p[9], 0.0f);
    EXPECT_FLOAT_EQ(p[12], 0.0f);
    EXPECT_FLOAT_EQ(p[13], 0.0f);
}

TEST(Camera, SetPerspectiveUpdatesProjection) {
    Camera cam;
    float fov = 90.0f * DEG2RAD; // 90 degrees
    float aspect = 4.0f / 3.0f;
    cam.set_perspective(fov, aspect, 1.0f, 1000.0f);

    const float* p = cam.projection_matrix();

    float f = 1.0f / std::tan(fov * 0.5f);
    EXPECT_NEAR(p[0], f / aspect, EPSILON);
    EXPECT_NEAR(p[5], f, EPSILON);

    // Near/far: p[10] = (far+near)/(near-far), p[14] = 2*far*near/(near-far)
    float nf_diff = 1.0f - 1000.0f;
    EXPECT_NEAR(p[10], (1000.0f + 1.0f) / nf_diff, EPSILON);
    EXPECT_NEAR(p[14], (2.0f * 1000.0f * 1.0f) / nf_diff, EPSILON);
}

// ---------------------------------------------------------------------------
// Movement
// ---------------------------------------------------------------------------

TEST(Camera, MoveForwardChangesPosition) {
    Camera cam;
    cam.set_position(0.0f, 0.0f, 0.0f);
    cam.set_rotation(0.0f, 0.0f);

    // With yaw=0 and pitch=0, forward is (0, 0, -1) in right-handed GL.
    cam.move_forward(10.0f);

    EXPECT_NEAR(cam.x(), 0.0f, EPSILON);
    EXPECT_NEAR(cam.y(), 0.0f, EPSILON);
    EXPECT_NEAR(cam.z(), -10.0f, EPSILON); // -Z is forward
}

TEST(Camera, MoveRightChangesPosition) {
    Camera cam;
    cam.set_position(0.0f, 0.0f, 0.0f);
    cam.set_rotation(0.0f, 0.0f);

    // With yaw=0, right direction is (+X).
    cam.move_right(5.0f);

    EXPECT_NEAR(cam.x(), 5.0f, EPSILON);
    EXPECT_NEAR(cam.y(), 0.0f, EPSILON);
    EXPECT_NEAR(cam.z(), 0.0f, EPSILON);
}

TEST(Camera, MoveUpChangesYOnly) {
    Camera cam;
    cam.set_position(0.0f, 0.0f, 0.0f);
    cam.move_up(15.0f);

    EXPECT_NEAR(cam.x(), 0.0f, EPSILON);
    EXPECT_NEAR(cam.y(), 15.0f, EPSILON);
    EXPECT_NEAR(cam.z(), 0.0f, EPSILON);
}

TEST(Camera, MoveForwardWithYaw) {
    Camera cam;
    cam.set_position(0.0f, 0.0f, 0.0f);
    // Yaw = 90 degrees: forward should be roughly (+X, 0, 0) because
    // forward = (sin(yaw)*cos(pitch), sin(pitch), -cos(yaw)*cos(pitch))
    // At yaw=pi/2, pitch=0: forward = (1, 0, 0)
    cam.set_rotation(0.0f, PI / 2.0f);
    cam.move_forward(10.0f);

    EXPECT_NEAR(cam.x(), 10.0f, EPSILON);
    EXPECT_NEAR(cam.y(), 0.0f, EPSILON);
    EXPECT_NEAR(cam.z(), 0.0f, 0.01f);
}

// ---------------------------------------------------------------------------
// Pitch clamping
// ---------------------------------------------------------------------------

TEST(Camera, PitchClampedAtPositive89) {
    Camera cam;
    float extreme_pitch = 100.0f * DEG2RAD; // 100 degrees, well above 89
    cam.set_rotation(extreme_pitch, 0.0f);

    // Pitch should be clamped to ~89 degrees (1.5533 radians).
    EXPECT_LE(cam.pitch(), 89.1f * DEG2RAD);
    EXPECT_GE(cam.pitch(), 88.9f * DEG2RAD);
}

TEST(Camera, PitchClampedAtNegative89) {
    Camera cam;
    float extreme_pitch = -100.0f * DEG2RAD;
    cam.set_rotation(extreme_pitch, 0.0f);

    EXPECT_GE(cam.pitch(), -89.1f * DEG2RAD);
    EXPECT_LE(cam.pitch(), -88.9f * DEG2RAD);
}

TEST(Camera, RotateClampsPitch) {
    Camera cam;
    cam.set_rotation(0.0f, 0.0f);

    // Rotate pitch by a huge amount
    cam.rotate(200.0f * DEG2RAD, 0.0f);
    EXPECT_LE(cam.pitch(), 89.1f * DEG2RAD);

    // Rotate pitch in negative direction far below
    cam.rotate(-400.0f * DEG2RAD, 0.0f);
    EXPECT_GE(cam.pitch(), -89.1f * DEG2RAD);
}

// ---------------------------------------------------------------------------
// View matrix changes with movement
// ---------------------------------------------------------------------------

TEST(Camera, ViewMatrixChangesAfterMovement) {
    Camera cam;
    cam.set_position(0.0f, 0.0f, 0.0f);
    cam.set_rotation(0.0f, 0.0f);

    const float* v1 = cam.view_matrix();
    float tx_before = v1[12];

    cam.move_right(10.0f);
    const float* v2 = cam.view_matrix();

    // Translation component should change.
    EXPECT_NE(v2[12], tx_before);
}

// ---------------------------------------------------------------------------
// update() is callable (no-op for now)
// ---------------------------------------------------------------------------

TEST(Camera, UpdateDoesNotCrash) {
    Camera cam;
    cam.update(0.016f); // 60 FPS dt
    // Just verify it doesn't crash or alter state.
    EXPECT_FLOAT_EQ(cam.pitch(), 0.0f);
    EXPECT_FLOAT_EQ(cam.yaw(), 0.0f);
}
