// Unit tests for game/spawn_system.h — spawn point registration, respawn
// timer, and unit class selection.

#include "game/spawn_system.h"
#include "game/entity_manager.h"
#include "game/command_post_system.h"

#include <gtest/gtest.h>

using namespace swbf;

// ---------------------------------------------------------------------------
// Spawn point registration
// ---------------------------------------------------------------------------

TEST(SpawnSystem, AddSpawnPointReturnsValidId) {
    SpawnSystem ss;
    uint32_t id = ss.add_spawn_point("SP1", 10, 0, 20, TEAM_REPUBLIC);
    EXPECT_NE(id, 0u);
    EXPECT_EQ(ss.spawn_point_count(), 1u);
}

TEST(SpawnSystem, AddMultipleSpawnPoints) {
    SpawnSystem ss;
    ss.add_spawn_point("SP1", 0, 0, 0, TEAM_REPUBLIC);
    ss.add_spawn_point("SP2", 10, 0, 0, TEAM_CIS);
    ss.add_spawn_point("SP3", 20, 0, 0, TEAM_NEUTRAL);
    EXPECT_EQ(ss.spawn_point_count(), 3u);
}

TEST(SpawnSystem, GetSpawnPointReturnsData) {
    SpawnSystem ss;
    uint32_t id = ss.add_spawn_point("SP1", 10, 5, 20, TEAM_REPUBLIC);

    const SpawnPoint* sp = ss.get_spawn_point(id);
    ASSERT_NE(sp, nullptr);
    EXPECT_EQ(sp->name, "SP1");
    EXPECT_FLOAT_EQ(sp->position[0], 10.0f);
    EXPECT_FLOAT_EQ(sp->position[1], 5.0f);
    EXPECT_FLOAT_EQ(sp->position[2], 20.0f);
    EXPECT_EQ(sp->owner, TEAM_REPUBLIC);
}

TEST(SpawnSystem, GetSpawnPointReturnsNullForInvalid) {
    SpawnSystem ss;
    EXPECT_EQ(ss.get_spawn_point(999), nullptr);
}

TEST(SpawnSystem, RemoveSpawnPoint) {
    SpawnSystem ss;
    uint32_t id = ss.add_spawn_point("SP1", 0, 0, 0);
    ss.remove_spawn_point(id);
    EXPECT_EQ(ss.spawn_point_count(), 0u);
    EXPECT_EQ(ss.get_spawn_point(id), nullptr);
}

// ---------------------------------------------------------------------------
// Team spawn filtering
// ---------------------------------------------------------------------------

TEST(SpawnSystem, GetTeamSpawnsFiltersCorrectly) {
    SpawnSystem ss;
    ss.add_spawn_point("SP1", 0, 0, 0, TEAM_REPUBLIC);
    ss.add_spawn_point("SP2", 10, 0, 0, TEAM_REPUBLIC);
    ss.add_spawn_point("SP3", 20, 0, 0, TEAM_CIS);

    auto rep_spawns = ss.get_team_spawns(TEAM_REPUBLIC);
    EXPECT_EQ(rep_spawns.size(), 2u);

    auto cis_spawns = ss.get_team_spawns(TEAM_CIS);
    EXPECT_EQ(cis_spawns.size(), 1u);

    auto neutral_spawns = ss.get_team_spawns(TEAM_NEUTRAL);
    EXPECT_EQ(neutral_spawns.size(), 0u);
}

// ---------------------------------------------------------------------------
// Sync with command posts
// ---------------------------------------------------------------------------

TEST(SpawnSystem, SyncWithPostsUpdatesOwnership) {
    SpawnSystem ss;
    CommandPostSystem cps;

    uint32_t cp1 = cps.add_post("CP1", 0, 0, 0, TEAM_REPUBLIC);
    uint32_t cp2 = cps.add_post("CP2", 50, 0, 0, TEAM_CIS);

    uint32_t sp1 = ss.add_spawn_point("SP1", 5, 0, 0, TEAM_NEUTRAL, cp1);
    uint32_t sp2 = ss.add_spawn_point("SP2", 55, 0, 0, TEAM_NEUTRAL, cp2);

    // Sync: spawn points should inherit their linked post's owner.
    ss.sync_with_posts(cps);

    const SpawnPoint* s1 = ss.get_spawn_point(sp1);
    ASSERT_NE(s1, nullptr);
    EXPECT_EQ(s1->owner, TEAM_REPUBLIC);

    const SpawnPoint* s2 = ss.get_spawn_point(sp2);
    ASSERT_NE(s2, nullptr);
    EXPECT_EQ(s2->owner, TEAM_CIS);
}

TEST(SpawnSystem, SyncDoesNotAffectUnlinkedSpawns) {
    SpawnSystem ss;
    CommandPostSystem cps;

    cps.add_post("CP1", 0, 0, 0, TEAM_REPUBLIC);

    // This spawn point has no linked post (linked_post = 0).
    uint32_t sp = ss.add_spawn_point("SP1", 0, 0, 0, TEAM_NEUTRAL);
    ss.sync_with_posts(cps);

    const SpawnPoint* s = ss.get_spawn_point(sp);
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->owner, TEAM_NEUTRAL);  // Unchanged.
}

// ---------------------------------------------------------------------------
// Respawn timer
// ---------------------------------------------------------------------------

TEST(SpawnSystem, RequestRespawnAddsPendingRequest) {
    SpawnSystem ss;
    ss.add_spawn_point("SP1", 0, 0, 0, TEAM_REPUBLIC);

    EntityManager em;
    EntityId e = em.create("trooper");

    ss.request_respawn(e, TEAM_REPUBLIC, UnitClass::Soldier, 5.0f);
    EXPECT_EQ(ss.pending_respawn_count(), 1u);
}

TEST(SpawnSystem, RespawnTimerCountsDown) {
    SpawnSystem ss;
    ss.add_spawn_point("SP1", 10, 0, 20, TEAM_REPUBLIC);

    EntityManager em;
    EntityId e = em.create("trooper");

    ss.request_respawn(e, TEAM_REPUBLIC, UnitClass::Soldier, 5.0f);

    // Update with 3 seconds — not enough to spawn.
    auto spawned = ss.update(3.0f, em);
    EXPECT_TRUE(spawned.empty());
    EXPECT_EQ(ss.pending_respawn_count(), 1u);
}

TEST(SpawnSystem, RespawnHappensWhenTimerExpires) {
    SpawnSystem ss;
    ss.add_spawn_point("SP1", 10, 0, 20, TEAM_REPUBLIC);

    EntityManager em;
    EntityId e = em.create("trooper");

    ss.request_respawn(e, TEAM_REPUBLIC, UnitClass::Soldier, 5.0f);

    // Update past the timer.
    auto spawned = ss.update(6.0f, em);
    ASSERT_EQ(spawned.size(), 1u);
    EXPECT_EQ(spawned[0], e);
    EXPECT_EQ(ss.pending_respawn_count(), 0u);

    // Verify the entity was placed at the spawn point.
    Transform* t = em.get_transform(e);
    ASSERT_NE(t, nullptr);
    EXPECT_FLOAT_EQ(t->position[0], 10.0f);
    EXPECT_FLOAT_EQ(t->position[1], 0.0f);
    EXPECT_FLOAT_EQ(t->position[2], 20.0f);
}

TEST(SpawnSystem, PreferredSpawnPointIsUsed) {
    SpawnSystem ss;
    ss.add_spawn_point("SP1", 0, 0, 0, TEAM_REPUBLIC);
    uint32_t sp2 = ss.add_spawn_point("SP2", 100, 0, 100, TEAM_REPUBLIC);

    EntityManager em;
    EntityId e = em.create("trooper");

    // Request spawn at SP2 specifically.
    ss.request_respawn(e, TEAM_REPUBLIC, UnitClass::Soldier, 0.0f, sp2);

    auto spawned = ss.update(0.1f, em);
    ASSERT_EQ(spawned.size(), 1u);

    Transform* t = em.get_transform(e);
    ASSERT_NE(t, nullptr);
    EXPECT_FLOAT_EQ(t->position[0], 100.0f);
    EXPECT_FLOAT_EQ(t->position[2], 100.0f);
}

// ---------------------------------------------------------------------------
// Unit class selection
// ---------------------------------------------------------------------------

TEST(SpawnSystem, DifferentUnitClassesCanSpawn) {
    SpawnSystem ss;
    ss.add_spawn_point("SP1", 0, 0, 0, TEAM_CIS);

    EntityManager em;
    EntityId soldier  = em.create("soldier");
    EntityId heavy    = em.create("heavy");
    EntityId sniper   = em.create("sniper");
    EntityId engineer = em.create("engineer");
    EntityId special  = em.create("special");

    ss.request_respawn(soldier,  TEAM_CIS, UnitClass::Soldier,  0.0f);
    ss.request_respawn(heavy,    TEAM_CIS, UnitClass::Heavy,    0.0f);
    ss.request_respawn(sniper,   TEAM_CIS, UnitClass::Sniper,   0.0f);
    ss.request_respawn(engineer, TEAM_CIS, UnitClass::Engineer,  0.0f);
    ss.request_respawn(special,  TEAM_CIS, UnitClass::Special,  0.0f);

    auto spawned = ss.update(0.1f, em);
    EXPECT_EQ(spawned.size(), 5u);
}

// ---------------------------------------------------------------------------
// No spawn point available
// ---------------------------------------------------------------------------

TEST(SpawnSystem, NoSpawnPointForTeamStillRemovesRequest) {
    SpawnSystem ss;
    // Only CIS spawn points.
    ss.add_spawn_point("SP1", 0, 0, 0, TEAM_CIS);

    EntityManager em;
    EntityId e = em.create("trooper");

    // Republic has no spawn points.
    ss.request_respawn(e, TEAM_REPUBLIC, UnitClass::Soldier, 0.0f);

    auto spawned = ss.update(0.1f, em);
    // No spawn happened, but the request is removed (no infinite queue).
    EXPECT_TRUE(spawned.empty());
    EXPECT_EQ(ss.pending_respawn_count(), 0u);
}

// ---------------------------------------------------------------------------
// Multiple respawns in sequence
// ---------------------------------------------------------------------------

TEST(SpawnSystem, MultipleRespawnsStagger) {
    SpawnSystem ss;
    ss.add_spawn_point("SP1", 0, 0, 0, TEAM_REPUBLIC);

    EntityManager em;
    EntityId e1 = em.create("a");
    EntityId e2 = em.create("b");

    ss.request_respawn(e1, TEAM_REPUBLIC, UnitClass::Soldier, 2.0f);
    ss.request_respawn(e2, TEAM_REPUBLIC, UnitClass::Heavy,   5.0f);

    // After 3 seconds: e1 should spawn, e2 still pending.
    auto spawned1 = ss.update(3.0f, em);
    EXPECT_EQ(spawned1.size(), 1u);
    EXPECT_EQ(spawned1[0], e1);
    EXPECT_EQ(ss.pending_respawn_count(), 1u);

    // After 3 more seconds (total 6): e2 should spawn.
    auto spawned2 = ss.update(3.0f, em);
    EXPECT_EQ(spawned2.size(), 1u);
    EXPECT_EQ(spawned2[0], e2);
    EXPECT_EQ(ss.pending_respawn_count(), 0u);
}
