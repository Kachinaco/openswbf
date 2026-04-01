// Unit tests for game/health_system.h — damage, healing, death callbacks.

#include "game/health_system.h"

#include <gtest/gtest.h>

using namespace swbf;

// ---------------------------------------------------------------------------
// Registration and initial state
// ---------------------------------------------------------------------------

TEST(HealthSystem, AddEntitySetsMaxHealth) {
    HealthSystem hs;
    hs.add(1, 100.0f);

    EXPECT_FLOAT_EQ(hs.get_health(1), 100.0f);
    EXPECT_FLOAT_EQ(hs.get_max_health(1), 100.0f);
    EXPECT_FALSE(hs.is_dead(1));
}

TEST(HealthSystem, CountIncrementsOnAdd) {
    HealthSystem hs;
    EXPECT_EQ(hs.count(), 0u);
    hs.add(1, 100.0f);
    EXPECT_EQ(hs.count(), 1u);
    hs.add(2, 200.0f);
    EXPECT_EQ(hs.count(), 2u);
}

TEST(HealthSystem, GetHealthReturnsNegativeForUnregistered) {
    HealthSystem hs;
    EXPECT_FLOAT_EQ(hs.get_health(999), -1.0f);
    EXPECT_FLOAT_EQ(hs.get_max_health(999), -1.0f);
}

// ---------------------------------------------------------------------------
// Damage
// ---------------------------------------------------------------------------

TEST(HealthSystem, DamageReducesHealth) {
    HealthSystem hs;
    hs.add(1, 100.0f);

    float dealt = hs.damage(1, 30.0f);
    EXPECT_FLOAT_EQ(dealt, 30.0f);
    EXPECT_FLOAT_EQ(hs.get_health(1), 70.0f);
}

TEST(HealthSystem, DamageCannotExceedCurrentHealth) {
    HealthSystem hs;
    hs.add(1, 50.0f);

    float dealt = hs.damage(1, 100.0f);
    EXPECT_FLOAT_EQ(dealt, 50.0f);
    EXPECT_FLOAT_EQ(hs.get_health(1), 0.0f);
}

TEST(HealthSystem, DamageToUnregisteredReturnsZero) {
    HealthSystem hs;
    EXPECT_FLOAT_EQ(hs.damage(999, 10.0f), 0.0f);
}

TEST(HealthSystem, NegativeDamageIsTreatedAsZero) {
    HealthSystem hs;
    hs.add(1, 100.0f);
    float dealt = hs.damage(1, -50.0f);
    EXPECT_FLOAT_EQ(dealt, 0.0f);
    EXPECT_FLOAT_EQ(hs.get_health(1), 100.0f);
}

// ---------------------------------------------------------------------------
// Death
// ---------------------------------------------------------------------------

TEST(HealthSystem, LethalDamageCausesDeath) {
    HealthSystem hs;
    hs.add(1, 100.0f);

    hs.damage(1, 100.0f);
    EXPECT_TRUE(hs.is_dead(1));
    EXPECT_FLOAT_EQ(hs.get_health(1), 0.0f);
}

TEST(HealthSystem, DamageToDeadEntityReturnsZero) {
    HealthSystem hs;
    hs.add(1, 50.0f);
    hs.damage(1, 50.0f);
    EXPECT_TRUE(hs.is_dead(1));

    float dealt = hs.damage(1, 10.0f);
    EXPECT_FLOAT_EQ(dealt, 0.0f);
}

TEST(HealthSystem, DeathCallbackIsFired) {
    HealthSystem hs;
    hs.add(1, 100.0f);

    EntityId dead_entity = INVALID_ENTITY;
    hs.set_death_callback([&](EntityId id) {
        dead_entity = id;
    });

    hs.damage(1, 100.0f);
    EXPECT_EQ(dead_entity, 1u);
}

TEST(HealthSystem, DeathCallbackNotFiredOnPartialDamage) {
    HealthSystem hs;
    hs.add(1, 100.0f);

    bool callback_fired = false;
    hs.set_death_callback([&](EntityId) {
        callback_fired = true;
    });

    hs.damage(1, 50.0f);
    EXPECT_FALSE(callback_fired);
}

TEST(HealthSystem, DeathCallbackNotFiredTwice) {
    HealthSystem hs;
    hs.add(1, 100.0f);

    int fire_count = 0;
    hs.set_death_callback([&](EntityId) {
        fire_count++;
    });

    hs.damage(1, 100.0f);
    hs.damage(1, 100.0f);  // Already dead — should not fire again.
    EXPECT_EQ(fire_count, 1);
}

// ---------------------------------------------------------------------------
// Healing
// ---------------------------------------------------------------------------

TEST(HealthSystem, HealRestoresHealth) {
    HealthSystem hs;
    hs.add(1, 100.0f);
    hs.damage(1, 40.0f);

    float healed = hs.heal(1, 20.0f);
    EXPECT_FLOAT_EQ(healed, 20.0f);
    EXPECT_FLOAT_EQ(hs.get_health(1), 80.0f);
}

TEST(HealthSystem, HealCannotExceedMax) {
    HealthSystem hs;
    hs.add(1, 100.0f);
    hs.damage(1, 10.0f);

    float healed = hs.heal(1, 50.0f);
    EXPECT_FLOAT_EQ(healed, 10.0f);
    EXPECT_FLOAT_EQ(hs.get_health(1), 100.0f);
}

TEST(HealthSystem, HealAtMaxReturnsZero) {
    HealthSystem hs;
    hs.add(1, 100.0f);

    float healed = hs.heal(1, 10.0f);
    EXPECT_FLOAT_EQ(healed, 0.0f);
}

TEST(HealthSystem, CannotHealDeadEntity) {
    HealthSystem hs;
    hs.add(1, 100.0f);
    hs.damage(1, 100.0f);
    EXPECT_TRUE(hs.is_dead(1));

    float healed = hs.heal(1, 50.0f);
    EXPECT_FLOAT_EQ(healed, 0.0f);
    EXPECT_FLOAT_EQ(hs.get_health(1), 0.0f);
}

TEST(HealthSystem, HealUnregisteredReturnsZero) {
    HealthSystem hs;
    EXPECT_FLOAT_EQ(hs.heal(999, 10.0f), 0.0f);
}

// ---------------------------------------------------------------------------
// Remove
// ---------------------------------------------------------------------------

TEST(HealthSystem, RemoveEntityRemovesFromSystem) {
    HealthSystem hs;
    hs.add(1, 100.0f);
    hs.remove(1);

    EXPECT_FLOAT_EQ(hs.get_health(1), -1.0f);
    EXPECT_EQ(hs.count(), 0u);
}

TEST(HealthSystem, IsDeadReturnsFalseForUnregistered) {
    HealthSystem hs;
    EXPECT_FALSE(hs.is_dead(999));
}
