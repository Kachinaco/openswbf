// Unit tests for game/command_post_system.h — post registration, capture
// progress, and team flip.

#include "game/command_post_system.h"

#include <gtest/gtest.h>

using namespace swbf;

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

TEST(CommandPostSystem, AddPostReturnsValidId) {
    CommandPostSystem cps;
    uint32_t id = cps.add_post("CP1", 10, 0, 20);
    EXPECT_NE(id, 0u);
    EXPECT_EQ(cps.total_posts(), 1u);
}

TEST(CommandPostSystem, AddMultiplePosts) {
    CommandPostSystem cps;
    cps.add_post("CP1", 0, 0, 0);
    cps.add_post("CP2", 10, 0, 0);
    cps.add_post("CP3", 20, 0, 0);
    EXPECT_EQ(cps.total_posts(), 3u);
}

TEST(CommandPostSystem, DefaultOwnerIsNeutral) {
    CommandPostSystem cps;
    uint32_t id = cps.add_post("CP1", 0, 0, 0);
    EXPECT_EQ(cps.get_owner(id), TEAM_NEUTRAL);
}

TEST(CommandPostSystem, InitialOwnerCanBeSet) {
    CommandPostSystem cps;
    uint32_t id = cps.add_post("CP1", 0, 0, 0, TEAM_REPUBLIC);
    EXPECT_EQ(cps.get_owner(id), TEAM_REPUBLIC);
}

// ---------------------------------------------------------------------------
// Capture progress
// ---------------------------------------------------------------------------

TEST(CommandPostSystem, BeginCaptureSetsCapturer) {
    CommandPostSystem cps;
    uint32_t id = cps.add_post("CP1", 0, 0, 0);
    cps.begin_capture(id, TEAM_REPUBLIC);

    const CommandPost* data = cps.get_post(id);
    ASSERT_NE(data, nullptr);
    EXPECT_EQ(data->capturing_team, TEAM_REPUBLIC);
}

TEST(CommandPostSystem, CaptureProgressAdvances) {
    CommandPostSystem cps;
    uint32_t id = cps.add_post("CP1", 0, 0, 0);
    cps.begin_capture(id, TEAM_REPUBLIC);

    // Default capture_rate = 0.25/s. After 2 seconds: 0.5 progress.
    cps.update_capture(id, 2.0f);

    const CommandPost* data = cps.get_post(id);
    ASSERT_NE(data, nullptr);
    EXPECT_FLOAT_EQ(data->capture_progress, 0.5f);
}

TEST(CommandPostSystem, MoreCapturersSpeedsUp) {
    CommandPostSystem cps;
    uint32_t id = cps.add_post("CP1", 0, 0, 0);
    cps.begin_capture(id, TEAM_REPUBLIC);

    // 2 capturers at 0.25/s = 0.5/s. After 1 second: 0.5 progress.
    cps.update_capture(id, 1.0f, 2);

    const CommandPost* data = cps.get_post(id);
    ASSERT_NE(data, nullptr);
    EXPECT_FLOAT_EQ(data->capture_progress, 0.5f);
}

// ---------------------------------------------------------------------------
// Team flip
// ---------------------------------------------------------------------------

TEST(CommandPostSystem, CaptureCompletesAndFlips) {
    CommandPostSystem cps;
    uint32_t id = cps.add_post("CP1", 0, 0, 0);
    cps.begin_capture(id, TEAM_CIS);

    // 0.25/s * 4s = 1.0 -> complete.
    bool flipped = cps.update_capture(id, 4.0f);

    EXPECT_TRUE(flipped);
    EXPECT_EQ(cps.get_owner(id), TEAM_CIS);
}

TEST(CommandPostSystem, CaptureProgressClampedAtOne) {
    CommandPostSystem cps;
    uint32_t id = cps.add_post("CP1", 0, 0, 0);
    cps.begin_capture(id, TEAM_REPUBLIC);

    // Overshoot: 0.25/s * 10s = 2.5, but clamped to 1.0.
    cps.update_capture(id, 10.0f);

    const CommandPost* data = cps.get_post(id);
    ASSERT_NE(data, nullptr);
    EXPECT_FLOAT_EQ(data->capture_progress, 1.0f);
}

TEST(CommandPostSystem, CapturedCallbackIsFired) {
    CommandPostSystem cps;
    uint32_t id = cps.add_post("CP1", 0, 0, 0);

    uint32_t cb_post_id = 0;
    int cb_old = TEAM_NEUTRAL;
    int cb_new = TEAM_NEUTRAL;

    cps.set_captured_callback([&](uint32_t pid, int old_team, int new_team) {
        cb_post_id = pid;
        cb_old = old_team;
        cb_new = new_team;
    });

    cps.begin_capture(id, TEAM_REPUBLIC);
    cps.update_capture(id, 5.0f);  // Completes capture.

    EXPECT_EQ(cb_post_id, id);
    EXPECT_EQ(cb_old, TEAM_NEUTRAL);
    EXPECT_EQ(cb_new, TEAM_REPUBLIC);
}

TEST(CommandPostSystem, PartialCaptureDoesNotFlip) {
    CommandPostSystem cps;
    uint32_t id = cps.add_post("CP1", 0, 0, 0);
    cps.begin_capture(id, TEAM_CIS);

    bool flipped = cps.update_capture(id, 1.0f);  // 0.25 < 1.0.
    EXPECT_FALSE(flipped);
    EXPECT_EQ(cps.get_owner(id), TEAM_NEUTRAL);
}

// ---------------------------------------------------------------------------
// Contested capture (different team starts capturing)
// ---------------------------------------------------------------------------

TEST(CommandPostSystem, DifferentTeamResetsProgress) {
    CommandPostSystem cps;
    uint32_t id = cps.add_post("CP1", 0, 0, 0);

    cps.begin_capture(id, TEAM_REPUBLIC);
    cps.update_capture(id, 2.0f);  // Progress = 0.5

    // CIS starts capturing — progress should reset.
    cps.begin_capture(id, TEAM_CIS);
    const CommandPost* data = cps.get_post(id);
    ASSERT_NE(data, nullptr);
    EXPECT_FLOAT_EQ(data->capture_progress, 0.0f);
    EXPECT_EQ(data->capturing_team, TEAM_CIS);
}

// ---------------------------------------------------------------------------
// Ownership counting
// ---------------------------------------------------------------------------

TEST(CommandPostSystem, CountOwnedBy) {
    CommandPostSystem cps;
    cps.add_post("CP1", 0, 0, 0, TEAM_REPUBLIC);
    cps.add_post("CP2", 10, 0, 0, TEAM_REPUBLIC);
    cps.add_post("CP3", 20, 0, 0, TEAM_CIS);
    cps.add_post("CP4", 30, 0, 0, TEAM_NEUTRAL);

    EXPECT_EQ(cps.count_owned_by(TEAM_REPUBLIC), 2);
    EXPECT_EQ(cps.count_owned_by(TEAM_CIS), 1);
    EXPECT_EQ(cps.count_owned_by(TEAM_NEUTRAL), 1);
}

// ---------------------------------------------------------------------------
// Remove
// ---------------------------------------------------------------------------

TEST(CommandPostSystem, RemovePostReducesCount) {
    CommandPostSystem cps;
    uint32_t id = cps.add_post("CP1", 0, 0, 0);
    cps.remove_post(id);
    EXPECT_EQ(cps.total_posts(), 0u);
    EXPECT_EQ(cps.get_post(id), nullptr);
}

// ---------------------------------------------------------------------------
// Edge: already-owned post update
// ---------------------------------------------------------------------------

TEST(CommandPostSystem, UpdateCaptureOnOwnedPostReturnsFalse) {
    CommandPostSystem cps;
    uint32_t id = cps.add_post("CP1", 0, 0, 0, TEAM_REPUBLIC);

    // The capturing team is already the owner.
    cps.begin_capture(id, TEAM_REPUBLIC);
    bool flipped = cps.update_capture(id, 10.0f);
    EXPECT_FALSE(flipped);
}
