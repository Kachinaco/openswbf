// Unit tests for game/conquest_mode.h — ticket bleed, win conditions.

#include "game/conquest_mode.h"

#include <gtest/gtest.h>

using namespace swbf;

// Helper: standard config with 100 tickets, 5 posts, bleed at 1/s below 50%.
static ConquestConfig default_config() {
    ConquestConfig cfg;
    cfg.initial_tickets    = 100;
    cfg.bleed_rate         = 1.0f;
    cfg.bleed_threshold    = 0.5f;
    cfg.total_command_posts = 5;
    return cfg;
}

// ---------------------------------------------------------------------------
// Initialisation
// ---------------------------------------------------------------------------

TEST(ConquestMode, InitSetsTickets) {
    ConquestMode mode;
    auto cfg = default_config();
    mode.init(cfg);

    EXPECT_EQ(mode.get_tickets(TEAM_REPUBLIC), 100);
    EXPECT_EQ(mode.get_tickets(TEAM_CIS), 100);
    EXPECT_FALSE(mode.is_game_over());
    EXPECT_EQ(mode.get_winner(), TEAM_NEUTRAL);
}

// ---------------------------------------------------------------------------
// Unit kill ticket deduction
// ---------------------------------------------------------------------------

TEST(ConquestMode, OnUnitKilledDeductsTicket) {
    ConquestMode mode;
    mode.init(default_config());

    mode.on_unit_killed(TEAM_REPUBLIC);
    EXPECT_EQ(mode.get_tickets(TEAM_REPUBLIC), 99);
    EXPECT_EQ(mode.get_tickets(TEAM_CIS), 100);
}

TEST(ConquestMode, MultipleKillsDeductMultipleTickets) {
    ConquestMode mode;
    mode.init(default_config());

    for (int i = 0; i < 10; ++i) {
        mode.on_unit_killed(TEAM_CIS);
    }
    EXPECT_EQ(mode.get_tickets(TEAM_CIS), 90);
}

// ---------------------------------------------------------------------------
// Ticket bleed
// ---------------------------------------------------------------------------

TEST(ConquestMode, NoBleedWhenControllingHalf) {
    ConquestMode mode;
    mode.init(default_config());

    // Republic controls 3 of 5 (60% >= 50%), CIS controls 2 of 5 (40% < 50%).
    mode.set_post_counts(TEAM_REPUBLIC, 3);
    mode.set_post_counts(TEAM_CIS, 2);

    mode.update(10.0f);  // 10 seconds.

    // Republic should not bleed, CIS should bleed.
    EXPECT_EQ(mode.get_tickets(TEAM_REPUBLIC), 100);
    EXPECT_LT(mode.get_tickets(TEAM_CIS), 100);
}

TEST(ConquestMode, BleedWhenControllingLessThanHalf) {
    ConquestMode mode;
    mode.init(default_config());

    // Republic controls 1 of 5 (20% < 50%), CIS controls 4 of 5 (80% >= 50%).
    mode.set_post_counts(TEAM_REPUBLIC, 1);
    mode.set_post_counts(TEAM_CIS, 4);

    mode.update(5.0f);

    // At bleed_rate 1/s, 5 seconds = 5 tickets lost.
    EXPECT_EQ(mode.get_tickets(TEAM_REPUBLIC), 95);
    EXPECT_EQ(mode.get_tickets(TEAM_CIS), 100);
}

TEST(ConquestMode, NoBleedWhenControllingExactlyHalf) {
    ConquestMode mode;
    ConquestConfig cfg = default_config();
    cfg.total_command_posts = 4;
    mode.init(cfg);

    // 2 of 4 = 50% — equal to threshold, should NOT bleed.
    mode.set_post_counts(TEAM_REPUBLIC, 2);
    mode.set_post_counts(TEAM_CIS, 2);

    mode.update(10.0f);

    EXPECT_EQ(mode.get_tickets(TEAM_REPUBLIC), 100);
    EXPECT_EQ(mode.get_tickets(TEAM_CIS), 100);
}

TEST(ConquestMode, BleedWithFasterRate) {
    ConquestMode mode;
    ConquestConfig cfg = default_config();
    cfg.bleed_rate = 2.0f;  // 2 tickets per second.
    mode.init(cfg);

    mode.set_post_counts(TEAM_REPUBLIC, 1);
    mode.set_post_counts(TEAM_CIS, 4);

    mode.update(5.0f);  // 2/s * 5s = 10 tickets lost.

    EXPECT_EQ(mode.get_tickets(TEAM_REPUBLIC), 90);
}

// ---------------------------------------------------------------------------
// Win conditions
// ---------------------------------------------------------------------------

TEST(ConquestMode, TeamEliminatedByKills) {
    ConquestMode mode;
    ConquestConfig cfg = default_config();
    cfg.initial_tickets = 5;
    mode.init(cfg);

    for (int i = 0; i < 5; ++i) {
        mode.on_unit_killed(TEAM_CIS);
    }

    EXPECT_TRUE(mode.is_eliminated(TEAM_CIS));
    EXPECT_TRUE(mode.is_game_over());
    EXPECT_EQ(mode.get_winner(), TEAM_REPUBLIC);
}

TEST(ConquestMode, TeamEliminatedByBleed) {
    ConquestMode mode;
    ConquestConfig cfg = default_config();
    cfg.initial_tickets = 10;
    cfg.bleed_rate = 1.0f;
    mode.init(cfg);

    mode.set_post_counts(TEAM_REPUBLIC, 0);
    mode.set_post_counts(TEAM_CIS, 5);

    // 10 tickets at 1/s = 10 seconds to bleed out.
    mode.update(10.0f);

    EXPECT_TRUE(mode.is_eliminated(TEAM_REPUBLIC));
    EXPECT_TRUE(mode.is_game_over());
    EXPECT_EQ(mode.get_winner(), TEAM_CIS);
}

TEST(ConquestMode, WinCallbackIsFired) {
    ConquestMode mode;
    ConquestConfig cfg = default_config();
    cfg.initial_tickets = 1;
    mode.init(cfg);

    int winner_from_cb = TEAM_NEUTRAL;
    mode.set_win_callback([&](int w) {
        winner_from_cb = w;
    });

    mode.on_unit_killed(TEAM_REPUBLIC);

    EXPECT_EQ(winner_from_cb, TEAM_CIS);
}

TEST(ConquestMode, GameNotOverWhenBothHaveTickets) {
    ConquestMode mode;
    mode.init(default_config());

    mode.on_unit_killed(TEAM_REPUBLIC);
    mode.on_unit_killed(TEAM_CIS);

    EXPECT_FALSE(mode.is_game_over());
    EXPECT_EQ(mode.get_winner(), TEAM_NEUTRAL);
}

// ---------------------------------------------------------------------------
// Post-game-over behaviour
// ---------------------------------------------------------------------------

TEST(ConquestMode, KillsAfterGameOverAreIgnored) {
    ConquestMode mode;
    ConquestConfig cfg = default_config();
    cfg.initial_tickets = 1;
    mode.init(cfg);

    mode.on_unit_killed(TEAM_REPUBLIC);
    EXPECT_TRUE(mode.is_game_over());

    // Further kills should not change anything.
    int tickets_before = mode.get_tickets(TEAM_CIS);
    mode.on_unit_killed(TEAM_CIS);
    EXPECT_EQ(mode.get_tickets(TEAM_CIS), tickets_before);
}

TEST(ConquestMode, BleedAfterGameOverIsIgnored) {
    ConquestMode mode;
    ConquestConfig cfg = default_config();
    cfg.initial_tickets = 1;
    mode.init(cfg);

    mode.on_unit_killed(TEAM_CIS);
    EXPECT_TRUE(mode.is_game_over());

    int tickets_before = mode.get_tickets(TEAM_REPUBLIC);
    mode.set_post_counts(TEAM_REPUBLIC, 0);
    mode.set_post_counts(TEAM_CIS, 0);
    mode.update(100.0f);
    EXPECT_EQ(mode.get_tickets(TEAM_REPUBLIC), tickets_before);
}

// ---------------------------------------------------------------------------
// Edge: both eliminated simultaneously
// ---------------------------------------------------------------------------

TEST(ConquestMode, DrawWhenBothEliminatedSimultaneously) {
    ConquestMode mode;
    ConquestConfig cfg = default_config();
    cfg.initial_tickets = 5;
    cfg.bleed_rate = 1.0f;
    mode.init(cfg);

    // Both teams have 0 posts, both below threshold.
    mode.set_post_counts(TEAM_REPUBLIC, 0);
    mode.set_post_counts(TEAM_CIS, 0);

    // Both bleed at 1/s. After 5 seconds both reach 0.
    mode.update(5.0f);

    EXPECT_TRUE(mode.is_game_over());
    // Draw: winner = TEAM_NEUTRAL.
    EXPECT_EQ(mode.get_winner(), TEAM_NEUTRAL);
}

// ---------------------------------------------------------------------------
// Tickets cannot go negative
// ---------------------------------------------------------------------------

TEST(ConquestMode, TicketsFloorAtZero) {
    ConquestMode mode;
    ConquestConfig cfg = default_config();
    cfg.initial_tickets = 3;
    mode.init(cfg);

    for (int i = 0; i < 10; ++i) {
        mode.on_unit_killed(TEAM_REPUBLIC);
    }

    EXPECT_EQ(mode.get_tickets(TEAM_REPUBLIC), 0);
}
