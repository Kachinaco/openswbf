// Unit tests for game/weapon_system.h — projectile creation, movement, collision, damage.

#include "game/weapon_system.h"
#include "game/entity_manager.h"

#include <gtest/gtest.h>

#include <cmath>

using namespace swbf;

static constexpr float EPSILON = 1e-3f;

// ---------------------------------------------------------------------------
// Projectile creation
// ---------------------------------------------------------------------------

TEST(WeaponSystem, FireCreatesProjectile) {
    WeaponSystem ws;
    WeaponConfig cfg;
    cfg.damage = 25.0f;
    cfg.projectile_speed = 100.0f;

    size_t idx = ws.fire(cfg, 1, 0, 0, 0, 0, 0, -1);
    EXPECT_EQ(idx, 0u);
    EXPECT_EQ(ws.active_count(), 1u);
    EXPECT_EQ(ws.total_count(), 1u);
}

TEST(WeaponSystem, FireMultipleProjectiles) {
    WeaponSystem ws;
    WeaponConfig cfg;

    ws.fire(cfg, 1, 0, 0, 0, 0, 0, -1);
    ws.fire(cfg, 1, 0, 0, 0, 1, 0, 0);
    ws.fire(cfg, 2, 0, 0, 0, 0, 0, 1);

    EXPECT_EQ(ws.active_count(), 3u);
    EXPECT_EQ(ws.total_count(), 3u);
}

TEST(WeaponSystem, ProjectileHasCorrectDamage) {
    WeaponSystem ws;
    WeaponConfig cfg;
    cfg.damage = 42.0f;

    size_t idx = ws.fire(cfg, 1, 0, 0, 0, 0, 0, -1);
    const Projectile* p = ws.get_projectile(idx);
    ASSERT_NE(p, nullptr);
    EXPECT_FLOAT_EQ(p->damage, 42.0f);
}

TEST(WeaponSystem, ProjectileDirectionIsNormalised) {
    WeaponSystem ws;
    WeaponConfig cfg;

    size_t idx = ws.fire(cfg, 1, 0, 0, 0, 3, 4, 0);
    const Projectile* p = ws.get_projectile(idx);
    ASSERT_NE(p, nullptr);

    float len = std::sqrt(p->direction[0] * p->direction[0] +
                          p->direction[1] * p->direction[1] +
                          p->direction[2] * p->direction[2]);
    EXPECT_NEAR(len, 1.0f, EPSILON);
}

// ---------------------------------------------------------------------------
// Projectile movement
// ---------------------------------------------------------------------------

TEST(WeaponSystem, ProjectileMovesAlongDirection) {
    WeaponSystem ws;
    EntityManager em;
    WeaponConfig cfg;
    cfg.projectile_speed = 100.0f;
    cfg.max_range = 1000.0f;

    // Fire along -Z.
    size_t idx = ws.fire(cfg, INVALID_ENTITY, 0, 0, 0, 0, 0, -1);

    ws.update(1.0f, em);

    const Projectile* p = ws.get_projectile(idx);
    ASSERT_NE(p, nullptr);
    EXPECT_NEAR(p->position[2], -100.0f, EPSILON);
    EXPECT_NEAR(p->position[0], 0.0f, EPSILON);
    EXPECT_NEAR(p->position[1], 0.0f, EPSILON);
}

TEST(WeaponSystem, ProjectileMovesPartialTimestep) {
    WeaponSystem ws;
    EntityManager em;
    WeaponConfig cfg;
    cfg.projectile_speed = 50.0f;
    cfg.max_range = 1000.0f;

    // Fire along +X.
    ws.fire(cfg, INVALID_ENTITY, 0, 0, 0, 1, 0, 0);

    ws.update(0.5f, em);  // 50 * 0.5 = 25 units

    const Projectile* p = ws.get_projectile(0);
    ASSERT_NE(p, nullptr);
    EXPECT_NEAR(p->position[0], 25.0f, EPSILON);
}

// ---------------------------------------------------------------------------
// Range expiry
// ---------------------------------------------------------------------------

TEST(WeaponSystem, ProjectileExpiresBeyondMaxRange) {
    WeaponSystem ws;
    EntityManager em;
    WeaponConfig cfg;
    cfg.projectile_speed = 100.0f;
    cfg.max_range = 50.0f;

    ws.fire(cfg, INVALID_ENTITY, 0, 0, 0, 1, 0, 0);

    // After 1 second at 100 units/s, the projectile has gone 100 > max_range 50.
    ws.update(1.0f, em);

    EXPECT_EQ(ws.active_count(), 0u);
}

TEST(WeaponSystem, ProjectileStaysActiveWithinRange) {
    WeaponSystem ws;
    EntityManager em;
    WeaponConfig cfg;
    cfg.projectile_speed = 10.0f;
    cfg.max_range = 100.0f;

    ws.fire(cfg, INVALID_ENTITY, 0, 0, 0, 1, 0, 0);

    ws.update(1.0f, em);  // 10 < 100 — still in range

    EXPECT_EQ(ws.active_count(), 1u);
}

// ---------------------------------------------------------------------------
// Collision detection
// ---------------------------------------------------------------------------

TEST(WeaponSystem, ProjectileHitsTarget) {
    WeaponSystem ws;
    EntityManager em;
    WeaponConfig cfg;
    cfg.projectile_speed = 100.0f;
    cfg.damage = 30.0f;
    cfg.max_range = 1000.0f;

    // Place a target 50 units ahead along +X.
    EntityId target = em.create("target");
    em.set_position(target, 50.0f, 0.0f, 0.0f);

    // Fire from the owner along +X.
    EntityId owner = em.create("shooter");
    ws.fire(cfg, owner, 0, 0, 0, 1, 0, 0);

    // 0.5 seconds at 100 u/s = 50 units — should reach the target.
    auto hits = ws.update(0.5f, em, 1.0f);

    ASSERT_GE(hits.size(), 1u);
    EXPECT_EQ(hits[0].target, target);
    EXPECT_FLOAT_EQ(hits[0].damage, 30.0f);
}

TEST(WeaponSystem, ProjectileDoesNotHitOwner) {
    WeaponSystem ws;
    EntityManager em;
    WeaponConfig cfg;
    cfg.projectile_speed = 100.0f;
    cfg.max_range = 1000.0f;

    // Owner at origin, fire from origin.
    EntityId owner = em.create("shooter");
    em.set_position(owner, 0.0f, 0.0f, 0.0f);
    ws.fire(cfg, owner, 0, 0, 0, 1, 0, 0);

    auto hits = ws.update(0.01f, em, 2.0f);
    // The owner should not appear in the hits.
    for (const auto& h : hits) {
        EXPECT_NE(h.target, owner);
    }
}

TEST(WeaponSystem, ProjectileDeactivatesAfterHit) {
    WeaponSystem ws;
    EntityManager em;
    WeaponConfig cfg;
    cfg.projectile_speed = 10.0f;
    cfg.max_range = 1000.0f;

    EntityId target = em.create("target");
    em.set_position(target, 5.0f, 0.0f, 0.0f);

    ws.fire(cfg, INVALID_ENTITY, 0, 0, 0, 1, 0, 0);
    ws.update(0.5f, em, 1.0f);  // 5 units — at target

    EXPECT_EQ(ws.active_count(), 0u);
}

// ---------------------------------------------------------------------------
// Clear
// ---------------------------------------------------------------------------

TEST(WeaponSystem, ClearRemovesAllProjectiles) {
    WeaponSystem ws;
    WeaponConfig cfg;

    ws.fire(cfg, 1, 0, 0, 0, 1, 0, 0);
    ws.fire(cfg, 1, 0, 0, 0, 0, 1, 0);
    EXPECT_EQ(ws.total_count(), 2u);

    ws.clear();
    EXPECT_EQ(ws.total_count(), 0u);
    EXPECT_EQ(ws.active_count(), 0u);
}

// ---------------------------------------------------------------------------
// Invalid index
// ---------------------------------------------------------------------------

TEST(WeaponSystem, GetProjectileReturnsNullForInvalidIndex) {
    WeaponSystem ws;
    EXPECT_EQ(ws.get_projectile(0), nullptr);
    EXPECT_EQ(ws.get_projectile(100), nullptr);
}
