// States.h
// The behaviors. A state holds only what lives as long as the state (a
// timer, the hit that caused it). Anything that must survive a transition
// (health, patrol bounds, the target) lives on the Actor.
//
// Bodies are in States.cpp: a state's method can only mention other states
// once every state is complete, so they cannot be defined inline here.
#pragma once

#include <optional>
#include <variant>

class Actor;

struct HitEvent
{
    int damage = 0;
    float fromX = 0.f;    // attacker's x, so the victim can face it and test front/back
};

// What the keyboard wants this step. Only the player's Actor gets one filled in.
struct Input
{
    bool left = false;
    bool right = false;
    bool attack = false;   // true only on the step the key went down
    bool guard = false;    // true while held
};

struct Idle;
struct Patrol;
struct Attack;
struct TakeHit;
struct Defend;
struct Dead;
struct Player;

// Idle first: it is what a fresh Behavior holds.
using States = std::variant<Idle, Patrol, Attack, TakeHit, Defend, Dead, Player>;
using Next = std::optional<States>;   // nullopt = stay in the current state

// Stand still, watch for a target, wander off after a while. The AI's hub.
struct Idle
{
    static constexpr const char* name = "Idle";
    float waited = 0.f;             // seconds in this state

    void enter(Actor& a);
    Next update(Actor& a, float dt);
    Next onHit(Actor& a, const HitEvent& hit);
};

// Walk between the actor's homeLeft and homeRight.
struct Patrol
{
    static constexpr const char* name = "Patrol";

    void enter(Actor& a);
    Next update(Actor& a, float dt);
    Next onHit(Actor& a, const HitEvent& hit);
};

// Commit to one swing, then return to the actor's rest state.
struct Attack
{
    static constexpr const char* name = "Attack";
    float targetX = 0.f;            // where the target was when we committed

    void enter(Actor& a);
    Next update(Actor& a, float dt);
    Next onHit(Actor& a, const HitEvent& hit);
};

// Take the damage, flinch, ignore further hits until the flinch ends.
struct TakeHit
{
    static constexpr const char* name = "TakeHit";
    HitEvent hit;

    void enter(Actor& a);
    Next update(Actor& a, float dt);
    // no onHit: invulnerable while flinching
};

// Hold the guard: front hits are absorbed, back hits land, too many blocks break it.
struct Defend
{
    static constexpr const char* name = "Defend";
    float holdFor = 1.f;            // seconds to keep the guard up (the player also holds the key)
    int blocked = 0;                // hits absorbed so far

    void enter(Actor& a);
    Next update(Actor& a, float dt);
    Next onHit(Actor& a, const HitEvent& hit);
};

// Play the death animation and stay there.
struct Dead
{
    static constexpr const char* name = "Dead";

    void enter(Actor& a);
    Next update(Actor& a, float dt);
    // no onHit
};

// Keyboard control: walk, attack, guard. The player's rest state.
struct Player
{
    static constexpr const char* name = "Player";

    void enter(Actor& a);
    Next update(Actor& a, float dt);
    Next onHit(Actor& a, const HitEvent& hit);
};
