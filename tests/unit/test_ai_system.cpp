// Unit tests for game/ai_system.h — state transitions:
// idle -> moving -> attacking -> retreating.

#include "game/ai_system.h"

#include <gtest/gtest.h>

using namespace swbf;

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

TEST(AISystem, AddAndCount) {
    AISystem ai;
    EXPECT_EQ(ai.count(), 0u);

    ai.add(1);
    EXPECT_EQ(ai.count(), 1u);

    ai.add(2);
    EXPECT_EQ(ai.count(), 2u);
}

TEST(AISystem, RemoveDecrementsCount) {
    AISystem ai;
    ai.add(1);
    ai.add(2);
    ai.remove(1);
    EXPECT_EQ(ai.count(), 1u);
}

TEST(AISystem, DefaultStateIsIdle) {
    AISystem ai;
    ai.add(1);
    EXPECT_EQ(ai.get_state(1), AIState::Idle);
}

TEST(AISystem, GetDataReturnsNullForUnregistered) {
    AISystem ai;
    EXPECT_EQ(ai.get_data(999), nullptr);
}

// ---------------------------------------------------------------------------
// Idle -> Moving (set waypoint)
// ---------------------------------------------------------------------------

TEST(AISystem, SetWaypointTransitionsIdleToMoving) {
    AISystem ai;
    ai.add(1);
    EXPECT_EQ(ai.get_state(1), AIState::Idle);

    ai.set_waypoint(1, 10.0f, 0.0f, 20.0f);
    EXPECT_EQ(ai.get_state(1), AIState::Moving);
}

TEST(AISystem, WaypointIsStored) {
    AISystem ai;
    ai.add(1);
    ai.set_waypoint(1, 10.0f, 5.0f, 20.0f);

    const AIData* data = ai.get_data(1);
    ASSERT_NE(data, nullptr);
    EXPECT_FLOAT_EQ(data->waypoint[0], 10.0f);
    EXPECT_FLOAT_EQ(data->waypoint[1], 5.0f);
    EXPECT_FLOAT_EQ(data->waypoint[2], 20.0f);
}

// ---------------------------------------------------------------------------
// Idle -> Attacking (set target)
// ---------------------------------------------------------------------------

TEST(AISystem, SetTargetTransitionsIdleToAttacking) {
    AISystem ai;
    ai.add(1);
    EXPECT_EQ(ai.get_state(1), AIState::Idle);

    ai.set_target(1, 42);
    EXPECT_EQ(ai.get_state(1), AIState::Attacking);

    const AIData* data = ai.get_data(1);
    ASSERT_NE(data, nullptr);
    EXPECT_EQ(data->target, 42u);
}

// ---------------------------------------------------------------------------
// Moving -> Attacking (set target while moving)
// ---------------------------------------------------------------------------

TEST(AISystem, SetTargetTransitionsMovingToAttacking) {
    AISystem ai;
    ai.add(1);
    ai.set_waypoint(1, 10, 0, 0);
    EXPECT_EQ(ai.get_state(1), AIState::Moving);

    ai.set_target(1, 99);
    EXPECT_EQ(ai.get_state(1), AIState::Attacking);
}

// ---------------------------------------------------------------------------
// Attacking -> Retreating (low health)
// ---------------------------------------------------------------------------

TEST(AISystem, LowHealthTransitionsAttackingToRetreating) {
    AISystem ai;
    AIData config;
    config.retreat_health = 30.0f;
    ai.add(1, config);

    // Go to Attacking.
    ai.set_target(1, 99);
    EXPECT_EQ(ai.get_state(1), AIState::Attacking);

    // Notify health below threshold.
    ai.notify_health(1, 25.0f);
    EXPECT_EQ(ai.get_state(1), AIState::Retreating);
}

TEST(AISystem, HealthAboveThresholdDoesNotCauseRetreat) {
    AISystem ai;
    AIData config;
    config.retreat_health = 30.0f;
    ai.add(1, config);

    ai.set_target(1, 99);
    ai.notify_health(1, 50.0f);  // Above threshold.
    EXPECT_EQ(ai.get_state(1), AIState::Attacking);
}

// ---------------------------------------------------------------------------
// Attacking -> Idle (target lost)
// ---------------------------------------------------------------------------

TEST(AISystem, TargetLostTransitionsAttackingToIdle) {
    AISystem ai;
    ai.add(1);
    ai.set_target(1, 42);
    EXPECT_EQ(ai.get_state(1), AIState::Attacking);

    ai.notify_target_lost(1);
    EXPECT_EQ(ai.get_state(1), AIState::Idle);

    const AIData* data = ai.get_data(1);
    ASSERT_NE(data, nullptr);
    EXPECT_EQ(data->target, INVALID_ENTITY);
}

// ---------------------------------------------------------------------------
// Retreating -> Idle (after timer)
// ---------------------------------------------------------------------------

TEST(AISystem, RetreatTimerExpiresTransitionsToIdle) {
    AISystem ai;
    AIData config;
    config.retreat_health = 30.0f;
    ai.add(1, config);

    // Attacking -> Retreating.
    ai.set_target(1, 99);
    ai.notify_health(1, 10.0f);
    EXPECT_EQ(ai.get_state(1), AIState::Retreating);

    // Update past the retreat duration (3.0 seconds).
    ai.update(1.0f);
    EXPECT_EQ(ai.get_state(1), AIState::Retreating);  // Not yet.

    ai.update(1.0f);
    EXPECT_EQ(ai.get_state(1), AIState::Retreating);  // Still not.

    ai.update(1.5f);  // Total: 3.5s > 3.0s.
    EXPECT_EQ(ai.get_state(1), AIState::Idle);
}

TEST(AISystem, RetreatDoesNotExpireEarly) {
    AISystem ai;
    AIData config;
    config.retreat_health = 30.0f;
    ai.add(1, config);

    ai.set_target(1, 99);
    ai.notify_health(1, 10.0f);
    EXPECT_EQ(ai.get_state(1), AIState::Retreating);

    ai.update(2.0f);
    EXPECT_EQ(ai.get_state(1), AIState::Retreating);
}

// ---------------------------------------------------------------------------
// State timer tracking
// ---------------------------------------------------------------------------

TEST(AISystem, StateTimerResetsOnTransition) {
    AISystem ai;
    ai.add(1);

    // Idle for 2 seconds.
    ai.update(2.0f);
    const AIData* data = ai.get_data(1);
    ASSERT_NE(data, nullptr);
    EXPECT_FLOAT_EQ(data->state_timer, 2.0f);

    // Transition to Moving — timer should reset.
    ai.set_waypoint(1, 10, 0, 0);
    data = ai.get_data(1);
    EXPECT_FLOAT_EQ(data->state_timer, 0.0f);
}

// ---------------------------------------------------------------------------
// Edge cases
// ---------------------------------------------------------------------------

TEST(AISystem, SetWaypointOnUnregisteredIsNoOp) {
    AISystem ai;
    ai.set_waypoint(999, 1, 2, 3);  // Should not crash.
    EXPECT_EQ(ai.count(), 0u);
}

TEST(AISystem, NotifyHealthOnIdleDoesNotTransition) {
    AISystem ai;
    AIData config;
    config.retreat_health = 30.0f;
    ai.add(1, config);

    // Entity is Idle, not Attacking — low health should not cause retreat.
    ai.notify_health(1, 10.0f);
    EXPECT_EQ(ai.get_state(1), AIState::Idle);
}

TEST(AISystem, NotifyTargetLostOnIdleIsNoOp) {
    AISystem ai;
    ai.add(1);
    ai.notify_target_lost(1);
    EXPECT_EQ(ai.get_state(1), AIState::Idle);
}

// ---------------------------------------------------------------------------
// Full lifecycle: Idle -> Moving -> Attacking -> Retreating -> Idle
// ---------------------------------------------------------------------------

TEST(AISystem, FullLifecycle) {
    AISystem ai;
    AIData config;
    config.retreat_health = 30.0f;
    ai.add(1, config);

    EXPECT_EQ(ai.get_state(1), AIState::Idle);

    // 1. Idle -> Moving
    ai.set_waypoint(1, 50, 0, 50);
    EXPECT_EQ(ai.get_state(1), AIState::Moving);

    // 2. Moving -> Attacking
    ai.set_target(1, 42);
    EXPECT_EQ(ai.get_state(1), AIState::Attacking);

    // 3. Attacking -> Retreating
    ai.notify_health(1, 20.0f);
    EXPECT_EQ(ai.get_state(1), AIState::Retreating);

    // 4. Retreating -> Idle (after timer)
    ai.update(4.0f);
    EXPECT_EQ(ai.get_state(1), AIState::Idle);
}
