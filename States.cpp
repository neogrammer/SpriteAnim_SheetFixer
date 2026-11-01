// States.cpp
#include "States.h"
#include "Actor.h"

#include <cmath>

namespace
{
    constexpr float kWalkSpeed        = 60.f;    // enemy patrol speed, pixels per second
    constexpr float kPlayerSpeed      = 90.f;
    constexpr float kSightRange       = 160.f;   // enemy notices the player
    constexpr float kAttackRange      = 40.f;    // enemy swings
    constexpr float kAttackCooldown   = 0.5f;    // pause in Idle before the next swing
    constexpr float kIdleBeforePatrol = 1.5f;
    constexpr float kGuardHold        = 0.6f;    // how long the enemy guards after seeing a swing
    constexpr int   kGuardBreakHits   = 3;

    float distanceTo(const Actor& a, const Actor& other)
    {
        return std::abs(other.position.x - a.position.x);
    }

    // True when x is on the side the actor is facing.
    bool inFront(const Actor& a, float x)
    {
        return a.facingLeft() ? x < a.position.x : x > a.position.x;
    }

    // A point just in front of the actor.
    float aheadOf(const Actor& a)
    {
        return a.position.x + (a.facingLeft() ? -1.f : 1.f);
    }

    const Actor* liveTarget(const Actor& a)
    {
        return a.target && a.target->alive() ? a.target : nullptr;
    }
}

// ---------------------------------------------------------------------------
// Idle
// ---------------------------------------------------------------------------

void Idle::enter(Actor& a)
{
    a.velocity.x = 0.f;
    a.play("idle");
}

Next Idle::update(Actor& a, float dt)
{
    waited += dt;

    if (const Actor* t = liveTarget(a))
    {
        const float d = distanceTo(a, *t);

        if (d <= kAttackRange && t->playing("attack"))
            return Defend{ .holdFor = kGuardHold };          // they are swinging: guard first

        if (d <= kAttackRange && waited >= kAttackCooldown)
            return Attack{ .targetX = t->position.x };

        if (d <= kSightRange)
        {
            a.face(t->position.x < a.position.x);             // watch them (a Chase state would go here)
            return std::nullopt;
        }
    }

    if (waited >= kIdleBeforePatrol)
        return Patrol{};

    return std::nullopt;
}

Next Idle::onHit(Actor&, const HitEvent& hit)
{
    return TakeHit{ .hit = hit };
}

// ---------------------------------------------------------------------------
// Patrol
// ---------------------------------------------------------------------------

void Patrol::enter(Actor& a)
{
    a.play("walk");
}

Next Patrol::update(Actor& a, float)
{
    if (const Actor* t = liveTarget(a); t && distanceTo(a, *t) <= kSightRange)
        return Idle{};

    if (a.position.x <= a.homeLeft)
        a.face(false);
    if (a.position.x >= a.homeRight)
        a.face(true);

    a.velocity.x = a.facingLeft() ? -kWalkSpeed : kWalkSpeed;
    return std::nullopt;
}

Next Patrol::onHit(Actor&, const HitEvent& hit)
{
    return TakeHit{ .hit = hit };
}

// ---------------------------------------------------------------------------
// Attack
// Which frames can hurt, and how far they reach, come from the hit lines in
// the .cfg (Actor::hitbox), so there is no timing logic here: face, play, wait.
// ---------------------------------------------------------------------------

void Attack::enter(Actor& a)
{
    a.velocity.x = 0.f;
    a.face(targetX < a.position.x);
    a.play("attack", false);
}

Next Attack::update(Actor& a, float)
{
    if (!a.animationFinished())
        return std::nullopt;

    return a.rest;   // Idle for the AI (which has the cooldown), Player for the player
}

Next Attack::onHit(Actor&, const HitEvent& hit)
{
    return TakeHit{ .hit = hit };   // interruptible; return std::nullopt for super armor
}

// ---------------------------------------------------------------------------
// TakeHit
// ---------------------------------------------------------------------------

void TakeHit::enter(Actor& a)
{
    a.health -= hit.damage;
    a.velocity.x = 0.f;
    a.face(hit.fromX < a.position.x);
    a.play("hurt", false);
}

Next TakeHit::update(Actor& a, float)
{
    if (!a.animationFinished())
        return std::nullopt;

    if (!a.alive())
        return Dead{};

    return a.rest;
}

// ---------------------------------------------------------------------------
// Defend
// ---------------------------------------------------------------------------

void Defend::enter(Actor& a)
{
    a.velocity.x = 0.f;

    if (const Actor* t = liveTarget(a))
        a.face(t->position.x < a.position.x);

    a.play("defend");
}

Next Defend::update(Actor& a, float dt)
{
    holdFor -= dt;

    // The AI guards for holdFor seconds; the player for as long as the key is held.
    if (holdFor <= 0.f && !a.input.guard)
        return a.rest;

    return std::nullopt;
}

Next Defend::onHit(Actor& a, const HitEvent& hit)
{
    if (!inFront(a, hit.fromX))
        return TakeHit{ .hit = hit };

    if (++blocked >= kGuardBreakHits)
        return TakeHit{ .hit = hit };

    a.health -= hit.damage / 4;         // chip damage

    if (!a.alive())
        return Dead{};

    a.play("defend_hit", false);        // its .cfg entry says "next defend", so it chains back
    return std::nullopt;
}

// ---------------------------------------------------------------------------
// Dead
// ---------------------------------------------------------------------------

void Dead::enter(Actor& a)
{
    a.velocity.x = 0.f;
    a.play("dead", false);
}

Next Dead::update(Actor&, float)
{
    return std::nullopt;
}

// ---------------------------------------------------------------------------
// Player
// ---------------------------------------------------------------------------

void Player::enter(Actor& a)
{
    a.velocity.x = 0.f;
    a.play("idle");
}

Next Player::update(Actor& a, float)
{
    const Input& in = a.input;

    if (in.attack)
        return Attack{ .targetX = aheadOf(a) };

    if (in.guard)
        return Defend{ .holdFor = 0.f };

    const float dir = (in.right ? 1.f : 0.f) - (in.left ? 1.f : 0.f);
    a.velocity.x = dir * kPlayerSpeed;

    if (dir != 0.f)
        a.face(dir < 0.f);

    const char* wanted = dir != 0.f ? "walk" : "idle";
    if (!a.playing(wanted))
        a.play(wanted);

    return std::nullopt;
}

Next Player::onHit(Actor&, const HitEvent& hit)
{
    return TakeHit{ .hit = hit };
}
