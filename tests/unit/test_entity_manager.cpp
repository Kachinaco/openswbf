// Unit tests for game/entity_manager.h — entity spawn, destroy, lookup, position.

#include "game/entity_manager.h"

#include <gtest/gtest.h>

using namespace swbf;

// ---------------------------------------------------------------------------
// Spawn and basic queries
// ---------------------------------------------------------------------------

TEST(EntityManager, SpawnReturnsValidId) {
    EntityManager em;
    EntityId id = em.create("trooper");
    EXPECT_NE(id, INVALID_ENTITY);
}

TEST(EntityManager, SpawnIncrementsCount) {
    EntityManager em;
    EXPECT_EQ(em.count(), 0u);

    em.create("a");
    EXPECT_EQ(em.count(), 1u);

    em.create("b");
    EXPECT_EQ(em.count(), 2u);
}

TEST(EntityManager, SpawnedEntityIsAlive) {
    EntityManager em;
    EntityId id = em.create("trooper");
    EXPECT_TRUE(em.alive(id));
}

TEST(EntityManager, UniqueIdsPerSpawn) {
    EntityManager em;
    EntityId a = em.create("a");
    EntityId b = em.create("b");
    EntityId c = em.create("c");
    EXPECT_NE(a, b);
    EXPECT_NE(b, c);
    EXPECT_NE(a, c);
}

// ---------------------------------------------------------------------------
// Destroy
// ---------------------------------------------------------------------------

TEST(EntityManager, DestroyRemovesEntity) {
    EntityManager em;
    EntityId id = em.create("trooper");
    EXPECT_TRUE(em.alive(id));

    bool result = em.destroy(id);
    EXPECT_TRUE(result);
    EXPECT_FALSE(em.alive(id));
    EXPECT_EQ(em.count(), 0u);
}

TEST(EntityManager, DestroyInvalidIdReturnsFalse) {
    EntityManager em;
    EXPECT_FALSE(em.destroy(INVALID_ENTITY));
    EXPECT_FALSE(em.destroy(999));
}

TEST(EntityManager, DoubleDestroyReturnsFalse) {
    EntityManager em;
    EntityId id = em.create("trooper");
    EXPECT_TRUE(em.destroy(id));
    EXPECT_FALSE(em.destroy(id));
}

// ---------------------------------------------------------------------------
// Transform lookup
// ---------------------------------------------------------------------------

TEST(EntityManager, GetTransformReturnsNonNull) {
    EntityManager em;
    EntityId id = em.create("trooper");
    Transform* t = em.get_transform(id);
    ASSERT_NE(t, nullptr);
}

TEST(EntityManager, GetTransformReturnsNullForDeadEntity) {
    EntityManager em;
    EntityId id = em.create("trooper");
    em.destroy(id);
    EXPECT_EQ(em.get_transform(id), nullptr);
}

TEST(EntityManager, GetTransformDefaultsToOrigin) {
    EntityManager em;
    EntityId id = em.create("trooper");
    Transform* t = em.get_transform(id);
    ASSERT_NE(t, nullptr);
    EXPECT_FLOAT_EQ(t->position[0], 0.0f);
    EXPECT_FLOAT_EQ(t->position[1], 0.0f);
    EXPECT_FLOAT_EQ(t->position[2], 0.0f);
}

// ---------------------------------------------------------------------------
// Position update
// ---------------------------------------------------------------------------

TEST(EntityManager, SetPositionUpdatesTransform) {
    EntityManager em;
    EntityId id = em.create("trooper");
    EXPECT_TRUE(em.set_position(id, 10.0f, 20.0f, 30.0f));

    Transform* t = em.get_transform(id);
    ASSERT_NE(t, nullptr);
    EXPECT_FLOAT_EQ(t->position[0], 10.0f);
    EXPECT_FLOAT_EQ(t->position[1], 20.0f);
    EXPECT_FLOAT_EQ(t->position[2], 30.0f);
}

TEST(EntityManager, SetPositionReturnsFalseForDeadEntity) {
    EntityManager em;
    EntityId id = em.create("trooper");
    em.destroy(id);
    EXPECT_FALSE(em.set_position(id, 1, 2, 3));
}

// ---------------------------------------------------------------------------
// Name lookup
// ---------------------------------------------------------------------------

TEST(EntityManager, FindByNameReturnsCorrectEntity) {
    EntityManager em;
    EntityId a = em.create("alpha");
    EntityId b = em.create("bravo");

    EXPECT_EQ(em.find_by_name("alpha"), a);
    EXPECT_EQ(em.find_by_name("bravo"), b);
}

TEST(EntityManager, FindByNameReturnsInvalidForMissing) {
    EntityManager em;
    em.create("alpha");
    EXPECT_EQ(em.find_by_name("missing"), INVALID_ENTITY);
}

TEST(EntityManager, GetNameReturnsCorrectName) {
    EntityManager em;
    EntityId id = em.create("trooper");
    EXPECT_EQ(em.get_name(id), "trooper");
}

TEST(EntityManager, GetNameReturnsEmptyForInvalid) {
    EntityManager em;
    EXPECT_EQ(em.get_name(INVALID_ENTITY), "");
    EXPECT_EQ(em.get_name(999), "");
}

// ---------------------------------------------------------------------------
// Iteration
// ---------------------------------------------------------------------------

TEST(EntityManager, ForEachVisitsAllEntities) {
    EntityManager em;
    em.create("a");
    em.create("b");
    em.create("c");

    int visited = 0;
    em.for_each([&](EntityId, Entity&) {
        ++visited;
    });
    EXPECT_EQ(visited, 3);
}

TEST(EntityManager, ForEachSkipsDestroyed) {
    EntityManager em;
    EntityId a = em.create("a");
    em.create("b");
    em.destroy(a);

    int visited = 0;
    em.for_each([&](EntityId, Entity&) {
        ++visited;
    });
    EXPECT_EQ(visited, 1);
}

// ---------------------------------------------------------------------------
// Spawn without name
// ---------------------------------------------------------------------------

TEST(EntityManager, SpawnWithEmptyName) {
    EntityManager em;
    EntityId id = em.create();
    EXPECT_NE(id, INVALID_ENTITY);
    EXPECT_TRUE(em.alive(id));
    // Auto-generated name like "entity_1".
    EXPECT_FALSE(em.get_name(id).empty());
}
